static int line_is_free_text_comment(const char *line) {
    size_t p = 0;
    while (line[p] == ' ' || line[p] == '\t') p++;
    if (!is_name_start(line[p])) return 0;
    while (is_name_part(line[p])) p++;
    if (line[p] != ' ' && line[p] != '\t') return 0;
    while (line[p] == ' ' || line[p] == '\t') p++;
    return is_name_start(line[p]);
}

static int execute_assignment(Environment *env, const char *line_text, size_t assignment_pos, char assign_op, int line, ParseError *err) {
    char *name = trim_copy(line_text, assignment_pos);
    if (!validate_variable_name(name, line, err)) {
        char *dot = strchr(name, '.');
        if (dot && assign_op == '=') {
            size_t obj_len = dot - name;
            char *obj_name = qs_strndup(name, obj_len);
            char *field_name = dot + 1;

            Environment *obj_env = env;
            int obj_idx = find_variable_scoped(env, obj_name, &obj_env);
            if (obj_idx < 0) {
                char msg[256];
                snprintf(msg, sizeof(msg), "unknown variable '%s'", obj_name);
                set_error(err, line, 1, msg);
                free(obj_name);
                free(name);
                return 0;
            }

            Value obj = obj_env->items[obj_idx].value;
            if (obj.type != VALUE_OBJECT) {
                char msg[256];
                snprintf(msg, sizeof(msg), "'%s' is not an object", obj_name);
                set_error(err, line, 1, msg);
                free(obj_name);
                free(name);
                return 0;
            }

            Value value;
            size_t rhs_offset = assign_op == '=' ? 1 : 2;
            if (!eval_expression_text(env, line_text + assignment_pos + rhs_offset, strlen(line_text + assignment_pos + rhs_offset), line, &value, err)) {
                free(obj_name);
                free(name);
                return 0;
            }

            dict_set(&obj.as.object.data->fields, field_name, value);

            free(obj_name);
            free(name);
            return 1;
        }
        char *br = strchr(name, '[');
        if (br && assign_op == '=') {
            size_t obj_len = br - name;
            char *obj_name = qs_strndup(name, obj_len);
            char *end = strchr(br, ']');
            if (!end) { free(obj_name); free(name); return 0; }
            *end = '\0';
            char *key_expr = br + 1;
            Value key_val;
            if (!eval_expression_text(env, key_expr, strlen(key_expr), line, &key_val, err)) {
                free(obj_name); free(name); return 0;
            }
            Environment *dict_env = env;
            int obj_idx = find_variable_scoped(env, obj_name, &dict_env);
            if (obj_idx < 0) {
                char msg[256]; snprintf(msg, sizeof(msg), "unknown variable '%s'", obj_name);
                set_error(err, line, 1, msg); free_value(key_val); free(obj_name); free(name); return 0;
            }
            Value value;
            if (!eval_expression_text(env, line_text + assignment_pos + 1, strlen(line_text + assignment_pos + 1), line, &value, err)) {
                free_value(key_val); free(obj_name); free(name); return 0;
            }
            if (dict_env->items[obj_idx].value.type == VALUE_DICT) {
                Value new_val = clone_value(dict_env->items[obj_idx].value);
                char *k = value_to_string(key_val);
                dict_set(&new_val.as.dict, k, clone_value(value));
                free(k); free_value(dict_env->items[obj_idx].value); dict_env->items[obj_idx].value = new_val;
                free_value(key_val); free_value(value); free(obj_name); free(name); return 1;
            } else {
                char msg[256]; snprintf(msg, sizeof(msg), "'%s' is not a map", obj_name);
                free_value(key_val); free_value(value); free(obj_name); free(name);
                set_error(err, line, 1, msg); return 0;
            }
        }
        free(name);
        return 0;
    }

    if (is_define_name(name) && find_variable(env, name) >= 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "definition '%s' already exists and cannot be changed", name);
        set_error(err, line, 1, msg);
        free(name);
        return 0;
    }

    Value value;
    size_t rhs_offset = assign_op == '=' ? 1 : 2;
    if (!eval_expression_text(env, line_text + assignment_pos + rhs_offset, strlen(line_text + assignment_pos + rhs_offset), line, &value, err)) {
        free(name);
        return 0;
    }

    if (assign_op != '=') {
        int index = find_variable(env, name);
        if (index < 0 && env != g_root_env && g_root_env &&
            g_current_function && !g_current_function->is_module) {
            index = find_variable(g_root_env, name);
        }
        if (index < 0) {
            char msg[256];
            snprintf(msg, sizeof(msg), "unknown variable '%s'", name);
            set_error(err, line, 1, msg);
            free_value(value);
            free(name);
            return 0;
        }

        char *expression = malloc(strlen(name) + strlen(line_text + assignment_pos + 2) + 4);
        if (!expression) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        sprintf(expression, "%s %c %s", name, assign_op, line_text + assignment_pos + 2);
        free_value(value);
        if (!eval_expression_text(env, expression, strlen(expression), line, &value, err)) {
            free(expression);
            free(name);
            return 0;
        }
        free(expression);
    }

    int propagate_global = 0;
    if (env != g_root_env && g_root_env &&
        g_current_function && !g_current_function->is_module) {
        int is_param = 0;
        for (size_t p = 0; p < g_current_function->param_count; p++) {
            if (g_current_function->params[p].name &&
                strcmp(g_current_function->params[p].name, name) == 0) {
                is_param = 1;
                break;
            }
        }
        if (!is_param) {
            propagate_global = 1;
        }
    }

    if (propagate_global) {
        env_set(g_root_env, name, value);
    } else {
        env_set(env, name, value);
    }
    free(name);
    return 1;
}

