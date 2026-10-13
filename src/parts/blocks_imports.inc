static int find_assignment(const char *line_text, size_t *assignment_pos, char *assign_op) {
    int in_string = 0;
    char string_quote = '\0';
    int bracket_depth = 0;
    int paren_depth = 0;

    for (size_t i = 0; line_text[i]; i++) {
        char ch = line_text[i];
        if (in_string && ch == string_quote && quote_can_close(line_text, i, ']')) {
            in_string = 0;
            string_quote = '\0';
            continue;
        }
        if ((ch == '\'' || ch == '"') && !in_string) {
            in_string = 1;
            string_quote = ch;
            continue;
        }
        if (in_string) {
            continue;
        }
        if (ch == '#') {
            break;
        }
        if (ch == '[') {
            bracket_depth++;
        } else if (ch == ']') {
            bracket_depth--;
        } else if (ch == '(') {
            paren_depth++;
        } else if (ch == ')') {
            paren_depth--;
        } else if (strchr("+-*/%", ch) && line_text[i + 1] == '=' && bracket_depth == 0 && paren_depth == 0) {
            *assignment_pos = i;
            *assign_op = ch;
            return 1;
        } else if (ch == '=' && bracket_depth == 0 && paren_depth == 0 &&
                   line_text[i + 1] != '=' && (i == 0 || (line_text[i - 1] != '!' && line_text[i - 1] != '<' && line_text[i - 1] != '>' && !strchr("+-*/%", line_text[i - 1])))) {
            *assignment_pos = i;
            *assign_op = '=';
            return 1;
        }
    }
    return 0;
}

static int line_starts_with(const char *line_text, const char *word, size_t *after_word) {
    size_t pos = 0;
    skip_spaces_text(line_text, &pos);
    size_t len = strlen(word);
    if (strncmp(line_text + pos, word, len) != 0 || is_name_part(line_text[pos + len])) {
        return 0;
    }
    *after_word = pos + len;
    return 1;
}

static int line_is_close_brace(const char *line_text) {
    size_t pos = 0;
    skip_spaces_text(line_text, &pos);
    if (line_text[pos] != '}') {
        return 0;
    }
    pos++;
    return rest_is_space_or_comment(line_text, pos);
}

static int line_is_close_else(const char *line_text, size_t *else_pos) {
    size_t pos = 0;
    skip_spaces_text(line_text, &pos);
    if (line_text[pos] != '}') {
        return 0;
    }
    pos++;
    skip_spaces_text(line_text, &pos);
    if (strncmp(line_text + pos, "else", 4) != 0 || is_name_part(line_text[pos + 4])) {
        return 0;
    }
    *else_pos = pos;
    return 1;
}

static int line_is_close_word(const char *line_text, const char *word, size_t *word_pos) {
    size_t pos = 0;
    skip_spaces_text(line_text, &pos);
    if (line_text[pos] != '}') {
        return 0;
    }
    pos++;
    skip_spaces_text(line_text, &pos);
    size_t len = strlen(word);
    if (strncmp(line_text + pos, word, len) != 0 || is_name_part(line_text[pos + len])) {
        return 0;
    }
    *word_pos = pos;
    return 1;
}

static int line_starts_with_oops(const char *line_text, size_t *after_pos) {
    size_t pos = 0;
    skip_spaces_text(line_text, &pos);
    if (strncmp(line_text + pos, "oops", 4) != 0) {
        return 0;
    }
    pos += 4;
    skip_spaces_text(line_text, &pos);
    if (line_text[pos] == '{') {
        *after_pos = pos;
        return 1;
    }
    if (is_name_start(line_text[pos])) {
        while (is_name_part(line_text[pos])) pos++;
        skip_spaces_text(line_text, &pos);
        if (line_text[pos] == '{') {
            *after_pos = pos;
            return 1;
        }
    }
    return 0;
}

static int line_is_empty_or_comment(const char *line_text) {
    size_t pos = 0;
    skip_spaces_text(line_text, &pos);
    return line_text[pos] == '\0' || line_text[pos] == '#';
}

