// QuantoScript Bytecode VM - Executor

static int g_vm_trace = 0;

// ---- Exception propagation helper ----
// Returns 1 if exception was handled (jumped to catch), 0 otherwise.
static int vm_raise(VM *vm, const char *msg) {
    if (vm->try_stack_top > 0) {
        int handler = vm->try_stack[vm->try_stack_top - 1];
        int saved_depth = vm->try_stack_depths[vm->try_stack_top - 1];
        vm->try_stack_top--;
        vm->stack_top = saved_depth;
        vm_push(vm, make_string_owned(qs_strdup(msg)));
        vm->ip = handler;
        return 1; // handled
    }
    fprintf(stderr, "%s\n", msg);
    return 0; // unhandled
}

// ---- Generic Method Dispatch ----

static Value dispatch_method(Value receiver, const char *method, Value *args, int argc) {
    Value result = make_null();
    if (receiver.type == VALUE_STRING) {
        if (strcmp(method, "upper") == 0) { char *s = value_to_string(receiver); for (char *p = s; *p; p++) *p = toupper((unsigned char)*p); result = make_string_owned(s); }
        else if (strcmp(method, "lower") == 0) { char *s = value_to_string(receiver); for (char *p = s; *p; p++) *p = tolower((unsigned char)*p); result = make_string_owned(s); }
        else if (strcmp(method, "title") == 0) { char *s = value_to_string(receiver); int cap = 1; for (char *p = s; *p; p++) { if (isspace((unsigned char)*p)) cap = 1; else if (cap) { *p = toupper((unsigned char)*p); cap = 0; } else *p = tolower((unsigned char)*p); } result = make_string_owned(s); }
        else if (strcmp(method, "len") == 0) { result = make_integer((long)strlen(receiver.as.string)); }
        else if (strcmp(method, "contains") == 0 && argc >= 1) { char *sub = value_to_string(args[0]); result = make_bool(strstr(receiver.as.string, sub) != NULL); free(sub); }
        else if (strcmp(method, "replace") == 0 && argc >= 2) { char *old = value_to_string(args[0]); char *nw = value_to_string(args[1]); const char *p = receiver.as.string; size_t ol = strlen(old), nl = strlen(nw), rl = strlen(p); size_t cap = rl + 1; char *res = malloc(cap); res[0] = '\0'; const char *f; while ((f = strstr(p, old)) != NULL) { size_t prefix = f - p; size_t cur = strlen(res); if (cur + prefix + nl + 1 > cap) { cap = (cur + prefix + nl + 1) * 2; res = realloc(res, cap); } strncat(res, p, prefix); strcat(res, nw); p = f + ol; } strcat(res, p); result = make_string_owned(res); free(old); free(nw); }
        else if (strcmp(method, "startsWith") == 0 && argc >= 1) { char *prefix = value_to_string(args[0]); result = make_bool(strncmp(receiver.as.string, prefix, strlen(prefix)) == 0); free(prefix); }
        else if (strcmp(method, "endsWith") == 0 && argc >= 1) { char *suffix = value_to_string(args[0]); size_t sl = strlen(suffix), ol = strlen(receiver.as.string); result = make_bool(ol >= sl && strcmp(receiver.as.string + ol - sl, suffix) == 0); free(suffix); }
        else if (strcmp(method, "split") == 0 && argc >= 1) { char *delim = value_to_string(args[0]); ValueList list = {0}; const char *sp = receiver.as.string; size_t dl = strlen(delim); while (1) { const char *f = strstr(sp, delim); if (!f) { list_push(&list, make_string_owned(qs_strdup(sp))); break; } list_push(&list, make_string_owned(qs_strndup(sp, f - sp))); sp = f + dl; } result.type = VALUE_LIST; result.as.list = list; free(delim); }
    } else if (receiver.type == VALUE_LIST) {
        if (strcmp(method, "len") == 0 || strcmp(method, "count") == 0) { result = make_integer(receiver.as.list.count); }
        else if (strcmp(method, "push") == 0 || strcmp(method, "append") == 0) { if (argc >= 1) list_push(&receiver.as.list, clone_value(args[0])); result = receiver; receiver.type = VALUE_NULL; }
        else if (strcmp(method, "pop") == 0) { if (receiver.as.list.count > 0) { result = receiver.as.list.items[receiver.as.list.count - 1]; receiver.as.list.count--; } }
        else if (strcmp(method, "contains") == 0 && argc >= 1) { int found = 0; for (size_t i = 0; i < receiver.as.list.count; i++) if (values_equal(receiver.as.list.items[i], args[0])) { found = 1; break; } result = make_bool(found); }
        else if (strcmp(method, "index") == 0 && argc >= 1) { long idx = -1; for (size_t i = 0; i < receiver.as.list.count; i++) if (values_equal(receiver.as.list.items[i], args[0])) { idx = (long)i; break; } result = make_integer(idx); }
        else if (strcmp(method, "remove") == 0 && argc >= 1) { for (size_t i = 0; i < receiver.as.list.count; i++) { if (values_equal(receiver.as.list.items[i], args[0])) { free_value(receiver.as.list.items[i]); for (size_t j = i; j < receiver.as.list.count - 1; j++) receiver.as.list.items[j] = receiver.as.list.items[j + 1]; receiver.as.list.count--; break; } } result = receiver; receiver.type = VALUE_NULL; }
    } else if (receiver.type == VALUE_DICT) {
        if (strcmp(method, "keys") == 0) { ValueList list = {0}; for (size_t i = 0; i < receiver.as.dict.count; i++) list_push(&list, make_string_owned(qs_strdup(receiver.as.dict.items[i].key))); result.type = VALUE_LIST; result.as.list = list; }
        else if (strcmp(method, "values") == 0) { ValueList list = {0}; for (size_t i = 0; i < receiver.as.dict.count; i++) list_push(&list, clone_value(receiver.as.dict.items[i].value)); result.type = VALUE_LIST; result.as.list = list; }
        else if (strcmp(method, "items") == 0) { ValueList list = {0}; for (size_t i = 0; i < receiver.as.dict.count; i++) { Value pair; pair.type = VALUE_LIST; pair.as.list = (ValueList){0}; list_push(&pair.as.list, make_string_owned(qs_strdup(receiver.as.dict.items[i].key))); list_push(&pair.as.list, clone_value(receiver.as.dict.items[i].value)); list_push(&list, pair); } result.type = VALUE_LIST; result.as.list = list; }
        else if (strcmp(method, "has") == 0 && argc >= 1) { char *key = value_to_string(args[0]); result = make_bool(dict_find(&receiver.as.dict, key) >= 0); free(key); }
        else if (strcmp(method, "remove") == 0 && argc >= 1) { char *key = value_to_string(args[0]); int idx = dict_find(&receiver.as.dict, key); if (idx >= 0) { free(receiver.as.dict.items[idx].key); free_value(receiver.as.dict.items[idx].value); for (size_t j = idx; j < receiver.as.dict.count - 1; j++) receiver.as.dict.items[j] = receiver.as.dict.items[j + 1]; receiver.as.dict.count--; } free(key); result = receiver; receiver.type = VALUE_NULL; }
    }
    return result;
}

