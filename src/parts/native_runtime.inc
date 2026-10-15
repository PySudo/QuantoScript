#include <setjmp.h>

#if defined(__GNUC__)
#define QS_NOINLINE __attribute__((noinline))
#else
#define QS_NOINLINE
#endif

#if defined(__GNUC__) || defined(__clang__)
typedef struct {
    void *jb[5];
} QsTryFrame;
#define QS_TRY_SET(f) (__builtin_setjmp((f)->jb))
#define QS_TRY_LONGJMP(f) (__builtin_longjmp((f)->jb, 1))
#else
typedef struct {
    jmp_buf jb;
} QsTryFrame;
#define QS_TRY_SET(f) (setjmp((f)->jb))
#define QS_TRY_LONGJMP(f) (longjmp((f)->jb, 1))
#endif

#define QS_MAX_TRY 256
static QsTryFrame *g_qs_tries[QS_MAX_TRY];
static int g_qs_try_top = 0;
static char g_qs_error[2048] = {0};

static QS_UNUSED_FN QS_NOINLINE void qs_push_try(QsTryFrame *frame) {
    if (g_qs_try_top < QS_MAX_TRY) {
        g_qs_tries[g_qs_try_top++] = frame;
    }
}

static QS_UNUSED_FN QS_NOINLINE void qs_pop_try(void) {
    if (g_qs_try_top > 0) g_qs_try_top--;
}

static QS_UNUSED_FN QS_NOINLINE void qs_raise(const char *msg) {
    snprintf(g_qs_error, sizeof(g_qs_error), "%s", msg ? msg : "error");
    if (g_qs_try_top > 0) {
        QsTryFrame *frame = g_qs_tries[--g_qs_try_top];
        QS_TRY_LONGJMP(frame);
    }
    fflush(stdout);
    fprintf(stderr, "Error: %s\n", g_qs_error);
    exit(1);
}

static QS_UNUSED_FN QS_NOINLINE void qs_raise_value(Value message) {
    char *text = value_to_string(message);
    qs_raise(text);
    free(text);
}

static QS_UNUSED_FN void qs_check_arg(Value value, const char *type, const char *name) {
    if (type_matches(type, value)) return;
    char msg[256];
    snprintf(msg, sizeof(msg), "argument '%s' has the wrong type", name);
    qs_raise(msg);
}

typedef Value (*QsFnPtr)(Environment *env, Value *args, int argc);

typedef struct {
    const char *name;
    QsFnPtr fn;
    int param_count;
} QsFnEntry;

#define QS_MAX_FUNCS 2048
static QsFnEntry g_qs_funcs[QS_MAX_FUNCS];
static int g_qs_func_count = 0;

static QS_UNUSED_FN void qs_register(const char *name, QsFnPtr fn, int param_count) {
    if (g_qs_func_count < QS_MAX_FUNCS) {
        g_qs_funcs[g_qs_func_count].name = name;
        g_qs_funcs[g_qs_func_count].fn = fn;
        g_qs_funcs[g_qs_func_count].param_count = param_count;
        g_qs_func_count++;
    }
}

static QS_UNUSED_FN QsFnEntry *qs_lookup_entry(const char *name) {
    for (int i = 0; i < g_qs_func_count; i++) {
        if (strcmp(g_qs_funcs[i].name, name) == 0) return &g_qs_funcs[i];
    }
    return NULL;
}

static QS_UNUSED_FN QsFnPtr qs_lookup(const char *name) {
    QsFnEntry *entry = qs_lookup_entry(name);
    return entry ? entry->fn : NULL;
}

static QS_UNUSED_FN Value qs_get(Environment *env, Environment *local, const char *name) {
    int idx = -1;
    if (local) idx = find_variable(local, name);
    if (idx >= 0) return clone_value(local->items[idx].value);
    idx = find_variable(env, name);
    if (idx >= 0) return clone_value(env->items[idx].value);
    const char *canon = qs_type_alias(name);
    if (canon) return make_string_owned(qs_strdup(canon));
    char msg[256];
    snprintf(msg, sizeof(msg), "unknown variable '%s'", name);
    qs_raise(msg);
    return make_null();
}

static QS_UNUSED_FN void qs_set(Environment *env, Environment *local, const char *name, Value value) {
    Environment *target = (local && find_variable(local, name) >= 0) ? local : env;
    if (is_define_name(name) && find_variable(target, name) >= 0) {
        free_value(value);
        char msg[256];
        snprintf(msg, sizeof(msg), "definition '%s' already exists and cannot be changed", name);
        qs_raise(msg);
        return;
    }
    env_set(target, name, value);
}

