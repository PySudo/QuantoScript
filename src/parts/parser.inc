static int parse_list(Parser *parser, Value *out) {
    ValueList list = {0};
    parser->pos++;
    skip_spaces(parser);

    if (parser->text[parser->pos] == ']') {
        parser->pos++;
        out->type = VALUE_LIST;
        out->as.list = list;
        return 1;
    }

    while (parser->text[parser->pos]) {
        Value element;
        if (!parse_expression(parser, &element)) {
            goto fail;
        }
        list_push(&list, element);
        skip_spaces(parser);

        if (parser->text[parser->pos] == ',') {
            parser->pos++;
            skip_spaces(parser);
            if (parser->text[parser->pos] == ']') {
                parser_error(parser, "trailing comma in list");
                goto fail;
            }
            continue;
        }

        if (parser->text[parser->pos] == ']') {
            parser->pos++;
            out->type = VALUE_LIST;
            out->as.list = list;
            return 1;
        }

        parser_error(parser, "expected ',' or ']' in list");
        goto fail;
    }

    parser_error(parser, "unterminated list literal");

fail:
    for (size_t i = 0; i < list.count; i++) {
        free_value(list.items[i]);
    }
    free(list.items);
    return 0;
}

static int parse_map_body(Parser *parser, Value *out, char end_char, const char *name, char separator) {
    Value map_value = make_dict_empty();
    skip_spaces(parser);
    if (match_char(parser, end_char)) {
        *out = map_value;
        return 1;
    }

    while (1) {
        Value key;
        if (!parse_expression(parser, &key)) {
            free_value(map_value);
            return 0;
        }
        if (key.type != VALUE_STRING) {
            parser_error(parser, "map keys must be strings");
            free_value(key);
            free_value(map_value);
            return 0;
        }
        if (!match_char(parser, separator)) {
            char message[64];
            snprintf(message, sizeof(message), "%s needs '%c' after key", name, separator);
            parser_error(parser, message);
            free_value(key);
            free_value(map_value);
            return 0;
        }
        Value value;
        if (!parse_expression(parser, &value)) {
            free_value(key);
            free_value(map_value);
            return 0;
        }
        dict_set(&map_value.as.dict, key.as.string, value);
        free_value(key);
        skip_spaces(parser);
        if (match_char(parser, ',')) {
            skip_spaces(parser);
            if (match_char(parser, end_char)) {
                break;
            }
            continue;
        }
        if (match_char(parser, end_char)) {
            break;
        }
        char message[64];
        snprintf(message, sizeof(message), "%s needs ',' or '%c'", name, end_char);
        parser_error(parser, message);
        free_value(map_value);
        return 0;
    }

    *out = map_value;
    return 1;
}

static char *value_to_string(Value value) {
    switch (value.type) {
        case VALUE_NULL: return qs_strdup("null");
        case VALUE_BOOL: return qs_strdup(value.as.boolean ? "true" : "false");
        case VALUE_INTEGER: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%ld", value.as.integer);
            return qs_strdup(buf);
        }
        case VALUE_FLOAT: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.10g", value.as.floating);
            return qs_strdup(buf);
        }
        case VALUE_STRING: return qs_strdup(value.as.string);
        case VALUE_LIST: {
            size_t cap = 64;
            char *result = malloc(cap);
            result[0] = '[';
            result[1] = '\0';
            for (size_t i = 0; i < value.as.list.count; i++) {
                if (i > 0) {
                    size_t len = strlen(result);
                    if (len + 2 >= cap) { cap *= 2; result = realloc(result, cap); }
                    strcat(result, ", ");
                }
                char *elem = value_to_string(value.as.list.items[i]);
                size_t rlen = strlen(result);
                size_t elen = strlen(elem);
                if (rlen + elen + 2 >= cap) { cap = (rlen + elen + 2) * 2; result = realloc(result, cap); }
                strcat(result, elem);
                free(elem);
            }
            size_t len = strlen(result);
            if (len + 2 >= cap) { result = realloc(result, len + 2); }
            result[len] = ']';
            result[len + 1] = '\0';
            return result;
        }
        case VALUE_DICT: {
            size_t cap = 64;
            char *result = malloc(cap);
            result[0] = '{';
            result[1] = '\0';
            for (size_t i = 0; i < value.as.dict.count; i++) {
                if (i > 0) {
                    size_t len = strlen(result);
                    if (len + 2 >= cap) { cap *= 2; result = realloc(result, cap); }
                    strcat(result, ", ");
                }
                char *k = value_to_string(make_string_owned(qs_strdup(value.as.dict.items[i].key)));
                char *v = value_to_string(value.as.dict.items[i].value);
                size_t rlen = strlen(result);
                size_t klen = strlen(k);
                size_t vlen = strlen(v);
                size_t need = rlen + klen + 3 + vlen + 2;
                if (need >= cap) { cap = need * 2; result = realloc(result, cap); }
                strcat(result, "'");
                strcat(result, k);
                strcat(result, "': ");
                strcat(result, v);
                free(k);
                free(v);
            }
            size_t len = strlen(result);
            if (len + 2 >= cap) { result = realloc(result, len + 2); }
            result[len] = '}';
            result[len + 1] = '\0';
            return result;
        }
        case VALUE_LAMBDA: return qs_strdup("<lambda>");
        case VALUE_CLASS: {
            char buf[256];
            snprintf(buf, sizeof(buf), "<class %s>", value.as.class_def.name);
            return qs_strdup(buf);
        }
        case VALUE_OBJECT: {
            char buf[256];
            snprintf(buf, sizeof(buf), "<object %s>", value.as.object.data->class_name);
            return qs_strdup(buf);
        }
        default: return qs_strdup("<unknown>");
    }
}

static int values_equal(Value left, Value right) {
    if (left.type != right.type) {
        if (left.type == VALUE_INTEGER && right.type == VALUE_FLOAT)
            return left.as.integer == (long)value_to_double(right);
        if (left.type == VALUE_FLOAT && right.type == VALUE_INTEGER)
            return (long)value_to_double(left) == right.as.integer;
        return 0;
    }
    switch (left.type) {
        case VALUE_NULL: return 1;
        case VALUE_BOOL: return left.as.boolean == right.as.boolean;
        case VALUE_INTEGER: return left.as.integer == right.as.integer;
        case VALUE_FLOAT: {
            double diff = value_to_double(left) - value_to_double(right);
            return diff < 1e-9 && diff > -1e-9;
        }
        case VALUE_STRING: return strcmp(left.as.string, right.as.string) == 0;
        case VALUE_LIST: {
            if (left.as.list.count != right.as.list.count) return 0;
            for (size_t i = 0; i < left.as.list.count; i++) {
                if (!values_equal(left.as.list.items[i], right.as.list.items[i])) return 0;
            }
            return 1;
        }
        case VALUE_DICT: {
            if (left.as.dict.count != right.as.dict.count) return 0;
            for (size_t i = 0; i < left.as.dict.count; i++) {
                int found = dict_find(&right.as.dict, left.as.dict.items[i].key);
                if (found < 0 || !values_equal(left.as.dict.items[i].value, right.as.dict.items[found].value)) return 0;
            }
            return 1;
        }
        default: return 0;
    }
}

static int require_numbers(Parser *parser, Value left, Value right, const char *op) {
    if ((left.type == VALUE_INTEGER || left.type == VALUE_FLOAT) &&
        (right.type == VALUE_INTEGER || right.type == VALUE_FLOAT)) {
        return 1;
    }
    char msg[128];
    snprintf(msg, sizeof(msg), "'%s' requires two numbers", op);
    parser_error(parser, msg);
    return 0;
}

static int parse_fstring(Parser *parser, Value *out) {
    char quote = parser->text[parser->pos];
    parser->pos++;
    size_t cap = 64;
    size_t len = 0;
    char *result = malloc(cap);
    result[0] = '\0';
    while (parser->text[parser->pos] && parser->text[parser->pos] != quote) {
        if (parser->text[parser->pos] == '{') {
            parser->pos++;
            Value expr_val;
            if (!parse_expression(parser, &expr_val)) {
                free(result);
                return 0;
            }
            skip_spaces(parser);
            if (parser->text[parser->pos] == '}') parser->pos++;
            char *expr_str = value_to_string(expr_val);
            size_t elen = strlen(expr_str);
            while (len + elen + 1 >= cap) { cap *= 2; result = realloc(result, cap); }
            memcpy(result + len, expr_str, elen);
            len += elen;
            result[len] = '\0';
            free(expr_str);
            free_value(expr_val);
        } else if (parser->text[parser->pos] == '\\' && parser->text[parser->pos + 1] == '{') {
            parser->pos++;
            if (len + 2 >= cap) { cap *= 2; result = realloc(result, cap); }
            result[len++] = '{';
            result[len] = '\0';
        } else {
            if (len + 2 >= cap) { cap *= 2; result = realloc(result, cap); }
            result[len++] = parser->text[parser->pos];
            result[len] = '\0';
        }
        parser->pos++;
    }
    if (parser->text[parser->pos] == quote) parser->pos++;
    *out = make_string_owned(result);
    return 1;
}

static int parse_fstring(Parser *parser, Value *out);
static int values_equal(Value left, Value right);
static int call_lambda(Parser *parser, Value *lambda, CallArgList *args, Value *out);
static int eval_expression_text(Environment *env, const char *text, size_t len, int line, Value *out, ParseError *err);