static int execute_method_statement(Environment *env, const char *line_text, int line, ParseError *err) {
    Parser parser = {line_text, 0, line, env, err, -1};
    char *name;
    if (!parse_name(&parser, &name)) {
        return 0;
    }
    if (!match_char(&parser, '.')) {
        free(name);
        return 0;
    }

    Environment *method_env = env;
    int index = find_variable(env, name);
    if (index < 0 && env != g_root_env && g_root_env &&
        g_current_function && !g_current_function->is_module) {
        index = find_variable(g_root_env, name);
        if (index >= 0) method_env = g_root_env;
    }
    if (index < 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "unknown variable '%s'", name);
        parser_error(&parser, msg);
        free(name);
        return 0;
    }

    if (is_define_name(name) && find_variable(method_env, name) >= 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "definition '%s' already exists and cannot be changed", name);
        set_error(err, line, 1, msg);
        free(name);
        return 0;
    }

    char *method;
    if (!parse_name(&parser, &method)) {
        free(name);
        return 0;
    }

    ValueList args = {0};
    if (!parse_call_args(&parser, &args)) {
        free(name);
        free(method);
        return 0;
    }
    if (!rest_is_space_or_comment(parser.text, parser.pos)) {
        parser_error(&parser, "unexpected text after method call");
        for (size_t i = 0; i < args.count; i++) {
            free_value(args.items[i]);
        }
        free(args.items);
        free(name);
        free(method);
        return 0;
    }

    Value value = clone_value(method_env->items[index].value);
    int ok = apply_method(&parser, &value, method, &args);
    for (size_t i = 0; i < args.count; i++) {
        free_value(args.items[i]);
    }
    free(args.items);

    int is_mutation = (strcmp(method, "extend") == 0 || strcmp(method, "append") == 0 ||
         strcmp(method, "sort") == 0 || strcmp(method, "remove") == 0 ||
         strcmp(method, "insert") == 0 || strcmp(method, "pop") == 0 ||
         strcmp(method, "clear") == 0 || strcmp(method, "shuffle") == 0 ||
         strcmp(method, "reverse") == 0 || strcmp(method, "fill") == 0 ||
         strcmp(method, "splice") == 0 || strcmp(method, "push") == 0);
    free(method);

    if (!ok) {
        free_value(value);
        free(name);
        return 0;
    }

    if (is_mutation) {
        free_value(method_env->items[index].value);
        method_env->items[index].value = value;
    } else {
        free_value(value);
    }
    free(name);
    return 1;
}

static void print_value(Value value);
static void print_list_value(Value value);

static int loop_control_should_stop(void) {
    return g_has_return || g_has_break || g_has_continue;
}

static int execute_print(Environment *env, const char *line_text, int line, ParseError *err) {
    size_t pos = 0;
    skip_spaces_text(line_text, &pos);
    pos += strlen("print");
    skip_spaces_text(line_text, &pos);

    if (line_text[pos] != '(') {
        set_error(err, line, (int)pos + 1, "print needs '('");
        return 0;
    }
    pos++;

    Parser parser = {line_text, pos, line, env, err, -1};
    ValueList args = {0};
    skip_spaces(&parser);
    if (match_char(&parser, ')')) {
        printf("\n");
        return rest_is_space_or_comment(parser.text, parser.pos) ? 1 : (parser_error(&parser, "unexpected text after print"), 0);
    }

    while (1) {
        Value value;
        if (!parse_expression(&parser, &value)) {
            for (size_t i = 0; i < args.count; i++) {
                free_value(args.items[i]);
            }
            free(args.items);
            return 0;
        }
        list_push(&args, value);

        skip_spaces(&parser);
        if (match_char(&parser, ',')) {
            continue;
        }
        if (match_char(&parser, ')')) {
            break;
        }
        for (size_t i = 0; i < args.count; i++) {
            free_value(args.items[i]);
        }
        free(args.items);
        parser_error(&parser, "print needs ',' or ')'");
        return 0;
    }

    if (!rest_is_space_or_comment(parser.text, parser.pos)) {
        for (size_t i = 0; i < args.count; i++) {
            free_value(args.items[i]);
        }
        free(args.items);
        parser_error(&parser, "unexpected text after print");
        return 0;
    }

    for (size_t i = 0; i < args.count; i++) {
        if (i) {
            printf(" ");
        }
        print_value(args.items[i]);
        free_value(args.items[i]);
    }
    free(args.items);
    printf("\n");
    return 1;
}