static QS_UNUSED_FN int qs_truthy(Value value) {
    int result = value_truthy(value);
    free_value(value);
    return result;
}

static QS_UNUSED_FN void qs_print_value(Value value) {
    print_value(value);
    printf("\n");
    free_value(value);
}

static QS_UNUSED_FN void qs_print_multi(int count, Value *values) {
    for (int i = 0; i < count; i++) {
        if (i > 0) printf(" ");
        print_value(values[i]);
        free_value(values[i]);
    }
    printf("\n");
}

static QS_UNUSED_FN Value qs_concat_strings(int count, Value *parts) {
    size_t total = 1;
    for (int i = 0; i < count; i++) {
        char *s = value_to_string(parts[i]);
        total += strlen(s);
        free(s);
    }
    char *buffer = malloc(total);
    buffer[0] = '\0';
    for (int i = 0; i < count; i++) {
        char *s = value_to_string(parts[i]);
        strcat(buffer, s);
        free(s);
        free_value(parts[i]);
    }
    return make_string_owned(buffer);
}

static QS_UNUSED_FN Value qs_add(Value left, Value right) {
    if (left.type == VALUE_STRING || right.type == VALUE_STRING) {
        char *lt = value_to_string(left);
        char *rt = value_to_string(right);
        size_t len = strlen(lt) + strlen(rt);
        char *joined = malloc(len + 1);
        strcpy(joined, lt);
        strcat(joined, rt);
        free(lt);
        free(rt);
        free_value(left);
        free_value(right);
        return make_string_owned(joined);
    }
    if (left.type == VALUE_FLOAT || right.type == VALUE_FLOAT) {
        Value result = make_float(value_to_double(left) + value_to_double(right));
        free_value(left);
        free_value(right);
        return result;
    }
    Value result = make_integer(left.as.integer + right.as.integer);
    free_value(left);
    free_value(right);
    return result;
}

static QS_UNUSED_FN Value qs_sub(Value left, Value right) {
    Value result;
    if (left.type == VALUE_FLOAT || right.type == VALUE_FLOAT) {
        result = make_float(value_to_double(left) - value_to_double(right));
    } else {
        result = make_integer(left.as.integer - right.as.integer);
    }
    free_value(left);
    free_value(right);
    return result;
}

static QS_UNUSED_FN Value qs_mul(Value left, Value right) {
    Value result;
    if (left.type == VALUE_STRING && right.type == VALUE_INTEGER) {
        long times = right.as.integer;
        if (times < 0) times = 0;
        char *repeated = calloc(1, strlen(left.as.string) * (size_t)times + 1);
        for (long i = 0; i < times; i++) strcat(repeated, left.as.string);
        free_value(left);
        free_value(right);
        return make_string_owned(repeated);
    }
    if (right.type == VALUE_STRING && left.type == VALUE_INTEGER) {
        long times = left.as.integer;
        if (times < 0) times = 0;
        char *repeated = calloc(1, strlen(right.as.string) * (size_t)times + 1);
        for (long i = 0; i < times; i++) strcat(repeated, right.as.string);
        free_value(left);
        free_value(right);
        return make_string_owned(repeated);
    }
    if (left.type == VALUE_FLOAT || right.type == VALUE_FLOAT) {
        result = make_float(value_to_double(left) * value_to_double(right));
    } else {
        result = make_integer(left.as.integer * right.as.integer);
    }
    free_value(left);
    free_value(right);
    return result;
}

static QS_UNUSED_FN Value qs_div(Value left, Value right) {
    Value result;
    if (left.type == VALUE_FLOAT || right.type == VALUE_FLOAT) {
        result = make_float(value_to_double(left) / value_to_double(right));
    } else {
        if (right.as.integer == 0) {
            free_value(left);
            free_value(right);
            qs_raise("cannot divide by zero");
        }
        result = make_integer(left.as.integer / right.as.integer);
    }
    free_value(left);
    free_value(right);
    return result;
}

static QS_UNUSED_FN Value qs_mod(Value left, Value right) {
    if (right.as.integer == 0) {
        free_value(left);
        free_value(right);
        qs_raise("cannot divide by zero");
    }
    Value result = make_integer(left.as.integer % right.as.integer);
    free_value(left);
    free_value(right);
    return result;
}

