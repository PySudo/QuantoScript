// Decode escape sequences in a string literal (after quote stripping).
// Returns a newly allocated decoded string.
static char *unescape_string(const char *raw, size_t raw_len, ParseError *err) {
    // Worst case: every char is an escape (1 input -> 1 output), so output <= raw_len
    char *out = malloc(raw_len + 1);
    if (!out) { fprintf(stderr, "out of memory\n"); exit(2); }
    size_t oi = 0;
    for (size_t i = 0; i < raw_len; i++) {
        if (raw[i] == '\\' && i + 1 < raw_len) {
            char next = raw[i + 1];
            switch (next) {
                case 'n':  out[oi++] = '\n'; i++; break;
                case 'r':  out[oi++] = '\r'; i++; break;
                case 't':  out[oi++] = '\t'; i++; break;
                case '\\': out[oi++] = '\\'; i++; break;
                case '"':  out[oi++] = '"';  i++; break;
                case '\'': out[oi++] = '\''; i++; break;
                case '0':  out[oi++] = '\0'; i++; break;
                default:
                    // Unknown escape — keep the backslash and the character
                    out[oi++] = '\\';
                    out[oi++] = next;
                    i++;
                    break;
            }
        } else {
            out[oi++] = raw[i];
        }
    }
    out[oi] = '\0';
    return out;
}

static int parse_string(Parser *parser, Value *out) {
    size_t start = parser->pos;
    char quote = parser->text[start];
    size_t cursor = start + 1;

    while (parser->text[cursor]) {
        if (parser->text[cursor] == '\\' && parser->text[cursor + 1]) {
            cursor += 2; // skip escaped char (always, including closing quote)
            continue;
        }
        if (parser->text[cursor] == quote && quote_can_close(parser->text, cursor, ']')) {
            // Decode escape sequences in the raw string content
            char *raw = qs_strndup(parser->text + start + 1, cursor - start - 1);
            char *decoded = unescape_string(raw, cursor - start - 1, parser->err);
            free(raw);
            *out = make_string_owned(decoded);
            parser->pos = cursor + 1;
            return 1;
        }
        cursor++;
    }

    parser_error(parser, "unterminated string literal");
    return 0;
}

static int parse_number(Parser *parser, Value *out) {
    char *end = NULL;
    errno = 0;
    long value = strtol(parser->text + parser->pos, &end, 10);
    if (end == parser->text + parser->pos) {
        parser_error(parser, "expected a number");
        return 0;
    }
    if (errno == ERANGE) {
        parser_error(parser, "number is out of range");
        return 0;
    }
    // Check for floating point
    if (*end == '.' && isdigit((unsigned char)end[1])) {
        errno = 0;
        double fvalue = strtod(parser->text + parser->pos, &end);
        if (errno == ERANGE) {
            parser_error(parser, "number is out of range");
            return 0;
        }
        parser->pos = (size_t)(end - parser->text);
        *out = make_float(fvalue);
        return 1;
    }
    parser->pos = (size_t)(end - parser->text);
    *out = make_integer(value);
    return 1;
}

static int parse_name(Parser *parser, char **out) {
    skip_spaces(parser);
    if (!is_name_start(parser->text[parser->pos])) {
        parser_error(parser, "expected a name");
        return 0;
    }
    size_t start = parser->pos;
    parser->pos++;
    while (is_name_part(parser->text[parser->pos])) {
        parser->pos++;
    }
    *out = qs_strndup(parser->text + start, parser->pos - start);
    return 1;
}

typedef struct {
    char *name;
    Value value;
    int is_keyword;
} CallArg;

typedef struct {
    CallArg *items;
    size_t count;
    size_t capacity;
} CallArgList;

static void call_arg_push(CallArgList *list, CallArg arg) {
    if (list->count == list->capacity) {
        size_t next_capacity = list->capacity ? list->capacity * 2 : 4;
        CallArg *next = realloc(list->items, next_capacity * sizeof(CallArg));
        if (!next) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        list->items = next;
        list->capacity = next_capacity;
    }
    list->items[list->count++] = arg;
}