static int line_has_open_brace(const char *line_text, size_t from, size_t *brace_pos) {
    int in_string = 0;
    char quote = '\0';
    for (size_t i = from; line_text[i]; i++) {
        char ch = line_text[i];
        if (in_string && ch == quote && quote_can_close(line_text, i, ']')) {
            in_string = 0;
            quote = '\0';
            continue;
        }
        if ((ch == '\'' || ch == '"') && !in_string) {
            in_string = 1;
            quote = ch;
            continue;
        }
        if (in_string) {
            continue;
        }
        if (ch == '#') {
            break;
        }
        if (ch == '{') {
            *brace_pos = i;
            return 1;
        }
    }
    return 0;
}

static int execute_range(Program *program, Environment *env, size_t start, size_t end, ParseError *err);

static int find_block_end(Program *program, size_t open_line, size_t end, size_t *close_line, ParseError *err) {
    {
        const char *open_text = program->items[open_line];
        size_t open_len = strlen(open_text);

        size_t brace_pos = 0;
        while (brace_pos < open_len && open_text[brace_pos] != '{') brace_pos++;
        if (brace_pos < open_len) {
            int depth = 1;
            for (size_t j = brace_pos + 1; j < open_len; j++) {
                if (open_text[j] == '{') depth++;
                else if (open_text[j] == '}') {
                    depth--;
                    if (depth == 0) {
                        *close_line = open_line;
                        return 1;
                    }
                }
            }
        }
    }

    int depth = 1;
    for (size_t i = open_line + 1; i < end; i++) {
        size_t after = 0;
        size_t brace = 0;
        int found_opener = 0;
        if (line_starts_with(program->items[i], "repeat", &after) && line_has_open_brace(program->items[i], after, &brace)) {
            depth++;
            found_opener = 1;
        } else if (line_starts_with(program->items[i], "while", &after) && line_has_open_brace(program->items[i], after, &brace)) {
            depth++;
            found_opener = 1;
        } else if (line_starts_with(program->items[i], "if", &after) && line_has_open_brace(program->items[i], after, &brace)) {
            depth++;
            found_opener = 1;
        } else if (line_starts_with(program->items[i], "try", &after) && line_has_open_brace(program->items[i], after, &brace)) {
            depth++;
            found_opener = 1;
        } else if (line_starts_with(program->items[i], "func", &after) && line_has_open_brace(program->items[i], after, &brace)) {
            depth++;
            found_opener = 1;
        } else if (line_starts_with(program->items[i], "class", &after) && line_has_open_brace(program->items[i], after, &brace)) {
            depth++;
            found_opener = 1;
        } else if (line_starts_with(program->items[i], "maybe", &after) && line_has_open_brace(program->items[i], after, &brace)) {
            depth++;
            found_opener = 1;
        } else if (line_starts_with(program->items[i], "else", &after) && line_has_open_brace(program->items[i], after, &brace)) {
            depth++;
            found_opener = 1;
        } else if (line_starts_with_oops(program->items[i], &after) && line_has_open_brace(program->items[i], after, &brace)) {
            depth++;
        }

        if (found_opener) {
            const char *line = program->items[i];
            for (size_t k = 0; line[k]; k++) {
                if (line[k] == '}') {
                    depth--;
                    if (depth == 0) {
                        *close_line = i;
                        return 1;
                    }
                }
            }
        } else if (line_is_close_word(program->items[i], "maybe", &after)) {
            depth--;
            if (depth == 0) {
                *close_line = i;
                return 1;
            }
            if (line_has_open_brace(program->items[i], after + strlen("maybe"), &brace)) {
                depth++;
            }
        } else if (line_is_close_else(program->items[i], &after)) {
            depth--;
            if (depth == 0) {
                *close_line = i;
                return 1;
            }
            if (line_has_open_brace(program->items[i], after + strlen("else"), &brace)) {
                depth++;
            }
        } else if (line_is_close_word(program->items[i], "oops", &after)) {
            depth--;
            if (depth == 0) {
                *close_line = i;
                return 1;
            }
            if (line_has_open_brace(program->items[i], after + strlen("oops"), &brace)) {
                depth++;
            }
        } else if (line_is_close_brace(program->items[i])) {
            depth--;
            if (depth == 0) {
                *close_line = i;
                return 1;
            }
        }
    }
    set_error(err, (int)open_line + 1, 1, "missing '}' for this block");
    return 0;
}