static QS_UNUSED_FN Value qs_neg(Value operand) {
    Value result;
    if (operand.type == VALUE_FLOAT) result = make_float(-operand.as.floating);
    else result = make_integer(-operand.as.integer);
    free_value(operand);
    return result;
}

static QS_UNUSED_FN Value qs_not(Value operand) {
    Value result = make_bool(!value_truthy(operand));
    free_value(operand);
    return result;
}

static QS_UNUSED_FN Value qs_eq(Value left, Value right) {
    Value result = make_bool(values_equal(left, right));
    free_value(left);
    free_value(right);
    return result;
}

static QS_UNUSED_FN Value qs_neq(Value left, Value right) {
    Value result = make_bool(!values_equal(left, right));
    free_value(left);
    free_value(right);
    return result;
}

static void qs_require_numbers(Value left, Value right, const char *op) {
    int ln = (left.type == VALUE_INTEGER || left.type == VALUE_FLOAT);
    int rn = (right.type == VALUE_INTEGER || right.type == VALUE_FLOAT);
    if (ln && rn) return;
    char msg[128];
    snprintf(msg, sizeof(msg), "'%s' requires two numbers", op);
    free_value(left);
    free_value(right);
    qs_raise(msg);
}

static QS_UNUSED_FN Value qs_lt(Value left, Value right) {
    qs_require_numbers(left, right, "<");
    Value result = make_bool(value_compare(left, right) < 0);
    free_value(left);
    free_value(right);
    return result;
}

static QS_UNUSED_FN Value qs_gt(Value left, Value right) {
    qs_require_numbers(left, right, ">");
    Value result = make_bool(value_compare(left, right) > 0);
    free_value(left);
    free_value(right);
    return result;
}

static QS_UNUSED_FN Value qs_lte(Value left, Value right) {
    qs_require_numbers(left, right, "<=");
    Value result = make_bool(value_compare(left, right) <= 0);
    free_value(left);
    free_value(right);
    return result;
}

static QS_UNUSED_FN Value qs_gte(Value left, Value right) {
    qs_require_numbers(left, right, ">=");
    Value result = make_bool(value_compare(left, right) >= 0);
    free_value(left);
    free_value(right);
    return result;
}

static QS_UNUSED_FN Value qs_in(Value container, Value item) {
    Value result = make_bool(value_contains(item, container));
    free_value(container);
    free_value(item);
    return result;
}

static QS_UNUSED_FN Value qs_and(Value left, Value right) {
    Value result = make_bool(value_truthy(left) && value_truthy(right));
    free_value(left);
    free_value(right);
    return result;
}

static QS_UNUSED_FN Value qs_or(Value left, Value right) {
    Value result = make_bool(value_truthy(left) || value_truthy(right));
    free_value(left);
    free_value(right);
    return result;
}

static QS_UNUSED_FN long qs_to_int(Value value) {
    long result = (value.type == VALUE_INTEGER) ? value.as.integer : (long)value_to_double(value);
    free_value(value);
    return result;
}

static QS_UNUSED_FN Value qs_index(Value obj, Value idx) {
    if (obj.type == VALUE_LIST) {
        long i = (idx.type == VALUE_INTEGER) ? idx.as.integer : 0;
        if (i > 0) i = i - 1;
        else if (i < 0) i = (long)obj.as.list.count + i;
        if (i >= 0 && (size_t)i < obj.as.list.count) {
            Value result = clone_value(obj.as.list.items[i]);
            free_value(obj);
            free_value(idx);
            return result;
        }
        free_value(obj);
        free_value(idx);
        qs_raise("list index is out of range");
    }
    if (obj.type == VALUE_STRING) {
        long raw = (idx.type == VALUE_INTEGER) ? idx.as.integer : 0;
        long i = raw;
        if (i > 0) i = i - 1;
        else if (i < 0) i = (long)strlen(obj.as.string) + i;
        Value result;
        if (i >= 0 && (size_t)i < strlen(obj.as.string)) {
            char letter[2] = {obj.as.string[i], '\0'};
            result = make_string_owned(qs_strdup(letter));
        } else {
            free_value(obj);
            free_value(idx);
            qs_raise(raw == 0 ? "indexes start at 1" : "string index is out of range");
            return make_null();
        }
        free_value(obj);
        free_value(idx);
        return result;
    }
    if (obj.type == VALUE_DICT) {
        char *key = value_to_string(idx);
        int di = dict_find(&obj.as.dict, key);
        if (di < 0) {
            free(key);
            free_value(obj);
            free_value(idx);
            qs_raise("dict key not found");
            return make_null();
        }
        Value result = clone_value(obj.as.dict.items[di].value);
        free(key);
        free_value(obj);
        free_value(idx);
        return result;
    }
    char msg[256];
    snprintf(msg, sizeof(msg), "cannot index this value");
    free_value(obj);
    free_value(idx);
    qs_raise(msg);
    return make_null();
}