static void free_call_args(CallArgList *args) {
    for (size_t i = 0; i < args->count; i++) {
        free(args->items[i].name);
        free_value(args->items[i].value);
    }
    free(args->items);
}

static int type_matches_single(const char *type, Value value) {
    if (!type || strcmp(type, "any") == 0) {
        return 1;
    }
    if (strcmp(type, "int") == 0 || strcmp(type, "integer") == 0) {
        return value.type == VALUE_INTEGER;
    }
    if (strcmp(type, "float") == 0 || strcmp(type, "double") == 0 || strcmp(type, "number") == 0) {
        return value.type == VALUE_FLOAT || value.type == VALUE_INTEGER;
    }
    if (strcmp(type, "str") == 0 || strcmp(type, "string") == 0) {
        return value.type == VALUE_STRING;
    }
    if (strcmp(type, "bool") == 0 || strcmp(type, "boolean") == 0) {
        return value.type == VALUE_BOOL;
    }
    if (strcmp(type, "list") == 0) {
        return value.type == VALUE_LIST;
    }
    if (strcmp(type, "map") == 0 || strcmp(type, "dict") == 0) {
        return value.type == VALUE_DICT;
    }
    if (strcmp(type, "null") == 0) {
        return value.type == VALUE_NULL;
    }
    return 0;
}

static int type_matches(const char *type, Value value) {
    if (!type || strcmp(type, "any") == 0) {
        return 1;
    }
    const char *cursor = type;
    while (*cursor) {
        const char *pipe = strchr(cursor, '|');
        size_t len = pipe ? (size_t)(pipe - cursor) : strlen(cursor);
        char *single = qs_strndup(cursor, len);
        int match = type_matches_single(single, value);
        free(single);
        if (match) return 1;
        if (!pipe) break;
        cursor = pipe + 1;
    }
    return 0;
}

static const char *value_type_name(Value value) {
    if (value.type == VALUE_NULL) {
        return "null";
    }
    if (value.type == VALUE_INTEGER) {
        return "int";
    }
    if (value.type == VALUE_FLOAT) {
        return "float";
    }
    if (value.type == VALUE_STRING) {
        return "string";
    }
    if (value.type == VALUE_BOOL) {
        return "bool";
    }
    if (value.type == VALUE_LIST) {
        return "list";
    }
    if (value.type == VALUE_LAMBDA) {
        return "lambda";
    }
    return "map";
}

static int parse_call_arguments(Parser *parser, CallArgList *args) {
    if (!match_char(parser, '(')) {
        parser_error(parser, "function needs '('");
        return 0;
    }
    skip_spaces(parser);
    if (match_char(parser, ')')) {
        return 1;
    }

    while (1) {
        CallArg arg = {0};
        size_t before = parser->pos;
        skip_spaces(parser);
        if (is_name_start(parser->text[parser->pos])) {
            char *maybe_name;
            if (!parse_name(parser, &maybe_name)) {
                return 0;
            }
            skip_spaces(parser);
            if (parser->text[parser->pos] == '=' && parser->text[parser->pos + 1] != '=') {
                parser->pos++;
                arg.name = maybe_name;
                arg.is_keyword = 1;
                if (!parse_expression(parser, &arg.value)) {
                    free(arg.name);
                    return 0;
                }
            } else {
                free(maybe_name);
                parser->pos = before;
                if (!parse_expression(parser, &arg.value)) {
                    return 0;
                }
            }
        } else {
            if (!parse_expression(parser, &arg.value)) {
                return 0;
            }
        }
        call_arg_push(args, arg);
        skip_spaces(parser);
        if (match_char(parser, ',')) {
            continue;
        }
        if (match_char(parser, ')')) {
            return 1;
        }
        parser_error(parser, "function needs ',' or ')'");
        return 0;
    }
}

