static int execute_assignment(Environment *env, const char *line_text, size_t assignment_pos, char assign_op, int line, ParseError *err) {
    char *name = trim_copy(line_text, assignment_pos);
    if (!validate_variable_name(name, line, err)) {
        // Check if it's a field assignment: obj.field = value
        char *dot = strchr(name, '.');
        if (dot && assign_op == '=') {
            // Field assignment: obj.field = value
            size_t obj_len = dot - name;
            char *obj_name = qs_strndup(name, obj_len);
            char *field_name = dot + 1;

            int obj_idx = find_variable(env, obj_name);
            if (obj_idx < 0) {
                char msg[256];
                snprintf(msg, sizeof(msg), "unknown variable '%s'", obj_name);
                set_error(err, line, 1, msg);
                free(obj_name);
                free(name);
                return 0;
            }

            Value obj = env->items[obj_idx].value;
            if (obj.type != VALUE_OBJECT) {
                char msg[256];
                snprintf(msg, sizeof(msg), "'%s' is not an object", obj_name);
                set_error(err, line, 1, msg);
                free(obj_name);
                free(name);
                return 0;
            }

            // Evaluate the value expression
            Value value;
            size_t rhs_offset = assign_op == '=' ? 1 : 2;
            if (!eval_expression_text(env, line_text + assignment_pos + rhs_offset, strlen(line_text + assignment_pos + rhs_offset), line, &value, err)) {
                free(obj_name);
                free(name);
                return 0;
            }

            // Set the field
            int found = 0;
            for (size_t i = 0; i < obj.as.object.data->fields.count; i++) {
                if (strcmp(obj.as.object.data->fields.items[i].key, field_name) == 0) {
                    free_value(obj.as.object.data->fields.items[i].value);
                    obj.as.object.data->fields.items[i].value = value;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                // Add new field
                if (obj.as.object.data->fields.count == obj.as.object.data->fields.capacity) {
                    size_t next_cap = obj.as.object.data->fields.capacity ? obj.as.object.data->fields.capacity * 2 : 8;
                    DictEntry *next = realloc(obj.as.object.data->fields.items, next_cap * sizeof(DictEntry));
                    if (!next) {
                        fprintf(stderr, "out of memory\n");
                        exit(2);
                    }
                    obj.as.object.data->fields.items = next;
                    obj.as.object.data->fields.capacity = next_cap;
                }
                obj.as.object.data->fields.items[obj.as.object.data->fields.count].key = qs_strdup(field_name);
                obj.as.object.data->fields.items[obj.as.object.data->fields.count].value = value;
                obj.as.object.data->fields.count++;
            }

            free(obj_name);
            free(name);
            return 1;
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

    env_set(env, name, value);
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

    int index = find_variable(env, name);
    if (index < 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "unknown variable '%s'", name);
        parser_error(&parser, msg);
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

    Value value = clone_value(env->items[index].value);
    int ok = apply_method(&parser, &value, method, &args);
    for (size_t i = 0; i < args.count; i++) {
        free_value(args.items[i]);
    }
    free(args.items);

    // For mutation methods, write back the modified value to the variable
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
        free_value(env->items[index].value);
        env->items[index].value = value;
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
        // Content after { on the same line — single-line loop body
        single_line_body = 1;
    }

    size_t close_line = 0;
    if (!find_block_end(program, *index, end, &close_line, err)) {
        return 0;
    }

    // For single-line loops (body on same line as {), create a temp program
    Program single_prog = {0};
    int using_single_prog = 0;
    size_t orig_index = *index;
    Program *orig_program = program;
    size_t loop_body_start = *index + 1;
    size_t loop_body_end = close_line;
    if (single_line_body) {
        // Extract body text between matching { and } (depth-tracked for nested blocks)
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

        // Split body into individual statements using shared splitter
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
                    if (index_name) env_set(env, index_name, make_integer((long)n + 1));
                    env_set(env, item_name, make_string_owned(qs_strdup(iterable.as.dict.items[n].key)));
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
        // Free the temp program manually (free_program is defined later in the amalgamation)
        for (size_t fi = 0; fi < single_prog.count; fi++) {
            free(single_prog.items[fi]);
        }
        free(single_prog.items);
        free(single_prog.path);
        free(single_prog.dir);
        *index = orig_index;
        program = orig_program;
        return 2;  // Signal: single-line body, caller should re-parse same line
    } else {
        *index = close_line;
    }
    return 1;
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
    if (!rest_is_space_or_comment(line_text, brace_pos + 1)) {
        set_error(err, (int)*index + 1, (int)brace_pos + 2, "nothing can appear after '{'");
        return 0;
    }

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
        if (!execute_range(program, env, *index + 1, if_close, err)) {
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
        if (!rest_is_space_or_comment(program->items[branch_line], branch_brace + 1)) {
            set_error(err, (int)branch_line + 1, (int)branch_brace + 2, "nothing can appear after '{'");
            return 0;
        }

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
                if (!execute_range(program, env, branch_line + 1, branch_close, err)) {
                    return 0;
                }
                if (g_has_return) {
                    return 1;
                }
                branch_ran = 1;
            }
        } else if (is_else) {
            if (!branch_ran) {
                if (!execute_range(program, env, branch_line + 1, branch_close, err)) {
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

static int execute_try(Program *program, Environment *env, size_t *index, size_t end, ParseError *err) {
    const char *line_text = program->items[*index];
    size_t after_word = 0;
    size_t brace_pos = 0;
    line_starts_with(line_text, "try", &after_word);

    if (!line_has_open_brace(line_text, after_word, &brace_pos)) {
        set_error(err, (int)*index + 1, 1, "try needs '{' on the same line");
        return 0;
    }
    if (!rest_is_space_or_comment(line_text, brace_pos + 1)) {
        set_error(err, (int)*index + 1, (int)brace_pos + 2, "nothing can appear after '{'");
        return 0;
    }

    size_t try_close = 0;
    if (!find_block_end(program, *index, end, &try_close, err)) {
        return 0;
    }

    size_t oops_line = try_close + 1;
    size_t oops_after = 0;
    size_t oops_brace = 0;
    char *error_var_name = NULL;

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

    // Check for optional error variable name after "oops"
    {
        const char *oops_text = program->items[oops_line];
        size_t pos = oops_after;
        // Skip spaces
        while (oops_text[pos] && isspace((unsigned char)oops_text[pos])) pos++;
        // Check if there's a name before '{'
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
    if (!rest_is_space_or_comment(program->items[oops_line], oops_brace + 1)) {
        set_error(err, (int)oops_line + 1, (int)oops_brace + 2, "nothing can appear after '{'");
        free(error_var_name);
        return 0;
    }

    size_t oops_close = 0;
    if (!find_block_end(program, oops_line, end, &oops_close, err)) {
        free(error_var_name);
        return 0;
    }

    ParseError try_err = {0};
    if (!execute_range(program, env, *index + 1, try_close, &try_err)) {
        // Error occurred — bind error variable if name was given
        if (error_var_name) {
            env_set(env, error_var_name, make_string_owned(qs_strdup(try_err.message)));
        }
        ParseError oops_err = {0};
        if (!execute_range(program, env, oops_line + 1, oops_close, &oops_err)) {
            *err = oops_err;
            free(error_var_name);
            return 0;
        }
        // Remove error variable from scope after oops block
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
            // Normal iteration
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
            // Parse class definition: class Name { methods... }
            // The file loader merges multi-line constructs into single logical lines.
            // So the entire class definition is on one line.
            // Extract class name
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

            // Store class name as a CLASS value in the environment
            Value class_val = make_class(class_name);
            env_set(env, class_name, class_val);

            // Find the opening { of the class body
            size_t body_start = name_end;
            while (line_text[body_start] && line_text[body_start] != '{') body_start++;
            body_start++; // skip {

            // Find the matching } of the class body
            size_t body_end = body_start;
            int depth = 1;
            while (line_text[body_end] && depth > 0) {
                if (line_text[body_end] == '{') depth++;
                else if (line_text[body_end] == '}') depth--;
                if (depth > 0) body_end++;
            }
            // body_end now points to the closing }

            // Parse methods from the class body text
            size_t pos = body_start;
            while (pos < body_end) {
                // Skip whitespace and newlines
                while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t' || line_text[pos] == '\n' || line_text[pos] == '\r')) pos++;
                if (pos >= body_end) break;

                // Check for func/fn keyword, or direct method definition
                int found_method = 0;
                if (strncmp(line_text + pos, "func", 4) == 0 && !is_name_part(line_text[pos + 4])) {
                    pos += 4;
                    found_method = 1;
                } else if (strncmp(line_text + pos, "fn", 2) == 0 && !is_name_part(line_text[pos + 2])) {
                    pos += 2;
                    found_method = 1;
                } else if (is_name_start(line_text[pos])) {
                    // Check if this looks like a method definition: name(
                    size_t check = pos;
                    while (check < body_end && is_name_part(line_text[check])) check++;
                    if (check < body_end && line_text[check] == '(') {
                        found_method = 1;
                    }
                }

                if (!found_method) {
                    // Skip to next method (find next '}' that closes a method body)
                    // This is a field declaration or other content - skip it
                    pos++;
                    continue;
                }

                // Skip whitespace after func/fn
                while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t')) pos++;

                // Parse method name
                size_t mname_start = pos;
                while (pos < body_end && is_name_part(line_text[pos])) pos++;
                size_t mname_len = pos - mname_start;
                char *method_name = qs_strndup(line_text + mname_start, mname_len);

                // Build qualified name: ClassName.method_name
                size_t qname_len = name_len + 1 + mname_len + 1;
                char *qname = malloc(qname_len);
                snprintf(qname, qname_len, "%s.%s", class_name, method_name);

                Function function = {0};
                function.name = qname;

                // Add 'self' as first parameter
                Param self_param = {0};
                self_param.name = qs_strdup("self");
                self_param.type = qs_strdup("any");
                param_push(&function, self_param);

                // Skip whitespace before (
                while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t')) pos++;

                // Parse params
                if (pos < body_end && line_text[pos] == '(') {
                    pos++; // skip (
                    while (pos < body_end && line_text[pos] != ')') {
                        // Skip whitespace
                        while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t')) pos++;
                        if (pos >= body_end || line_text[pos] == ')') break;

                        // Parse param name
                        size_t pname_start = pos;
                        while (pos < body_end && is_name_part(line_text[pos])) pos++;
                        size_t pname_len = pos - pname_start;
                        char *pname = qs_strndup(line_text + pname_start, pname_len);

                        // Skip whitespace
                        while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t')) pos++;

                        // Optional type annotation
                        Param param = {0};
                        param.name = pname;
                        if (pos < body_end && line_text[pos] == ':') {
                            pos++; // skip :
                            while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t')) pos++;
                            size_t tname_start = pos;
                            while (pos < body_end && is_name_part(line_text[pos])) pos++;
                            param.type = qs_strndup(line_text + tname_start, pos - tname_start);
                        } else {
                            param.type = NULL;
                        }

                        param_push(&function, param);

                        // Skip whitespace
                        while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t')) pos++;
                        if (pos < body_end && line_text[pos] == ',') {
                            pos++;
                            continue;
                        }
                        break;
                    }
                    // Skip past )
                    if (pos < body_end && line_text[pos] == ')') pos++;
                }

                // Skip whitespace
                while (pos < body_end && (line_text[pos] == ' ' || line_text[pos] == '\t')) pos++;

                // Skip optional return type: -> type
                if (pos < body_end && line_text[pos] == '-' && pos + 1 < body_end && line_text[pos + 1] == '>') {
                    pos += 2;
                    while (pos < body_end && line_text[pos] != '{') pos++;
                }

                // Find method body { ... }
                if (pos >= body_end || line_text[pos] != '{') {
                    free(method_name);
                    continue;
                }

                // Find matching }
                int mdepth = 1;
                size_t mbody_start = pos + 1;
                size_t mbody_end = mbody_start;
                while (mbody_end < body_end && mdepth > 0) {
                    if (line_text[mbody_end] == '{') mdepth++;
                    else if (line_text[mbody_end] == '}') mdepth--;
                    if (mdepth > 0) mbody_end++;
                }
                // mbody_end now points to the closing }

                // Store method: body is from mbody_start to mbody_end
                size_t body_len = mbody_end - mbody_start;
                char *body_raw = malloc(body_len + 1);
                memcpy(body_raw, line_text + mbody_start, body_len);
                body_raw[body_len] = '\0';

                // Create a temporary program for the method body
                Program *method_prog = calloc(1, sizeof(Program));
                method_prog->path = qs_strdup("<class_method>");
                method_prog->dir = qs_strdup(".");

                // Split merged method body into individual statement lines.
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
                pos = mbody_end + 1; // skip past closing }
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
            // Check if this is oops("msg") throw expression
            size_t check = after_word;
            while (line_text[check] && isspace((unsigned char)line_text[check])) check++;
            if (line_text[check] == '(') {
                // oops("message") — evaluate argument and throw
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
                // Single-line body: re-parse rest of same line (don't skip)
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
                // Single-line body: re-parse rest of same line (don't skip)
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

    }
    return 1;
}