static int parse_lambda(Parser *parser, Value *out) {
    // Consume '('
    if (!match_char(parser, '(')) {
        parser_error(parser, "fn needs '('");
        return 0;
    }

    // Parse parameter list
    char **param_names = NULL;
    char **param_types = NULL;
    size_t param_count = 0;
    size_t param_capacity = 0;
    int has_types = 0;

    skip_spaces(parser);
    while (parser->text[parser->pos] && parser->text[parser->pos] != ')') {
        if (param_count > 0) {
            if (!match_char(parser, ',')) {
                parser_error(parser, "fn params need ','");
                goto lambda_fail;
            }
            skip_spaces(parser);
        }

        // Parse param name
        char *pname;
        if (!parse_name(parser, &pname)) {
            goto lambda_fail;
        }

        // Check for optional type hint
        char *ptype = NULL;
        skip_spaces(parser);
        if (parser->text[parser->pos] == ':') {
            parser->pos++;
            skip_spaces(parser);

            // Parse type: could be "int", "string", "int|string", etc.
            size_t type_cap = 0;
            size_t type_len = 0;
            char *type_buf = NULL;

            char *first_type;
            if (!parse_name(parser, &first_type)) {
                free(pname);
                goto lambda_fail;
            }
            size_t ft_len = strlen(first_type);
            type_cap = ft_len + 16;
            type_buf = malloc(type_cap);
            memcpy(type_buf, first_type, ft_len);
            type_len = ft_len;
            type_buf[type_len] = '\0';
            free(first_type);

            skip_spaces(parser);
            while (parser->text[parser->pos] == '|') {
                parser->pos++;
                skip_spaces(parser);
                char *alt_type;
                if (!parse_name(parser, &alt_type)) {
                    free(type_buf);
                    free(pname);
                    goto lambda_fail;
                }
                size_t alt_len = strlen(alt_type);
                while (type_len + alt_len + 2 > type_cap) {
                    type_cap *= 2;
                    type_buf = realloc(type_buf, type_cap);
                }
                type_buf[type_len++] = '|';
                memcpy(type_buf + type_len, alt_type, alt_len);
                type_len += alt_len;
                type_buf[type_len] = '\0';
                free(alt_type);
                skip_spaces(parser);
            }
            ptype = type_buf;
            has_types = 1;
        }

        // Grow arrays
        if (param_count >= param_capacity) {
            param_capacity = param_capacity ? param_capacity * 2 : 4;
            char **next_names = realloc(param_names, param_capacity * sizeof(char *));
            char **next_types = realloc(param_types, param_capacity * sizeof(char *));
            if (!next_names || !next_types) {
                fprintf(stderr, "out of memory\n");
                exit(2);
            }
            param_names = next_names;
            param_types = next_types;
        }
        param_names[param_count] = pname;
        param_types[param_count] = ptype;
        param_count++;

        skip_spaces(parser);
    }

    // Consume ')'
    if (!match_char(parser, ')')) {
        parser_error(parser, "fn needs ')'");
        goto lambda_fail;
    }

    // Consume '->'
    skip_spaces(parser);
    if (!match_op(parser, "->")) {
        parser_error(parser, "fn needs '->'");
        goto lambda_fail;
    }
    skip_spaces(parser);

    // Capture body text: everything until expression boundary
    size_t body_start = parser->pos;
    int depth = 0;
    while (parser->text[parser->pos]) {
        char ch = parser->text[parser->pos];
        if (ch == '(' || ch == '[' || ch == '{') depth++;
        else if (ch == ')' || ch == ']' || ch == '}') {
            if (depth == 0) break;
            depth--;
        }
        else if (ch == ',' && depth == 0) break;
        else if (ch == '#' && depth == 0) break;
        // Stop at keywords that start new statements
        else if (depth == 0 && is_name_start(ch)) {
            // Check for keywords that end the expression
            if (strncmp(parser->text + parser->pos, "else", 4) == 0 && !is_name_part(parser->text[parser->pos + 4])) break;
            if (strncmp(parser->text + parser->pos, "maybe", 5) == 0 && !is_name_part(parser->text[parser->pos + 5])) break;
            if (strncmp(parser->text + parser->pos, "and", 3) == 0 && !is_name_part(parser->text[parser->pos + 3])) break;
            if (strncmp(parser->text + parser->pos, "or", 2) == 0 && !is_name_part(parser->text[parser->pos + 2])) break;
            if (strncmp(parser->text + parser->pos, "in", 2) == 0 && !is_name_part(parser->text[parser->pos + 2])) break;
        }
        parser->pos++;
    }

    char *body = qs_strndup(parser->text + body_start, parser->pos - body_start);

    // Capture closure environment
    Environment *closure = malloc(sizeof(Environment));
    closure->count = parser->env->count;
    closure->capacity = parser->env->count;
    closure->items = calloc(closure->count, sizeof(Variable));
    for (size_t i = 0; i < closure->count; i++) {
        closure->items[i].name = qs_strdup(parser->env->items[i].name);
        closure->items[i].value = clone_value(parser->env->items[i].value);
    }

    *out = make_lambda(param_names, has_types ? param_types : NULL, param_count, body, closure);
    return 1;

lambda_fail:
    for (size_t i = 0; i < param_count; i++) {
        free(param_names[i]);
        if (param_types && param_types[i]) free(param_types[i]);
    }
    free(param_names);
    free(param_types);
    return 0;
}