static int bind_arg(Parser *parser, Environment *local, Param *param, Value value) {
    if (!type_matches(param->type, value)) {
        char msg[256];
        snprintf(msg, sizeof(msg), "argument '%s' has the wrong type", param->name);
        parser_error(parser, msg);
        return 0;
    }
    env_set(local, param->name, clone_value(value));
    return 1;
}

static int path_is_stdlib(const char *path) {
    if (!path) {
        return 0;
    }
    if (strncmp(path, "stdlib/", 7) == 0 || strncmp(path, "stdlib\\", 7) == 0) {
        return 1;
    }
    return strstr(path, "/stdlib/") != NULL || strstr(path, "\\stdlib\\") != NULL;
}

static void call_stack_push(const char *name, int line) {
    if (g_call_stack_count == g_call_stack_capacity) {
        size_t next_capacity = g_call_stack_capacity ? g_call_stack_capacity * 2 : 8;
        char **next = realloc(g_call_stack, next_capacity * sizeof(char *));
        if (!next) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        g_call_stack = next;
        g_call_stack_capacity = next_capacity;
    }
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s() at line %d", name, line);
    g_call_stack[g_call_stack_count++] = qs_strdup(buffer);
}

static void call_stack_pop(void) {
    if (g_call_stack_count == 0) {
        return;
    }
    free(g_call_stack[--g_call_stack_count]);
}

static void free_call_stack(void) {
    for (size_t i = 0; i < g_call_stack_count; i++) {
        free(g_call_stack[i]);
    }
    free(g_call_stack);
    g_call_stack = NULL;
    g_call_stack_count = 0;
    g_call_stack_capacity = 0;
}

static int call_user_function(Parser *parser, Function *function, CallArgList *args, Value *out) {
    clock_t start_time = 0;
    if (g_profiling) {
        start_time = clock();
        fprintf(stderr, "[PROFILE] %s() called\n", function->name);
    }

    Environment local = {0};

    for (size_t i = 0; i < parser->env->count; i++) {
        env_set(&local, parser->env->items[i].name, clone_value(parser->env->items[i].value));
    }

    size_t positional_index = 0;
    int *used = calloc(args->count ? args->count : 1, sizeof(int));
    if (!used) {
        fprintf(stderr, "out of memory\n");
        exit(2);
    }

    for (size_t p = 0; p < function->param_count; p++) {
        Param *param = &function->params[p];
        if (param->is_args || param->is_kwargs) {
            continue;
        }

        int bound = 0;
        for (size_t a = 0; a < args->count; a++) {
            if (!used[a] && args->items[a].is_keyword && strcmp(args->items[a].name, param->name) == 0) {
                if (!bind_arg(parser, &local, param, args->items[a].value)) {
                    free(used);
                    free_environment(&local);
                    return 0;
                }
                used[a] = 1;
                bound = 1;
                break;
            }
        }

        if (!bound) {
            while (positional_index < args->count && args->items[positional_index].is_keyword) {
                positional_index++;
            }
            if (positional_index < args->count) {
                if (!bind_arg(parser, &local, param, args->items[positional_index].value)) {
                    free(used);
                    free_environment(&local);
                    return 0;
                }
                used[positional_index] = 1;
                positional_index++;
                bound = 1;
            }
        }

        if (!bound) {
            if (param->has_default) {
                if (!bind_arg(parser, &local, param, param->default_value)) {
                    free(used);
                    free_environment(&local);
                    return 0;
                }
                bound = 1;
            }
        }

        if (!bound) {
            char msg[256];
            snprintf(msg, sizeof(msg), "missing argument '%s'", param->name);
            parser_error(parser, msg);
            free(used);
            free_environment(&local);
            return 0;
        }
    }

    for (size_t p = 0; p < function->param_count; p++) {
        Param *param = &function->params[p];
        if (!param->is_args) {
            continue;
        }
        Value args_value = make_list_empty();
        for (size_t a = 0; a < args->count; a++) {
            if (!used[a] && !args->items[a].is_keyword) {
                list_push(&args_value.as.list, clone_value(args->items[a].value));
                used[a] = 1;
            }
        }
        env_set(&local, param->name, args_value);
    }

    for (size_t p = 0; p < function->param_count; p++) {
        Param *param = &function->params[p];
        if (!param->is_kwargs) {
            continue;
        }
        Value kwargs_value = make_dict_empty();
        for (size_t a = 0; a < args->count; a++) {
            if (!used[a] && args->items[a].is_keyword) {
                dict_set(&kwargs_value.as.dict, args->items[a].name, clone_value(args->items[a].value));
                used[a] = 1;
            }
        }
        env_set(&local, param->name, kwargs_value);
    }

    for (size_t a = 0; a < args->count; a++) {
        if (!used[a]) {
            parser_error(parser, args->items[a].is_keyword ? "unexpected keyword argument" : "too many arguments");
            free(used);
            free_environment(&local);
            return 0;
        }
    }
    free(used);

    int previous_return = g_has_return;
    Value previous_value = g_return_value;
    g_has_return = 0;
    g_function_depth++;
    call_stack_push(function->name, parser->line);
    int native_context = path_is_stdlib(function->program ? function->program->path : NULL);
    if (native_context) {
        g_native_access_depth++;
    }
    int ok = execute_range(function->program, &local, function->body_start, function->body_end, parser->err);
    if (native_context) {
        g_native_access_depth--;
    }
    g_function_depth--;

    if (!ok) {
        free_environment(&local);
        g_has_return = previous_return;
        g_return_value = previous_value;
        return 0;
    }

    if (g_has_return) {
        *out = clone_value(g_return_value);
        free_value(g_return_value);
    } else {
        *out = make_null();
    }
    call_stack_pop();
    g_has_return = previous_return;
    g_return_value = previous_value;
    free_environment(&local);

    if (g_profiling) {
        clock_t end_time = clock();
        double elapsed = (double)(end_time - start_time) / CLOCKS_PER_SEC * 1000.0;
        fprintf(stderr, "[PROFILE] %s() returned in %.3f ms\n", function->name, elapsed);
    }

    return 1;
}