static int eval_expression_text(Environment *env, const char *text, size_t len, int line, Value *out, ParseError *err) {
    char *expr = trim_copy(text, len);
    Parser parser = {expr, 0, line, env, err, -1};
    int ok = parse_expression(&parser, out);
    if (ok && !rest_is_space_or_comment(parser.text, parser.pos)) {
        parser_error(&parser, "unexpected text after expression");
        free_value(*out);
        ok = 0;
    }
    free(expr);
    return ok;
}

static int find_arrow(const char *text, size_t start, size_t end, size_t *arrow_pos) {
    int in_string = 0;
    char quote = '\0';
    int bracket_depth = 0;
    int paren_depth = 0;

    for (size_t i = start; i + 2 <= end; i++) {
        char ch = text[i];
        if (in_string && ch == quote && quote_can_close(text, i, ']')) {
            in_string = 0;
            quote = '\0';
            continue;
        }
        if (!in_string && (ch == '\'' || ch == '"')) {
            in_string = 1;
            quote = ch;
            continue;
        }
        if (in_string) {
            continue;
        }
        if (ch == '[') {
            bracket_depth++;
        } else if (ch == ']') {
            bracket_depth--;
        } else if (ch == '(') {
            paren_depth++;
        } else if (ch == ')') {
            paren_depth--;
        }
        if (bracket_depth == 0 && paren_depth == 0 && i + 2 <= end && strncmp(text + i, "->", 2) == 0) {
            *arrow_pos = i;
            return 1;
        }
    }
    return 0;
}