static QS_UNUSED_FN void qs_store_index(Value obj, Value idx, Value value) {
    if (obj.type == VALUE_LIST) {
        long i = (idx.type == VALUE_INTEGER) ? idx.as.integer : 0;
        if (i > 0) i = i - 1;
        else if (i < 0) i = (long)obj.as.list.count + i;
        if (i >= 0 && (size_t)i < obj.as.list.count) {
            free_value(obj.as.list.items[i]);
            obj.as.list.items[i] = value;
        } else {
            free_value(value);
            free_value(obj);
            free_value(idx);
            qs_raise("list index is out of range");
        }
    } else if (obj.type == VALUE_DICT) {
        char *key = value_to_string(idx);
        dict_set(&obj.as.dict, key, value);
        free(key);
    } else {
        free_value(value);
        free_value(obj);
        free_value(idx);
        qs_raise("cannot assign into this value");
    }
    free_value(idx);
}

static QS_UNUSED_FN Value qs_slice(Value obj, Value start, Value end) {
    if (obj.type == VALUE_STRING) {
        size_t len = obj.as.string ? strlen(obj.as.string) : 0;
        long raw_s = (start.type == VALUE_INTEGER) ? start.as.integer : 0;
        long raw_e = (end.type == VALUE_INTEGER) ? end.as.integer : 0;
        long s = normalize_slice_edge(raw_s, len, 1);
        long e = normalize_slice_edge(raw_e, len, 0);
        if (e < s) e = s;
        Value result = make_string_owned(qs_strndup(obj.as.string + s, (size_t)(e - s)));
        free_value(obj);
        free_value(start);
        free_value(end);
        return result;
    }
    if (obj.type == VALUE_LIST) {
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
        free_value(obj);
        free_value(start);
        free_value(end);
        return result;
    }
    free_value(obj);
    free_value(start);
    free_value(end);
    return make_null();
}

static QS_UNUSED_FN Value qs_get_field(Environment *env, Value obj, const char *field) {
    Value result = make_null();
    if (obj.type == VALUE_OBJECT) {
        ObjectData *data = obj.as.object.data;
        for (size_t i = 0; i < data->fields.count; i++) {
            if (strcmp(data->fields.items[i].key, field) == 0) {
                result = clone_value(data->fields.items[i].value);
                break;
            }
        }
        if (result.type == VALUE_NULL) {
            char method_key[256];
            snprintf(method_key, sizeof(method_key), "%s.%s", data->class_name, field);
            int idx = find_variable(env, method_key);
            if (idx >= 0) {
                free_value(result);
                result = clone_value(env->items[idx].value);
            }
        }
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "cannot access field '%s' on non-object", field);
        free_value(obj);
        qs_raise(msg);
    }
    free_value(obj);
    return result;
}

static QS_UNUSED_FN Value qs_set_field(Value obj, const char *field, Value value) {
    if (obj.type == VALUE_OBJECT) {
        ObjectData *data = obj.as.object.data;
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
            if (data->fields.count >= data->fields.capacity) {
                size_t new_cap = data->fields.capacity ? data->fields.capacity * 2 : 8;
                data->fields.items = realloc(data->fields.items, new_cap * sizeof(DictEntry));
                data->fields.capacity = new_cap;
            }
            data->fields.items[data->fields.count].key = qs_strdup(field);
            data->fields.items[data->fields.count].value = value;
            data->fields.count++;
        }
    } else {
        free_value(value);
        char msg[256];
        snprintf(msg, sizeof(msg), "cannot set field '%s' on non-object", field);
        free_value(obj);
        qs_raise(msg);
    }
    return obj;
}