static int call_lambda(Parser *parser, Value *lambda, CallArgList *args, Value *out) {
    if (lambda->type != VALUE_LAMBDA) {
        parser_error(parser, "not a lambda");
        return 0;
    }
    if (args->count != lambda->as.lambda.param_count) {
        char msg[256];
        snprintf(msg, sizeof(msg), "lambda needs %zu arguments, got %zu", lambda->as.lambda.param_count, args->count);
        parser_error(parser, msg);
        return 0;
    }

    // Type check parameters
    for (size_t i = 0; i < lambda->as.lambda.param_count; i++) {
        if (lambda->as.lambda.param_types && lambda->as.lambda.param_types[i]) {
            if (!type_matches(lambda->as.lambda.param_types[i], args->items[i].value)) {
                char msg[256];
                snprintf(msg, sizeof(msg), "argument '%s' has wrong type", lambda->as.lambda.param_names[i]);
                parser_error(parser, msg);
                return 0;
            }
        }
    }

    // Create local environment from closure
    Environment local = {0};
    if (lambda->as.lambda.closure) {
        for (size_t i = 0; i < lambda->as.lambda.closure->count; i++) {
            env_set(&local, lambda->as.lambda.closure->items[i].name,
                    clone_value(lambda->as.lambda.closure->items[i].value));
        }
    }

    // Bind parameters
    for (size_t i = 0; i < lambda->as.lambda.param_count; i++) {
        env_set(&local, lambda->as.lambda.param_names[i], clone_value(args->items[i].value));
    }

    // Parse and evaluate body
    Parser lambda_parser = {lambda->as.lambda.body, 0, parser->line, &local, parser->err, -1};

    int saved_module_mode = g_module_mode;
    if (lambda->as.lambda.is_module) g_module_mode = 1;

    skip_spaces(&lambda_parser);
    Value result;
    if (!parse_expression(&lambda_parser, &result)) {
        g_module_mode = saved_module_mode;
        free_environment(&local);
        return 0;
    }

    g_module_mode = saved_module_mode;
    *out = result;
    free_environment(&local);
    return 1;
}