static int parse_primary(Parser *parser, Value *out) {
    skip_spaces(parser);
    char ch = parser->text[parser->pos];

    if ((ch == 'f' || ch == 'F') && (parser->text[parser->pos + 1] == '\'' || parser->text[parser->pos + 1] == '"')) {
        parser->pos++;
        return parse_fstring(parser, out);
    }
    if (ch == '\'' || ch == '"') {
        return parse_string(parser, out);
    }
    if (ch == '[') {
        // Check for list comprehension: [expr for var in collection if cond]
        // Scan forward to see if 'for' keyword exists before ']'
        size_t scan = parser->pos + 1;
        int in_string = 0;
        char str_quote = '\0';
        int bracket_depth = 0;
        int found_for = 0;
        while (parser->text[scan] && parser->text[scan] != ']') {
            if (in_string) {
                if (parser->text[scan] == str_quote && quote_can_close(parser->text, scan, ']')) {
                    in_string = 0;
                }
            } else {
                if (parser->text[scan] == '\'' || parser->text[scan] == '"') {
                    in_string = 1;
                    str_quote = parser->text[scan];
                } else if (parser->text[scan] == '[') {
                    bracket_depth++;
                } else if (parser->text[scan] == ']') {
                    if (bracket_depth == 0) break;
                    bracket_depth--;
                } else if (parser->text[scan] == 'f' && parser->text[scan + 1] == 'o' &&
                           parser->text[scan + 2] == 'r' && !is_name_part(parser->text[scan + 3])) {
                    found_for = 1;
                    break;
                }
            }
            scan++;
        }

        if (found_for) {
            // Parse comprehension
            // Extract expression text (everything before 'for')
            size_t body_start = parser->pos + 1;
            size_t for_pos = scan;
            char *expr_text = qs_strndup(parser->text + body_start, for_pos - body_start);

            // Skip 'for'
            parser->pos = for_pos + 3;
            skip_spaces(parser);

            // Parse loop variable
            char *var_name;
            if (!parse_name(parser, &var_name)) {
                free(expr_text);
                parser_error(parser, "comprehension needs a variable name");
                return 0;
            }

            // Skip 'in'
            skip_spaces(parser);
            if (!match_word(parser, "in")) {
                free(expr_text);
                free(var_name);
                parser_error(parser, "comprehension needs 'in'");
                return 0;
            }

            // Parse collection
            Value collection;
            if (!parse_expression(parser, &collection)) {
                free(expr_text);
                free(var_name);
                return 0;
            }

            // Check for optional 'if' filter
            char *filter_text = NULL;
            skip_spaces(parser);
            if (match_word(parser, "if")) {
                size_t filter_start = parser->pos;
                int fdepth = 0;
                while (parser->text[parser->pos] && parser->text[parser->pos] != ']') {
                    if (parser->text[parser->pos] == '[') fdepth++;
                    else if (parser->text[parser->pos] == ']') { if (fdepth == 0) break; fdepth--; }
                    parser->pos++;
                }
                filter_text = qs_strndup(parser->text + filter_start, parser->pos - filter_start);
            }

            // Expect ']'
            skip_spaces(parser);
            if (!match_char(parser, ']')) {
                free(expr_text);
                free(var_name);
                free_value(collection);
                if (filter_text) free(filter_text);
                parser_error(parser, "comprehension needs ']'");
                return 0;
            }

            // Execute comprehension
            ValueList result = {0};
            size_t coll_len = 0;
            if (collection.type == VALUE_STRING) coll_len = strlen(collection.as.string);
            else if (collection.type == VALUE_LIST) coll_len = collection.as.list.count;
            else if (collection.type == VALUE_DICT) coll_len = collection.as.dict.count;

            for (size_t n = 0; n < coll_len; n++) {
                if (collection.type == VALUE_STRING) {
                    char letter[2] = {collection.as.string[n], '\0'};
                    env_set(parser->env, var_name, make_string_owned(qs_strdup(letter)));
                } else if (collection.type == VALUE_LIST) {
                    env_set(parser->env, var_name, clone_value(collection.as.list.items[n]));
                } else if (collection.type == VALUE_DICT) {
                    env_set(parser->env, var_name, make_string_owned(qs_strdup(collection.as.dict.items[n].key)));
                }

                if (filter_text) {
                    Value cond;
                    ParseError ferr = {0};
                    if (!eval_expression_text(parser->env, filter_text, strlen(filter_text), parser->line, &cond, &ferr)) {
                        free(expr_text); free(var_name); free_value(collection);
                        if (filter_text) free(filter_text);
                        parser_error(parser, ferr.message);
                        return 0;
                    }
                    int keep = value_truthy(cond);
                    free_value(cond);
                    if (!keep) continue;
                }

                Value item;
                ParseError eerr = {0};
                if (!eval_expression_text(parser->env, expr_text, strlen(expr_text), parser->line, &item, &eerr)) {
                    free(expr_text); free(var_name); free_value(collection);
                    if (filter_text) free(filter_text);
                    parser_error(parser, eerr.message);
                    return 0;
                }
                list_push(&result, item);
            }

            free(expr_text);
            free(var_name);
            free_value(collection);
            if (filter_text) free(filter_text);

            *out = (Value){.type = VALUE_LIST, .as.list = result};
            return 1;
        }

        // Not a comprehension, parse as regular list
        return parse_list(parser, out);
    }
    if (ch == '{') {
        parser->pos++;
        return parse_map_body(parser, out, '}', "map literal", ':');
    }
    if (isdigit((unsigned char)ch)) {
        return parse_number(parser, out);
    }
    if (ch == '(') {
        parser->pos++;
        skip_spaces(parser);
        if (!parse_expression(parser, out)) {
            return 0;
        }
        if (!match_char(parser, ')')) {
            parser_error(parser, "expected ')'");
            free_value(*out);
            return 0;
        }
        return 1;
    }

    if (is_name_start(ch)) {
        char *name;
        if (!parse_name(parser, &name)) {
            return 0;
        }
        if (strcmp(name, "true") == 0) {
            free(name);
            *out = make_bool(1);
            return 1;
        }
        if (strcmp(name, "false") == 0) {
            free(name);
            *out = make_bool(0);
            return 1;
        }
        if (strcmp(name, "null") == 0) {
            free(name);
            *out = make_null();
            return 1;
        }
        if (strcmp(name, "map") == 0 && parser->text[parser->pos] == '(') {
            free(name);
            if (!match_char(parser, '(')) {
                parser_error(parser, "map needs '('");
                return 0;
            }
            return parse_map_body(parser, out, ')', "map", '=');
        }

        if (strcmp(name, "fn") == 0 && parser->text[parser->pos] == '(') {
            free(name);
            return parse_lambda(parser, out);
        }

        if ((strcmp(name, "abs") == 0 || strcmp(name, "len") == 0 || strcmp(name, "type") == 0 || strcmp(name, "isinstance") == 0) && parser->text[parser->pos] == '(') {
            char *function_name = name;
            if (!match_char(parser, '(')) {
                char msg[256];
                snprintf(msg, sizeof(msg), "unknown variable '%s'", function_name);
                parser_error(parser, msg);
                free(function_name);
                return 0;
            }
            Value arg;
            if (!parse_expression(parser, &arg)) {
                free(function_name);
                return 0;
            }
            if (strcmp(function_name, "isinstance") == 0) {
                if (!match_char(parser, ',')) {
                    parser_error(parser, "isinstance needs two values");
                    free_value(arg);
                    free(function_name);
                    return 0;
                }
                Value type_name;
                if (!parse_expression(parser, &type_name)) {
                    free_value(arg);
                    free(function_name);
                    return 0;
                }
                if (!match_char(parser, ')')) {
                    parser_error(parser, "function needs ')'");
                    free_value(arg);
                    free_value(type_name);
                    free(function_name);
                    return 0;
                }
                if (type_name.type != VALUE_STRING) {
                    parser_error(parser, "isinstance type must be a string");
                    free_value(arg);
                    free_value(type_name);
                    free(function_name);
                    return 0;
                }
                *out = make_bool(type_matches(type_name.as.string, arg));
                free_value(arg);
                free_value(type_name);
                free(function_name);
                return 1;
            }
            if (!match_char(parser, ')')) {
                parser_error(parser, "function needs ')'");
                free_value(arg);
                free(function_name);
                return 0;
            }
            if (strcmp(function_name, "type") == 0) {
                *out = make_string_owned(qs_strdup(value_type_name(arg)));
            } else if (strcmp(function_name, "abs") == 0) {
                if (arg.type == VALUE_INTEGER) {
                    long n = arg.as.integer;
                    *out = make_integer(n < 0 ? -n : n);
                } else if (arg.type == VALUE_FLOAT) {
                    double d = arg.as.floating;
                    *out = make_float(d < 0 ? -d : d);
                } else {
                    parser_error(parser, "abs needs a number");
                    free_value(arg);
                    free(function_name);
                    return 0;
                }
            } else {
                if (arg.type == VALUE_STRING) {
                    *out = make_integer((long)strlen(arg.as.string));
                } else if (arg.type == VALUE_LIST) {
                    *out = make_integer((long)arg.as.list.count);
                } else {
                    parser_error(parser, "len needs a string or list");
                    free_value(arg);
                    free(function_name);
                    return 0;
                }
            }
            free_value(arg);
            free(function_name);
            return 1;
        }

        if (parser->text[parser->pos] == '(') {
            if (strcmp(name, "ever_return") == 0) {
                if (!match_char(parser, '(')) {
                    parser_error(parser, "ever_return needs '('");
                    free(name);
                    return 0;
                }
                skip_spaces(parser);
                char *target = NULL;
                if (parser->text[parser->pos] == '\'' || parser->text[parser->pos] == '"') {
                    Value target_value;
                    if (!parse_string(parser, &target_value)) {
                        free(name);
                        return 0;
                    }
                    target = qs_strdup(target_value.as.string);
                    free_value(target_value);
                } else if (!parse_name(parser, &target)) {
                    free(name);
                    return 0;
                }
                skip_spaces(parser);
                if (!match_char(parser, ')')) {
                    parser_error(parser, "ever_return needs ')'");
                    free(target);
                    free(name);
                    return 0;
                }
                int result = 0;
                if (!ever_return_name(target, &result)) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "unknown function '%s'", target);
                    parser_error(parser, msg);
                    free(target);
                    free(name);
                    return 0;
                }
                *out = make_bool(result);
                free(target);
                free(name);
    return 1;
}
            if (is_builtin_function(name)) {
                CallArgList args = {0};
                if (!parse_call_arguments(parser, &args)) {
                    free_call_args(&args);
                    free(name);
                    return 0;
                }
                int ok = call_builtin(parser, name, &args, out);
                free_call_args(&args);
                free(name);
                return ok;
            }
            // Check if it's a variable containing a lambda
            {
                int idx = find_variable(parser->env, name);
                if (idx >= 0 && parser->env->items[idx].value.type == VALUE_LAMBDA) {
                    *out = clone_value(parser->env->items[idx].value);
                    free(name);
                    CallArgList args = {0};
                    if (!parse_call_arguments(parser, &args)) {
                        free_call_args(&args);
                        free_value(*out);
                        return 0;
                    }
                    int ok = call_lambda(parser, out, &args, out);
                    free_call_args(&args);
                    if (!ok) {
                        free_value(*out);
                        return 0;
                    }
                    return 1;
                }
            }
            // Check if it's a class (for object creation: ClassName(args))
            {
                int idx = find_variable(parser->env, name);
                if (idx >= 0 && parser->env->items[idx].value.type == VALUE_CLASS) {
                    char *class_name = qs_strdup(parser->env->items[idx].value.as.class_def.name);
                    free(name);
                    CallArgList args = {0};
                    if (!parse_call_arguments(parser, &args)) {
                        free_call_args(&args);
                        free(class_name);
                        return 0;
                    }

                    // Create object
                    Value obj = make_object(class_name);
                    free(class_name);

                    // Find init method
                    char init_key[256];
                    snprintf(init_key, sizeof(init_key), "%s.init", obj.as.object.data->class_name);
                    Function *init_func = find_function(init_key);

                    if (init_func) {
                        // Call init(self, args...)
                        CallArgList init_args = {0};
                        init_args.count = args.count + 1;
                        init_args.items = calloc(args.count + 1, sizeof(CallArg));
                        init_args.items[0].value = clone_value(obj);
                        init_args.items[0].name = NULL;
                        init_args.items[0].is_keyword = 0;
                        for (size_t i = 0; i < args.count; i++) {
                            init_args.items[i + 1].value = clone_value(args.items[i].value);
                            init_args.items[i + 1].name = NULL;
                            init_args.items[i + 1].is_keyword = 0;
                        }
                        Value init_result;
                        int ok = call_user_function(parser, init_func, &init_args, &init_result);
                        free_call_args(&init_args);
                        if (!ok) {
                            free_value(obj);
                            free_call_args(&args);
                            return 0;
                        }
                        free_value(init_result);
                    }
                    free_call_args(&args);
                    *out = obj;
                    return 1;
                }
            }
            Function *function = find_function(name);
            if (!function) {
                char msg[256];
                snprintf(msg, sizeof(msg), "unknown function '%s'", name);
                parser_error(parser, msg);
                free(name);
                return 0;
            }
            CallArgList args = {0};
            if (!parse_call_arguments(parser, &args)) {
                free_call_args(&args);
                free(name);
                return 0;
            }
            int ok = call_user_function(parser, function, &args, out);
            free_call_args(&args);
            free(name);
            return ok;
        }

        int index = find_variable(parser->env, name);
        if (index < 0) {
            char msg[256];
            snprintf(msg, sizeof(msg), "unknown variable '%s'", name);
            parser_error(parser, msg);
            free(name);
            return 0;
        }
        parser->var_index = index;
        *out = clone_value(parser->env->items[index].value);
        free(name);

        // Check if this is a lambda being called
        if (out->type == VALUE_LAMBDA && parser->text[parser->pos] == '(') {
            CallArgList args = {0};
            if (!parse_call_arguments(parser, &args)) {
                free_call_args(&args);
                free_value(*out);
                return 0;
            }
            int ok = call_lambda(parser, out, &args, out);
            free_call_args(&args);
            if (!ok) {
                free_value(*out);
                return 0;
            }
        }
        return 1;
    }

    parser_error(parser, "expected a value");
    return 0;
}