static int execute_loop(Program *program, Environment *env, size_t *index, size_t end, const char *word, ParseError *err) {
    const char *line_text = program->items[*index];
    size_t after_word = 0;
    size_t brace_pos = 0;
    line_starts_with(line_text, word, &after_word);

    if (!line_has_open_brace(line_text, after_word, &brace_pos)) {
        set_error(err, (int)*index + 1, 1, "loops need '{' on the same line");
        return 0;
    }

    int single_line_body = 0;
    if (!rest_is_space_or_comment(line_text, brace_pos + 1)) {
        single_line_body = 1;
    }

    size_t close_line = 0;
    if (!find_block_end(program, *index, end, &close_line, err)) {
        return 0;
    }

    Program single_prog = {0};
    int using_single_prog = 0;
    size_t orig_index = *index;
    Program *orig_program = program;
    size_t loop_body_start = *index + 1;
    size_t loop_body_end = close_line;
    if (single_line_body) {
        size_t body_start = brace_pos + 1;
        size_t body_end = body_start;
        int depth = 1;
        while (body_end < strlen(line_text) && depth > 0) {
            if (line_text[body_end] == '{') depth++;
            else if (line_text[body_end] == '}') { depth--; if (depth == 0) break; }
            body_end++;
        }
        size_t body_len = body_end - body_start;
        char *body_copy = malloc(body_len + 1);
        memcpy(body_copy, line_text + body_start, body_len);
        body_copy[body_len] = '\0';

        single_prog.path = qs_strdup("<loop_body>");
        single_prog.dir = qs_strdup(".");
        split_text_to_program(body_copy, &single_prog);
        free(body_copy);
        using_single_prog = 1;
        program = &single_prog;
        loop_body_start = 0;
        loop_body_end = single_prog.count;
    }

    if (strcmp(word, "repeat") == 0) {
        size_t arrow_pos = 0;
        if (find_arrow(line_text, after_word, brace_pos, &arrow_pos)) {
            char *item_part = trim_copy(line_text + after_word, arrow_pos - after_word);
            char *index_name = NULL;
            char *item_name = NULL;

            char *comma = strchr(item_part, ',');
            if (comma) {
                *comma = '\0';
                index_name = trim_copy(item_part, strlen(item_part));
                item_name = trim_copy(comma + 1, strlen(comma + 1));
                if (!validate_variable_name(index_name, (int)*index + 1, err) ||
                    !validate_variable_name(item_name, (int)*index + 1, err)) {
                    free(item_part);
                    free(index_name);
                    free(item_name);
                    return 0;
                }
            } else {
                item_name = qs_strdup(item_part);
                if (!validate_variable_name(item_name, (int)*index + 1, err)) {
                    free(item_part);
                    free(item_name);
                    return 0;
                }
            }
            free(item_part);

            Value iterable;
            if (!eval_expression_text(env, line_text + arrow_pos + 2, brace_pos - arrow_pos - 2, (int)*index + 1, &iterable, err)) {
                free(item_name);
                free(index_name);
                return 0;
            }

            if (iterable.type == VALUE_STRING) {
                size_t len = strlen(iterable.as.string);
                for (size_t n = 0; n < len; n++) {
                    if (index_name) env_set(env, index_name, make_integer((long)n + 1));
                    char *letter = qs_strndup(iterable.as.string + n, 1);
                    env_set(env, item_name, make_string_owned(letter));
                    g_loop_depth++;
                    if (!execute_range(program, env, loop_body_start, loop_body_end, err)) {
                        g_loop_depth--;
                        free_value(iterable);
                        free(item_name);
                        free(index_name);
                        return 0;
                    }
                    g_loop_depth--;
                    if (g_has_return) {
                        free_value(iterable);
                        free(item_name);
                        free(index_name);
                        return 1;
                    }
                    if (g_has_break) {
                        g_has_break = 0;
                        break;
                    }
                    if (g_has_continue) {
                        g_has_continue = 0;
                        continue;
                    }
                }
            } else if (iterable.type == VALUE_LIST) {
                for (size_t n = 0; n < iterable.as.list.count; n++) {
                    if (index_name) env_set(env, index_name, make_integer((long)n + 1));
                    env_set(env, item_name, clone_value(iterable.as.list.items[n]));
                    g_loop_depth++;
                    if (!execute_range(program, env, loop_body_start, loop_body_end, err)) {
                        g_loop_depth--;
                        free_value(iterable);
                        free(item_name);
                        free(index_name);
                        return 0;
                    }
                    g_loop_depth--;
                    if (g_has_return) {
                        free_value(iterable);
                        free(item_name);
                        free(index_name);
                        return 1;
                    }
                    if (g_has_break) {
                        g_has_break = 0;
                        break;
                    }
                    if (g_has_continue) {
                        g_has_continue = 0;
                        continue;
                    }
                }
            } else if (iterable.type == VALUE_DICT) {
                for (size_t n = 0; n < iterable.as.dict.count; n++) {
                    if (index_name) {
                        env_set(env, index_name, make_string_owned(qs_strdup(iterable.as.dict.entries[n].key)));
                        env_set(env, item_name, clone_value(iterable.as.dict.entries[n].value));
                    } else {
                        env_set(env, item_name, make_string_owned(qs_strdup(iterable.as.dict.entries[n].key)));
                    }
                    g_loop_depth++;
                    if (!execute_range(program, env, loop_body_start, loop_body_end, err)) {
                        g_loop_depth--;
                        free_value(iterable);
                        free(item_name);
                        free(index_name);
                        return 0;
                    }
                    g_loop_depth--;
                    if (g_has_return) {
                        free_value(iterable);
                        free(item_name);
                        free(index_name);
                        return 1;
                    }
                    if (g_has_break) {
                        g_has_break = 0;
                        break;
                    }
                    if (g_has_continue) {
                        g_has_continue = 0;
                        continue;
                    }
                }
            } else if (iterable.type == VALUE_INTEGER) {
                for (long n = 1; n <= iterable.as.integer; n++) {
                    if (index_name) env_set(env, index_name, make_integer(n));
                    env_set(env, item_name, make_integer(n));
                    g_loop_depth++;
                    if (!execute_range(program, env, loop_body_start, loop_body_end, err)) {
                        g_loop_depth--;
                        free_value(iterable);
                        free(item_name);
                        free(index_name);
                        return 0;
                    }
                    g_loop_depth--;
                    if (g_has_return) {
                        free_value(iterable);
                        free(item_name);
                        free(index_name);
                        return 1;
                    }
                    if (g_has_break) {
                        g_has_break = 0;
                        break;
                    }
                    if (g_has_continue) {
                        g_has_continue = 0;
                        continue;
                    }
                }
            } else {
                set_error(err, (int)*index + 1, (int)arrow_pos + 1, "repeat item -> value needs a number, string, list, or map");
                free_value(iterable);
                free(item_name);
                free(index_name);
                return 0;
            }

            free_value(iterable);
            free(item_name);
            free(index_name);
        } else {
            Value times;
            if (!eval_expression_text(env, line_text + after_word, brace_pos - after_word, (int)*index + 1, &times, err)) {
                return 0;
            }
            if (times.type != VALUE_INTEGER) {
                set_error(err, (int)*index + 1, (int)after_word + 1, "repeat needs a number");
                free_value(times);
                return 0;
            }
            for (long n = 0; n < times.as.integer; n++) {
                g_loop_depth++;
                if (!execute_range(program, env, loop_body_start, loop_body_end, err)) {
                    g_loop_depth--;
                    free_value(times);
                    return 0;
                }
                g_loop_depth--;
                if (g_has_return) {
                    free_value(times);
                    return 1;
                }
                if (g_has_break) {
                    g_has_break = 0;
                    break;
                }
                if (g_has_continue) {
                    g_has_continue = 0;
                    continue;
                }
            }
            free_value(times);
        }
    } else {
        int guard = 0;
        while (1) {
            Value condition;
            if (!eval_expression_text(env, line_text + after_word, brace_pos - after_word, (int)*index + 1, &condition, err)) {
                return 0;
            }
            int keep_going = value_truthy(condition);
            free_value(condition);
            if (!keep_going) {
                break;
            }
            if (++guard > 100000) {
                set_error(err, (int)*index + 1, 1, "while loop stopped after 100000 runs");
                return 0;
            }
            g_loop_depth++;
            if (!execute_range(program, env, loop_body_start, loop_body_end, err)) {
                g_loop_depth--;
                return 0;
            }
            g_loop_depth--;
            if (g_has_return) {
                return 1;
            }
            if (g_has_break) {
                g_has_break = 0;
                break;
            }
            if (g_has_continue) {
                g_has_continue = 0;
                continue;
            }
        }
    }

    if (using_single_prog) {
        for (size_t fi = 0; fi < single_prog.count; fi++) {
            free(single_prog.items[fi]);
        }
        free(single_prog.items);
        free(single_prog.path);
        free(single_prog.dir);
        *index = orig_index;
        program = orig_program;
        return 2;
    } else {
        *index = close_line;
    }
    return 1;
}

