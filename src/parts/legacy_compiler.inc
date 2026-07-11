static void compiler_mkdir_build(void) {
#ifdef _WIN32
    _mkdir("build");
#else
    mkdir("build", 0755);
#endif
}

static void write_c_string(FILE *out, const char *text) {
    fputc('"', out);
    for (size_t i = 0; text[i]; i++) {
        unsigned char ch = (unsigned char)text[i];
        if (ch == '\\') {
            fputs("\\\\", out);
        } else if (ch == '"') {
            fputs("\\\"", out);
        } else if (ch == '\n') {
            fputs("\\n", out);
        } else if (ch == '\r') {
            fputs("\\r", out);
        } else if (ch == '\t') {
            fputs("\\t", out);
        } else if (ch < 32 || ch > 126) {
            fprintf(out, "\\x%02x", ch);
        } else {
            fputc(ch, out);
        }
    }
    fputc('"', out);
}

static int compiler_is_identifier(const char *text) {
    if (!is_name_start(text[0])) {
        return 0;
    }
    for (size_t i = 1; text[i]; i++) {
        if (!is_name_part(text[i])) {
            return 0;
        }
    }
    return 1;
}

typedef enum {
    COMPILER_VAR_UNKNOWN,
    COMPILER_VAR_INT
} CompilerVarKind;

typedef struct {
    char *name;
    CompilerVarKind kind;
} CompilerVar;

typedef struct {
    CompilerVar *items;
    size_t count;
    size_t capacity;
} CompilerState;

typedef struct {
    char *name;
    char op;
    long rhs;
    int line;
} CompilerIntOp;

typedef struct {
    char *name;
    char compare[3];
    long rhs;
} CompilerIntCondition;

static int compiler_parse_int_literal(const char *text, long *out) {
    char *trimmed = trim_copy(text, strlen(text));
    char *end = NULL;
    errno = 0;
    long number = strtol(trimmed, &end, 10);
    int ok = trimmed[0] && end && *end == '\0' && errno != ERANGE;
    if (ok) {
        *out = number;
    }
    free(trimmed);
    return ok;
}