static int vm_execute(VM *vm) {
    while (vm->ip < (int)vm->program->count) {
        Instruction *instr = &vm->program->instructions[vm->ip];
        if (g_vm_trace) vm_dump_state(vm, "exec");
        vm->ip++;
        AUDIT_OPCODE();

        switch (instr->op) {
            case OP_LOAD_CONST: {
                vm_push(vm, clone_value(vm->program->constants[instr->arg_i]));
                break;
            }
            case OP_LOAD_VAR: {
                int idx = find_variable(vm->env, instr->arg.as.string);
                if (idx < 0) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "unknown variable '%s'", instr->arg.as.string);
                    if (vm_raise(vm, msg)) { continue; }
                    return 1;
                }
                vm_push(vm, clone_value(vm->env->items[idx].value));
                break;
            }
            case OP_STORE_VAR: {
                Value val = vm_pop(vm);
                env_set(vm->env, instr->arg.as.string, val);
                break;
            }
            case OP_LOAD_LOCAL: {
                // O(1) slot access from CallFrame's inline storage — NO HEAP
                CallFrame *cf = &vm->frames[vm->frame_count - 1];
                vm_push(vm, cf->local_slots[instr->arg_i]);
                break;
            }
            case OP_STORE_LOCAL: {
                // O(1) slot access from CallFrame's inline storage — NO HEAP
                CallFrame *cf = &vm->frames[vm->frame_count - 1];
                free_value(cf->local_slots[instr->arg_i]);
                cf->local_slots[instr->arg_i] = vm_pop(vm);
                break;
            }
            case OP_ADD: {
                Value right = vm_pop(vm);
                Value left = vm_pop(vm);
                if (left.type == VALUE_STRING || right.type == VALUE_STRING) {
                    char *lt = value_to_string(left);
                    char *rt = value_to_string(right);
                    size_t len = strlen(lt) + strlen(rt);
                    char *joined = malloc(len + 1);
                    strcpy(joined, lt);
                    strcat(joined, rt);
                    free(lt); free(rt);
                    free_value(left); free_value(right);
                    vm_push(vm, make_string_owned(joined));
                } else if (left.type == VALUE_FLOAT || right.type == VALUE_FLOAT) {
                    vm_push(vm, make_float(value_to_double(left) + value_to_double(right)));
                    free_value(left); free_value(right);
                } else {
                    vm_push(vm, make_integer(left.as.integer + right.as.integer));
                    free_value(left); free_value(right);
                }
                break;
            }
            case OP_SUB: {
                Value right = vm_pop(vm);
                Value left = vm_pop(vm);
                if (left.type == VALUE_FLOAT || right.type == VALUE_FLOAT) {
                    vm_push(vm, make_float(value_to_double(left) - value_to_double(right)));
                } else {
                    vm_push(vm, make_integer(left.as.integer - right.as.integer));
                }
                free_value(left); free_value(right);
                break;
            }
            case OP_MUL: {
                Value right = vm_pop(vm);
                Value left = vm_pop(vm);
                if (left.type == VALUE_STRING && right.type == VALUE_INTEGER) {
                    long times = right.as.integer;
                    if (times < 0) times = 0;
                    char *repeated = calloc(1, strlen(left.as.string) * (size_t)times + 1);
                    for (long i = 0; i < times; i++) strcat(repeated, left.as.string);
                    free_value(left); free_value(right);
                    vm_push(vm, make_string_owned(repeated));
                } else if (left.type == VALUE_FLOAT || right.type == VALUE_FLOAT) {
                    vm_push(vm, make_float(value_to_double(left) * value_to_double(right)));
                    free_value(left); free_value(right);
                } else {
                    vm_push(vm, make_integer(left.as.integer * right.as.integer));
                    free_value(left); free_value(right);
                }
                break;
            }
            case OP_DIV: {
                Value right = vm_pop(vm);
                Value left = vm_pop(vm);
                if (left.type == VALUE_FLOAT || right.type == VALUE_FLOAT) {
                    vm_push(vm, make_float(value_to_double(left) / value_to_double(right)));
                } else {
                    if (right.as.integer == 0) {
                        free_value(left); free_value(right);
                        if (vm_raise(vm, "cannot divide by zero")) { continue; }
                        return 1;
                    }
                    vm_push(vm, make_integer(left.as.integer / right.as.integer));
                }
                free_value(left); free_value(right);
                break;
            }
            case OP_MOD: {
                Value right = vm_pop(vm);
                Value left = vm_pop(vm);
                if (right.as.integer == 0) {
                    free_value(left); free_value(right);
                    if (vm_raise(vm, "cannot divide by zero")) { continue; }
                    return 1;
                }
                vm_push(vm, make_integer(left.as.integer % right.as.integer));
                free_value(left); free_value(right);
                break;
            }
            case OP_NEG: {
                Value val = vm_pop(vm);
                if (val.type == VALUE_FLOAT) vm_push(vm, make_float(-val.as.floating));
                else vm_push(vm, make_integer(-val.as.integer));
                free_value(val);
                break;
            }
            case OP_EQUAL: { Value r = vm_pop(vm), l = vm_pop(vm); vm_push(vm, make_bool(values_equal(l, r))); free_value(l); free_value(r); break; }
            case OP_NOT_EQUAL: { Value r = vm_pop(vm), l = vm_pop(vm); vm_push(vm, make_bool(!values_equal(l, r))); free_value(l); free_value(r); break; }
            case OP_LESS: { Value r = vm_pop(vm), l = vm_pop(vm); vm_push(vm, make_bool(value_compare(l, r) < 0)); free_value(l); free_value(r); break; }
            case OP_GREATER: { Value r = vm_pop(vm), l = vm_pop(vm); vm_push(vm, make_bool(value_compare(l, r) > 0)); free_value(l); free_value(r); break; }
            case OP_LESS_EQUAL: { Value r = vm_pop(vm), l = vm_pop(vm); vm_push(vm, make_bool(value_compare(l, r) <= 0)); free_value(l); free_value(r); break; }
            case OP_GREATER_EQUAL: { Value r = vm_pop(vm), l = vm_pop(vm); vm_push(vm, make_bool(value_compare(l, r) >= 0)); free_value(l); free_value(r); break; }
            case OP_AND: { Value r = vm_pop(vm), l = vm_pop(vm); vm_push(vm, make_bool(value_truthy(l) && value_truthy(r))); free_value(l); free_value(r); break; }
            case OP_OR: { Value r = vm_pop(vm), l = vm_pop(vm); vm_push(vm, make_bool(value_truthy(l) || value_truthy(r))); free_value(l); free_value(r); break; }
            case OP_NOT: { Value v = vm_pop(vm); vm_push(vm, make_bool(!value_truthy(v))); free_value(v); break; }
            case OP_PRINT: { Value v = vm_pop(vm); print_value(v); printf("\n"); free_value(v); break; }
            case OP_JUMP: { vm->ip = instr->arg_i; break; }
            case OP_JUMP_IF_FALSE: { Value c = vm_pop(vm); if (!value_truthy(c)) vm->ip = instr->arg_i; free_value(c); break; }
            case OP_BUILD_LIST: {
                int n = instr->arg_i;
                Value v;
                v.type = VALUE_LIST;
                v.as.list.count = 0;
                v.as.list.capacity = 0;
                v.as.list.items = NULL;
                for (int i = 0; i < n; i++) {
                    list_push(&v.as.list, clone_value(vm->stack[vm->stack_top - n + i]));
                }
                vm->stack_top -= n;
                vm_push(vm, v);
                break;
            }
            case OP_BUILD_MAP: { int n = instr->arg_i; Value m = make_dict_empty(); for (int i = 0; i < n; i += 2) { dict_set(&m.as.dict, vm->stack[vm->stack_top-n+i].as.string, clone_value(vm->stack[vm->stack_top-n+i+1])); } vm->stack_top -= n; vm_push(vm, m); break; }
            case OP_LOAD_INDEX: {
                Value idx = vm_pop(vm);
                Value obj = vm_pop(vm);
                if (obj.type == VALUE_LIST) {
                    long i = idx.as.integer;
                    // Convert 1-based to 0-based (match interpreter semantics)
                    if (i > 0) i = i - 1;
                    else if (i < 0) i = (long)obj.as.list.count + i;
                    if (i >= 0 && (size_t)i < obj.as.list.count)
                        vm_push(vm, clone_value(obj.as.list.items[i]));
                    else {
                        free_value(obj); free_value(idx);
                        if (vm_raise(vm, "list index is out of range")) { continue; }
                        return 1;
                    }
                } else if (obj.type == VALUE_STRING) {
                    long i = idx.as.integer;
                    // Convert 1-based to 0-based for strings too
                    if (i > 0) i = i - 1;
                    else if (i < 0) i = (long)strlen(obj.as.string) + i;
                    if (i >= 0 && (size_t)i < strlen(obj.as.string)) {
                        char letter[2] = {obj.as.string[i], '\0'};
                        vm_push(vm, make_string_owned(qs_strdup(letter)));
                    } else {
                        vm_push(vm, make_null());
                    }
                } else if (obj.type == VALUE_DICT) {
                    char *key = value_to_string(idx);
                    int di = dict_find(&obj.as.dict, key);
                    if (di >= 0) vm_push(vm, clone_value(obj.as.dict.items[di].value));
                    else vm_push(vm, make_null());
                    free(key);
                }
                free_value(obj);
                free_value(idx);
                break;
            }
            case OP_STORE_INDEX: { Value val = vm_pop(vm), idx = vm_pop(vm), obj = vm_pop(vm); if (obj.type == VALUE_LIST) { long i; normalize_index(idx.as.integer, obj.as.list.count, &i); free_value(obj.as.list.items[i]); obj.as.list.items[i] = val; } vm_push(vm, obj); free_value(idx); break; }
            case OP_CALL_METHOD: {
                int argc = instr->arg_i;
                const char *method = instr->arg.as.string;
                Value *args_arr = NULL;
                if (argc > 0) args_arr = malloc(argc * sizeof(Value));
                for (int i = argc - 1; i >= 0; i--) args_arr[i] = vm_pop(vm);
                Value obj = vm_pop(vm);
                Value result = make_null();

                if (obj.type == VALUE_OBJECT) {
                    if (argc == 0) {
                        // Field access or zero-arg method call
                        for (size_t fi = 0; fi < obj.as.object.data->fields.count; fi++) {
                            if (strcmp(obj.as.object.data->fields.items[fi].key, method) == 0) {
                                result = clone_value(obj.as.object.data->fields.items[fi].value);
                                goto method_done;
                            }
                        }
                    }
                    // Method call
                    char method_key[256];
                    snprintf(method_key, sizeof(method_key), "%s.%s", obj.as.object.data->class_name, method);

                    BytecodeProgram *body = compiled_funcs_find(method_key);
                    if (body) {
                        CompiledFunc *cf = NULL;
                        for (int ci = 0; ci < g_compiled_func_count; ci++) {
                            if (strcmp(g_compiled_funcs[ci].name, method_key) == 0) {
                                cf = &g_compiled_funcs[ci];
                                break;
                            }
                        }

                        int saved_return = g_has_return;
                        Value saved_retval = g_return_value;
                        g_has_return = 0;

                        if (frame_push(vm, body, vm->env)) {
                            CallFrame *frame = &vm->frames[vm->frame_count - 1];
                            int total_args = argc + 1;
                            frame->local_count = cf ? cf->slot_count : total_args;
                            frame->local_slots[0] = obj; // share identity with frame
                            for (int ai = 0; ai < argc && (ai + 1) < frame->local_count; ai++) {
                                frame->local_slots[ai + 1] = clone_value(args_arr[ai]);
                            }

                            vm_execute(vm);

                            if (g_has_return) {
                                result = g_return_value;
                            }
                            g_has_return = saved_return;
                            g_return_value = saved_retval;
                            frame_pop(vm);
                        }
                    } else {
                        char err[256];
                        snprintf(err, sizeof(err), "unknown method '%s' on '%s'", method, obj.as.object.data->class_name);
                        vm_raise(vm, err);
                    }
                } else {
                    result = dispatch_method(obj, method, args_arr, argc);
                }

                method_done:
                for (int i = 0; i < argc; i++) free_value(args_arr[i]);
                free(args_arr);
                vm_push(vm, result);
                break;
            }
            case OP_CALL_FUNC: {
                int argc = instr->arg_i;
                const char *func_name = instr->arg.as.string;
                Value *args_arr = NULL;
                if (argc > 0) args_arr = malloc(argc * sizeof(Value));
                for (int i = argc - 1; i >= 0; i--) args_arr[i] = vm_pop(vm);
                Value result = make_null();

                int var_idx = find_variable(vm->env, func_name);
                if (var_idx >= 0 && vm->env->items[var_idx].value.type == VALUE_LAMBDA) {
                    CallArgList cargs = {0};
                    for (int i = 0; i < argc; i++) {
                        CallArg ca = {0}; ca.value = args_arr[i];
                        cargs.items = realloc(cargs.items, (cargs.count + 1) * sizeof(CallArg));
                        cargs.items[cargs.count++] = ca;
                    }
                    Parser dummy = {"", 0, 0, vm->env, &(ParseError){0}, -1};
                    call_lambda(&dummy, &vm->env->items[var_idx].value, &cargs, &result);
                    free(cargs.items);
                } else {
                    // Check for tree-walk function (from imports)
                    Function *tw_func = find_function(func_name);
                    if (tw_func) {
                        CallArgList cargs = {0};
                        cargs.count = argc;
                        cargs.items = argc > 0 ? calloc(argc, sizeof(CallArg)) : NULL;
                        for (int i = 0; i < argc; i++) {
                            cargs.items[i].value = clone_value(args_arr[i]);
                            cargs.items[i].name = NULL;
                            cargs.items[i].is_keyword = 0;
                        }
                        Parser min_parser = {0};
                        min_parser.text = "";
                        min_parser.pos = 0;
                        min_parser.line = instr->line;
                        min_parser.env = vm->env;
                        min_parser.var_index = -1;
                        ParseError ferr = {0};
                        min_parser.err = &ferr;
                        call_user_function(&min_parser, tw_func, &cargs, &result);
                        free(cargs.items);
                    } else {
                        CallArgList cargs = {0};
                        for (int i = 0; i < argc; i++) {
                            CallArg ca = {0}; ca.value = args_arr[i];
                            cargs.items = realloc(cargs.items, (cargs.count + 1) * sizeof(CallArg));
                            cargs.items[cargs.count++] = ca;
                        }
                        Parser dummy = {func_name, 0, 0, vm->env, &(ParseError){0}, -1};
                        call_builtin(&dummy, func_name, &cargs, &result);
                        free(cargs.items);
                    }
                }
                for (int i = 0; i < argc; i++) free_value(args_arr[i]);
                free(args_arr);
                vm_push(vm, result);
                break;
            }
            case OP_BUILD_STRING: {
                int n = instr->arg_i;
                size_t total = 1;
                for (int i = 0; i < n; i++) {
                    char *s = value_to_string(vm->stack[vm->stack_top - n + i]);
                    total += strlen(s); free(s);
                }
                char *buf = malloc(total); buf[0] = '\0';
                for (int i = 0; i < n; i++) {
                    char *s = value_to_string(vm->stack[vm->stack_top - n + i]);
                    strcat(buf, s); free(s);
                }
                for (int i = 0; i < n; i++) { Value v = vm_pop(vm); free_value(v); }
                vm_push(vm, make_string_owned(buf));
                break;
            }
            case OP_PRINT_MULTI: {
                int n = instr->arg_i;
                // Values are on stack in order [arg0, arg1, ...] (arg0 at bottom)
                // Pop all into array, then print in correct order
                Value *vals = malloc(n * sizeof(Value));
                for (int i = n - 1; i >= 0; i--) {
                    vals[i] = vm_pop(vm);
                }
                for (int i = 0; i < n; i++) {
                    if (i > 0) printf(" ");
                    print_value(vals[i]);
                    free_value(vals[i]);
                }
                printf("\n");
                free(vals);
                break;
            }
            case OP_EVAL_STMT: {
                const char *text = vm->program->constants[instr->arg_i].as.string;
                Program sub_prog = {0};
                if (load_source(text, &sub_prog)) {
                    Program *saved_g_program = g_program;
                    g_program = &sub_prog;
                    strip_block_comments(&sub_prog);
                    ParseError ferr = {0};
                    collect_imports(&sub_prog, vm->env, &ferr);
                    collect_functions(&sub_prog, &ferr);
                    ParseError err = {0};
                    execute_range(&sub_prog, vm->env, 0, sub_prog.count, &err);
                    if (err.message[0]) {
                        fprintf(stderr, "Error at line %d: %s\n", err.line, err.message);
                    }
                    free_program(&sub_prog);
                    g_program = saved_g_program;
                }
                break;
            }
            case OP_POP: { Value v = vm_pop(vm); free_value(v); break; }
            case OP_DUP: { vm_push(vm, clone_value(vm_peek(vm))); break; }
            case OP_CALL: break;
            case OP_RETURN: {
                // Pop return value from stack and set g_return_value
                if (vm->stack_top > 0) {
                    g_return_value = vm_pop(vm);
                    g_has_return = 1;
                }
                return 0;
            }
            case OP_SLICE: {
                // Stack: obj, start, end (3 values for argc=2)
                Value end = vm_pop(vm);
                Value start = vm_pop(vm);
                Value obj = vm_pop(vm);
                if (obj.type == VALUE_STRING) {
                    long s = 0, e = (long)strlen(obj.as.string);
                    if (start.type == VALUE_INTEGER) s = start.as.integer;
                    if (end.type == VALUE_INTEGER) e = end.as.integer;
                    if (s < 0) s = (long)strlen(obj.as.string) + s;
                    if (e < 0) e = (long)strlen(obj.as.string) + e;
                    if (s < 0) s = 0;
                    if (e > (long)strlen(obj.as.string)) e = (long)strlen(obj.as.string);
                    if (e > s) {
                        char *sliced = qs_strndup(obj.as.string + s, e - s);
                        vm_push(vm, make_string_owned(sliced));
                    } else {
                        vm_push(vm, make_string_owned(qs_strdup("")));
                    }
                } else if (obj.type == VALUE_LIST) {
                    long s = 0, e = (long)obj.as.list.count;
                    if (start.type == VALUE_INTEGER) s = start.as.integer;
                    if (end.type == VALUE_INTEGER) e = end.as.integer;
                    if (s < 0) s = (long)obj.as.list.count + s;
                    if (e < 0) e = (long)obj.as.list.count + e;
                    if (s < 0) s = 0;
                    if (e > (long)obj.as.list.count) e = (long)obj.as.list.count;
                    ValueList list = {0};
                    for (long i = s; i < e; i++) {
                        list_push(&list, clone_value(obj.as.list.items[i]));
                    }
                    Value result;
                    result.type = VALUE_LIST;
                    result.as.list = list;
                    vm_push(vm, result);
                } else {
                    vm_push(vm, make_null());
                }
                free_value(obj); free_value(start); free_value(end);
                break;
            }
            case OP_JUMP_IF_TRUE: {
                Value c = vm_pop(vm);
                if (value_truthy(c)) vm->ip = instr->arg_i;
                free_value(c);
                break;
            }
            case OP_TRY_PUSH: {
                if (vm->try_stack_top < 64) {
                    vm->try_stack[vm->try_stack_top] = instr->arg_i;
                    vm->try_stack_depths[vm->try_stack_top] = vm->stack_top;
                    vm->try_stack_top++;
                }
                break;
            }
            case OP_CATCH_END: {
                if (vm->try_stack_top > 0) vm->try_stack_top--;
                break;
            }
            case OP_CALL_USER_FUNC: {
                AUDIT_USER_FUNC();
                int argc = instr->arg_i;
                const char *func_name = instr->arg.as.string;
                // Pop arguments directly into frame's inline slots (NO HEAP)
                Value *args_arr = NULL;
                if (argc > 0) args_arr = malloc(argc * sizeof(Value));
                for (int i = argc - 1; i >= 0; i--) args_arr[i] = vm_pop(vm);

                BytecodeProgram *body = compiled_funcs_find(func_name);
                Value result = make_null();
                if (body) {
                    CompiledFunc *cf = NULL;
                    for (int i = 0; i < g_compiled_func_count; i++) {
                        if (strcmp(g_compiled_funcs[i].name, func_name) == 0) {
                            cf = &g_compiled_funcs[i];
                            break;
                        }
                    }
                    if (cf) {
                        if (argc != cf->param_count) {
                            char msg[256];
                            if (argc > cf->param_count) {
                                snprintf(msg, sizeof(msg), "too many arguments");
                            } else {
                                snprintf(msg, sizeof(msg), "missing argument '%s'", cf->param_names[argc]);
                            }
                            for (int i = 0; i < argc; i++) free_value(args_arr[i]);
                            free(args_arr);
                            if (vm_raise(vm, msg)) { continue; }
                            return 1;
                        }
                    }

                    int saved_return = g_has_return;
                    Value saved_retval = g_return_value;
                    g_has_return = 0;

                    // Push frame with INLINE slot storage (NO HEAP ALLOCATION)
                    // Pass current env for global variable access
                    if (!frame_push(vm, body, vm->env)) {
                        for (int i = 0; i < argc; i++) free_value(args_arr[i]);
                        free(args_arr);
                        return 1;
                    }

                    // Store parameters directly in frame's inline slots
                    CallFrame *frame = &vm->frames[vm->frame_count - 1];
                    frame->local_count = cf ? cf->slot_count : argc;
                    for (int i = 0; i < argc && i < frame->local_count; i++) {
                        frame->local_slots[i] = clone_value(args_arr[i]);
                    }

                    vm_execute(vm);

                    if (g_has_return) {
                        result = g_return_value;
                    }
                    g_has_return = saved_return;
                    g_return_value = saved_retval;

                    frame_pop(vm);
                }
                // Fallback: if not compiled, try tree-walk function (from imports)
                if (!body) {
                    Function *func = find_function(func_name);
                    if (func) {
                        CallArgList call_args = {0};
                        call_args.count = argc;
                        call_args.items = argc > 0 ? calloc(argc, sizeof(CallArg)) : NULL;
                        for (int i = 0; i < argc; i++) {
                            call_args.items[i].value = clone_value(args_arr[i]);
                            call_args.items[i].name = NULL;
                            call_args.items[i].is_keyword = 0;
                        }
                        Parser min_parser = {0};
                        min_parser.text = "";
                        min_parser.pos = 0;
                        min_parser.line = instr->line;
                        min_parser.env = vm->env;
                        min_parser.var_index = -1;
                        ParseError ferr = {0};
                        min_parser.err = &ferr;
                        Value tw_result = make_null();
                        call_user_function(&min_parser, func, &call_args, &tw_result);
                        free_value(result);
                        result = tw_result;
                        free_call_args(&call_args);
                    }
                }
                for (int i = 0; i < argc; i++) free_value(args_arr[i]);
                free(args_arr);
                vm_push(vm, result);
                break;
            }
            case OP_HALT: return 0;

            case OP_NEW: {
                // Create new object from class
                const char *class_name = instr->arg.as.string;
                int argc = instr->arg_i;

                // Create object
                Value obj = make_object(class_name);

                // Pop arguments (in reverse order)
                Value *args_arr = NULL;
                if (argc > 0) args_arr = malloc(argc * sizeof(Value));
                for (int i = argc - 1; i >= 0; i--) args_arr[i] = vm_pop(vm);

                // Call init(self, args...) if it exists via compiled bytecode
                char init_key[256];
                snprintf(init_key, sizeof(init_key), "%s.init", class_name);
                BytecodeProgram *init_body = compiled_funcs_find(init_key);
                if (init_body) {
                    CompiledFunc *cf = NULL;
                    for (int i = 0; i < g_compiled_func_count; i++) {
                        if (strcmp(g_compiled_funcs[i].name, init_key) == 0) {
                            cf = &g_compiled_funcs[i];
                            break;
                        }
                    }

                    int saved_return = g_has_return;
                    Value saved_retval = g_return_value;
                    g_has_return = 0;

                    if (frame_push(vm, init_body, vm->env)) {
                        CallFrame *frame = &vm->frames[vm->frame_count - 1];
                        int total_args = argc + 1; // self + user args
                        frame->local_count = cf ? cf->slot_count : total_args;
                        frame->local_slots[0] = obj; // pass obj directly, not clone
                        for (int i = 0; i < argc && (i + 1) < frame->local_count; i++) {
                            frame->local_slots[i + 1] = clone_value(args_arr[i]);
                        }

                        int stack_before = vm->stack_top;
                        vm_execute(vm);

                        // After init, the modified self should be on the stack
                        // from the last SET_FIELD. Pop and use it.
                        if (vm->stack_top > stack_before) {
                            obj = vm_pop(vm);
                        }

                        frame_pop(vm);
                    }

                    g_has_return = saved_return;
                    g_return_value = saved_retval;
                }

                for (int i = 0; i < argc; i++) free_value(args_arr[i]);
                free(args_arr);
                vm_push(vm, obj);
                break;
            }

            case OP_GET_FIELD: {
                // Get field: obj "field" -> value
                const char *field = instr->arg.as.string;
                Value obj = vm_pop(vm);
                Value result = make_null();

                if (obj.type == VALUE_OBJECT) {
                    // Search fields
                    for (size_t i = 0; i < obj.as.object.data->fields.count; i++) {
                        if (strcmp(obj.as.object.data->fields.items[i].key, field) == 0) {
                            result = clone_value(obj.as.object.data->fields.items[i].value);
                            break;
                        }
                    }
                    if (result.type == VALUE_NULL) {
                        // Not a field — might be a method: look up class method
                        char method_key[256];
                        snprintf(method_key, sizeof(method_key), "%s.%s", obj.as.object.data->class_name, field);
                        int idx = find_variable(vm->env, method_key);
                        if (idx >= 0) {
                            result = clone_value(vm->env->items[idx].value);
                        }
                    }
                } else {
                    char err[256];
                    snprintf(err, sizeof(err), "cannot access field '%s' on non-object", field);
                    vm_raise(vm, err);
                }

                free_value(obj);
                vm_push(vm, result);
                break;
            }

            case OP_SET_FIELD: {
                // Set field: obj value -> obj
                const char *field = instr->arg.as.string;
                Value value = vm_pop(vm);
                Value obj = vm_pop(vm);

                if (obj.type == VALUE_OBJECT) {
                    ObjectData *data = obj.as.object.data;
                    // Find existing field or add new one
                    // Find existing field or add new one
                    int found = 0;
                    for (size_t i = 0; i < data->fields.count; i++) {
                        if (strcmp(data->fields.items[i].key, field) == 0) {
                            free_value(data->fields.items[i].value);
                            data->fields.items[i].value = value;
                            found = 1;
                            break;
                        }
                    }
                    if (!found) {
                        // Grow fields array
                        if (data->fields.count >= data->fields.capacity) {
                            size_t new_cap = data->fields.capacity ? data->fields.capacity * 2 : 8;
                            data->fields.items = realloc(data->fields.items, new_cap * sizeof(DictEntry));
                            data->fields.capacity = new_cap;
                        }
                        data->fields.items[data->fields.count].key = qs_strdup(field);
                        data->fields.items[data->fields.count].value = value;
                        data->fields.count++;
                    }
                    vm_push(vm, obj);
                } else {
                    free_value(value);
                    char err[256];
                    snprintf(err, sizeof(err), "cannot set field '%s' on non-object", field);
                    vm_raise(vm, err);
                    vm_push(vm, obj);
                }
                break;
            }
        }
    }
    return 0;
}