static int execute_inline_block(Environment *env, const char *line_text, size_t brace_pos, int line_no, ParseError *err) {
    size_t body_start = brace_pos + 1;
    size_t body_end = body_start;
    size_t len = strlen(line_text);
    int depth = 1;
    while (body_end < len && depth > 0) {
        if (line_text[body_end] == '{') depth++;
        else if (line_text[body_end] == '}') { depth--; if (depth == 0) break; }
        body_end++;
    }
    size_t body_len = body_end - body_start;
    char *body_copy = malloc(body_len + 1);
    memcpy(body_copy, line_text + body_start, body_len);
    body_copy[body_len] = '\0';

    Program body_prog = {0};
    body_prog.path = qs_strdup("<inline_block>");
    body_prog.dir = qs_strdup(".");
    split_text_to_program(body_copy, &body_prog);
    free(body_copy);
    (void)line_no;

    int ok = execute_range(&body_prog, env, 0, body_prog.count, err);

    for (size_t fi = 0; fi < body_prog.count; fi++) {
        free(body_prog.items[fi]);
    }
    free(body_prog.items);
    free(body_prog.path);
    free(body_prog.dir);
    return ok;
}

static int execute_if(Program *program, Environment *env, size_t *index, size_t end, ParseError *err) {
    const char *line_text = program->items[*index];
    size_t after_word = 0;
    size_t brace_pos = 0;
    line_starts_with(line_text, "if", &after_word);

    if (!line_has_open_brace(line_text, after_word, &brace_pos)) {
        set_error(err, (int)*index + 1, 1, "if needs '{' on the same line");
        return 0;
    }
    int inline_body = !rest_is_space_or_comment(line_text, brace_pos + 1);

    size_t if_close = 0;
    if (!find_block_end(program, *index, end, &if_close, err)) {
        return 0;
    }

    Value condition;
    if (!eval_expression_text(env, line_text + after_word, brace_pos - after_word, (int)*index + 1, &condition, err)) {
        return 0;
    }
    int is_true = value_truthy(condition);
    free_value(condition);

    if (is_true) {
        if (inline_body) {
            if (!execute_inline_block(env, line_text, brace_pos, (int)*index + 1, err)) {
                return 0;
            }
        } else if (!execute_range(program, env, *index + 1, if_close, err)) {
            return 0;
        }
        if (g_has_return) {
            return 1;
        }
    }

    int branch_ran = is_true;
    size_t chain_end = if_close;
    size_t cursor = if_close;

    while (1) {
        size_t branch_line = cursor + 1;
        size_t branch_after = 0;
        size_t branch_brace = 0;
        int is_maybe = 0;
        int is_else = 0;

        if (line_is_close_word(program->items[cursor], "maybe", &branch_after)) {
            branch_line = cursor;
            branch_after += strlen("maybe");
            is_maybe = 1;
        } else if (line_is_close_else(program->items[cursor], &branch_after)) {
            branch_line = cursor;
            branch_after += strlen("else");
            is_else = 1;
        } else {
            while (branch_line < end && line_is_empty_or_comment(program->items[branch_line])) {
                branch_line++;
            }
            if (branch_line < end && line_starts_with(program->items[branch_line], "maybe", &branch_after)) {
                is_maybe = 1;
            } else if (branch_line < end && line_starts_with(program->items[branch_line], "else", &branch_after)) {
                is_else = 1;
            }
        }

        if (!is_maybe && !is_else) {
            break;
        }

        if (!line_has_open_brace(program->items[branch_line], branch_after, &branch_brace)) {
            set_error(err, (int)branch_line + 1, 1, is_maybe ? "maybe needs '{' on the same line" : "else needs '{' on the same line");
            return 0;
        }
        int branch_inline = !rest_is_space_or_comment(program->items[branch_line], branch_brace + 1);

        size_t branch_close = 0;
        if (!find_block_end(program, branch_line, end, &branch_close, err)) {
            return 0;
        }

        if (is_maybe && !branch_ran) {
            Value maybe_condition;
            if (!eval_expression_text(env, program->items[branch_line] + branch_after, branch_brace - branch_after, (int)branch_line + 1, &maybe_condition, err)) {
                return 0;
            }
            int maybe_true = value_truthy(maybe_condition);
            free_value(maybe_condition);
            if (maybe_true) {
                if (branch_inline) {
                    if (!execute_inline_block(env, program->items[branch_line], branch_brace, (int)branch_line + 1, err)) {
                        return 0;
                    }
                } else if (!execute_range(program, env, branch_line + 1, branch_close, err)) {
                    return 0;
                }
                if (g_has_return) {
                    return 1;
                }
                branch_ran = 1;
            }
        } else if (is_else) {
            if (!branch_ran) {
                if (branch_inline) {
                    if (!execute_inline_block(env, program->items[branch_line], branch_brace, (int)branch_line + 1, err)) {
                        return 0;
                    }
                } else if (!execute_range(program, env, branch_line + 1, branch_close, err)) {
                    return 0;
                }
                if (g_has_return) {
                    return 1;
                }
                branch_ran = 1;
            }
            chain_end = branch_close;
            break;
        }

        chain_end = branch_close;
        cursor = branch_close;
    }

    *index = chain_end;
    return 1;
}