static int compiler_find_var(CompilerState *state, const char *name) {
    for (size_t i = 0; i < state->count; i++) {
        if (strcmp(state->items[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static CompilerVarKind compiler_get_var_kind(CompilerState *state, const char *name) {
    int index = compiler_find_var(state, name);
    return index >= 0 ? state->items[index].kind : COMPILER_VAR_UNKNOWN;
}

static void compiler_set_var_kind(CompilerState *state, const char *name, CompilerVarKind kind) {
    int index = compiler_find_var(state, name);
    if (index >= 0) {
        state->items[index].kind = kind;
        return;
    }
    if (state->count == state->capacity) {
        size_t next_capacity = state->capacity ? state->capacity * 2 : 16;
        CompilerVar *next = realloc(state->items, next_capacity * sizeof(CompilerVar));
        if (!next) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        state->items = next;
        state->capacity = next_capacity;
    }
    state->items[state->count].name = qs_strdup(name);
    state->items[state->count].kind = kind;
    state->count++;
}

static void compiler_forget_all_vars(CompilerState *state) {
    for (size_t i = 0; i < state->count; i++) {
        state->items[i].kind = COMPILER_VAR_UNKNOWN;
    }
}

static void compiler_free_state(CompilerState *state) {
    for (size_t i = 0; i < state->count; i++) {
        free(state->items[i].name);
    }
    free(state->items);
}

static void compiler_emit_slot_set_int(FILE *out, const char *name, long value, int line) {
    fprintf(out, "    qs_slot = env_get_slot(&env, ");
    write_c_string(out, name);
    fprintf(out, ");\n");
    fprintf(out, "    if (!qs_slot) { set_error(&err, %d, 1, \"cannot store variable\"); goto qs_error; }\n", line);
    fprintf(out, "    free_value(qs_slot->value);\n");
    fprintf(out, "    qs_slot->value = make_integer(%ld);\n", value);
}

static void compiler_emit_define_guard(FILE *out, const char *name, int line) {
    if (!is_define_name(name)) {
        return;
    }
    fprintf(out, "    if (find_variable(&env, ");
    write_c_string(out, name);
    fprintf(out, ") >= 0) { set_error(&err, %d, 1, \"definition cannot be changed\"); goto qs_error; }\n", line);
}

static void compiler_emit_slot_int_op(FILE *out, const char *name, char op, long rhs, int line, const char *slot_name) {
    fprintf(out, "    %s = env_get_slot(&env, ", slot_name);
    write_c_string(out, name);
    fprintf(out, ");\n");
    fprintf(out, "    if (!%s || %s->value.type != VALUE_INTEGER) { set_error(&err, %d, 1, \"integer operation needs an integer variable\"); goto qs_error; }\n", slot_name, slot_name, line);
    if (op == '+') {
        fprintf(out, "    %s->value.as.integer += %ld;\n", slot_name, rhs);
    } else if (op == '-') {
        fprintf(out, "    %s->value.as.integer -= %ld;\n", slot_name, rhs);
    } else if (op == '*') {
        fprintf(out, "    %s->value.as.integer *= %ld;\n", slot_name, rhs);
    } else if (op == '/') {
        fprintf(out, "    if (%ld == 0) { set_error(&err, %d, 1, \"division by zero\"); goto qs_error; }\n", rhs, line);
        fprintf(out, "    %s->value.as.integer /= %ld;\n", slot_name, rhs);
    } else if (op == '%') {
        fprintf(out, "    if (%ld == 0) { set_error(&err, %d, 1, \"division by zero\"); goto qs_error; }\n", rhs, line);
        fprintf(out, "    %s->value.as.integer %%= %ld;\n", slot_name, rhs);
    }
}

static int compiler_parse_simple_int_op(const char *line, int line_number, CompilerState *state, CompilerIntOp *op_out) {
    size_t assignment_pos = 0;
    char assign_op = '=';
    if (!find_assignment(line, &assignment_pos, &assign_op) || assign_op == '=') {
        return 0;
    }

    char *name = trim_copy(line, assignment_pos);
    char *rhs = trim_copy(line + assignment_pos + 2, strlen(line + assignment_pos + 2));
    long rhs_number = 0;
    int ok = compiler_is_identifier(name) &&
             compiler_get_var_kind(state, name) == COMPILER_VAR_INT &&
             compiler_parse_int_literal(rhs, &rhs_number);
    if (ok) {
        op_out->name = name;
        op_out->op = assign_op;
        op_out->rhs = rhs_number;
        op_out->line = line_number;
        free(rhs);
        return 1;
    }
    free(name);
    free(rhs);
    return 0;
}

static void compiler_free_int_ops(CompilerIntOp *ops, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(ops[i].name);
    }
    free(ops);
}

static int compiler_parse_simple_int_condition(const char *text, CompilerState *state, CompilerIntCondition *condition) {
    const char *ops[] = {"<=", ">=", "==", "!=", "<", ">"};
    char *trimmed = trim_copy(text, strlen(text));
    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
        char *pos = strstr(trimmed, ops[i]);
        if (!pos) {
            continue;
        }
        char *name = trim_copy(trimmed, (size_t)(pos - trimmed));
        char *rhs = trim_copy(pos + strlen(ops[i]), strlen(pos + strlen(ops[i])));
        long number = 0;
        int ok = compiler_is_identifier(name) &&
                 compiler_get_var_kind(state, name) == COMPILER_VAR_INT &&
                 compiler_parse_int_literal(rhs, &number);
        if (ok) {
            condition->name = name;
            snprintf(condition->compare, sizeof(condition->compare), "%s", ops[i]);
            condition->rhs = number;
            free(rhs);
            free(trimmed);
            return 1;
        }
        free(name);
        free(rhs);
    }
    free(trimmed);
    return 0;
}

static int compiler_parse_string_literal(const char *text, char **out) {
    char *trimmed = trim_copy(text, strlen(text));
    size_t len = strlen(trimmed);
    if (len < 2 || (trimmed[0] != '\'' && trimmed[0] != '"')) {
        free(trimmed);
        return 0;
    }
    char quote = trimmed[0];
    if (trimmed[len - 1] != quote) {
        free(trimmed);
        return 0;
    }
    *out = qs_strndup(trimmed + 1, len - 2);
    free(trimmed);
    return 1;
}

static int compiler_parse_simple_get_call(const char *expr, char **url_out) {
    char *trimmed = trim_copy(expr, strlen(expr));
    size_t after = 0;
    if (!line_starts_with(trimmed, "get", &after)) {
        free(trimmed);
        return 0;
    }
    size_t pos = after;
    skip_spaces_text(trimmed, &pos);
    if (trimmed[pos] != '(') {
        free(trimmed);
        return 0;
    }
    const char *close = strrchr(trimmed, ')');
    if (!close || close[1] != '\0') {
        free(trimmed);
        return 0;
    }
    char *inside = qs_strndup(trimmed + pos + 1, (size_t)(close - (trimmed + pos + 1)));
    int ok = compiler_parse_string_literal(inside, url_out);
    free(inside);
    free(trimmed);
    return ok;
}

static int compiler_parse_status_index_expr(const char *expr, char **name_out) {
    char *trimmed = trim_copy(expr, strlen(expr));
    const char *suffixes[] = {"['status']", "[\"status\"]"};
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
        size_t trimmed_len = strlen(trimmed);
        size_t suffix_len = strlen(suffixes[i]);
        if (trimmed_len <= suffix_len) {
            continue;
        }
        if (strcmp(trimmed + trimmed_len - suffix_len, suffixes[i]) != 0) {
            continue;
        }
        char *name = qs_strndup(trimmed, trimmed_len - suffix_len);
        char *clean = trim_copy(name, strlen(name));
        free(name);
        if (compiler_is_identifier(clean)) {
            *name_out = clean;
            free(trimmed);
            return 1;
        }
        free(clean);
    }
    free(trimmed);
    return 0;
}

static void compiler_emit_http_get_literal(FILE *out, const char *name, const char *url, int line_number) {
    compiler_emit_define_guard(out, name, line_number);
    fprintf(out, "    {\n");
    fprintf(out, "        CallArgList qs_http_args = {0};\n");
    fprintf(out, "        CallArg qs_http_arg = {0};\n");
    fprintf(out, "        Parser qs_http_parser = {\"\", 0, %d, &env, &err};\n", line_number);
    fprintf(out, "        qs_http_arg.value = make_string_owned(qs_strdup(");
    write_c_string(out, url);
    fprintf(out, "));\n");
    fprintf(out, "        call_arg_push(&qs_http_args, qs_http_arg);\n");
    fprintf(out, "        if (!builtin_http_get(&qs_http_parser, &qs_http_args, &qs_tmp)) { free_call_args(&qs_http_args); goto qs_error; }\n");
    fprintf(out, "        free_call_args(&qs_http_args);\n");
    fprintf(out, "        env_set(&env, ");
    write_c_string(out, name);
    fprintf(out, ", qs_tmp);\n");
    fprintf(out, "    }\n");
}

static int compiler_split_print_args(const char *inside, char ***items, size_t *count) {
    int in_string = 0;
    char quote = '\0';
    int paren = 0;
    int bracket = 0;
    size_t start = 0;
    *items = NULL;
    *count = 0;

    for (size_t i = 0;; i++) {
        char ch = inside[i];
        if (in_string) {
            if (ch == quote) {
                in_string = 0;
            }
        } else if (ch == '\'' || ch == '"') {
            in_string = 1;
            quote = ch;
        } else if (ch == '(') {
            paren++;
        } else if (ch == ')') {
            paren--;
        } else if (ch == '[') {
            bracket++;
        } else if (ch == ']') {
            bracket--;
        }

        if ((ch == ',' && !in_string && paren == 0 && bracket == 0) || ch == '\0') {
            char *part = trim_copy(inside + start, i - start);
            if (part[0]) {
                char **next = realloc(*items, (*count + 1) * sizeof(char *));
                if (!next) {
                    fprintf(stderr, "out of memory\n");
                    exit(2);
                }
                *items = next;
                (*items)[(*count)++] = part;
            } else {
                free(part);
            }
            start = i + 1;
        }
        if (ch == '\0') {
            break;
        }
    }
    return 1;
}

static void compiler_free_args(char **items, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(items[i]);
    }
    free(items);
}

static void compiler_emit_expr(FILE *out, const char *expr, int line, const char *target) {
    char *trimmed = trim_copy(expr, strlen(expr));
    char *end = NULL;
    errno = 0;
    long number = strtol(trimmed, &end, 10);
    if (trimmed[0] && end && *end == '\0' && errno != ERANGE) {
        fprintf(out, "    %s = make_integer(%ld);\n", target, number);
    } else if (strcmp(trimmed, "true") == 0) {
        fprintf(out, "    %s = make_bool(1);\n", target);
    } else if (strcmp(trimmed, "false") == 0) {
        fprintf(out, "    %s = make_bool(0);\n", target);
    } else if (strcmp(trimmed, "null") == 0) {
        fprintf(out, "    %s = make_null();\n", target);
    } else {
        fprintf(out, "    %s = make_null();\n", target);
        fprintf(out, "    if (!eval_expression_text(&env, ");
        write_c_string(out, expr);
        fprintf(out, ", strlen(");
        write_c_string(out, expr);
        fprintf(out, "), %d, &%s, &err)) goto qs_error;\n", line, target);
    }
    free(trimmed);
}

static int compiler_analyze(Program *program, int **skip_out) {
    int *skip = calloc(program->count ? program->count : 1, sizeof(int));
    if (!skip) {
        fprintf(stderr, "out of memory\n");
        exit(2);
    }

    for (size_t i = 0; i < program->count; i++) {
        char *code = trim_copy(program->items[i], strlen(program->items[i]));
        if (!code[0] || code[0] == '#') {
            free(code);
            continue;
        }

        size_t after = 0;
        if (line_starts_with(code, "func", &after)) {
            size_t close_line = 0;
            ParseError err = {0};
            if (!find_block_end(program, i, program->count, &close_line, &err)) {
                free(code);
                free(skip);
                return 0;
            }
            for (size_t s = i; s <= close_line; s++) {
                skip[s] = 1;
            }
            i = close_line;
            free(code);
            continue;
        }

        free(code);
    }

    *skip_out = skip;
    return 1;
}

static void compiler_emit_line_table(FILE *out, Program *program) {
    fprintf(out, "static const char *QUANTO_COMPILED_LINES[] = {\n");
    for (size_t i = 0; i < program->count; i++) {
        fprintf(out, "    ");
        write_c_string(out, program->items[i]);
        if (i + 1 < program->count) {
            fprintf(out, ",");
        }
        fprintf(out, "\n");
    }
    fprintf(out, "};\n\n");
}

static int compiler_find_if_chain_end(Program *program, size_t start, size_t *chain_end) {
    ParseError block_err = {0};
    size_t close_line = 0;
    if (!find_block_end(program, start, program->count, &close_line, &block_err)) {
        return 0;
    }

    while (1) {
        size_t after = 0;
        size_t brace = 0;

        if ((line_is_close_word(program->items[close_line], "maybe", &after) ||
             line_is_close_else(program->items[close_line], &after)) &&
            line_has_open_brace(program->items[close_line], after, &brace)) {
            if (!find_block_end(program, close_line, program->count, &close_line, &block_err)) {
                return 0;
            }
            continue;
        }

        if (close_line + 1 < program->count &&
            (line_starts_with(program->items[close_line + 1], "maybe", &after) ||
             line_starts_with(program->items[close_line + 1], "else", &after)) &&
            line_has_open_brace(program->items[close_line + 1], after, &brace)) {
            if (!find_block_end(program, close_line + 1, program->count, &close_line, &block_err)) {
                return 0;
            }
            continue;
        }

        break;
    }

    *chain_end = close_line;
    return 1;
}

static int compiler_find_try_chain_end(Program *program, size_t start, size_t *chain_end) {
    ParseError block_err = {0};
    size_t try_close = 0;
    if (!find_block_end(program, start, program->count, &try_close, &block_err)) {
        return 0;
    }
    if (try_close + 1 >= program->count) {
        size_t after_close_oops = 0;
        if (line_is_close_word(program->items[try_close], "oops", &after_close_oops)) {
            return find_block_end(program, try_close, program->count, chain_end, &block_err);
        }
        return 0;
    }
    size_t after = 0;
    if (line_is_close_word(program->items[try_close], "oops", &after)) {
        return find_block_end(program, try_close, program->count, chain_end, &block_err);
    }
    if (!line_starts_with(program->items[try_close + 1], "oops", &after)) {
        return 0;
    }
    return find_block_end(program, try_close + 1, program->count, chain_end, &block_err);
}

static int compiler_try_emit_fast_repeat(FILE *out, Program *program, size_t start, size_t close_line, CompilerState *state) {
    const char *line = program->items[start];
    size_t after = 0;
    size_t brace = 0;
    if (!line_starts_with(line, "repeat", &after) || !line_has_open_brace(line, after, &brace)) {
        return 0;
    }
    char *count_expr = trim_copy(line + after, brace - after);
    char *end = NULL;
    errno = 0;
    long repeat_count = strtol(count_expr, &end, 10);
    if (!count_expr[0] || !end || *end != '\0' || errno == ERANGE || repeat_count < 0) {
        free(count_expr);
        return 0;
    }
    free(count_expr);

    CompilerIntOp *ops = NULL;
    size_t op_count = 0;
    size_t op_capacity = 0;
    for (size_t i = start + 1; i < close_line; i++) {
        if (line_is_empty_or_comment(program->items[i])) {
            continue;
        }
        CompilerIntOp op = {0};
        if (!compiler_parse_simple_int_op(program->items[i], (int)i + 1, state, &op)) {
            compiler_free_int_ops(ops, op_count);
            return 0;
        }
        if (op_count == op_capacity) {
            size_t next_capacity = op_capacity ? op_capacity * 2 : 4;
            CompilerIntOp *next = realloc(ops, next_capacity * sizeof(CompilerIntOp));
            if (!next) {
                fprintf(stderr, "out of memory\n");
                exit(2);
            }
            ops = next;
            op_capacity = next_capacity;
        }
        ops[op_count++] = op;
    }
    if (op_count == 0) {
        free(ops);
        return 0;
    }

    fprintf(out, "    {\n");
    for (size_t i = 0; i < op_count; i++) {
        size_t slot_index = i;
        for (size_t j = 0; j < i; j++) {
            if (strcmp(ops[j].name, ops[i].name) == 0) {
                slot_index = j;
                break;
            }
        }
        if (slot_index != i) {
            continue;
        }
        fprintf(out, "        Variable *qs_fast_slot_%lu = env_get_slot(&env, ", (unsigned long)i);
        write_c_string(out, ops[i].name);
        fprintf(out, ");\n");
        fprintf(out, "        if (!qs_fast_slot_%lu || qs_fast_slot_%lu->value.type != VALUE_INTEGER) { set_error(&err, %d, 1, \"fast repeat needs integer variables\"); goto qs_error; }\n", (unsigned long)i, (unsigned long)i, ops[i].line);
    }
    fprintf(out, "        for (long qs_n = 0; qs_n < %ld; qs_n++) {\n", repeat_count);
    for (size_t i = 0; i < op_count; i++) {
        size_t slot_index = i;
        for (size_t j = 0; j < i; j++) {
            if (strcmp(ops[j].name, ops[i].name) == 0) {
                slot_index = j;
                break;
            }
        }
        if (ops[i].op == '+') {
            fprintf(out, "            qs_fast_slot_%lu->value.as.integer += %ld;\n", (unsigned long)slot_index, ops[i].rhs);
        } else if (ops[i].op == '-') {
            fprintf(out, "            qs_fast_slot_%lu->value.as.integer -= %ld;\n", (unsigned long)slot_index, ops[i].rhs);
        } else if (ops[i].op == '*') {
            fprintf(out, "            qs_fast_slot_%lu->value.as.integer *= %ld;\n", (unsigned long)slot_index, ops[i].rhs);
        } else if (ops[i].op == '/') {
            if (ops[i].rhs == 0) {
                compiler_free_int_ops(ops, op_count);
                return 0;
            }
            fprintf(out, "            qs_fast_slot_%lu->value.as.integer /= %ld;\n", (unsigned long)slot_index, ops[i].rhs);
        } else if (ops[i].op == '%') {
            if (ops[i].rhs == 0) {
                compiler_free_int_ops(ops, op_count);
                return 0;
            }
            fprintf(out, "            qs_fast_slot_%lu->value.as.integer %%= %ld;\n", (unsigned long)slot_index, ops[i].rhs);
        }
    }
    fprintf(out, "        }\n");
    fprintf(out, "    }\n");

    compiler_free_int_ops(ops, op_count);
    return 1;
}

static int compiler_try_emit_fast_while(FILE *out, Program *program, size_t start, size_t close_line, CompilerState *state) {
    const char *line = program->items[start];
    size_t after = 0;
    size_t brace = 0;
    if (!line_starts_with(line, "while", &after) || !line_has_open_brace(line, after, &brace)) {
        return 0;
    }

    char *condition_text = trim_copy(line + after, brace - after);
    CompilerIntCondition condition = {0};
    if (!compiler_parse_simple_int_condition(condition_text, state, &condition)) {
        free(condition_text);
        return 0;
    }
    free(condition_text);

    CompilerIntOp *ops = NULL;
    size_t op_count = 0;
    size_t op_capacity = 0;
    int condition_changes = 0;

    for (size_t i = start + 1; i < close_line; i++) {
        if (line_is_empty_or_comment(program->items[i])) {
            continue;
        }
        CompilerIntOp op = {0};
        if (!compiler_parse_simple_int_op(program->items[i], (int)i + 1, state, &op)) {
            free(condition.name);
            compiler_free_int_ops(ops, op_count);
            return 0;
        }
        if (strcmp(op.name, condition.name) == 0 && op.rhs != 0) {
            condition_changes = 1;
        }
        if (op_count == op_capacity) {
            size_t next_capacity = op_capacity ? op_capacity * 2 : 4;
            CompilerIntOp *next = realloc(ops, next_capacity * sizeof(CompilerIntOp));
            if (!next) {
                fprintf(stderr, "out of memory\n");
                exit(2);
            }
            ops = next;
            op_capacity = next_capacity;
        }
        ops[op_count++] = op;
    }

    if (op_count == 0 || !condition_changes) {
        free(condition.name);
        compiler_free_int_ops(ops, op_count);
        return 0;
    }

    fprintf(out, "    {\n");
    fprintf(out, "        Variable *qs_while_cond = env_get_slot(&env, ");
    write_c_string(out, condition.name);
    fprintf(out, ");\n");
    fprintf(out, "        if (!qs_while_cond || qs_while_cond->value.type != VALUE_INTEGER) { set_error(&err, %d, 1, \"fast while needs an integer condition variable\"); goto qs_error; }\n", (int)start + 1);

    for (size_t i = 0; i < op_count; i++) {
        size_t slot_index = i;
        for (size_t j = 0; j < i; j++) {
            if (strcmp(ops[j].name, ops[i].name) == 0) {
                slot_index = j;
                break;
            }
        }
        if (slot_index != i || strcmp(ops[i].name, condition.name) == 0) {
            continue;
        }
        fprintf(out, "        Variable *qs_while_slot_%lu = env_get_slot(&env, ", (unsigned long)i);
        write_c_string(out, ops[i].name);
        fprintf(out, ");\n");
        fprintf(out, "        if (!qs_while_slot_%lu || qs_while_slot_%lu->value.type != VALUE_INTEGER) { set_error(&err, %d, 1, \"fast while needs integer variables\"); goto qs_error; }\n", (unsigned long)i, (unsigned long)i, ops[i].line);
    }

    fprintf(out, "        while (qs_while_cond->value.as.integer %s %ld) {\n", condition.compare, condition.rhs);
    for (size_t i = 0; i < op_count; i++) {
        const char *slot_expr = NULL;
        char slot_buffer[64];
        if (strcmp(ops[i].name, condition.name) == 0) {
            slot_expr = "qs_while_cond";
        } else {
            size_t slot_index = i;
            for (size_t j = 0; j < i; j++) {
                if (strcmp(ops[j].name, ops[i].name) == 0) {
                    slot_index = j;
                    break;
                }
            }
            snprintf(slot_buffer, sizeof(slot_buffer), "qs_while_slot_%lu", (unsigned long)slot_index);
            slot_expr = slot_buffer;
        }

        if (ops[i].op == '+') {
            fprintf(out, "            %s->value.as.integer += %ld;\n", slot_expr, ops[i].rhs);
        } else if (ops[i].op == '-') {
            fprintf(out, "            %s->value.as.integer -= %ld;\n", slot_expr, ops[i].rhs);
        } else if (ops[i].op == '*') {
            fprintf(out, "            %s->value.as.integer *= %ld;\n", slot_expr, ops[i].rhs);
        } else if (ops[i].op == '/') {
            if (ops[i].rhs == 0) {
                free(condition.name);
                compiler_free_int_ops(ops, op_count);
                return 0;
            }
            fprintf(out, "            %s->value.as.integer /= %ld;\n", slot_expr, ops[i].rhs);
        } else if (ops[i].op == '%') {
            if (ops[i].rhs == 0) {
                free(condition.name);
                compiler_free_int_ops(ops, op_count);
                return 0;
            }
            fprintf(out, "            %s->value.as.integer %%= %ld;\n", slot_expr, ops[i].rhs);
        }
    }
    fprintf(out, "        }\n");
    fprintf(out, "    }\n");

    free(condition.name);
    compiler_free_int_ops(ops, op_count);
    return 1;
}

static int compiler_emit_direct_print(FILE *out, char **args, size_t count, CompilerState *state, int line_number) {
    int has_fast_arg = 0;
    for (size_t i = 0; i < count; i++) {
        long number = 0;
        char *status_name = NULL;
        if (compiler_parse_int_literal(args[i], &number) ||
            (compiler_is_identifier(args[i]) && compiler_get_var_kind(state, args[i]) == COMPILER_VAR_INT) ||
            compiler_parse_status_index_expr(args[i], &status_name)) {
            has_fast_arg = 1;
        }
        free(status_name);
    }
    if (!has_fast_arg) {
        return 0;
    }

    for (size_t i = 0; i < count; i++) {
        long number = 0;
        if (i) {
            fprintf(out, "    printf(\" \");\n");
        }
        if (compiler_parse_int_literal(args[i], &number)) {
            fprintf(out, "    printf(\"%%ld\", %ldL);\n", number);
        } else if (compiler_is_identifier(args[i]) && compiler_get_var_kind(state, args[i]) == COMPILER_VAR_INT) {
            fprintf(out, "    qs_slot = env_get_slot(&env, ");
            write_c_string(out, args[i]);
            fprintf(out, ");\n");
            fprintf(out, "    if (!qs_slot || qs_slot->value.type != VALUE_INTEGER) { set_error(&err, %d, 1, \"print expected an integer variable\"); goto qs_error; }\n", line_number);
            fprintf(out, "    printf(\"%%ld\", qs_slot->value.as.integer);\n");
        } else {
            char *status_name = NULL;
            if (compiler_parse_status_index_expr(args[i], &status_name)) {
                fprintf(out, "    qs_slot = env_get_slot(&env, ");
                write_c_string(out, status_name);
                fprintf(out, ");\n");
                fprintf(out, "    if (!qs_slot || qs_slot->value.type != VALUE_DICT) { set_error(&err, %d, 1, \"status needs a map response\"); goto qs_error; }\n", line_number);
                fprintf(out, "    { int qs_status_index = dict_find(&qs_slot->value.as.dict, \"status\"); if (qs_status_index < 0 || qs_slot->value.as.dict.items[qs_status_index].value.type != VALUE_INTEGER) { set_error(&err, %d, 1, \"response has no integer status\"); goto qs_error; } printf(\"%%ld\", qs_slot->value.as.dict.items[qs_status_index].value.as.integer); }\n", line_number);
                free(status_name);
            } else {
                compiler_emit_expr(out, args[i], line_number, "qs_tmp");
                fprintf(out, "    print_value(qs_tmp);\n");
                fprintf(out, "    free_value(qs_tmp);\n");
            }
        }
    }
    if (count) {
        fprintf(out, "    printf(\"\\n\");\n");
    }
    return 1;
}

static int compiler_emit_direct_assignment(FILE *out, const char *line, int line_number, CompilerState *state) {
    size_t assignment_pos = 0;
    char assign_op = '=';
    if (!find_assignment(line, &assignment_pos, &assign_op)) {
        return 0;
    }

    char *name = trim_copy(line, assignment_pos);
    if (!compiler_is_identifier(name)) {
        fprintf(out, "    if (!execute_assignment(&env, ");
        write_c_string(out, line);
        fprintf(out, ", (size_t)%ld, '%c', %d, &err)) goto qs_error;\n", (long)assignment_pos, assign_op, line_number);
        free(name);
        return 1;
    }

    if (assign_op == '=') {
        long number = 0;
        const char *expr = line + assignment_pos + 1;
        char *url = NULL;
        if (compiler_parse_simple_get_call(expr, &url)) {
            compiler_emit_http_get_literal(out, name, url, line_number);
            compiler_set_var_kind(state, name, COMPILER_VAR_UNKNOWN);
            free(url);
            free(name);
            return 1;
        }
        if (compiler_parse_int_literal(expr, &number)) {
            compiler_emit_define_guard(out, name, line_number);
            compiler_emit_slot_set_int(out, name, number, line_number);
            compiler_set_var_kind(state, name, COMPILER_VAR_INT);
            free(name);
            return 1;
        }

        compiler_emit_define_guard(out, name, line_number);
        compiler_emit_expr(out, expr, line_number, "qs_tmp");
        fprintf(out, "    env_set(&env, ");
        write_c_string(out, name);
        fprintf(out, ", qs_tmp);\n");
        compiler_set_var_kind(state, name, COMPILER_VAR_UNKNOWN);
        free(name);
        return 1;
    }

    CompilerIntOp op = {0};
    if (compiler_parse_simple_int_op(line, line_number, state, &op)) {
        compiler_emit_define_guard(out, op.name, line_number);
        compiler_emit_slot_int_op(out, op.name, op.op, op.rhs, op.line, "qs_slot");
        compiler_set_var_kind(state, op.name, COMPILER_VAR_INT);
        free(op.name);
        free(name);
        return 1;
    }

    fprintf(out, "    if (!execute_assignment(&env, ");
    write_c_string(out, line);
    fprintf(out, ", (size_t)%ld, '%c', %d, &err)) goto qs_error;\n", (long)assignment_pos, assign_op, line_number);
    compiler_set_var_kind(state, name, COMPILER_VAR_UNKNOWN);
    free(name);
    return 1;
}

static int compiler_emit_direct_statement(FILE *out, Program *program, size_t *index, CompilerState *state) {
    const char *line = program->items[*index];
    int line_number = (int)*index + 1;
    char *code = trim_copy(line, strlen(line));
    size_t after = 0;
    if (!code[0] || code[0] == '#' || line_starts_with(code, "from", &after)) {
        free(code);
        return 1;
    }

    if (line_starts_with(code, "repeat", &after)) {
        size_t close_line = 0;
        ParseError block_err = {0};
        if (!find_block_end(program, *index, program->count, &close_line, &block_err)) {
            free(code);
            return 0;
        }
        if (!compiler_try_emit_fast_repeat(out, program, *index, close_line, state)) {
            fprintf(out, "    qs_index = (size_t)%ld;\n", (long)*index);
            fprintf(out, "    if (!execute_loop(&program, &env, &qs_index, program.count, \"repeat\", &err)) goto qs_error;\n");
            compiler_forget_all_vars(state);
        }
        *index = close_line;
        free(code);
        return 1;
    }

    if (line_starts_with(code, "while", &after)) {
        size_t close_line = 0;
        ParseError block_err = {0};
        if (!find_block_end(program, *index, program->count, &close_line, &block_err)) {
            free(code);
            return 0;
        }
        if (!compiler_try_emit_fast_while(out, program, *index, close_line, state)) {
            fprintf(out, "    qs_index = (size_t)%ld;\n", (long)*index);
            fprintf(out, "    if (!execute_loop(&program, &env, &qs_index, program.count, \"while\", &err)) goto qs_error;\n");
            compiler_forget_all_vars(state);
        }
        *index = close_line;
        free(code);
        return 1;
    }

    if (line_starts_with(code, "if", &after)) {
        size_t chain_end = 0;
        if (!compiler_find_if_chain_end(program, *index, &chain_end)) {
            free(code);
            return 0;
        }
        fprintf(out, "    qs_index = (size_t)%ld;\n", (long)*index);
        fprintf(out, "    if (!execute_if(&program, &env, &qs_index, program.count, &err)) goto qs_error;\n");
        compiler_forget_all_vars(state);
        *index = chain_end;
        free(code);
        return 1;
    }

    if (line_starts_with(code, "try", &after)) {
        size_t chain_end = 0;
        if (!compiler_find_try_chain_end(program, *index, &chain_end)) {
            free(code);
            return 0;
        }
        fprintf(out, "    qs_index = (size_t)%ld;\n", (long)*index);
        fprintf(out, "    if (!execute_try(&program, &env, &qs_index, program.count, &err)) goto qs_error;\n");
        compiler_forget_all_vars(state);
        *index = chain_end;
        free(code);
        return 1;
    }

    if (line_starts_with(code, "maybe", &after) ||
        line_starts_with(code, "else", &after) ||
        line_starts_with(code, "oops", &after) ||
        line_starts_with(code, "return", &after)) {
        free(code);
        return 0;
    }

    size_t after_print = 0;
    if (line_starts_with(code, "print", &after_print)) {
        const char *open = strchr(line, '(');
        const char *close = strrchr(line, ')');
        if (!open || !close || close < open) {
            free(code);
            return 0;
        }
        char *inside = qs_strndup(open + 1, (size_t)(close - open - 1));
        char **args = NULL;
        size_t count = 0;
        compiler_split_print_args(inside, &args, &count);
        if (count == 0) {
            fprintf(out, "    printf(\"\\n\");\n");
        } else if (!compiler_emit_direct_print(out, args, count, state, line_number)) {
            for (size_t i = 0; i < count; i++) {
                compiler_emit_expr(out, args[i], line_number, "qs_tmp");
                if (i) {
                    fprintf(out, "    printf(\" \");\n");
                }
                fprintf(out, "    print_value(qs_tmp);\n");
                fprintf(out, "    free_value(qs_tmp);\n");
            }
            fprintf(out, "    printf(\"\\n\");\n");
        }
        compiler_free_args(args, count);
        free(inside);
        free(code);
        return 1;
    }

    size_t assignment_pos = 0;
    char assign_op = '=';
    if (find_assignment(line, &assignment_pos, &assign_op)) {
        (void)assignment_pos;
        (void)assign_op;
        compiler_emit_direct_assignment(out, line, line_number, state);
        free(code);
        return 1;
    }

    if (is_name_start(code[0]) && strchr(code, '(')) {
        compiler_emit_expr(out, line, line_number, "qs_tmp");
        fprintf(out, "    free_value(qs_tmp);\n");
        free(code);
        return 1;
    }

    free(code);
    return 0;
}

static int compiler_write_c(const char *generated_path, Program *program, const char *input_path, int *skip) {
    FILE *out = fopen(generated_path, "wb");
    if (!out) {
        fprintf(stderr, "QuantoScript: cannot write '%s'\n", generated_path);
        return 0;
    }
    CompilerState state = {0};

    fprintf(out, "#define QUANTO_NO_MAIN\n");
    fprintf(out, "#include \"quanto.c\"\n\n");
    fprintf(out, "static const char *QUANTO_COMPILED_PATH = ");
    write_c_string(out, input_path);
    fprintf(out, ";\n");
    compiler_emit_line_table(out, program);

    fprintf(out, "int main(void) {\n");
    fprintf(out, "    Program program = {0};\n");
    fprintf(out, "    Environment env = {0};\n");
    fprintf(out, "    ParseError err = {0};\n");
    fprintf(out, "    Value qs_tmp = make_null();\n");
    fprintf(out, "    Variable *qs_slot = NULL;\n");
    fprintf(out, "    size_t qs_index = 0;\n");
    fprintf(out, "    (void)qs_tmp;\n");
    fprintf(out, "    (void)qs_slot;\n");
    fprintf(out, "    (void)qs_index;\n");
    fprintf(out, "    program.path = qs_strdup(QUANTO_COMPILED_PATH);\n");
    fprintf(out, "    program.dir = path_dirname(program.path);\n");
    fprintf(out, "    for (size_t i = 0; i < sizeof(QUANTO_COMPILED_LINES) / sizeof(QUANTO_COMPILED_LINES[0]); i++) {\n");
    fprintf(out, "        program_push(&program, qs_strdup(QUANTO_COMPILED_LINES[i]));\n");
    fprintf(out, "    }\n");
    fprintf(out, "    g_program = &program;\n");
    fprintf(out, "    strip_block_comments(&program);\n");
    fprintf(out, "    if (!collect_imports(&program, &env, &err)) goto qs_error;\n");
    fprintf(out, "    if (!collect_functions(&program, &err)) goto qs_error;\n");
    for (size_t i = 0; i < program->count; i++) {
        if (skip[i]) {
            continue;
        }
        if (!compiler_emit_direct_statement(out, program, &i, &state)) {
            compiler_free_state(&state);
            fclose(out);
            return 0;
        }
    }
    fprintf(out, "    free_environment(&env);\n");
    fprintf(out, "    free_program(&program);\n");
    fprintf(out, "    free_functions();\n");
    fprintf(out, "    free_modules();\n");
    fprintf(out, "    free_call_stack();\n");
    fprintf(out, "    return 0;\n\n");
    fprintf(out, "qs_error:\n");
    fprintf(out, "    fprintf(stderr, \"SyntaxError: %%s\\nline %%d, column %%d\\n\", err.message, err.line, err.column);\n");
    fprintf(out, "    if (g_call_stack_count) { fprintf(stderr, \"stack trace:\\n\"); for (size_t i = g_call_stack_count; i > 0; i--) fprintf(stderr, \"  at %%s\\n\", g_call_stack[i - 1]); }\n");
    fprintf(out, "    free_environment(&env);\n");
    fprintf(out, "    free_program(&program);\n");
    fprintf(out, "    free_functions();\n");
    fprintf(out, "    free_modules();\n");
    fprintf(out, "    free_call_stack();\n");
    fprintf(out, "    return 1;\n");
    fprintf(out, "}\n");
    fclose(out);
    compiler_free_state(&state);
    return 1;
}

// Find the directory containing quanto.c at runtime.
// Checks: QS_SOURCE_DIR define, executable-relative paths, cwd-relative paths.
static void find_runtime_dir(char *buf, size_t bufsz) {
#ifdef QS_SOURCE_DIR
    snprintf(buf, bufsz, "%s", QS_SOURCE_DIR);
    return;
#endif
    // Try common relative locations from cwd
    const char *candidates[] = {
        "src",
        "../src",
        "../../src",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        char probe[2048];
        snprintf(probe, sizeof(probe), "%s/quanto.c", candidates[i]);
        FILE *f = fopen(probe, "r");
        if (f) {
            fclose(f);
            snprintf(buf, bufsz, "%s", candidates[i]);
            return;
        }
    }
    // Fallback
    snprintf(buf, bufsz, "src");
}

static QS_UNUSED_FN int compile_file(const char *input_path, const char *output_path) {
    Program program = {0};
    if (!load_program_file(input_path, &program)) {
        return 1;
    }
    compiler_mkdir_build();

    int *skip = NULL;
    if (!compiler_analyze(&program, &skip)) {
        free_program(&program);
        return 1;
    }

    char generated_path[1024];
    snprintf(generated_path, sizeof(generated_path), "%s.c", output_path);
    if (!compiler_write_c(generated_path, &program, input_path, skip)) {
        free(skip);
        free_program(&program);
        return 1;
    }

    char runtime_dir[2048];
    find_runtime_dir(runtime_dir, sizeof(runtime_dir));

    char command[4096];
#ifdef _WIN32
    snprintf(command, sizeof(command),
             "gcc -std=c99 -O3 -DNDEBUG -Wall -Wextra -pedantic -I\"%s\" \"%s\" -o \"%s\" -lssl -lcrypto -lws2_32 -lwinhttp -lwininet",
             runtime_dir, generated_path, output_path);
#else
    snprintf(command, sizeof(command),
             "gcc -std=c99 -O3 -DNDEBUG -Wall -Wextra -pedantic -I\"%s\" \"%s\" -o \"%s\" -lssl -lcrypto",
             runtime_dir, generated_path, output_path);
#endif
    int status = system(command);
    if (status != 0) {
        fprintf(stderr, "QuantoScript: native C build failed\n");
        fprintf(stderr, "  runtime dir: %s\n", runtime_dir);
        free(skip);
        free_program(&program);
        return 1;
    }

    printf("compiled %s -> %s (compiled-C)\n", input_path, output_path);
    free(skip);
    free_program(&program);
    return 0;
}