static int normalize_index(long raw, size_t len, long *index) {
    if (raw > 0) {
        *index = raw - 1;
    } else if (raw < 0) {
        *index = (long)len + raw;
    } else {
        *index = 0;
    }
    return *index >= 0 && (size_t)*index < len;
}

static long normalize_slice_edge(long raw, size_t len, int is_start) {
    long edge;
    if (raw > 0) {
        edge = is_start ? raw - 1 : raw;
    } else if (raw < 0) {
        edge = is_start ? (long)len + raw : (long)len + raw + 1;
    } else {
        edge = is_start ? 0 : (long)len;
    }
    if (edge < 0) {
        return 0;
    }
    if ((size_t)edge > len) {
        return (long)len;
    }
    return edge;
}

static int slice_value(Parser *parser, Value *out, long start, long stop) {
    if (stop < start) {
        stop = start;
    }

    if (out->type == VALUE_STRING) {
        char *text = qs_strndup(out->as.string + start, (size_t)(stop - start));
        free_value(*out);
        *out = make_string_owned(text);
        return 1;
    }

    if (out->type == VALUE_LIST) {
        ValueList list = {0};
        for (long i = start; i < stop; i++) {
            list_push(&list, clone_value(out->as.list.items[i]));
        }
        free_value(*out);
        out->type = VALUE_LIST;
        out->as.list = list;
        return 1;
    }

    parser_error(parser, "only strings and lists can be sliced");
    free_value(*out);
    return 0;
}

static int parse_call_args(Parser *parser, ValueList *args) {
    if (!match_char(parser, '(')) {
        parser_error(parser, "method needs '('");
        return 0;
    }
    skip_spaces(parser);
    if (match_char(parser, ')')) {
        return 1;
    }

    while (1) {
        Value arg;
        if (!parse_expression(parser, &arg)) {
            for (size_t i = 0; i < args->count; i++) {
                free_value(args->items[i]);
            }
            free(args->items);
            args->items = NULL;
            args->count = 0;
            args->capacity = 0;
            return 0;
        }
        list_push(args, arg);
        skip_spaces(parser);
        if (match_char(parser, ',')) {
            continue;
        }
        if (match_char(parser, ')')) {
            return 1;
        }
        parser_error(parser, "method needs ',' or ')'");
        for (size_t i = 0; i < args->count; i++) {
            free_value(args->items[i]);
        }
        free(args->items);
        args->items = NULL;
        args->count = 0;
        args->capacity = 0;
        return 0;
    }
}

