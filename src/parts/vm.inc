static int g_vm_trace = 0;
static char *g_vm_source_dir = NULL;

static int vm_raise(VM *vm, const char *msg) {
    if (vm->try_stack_top > 0 && vm->try_stack_prog[vm->try_stack_top - 1] == vm->program) {
        int handler = vm->try_stack[vm->try_stack_top - 1];
        int saved_depth = vm->try_stack_depths[vm->try_stack_top - 1];
        vm->try_stack_top--;
        vm->stack_top = saved_depth;
        vm_push(vm, make_string_owned(qs_strdup(msg)));
        vm->ip = handler;
        return 1;
    }
    free(vm->exception_msg);
    vm->exception_msg = qs_strdup(msg);
    vm->pending_exception = 1;
    return 0;
}

static int dispatch_method(Value *receiver, const char *method, Value *args, int argc,
                           Environment *env, char *err, size_t errsz) {
    ValueList margs = {0};
    for (int i = 0; i < argc; i++) list_push(&margs, clone_value(args[i]));
    ParseError perr = {0};
    Parser mp = {"", 0, 0, env, &perr, -1};
    int ok = apply_method(&mp, receiver, method, &margs);
    for (size_t i = 0; i < margs.count; i++) free_value(margs.items[i]);
    free(margs.items);
    if (!ok) {
        if (err && errsz) snprintf(err, errsz, "%s", perr.message[0] ? perr.message : "method call failed");
        return 0;
    }
    return 1;
}