static int find_block_end_at_brace(Program *program, size_t open_line, size_t brace_pos, size_t end, size_t *close_line, ParseError *err) {
    int depth = 0;
    for (size_t i = open_line; i < end; i++) {
        const char *line = program->items[i];
        size_t start = (i == open_line) ? brace_pos : 0;
        size_t len = strlen(line);
        for (size_t j = start; j < len; j++) {
            if (line[j] == '{') depth++;
            else if (line[j] == '}') {
                depth--;
                if (depth == 0) { *close_line = i; return 1; }
            }
        }
    }
    set_error(err, (int)open_line + 1, 1, "try/oops block is missing '}'");
    return 0;
}

static int execute_try(Program *program, Environment *env, size_t *index, size_t end, ParseError *err) {
    const char *line_text = program->items[*index];
    size_t after_word = 0;
    size_t brace_pos = 0;
    line_starts_with(line_text, "try", &after_word);

    if (!line_has_open_brace(line_text, after_word, &brace_pos)) {
        set_error(err, (int)*index + 1, 1, "try needs '{' on the same line");
        return 0;
    }
    int try_inline = !rest_is_space_or_comment(line_text, brace_pos + 1);

    size_t try_close = 0;
    if (!find_block_end(program, *index, end, &try_close, err)) {
        return 0;
    }

    size_t oops_line = try_close + 1;
    size_t oops_after = 0;
    size_t oops_brace = 0;
    char *error_var_name = NULL;

    int oops_found = 0;
    if (try_close == *index) {
        size_t depth = 0;
        size_t close_pos = brace_pos;
        size_t p = brace_pos + 1;
        for (size_t j = brace_pos; line_text[j]; j++) {
            if (line_text[j] == '{') {
                depth++;
            } else if (line_text[j] == '}') {
                depth--;
                if (depth == 0) {
                    close_pos = j;
                    p = j + 1;
                    break;
                }
            }
        }
        while (line_text[p] && isspace((unsigned char)line_text[p])) p++;
        if (strncmp(line_text + p, "oops", 4) == 0 && !is_name_part(line_text[p + 4])) {
            oops_line = *index;
            oops_after = p + strlen("oops");
            oops_found = 1;
        }
        (void)close_pos;
    }

    if (!oops_found) {
        if (line_is_close_word(program->items[try_close], "oops", &oops_after)) {
            oops_line = try_close;
            oops_after += strlen("oops");
        } else {
            while (oops_line < end && line_is_empty_or_comment(program->items[oops_line])) {
                oops_line++;
            }
            if (oops_line >= end || !line_starts_with(program->items[oops_line], "oops", &oops_after)) {
                set_error(err, (int)*index + 1, 1, "try needs oops");
                return 0;
            }
        }
    }

    {
        const char *oops_text = program->items[oops_line];
        size_t pos = oops_after;

        while (oops_text[pos] && isspace((unsigned char)oops_text[pos])) pos++;

        if (is_name_start(oops_text[pos]) && oops_text[pos] != '{') {
            size_t name_start = pos;
            while (is_name_part(oops_text[pos])) pos++;
            error_var_name = qs_strndup(oops_text + name_start, pos - name_start);
            oops_after = pos;
        }
    }

    if (!line_has_open_brace(program->items[oops_line], oops_after, &oops_brace)) {
        set_error(err, (int)oops_line + 1, 1, "oops needs '{' on the same line");
        free(error_var_name);
        return 0;
    }
    size_t oops_close = 0;
    if (!find_block_end_at_brace(program, oops_line, oops_brace, end, &oops_close, err)) {
        free(error_var_name);
        return 0;
    }

    ParseError try_err = {0};
    int try_ok;
    if (try_inline) {
        try_ok = execute_inline_block(env, line_text, brace_pos, (int)*index + 1, &try_err);
    } else {
        try_ok = execute_range(program, env, *index + 1, try_close, &try_err);
    }
    if (!try_ok) {
        if (error_var_name) {
            env_set(env, error_var_name, make_string_owned(qs_strdup(try_err.message)));
        }
        ParseError oops_err = {0};
        int oops_ok = 1;
        if (oops_close == oops_line) {
            oops_ok = execute_inline_block(env, program->items[oops_line], oops_brace, (int)oops_line + 1, &oops_err);
        } else {
            if (!rest_is_space_or_comment(program->items[oops_line], oops_brace + 1)) {
                oops_ok = execute_inline_block(env, program->items[oops_line], oops_brace, (int)oops_line + 1, &oops_err);
            }
            if (oops_ok) {
                oops_ok = execute_range(program, env, oops_line + 1, oops_close, &oops_err);
            }
        }
        if (!oops_ok) {
            *err = oops_err;
            free(error_var_name);
            return 0;
        }

        if (error_var_name) {
            int idx = find_variable(env, error_var_name);
            if (idx >= 0) {
                free_value(env->items[idx].value);
                for (size_t i = (size_t)idx; i < env->count - 1; i++) {
                    env->items[i] = env->items[i + 1];
                }
                env->count--;
            }
        }
    }
    free(error_var_name);
    *index = oops_close;
    return 1;
}