static int apply_method(Parser *parser, Value *out, const char *method, ValueList *args) {
    if (strcmp(method, "append") == 0) {
        if (out->type != VALUE_LIST) {
            parser_error(parser, "append works only on lists");
            return 0;
        }
        if (args->count != 1) {
            parser_error(parser, "append needs one value");
            return 0;
        }
        list_push(&out->as.list, clone_value(args->items[0]));
        return 1;
    }

    if (strcmp(method, "extend") == 0) {
        if (out->type != VALUE_LIST) {
            parser_error(parser, "extend works only on lists");
            return 0;
        }
        if (args->count != 1 || args->items[0].type != VALUE_LIST) {
            parser_error(parser, "extend needs one list");
            return 0;
        }
        for (size_t i = 0; i < args->items[0].as.list.count; i++) {
            list_push(&out->as.list, clone_value(args->items[0].as.list.items[i]));
        }
        return 1;
    }

    if (strcmp(method, "sort") == 0) {
        if (out->type != VALUE_LIST) {
            parser_error(parser, "sort works only on lists");
            return 0;
        }
        if (args->count != 0) {
            parser_error(parser, "sort does not take values");
            return 0;
        }
        list_sort(&out->as.list);
        return 1;
    }

    if (strcmp(method, "join") == 0) {
        if (out->type != VALUE_STRING) {
            parser_error(parser, "join works only on strings");
            return 0;
        }
        if (args->count != 1 || args->items[0].type != VALUE_LIST) {
            parser_error(parser, "join needs one list");
            return 0;
        }

        const char *separator = out->as.string;
        size_t separator_len = strlen(separator);
        size_t total = 0;
        char **parts = calloc(args->items[0].as.list.count, sizeof(char *));
        if (args->items[0].as.list.count && !parts) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        for (size_t i = 0; i < args->items[0].as.list.count; i++) {
            parts[i] = value_to_string(args->items[0].as.list.items[i]);
            total += strlen(parts[i]);
            if (i) {
                total += separator_len;
            }
        }

        char *joined = malloc(total + 1);
        if (!joined) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        joined[0] = '\0';
        for (size_t i = 0; i < args->items[0].as.list.count; i++) {
            if (i) {
                strcat(joined, separator);
            }
            strcat(joined, parts[i]);
            free(parts[i]);
        }
        free(parts);
        free_value(*out);
        *out = make_string_owned(joined);
        return 1;
    }

    if (strcmp(method, "contains") == 0 && out->type == VALUE_STRING) {
        if (args->count != 1) {
            parser_error(parser, "contains needs one value");
            return 0;
        }
        char *needle = value_to_string(args->items[0]);
        int found = strstr(out->as.string, needle) != NULL;
        free(needle);
        free_value(*out);
        *out = make_bool(found);
        return 1;
    }

    if (strcmp(method, "replace") == 0) {
        if (out->type != VALUE_STRING) {
            parser_error(parser, "replace works only on strings");
            return 0;
        }
        if (args->count != 2) {
            parser_error(parser, "replace needs two values");
            return 0;
        }
        char *old_str = value_to_string(args->items[0]);
        char *new_str = value_to_string(args->items[1]);
        const char *src = out->as.string;
        size_t old_len = strlen(old_str);
        size_t new_len = strlen(new_str);
        size_t result_cap = strlen(src) + 1;
        char *result = malloc(result_cap);
        if (!result) { fprintf(stderr, "out of memory\n"); exit(2); }
        result[0] = '\0';
        while (1) {
            const char *pos = strstr(src, old_str);
            if (!pos) { strcat(result, src); break; }
            size_t prefix = (size_t)(pos - src);
            if (strlen(result) + prefix + new_len + 1 > result_cap) {
                result_cap = (strlen(result) + prefix + new_len + 1) * 2;
                char *next = realloc(result, result_cap);
                if (!next) { fprintf(stderr, "out of memory\n"); exit(2); }
                result = next;
            }
            strncat(result, src, prefix);
            strcat(result, new_str);
            src = pos + old_len;
        }
        free(old_str);
        free(new_str);
        free_value(*out);
        *out = make_string_owned(result);
        return 1;
    }

    if (strcmp(method, "startsWith") == 0) {
        if (out->type != VALUE_STRING) {
            parser_error(parser, "startsWith works only on strings");
            return 0;
        }
        if (args->count != 1) {
            parser_error(parser, "startsWith needs one value");
            return 0;
        }
        char *prefix = value_to_string(args->items[0]);
        int match = strncmp(out->as.string, prefix, strlen(prefix)) == 0;
        free(prefix);
        free_value(*out);
        *out = make_bool(match);
        return 1;
    }

    if (strcmp(method, "endsWith") == 0) {
        if (out->type != VALUE_STRING) {
            parser_error(parser, "endsWith works only on strings");
            return 0;
        }
        if (args->count != 1) {
            parser_error(parser, "endsWith needs one value");
            return 0;
        }
        char *suffix = value_to_string(args->items[0]);
        size_t str_len = strlen(out->as.string);
        size_t suf_len = strlen(suffix);
        int match = str_len >= suf_len && strcmp(out->as.string + str_len - suf_len, suffix) == 0;
        free(suffix);
        free_value(*out);
        *out = make_bool(match);
        return 1;
    }

    if (strcmp(method, "title") == 0) {
        if (out->type != VALUE_STRING) {
            parser_error(parser, "title works only on strings");
            return 0;
        }
        if (args->count != 0) {
            parser_error(parser, "title does not take values");
            return 0;
        }
        char *text = qs_strdup(out->as.string);
        int new_word = 1;
        for (size_t i = 0; text[i]; i++) {
            if (isalpha((unsigned char)text[i])) {
                text[i] = new_word ? (char)toupper((unsigned char)text[i]) : (char)tolower((unsigned char)text[i]);
                new_word = 0;
            } else {
                new_word = 1;
            }
        }
        free_value(*out);
        *out = make_string_owned(text);
        return 1;
    }

    if (strcmp(method, "has") == 0) {
        if (out->type != VALUE_DICT) {
            parser_error(parser, "has works only on maps");
            return 0;
        }
        if (args->count != 1) {
            parser_error(parser, "has needs one value");
            return 0;
        }
        char *key = value_to_string(args->items[0]);
        int found = dict_find(&out->as.dict, key) >= 0;
        free(key);
        free_value(*out);
        *out = make_bool(found);
        return 1;
    }

    if (strcmp(method, "keys") == 0) {
        if (out->type != VALUE_DICT) {
            parser_error(parser, "keys works only on maps");
            return 0;
        }
        if (args->count != 0) {
            parser_error(parser, "keys does not take values");
            return 0;
        }
        ValueList keys = {0};
        for (size_t i = 0; i < out->as.dict.count; i++) {
            list_push(&keys, make_string_owned(qs_strdup(out->as.dict.items[i].key)));
        }
        free_value(*out);
        out->type = VALUE_LIST;
        out->as.list = keys;
        return 1;
    }

    if (strcmp(method, "values") == 0) {
        if (out->type != VALUE_DICT) {
            parser_error(parser, "values works only on maps");
            return 0;
        }
        if (args->count != 0) {
            parser_error(parser, "values does not take values");
            return 0;
        }
        ValueList vals = {0};
        for (size_t i = 0; i < out->as.dict.count; i++) {
            list_push(&vals, clone_value(out->as.dict.items[i].value));
        }
        free_value(*out);
        out->type = VALUE_LIST;
        out->as.list = vals;
        return 1;
    }

    if (strcmp(method, "items") == 0) {
        if (out->type != VALUE_DICT) {
            parser_error(parser, "items works only on maps");
            return 0;
        }
        if (args->count != 0) {
            parser_error(parser, "items does not take values");
            return 0;
        }
        ValueList pairs = {0};
        for (size_t i = 0; i < out->as.dict.count; i++) {
            ValueList pair = {0};
            list_push(&pair, make_string_owned(qs_strdup(out->as.dict.items[i].key)));
            list_push(&pair, clone_value(out->as.dict.items[i].value));
            Value pair_val;
            pair_val.type = VALUE_LIST;
            pair_val.as.list = pair;
            list_push(&pairs, pair_val);
        }
        free_value(*out);
        out->type = VALUE_LIST;
        out->as.list = pairs;
        return 1;
    }

    if (strcmp(method, "remove") == 0) {
        if (out->type != VALUE_DICT) {
            parser_error(parser, "remove works only on maps");
            return 0;
        }
        if (args->count != 1) {
            parser_error(parser, "remove needs one value");
            return 0;
        }
        char *key = value_to_string(args->items[0]);
        int idx = dict_find(&out->as.dict, key);
        free(key);
        if (idx >= 0) {
            free(out->as.dict.items[idx].key);
            free_value(out->as.dict.items[idx].value);
            for (size_t i = (size_t)idx; i < out->as.dict.count - 1; i++) {
                out->as.dict.items[i] = out->as.dict.items[i + 1];
            }
            out->as.dict.count--;
        }
        return 1;
    }

    if (strcmp(method, "split") == 0) {
        if (out->type != VALUE_STRING) {
            parser_error(parser, "split works only on strings");
            return 0;
        }
        if (args->count != 1) {
            parser_error(parser, "split needs one separator");
            return 0;
        }
        char *separator = value_to_string(args->items[0]);
        const char *text = out->as.string;
        size_t sep_len = strlen(separator);
        ValueList parts = {0};
        if (sep_len == 0) {
            for (size_t i = 0; text[i]; i++) {
                char single[2] = {text[i], '\0'};
                list_push(&parts, make_string_owned(qs_strdup(single)));
            }
        } else {
            const char *start = text;
            const char *found = NULL;
            while ((found = strstr(start, separator)) != NULL) {
                list_push(&parts, make_string_owned(qs_strndup(start, (size_t)(found - start))));
                start = found + sep_len;
            }
            list_push(&parts, make_string_owned(qs_strdup(start)));
        }
        free(separator);
        free_value(*out);
        out->type = VALUE_LIST;
        out->as.list = parts;
        return 1;
    }

    if (strcmp(method, "replace") == 0 && out->type == VALUE_STRING) {
        if (args->count != 2) { parser_error(parser, "replace needs old and new strings"); return 0; }
        char *old_str = value_to_string(args->items[0]);
        char *new_str = value_to_string(args->items[1]);
        const char *src = out->as.string;
        size_t old_len = strlen(old_str);
        size_t new_len = strlen(new_str);
        size_t cap = strlen(src) + 1;
        char *result = malloc(cap);
        if (!result) { fprintf(stderr, "out of memory\n"); exit(2); }
        result[0] = '\0';
        while (1) {
            const char *pos = strstr(src, old_str);
            if (!pos) { strcat(result, src); break; }
            size_t prefix = (size_t)(pos - src);
            if (strlen(result) + prefix + new_len + 1 > cap) {
                cap = (strlen(result) + prefix + new_len + 1) * 2;
                result = realloc(result, cap);
            }
            strncat(result, src, prefix);
            strcat(result, new_str);
            src = pos + old_len;
        }
        free(old_str);
        free(new_str);
        free_value(*out);
        *out = make_string_owned(result);
        return 1;
    }

    if (strcmp(method, "indexOf") == 0 && out->type == VALUE_STRING) {
        if (args->count != 1) { parser_error(parser, "indexOf needs one value"); return 0; }
        char *needle = value_to_string(args->items[0]);
        const char *pos = strstr(out->as.string, needle);
        free(needle);
        free_value(*out);
        *out = pos ? make_integer((long)(pos - out->as.string) + 1) : make_integer(0);
        return 1;
    }

    if (strcmp(method, "substring") == 0 && out->type == VALUE_STRING) {
        if (args->count < 1 || args->count > 2) { parser_error(parser, "substring needs 1-2 args"); return 0; }
        long start = args->items[0].as.integer;
        long end = args->count == 2 ? args->items[1].as.integer : (long)strlen(out->as.string);
        if (start < 1) start = 1;
        if (end > (long)strlen(out->as.string)) end = (long)strlen(out->as.string);
        if (start > end) start = end;
        char *sub = qs_strndup(out->as.string + start - 1, (size_t)(end - start + 1));
        free_value(*out);
        *out = make_string_owned(sub);
        return 1;
    }

    if (strcmp(method, "trimStart") == 0 && out->type == VALUE_STRING) {
        const char *s = out->as.string;
        while (*s && isspace((unsigned char)*s)) s++;
        char *trimmed = qs_strdup(s);
        free_value(*out);
        *out = make_string_owned(trimmed);
        return 1;
    }

    if (strcmp(method, "trimEnd") == 0 && out->type == VALUE_STRING) {
        char *s = qs_strdup(out->as.string);
        size_t len = strlen(s);
        while (len > 0 && isspace((unsigned char)s[len - 1])) len--;
        s[len] = '\0';
        free_value(*out);
        *out = make_string_owned(s);
        return 1;
    }

    if (strcmp(method, "repeat") == 0 && out->type == VALUE_STRING) {
        if (args->count != 1) { parser_error(parser, "repeat needs one count"); return 0; }
        long times = args->items[0].as.integer;
        if (times < 0) times = 0;
        size_t text_len = strlen(out->as.string);
        size_t total = text_len * (size_t)times;
        char *repeated = malloc(total + 1);
        if (!repeated) { fprintf(stderr, "out of memory\n"); exit(2); }
        repeated[0] = '\0';
        for (long i = 0; i < times; i++) strcat(repeated, out->as.string);
        free_value(*out);
        *out = make_string_owned(repeated);
        return 1;
    }

    if (strcmp(method, "reverse") == 0 && out->type == VALUE_STRING) {
        size_t len = strlen(out->as.string);
        char *rev = malloc(len + 1);
        if (!rev) { fprintf(stderr, "out of memory\n"); exit(2); }
        for (size_t i = 0; i < len; i++) rev[i] = out->as.string[len - 1 - i];
        rev[len] = '\0';
        free_value(*out);
        *out = make_string_owned(rev);
        return 1;
    }

    if (strcmp(method, "contains") == 0 && out->type == VALUE_STRING) {
        if (args->count != 1) { parser_error(parser, "contains needs one value"); return 0; }
        char *needle = value_to_string(args->items[0]);
        int found = strstr(out->as.string, needle) != NULL;
        free(needle);
        free_value(*out);
        *out = make_bool(found);
        return 1;
    }

    if (strcmp(method, "contains") == 0) {
        if (out->type != VALUE_LIST) {
            parser_error(parser, "contains works only on lists");
            return 0;
        }
        if (args->count != 1) {
            parser_error(parser, "contains needs one value");
            return 0;
        }
        int found = 0;
        for (size_t i = 0; i < out->as.list.count; i++) {
            if (values_equal(out->as.list.items[i], args->items[0])) {
                found = 1;
                break;
            }
        }
        free_value(*out);
        *out = make_bool(found);
        return 1;
    }

    if (strcmp(method, "flatten") == 0) {
        if (out->type != VALUE_LIST) {
            parser_error(parser, "flatten works only on lists");
            return 0;
        }
        if (args->count != 0) {
            parser_error(parser, "flatten does not take values");
            return 0;
        }
        ValueList flat = {0};
        for (size_t i = 0; i < out->as.list.count; i++) {
            if (out->as.list.items[i].type == VALUE_LIST) {
                for (size_t j = 0; j < out->as.list.items[i].as.list.count; j++) {
                    list_push(&flat, clone_value(out->as.list.items[i].as.list.items[j]));
                }
            } else {
                list_push(&flat, clone_value(out->as.list.items[i]));
            }
        }
        free_value(*out);
        out->type = VALUE_LIST;
        out->as.list = flat;
        return 1;
    }

    if (strcmp(method, "map") == 0) {
        if (out->type != VALUE_LIST) {
            parser_error(parser, "map works only on lists");
            return 0;
        }
        if (args->count != 1) {
            parser_error(parser, "map needs one argument (lambda)");
            return 0;
        }
        if (args->items[0].type != VALUE_LAMBDA) {
            parser_error(parser, "map argument must be a lambda");
            return 0;
        }
        ValueList result = {0};
        for (size_t i = 0; i < out->as.list.count; i++) {
            CallArgList call_args = {0};
            CallArg arg = {0};
            arg.value = clone_value(out->as.list.items[i]);
            call_arg_push(&call_args, arg);
            Value mapped;
            if (!call_lambda(parser, &args->items[0], &call_args, &mapped)) {
                free_call_args(&call_args);
                for (size_t j = 0; j < result.count; j++) free_value(result.items[j]);
                free(result.items);
                return 0;
            }
            list_push(&result, mapped);
            free_call_args(&call_args);
        }
        free_value(*out);
        out->type = VALUE_LIST;
        out->as.list = result;
        return 1;
    }

    if (strcmp(method, "filter") == 0) {
        if (out->type != VALUE_LIST) {
            parser_error(parser, "filter works only on lists");
            return 0;
        }
        if (args->count != 1) {
            parser_error(parser, "filter needs one argument (lambda)");
            return 0;
        }
        if (args->items[0].type != VALUE_LAMBDA) {
            parser_error(parser, "filter argument must be a lambda");
            return 0;
        }
        ValueList result = {0};
        for (size_t i = 0; i < out->as.list.count; i++) {
            CallArgList call_args = {0};
            CallArg arg = {0};
            arg.value = clone_value(out->as.list.items[i]);
            call_arg_push(&call_args, arg);
            Value cond;
            if (!call_lambda(parser, &args->items[0], &call_args, &cond)) {
                free_call_args(&call_args);
                for (size_t j = 0; j < result.count; j++) free_value(result.items[j]);
                free(result.items);
                return 0;
            }
            if (value_truthy(cond)) {
                list_push(&result, clone_value(out->as.list.items[i]));
            }
            free_value(cond);
            free_call_args(&call_args);
        }
        free_value(*out);
        out->type = VALUE_LIST;
        out->as.list = result;
        return 1;
    }

    if (strcmp(method, "find") == 0) {
        if (out->type != VALUE_LIST) {
            parser_error(parser, "find works only on lists");
            return 0;
        }
        if (args->count != 1) {
            parser_error(parser, "find needs one argument");
            return 0;
        }
        if (args->items[0].type == VALUE_LAMBDA) {
            for (size_t i = 0; i < out->as.list.count; i++) {
                CallArgList call_args = {0};
                CallArg arg = {0};
                arg.value = clone_value(out->as.list.items[i]);
                call_arg_push(&call_args, arg);
                Value cond;
                if (!call_lambda(parser, &args->items[0], &call_args, &cond)) {
                    free_call_args(&call_args);
                    return 0;
                }
                if (value_truthy(cond)) {
                    free_value(cond);
                    free_call_args(&call_args);
                    free_value(*out);
                    *out = clone_value(out->as.list.items[i]);
                    return 1;
                }
                free_value(cond);
                free_call_args(&call_args);
            }
        } else {
            for (size_t i = 0; i < out->as.list.count; i++) {
                if (values_equal(out->as.list.items[i], args->items[0])) {
                    free_value(*out);
                    *out = clone_value(out->as.list.items[i]);
                    return 1;
                }
            }
        }
        free_value(*out);
        *out = make_null();
        return 1;
    }

    if (strcmp(method, "find_type") == 0) {
        if (out->type != VALUE_LIST) {
            parser_error(parser, "find_type works only on lists");
            return 0;
        }
        if (args->count != 1 || args->items[0].type != VALUE_STRING) {
            parser_error(parser, "find_type needs a type name (string)");
            return 0;
        }
        char *type_name = args->items[0].as.string;
        for (size_t i = 0; i < out->as.list.count; i++) {
            if (type_matches(type_name, out->as.list.items[i])) {
                free_value(*out);
                *out = clone_value(out->as.list.items[i]);
                return 1;
            }
        }
        free_value(*out);
        *out = make_null();
        return 1;
    }

    if (strcmp(method, "sum") == 0) {
        if (out->type != VALUE_LIST) {
            parser_error(parser, "sum works only on lists");
            return 0;
        }
        long total = 0;
        for (size_t i = 0; i < out->as.list.count; i++) {
            if (out->as.list.items[i].type == VALUE_INTEGER) {
                total += out->as.list.items[i].as.integer;
            }
        }
        free_value(*out);
        *out = make_integer(total);
        return 1;
    }

    // Handle object method calls: obj.method(args)
    if (out->type == VALUE_OBJECT) {
        // Look up method as ClassName.method_name
        char mk[256];
        snprintf(mk, sizeof(mk), "%s.%s", out->as.object.data->class_name, method);
        Function *f = find_function(mk);
        if (f) {
            // Call method with self as first argument
            CallArgList call_args = {0};
            call_args.count = args->count + 1;
            call_args.items = calloc(args->count + 1, sizeof(CallArg));
            call_args.items[0].value = clone_value(*out);
            call_args.items[0].name = NULL;
            call_args.items[0].is_keyword = 0;
            for (size_t i = 0; i < args->count; i++) {
                call_args.items[i + 1].value = clone_value(args->items[i]);
                call_args.items[i + 1].name = NULL;
                call_args.items[i + 1].is_keyword = 0;
            }
            Value result;
            int ok = call_user_function(parser, f, &call_args, &result);
            free_call_args(&call_args);
            if (!ok) {
                return 0;
            }
            free_value(*out);
            *out = result;
            return 1;
        }
        // Check for field access (no args)
        if (args->count == 0) {
            // Look up field in object
            for (size_t i = 0; i < out->as.object.data->fields.count; i++) {
                if (strcmp(out->as.object.data->fields.items[i].key, method) == 0) {
                    Value field_val = clone_value(out->as.object.data->fields.items[i].value);
                    free_value(*out);
                    *out = field_val;
                    return 1;
                }
            }
            // Try as method with 0 args (property access)
            char mk2[256];
            snprintf(mk2, sizeof(mk2), "%s.%s", out->as.object.data->class_name, method);
            Function *f2 = find_function(mk2);
            if (f2) {
                CallArgList call_args = {0};
                call_args.count = 1;
                call_args.items = calloc(1, sizeof(CallArg));
                call_args.items[0].value = clone_value(*out);
                call_args.items[0].name = NULL;
                call_args.items[0].is_keyword = 0;
                Value result;
                int ok = call_user_function(parser, f2, &call_args, &result);
                free_call_args(&call_args);
                if (!ok) {
                    return 0;
                }
                free_value(*out);
                *out = result;
                return 1;
            }
            char msg[256];
            snprintf(msg, sizeof(msg), "unknown field or method '%s' on object", method);
            parser_error(parser, msg);
            return 0;
        }
        // Call method with args
        char mk3[256];
        snprintf(mk3, sizeof(mk3), "%s.%s", out->as.object.data->class_name, method);
        Function *f3 = find_function(mk3);
        if (!f3) {
            char msg[256];
            snprintf(msg, sizeof(msg), "unknown method '%s' on object", method);
            parser_error(parser, msg);
            return 0;
        }
        CallArgList call_args = {0};
        call_args.count = args->count + 1;
        call_args.items = calloc(args->count + 1, sizeof(CallArg));
        call_args.items[0].value = clone_value(*out);
        call_args.items[0].name = NULL;
        call_args.items[0].is_keyword = 0;
        for (size_t i = 0; i < args->count; i++) {
            call_args.items[i + 1].value = clone_value(args->items[i]);
            call_args.items[i + 1].name = NULL;
            call_args.items[i + 1].is_keyword = 0;
        }
        Value result;
        int ok = call_user_function(parser, f3, &call_args, &result);
        free_call_args(&call_args);
        if (!ok) {
            return 0;
        }
        free_value(*out);
        *out = result;
        return 1;
    }

    parser_error(parser, "unknown method");
    return 0;
}