static int parse_function_definition(Program *program, size_t line, size_t close_line, int force_push, ParseError *err) {
    Environment default_env = {0};
    Parser parser = {program->items[line], 0, (int)line + 1, &default_env, err, -1};
    match_word(&parser, "func");

    Function function = {0};
    if (!parse_name(&parser, &function.name)) {
        return 0;
    }
    if (!match_char(&parser, '(')) {
        parser_error(&parser, "func needs '('");
        free(function.name);
        return 0;
    }

    skip_spaces(&parser);
    while (parser.text[parser.pos] && parser.text[parser.pos] != ')') {
        Param param = {0};
        if (match_op(&parser, "**")) {
            param.is_kwargs = 1;
        } else if (match_char(&parser, '*')) {
            param.is_args = 1;
        }

        if (!parse_name(&parser, &param.name)) {
            free(function.name);
            return 0;
        }
        skip_spaces(&parser);
        if (match_char(&parser, ':')) {
            char *type_buf = NULL;
            size_t type_len = 0;
            char *first_type = NULL;
            if (!parse_name(&parser, &first_type)) {
                free(function.name);
                free(param.name);
                return 0;
            }
            size_t flen = strlen(first_type);
            type_buf = realloc(type_buf, flen + 1);
            strcpy(type_buf, first_type);
            type_len = flen;
            free(first_type);
            skip_spaces(&parser);
            while (match_char(&parser, '|')) {
                skip_spaces(&parser);
                char *alt_type = NULL;
                if (!parse_name(&parser, &alt_type)) {
                    free(function.name);
                    free(param.name);
                    free(type_buf);
                    return 0;
                }
                size_t alen = strlen(alt_type);
                type_buf = realloc(type_buf, type_len + 1 + alen + 1);
                type_buf[type_len] = '|';
                strcpy(type_buf + type_len + 1, alt_type);
                type_len += 1 + alen;
                free(alt_type);
                skip_spaces(&parser);
            }
            param.type = type_buf ? type_buf : qs_strdup("");
        }
        skip_spaces(&parser);
        if (match_char(&parser, '=')) {
            if (param.is_args || param.is_kwargs) {
                parser_error(&parser, "*args and **kwargs cannot have defaults");
                free(function.name);
                free(param.name);
                free(param.type);
                return 0;
            }
            if (!parse_expression(&parser, &param.default_value)) {
                free(function.name);
                free(param.name);
                free(param.type);
                return 0;
            }
            param.has_default = 1;
        }
        param_push(&function, param);
        skip_spaces(&parser);
        if (match_char(&parser, ',')) {
            skip_spaces(&parser);
            continue;
        }
        break;
    }

    if (!match_char(&parser, ')')) {
        parser_error(&parser, "func needs ')'");
        free(function.name);
        return 0;
    }

    size_t brace = 0;
    if (!line_has_open_brace(program->items[line], parser.pos, &brace)) {
        parser_error(&parser, "func needs '{'");
        free(function.name);
        return 0;
    }

    if (!rest_is_space_or_comment(program->items[line], brace + 1)) {
        if (close_line != line) {
            set_error(err, (int)line + 1, (int)brace + 2, "nothing can appear after '{'");
            free(function.name);
            return 0;
        }

        const char *lt = program->items[line];
        int bd = 1;
        size_t end_pos = brace + 1;
        while (end_pos < strlen(lt) && bd > 0) {
            if (lt[end_pos] == '{') bd++;
            else if (lt[end_pos] == '}') { bd--; if (bd == 0) break; }
            end_pos++;
        }

        function.body_start = line;
        function.body_end = line + 1;

        size_t body_len = end_pos - brace - 1;
        char *body_text = malloc(body_len + 1);
        memcpy(body_text, lt + brace + 1, body_len);
        body_text[body_len] = '\0';

        Program body_prog = {0};
        body_prog.path = qs_strdup("<func_body>");
        body_prog.dir = qs_strdup(".");
        split_text_to_program(body_text, &body_prog);
        free(body_text);
        if (body_prog.count == 0) {
            program_push(&body_prog, qs_strdup(""));
        }
        function.program = calloc(1, sizeof(Program));
        *function.program = body_prog;
        function.is_module = g_module_mode;
        function.body_start = 0;
        function.body_end = body_prog.count;
        if (g_module_mode && !force_push) {
            function_push_if_new(function);
        } else {
            function_push(function);
        }
        return 1;
    }

    function.body_start = line + 1;
    function.body_end = close_line;
    function.program = program;
    function.is_module = g_module_mode;
    if (g_module_mode && !force_push) {
        function_push_if_new(function);
    } else {
        function_push(function);
    }
    return 1;
}