static QS_UNUSED_FN Value qs_call_named(Environment *env, const char *name, Value *args,
                                        const char **arg_names, int argc) {
    int var_idx = find_variable(env, name);
    if (var_idx >= 0 && env->items[var_idx].value.type == VALUE_LAMBDA) {
        CallArgList cargs = {0};
        for (int i = 0; i < argc; i++) {
            CallArg arg = {0};
            arg.value = clone_value(args[i]);
            if (arg_names && arg_names[i]) {
                arg.name = qs_strdup(arg_names[i]);
                arg.is_keyword = 1;
            }
            call_arg_push(&cargs, arg);
        }
        ParseError perr = {0};
        Parser parser = {"", 0, 0, env, &perr, -1};
        Value out = make_null();
        int ok = call_lambda(&parser, &env->items[var_idx].value, &cargs, &out);
        free_call_args(&cargs);
        for (int i = 0; i < argc; i++) free_value(args[i]);
        if (!ok) {
            free_value(out);
            qs_raise(perr.message[0] ? perr.message : "call failed");
        }
        return out;
    }

    QsFnEntry *entry = qs_lookup_entry(name);
    if (entry && (entry->param_count == argc || !find_function(name)) && !arg_names) {
        Function *tf = find_function(name);
        if (tf) {
            int pi = 0;
            int mismatch = 0;
            size_t bad_p = 0;
            for (size_t p = 0; p < (size_t)tf->param_count && pi < argc; p++) {
                Param *pp = &tf->params[p];
                if (pp->is_args || pp->is_kwargs) continue;
                if (!type_matches(pp->type, args[pi])) {
                    mismatch = 1;
                    bad_p = p;
                    break;
                }
                pi++;
            }
            if (mismatch) {
                char msg[256];
                snprintf(msg, sizeof(msg), "argument '%s' has the wrong type", tf->params[bad_p].name);
                for (int i = 0; i < argc; i++) free_value(args[i]);
                qs_raise(msg);
            }
        }
        Value out = entry->fn(env, args, argc);
        for (int i = 0; i < argc; i++) free_value(args[i]);
        return out;
    }

    Function *func = find_function(name);
    if (func) {
        CallArgList cargs = {0};
        cargs.count = (size_t)argc;
        cargs.items = argc > 0 ? calloc((size_t)argc, sizeof(CallArg)) : NULL;
        for (int i = 0; i < argc; i++) {
            cargs.items[i].value = clone_value(args[i]);
            if (arg_names && arg_names[i]) {
                cargs.items[i].name = qs_strdup(arg_names[i]);
                cargs.items[i].is_keyword = 1;
            } else {
                cargs.items[i].name = NULL;
                cargs.items[i].is_keyword = 0;
            }
        }
        ParseError perr = {0};
        Parser parser = {"", 0, 0, env, &perr, -1};
        Value out = make_null();
        int ok = call_user_function(&parser, func, &cargs, &out);
        free_call_args(&cargs);
        for (int i = 0; i < argc; i++) free_value(args[i]);
        if (!ok) {
            free_value(out);
            qs_raise(perr.message[0] ? perr.message : "call failed");
        }
        return out;
    }

    if (is_builtin_function(name)) {
        CallArgList cargs = {0};
        for (int i = 0; i < argc; i++) {
            CallArg arg = {0};
            arg.value = clone_value(args[i]);
            if (arg_names && arg_names[i]) {
                arg.name = qs_strdup(arg_names[i]);
                arg.is_keyword = 1;
            }
            call_arg_push(&cargs, arg);
        }
        ParseError perr = {0};
        Parser parser = {"", 0, 0, env, &perr, -1};
        Value out = make_null();
        int ok = call_builtin(&parser, name, &cargs, &out);
        free_call_args(&cargs);
        for (int i = 0; i < argc; i++) free_value(args[i]);
        if (!ok) {
            free_value(out);
            qs_raise(perr.message[0] ? perr.message : "call failed");
        }
        return out;
    }

    for (int i = 0; i < argc; i++) free_value(args[i]);
    char msg[256];
    snprintf(msg, sizeof(msg), "unknown function '%s'", name);
    qs_raise(msg);
    return make_null();
}

static QS_UNUSED_FN Value qs_call(Environment *env, const char *name, Value *args, int argc) {
    return qs_call_named(env, name, args, NULL, argc);
}