static int execute_range(Program *program, Environment *env, size_t start, size_t end, ParseError *err) {
    int skip_increment = 0;
    for (size_t i = start; i < end; i++) {
        if (!skip_increment) {
        } else {
            skip_increment = 0;
        }
        if (loop_control_should_stop()) {
            return 1;
        }
        const char *line_text = program->items[i];
        size_t pos = 0;
        skip_spaces_text(line_text, &pos);
        if (line_text[pos] == '\0' || line_text[pos] == '#') {
            continue;
        }
        if (line_is_close_brace(line_text)) {
            set_error(err, (int)i + 1, 1, "unexpected '}'");
            return 0;
        }

        size_t after_word = 0;
        if (line_starts_with(line_text, "func", &after_word)) {
            size_t close_line = 0;
            if (!find_block_end(program, i, end, &close_line, err)) {
                return 0;
            }
            i = close_line;
            continue;
        }
        if (line_starts_with(line_text, "class", &after_word)) {
            size_t name_start = after_word;
            while (line_text[name_start] == ' ' || line_text[name_start] == '\t') name_start++;
            size_t name_end = name_start;
            while (line_text[name_end] && is_name_part(line_text[name_end])) name_end++;
            size_t name_len = name_end - name_start;
            char *class_name = qs_strndup(line_text + name_start, name_len);

            size_t close_line = 0;
            if (!find_block_end(program, i, end, &close_line, err)) {
                free(class_name);
                return 0;
            }

            Value class_val = make_class(class_name);
            env_set(env, class_name, class_val);

            size_t body_start = name_end;
            while (line_text[body_start] && line_text[body_start] != '{') body_start++;
            body_start++;

            size_t body_end = body_start;
            int depth = 1;
            while (line_text[body_end] && depth > 0) {
                if (line_text[body_end] == '{') depth++;
                else if (line_text[body_end] == '}') depth--;
                if (depth > 0) body_end++;
            }

            size_t pos = body_start;
            while (pos < body_end) {
                while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t' || line_text[pos] == '\n' || line_text[pos] == '\r')) pos++;
                if (pos >= body_end) break;

                int found_method = 0;
                if (strncmp(line_text + pos, "func", 4) == 0 && !is_name_part(line_text[pos + 4])) {
                    pos += 4;
                    found_method = 1;
                } else if (strncmp(line_text + pos, "fn", 2) == 0 && !is_name_part(line_text[pos + 2])) {
                    pos += 2;
                    found_method = 1;
                } else if (is_name_start(line_text[pos])) {
                    size_t check = pos;
                    while (check < body_end && is_name_part(line_text[check])) check++;
                    if (check < body_end && line_text[check] == '(') {
                        found_method = 1;
                    }
                }

                if (!found_method) {
                    pos++;
                    continue;
                }

                while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t')) pos++;

                size_t mname_start = pos;
                while (pos < body_end && is_name_part(line_text[pos])) pos++;
                size_t mname_len = pos - mname_start;
                char *method_name = qs_strndup(line_text + mname_start, mname_len);

                size_t qname_len = name_len + 1 + mname_len + 1;
                char *qname = malloc(qname_len);
                snprintf(qname, qname_len, "%s.%s", class_name, method_name);

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

                        size_t pname_start = pos;
                        while (pos < body_end && is_name_part(line_text[pos])) pos++;
                        size_t pname_len = pos - pname_start;
                        char *pname = qs_strndup(line_text + pname_start, pname_len);

                        while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t')) pos++;

                        Param param = {0};
                        param.name = pname;
                        if (pos < body_end && line_text[pos] == ':') {
                            pos++;
                            while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t')) pos++;
                            size_t tname_start = pos;
                            while (pos < body_end && is_name_part(line_text[pos])) pos++;
                            param.type = qs_strndup(line_text + tname_start, pos - tname_start);
                        } else {
                            param.type = NULL;
                        }

                        param_push(&function, param);

                        while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t')) pos++;
                        if (pos < body_end && line_text[pos] == ',') {
                            pos++;
                            continue;
                        }
                        break;
                    }

                    if (pos < body_end && line_text[pos] == ')') pos++;
                }

                while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t')) pos++;

                if (pos < body_end && line_text[pos] == '-' && pos + 1 < body_end && line_text[pos + 1] == '>') {
                    pos += 2;
                    while (pos < body_end && line_text[pos] != '{') pos++;
                }

                if (pos >= body_end || line_text[pos] != '{') {
                    free(method_name);
                    continue;
                }

                int mdepth = 1;
                size_t mbody_start = pos + 1;
                size_t mbody_end = mbody_start;
                while (mbody_end < body_end && mdepth > 0) {
                    if (line_text[mbody_end] == '{') mdepth++;
                    else if (line_text[mbody_end] == '}') mdepth--;
                    if (mdepth > 0) mbody_end++;
                }

                size_t body_len = mbody_end - mbody_start;
                char *body_raw = malloc(body_len + 1);
                memcpy(body_raw, line_text + mbody_start, body_len);
                body_raw[body_len] = '\0';

                Program *method_prog = calloc(1, sizeof(Program));
                method_prog->path = qs_strdup("<class_method>");
                method_prog->dir = qs_strdup(".");

                split_text_to_program(body_raw, method_prog);
                if (method_prog->count == 0) {
                    program_push(method_prog, qs_strdup(""));
                }
                free(body_raw);

                function.body_start = 0;
                function.body_end = method_prog->count;
                function.program = method_prog;
                function.is_module = 0;
                function_push(function);

                free(method_name);
                pos = mbody_end + 1;
            }

            free(class_name);
            i = close_line;
            continue;
        }
        if (line_starts_with(line_text, "from", &after_word)) {
            continue;
        }
        if (line_starts_with(line_text, "return", &after_word)) {
            if (g_function_depth <= 0) {
                set_error(err, (int)i + 1, 1, "return can only be used inside a function");
                return 0;
            }
            Value value;
            if (!eval_expression_text(env, line_text + after_word, strlen(line_text + after_word), (int)i + 1, &value, err)) {
                return 0;
            }
            g_return_value = value;
            g_has_return = 1;
            return 1;
        }
        if (line_starts_with(line_text, "break", &after_word)) {
            if (g_loop_depth <= 0) {
                set_error(err, (int)i + 1, 1, "break can only be used inside a loop");
                return 0;
            }
            if (!rest_is_space_or_comment(line_text, after_word)) {
                set_error(err, (int)i + 1, (int)after_word + 1, "break does not take a value");
                return 0;
            }
            g_has_break = 1;
            return 1;
        }
        if (line_starts_with(line_text, "continue", &after_word)) {
            if (g_loop_depth <= 0) {
                set_error(err, (int)i + 1, 1, "continue can only be used inside a loop");
                return 0;
            }
            if (!rest_is_space_or_comment(line_text, after_word)) {
                set_error(err, (int)i + 1, (int)after_word + 1, "continue does not take a value");
                return 0;
            }
            g_has_continue = 1;
            return 1;
        }
        if (line_starts_with(line_text, "if", &after_word)) {
            if (!execute_if(program, env, &i, end, err)) {
                return 0;
            }
            continue;
        }
        if (line_starts_with(line_text, "try", &after_word)) {
            if (!execute_try(program, env, &i, end, err)) {
                return 0;
            }
            continue;
        }
        if (line_starts_with(line_text, "oops", &after_word)) {
            size_t check = after_word;
            while (line_text[check] && isspace((unsigned char)line_text[check])) check++;
            if (line_text[check] == '(') {
                Value msg = make_null();
                ParseError expr_err = {0};
                const char *expr_start = line_text + after_word;
                size_t expr_len = strlen(expr_start);
                if (eval_expression_text(env, expr_start, expr_len, (int)i + 1, &msg, &expr_err)) {
                    char *msg_str = value_to_string(msg);
                    set_error(err, (int)i + 1, 1, msg_str);
                    free(msg_str);
                    free_value(msg);
                } else {
                    set_error(err, (int)i + 1, 1, "oops needs a message");
                }
                return 0;
            }
            set_error(err, (int)i + 1, 1, "oops without try");
            return 0;
        }
        if (line_starts_with(line_text, "maybe", &after_word)) {
            set_error(err, (int)i + 1, 1, "maybe without if");
            return 0;
        }
        if (line_starts_with(line_text, "else", &after_word)) {
            set_error(err, (int)i + 1, 1, "else without if");
            return 0;
        }
        if (line_starts_with(line_text, "repeat", &after_word)) {
            int loop_result = execute_loop(program, env, &i, end, "repeat", err);
            if (loop_result == 0) {
                return 0;
            }
            if (g_has_return) {
                return 1;
            }
            if (loop_result == 2) {
                skip_increment = 1;
            }
            continue;
        }
        if (line_starts_with(line_text, "while", &after_word)) {
            int loop_result = execute_loop(program, env, &i, end, "while", err);
            if (loop_result == 0) {
                return 0;
            }
            if (g_has_return) {
                return 1;
            }
            if (loop_result == 2) {
                skip_increment = 1;
            }
            continue;
        }
        if (line_starts_with(line_text, "print", &after_word)) {
            if (!execute_print(env, line_text, (int)i + 1, err)) {
                return 0;
            }
            continue;
        }

        size_t assignment_pos = 0;
        char assign_op = '=';
        if (find_assignment(line_text, &assignment_pos, &assign_op)) {
            if (!execute_assignment(env, line_text, assignment_pos, assign_op, (int)i + 1, err)) {
                return 0;
            }
            continue;
        }

        if (is_name_start(line_text[pos])) {
            size_t method_pos = pos + 1;
            while (is_name_part(line_text[method_pos])) {
                method_pos++;
            }
            skip_spaces_text(line_text, &method_pos);
            if (line_text[method_pos] == '.') {
                if (!execute_method_statement(env, line_text, (int)i + 1, err)) {
                    return 0;
                }
                continue;
            }
            if (line_text[method_pos] == '(') {
                Value ignored;
                if (!eval_expression_text(env, line_text, strlen(line_text), (int)i + 1, &ignored, err)) {
                    return 0;
                }
                free_value(ignored);
                continue;
            }
        }

        if (line_is_free_text_comment(line_text)) {
            continue;
        }

        {
            const char *word = line_text + pos;
            size_t wl = 0;
            while (is_name_part(word[wl])) wl++;
            char msg[256];
            if (wl > 0 && is_name_start(word[0])) {
                size_t copy = wl < sizeof(msg) - 32 ? wl : sizeof(msg) - 32;
                snprintf(msg, sizeof(msg), "unknown variable '%.*s'", (int)copy, word);
            } else {
                snprintf(msg, sizeof(msg), "unexpected statement");
            }
            set_error(err, (int)i + 1, (int)pos + 1, msg);
            return 0;
        }
    }
    return 1;
}