static int vm_execute(VM *vm) {
    while (vm->ip < (int)vm->program->count) {
        Instruction *instr = &vm->program->instructions[vm->ip];
        if (g_vm_trace) vm_dump_state(vm, "exec");
        vm->ip++;

        switch (instr->op) {
            case OP_LOAD_CONST: {
                vm_push(vm, clone_value(vm->program->constants[instr->arg_i]));
                break;
            }
            case OP_LOAD_VAR: {
                int idx = find_variable(vm->env, instr->arg.as.string);
                if (idx < 0) {
                    const char *canon = qs_type_alias(instr->arg.as.string);
                    if (canon) {
                        vm_push(vm, make_string_owned(qs_strdup(canon)));
                        break;
                    }
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
                CallFrame *cf = &vm->frames[vm->frame_count - 1];
                vm_push(vm, clone_value(cf->local_slots[instr->arg_i]));
                break;
            }
            case OP_STORE_LOCAL: {
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
                } else if (right.type == VALUE_STRING && left.type == VALUE_INTEGER) {
                    long times = left.as.integer;
                    if (times < 0) times = 0;
                    char *repeated = calloc(1, strlen(right.as.string) * (size_t)times + 1);
                    for (long i = 0; i < times; i++) strcat(repeated, right.as.string);
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
            case OP_THROW: {
                Value msgv = vm_pop(vm);
                char *msg = value_to_string(msgv);
                free_value(msgv);
                int handled = vm_raise(vm, msg);
                free(msg);
                if (!handled) return 1;
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
            case OP_IN: { Value r = vm_pop(vm), l = vm_pop(vm); vm_push(vm, make_bool(value_contains(r, l))); free_value(l); free_value(r); break; }
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
                    if (di >= 0) vm_push(vm, clone_value(obj.as.dict.entries[di].value));
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
                        for (size_t fi = 0; fi < obj.as.object.data->fields.count; fi++) {
                            if (strcmp(obj.as.object.data->fields.entries[fi].key, method) == 0) {
                                result = clone_value(obj.as.object.data->fields.entries[fi].value);
                                goto method_done;
                            }
                        }
                    }

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

                        Function *mf = find_function(method_key);
                        if (mf) {
                            int pi = 0;
                            int mismatch = 0;
                            size_t bad_p = 0;
                            for (size_t p = 1; p < mf->param_count && pi < argc; p++) {
                                Param *pp = &mf->params[p];
                                if (pp->is_args || pp->is_kwargs) continue;
                                if (!type_matches(pp->type, args_arr[pi])) { mismatch = 1; bad_p = p; break; }
                                pi++;
                            }
                            if (mismatch) {
                                char tmsg[256];
                                snprintf(tmsg, sizeof(tmsg), "argument '%s' has the wrong type", mf->params[bad_p].name);
                                for (int i = 0; i < argc; i++) free_value(args_arr[i]);
                                free(args_arr);
                                free_value(obj);
                                g_has_return = saved_return;
                                g_return_value = saved_retval;
                                if (vm_raise(vm, tmsg)) { continue; }
                                return 1;
                            }
                        }

                        if (frame_push(vm, body, vm->env)) {
                            CallFrame *frame = &vm->frames[vm->frame_count - 1];
                            int total_args = argc + 1;
                            frame->local_count = cf ? cf->slot_count : total_args;
                            frame->local_slots[0] = obj;
                            for (int ai = 0; ai < argc && (ai + 1) < frame->local_count; ai++) {
                                frame->local_slots[ai + 1] = clone_value(args_arr[ai]);
                            }

                            vm_execute(vm);

                            if (vm->pending_exception) {
                                g_has_return = saved_return;
                                g_return_value = saved_retval;
                                frame_pop(vm);
                                char *m = qs_strdup(vm->exception_msg ? vm->exception_msg : "exception");
                                vm->pending_exception = 0;
                                free(vm->exception_msg); vm->exception_msg = NULL;
                                for (int i = 0; i < argc; i++) free_value(args_arr[i]);
                                free(args_arr);
                                int handled = vm_raise(vm, m);
                                free(m);
                                if (handled) { continue; }
                                return 1;
                            }

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
                        for (int i = 0; i < argc; i++) free_value(args_arr[i]);
                        free(args_arr);
                        free_value(obj);
                        if (!vm_raise(vm, err)) return 1;
                        break;
                    }
                } else {
                    char merr[256];
                    merr[0] = '\0';
                    int ok = dispatch_method(&obj, method, args_arr, argc, vm->env, merr, sizeof(merr));
                    if (!ok) {
                        for (int i = 0; i < argc; i++) free_value(args_arr[i]);
                        free(args_arr);
                        if (merr[0] == '\0') snprintf(merr, sizeof(merr), "unknown method '%s'", method);
                        if (!vm_raise(vm, merr)) return 1;
                        break;
                    }
                    result = obj;
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
                ParseError ferr = {0};
                int call_ok = 1;

                int var_idx = find_variable(vm->env, func_name);
                if (var_idx >= 0 && vm->env->items[var_idx].value.type == VALUE_LAMBDA) {
                    CallArgList cargs = {0};
                    for (int i = 0; i < argc; i++) {
                        CallArg ca = {0}; ca.value = args_arr[i];
                        cargs.items = realloc(cargs.items, (cargs.count + 1) * sizeof(CallArg));
                        cargs.items[cargs.count++] = ca;
                    }
                    Parser dummy = {"", 0, 0, vm->env, &ferr, -1};
                    call_ok = call_lambda(&dummy, &vm->env->items[var_idx].value, &cargs, &result);
                    free(cargs.items);
                } else {
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
                        min_parser.err = &ferr;
                        call_ok = call_user_function(&min_parser, tw_func, &cargs, &result);
                        free(cargs.items);
                    } else {
                        CallArgList cargs = {0};
                        for (int i = 0; i < argc; i++) {
                            CallArg ca = {0}; ca.value = args_arr[i];
                            cargs.items = realloc(cargs.items, (cargs.count + 1) * sizeof(CallArg));
                            cargs.items[cargs.count++] = ca;
                        }
                        Parser dummy = {func_name, 0, 0, vm->env, &ferr, -1};
                        call_ok = call_builtin(&dummy, func_name, &cargs, &result);
                        free(cargs.items);
                    }
                }
                for (int i = 0; i < argc; i++) free_value(args_arr[i]);
                free(args_arr);
                if (!call_ok) {
                    const char *m = ferr.message[0] ? ferr.message : "call failed";
                    if (vm_raise(vm, m)) { continue; }
                    return 1;
                }
                vm_push(vm, result);
                break;
            }
            case OP_CALL_KW: {
                int argc = instr->arg_i;
                const char *func_name = instr->arg.as.string;
                CallArgList cargs = {0};
                cargs.count = argc;
                cargs.items = argc > 0 ? calloc(argc, sizeof(CallArg)) : NULL;
                for (int i = argc - 1; i >= 0; i--) {
                    Value val = vm_pop(vm);
                    Value nm = vm_pop(vm);
                    cargs.items[i].value = val;
                    cargs.items[i].name = (nm.type == VALUE_STRING && nm.as.string) ? qs_strdup(nm.as.string) : NULL;
                    cargs.items[i].is_keyword = (nm.type == VALUE_STRING) ? 1 : 0;
                    free_value(nm);
                }
                Value result = make_null();
                ParseError ferr = {0};
                int call_ok = 1;
                int var_idx = find_variable(vm->env, func_name);
                if (var_idx >= 0 && vm->env->items[var_idx].value.type == VALUE_LAMBDA) {
                    Parser dummy = {"", 0, 0, vm->env, &ferr, -1};
                    call_ok = call_lambda(&dummy, &vm->env->items[var_idx].value, &cargs, &result);
                } else {
                    Function *tw_func = find_function(func_name);
                    if (tw_func) {
                        Parser min_parser = {0};
                        min_parser.text = "";
                        min_parser.pos = 0;
                        min_parser.line = instr->line;
                        min_parser.env = vm->env;
                        min_parser.var_index = -1;
                        min_parser.err = &ferr;
                        call_ok = call_user_function(&min_parser, tw_func, &cargs, &result);
                    } else {
                        Parser dummy = {func_name, 0, 0, vm->env, &ferr, -1};
                        call_ok = call_builtin(&dummy, func_name, &cargs, &result);
                    }
                }
                free_call_args(&cargs);
                if (!call_ok) {
                    const char *m = ferr.message[0] ? ferr.message : "call failed";
                    if (vm_raise(vm, m)) { continue; }
                    return 1;
                }
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
                    if (g_vm_source_dir) {
                        free(sub_prog.dir);
                        sub_prog.dir = qs_strdup(g_vm_source_dir);
                    }
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
                if (vm->stack_top > 0) {
                    g_return_value = vm_pop(vm);
                    g_has_return = 1;
                }
                return 0;
            }
            case OP_SLICE: {
                Value end = vm_pop(vm);
                Value start = vm_pop(vm);
                Value obj = vm_pop(vm);
                if (obj.type == VALUE_STRING) {
                    size_t len = obj.as.string ? strlen(obj.as.string) : 0;
                    long raw_s = (start.type == VALUE_INTEGER) ? start.as.integer : 0;
                    long raw_e = (end.type == VALUE_INTEGER) ? end.as.integer : 0;
                    long s = normalize_slice_edge(raw_s, len, 1);
                    long e = normalize_slice_edge(raw_e, len, 0);
                    if (e < s) e = s;
                    vm_push(vm, make_string_owned(qs_strndup(obj.as.string + s, (size_t)(e - s))));
                } else if (obj.type == VALUE_LIST) {
                    size_t len = obj.as.list.count;
                    long raw_s = (start.type == VALUE_INTEGER) ? start.as.integer : 0;
                    long raw_e = (end.type == VALUE_INTEGER) ? end.as.integer : 0;
                    long s = normalize_slice_edge(raw_s, len, 1);
                    long e = normalize_slice_edge(raw_e, len, 0);
                    if (e < s) e = s;
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
                    vm->try_stack_prog[vm->try_stack_top] = vm->program;
                    vm->try_stack_top++;
                }
                break;
            }
            case OP_CATCH_END: {
                if (vm->try_stack_top > 0) vm->try_stack_top--;
                break;
            }
            case OP_CALL_USER_FUNC: {
                int argc = instr->arg_i;
                const char *func_name = instr->arg.as.string;

                Value *args_arr = NULL;
                if (argc > 0) args_arr = malloc(argc * sizeof(Value));
                for (int i = argc - 1; i >= 0; i--) args_arr[i] = vm_pop(vm);

                BytecodeProgram *body = compiled_funcs_find(func_name);
                Value result = make_null();
                int delegate_tw = 0;
                if (body) {
                    CompiledFunc *probe = NULL;
                    for (int i = 0; i < g_compiled_func_count; i++) {
                        if (strcmp(g_compiled_funcs[i].name, func_name) == 0) { probe = &g_compiled_funcs[i]; break; }
                    }

                    if (probe && argc != probe->param_count && find_function(func_name))
                        delegate_tw = 1;
                }
                if (body && !delegate_tw) {
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

                        Function *tf = find_function(func_name);
                        if (tf) {
                            int pi = 0;
                            int mismatch = 0;
                            size_t bad_p = 0;
                            for (size_t p = 0; p < tf->param_count && pi < argc; p++) {
                                Param *pp = &tf->params[p];
                                if (pp->is_args || pp->is_kwargs) continue;
                                if (!type_matches(pp->type, args_arr[pi])) {
                                    mismatch = 1;
                                    bad_p = p;
                                    break;
                                }
                                pi++;
                            }
                            if (mismatch) {
                                char msg[256];
                                snprintf(msg, sizeof(msg), "argument '%s' has the wrong type", tf->params[bad_p].name);
                                for (int i = 0; i < argc; i++) free_value(args_arr[i]);
                                free(args_arr);
                                if (vm_raise(vm, msg)) { continue; }
                                return 1;
                            }
                        }
                    }

                    int saved_return = g_has_return;
                    Value saved_retval = g_return_value;
                    g_has_return = 0;

                    if (!frame_push(vm, body, vm->env)) {
                        for (int i = 0; i < argc; i++) free_value(args_arr[i]);
                        free(args_arr);
                        return 1;
                    }

                    CallFrame *frame = &vm->frames[vm->frame_count - 1];
                    frame->local_count = cf ? cf->slot_count : argc;
                    for (int i = 0; i < argc && i < frame->local_count; i++) {
                        frame->local_slots[i] = clone_value(args_arr[i]);
                    }

                    vm_execute(vm);

                    if (vm->pending_exception) {
                        g_has_return = saved_return;
                        g_return_value = saved_retval;
                        frame_pop(vm);
                        char *m = qs_strdup(vm->exception_msg ? vm->exception_msg : "exception");
                        vm->pending_exception = 0;
                        free(vm->exception_msg); vm->exception_msg = NULL;
                        for (int i = 0; i < argc; i++) free_value(args_arr[i]);
                        free(args_arr);
                        int handled = vm_raise(vm, m);
                        free(m);
                        if (handled) { continue; }
                        return 1;
                    }

                    if (g_has_return) {
                        result = g_return_value;
                    }
                    g_has_return = saved_return;
                    g_return_value = saved_retval;

                    frame_pop(vm);
                }

                if (!body || delegate_tw) {
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
                const char *class_name = instr->arg.as.string;
                int argc = instr->arg_i;

                Value obj = make_object(class_name);

                Value *args_arr = NULL;
                if (argc > 0) args_arr = malloc(argc * sizeof(Value));
                for (int i = argc - 1; i >= 0; i--) args_arr[i] = vm_pop(vm);

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
                        int total_args = argc + 1;
                        frame->local_count = cf ? cf->slot_count : total_args;
                        frame->local_slots[0] = obj;
                        for (int i = 0; i < argc && (i + 1) < frame->local_count; i++) {
                            frame->local_slots[i + 1] = clone_value(args_arr[i]);
                        }

                        int stack_before = vm->stack_top;
                        vm_execute(vm);

                        if (vm->pending_exception) {
                            frame_pop(vm);
                            g_has_return = saved_return;
                            g_return_value = saved_retval;
                            char *m = qs_strdup(vm->exception_msg ? vm->exception_msg : "exception");
                            vm->pending_exception = 0;
                            free(vm->exception_msg); vm->exception_msg = NULL;
                            for (int i = 0; i < argc; i++) free_value(args_arr[i]);
                            free(args_arr);
                            free_value(obj);
                            int handled = vm_raise(vm, m);
                            free(m);
                            if (handled) { continue; }
                            return 1;
                        }

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
                const char *field = instr->arg.as.string;
                Value obj = vm_pop(vm);
                Value result = make_null();

                if (obj.type == VALUE_OBJECT) {
                    for (size_t i = 0; i < obj.as.object.data->fields.count; i++) {
                        if (strcmp(obj.as.object.data->fields.entries[i].key, field) == 0) {
                            result = clone_value(obj.as.object.data->fields.entries[i].value);
                            break;
                        }
                    }
                    if (result.type == VALUE_NULL) {
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
                const char *field = instr->arg.as.string;
                Value value = vm_pop(vm);
                Value obj = vm_pop(vm);

                if (obj.type == VALUE_OBJECT) {
                    ObjectData *data = obj.as.object.data;
                    dict_set(&data->fields, field, value);
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