static QS_UNUSED_FN Value qs_make_lambda(Environment *capture, const char *body, int count,
                                          const char **names, const char **types) {
    char **param_names = count > 0 ? malloc((size_t)count * sizeof(char *)) : NULL;
    char **param_types = count > 0 ? calloc((size_t)count, sizeof(char *)) : NULL;
    for (int i = 0; i < count; i++) {
        param_names[i] = qs_strdup(names[i]);
        if (types && types[i]) param_types[i] = qs_strdup(types[i]);
    }
    Environment *closure = NULL;
    if (capture) {
        closure = malloc(sizeof(Environment));
        closure->count = capture->count;
        closure->capacity = capture->count;
        closure->items = capture->count ? calloc(capture->count, sizeof(Variable)) : NULL;
        closure->slots = NULL;
        closure->slot_count = 0;
        for (size_t i = 0; i < capture->count; i++) {
            closure->items[i].name = qs_strdup(capture->items[i].name);
            closure->items[i].value = clone_value(capture->items[i].value);
        }
    }
    return make_lambda(param_names, param_types, (size_t)count, qs_strdup(body), closure);
}

static QS_UNUSED_FN void qs_import(Environment *env, const char *text) {
    Program sub = {0};
    if (!load_source(text, &sub)) return;
    if (g_vm_source_dir) {
        free(sub.dir);
        sub.dir = qs_strdup(g_vm_source_dir);
    }
    Program *saved = g_program;
    g_program = &sub;
    strip_block_comments(&sub);
    ParseError ferr = {0};
    collect_imports(&sub, env, &ferr);
    collect_functions(&sub, &ferr);
    ParseError err = {0};
    execute_range(&sub, env, 0, sub.count, &err);
    if (err.message[0]) {
        fprintf(stderr, "Error at line %d: %s\n", err.line, err.message);
    }
    free_program(&sub);
    g_program = saved;
}

static QS_UNUSED_FN Value qs_method_call(Environment *env, Value obj, const char *method, Value *args, int argc) {
    if (obj.type == VALUE_OBJECT) {
        ObjectData *data = obj.as.object.data;
        if (argc == 0) {
            for (size_t i = 0; i < data->fields.count; i++) {
                if (strcmp(data->fields.items[i].key, method) == 0) {
                    Value result = clone_value(data->fields.items[i].value);
                    free_value(obj);
                    return result;
                }
            }
        }
        char key[256];
        snprintf(key, sizeof(key), "%s.%s", data->class_name, method);
        QsFnPtr fn = qs_lookup(key);
        if (fn) {
            Value *with_self = malloc((size_t)(argc + 1) * sizeof(Value));
            with_self[0] = obj;
            for (int i = 0; i < argc; i++) with_self[i + 1] = args[i];
            Value result = fn(env, with_self, argc + 1);
            free(with_self);
            for (int i = 0; i < argc; i++) free_value(args[i]);
            free_value(obj);
            return result;
        }
        free_value(obj);
        for (int i = 0; i < argc; i++) free_value(args[i]);
        char msg[256];
        snprintf(msg, sizeof(msg), "unknown field or method '%s' on object", method);
        qs_raise(msg);
    }

    ValueList margs = {0};
    for (int i = 0; i < argc; i++) list_push(&margs, args[i]);
    ParseError perr = {0};
    Parser parser = {"", 0, 0, env, &perr, -1};
    int ok = apply_method(&parser, &obj, method, &margs);
    for (size_t i = 0; i < margs.count; i++) free_value(margs.items[i]);
    free(margs.items);
    if (!ok) {
        free_value(obj);
        char msg[256];
        snprintf(msg, sizeof(msg), "%s", perr.message[0] ? perr.message : "method call failed");
        qs_raise(msg);
    }
    return obj;
}

static QS_UNUSED_FN Value qs_new(Environment *env, const char *class_name, Value *args, int argc) {
    Value obj = make_object(class_name);
    char key[256];
    snprintf(key, sizeof(key), "%s.init", class_name);
    QsFnPtr fn = qs_lookup(key);
    if (fn) {
        Value *with_self = malloc((size_t)(argc + 1) * sizeof(Value));
        with_self[0] = obj;
        for (int i = 0; i < argc; i++) with_self[i + 1] = args[i];
        Value result = fn(env, with_self, argc + 1);
        free(with_self);
        free_value(result);
        for (int i = 0; i < argc; i++) free_value(args[i]);
        return obj;
    }
    for (int i = 0; i < argc; i++) free_value(args[i]);
    return obj;
}