static int parse_postfix(Parser *parser, Value *out) {
    if (!parse_primary(parser, out)) {
        return 0;
    }

    while (1) {
        if (match_char(parser, '.')) {
            char *method;
            if (!parse_name(parser, &method)) {
                free_value(*out);
                return 0;
            }
            ValueList args = {0};
            skip_spaces(parser);
            if (parser->text[parser->pos] == '(') {
                if (!parse_call_args(parser, &args)) {
                    free(method);
                    free_value(*out);
                    return 0;
                }
            }
            int ok = apply_method(parser, out, method, &args);
            int is_mutation = (strcmp(method, "extend") == 0 || strcmp(method, "append") == 0 ||
                 strcmp(method, "sort") == 0 || strcmp(method, "remove") == 0 ||
                 strcmp(method, "insert") == 0 || strcmp(method, "pop") == 0 ||
                 strcmp(method, "clear") == 0 || strcmp(method, "shuffle") == 0 ||
                 strcmp(method, "reverse") == 0 || strcmp(method, "fill") == 0 ||
                 strcmp(method, "splice") == 0 || strcmp(method, "push") == 0);
            for (size_t i = 0; i < args.count; i++) {
                free_value(args.items[i]);
            }
            free(args.items);
            free(method);
            if (!ok) {
                free_value(*out);
                return 0;
            }
            // Write back mutated value to variable for in-place methods
            if (parser->var_index >= 0 && is_mutation) {
                free_value(parser->env->items[parser->var_index].value);
                parser->env->items[parser->var_index].value = clone_value(*out);
            }
            parser->var_index = -1;
            continue;
        }

        if (!match_char(parser, '[')) {
            break;
        }

        int has_start = 0;
        long raw_start = 0;
        Value key_value = make_null();
        int has_key_value = 0;
        skip_spaces(parser);
        if (parser->text[parser->pos] != ':') {
            Value start_value;
            if (!parse_expression(parser, &start_value)) {
                free_value(*out);
                return 0;
            }
            if (start_value.type != VALUE_INTEGER) {
                if (start_value.type == VALUE_STRING) {
                    key_value = start_value;
                    has_key_value = 1;
                    has_start = 1;
                    goto parsed_index_start;
                }
                parser_error(parser, "index must be a number");
                free_value(*out);
                free_value(start_value);
                return 0;
            }
            raw_start = start_value.as.integer;
            has_start = 1;
            free_value(start_value);
        }
parsed_index_start:

        if (match_char(parser, ':')) {
            if (has_key_value) {
                parser_error(parser, "slice indexes must be numbers");
                free_value(*out);
                free_value(key_value);
                return 0;
            }
            int has_stop = 0;
            long raw_stop = 0;
            skip_spaces(parser);
            if (parser->text[parser->pos] != ']') {
                Value stop_value;
                if (!parse_expression(parser, &stop_value)) {
                    free_value(*out);
                    return 0;
                }
                if (stop_value.type != VALUE_INTEGER) {
                    parser_error(parser, "slice end must be a number");
                    free_value(*out);
                    free_value(stop_value);
                    return 0;
                }
                raw_stop = stop_value.as.integer;
                has_stop = 1;
                free_value(stop_value);
            }
            if (!match_char(parser, ']')) {
                parser_error(parser, "slice needs ']'");
                free_value(*out);
                return 0;
            }

            size_t len = out->type == VALUE_STRING ? strlen(out->as.string) :
                         out->type == VALUE_LIST ? out->as.list.count : 0;
            long start = has_start ? normalize_slice_edge(raw_start, len, 1) : 0;
            long stop = has_stop ? normalize_slice_edge(raw_stop, len, 0) : (long)len;
            if (!slice_value(parser, out, start, stop)) {
                return 0;
            }
            continue;
        }

        if (!has_start) {
            parser_error(parser, "index needs a number");
            free_value(*out);
            return 0;
        }

        if (out->type == VALUE_DICT) {
            if (!has_key_value) {
                parser_error(parser, "dict index must be a string");
                free_value(*out);
                return 0;
            }
            if (!match_char(parser, ']')) {
                parser_error(parser, "index needs ']'");
                free_value(*out);
                free_value(key_value);
                return 0;
            }
            int found = dict_find(&out->as.dict, key_value.as.string);
            if (found < 0) {
                parser_error(parser, "dict key not found");
                free_value(*out);
                free_value(key_value);
                return 0;
            }
            Value selected = clone_value(out->as.dict.items[found].value);
            free_value(*out);
            free_value(key_value);
            *out = selected;
        } else if (has_key_value) {
            parser_error(parser, "string keys work only on dicts");
            free_value(*out);
            free_value(key_value);
            return 0;
        } else if (out->type == VALUE_LIST) {
            long index;
            if (!normalize_index(raw_start, out->as.list.count, &index)) {
                parser_error(parser, raw_start == 0 ? "indexes start at 1" : "list index is out of range");
                free_value(*out);
                return 0;
            }
            if (!match_char(parser, ']')) {
                parser_error(parser, "index needs ']'");
                free_value(*out);
                return 0;
            }
            if ((size_t)index >= out->as.list.count) {
                parser_error(parser, "list index is out of range");
                free_value(*out);
                return 0;
            }
            Value selected = clone_value(out->as.list.items[index]);
            free_value(*out);
            *out = selected;
        } else if (out->type == VALUE_STRING) {
            size_t len = strlen(out->as.string);
            long index;
            if (!normalize_index(raw_start, len, &index)) {
                parser_error(parser, raw_start == 0 ? "indexes start at 1" : "string index is out of range");
                free_value(*out);
                return 0;
            }
            if (!match_char(parser, ']')) {
                parser_error(parser, "index needs ']'");
                free_value(*out);
                return 0;
            }
            char *letter = qs_strndup(out->as.string + index, 1);
            free_value(*out);
            *out = make_string_owned(letter);
        } else {
            parser_error(parser, "only strings and lists can be indexed");
            free_value(*out);
            return 0;
        }
    }
    return 1;
}

