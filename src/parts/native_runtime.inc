#include <setjmp.h>

#if defined(__GNUC__)
#define QS_NOINLINE __attribute__((noinline))
#else
#define QS_NOINLINE
#endif

#if defined(_WIN32) && defined(__GNUC__) && !defined(__clang__)
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
    if (g_call_stack_count) {
        fprintf(stderr, "Stack trace:\n");
        for (size_t i = g_call_stack_count; i > 0; i--) {
            fprintf(stderr, "  at %s\n", g_call_stack[i - 1]);
        }
    } else {
        fprintf(stderr, "Stack trace:\n");
        fprintf(stderr, "  at <native> at a_native.qs:1\n");
    }
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

static QS_UNUSED_FN Value *qs_get_ptr(Environment *env, Environment *local, const char *name) {
    int idx = -1;
    if (local) idx = find_variable(local, name);
    if (idx >= 0) return &local->items[idx].value;
    idx = find_variable(env, name);
    if (idx >= 0) return &env->items[idx].value;
    return NULL;
}

static QS_UNUSED_FN void qs_set(Environment *env, Environment *local, const char *name, Value value) {
    Environment *target = env;
    int idx = -1;
    if (local) {
        idx = find_variable(local, name);
        if (idx >= 0) target = local;
    }
    if (idx < 0) idx = find_variable(env, name);
    if (idx >= 0) {
        if (is_define_name(name)) {
            free_value(value);
            char msg[256];
            snprintf(msg, sizeof(msg), "definition '%s' already exists and cannot be changed", name);
            qs_raise(msg);
            return;
        }
        free_value(target->items[idx].value);
        target->items[idx].value = value;
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

static QS_UNUSED_FN Value qs_bool_cmp(Value left, Value right, int op) {
    if (left.type == VALUE_INTEGER && right.type == VALUE_INTEGER) {
        long a = left.as.integer, b = right.as.integer;
        int r = 0;
        switch (op) {
            case 0: r = (a == b); break;
            case 1: r = (a != b); break;
            case 2: r = (a < b); break;
            case 3: r = (a > b); break;
            case 4: r = (a <= b); break;
            case 5: r = (a >= b); break;
            default: break;
        }
        return make_bool(r);
    }
    switch (op) {
        case 0: return qs_eq(left, right);
        case 1: return qs_neq(left, right);
        case 2: return qs_lt(left, right);
        case 3: return qs_gt(left, right);
        case 4: return qs_lte(left, right);
        case 5: return qs_gte(left, right);
        default: break;
    }
    free_value(left);
    free_value(right);
    return make_bool(0);
}

static QS_UNUSED_FN void qs_append_var(Environment *env, Environment *local,
                                       const char *name, Value rhs) {
    Environment *target = (local && find_variable(local, name) >= 0) ? local : env;
    int idx = find_variable(target, name);
    if (idx < 0 || is_define_name(name)) {
        Value r = qs_add(qs_get(env, local, name), rhs);
        qs_set(env, local, name, r);
        return;
    }
    Value cur = target->items[idx].value;
    if (cur.type == VALUE_STRING && rhs.type == VALUE_STRING) {
        size_t la = cur.as.string ? strlen(cur.as.string) : 0;
        size_t lb = rhs.as.string ? strlen(rhs.as.string) : 0;
        char *na = realloc(cur.as.string, la + lb + 1);
        if (na) {
            if (lb) memcpy(na + la, rhs.as.string, lb);
            na[la + lb] = '\0';
            target->items[idx].value.as.string = na;
            free_value(rhs);
            return;
        }
    }
    Value r = qs_add(clone_value(cur), rhs);
    free_value(target->items[idx].value);
    target->items[idx].value = r;
}

static QS_UNUSED_FN Value qs_len_ref(const Value *value) {
    if (value->type == VALUE_STRING) {
        return make_integer((long)strlen(value->as.string ? value->as.string : ""));
    }
    if (value->type == VALUE_LIST) {
        return make_integer((long)value->as.list.count);
    }
    if (value->type == VALUE_DICT) {
        return make_integer((long)value->as.dict.count);
    }
    qs_raise("len needs a string, list, or map");
    return make_null();
}

static QS_UNUSED_FN Value qs_len_value(Value value) {
    Value out;
    if (value.type == VALUE_STRING) {
        out = make_integer((long)strlen(value.as.string ? value.as.string : ""));
    } else if (value.type == VALUE_LIST) {
        out = make_integer((long)value.as.list.count);
    } else if (value.type == VALUE_DICT) {
        out = make_integer((long)value.as.dict.count);
    } else {
        free_value(value);
        qs_raise("len needs a string, list, or map");
        return make_null();
    }
    free_value(value);
    return out;
}

static QS_UNUSED_FN char *qs_text_arg(Value value, int *owned) {
    if (value.type == VALUE_STRING) {
        *owned = 0;
        return value.as.string ? value.as.string : "";
    }
    *owned = 1;
    return value_to_string(value);
}

static QS_UNUSED_FN Value qs_dict_has(const Value *dict, Value key_value) {
    int owned;
    char *key = qs_text_arg(key_value, &owned);
    int found = dict_find(&dict->as.dict, key) >= 0;
    if (owned) free(key);
    free_value(key_value);
    return make_bool(found);
}

static QS_UNUSED_FN Value qs_dict_get(const Value *dict, Value key_value) {
    int owned;
    char *key = qs_text_arg(key_value, &owned);
    int index = dict_find(&dict->as.dict, key);
    if (owned) free(key);
    free_value(key_value);
    return index >= 0 ? clone_value(dict->as.dict.entries[index].value) : make_null();
}

static QS_UNUSED_FN Value qs_dict_get_or(const Value *dict, Value key_value, Value fallback) {
    int owned;
    char *key = qs_text_arg(key_value, &owned);
    int index = dict_find(&dict->as.dict, key);
    if (owned) free(key);
    free_value(key_value);
    Value result = clone_value(index >= 0 ? dict->as.dict.entries[index].value : fallback);
    free_value(fallback);
    return result;
}

static QS_UNUSED_FN Value qs_dict_keys(const Value *dict) {
    ValueList keys = {0};
    for (size_t i = 0; i < dict->as.dict.count; i++) {
        list_push(&keys, make_string_owned(qs_strdup(dict->as.dict.entries[i].key)));
    }
    Value result;
    result.type = VALUE_LIST;
    result.as.list = keys;
    return result;
}

static QS_UNUSED_FN Value qs_dict_values(const Value *dict) {
    ValueList vals = {0};
    for (size_t i = 0; i < dict->as.dict.count; i++) {
        list_push(&vals, clone_value(dict->as.dict.entries[i].value));
    }
    Value result;
    result.type = VALUE_LIST;
    result.as.list = vals;
    return result;
}

static QS_UNUSED_FN Value qs_dict_items(const Value *dict) {
    ValueList pairs = {0};
    for (size_t i = 0; i < dict->as.dict.count; i++) {
        ValueList pair = {0};
        list_push(&pair, make_string_owned(qs_strdup(dict->as.dict.entries[i].key)));
        list_push(&pair, clone_value(dict->as.dict.entries[i].value));
        Value entry;
        entry.type = VALUE_LIST;
        entry.as.list = pair;
        list_push(&pairs, entry);
    }
    Value result;
    result.type = VALUE_LIST;
    result.as.list = pairs;
    return result;
}

static QS_UNUSED_FN Value qs_str_contains(const Value *text, Value needle_value) {
    int owned;
    char *needle = qs_text_arg(needle_value, &owned);
    int found = strstr(text->as.string ? text->as.string : "", needle) != NULL;
    if (owned) free(needle);
    free_value(needle_value);
    return make_bool(found);
}

static QS_UNUSED_FN Value qs_str_index_of(const Value *text, Value needle_value) {
    const char *src = text->as.string ? text->as.string : "";
    int owned;
    char *needle = qs_text_arg(needle_value, &owned);
    const char *pos = strstr(src, needle);
    long index = pos ? (long)(pos - src) + 1 : 0;
    if (owned) free(needle);
    free_value(needle_value);
    return make_integer(index);
}

static QS_UNUSED_FN Value qs_substring_range(const Value *text, long start, long end) {
    const char *src = text->as.string ? text->as.string : "";
    long len = (long)strlen(src);
    if (start < 1) start = 1;
    if (end > len) end = len;
    if (start > end) start = end;
    char *sub = qs_strndup(src + start - 1, (size_t)(end - start + 1));
    return make_string_owned(sub);
}

static QS_UNUSED_FN Value qs_str_substring(const Value *text, Value start_value) {
    long start = start_value.as.integer;
    free_value(start_value);
    long end = (long)strlen(text->as.string ? text->as.string : "");
    return qs_substring_range(text, start, end);
}

static QS_UNUSED_FN Value qs_str_substring2(const Value *text, Value start_value, Value end_value) {
    long start = start_value.as.integer;
    long end = end_value.as.integer;
    free_value(start_value);
    free_value(end_value);
    return qs_substring_range(text, start, end);
}

static QS_UNUSED_FN Value qs_str_starts_with(const Value *text, Value prefix_value) {
    const char *src = text->as.string ? text->as.string : "";
    int owned;
    char *prefix = qs_text_arg(prefix_value, &owned);
    int match = strncmp(src, prefix, strlen(prefix)) == 0;
    if (owned) free(prefix);
    free_value(prefix_value);
    return make_bool(match);
}

static QS_UNUSED_FN Value qs_str_ends_with(const Value *text, Value suffix_value) {
    const char *src = text->as.string ? text->as.string : "";
    int owned;
    char *suffix = qs_text_arg(suffix_value, &owned);
    size_t src_len = strlen(src);
    size_t suffix_len = strlen(suffix);
    int match = src_len >= suffix_len && strcmp(src + src_len - suffix_len, suffix) == 0;
    if (owned) free(suffix);
    free_value(suffix_value);
    return make_bool(match);
}

static QS_UNUSED_FN Value qs_str_trim_start(const Value *text) {
    const char *src = text->as.string ? text->as.string : "";
    while (*src && isspace((unsigned char)*src)) src++;
    return make_string_owned(qs_strdup(src));
}

static QS_UNUSED_FN Value qs_str_trim_end(const Value *text) {
    char *trimmed = qs_strdup(text->as.string ? text->as.string : "");
    size_t len = strlen(trimmed);
    while (len > 0 && isspace((unsigned char)trimmed[len - 1])) len--;
    trimmed[len] = '\0';
    return make_string_owned(trimmed);
}

static QS_UNUSED_FN Value qs_str_title(const Value *text) {
    char *titled = qs_strdup(text->as.string ? text->as.string : "");
    int new_word = 1;
    for (char *p = titled; *p; p++) {
        if (isalpha((unsigned char)*p)) {
            *p = (char)(new_word ? toupper((unsigned char)*p) : tolower((unsigned char)*p));
            new_word = 0;
        } else {
            new_word = 1;
        }
    }
    return make_string_owned(titled);
}

static QS_UNUSED_FN Value qs_str_split(const Value *text, Value separator_value) {
    int owned;
    char *separator = qs_text_arg(separator_value, &owned);
    const char *src = text->as.string ? text->as.string : "";
    size_t separator_len = strlen(separator);
    ValueList parts = {0};
    if (separator_len == 0) {
        for (size_t i = 0; src[i]; i++) {
            char single[2] = {src[i], '\0'};
            list_push(&parts, make_string_owned(qs_strdup(single)));
        }
    } else {
        const char *start = src;
        const char *found;
        while ((found = strstr(start, separator)) != NULL) {
            list_push(&parts, make_string_owned(qs_strndup(start, (size_t)(found - start))));
            start = found + separator_len;
        }
        list_push(&parts, make_string_owned(qs_strdup(start)));
    }
    if (owned) free(separator);
    free_value(separator_value);
    Value result;
    result.type = VALUE_LIST;
    result.as.list = parts;
    return result;
}

static QS_UNUSED_FN Value qs_str_replace(const Value *text, Value from_value, Value to_value) {
    int from_owned;
    int to_owned;
    char *from = qs_text_arg(from_value, &from_owned);
    char *to = qs_text_arg(to_value, &to_owned);
    const char *src = text->as.string ? text->as.string : "";
    size_t from_len = strlen(from);
    size_t to_len = strlen(to);
    size_t cap = strlen(src) + 1;
    char *result = malloc(cap);
    if (!result) { fprintf(stderr, "out of memory\n"); exit(2); }
    result[0] = '\0';
    while (1) {
        const char *pos = strstr(src, from);
        if (!pos) {
            strcat(result, src);
            break;
        }
        size_t prefix = (size_t)(pos - src);
        if (strlen(result) + prefix + to_len + 1 > cap) {
            cap = (strlen(result) + prefix + to_len + 1) * 2;
            char *grown = realloc(result, cap);
            if (!grown) { fprintf(stderr, "out of memory\n"); exit(2); }
            result = grown;
        }
        strncat(result, src, prefix);
        strcat(result, to);
        src = pos + from_len;
    }
    if (from_owned) free(from);
    if (to_owned) free(to);
    free_value(from_value);
    free_value(to_value);
    return make_string_owned(result);
}

static QS_UNUSED_FN Value qs_str_reverse(const Value *text) {
    const char *src = text->as.string ? text->as.string : "";
    size_t len = strlen(src);
    char *rev = malloc(len + 1);
    if (!rev) { fprintf(stderr, "out of memory\n"); exit(2); }
    for (size_t i = 0; i < len; i++) rev[i] = src[len - 1 - i];
    rev[len] = '\0';
    return make_string_owned(rev);
}

static QS_UNUSED_FN Value qs_str_repeat(const Value *text, Value count_value) {
    long times = count_value.as.integer;
    free_value(count_value);
    if (times < 0) times = 0;
    const char *src = text->as.string ? text->as.string : "";
    size_t text_len = strlen(src);
    char *repeated = malloc(text_len * (size_t)times + 1);
    if (!repeated) { fprintf(stderr, "out of memory\n"); exit(2); }
    for (long i = 0; i < times; i++) {
        memcpy(repeated + (size_t)i * text_len, src, text_len);
    }
    repeated[text_len * (size_t)times] = '\0';
    return make_string_owned(repeated);
}

static QS_UNUSED_FN Value qs_str_join(const Value *separator_value, Value items) {
    const char *separator = separator_value->as.string ? separator_value->as.string : "";
    size_t separator_len = strlen(separator);
    size_t total = 0;
    char **parts = calloc(items.as.list.count, sizeof(char *));
    if (items.as.list.count && !parts) { fprintf(stderr, "out of memory\n"); exit(2); }
    for (size_t i = 0; i < items.as.list.count; i++) {
        parts[i] = value_to_string(items.as.list.items[i]);
        total += strlen(parts[i]);
        if (i) total += separator_len;
    }
    char *joined = malloc(total + 1);
    if (!joined) { fprintf(stderr, "out of memory\n"); exit(2); }
    joined[0] = '\0';
    for (size_t i = 0; i < items.as.list.count; i++) {
        if (i) strcat(joined, separator);
        strcat(joined, parts[i]);
        free(parts[i]);
    }
    free(parts);
    free_value(items);
    return make_string_owned(joined);
}

static QS_UNUSED_FN Value qs_in_ref(const Value *container, Value item) {
    Value result = make_bool(value_contains(item, *container));
    free_value(item);
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

static QS_UNUSED_FN Value qs_index_ref(const Value *obj, Value idx) {
    if (obj->type == VALUE_LIST) {
        long i = (idx.type == VALUE_INTEGER) ? idx.as.integer : 0;
        if (i > 0) i = i - 1;
        else if (i < 0) i = (long)obj->as.list.count + i;
        if (i >= 0 && (size_t)i < obj->as.list.count) {
            Value result = clone_value(obj->as.list.items[i]);
            free_value(idx);
            return result;
        }
        free_value(idx);
        qs_raise("list index is out of range");
    }
    if (obj->type == VALUE_STRING) {
        long raw = (idx.type == VALUE_INTEGER) ? idx.as.integer : 0;
        long i = raw;
        if (i > 0) i = i - 1;
        else if (i < 0) i = (long)strlen(obj->as.string) + i;
        Value result;
        if (i >= 0 && (size_t)i < strlen(obj->as.string)) {
            char letter[2] = {obj->as.string[i], '\0'};
            result = make_string_owned(qs_strdup(letter));
        } else {
            free_value(idx);
            qs_raise(raw == 0 ? "indexes start at 1" : "string index is out of range");
            return make_null();
        }
        free_value(idx);
        return result;
    }
    if (obj->type == VALUE_DICT) {
        char *key = value_to_string(idx);
        int di = dict_find(&obj->as.dict, key);
        if (di < 0) {
            free(key);
            free_value(idx);
            qs_raise("dict key not found");
            return make_null();
        }
        Value result = clone_value(obj->as.dict.entries[di].value);
        free(key);
        free_value(idx);
        return result;
    }
    free_value(idx);
    qs_raise("cannot index this value");
    return make_null();
}

static QS_UNUSED_FN Value qs_index(Value obj, Value idx) {
    Value result = qs_index_ref(&obj, idx);
    free_value(obj);
    return result;
}

static QS_UNUSED_FN Value qs_opt_index(Value obj, Value idx) {
    Value result = make_null();
    if (obj.type == VALUE_DICT) {
        if (idx.type != VALUE_STRING) {
            free_value(idx);
            free_value(obj);
            qs_raise("dict index must be a string");
            return make_null();
        }
        int found = dict_find(&obj.as.dict, idx.as.string);
        if (found >= 0) {
            result = clone_value(obj.as.dict.entries[found].value);
        }
    } else if (obj.type == VALUE_LIST) {
        long i = (idx.type == VALUE_INTEGER) ? idx.as.integer : 0;
        if (i > 0) i = i - 1;
        else if (i < 0) i = (long)obj.as.list.count + i;
        if (idx.type == VALUE_INTEGER && i >= 0 && (size_t)i < obj.as.list.count) {
            result = clone_value(obj.as.list.items[i]);
        }
    } else if (obj.type == VALUE_STRING) {
        long raw = (idx.type == VALUE_INTEGER) ? idx.as.integer : 0;
        long i = raw;
        if (i > 0) i = i - 1;
        else if (i < 0) i = (long)strlen(obj.as.string) + i;
        if (idx.type == VALUE_INTEGER && i >= 0 && (size_t)i < strlen(obj.as.string)) {
            char letter[2] = {obj.as.string[i], '\0'};
            result = make_string_owned(qs_strdup(letter));
        }
    }
    free_value(idx);
    free_value(obj);
    return result;
}

static QS_UNUSED_FN Value qs_opt_field(Environment *env, Value obj, const char *field) {
    Value result = make_null();
    if (obj.type == VALUE_DICT) {
        int found = dict_find(&obj.as.dict, field);
        if (found >= 0) {
            result = clone_value(obj.as.dict.entries[found].value);
        }
    } else if (obj.type == VALUE_OBJECT) {
        for (size_t i = 0; i < obj.as.object.data->fields.count; i++) {
            if (strcmp(obj.as.object.data->fields.entries[i].key, field) == 0) {
                result = clone_value(obj.as.object.data->fields.entries[i].value);
                break;
            }
        }
        if (result.type == VALUE_NULL) {
            char method_key[256];
            snprintf(method_key, sizeof(method_key), "%s.%s", obj.as.object.data->class_name, field);
            int idx = find_variable(env, method_key);
            if (idx >= 0) {
                result = clone_value(env->items[idx].value);
            }
        }
    }
    free_value(obj);
    return result;
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

static QS_UNUSED_FN Value qs_slice_ref(const Value *obj, Value start, Value end) {
    if (obj->type == VALUE_STRING) {
        size_t len = obj->as.string ? strlen(obj->as.string) : 0;
        long raw_s = (start.type == VALUE_INTEGER) ? start.as.integer : 0;
        long raw_e = (end.type == VALUE_INTEGER) ? end.as.integer : 0;
        long s = normalize_slice_edge(raw_s, len, 1);
        long e = normalize_slice_edge(raw_e, len, 0);
        if (e < s) e = s;
        Value result = make_string_owned(qs_strndup(obj->as.string + s, (size_t)(e - s)));
        free_value(start);
        free_value(end);
        return result;
    }
    if (obj->type == VALUE_LIST) {
        size_t len = obj->as.list.count;
        long raw_s = (start.type == VALUE_INTEGER) ? start.as.integer : 0;
        long raw_e = (end.type == VALUE_INTEGER) ? end.as.integer : 0;
        long s = normalize_slice_edge(raw_s, len, 1);
        long e = normalize_slice_edge(raw_e, len, 0);
        if (e < s) e = s;
        ValueList list = {0};
        for (long i = s; i < e; i++) {
            list_push(&list, clone_value(obj->as.list.items[i]));
        }
        Value result = {0};
        result.type = VALUE_LIST;
        result.as.list = list;
        free_value(start);
        free_value(end);
        return result;
    }
    free_value(start);
    free_value(end);
    return make_null();
}

static QS_UNUSED_FN Value qs_slice(Value obj, Value start, Value end) {
    Value result = qs_slice_ref(&obj, start, end);
    free_value(obj);
    return result;
}

static QS_UNUSED_FN Value qs_get_field(Environment *env, Value obj, const char *field) {
    Value result = make_null();
    if (obj.type == VALUE_OBJECT) {
        ObjectData *data = obj.as.object.data;
        for (size_t i = 0; i < data->fields.count; i++) {
            if (strcmp(data->fields.entries[i].key, field) == 0) {
                result = clone_value(data->fields.entries[i].value);
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
        dict_set(&data->fields, field, value);
    } else {
        free_value(value);
        char msg[256];
        snprintf(msg, sizeof(msg), "cannot set field '%s' on non-object", field);
        free_value(obj);
        qs_raise(msg);
    }
    return obj;
}

static QS_UNUSED_FN void qs_index_assign(Environment *env, Environment *local, const char *name, Value key, Value value) {
    Value *slot = qs_get_ptr(env, local, name);
    if (!slot) {
        free_value(key);
        free_value(value);
        char msg[256];
        snprintf(msg, sizeof(msg), "unknown variable '%s'", name);
        qs_raise(msg);
        return;
    }
    if (slot->type == VALUE_DICT) {
        char *k = value_to_string(key);
        dict_set(&slot->as.dict, k, value);
        free(k);
    } else {
        free_value(value);
        char msg[256];
        snprintf(msg, sizeof(msg), "'%s' is not a map", name);
        qs_raise(msg);
    }
    free_value(key);
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
        fprintf(stderr, "Error: %s\n", err.message);
        if (err.line > 0) fprintf(stderr, "  at %s:%d:%d\n", "a_native.qs", err.line, err.column > 0 ? err.column : 1);
    }
    free_program(&sub);
    g_program = saved;
}

static QS_UNUSED_FN Value qs_method_call(Environment *env, Value obj, const char *method, Value *args, int argc) {
    if (obj.type == VALUE_OBJECT) {
        ObjectData *data = obj.as.object.data;
        if (argc == 0) {
            for (size_t i = 0; i < data->fields.count; i++) {
                if (strcmp(data->fields.entries[i].key, method) == 0) {
                    Value result = clone_value(data->fields.entries[i].value);
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

static QS_UNUSED_FN void qs_method_call_inplace(Environment *env, Environment *local,
                                                const char *name, const char *method,
                                                Value *args, int argc) {
    Environment *target = (local && find_variable(local, name) >= 0) ? local : env;
    int idx = find_variable(target, name);
    if (idx < 0) {
        for (int i = 0; i < argc; i++) free_value(args[i]);
        char msg[256];
        snprintf(msg, sizeof(msg), "unknown variable '%s'", name);
        qs_raise(msg);
        return;
    }
    if (is_define_name(name)) {
        for (int i = 0; i < argc; i++) free_value(args[i]);
        char msg[256];
        snprintf(msg, sizeof(msg), "definition '%s' already exists and cannot be changed", name);
        qs_raise(msg);
        return;
    }
    Value *slot = &target->items[idx].value;
    if (slot->type == VALUE_LIST && argc == 1 && strcmp(method, "append") == 0) {
        list_push(&slot->as.list, args[0]);
        return;
    }
    if (slot->type == VALUE_OBJECT) {
        Value cur = clone_value(*slot);
        Value result = qs_method_call(env, cur, method, args, argc);
        env_set(target, name, result);
        return;
    }
    ValueList margs = {0};
    for (int i = 0; i < argc; i++) list_push(&margs, args[i]);
    ParseError perr = {0};
    Parser parser = {"", 0, 0, env, &perr, -1};
    int ok = apply_method(&parser, slot, method, &margs);
    for (size_t i = 0; i < margs.count; i++) free_value(margs.items[i]);
    free(margs.items);
    if (!ok) {
        char msg[256];
        snprintf(msg, sizeof(msg), "%s", perr.message[0] ? perr.message : "method call failed");
        qs_raise(msg);
    }
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