static int line_simple_starts_with_word(const char *line, const char *word) {
    char *trimmed = trim_copy(line, strlen(line));
    size_t len = strlen(word);
    int ok = strncmp(trimmed, word, len) == 0 && !is_name_part(trimmed[len]);
    free(trimmed);
    return ok;
}

static int line_is_simple_exit_call(const char *line) {
    char *trimmed = trim_copy(line, strlen(line));
    int ok = strncmp(trimmed, "exit", 4) == 0 && !is_name_part(trimmed[4]);
    if (ok) {
        size_t pos = 4;
        while (isspace((unsigned char)trimmed[pos])) {
            pos++;
        }
        ok = trimmed[pos] == '(';
    }
    free(trimmed);
    return ok;
}

static int line_is_simple_while_true(const char *line) {
    char *trimmed = trim_copy(line, strlen(line));
    int ok = strncmp(trimmed, "while", 5) == 0 && !is_name_part(trimmed[5]);
    if (ok) {
        size_t pos = 5;
        while (isspace((unsigned char)trimmed[pos])) {
            pos++;
        }
        ok = strncmp(trimmed + pos, "true", 4) == 0 && !is_name_part(trimmed[pos + 4]) &&
             strchr(trimmed + pos + 4, '{') != NULL;
    }
    free(trimmed);
    return ok;
}

static int line_brace_delta_simple(const char *line) {
    int delta = 0;
    int in_string = 0;
    char quote = '\0';
    for (size_t i = 0; line[i]; i++) {
        char ch = line[i];
        if (in_string) {
            if (ch == quote) {
                in_string = 0;
            }
            continue;
        }
        if (ch == '\'' || ch == '"') {
            in_string = 1;
            quote = ch;
            continue;
        }
        if (ch == '#') {
            break;
        }
        if (ch == '{') {
            delta++;
        } else if (ch == '}') {
            delta--;
        }
    }
    return delta;
}

static int function_range_has_return(Function *function, size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
        if (line_simple_starts_with_word(function->program->items[i], "return")) {
            return 1;
        }
    }
    return 0;
}

static int find_simple_block_end(Function *function, size_t open_line, size_t *close_line) {
    int depth = 0;
    for (size_t i = open_line; i < function->body_end; i++) {
        depth += line_brace_delta_simple(function->program->items[i]);
        if (i > open_line && depth <= 0) {
            *close_line = i;
            return 1;
        }
    }
    return 0;
}

static int user_function_ever_returns(Function *function) {
    int depth = 0;
    for (size_t i = function->body_start; i < function->body_end; i++) {
        const char *line = function->program->items[i];
        if (depth == 0) {
            char *trimmed = trim_copy(line, strlen(line));
            int empty = trimmed[0] == '\0' || trimmed[0] == '#';
            free(trimmed);
            if (empty) {
                continue;
            }
            if (line_simple_starts_with_word(line, "return")) {
                return 1;
            }
            if (line_is_simple_exit_call(line)) {
                return 0;
            }
            if (line_is_simple_while_true(line)) {
                size_t close_line = i;
                if (find_simple_block_end(function, i, &close_line)) {
                    return function_range_has_return(function, i + 1, close_line);
                }
                return 0;
            }
        }
        depth += line_brace_delta_simple(line);
        if (depth < 0) {
            depth = 0;
        }
    }
    return 1;
}

static int ever_return_name(const char *name, int *out) {
    if (strcmp(name, "exit") == 0) {
        *out = 0;
        return 1;
    }
    Function *function = find_function(name);
    if (function) {
        *out = user_function_ever_returns(function);
        return 1;
    }
    return 0;
}