static int parse_unary(Parser *parser, Value *out) {
    skip_spaces(parser);
    if (match_word(parser, "not") || match_char(parser, '!')) {
        Value right;
        if (!parse_unary(parser, &right)) {
            return 0;
        }
        *out = make_bool(!value_truthy(right));
        free_value(right);
        return 1;
    }
    if (match_char(parser, '-')) {
        Value right;
        if (!parse_unary(parser, &right)) {
            return 0;
        }
        if (right.type == VALUE_INTEGER) {
            *out = make_integer(-right.as.integer);
            return 1;
        }
        if (right.type == VALUE_FLOAT) {
            *out = make_float(-right.as.floating);
            return 1;
        }
        parser_error(parser, "minus works only with numbers");
        free_value(right);
        return 0;
    }
    return parse_postfix(parser, out);
}

static int parse_factor(Parser *parser, Value *out) {
    if (!parse_unary(parser, out)) {
        return 0;
    }

    while (1) {
        char op = '\0';
        if (match_char(parser, '*')) {
            op = '*';
        } else if (match_char(parser, '/')) {
            op = '/';
        } else if (match_char(parser, '%')) {
            op = '%';
        } else {
            break;
        }

        Value right;
        if (!parse_unary(parser, &right)) {
            free_value(*out);
            return 0;
        }

        if (op == '*' &&
            ((out->type == VALUE_STRING && right.type == VALUE_INTEGER) ||
             (out->type == VALUE_INTEGER && right.type == VALUE_STRING))) {
            const char *text = out->type == VALUE_STRING ? out->as.string : right.as.string;
            long times = out->type == VALUE_INTEGER ? out->as.integer : right.as.integer;
            if (times < 0) {
                times = 0;
            }
            size_t text_len = strlen(text);
            size_t total_len = text_len * (size_t)times;
            char *repeated = malloc(total_len + 1);
            if (!repeated) {
                fprintf(stderr, "out of memory\n");
                exit(2);
            }
            repeated[0] = '\0';
            for (long i = 0; i < times; i++) {
                memcpy(repeated + ((size_t)i * text_len), text, text_len);
            }
            repeated[total_len] = '\0';
            free_value(*out);
            free_value(right);
            *out = make_string_owned(repeated);
            continue;
        }
        if (!require_numbers(parser, *out, right, op == '*' ? "*" : op == '/' ? "/" : "%")) {
            free_value(*out);
            free_value(right);
            return 0;
        }
        int left_float = (out->type == VALUE_FLOAT);
        int right_float = (right.type == VALUE_FLOAT);
        if (op == '%' && (left_float || right_float)) {
            parser_error(parser, "'%' works only with integers");
            free_value(*out);
            free_value(right);
            return 0;
        }
        if (left_float || right_float) {
            double l = value_to_double(*out);
            double r = value_to_double(right);
            if (op == '/' && r == 0.0) {
                parser_error(parser, "cannot divide by zero");
                free_value(*out);
                free_value(right);
                return 0;
            }
            double result = op == '*' ? l * r : l / r;
            free_value(*out);
            free_value(right);
            *out = make_float(result);
        } else {
            if ((op == '/' || op == '%') && right.as.integer == 0) {
                parser_error(parser, "cannot divide by zero");
                free_value(*out);
                free_value(right);
                return 0;
            }
            long result = op == '*' ? out->as.integer * right.as.integer :
                          op == '/' ? out->as.integer / right.as.integer :
                                      out->as.integer % right.as.integer;
            free_value(*out);
            free_value(right);
            *out = make_integer(result);
        }
    }
    return 1;
}

static int parse_term(Parser *parser, Value *out) {
    if (!parse_factor(parser, out)) {
        return 0;
    }

    while (1) {
        char op = '\0';
        if (match_char(parser, '+')) {
            op = '+';
        } else if (match_char(parser, '-')) {
            op = '-';
        } else {
            break;
        }

        Value right;
        if (!parse_factor(parser, &right)) {
            free_value(*out);
            return 0;
        }

        if (op == '+' && (out->type == VALUE_STRING || right.type == VALUE_STRING)) {
            char *left_text = value_to_string(*out);
            char *right_text = value_to_string(right);
            size_t len = strlen(left_text) + strlen(right_text);
            char *joined = malloc(len + 1);
            if (!joined) {
                fprintf(stderr, "out of memory\n");
                exit(2);
            }
            strcpy(joined, left_text);
            strcat(joined, right_text);
            free(left_text);
            free(right_text);
            free_value(*out);
            free_value(right);
            *out = make_string_owned(joined);
            continue;
        }

        if (!require_numbers(parser, *out, right, op == '+' ? "+" : "-")) {
            free_value(*out);
            free_value(right);
            return 0;
        }
        int left_float = (out->type == VALUE_FLOAT);
        int right_float = (right.type == VALUE_FLOAT);
        if (left_float || right_float) {
            double l = value_to_double(*out);
            double r = value_to_double(right);
            double result = op == '+' ? l + r : l - r;
            free_value(*out);
            free_value(right);
            *out = make_float(result);
        } else {
            long result = op == '+' ? out->as.integer + right.as.integer : out->as.integer - right.as.integer;
            free_value(*out);
            free_value(right);
            *out = make_integer(result);
        }
        return 1;
    }
    return 1;
}