static void collect_class_methods(Program *program, size_t class_line, ParseError *err) {
    char *line_text = program->items[class_line];
    size_t line_len = strlen(line_text);
    Parser parser = {line_text, 0, (int)class_line + 1, NULL, err, -1};
    if (!match_word(&parser, "class")) return;
    char *class_name = NULL;
    if (!parse_name(&parser, &class_name)) return;

    while (parser.pos < line_len && line_text[parser.pos] != '{') parser.pos++;
    if (parser.pos >= line_len) { free(class_name); return; }
    size_t body_start = parser.pos + 1;
    size_t body_end = line_len;

    int cdepth = 1;
    size_t pos = body_start;
    while (pos < line_len && cdepth > 0) {
        if (line_text[pos] == '{') cdepth++;
        else if (line_text[pos] == '}') { cdepth--; if (cdepth == 0) { body_end = pos; break; } }
        pos++;
    }
    pos = body_start;
    while (pos < body_end) {
        while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t' || line_text[pos] == '\n' || line_text[pos] == '\r')) pos++;
        if (pos >= body_end) break;
        int found_method = 0;
        if (strncmp(line_text + pos, "func", 4) == 0 && !is_name_part(line_text[pos + 4])) { found_method = 1; pos += 4; }
        else if (strncmp(line_text + pos, "fn", 2) == 0 && !is_name_part(line_text[pos + 2])) { found_method = 1; pos += 2; }
        else if (is_name_start(line_text[pos])) {
            size_t check = pos;
            while (check < body_end && is_name_part(line_text[check])) check++;
            if (check < body_end && line_text[check] == '(') found_method = 1;
        }
        if (!found_method) { pos++; continue; }
        while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t')) pos++;
        if (pos >= body_end || !is_name_start(line_text[pos])) continue;
        size_t mname_start = pos;
        while (pos < body_end && is_name_part(line_text[pos])) pos++;
        size_t mname_len = pos - mname_start;
        char *method_name = qs_strndup(line_text + mname_start, mname_len);
        size_t qlen = strlen(class_name) + 1 + mname_len + 1;
        char *qname = malloc(qlen);
        snprintf(qname, qlen, "%s.%s", class_name, method_name);
        free(method_name);

        Function function = {0};
        function.name = qname;
        Param self_param = {0};
        self_param.name = qs_strdup("self");
        self_param.type = qs_strdup("any");
        param_push(&function, self_param);

        while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t')) pos++;
        if (pos < body_end && line_text[pos] == '(') {
            pos++;
            while (pos < body_end && line_text[pos] != ')') {
                while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t')) pos++;
                if (pos >= body_end || line_text[pos] == ')') break;
                Param param = {0};
                if (line_text[pos] == '*') {
                    pos++;
                    if (pos < body_end && line_text[pos] == '*') { param.is_kwargs = 1; pos++; }
                    else param.is_args = 1;
                }
                size_t pns = pos;
                while (pos < body_end && is_name_part(line_text[pos])) pos++;
                param.name = qs_strndup(line_text + pns, pos - pns);
                while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t')) pos++;
                if (pos < body_end && line_text[pos] == ':') {
                    pos++;
                    while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t')) pos++;
                    size_t ts = pos;
                    while (pos < body_end && (is_name_part(line_text[pos]) || line_text[pos] == '|')) pos++;
                    param.type = qs_strndup(line_text + ts, pos - ts);
                }
                param_push(&function, param);
                while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t')) pos++;
                if (pos < body_end && line_text[pos] == ',') { pos++; continue; }
                break;
            }
            if (pos < body_end && line_text[pos] == ')') pos++;
        }
        while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t')) pos++;
        if (pos + 1 < body_end && line_text[pos] == '-' && line_text[pos + 1] == '>') {
            pos += 2;
            while (pos < body_end && line_text[pos] != '{') pos++;
        }
        if (pos >= body_end || line_text[pos] != '{') {
            for (size_t p = 0; p < function.param_count; p++) {
                free(function.params[p].name);
                free(function.params[p].type);
            }
            free(function.params);
            free(function.name);
            continue;
        }
        int mdepth = 1;
        size_t mbs = pos + 1;
        size_t mbe = mbs;
        while (mbe < body_end && mdepth > 0) {
            if (line_text[mbe] == '{') mdepth++;
            else if (line_text[mbe] == '}') mdepth--;
            if (mdepth > 0) mbe++;
        }
        size_t blen = mbe - mbs;
        char *body_raw = malloc(blen + 1);
        if (blen > 0) memcpy(body_raw, line_text + mbs, blen);
        body_raw[blen] = '\0';
        Program *method_prog = calloc(1, sizeof(Program));
        method_prog->path = qs_strdup("<class_method>");
        method_prog->dir = qs_strdup(".");
        split_text_to_program(body_raw, method_prog);
        if (method_prog->count == 0) program_push(method_prog, qs_strdup(""));
        free(body_raw);
        function.body_start = 0;
        function.body_end = method_prog->count;
        function.program = method_prog;
        function.is_module = g_module_mode;
        function_push(function);
        pos = mbe + 1;
    }
    free(class_name);
}

static int collect_functions(Program *program, ParseError *err) {
    for (size_t i = 0; i < program->count; i++) {
        size_t after = 0;
        size_t brace = 0;
        if (line_starts_with(program->items[i], "func", &after) && line_has_open_brace(program->items[i], after, &brace)) {
            size_t close_line = 0;
            if (!find_block_end(program, i, program->count, &close_line, err)) {
                return 0;
            }
            if (!parse_function_definition(program, i, close_line, 0, err)) {
                return 0;
            }
            i = close_line;
        } else if (line_starts_with(program->items[i], "class", &after) && line_has_open_brace(program->items[i], after, &brace)) {
            size_t close_line = 0;
            if (!find_block_end(program, i, program->count, &close_line, err)) {
                return 0;
            }
            collect_class_methods(program, i, err);
            i = close_line;
        }
    }
    return 1;
}

typedef struct {
    char *path;
    char **names;
    size_t count;
    size_t capacity;
    int import_all;
} ImportSpec;

static void import_name_push(ImportSpec *spec, char *name) {
    if (spec->count == spec->capacity) {
        size_t next_capacity = spec->capacity ? spec->capacity * 2 : 4;
        char **next = realloc(spec->names, next_capacity * sizeof(char *));
        if (!next) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        spec->names = next;
        spec->capacity = next_capacity;
    }
    spec->names[spec->count++] = name;
}

static void free_import_spec(ImportSpec *spec) {
    free(spec->path);
    for (size_t i = 0; i < spec->count; i++) {
        free(spec->names[i]);
    }
    free(spec->names);
}

static int import_wants(ImportSpec *spec, const char *name) {
    if (spec->import_all) {
        return 1;
    }
    for (size_t i = 0; i < spec->count; i++) {
        if (strcmp(spec->names[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int parse_import_line(Program *program, size_t line, ImportSpec *spec, ParseError *err) {
    Parser parser = {program->items[line], 0, (int)line + 1, NULL, err, -1};
    if (!match_word(&parser, "from")) {
        return 0;
    }

    skip_spaces(&parser);
    Value path_value;
    if (!parse_string(&parser, &path_value)) {
        return 0;
    }

    spec->path = qs_strdup(path_value.as.string);
    free_value(path_value);

    if (!match_word(&parser, "import")) {
        parser_error(&parser, "import needs the word 'import'");
        return 0;
    }

    skip_spaces(&parser);
    if (match_char(&parser, '*')) {
        spec->import_all = 1;
        if (!rest_is_space_or_comment(parser.text, parser.pos)) {
            parser_error(&parser, "nothing can appear after import *");
            return 0;
        }
        return 1;
    }

    while (1) {
        char *name = NULL;
        if (!parse_name(&parser, &name)) {
            return 0;
        }
        import_name_push(spec, name);
        skip_spaces(&parser);
        if (match_char(&parser, ',')) {
            continue;
        }
        break;
    }

    if (!rest_is_space_or_comment(parser.text, parser.pos)) {
        parser_error(&parser, "unexpected text after import");
        return 0;
    }
    return 1;
}

static int function_name_on_line(const char *line_text, char **name, ParseError *err, int line) {
    Parser parser = {line_text, 0, line, NULL, err, -1};
    if (!match_word(&parser, "func")) {
        return 0;
    }
    return parse_name(&parser, name);
}

static int import_assignment(Environment *env, Program *program, size_t line, size_t assignment_pos, ParseError *err) {
    char *name = trim_copy(program->items[line], assignment_pos);
    Value value;
    int native_context = path_is_stdlib(program ? program->path : NULL);
    if (native_context) {
        g_native_access_depth++;
    }
    int ok = eval_expression_text(env, program->items[line] + assignment_pos + 1, strlen(program->items[line] + assignment_pos + 1), (int)line + 1, &value, err);
    if (native_context) {
        g_native_access_depth--;
    }
    if (ok) {
        env_set(env, name, value);
    }
    free(name);
    return ok;
}

static int collect_imports(Program *program, Environment *env, ParseError *err);

static int import_from_module(Program *program, Environment *env, size_t line, ParseError *err) {
    ImportSpec spec = {0};
    if (!parse_import_line(program, line, &spec, err)) {
        free_import_spec(&spec);
        return 0;
    }

    int saved_module_mode = g_module_mode;
    g_module_mode = 1;
    int result = 0;

    Program loaded = {0};

    char *resolved_path = qs_resolve_import(spec.path, program ? program->dir : NULL);
    if (!load_program_file(resolved_path, &loaded)) {
        set_error(err, (int)line + 1, 1, "cannot open imported module");
        free(resolved_path);
        goto import_cleanup;
    }
    free(resolved_path);
    strip_block_comments(&loaded);
    Program *module = module_push(loaded);

    if (!collect_imports(module, env, err)) {
        goto import_cleanup;
    }

    {
    int *found = calloc(spec.count ? spec.count : 1, sizeof(int));
    if (!found) {
        fprintf(stderr, "out of memory\n");
        exit(2);
    }

    for (size_t i = 0; i < module->count; i++) {
        size_t after = 0;
        size_t brace = 0;
        if (line_starts_with(module->items[i], "from", &after)) {
            continue;
        }
        if (line_starts_with(module->items[i], "func", &after) && line_has_open_brace(module->items[i], after, &brace)) {
            char *name = NULL;
            if (!function_name_on_line(module->items[i], &name, err, (int)i + 1)) {
                free(found);
                goto import_cleanup;
            }
            size_t close_line = 0;
            if (!find_block_end(module, i, module->count, &close_line, err)) {
                free(name);
                free(found);
                goto import_cleanup;
            }

            if (!parse_function_definition(module, i, close_line, import_wants(&spec, name), err)) {
                free(name);
                free(found);
                goto import_cleanup;
            }
            if (import_wants(&spec, name)) {
                for (size_t n = 0; n < spec.count; n++) {
                    if (strcmp(spec.names[n], name) == 0) {
                        found[n] = 1;
                    }
                }
            }
            free(name);
            i = close_line;
            continue;
        }
        if ((line_starts_with(module->items[i], "if", &after) ||
             line_starts_with(module->items[i], "repeat", &after) ||
             line_starts_with(module->items[i], "while", &after) ||
             line_starts_with(module->items[i], "try", &after)) &&
            line_has_open_brace(module->items[i], after, &brace)) {
            size_t close_line = 0;
            if (!find_block_end(module, i, module->count, &close_line, err)) {
                free(found);
                goto import_cleanup;
            }
            i = close_line;
            continue;
        }

        size_t assignment_pos = 0;
        char assign_op = '=';
        if (find_assignment(module->items[i], &assignment_pos, &assign_op) && assign_op == '=') {
            char *name = trim_copy(module->items[i], assignment_pos);
            if (import_wants(&spec, name)) {
                if (!import_assignment(env, module, i, assignment_pos, err)) {
                    free(name);
                    free(found);
                    goto import_cleanup;
                }
                for (size_t n = 0; n < spec.count; n++) {
                    if (strcmp(spec.names[n], name) == 0) {
                        found[n] = 1;
                    }
                }
            }
            free(name);
        }
    }

    for (size_t n = 0; n < spec.count; n++) {
        if (!found[n]) {
            char msg[256];
            snprintf(msg, sizeof(msg), "module does not export '%s'", spec.names[n]);
            set_error(err, (int)line + 1, 1, msg);
            free(found);
            goto import_cleanup;
        }
    }

    free(found);
    result = 1;
    }

import_cleanup:
    free_import_spec(&spec);
    g_module_mode = saved_module_mode;
    return result;
}

static int collect_imports(Program *program, Environment *env, ParseError *err) {
    for (size_t i = 0; i < program->count; i++) {
        size_t after = 0;
        size_t brace = 0;
        if (line_starts_with(program->items[i], "from", &after)) {
            if (!import_from_module(program, env, i, err)) {
                return 0;
            }
        } else if ((line_starts_with(program->items[i], "func", &after) ||
                    line_starts_with(program->items[i], "if", &after) ||
                    line_starts_with(program->items[i], "repeat", &after) ||
                    line_starts_with(program->items[i], "while", &after) ||
                    line_starts_with(program->items[i], "try", &after)) &&
                   line_has_open_brace(program->items[i], after, &brace)) {
            size_t close_line = 0;
            if (!find_block_end(program, i, program->count, &close_line, err)) {
                return 0;
            }
            i = close_line;
        }
    }
    return 1;
}