static int value_contains(Value container, Value item) {
    if (container.type == VALUE_STRING) {
        char *needle = value_to_string(item);
        int found = strstr(container.as.string, needle) != NULL;
        free(needle);
        return found;
    }
    if (container.type == VALUE_LIST) {
        for (size_t i = 0; i < container.as.list.count; i++) {
            if (values_equal(container.as.list.items[i], item)) {
                return 1;
            }
        }
        return 0;
    }
    return 0;
}

static int parse_comparison(Parser *parser, Value *out) {
    if (!parse_term(parser, out)) {
        return 0;
    }

    int saw_comparison = 0;
    int all_true = 1;

    while (1) {
        const char *op = NULL;
        if (match_op(parser, "<=")) {
            op = "<=";
        } else if (match_op(parser, ">=")) {
            op = ">=";
        } else if (match_op(parser, "<")) {
            op = "<";
        } else if (match_op(parser, ">")) {
            op = ">";
        } else {
            break;
        }

        Value right;
        if (!parse_term(parser, &right)) {
            free_value(*out);
            return 0;
        }

        if (!require_numbers(parser, *out, right, op)) {
            free_value(*out);
            free_value(right);
            return 0;
        }
        int result;
        if (out->type == VALUE_FLOAT || right.type == VALUE_FLOAT) {
            double l = value_to_double(*out);
            double r = value_to_double(right);
            result = strcmp(op, "<") == 0 ? l < r :
                     strcmp(op, ">") == 0 ? l > r :
                     strcmp(op, "<=") == 0 ? l <= r :
                                             l >= r;
        } else {
            result = strcmp(op, "<") == 0 ? out->as.integer < right.as.integer :
                     strcmp(op, ">") == 0 ? out->as.integer > right.as.integer :
                     strcmp(op, "<=") == 0 ? out->as.integer <= right.as.integer :
                                             out->as.integer >= right.as.integer;
        }
        saw_comparison = 1;
        all_true = all_true && result;
        free_value(*out);
        *out = right;
    }

    if (saw_comparison) {
        free_value(*out);
        *out = make_bool(all_true);
    }
    return 1;
}

static int parse_membership(Parser *parser, Value *out) {
    if (!parse_comparison(parser, out)) {
        return 0;
    }

    while (match_word(parser, "in")) {
        Value container;
        if (!parse_comparison(parser, &container)) {
            free_value(*out);
            return 0;
        }
        int result = value_contains(container, *out);
        free_value(*out);
        free_value(container);
        *out = make_bool(result);
    }
    return 1;
}

static int parse_equality(Parser *parser, Value *out) {
    if (!parse_membership(parser, out)) {
        return 0;
    }

    while (1) {
        int negate = 0;
        if (match_op(parser, "==")) {
            negate = 0;
        } else if (match_op(parser, "!=")) {
            negate = 1;
        } else {
            break;
        }

        Value right;
        if (!parse_membership(parser, &right)) {
            free_value(*out);
            return 0;
        }

        int result = values_equal(*out, right);
        free_value(*out);
        free_value(right);
        *out = make_bool(negate ? !result : result);
    }
    return 1;
}

static int parse_and(Parser *parser, Value *out) {
    if (!parse_equality(parser, out)) {
        return 0;
    }

    while (match_word(parser, "and")) {
        Value right;
        if (!parse_equality(parser, &right)) {
            free_value(*out);
            return 0;
        }
        int result = value_truthy(*out) && value_truthy(right);
        free_value(*out);
        free_value(right);
        *out = make_bool(result);
    }
    return 1;
}

static int parse_expression(Parser *parser, Value *out) {
    if (!parse_and(parser, out)) {
        return 0;
    }

    while (match_word(parser, "or")) {
        Value right;
        if (!parse_and(parser, &right)) {
            free_value(*out);
            return 0;
        }
        int result = value_truthy(*out) || value_truthy(right);
        free_value(*out);
        free_value(right);
        *out = make_bool(result);
    }
    return 1;
}

// ---- JSON support (restored) ----
static int json_parse_value(const char *text, int *pos, Value *out, int *err) {
    while (text[*pos] == ' ' || text[*pos] == '\t' || text[*pos] == '\n' || text[*pos] == '\r') (*pos)++;
    char ch = text[*pos];
    if (ch == '"') { (*pos)++; size_t cap = 64; size_t len = 0; char *result = malloc(cap); result[0] = '\0';
        while (text[*pos] && text[*pos] != '"') { if (text[*pos] == '\\') { (*pos)++; char c = text[*pos]; switch (c) { case 'n': c = '\n'; break; case 't': c = '\t'; break; case 'r': c = '\r'; break; case '\\': c = '\\'; break; case '"': c = '"'; break; default: break; } if (len+2>=cap){cap*=2;result=realloc(result,cap);} result[len++]=c; result[len]='\0'; } else { if (len+2>=cap){cap*=2;result=realloc(result,cap);} result[len++]=text[*pos]; result[len]='\0'; } (*pos)++; } if (text[*pos]=='"') (*pos)++; *out = make_string_owned(result); return 1; }
    if (ch == '{') { (*pos)++; Value map = make_dict_empty(); while (text[*pos]==' ') (*pos)++;
        if (text[*pos]=='}') { (*pos)++; *out = map; return 1; }
        while (1) {
            while (text[*pos]==' ') (*pos)++;
            Value key;
            if (!json_parse_value(text,pos,&key,err)) { free_value(map); return 0; }
            while (text[*pos]==' ') (*pos)++;
            if (text[*pos]!=':') { free_value(map); free_value(key); *err=1; return 0; } (*pos)++;
            Value val; if (!json_parse_value(text,pos,&val,err)) { free_value(map); free_value(key); return 0; }
            dict_set(&map.as.dict, key.as.string, val); free_value(key); while (text[*pos]==' ') (*pos)++;
            if (text[*pos]==',') { (*pos)++; continue; } if (text[*pos]=='}') { (*pos)++; break; } free_value(map); *err=1; return 0; }
        *out = map; return 1; }
    if (ch == '[') { (*pos)++; ValueList list = {0}; while (text[*pos]==' ') (*pos)++;
        if (text[*pos]==']') { (*pos)++; Value v; v.type=VALUE_LIST; v.as.list=list; *out=v; return 1; }
        while (1) { Value item; if (!json_parse_value(text,pos,&item,err)) { for (size_t i=0;i<list.count;i++) free_value(list.items[i]); free(list.items); return 0; }
            list_push(&list, item); while (text[*pos]==' ') (*pos)++;
            if (text[*pos]==',') { (*pos)++; continue; } if (text[*pos]==']') { (*pos)++; break; } } Value v; v.type=VALUE_LIST; v.as.list=list; *out=v; return 1; }
    if (ch=='t' && strncmp(text+*pos,"true",4)==0) { *pos+=4; *out=make_bool(1); return 1; }
    if (ch=='f' && strncmp(text+*pos,"false",5)==0) { *pos+=5; *out=make_bool(0); return 1; }
    if (ch=='n' && strncmp(text+*pos,"null",4)==0) { *pos+=4; *out=make_null(); return 1; }
    if (ch=='-' || isdigit((unsigned char)ch)) { char *end=NULL; double d=strtod(text+*pos,&end); if (end>text+*pos) { *pos=(int)(end-text); *out=make_float(d); return 1; } }
    *err=1; return 0;
}
static char *json_stringify_value(Value value) {
    switch (value.type) {
        case VALUE_NULL: return qs_strdup("null");
        case VALUE_BOOL: return qs_strdup(value.as.boolean ? "true" : "false");
        case VALUE_INTEGER: { char buf[64]; snprintf(buf,sizeof(buf),"%ld",value.as.integer); return qs_strdup(buf); }
        case VALUE_FLOAT: { char buf[64]; snprintf(buf,sizeof(buf),"%g",value.as.floating); return qs_strdup(buf); }
        case VALUE_STRING: { size_t cap=strlen(value.as.string)*2+3; char *r=malloc(cap); r[0]='"'; r[1]='\0'; for (const char *p=value.as.string;*p;p++) { size_t l=strlen(r); if (*p=='"'||*p=='\\') { r=realloc(r,cap+2); r[l]='\\'; r[l+1]=*p; r[l+2]='\0'; } else { r=realloc(r,cap+1); r[l]=*p; r[l+1]='\0'; } } size_t l=strlen(r); r[l]='"'; r[l+1]='\0'; return r; }
        case VALUE_LIST: { size_t cap=64; char *r=malloc(cap); r[0]='['; r[1]='\0'; for (size_t i=0;i<value.as.list.count;i++) { if(i>0)strcat(r,","); char *e=json_stringify_value(value.as.list.items[i]); size_t l=strlen(r); r=realloc(r,l+strlen(e)+2); strcat(r,e); free(e); } size_t l=strlen(r); r=realloc(r,l+2); r[l]=']'; r[l+1]='\0'; return r; }
        case VALUE_DICT: { size_t cap=64; char *r=malloc(cap); r[0]='{'; r[1]='\0'; for (size_t i=0;i<value.as.dict.count;i++) { if(i>0)strcat(r,","); char *k=json_stringify_value(make_string_owned(qs_strdup(value.as.dict.items[i].key))); char *v=json_stringify_value(value.as.dict.items[i].value); size_t l=strlen(r); r=realloc(r,l+strlen(k)+1+strlen(v)+2); strcat(r,k); strcat(r,":"); strcat(r,v); free(k); free(v); } size_t l=strlen(r); r=realloc(r,l+2); r[l]='}'; r[l+1]='\0'; return r; }
        default: return qs_strdup("null");
    }
}
