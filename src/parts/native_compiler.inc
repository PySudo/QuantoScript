#include <stdarg.h>

typedef struct {
    char *c_name;
    char *reg_name;
    char **params;
    char **param_types;
    int param_count;
    ASTNode *body;
    int is_method;
} CgFunc;

#define CG_MAX_FUNCS 2048
static CgFunc cg_funcs[CG_MAX_FUNCS];
static int cg_func_count = 0;

#define CG_MAX_CLASSES 512
static char *cg_classes[CG_MAX_CLASSES];
static int cg_class_count = 0;

static FILE *cg_out = NULL;
static int cg_temp = 0;
static int cg_label = 0;
static int cg_loop = 0;
static int cg_in_func = 0;
static char *cg_loop_save[128];
static int cg_loop_top = 0;

static void cg_stmt(ASTNode *node);
static void cg_block(ASTNode *block);
static char *cg_expr(ASTNode *node);
static char *cg_method_call(ASTNode *object, const char *method, ASTNode **args, int argc);

static void cg_emit(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(cg_out, fmt, ap);
    va_end(ap);
}

static char *cg_newtemp(void) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "t%d", cg_temp++);
    return qs_strdup(buffer);
}

static char *cg_label_name(const char *prefix) {
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "%s%d", prefix, cg_label++);
    return qs_strdup(buffer);
}

static int cg_is_class(const char *name) {
    for (int i = 0; i < cg_class_count; i++) {
        if (strcmp(cg_classes[i], name) == 0) return 1;
    }
    return 0;
}

static CgFunc *cg_add_func(const char *reg_name, char **params, char **param_types,
                           int param_count, ASTNode *body, int is_method) {
    if (cg_func_count >= CG_MAX_FUNCS) return NULL;
    CgFunc *f = &cg_funcs[cg_func_count];
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "qs_fn_%d", cg_func_count);
    f->c_name = qs_strdup(buffer);
    f->reg_name = qs_strdup(reg_name);
    f->param_count = param_count;
    f->params = NULL;
    f->param_types = NULL;
    if (param_count > 0) {
        f->params = malloc((size_t)param_count * sizeof(char *));
        f->param_types = malloc((size_t)param_count * sizeof(char *));
        for (int i = 0; i < param_count; i++) {
            f->params[i] = qs_strdup(params[i]);
            f->param_types[i] = (param_types && param_types[i]) ? qs_strdup(param_types[i]) : NULL;
        }
    }
    f->body = body;
    f->is_method = is_method;
    cg_func_count++;
    return f;
}

static void cg_add_class(const char *name) {
    if (cg_class_count < CG_MAX_CLASSES) {
        cg_classes[cg_class_count++] = qs_strdup(name);
    }
}

static void cg_collect(ASTNode *node) {
    if (!node) return;
    switch (node->type) {
        case AST_FUNC_DEF:
            cg_add_func(node->as.func_def.name, node->as.func_def.params,
                        node->as.func_def.param_types,
                        node->as.func_def.param_count, node->as.func_def.body, 0);
            cg_collect(node->as.func_def.body);
            break;
        case AST_CLASS_DEF: {
            cg_add_class(node->as.class_def.name);
            for (int m = 0; m < node->as.class_def.method_count; m++) {
                ASTNode *method = node->as.class_def.methods[m];
                char full[256];
                snprintf(full, sizeof(full), "%s.%s", node->as.class_def.name,
                         method->as.method_def.name);
                cg_add_func(full, method->as.method_def.params,
                            method->as.method_def.param_types,
                            method->as.method_def.param_count, method->as.method_def.body, 1);
                cg_collect(method->as.method_def.body);
            }
            break;
        }
        case AST_BLOCK:
        case AST_PROGRAM:
            for (int i = 0; i < node->as.block.count; i++) cg_collect(node->as.block.stmts[i]);
            break;
        case AST_IF:
            cg_collect(node->as.if_stmt.then_body);
            cg_collect(node->as.if_stmt.elif_branch);
            cg_collect(node->as.if_stmt.else_body);
            break;
        case AST_WHILE:
            cg_collect(node->as.while_stmt.body);
            break;
        case AST_REPEAT:
            cg_collect(node->as.repeat.body);
            break;
        default:
            break;
    }
}

static char *cg_emit_null(void) {
    char *t = cg_newtemp();
    cg_emit("    Value %s = make_null();\n", t);
    return t;
}

static void cg_emit_args_array(char **temps, int count) {
    if (count <= 0) {
        cg_emit("NULL");
        return;
    }
    cg_emit("(Value[]){");
    for (int i = 0; i < count; i++) {
        cg_emit("%s%s", i ? ", " : "", temps[i]);
    }
    cg_emit("}");
}

static void cg_emit_names_array(char **names, int count) {
    int any = 0;
    for (int i = 0; i < count; i++) {
        if (names && names[i]) { any = 1; break; }
    }
    if (!any) {
        cg_emit("NULL");
        return;
    }
    cg_emit("(const char*[]){");
    for (int i = 0; i < count; i++) {
        if (i) cg_emit(", ");
        if (names && names[i]) write_c_string(cg_out, names[i]);
        else cg_emit("NULL");
    }
    cg_emit("}");
}

static char *cg_binary_op(ASTNode *node) {
    char *left = cg_expr(node->as.binary.left);
    char *right = cg_expr(node->as.binary.right);
    char *t = cg_newtemp();
    const char *fn = "qs_add";
    switch (node->type) {
        case AST_BINARY_ADD: fn = "qs_add"; break;
        case AST_BINARY_SUB: fn = "qs_sub"; break;
        case AST_BINARY_MUL: fn = "qs_mul"; break;
        case AST_BINARY_DIV: fn = "qs_div"; break;
        case AST_BINARY_MOD: fn = "qs_mod"; break;
        case AST_BINARY_EQ: fn = "qs_eq"; break;
        case AST_BINARY_NEQ: fn = "qs_neq"; break;
        case AST_BINARY_LT: fn = "qs_lt"; break;
        case AST_BINARY_GT: fn = "qs_gt"; break;
        case AST_BINARY_LTE: fn = "qs_lte"; break;
        case AST_BINARY_GTE: fn = "qs_gte"; break;
        case AST_BINARY_IN: fn = "qs_in"; break;
        default: fn = "qs_add"; break;
    }
    cg_emit("    Value %s = %s(%s, %s);\n", t, fn, left, right);
    return t;
}

static char *cg_logic_op(ASTNode *node) {
    char *left = cg_expr(node->as.binary.left);
    char *t = cg_newtemp();
    cg_emit("    Value %s;\n", t);
    if (node->type == AST_BINARY_AND) {
        cg_emit("    if (qs_truthy(%s)) {\n", left);
        char *right = cg_expr(node->as.binary.right);
        cg_emit("        %s = make_bool(qs_truthy(%s));\n", t, right);
        cg_emit("    } else {\n        %s = make_bool(0);\n    }\n", t);
    } else {
        cg_emit("    if (qs_truthy(%s)) {\n", left);
        cg_emit("        %s = make_bool(1);\n", t);
        cg_emit("    } else {\n", t);
        char *right = cg_expr(node->as.binary.right);
        cg_emit("        %s = make_bool(qs_truthy(%s));\n    }\n", t, right);
    }
    return t;
}

static char *cg_chained(ASTNode *node) {
    int count = node->as.chained.count;
    char **vals = malloc((size_t)count * sizeof(char *));
    for (int i = 0; i < count; i++) vals[i] = cg_expr(node->as.chained.operands[i]);
    char *acc = NULL;
    for (int i = 0; i < count - 1; i++) {
        char *t = cg_newtemp();
        const char *fn = "qs_lt";
        const char *op = node->as.chained.ops[i];
        if (strcmp(op, ">") == 0) fn = "qs_gt";
        else if (strcmp(op, "<=") == 0) fn = "qs_lte";
        else if (strcmp(op, ">=") == 0) fn = "qs_gte";
        cg_emit("    Value %s = %s(%s, %s);\n", t, fn, vals[i], vals[i + 1]);
        if (!acc) {
            acc = t;
        } else {
            char *n = cg_newtemp();
            cg_emit("    Value %s = qs_and(%s, %s);\n", n, acc, t);
            acc = n;
        }
    }
    free(vals);
    if (!acc) acc = cg_emit_null();
    return acc;
}

static char *cg_list_comp(ASTNode *node) {
    char *t = cg_newtemp();
    char *coll = cg_expr(node->as.comp.iterable);
    char *it = cg_newtemp();
    char *ix = cg_newtemp();
    cg_emit("    Value %s = make_list_empty();\n", t);
    cg_emit("    Value %s = qs_call(env, \"_iter_items\", (Value[]){%s}, 1);\n", it, coll);
    cg_emit("    for (size_t %s = 0; %s < %s.as.list.count; %s++) {\n", ix, ix, it, ix);
    cg_emit("        qs_set(env, &local, ");
    write_c_string(cg_out, node->as.comp.var);
    cg_emit(", clone_value(%s.as.list.items[%s]));\n", it, ix);
    if (node->as.comp.cond) {
        char *cond = cg_expr(node->as.comp.cond);
        cg_emit("        if (qs_truthy(%s)) {\n", cond);
        char *val = cg_expr(node->as.comp.expr);
        cg_emit("            list_push(&%s.as.list, %s);\n", t, val);
        cg_emit("        }\n");
    } else {
        char *val = cg_expr(node->as.comp.expr);
        cg_emit("        list_push(&%s.as.list, %s);\n", t, val);
    }
    cg_emit("    }\n");
    cg_emit("    free_value(%s);\n", it);
    return t;
}

static char *cg_lambda(ASTNode *node) {
    char *t = cg_newtemp();
    cg_emit("    Value %s = qs_make_lambda(%s, ", t, cg_in_func ? "&local" : "env");
    write_c_string(cg_out, node->as.lambda.body_text ? node->as.lambda.body_text : "");
    cg_emit(", %d, ", node->as.lambda.param_count);
    if (node->as.lambda.param_count > 0) {
        cg_emit("(const char*[]){");
        for (int i = 0; i < node->as.lambda.param_count; i++) {
            if (i) cg_emit(", ");
            write_c_string(cg_out, node->as.lambda.params[i]);
        }
        cg_emit("}, ");
        int any_type = 0;
        for (int i = 0; i < node->as.lambda.param_count; i++) {
            if (node->as.lambda.param_types && node->as.lambda.param_types[i]) { any_type = 1; break; }
        }
        if (any_type) {
            cg_emit("(const char*[]){");
            for (int i = 0; i < node->as.lambda.param_count; i++) {
                if (i) cg_emit(", ");
                if (node->as.lambda.param_types && node->as.lambda.param_types[i])
                    write_c_string(cg_out, node->as.lambda.param_types[i]);
                else
                    cg_emit("NULL");
            }
            cg_emit("}");
        } else {
            cg_emit("NULL");
        }
    } else {
        cg_emit("NULL, NULL");
    }
    cg_emit(");\n");
    return t;
}

static char *cg_call_expr(ASTNode *node) {
    int argc = node->as.call.argc;
    char **temps = argc > 0 ? malloc((size_t)argc * sizeof(char *)) : NULL;
    for (int i = 0; i < argc; i++) temps[i] = cg_expr(node->as.call.args[i]);

    char *t = cg_newtemp();
    if (cg_is_class(node->as.call.name)) {
        cg_emit("    Value %s = qs_new(env, ", t);
        write_c_string(cg_out, node->as.call.name);
        cg_emit(", ");
        cg_emit_args_array(temps, argc);
        cg_emit(", %d);\n", argc);
    } else {
        cg_emit("    Value %s = qs_call_named(env, ", t);
        write_c_string(cg_out, node->as.call.name);
        cg_emit(", ");
        cg_emit_args_array(temps, argc);
        cg_emit(", ");
        cg_emit_names_array(node->as.call.arg_names, argc);
        cg_emit(", %d);\n", argc);
    }
    free(temps);
    return t;
}

static char *cg_method_call(ASTNode *object, const char *method, ASTNode **args, int argc) {
    char *obj = cg_expr(object);
    char **temps = argc > 0 ? malloc((size_t)argc * sizeof(char *)) : NULL;
    for (int i = 0; i < argc; i++) temps[i] = cg_expr(args[i]);
    char *t = cg_newtemp();
    cg_emit("    Value %s = qs_method_call(env, %s, ", t, obj);
    write_c_string(cg_out, method);
    cg_emit(", ");
    cg_emit_args_array(temps, argc);
    cg_emit(", %d);\n", argc);
    free(temps);
    return t;
}

static char *cg_expr(ASTNode *node) {
    if (!node) return cg_emit_null();

    switch (node->type) {
        case AST_INT_LIT: {
            char *t = cg_newtemp();
            cg_emit("    Value %s = make_integer(%ldL);\n", t, node->as.integer);
            return t;
        }
        case AST_FLOAT_LIT: {
            char *t = cg_newtemp();
            cg_emit("    Value %s = make_float(%.17g);\n", t, node->as.floating);
            return t;
        }
        case AST_STR_LIT: {
            char *t = cg_newtemp();
            cg_emit("    Value %s = make_string_owned(qs_strdup(", t);
            write_c_string(cg_out, node->as.string ? node->as.string : "");
            cg_emit("));\n");
            return t;
        }
        case AST_BOOL_LIT: {
            char *t = cg_newtemp();
            cg_emit("    Value %s = make_bool(%d);\n", t, node->as.boolean ? 1 : 0);
            return t;
        }
        case AST_NULL_LIT:
            return cg_emit_null();

        case AST_IDENT: {
            char *t = cg_newtemp();
            cg_emit("    Value %s = qs_get(env, &local, ", t);
            write_c_string(cg_out, node->as.string ? node->as.string : "");
            cg_emit(");\n");
            return t;
        }

        case AST_BINARY_ADD: case AST_BINARY_SUB: case AST_BINARY_MUL:
        case AST_BINARY_DIV: case AST_BINARY_MOD:
        case AST_BINARY_EQ: case AST_BINARY_NEQ:
        case AST_BINARY_LT: case AST_BINARY_GT:
        case AST_BINARY_LTE: case AST_BINARY_GTE:
        case AST_BINARY_IN:
            return cg_binary_op(node);
        case AST_BINARY_AND:
        case AST_BINARY_OR:
            return cg_logic_op(node);

        case AST_UNARY_NEG: {
            char *operand = cg_expr(node->as.unary.operand);
            char *t = cg_newtemp();
            cg_emit("    Value %s = qs_neg(%s);\n", t, operand);
            return t;
        }
        case AST_UNARY_NOT: {
            char *operand = cg_expr(node->as.unary.operand);
            char *t = cg_newtemp();
            cg_emit("    Value %s = qs_not(%s);\n", t, operand);
            return t;
        }

        case AST_CHAINED_CMP:
            return cg_chained(node);

        case AST_INDEX: {
            char *obj = cg_expr(node->as.index.object);
            char *key = cg_expr(node->as.index.key);
            char *t = cg_newtemp();
            cg_emit("    Value %s = qs_index(%s, %s);\n", t, obj, key);
            return t;
        }
        case AST_SLICE: {
            char *obj = cg_expr(node->as.slice.object);
            char *start = node->as.slice.start ? cg_expr(node->as.slice.start) : NULL;
            if (!start) {
                start = cg_newtemp();
                cg_emit("    Value %s = make_integer(0);\n", start);
            }
            char *end = node->as.slice.end ? cg_expr(node->as.slice.end) : NULL;
            if (!end) end = cg_emit_null();
            char *t = cg_newtemp();
            cg_emit("    Value %s = qs_slice(%s, %s, %s);\n", t, obj, start, end);
            return t;
        }

        case AST_LIST_LIT: {
            char *t = cg_newtemp();
            cg_emit("    Value %s = make_list_empty();\n", t);
            for (int i = 0; i < node->as.list.count; i++) {
                char *item = cg_expr(node->as.list.items[i]);
                cg_emit("    list_push(&%s.as.list, %s);\n", t, item);
            }
            return t;
        }
        case AST_LIST_COMP:
            return cg_list_comp(node);
        case AST_MAP_LIT: {
            char *t = cg_newtemp();
            cg_emit("    Value %s = make_dict_empty();\n", t);
            for (int i = 0; i < node->as.map.count; i++) {
                char *key = cg_expr(node->as.map.keys[i]);
                char *val = cg_expr(node->as.map.vals[i]);
                char *ks = cg_newtemp();
                cg_emit("    { char *%s = value_to_string(%s); dict_set(&%s.as.dict, %s, %s); free(%s); }\n",
                        ks, key, t, ks, val, ks);
            }
            return t;
        }
        case AST_FSTRING: {
            int count = node->as.fstring.count;
            char **parts = count > 0 ? malloc((size_t)count * sizeof(char *)) : NULL;
            for (int i = 0; i < count; i++) parts[i] = cg_expr(node->as.fstring.parts[i]);
            char *t = cg_newtemp();
            cg_emit("    Value %s = qs_concat_strings(%d, ", t, count);
            cg_emit_args_array(parts, count);
            cg_emit(");\n");
            free(parts);
            return t;
        }

        case AST_CALL:
            return cg_call_expr(node);

        case AST_METHOD_CALL:
            return cg_method_call(node->as.method_call.object, node->as.method_call.method,
                                  node->as.method_call.args, node->as.method_call.argc);

        case AST_FIELD_ACCESS: {
            char *obj = cg_expr(node->as.field_access.object);
            char *t = cg_newtemp();
            cg_emit("    Value %s = qs_get_field(env, %s, ", t, obj);
            write_c_string(cg_out, node->as.field_access.field);
            cg_emit(");\n");
            return t;
        }

        case AST_LAMBDA:
            return cg_lambda(node);

        default:
            return cg_emit_null();
    }
}

static int cg_is_mutating_method(const char *m) {
    return strcmp(m, "append") == 0 || strcmp(m, "push") == 0 ||
           strcmp(m, "pop") == 0 || strcmp(m, "remove") == 0 ||
           strcmp(m, "extend") == 0 || strcmp(m, "sort") == 0 ||
           strcmp(m, "insert") == 0 || strcmp(m, "clear") == 0 ||
           strcmp(m, "shuffle") == 0 || strcmp(m, "reverse") == 0 ||
           strcmp(m, "fill") == 0 || strcmp(m, "splice") == 0;
}

static void cg_assign(ASTNode *node) {
    const char *name = node->as.assign.name;
    char op = node->as.assign.op;
    char *value = cg_expr(node->as.assign.value);
    if (op != '=') {
        char *cur = cg_newtemp();
        cg_emit("    Value %s = qs_get(env, &local, ", cur);
        write_c_string(cg_out, name);
        cg_emit(");\n");
        const char *fn = "qs_add";
        if (op == '-') fn = "qs_sub";
        else if (op == '*') fn = "qs_mul";
        else if (op == '/') fn = "qs_div";
        else if (op == '%') fn = "qs_mod";
        char *res = cg_newtemp();
        cg_emit("    Value %s = %s(%s, %s);\n", res, fn, cur, value);
        value = res;
    }
    cg_emit("    qs_set(env, &local, ");
    write_c_string(cg_out, name);
    cg_emit(", %s);\n", value);
}

static void cg_print(ASTNode *node) {
    int argc = node->as.print.argc;
    if (argc == 0) {
        cg_emit("    printf(\"\\n\");\n");
        return;
    }
    if (argc == 1) {
        char *v = cg_expr(node->as.print.args[0]);
        cg_emit("    qs_print_value(%s);\n", v);
        return;
    }
    char **temps = malloc((size_t)argc * sizeof(char *));
    for (int i = 0; i < argc; i++) temps[i] = cg_expr(node->as.print.args[i]);
    cg_emit("    qs_print_multi(%d, ", argc);
    cg_emit_args_array(temps, argc);
    cg_emit(");\n");
    free(temps);
}

static void cg_if(ASTNode *node) {
    char *cond = cg_expr(node->as.if_stmt.cond);
    cg_emit("    if (qs_truthy(%s)) {\n", cond);
    cg_block(node->as.if_stmt.then_body);
    cg_emit("    }\n");
    if (node->as.if_stmt.elif_branch) {
        cg_emit("    else {\n");
        cg_if(node->as.if_stmt.elif_branch);
        cg_emit("    }\n");
    } else if (node->as.if_stmt.else_body) {
        cg_emit("    else {\n");
        cg_block(node->as.if_stmt.else_body);
        cg_emit("    }\n");
    }
}

static void cg_while(ASTNode *node) {
    char *save = cg_label_name("qsLtry");
    cg_emit("    { int %s = g_qs_try_top;\n", save);
    cg_loop_save[cg_loop_top++] = save;
    cg_emit("    while (1) {\n");
    char *cond = cg_expr(node->as.while_stmt.cond);
    cg_emit("        if (!qs_truthy(%s)) break;\n", cond);
    cg_block(node->as.while_stmt.body);
    cg_emit("    }\n");
    cg_loop_top--;
    cg_emit("    }\n");
}

static void cg_repeat(ASTNode *node) {
    char *save = cg_label_name("qsLtry");
    int id = cg_loop++;
    cg_emit("    { int %s = g_qs_try_top;\n", save);
    cg_loop_save[cg_loop_top++] = save;

    if (node->as.repeat.is_counted) {
        char *count = cg_expr(node->as.repeat.count_or_coll);
        cg_emit("    for (long qs_rI%d = 0; qs_rI%d < qs_to_int(%s); qs_rI%d++) {\n",
                id, id, count, id);
        cg_block(node->as.repeat.body);
        cg_emit("    }\n");
    } else {
        char *coll = cg_expr(node->as.repeat.count_or_coll);
        const char *iter_fn = node->as.repeat.index_name ? "_iter_items2" : "_iter_items";
        cg_emit("    Value qs_it%d = qs_call(env, \"%s\", (Value[]){%s}, 1);\n",
                id, iter_fn, coll);
        cg_emit("    for (size_t qs_ix%d = 0; qs_ix%d < qs_it%d.as.list.count; qs_ix%d++) {\n",
                id, id, id, id);
        if (node->as.repeat.index_name && node->as.repeat.var_name) {
            cg_emit("        Value qs_pair%d = qs_it%d.as.list.items[qs_ix%d];\n", id, id, id);
            cg_emit("        qs_set(env, &local, ");
            write_c_string(cg_out, node->as.repeat.index_name);
            cg_emit(", clone_value(qs_pair%d.as.list.items[0]));\n", id);
            cg_emit("        qs_set(env, &local, ");
            write_c_string(cg_out, node->as.repeat.var_name);
            cg_emit(", clone_value(qs_pair%d.as.list.items[1]));\n", id);
        } else if (node->as.repeat.var_name) {
            cg_emit("        qs_set(env, &local, ");
            write_c_string(cg_out, node->as.repeat.var_name);
            cg_emit(", clone_value(qs_it%d.as.list.items[qs_ix%d]));\n", id, id);
        }
        cg_block(node->as.repeat.body);
        cg_emit("    }\n");
        cg_emit("    free_value(qs_it%d);\n", id);
    }

    cg_loop_top--;
    cg_emit("    }\n");
}

static void cg_try(ASTNode *node) {
    int id = cg_loop++;
    cg_emit("    { QsTryFrame qs_try%d;\n", id);
    cg_emit("      if (QS_TRY_SET(&qs_try%d) == 0) {\n", id);
    cg_emit("        qs_push_try(&qs_try%d);\n", id);
    cg_block(node->as.try_stmt.try_body);
    cg_emit("        qs_pop_try();\n");
    cg_emit("        goto qs_try_done%d;\n", id);
    cg_emit("      } else {\n");
    if (node->as.try_stmt.err_var) {
        cg_emit("        qs_set(env, &local, ");
        write_c_string(cg_out, node->as.try_stmt.err_var);
        cg_emit(", make_string_owned(qs_strdup(g_qs_error)));\n");
    }
    if (node->as.try_stmt.oops_body) {
        cg_block(node->as.try_stmt.oops_body);
    }
    cg_emit("      }\n");
    cg_emit("    qs_try_done%d: ;\n", id);
    cg_emit("    }\n");
}

static void cg_expr_stmt(ASTNode *node) {
    ASTNode *e = node->as.unary.operand;
    if (e && e->type == AST_METHOD_CALL &&
        e->as.method_call.object && e->as.method_call.object->type == AST_IDENT &&
        cg_is_mutating_method(e->as.method_call.method)) {
        char *result = cg_method_call(e->as.method_call.object, e->as.method_call.method,
                                      e->as.method_call.args, e->as.method_call.argc);
        cg_emit("    qs_set(env, &local, ");
        write_c_string(cg_out, e->as.method_call.object->as.string);
        cg_emit(", %s);\n", result);
        return;
    }
    char *t = cg_expr(e);
    cg_emit("    free_value(%s);\n", t);
}

static void cg_stmt(ASTNode *node) {
    if (!node) return;
    switch (node->type) {
        case AST_ASSIGN:
            cg_assign(node);
            break;
        case AST_PRINT:
            cg_print(node);
            break;
        case AST_RETURN: {
            if (node->as.unary.operand) {
                char *v = cg_expr(node->as.unary.operand);
                cg_emit("    qs_ret = %s;\n", v);
            } else {
                cg_emit("    qs_ret = make_null();\n");
            }
            if (cg_in_func) {
                cg_emit("    goto qs_fn_end;\n");
            }
            break;
        }
        case AST_THROW: {
            char *v = node->as.unary.operand ? cg_expr(node->as.unary.operand) : cg_emit_null();
            cg_emit("    qs_raise_value(%s);\n", v);
            break;
        }
        case AST_BREAK:
            if (cg_loop_top > 0) cg_emit("    g_qs_try_top = %s; break;\n", cg_loop_save[cg_loop_top - 1]);
            else cg_emit("    qs_raise(\"break can only be used inside a loop\");\n");
            break;
        case AST_CONTINUE:
            if (cg_loop_top > 0) cg_emit("    g_qs_try_top = %s; continue;\n", cg_loop_save[cg_loop_top - 1]);
            else cg_emit("    qs_raise(\"continue can only be used inside a loop\");\n");
            break;
        case AST_IF:
            cg_if(node);
            break;
        case AST_WHILE:
            cg_while(node);
            break;
        case AST_REPEAT:
            cg_repeat(node);
            break;
        case AST_FIELD_ASSIGN: {
            char *obj = cg_expr(node->as.field_assign.object);
            char *val = cg_expr(node->as.field_assign.value);
            char *t = cg_newtemp();
            cg_emit("    Value %s = qs_set_field(%s, ", t, obj);
            write_c_string(cg_out, node->as.field_assign.field);
            cg_emit(", %s); free_value(%s);\n", val, t);
            break;
        }
        case AST_TRY:
            cg_try(node);
            break;
        case AST_IMPORT: {
            cg_emit("    qs_import(env, ");
            write_c_string(cg_out, node->as.string ? node->as.string : "");
            cg_emit(");\n");
            break;
        }
        case AST_EXPR_STMT:
            cg_expr_stmt(node);
            break;
        case AST_BLOCK:
            cg_block(node);
            break;
        case AST_FUNC_DEF:
        case AST_CLASS_DEF:
        case AST_METHOD_DEF:
        case AST_FIELD_DEF:
            break;
        default:
            break;
    }
}

static void cg_block(ASTNode *block) {
    if (!block || (block->type != AST_BLOCK && block->type != AST_PROGRAM)) return;
    for (int i = 0; i < block->as.block.count; i++) {
        cg_stmt(block->as.block.stmts[i]);
    }
}

static void cg_emit_func(CgFunc *f) {
    cg_emit("static Value %s(Environment *env, Value *args, int argc) {\n", f->c_name);
    cg_emit("    Environment local = {0};\n");
    cg_emit("    int qs_saved_try = g_qs_try_top;\n");
    cg_emit("    Value qs_ret = make_null();\n");
    cg_emit("    (void)qs_saved_try; (void)qs_ret; (void)argc;\n");
    {
        int off = f->is_method ? 1 : 0;
        for (int i = 0; i < f->param_count; i++) {
            const char *pt = f->param_types ? f->param_types[i] : NULL;
            if (!pt || strcmp(pt, "any") == 0) continue;
            cg_emit("    if (argc > %d) qs_check_arg(args[%d], ", i + off, i + off);
            write_c_string(cg_out, pt);
            cg_emit(", ");
            write_c_string(cg_out, f->params[i]);
            cg_emit(");\n");
        }
    }
    if (f->is_method) {
        cg_emit("    if (argc > 0) env_set(&local, \"self\", args[0]);\n");
        for (int i = 0; i < f->param_count; i++) {
            cg_emit("    if (argc > %d) env_set(&local, ", i + 1);
            write_c_string(cg_out, f->params[i]);
            cg_emit(", args[%d]);\n", i + 1);
        }
    } else {
        for (int i = 0; i < f->param_count; i++) {
            cg_emit("    if (argc > %d) env_set(&local, ", i);
            write_c_string(cg_out, f->params[i]);
            cg_emit(", args[%d]);\n", i);
        }
    }
    int saved_loop_top = cg_loop_top;
    int saved_in_func = cg_in_func;
    cg_in_func = 1;
    cg_loop_top = 0;
    cg_block(f->body);
    cg_loop_top = saved_loop_top;
    cg_in_func = saved_in_func;
    cg_emit("qs_fn_end:\n");
    cg_emit("    g_qs_try_top = qs_saved_try;\n");
    cg_emit("    return qs_ret;\n");
    cg_emit("}\n\n");
}

static int native_compile(const char *input_path, const char *output_path) {
    Program program = {0};
    if (!load_program_file(input_path, &program)) {
        fprintf(stderr, "Cannot read '%s'\n", input_path);
        return 1;
    }
    strip_block_comments(&program);
    ast_normalize_source(&program);

    ParseError ferr = {0};
    collect_functions(&program, &ferr);
    g_program = &program;

    ast_parse_failed = 0;
    ast_parse_failed_line = 0;
    ast_parse_failed_msg[0] = '\0';
    ASTNode *ast = ast_parse_program(&program);
    if (ast_parse_failed) {
        if (ast_parse_failed_msg[0] != '\0') {
            fprintf(stderr, "SyntaxError: %s\n", ast_parse_failed_msg);
        } else {
            fprintf(stderr, "SyntaxError: expected a value\n");
        }
        fprintf(stderr, "line %d, column 0\n", ast_parse_failed_line);
        ast_parse_failed = 0;
        ast_parse_failed_line = 0;
        ast_parse_failed_msg[0] = '\0';
        ast_free(ast);
        free_program(&program);
        g_program = NULL;
        return 1;
    }
    if (!ast) {
        fprintf(stderr, "native: parse error\n");
        free_program(&program);
        g_program = NULL;
        return 1;
    }

    cg_func_count = 0;
    cg_class_count = 0;
    cg_temp = 0;
    cg_label = 0;
    cg_loop = 0;
    cg_loop_top = 0;
    cg_in_func = 0;
    cg_collect(ast);

    FILE *out = fopen(output_path, "w");
    if (!out) {
        fprintf(stderr, "Cannot write '%s'\n", output_path);
        cg_func_count = 0;
        cg_class_count = 0;
        ast_free(ast);
        free_program(&program);
        g_program = NULL;
        return 1;
    }
    cg_out = out;

    cg_emit("/* Generated by the QuantoScript native (AST->C) backend. */\n");
    cg_emit("#define QUANTO_NO_MAIN\n");
    cg_emit("#if defined(__GNUC__)\n");
    cg_emit("#pragma GCC diagnostic ignored \"-Wunused-label\"\n");
    cg_emit("#pragma GCC diagnostic ignored \"-Wunused-variable\"\n");
    cg_emit("#pragma GCC diagnostic ignored \"-Wunused-parameter\"\n");
    cg_emit("#pragma GCC diagnostic ignored \"-Wunused-function\"\n");
    cg_emit("#endif\n");
    cg_emit("#include \"quanto.c\"\n\n");

    {
        size_t src_total = 0;
        for (size_t i = 0; i < program.count; i++) src_total += strlen(program.items[i]) + 1;
        char *src_text = malloc(src_total + 1);
        src_text[0] = '\0';
        for (size_t i = 0; i < program.count; i++) {
            strcat(src_text, program.items[i]);
            strcat(src_text, "\n");
        }
        cg_emit("static const char *g_qs_source =\n");
        write_c_string(cg_out, src_text);
        cg_emit(";\n\n");
        free(src_text);
    }

    cg_emit("#if defined(_WIN32)\n");
    cg_emit("#define QS_GETPID() ((unsigned long)GetCurrentProcessId())\n");
    cg_emit("#else\n");
    cg_emit("#include <unistd.h>\n");
    cg_emit("#define QS_GETPID() ((unsigned long)getpid())\n");
    cg_emit("#endif\n\n");

    cg_emit("static void qs_load_interp_functions(void) {\n");
    cg_emit("    const char *dir = getenv(\"TEMP\");\n");
    cg_emit("    if (!dir) dir = getenv(\"TMP\");\n");
    cg_emit("    if (!dir) dir = \".\";\n");
    cg_emit("    char tmp[1024];\n");
    cg_emit("    snprintf(tmp, sizeof(tmp), \"%%s/qs_native_src_%%lu.qs\", dir, QS_GETPID());\n");
    cg_emit("    FILE *f = fopen(tmp, \"w\");\n");
    cg_emit("    if (!f) return;\n");
    cg_emit("    fputs(g_qs_source, f);\n");
    cg_emit("    fclose(f);\n");
    cg_emit("    static Program qs_interp_program;\n");
    cg_emit("    if (!load_program_file(tmp, &qs_interp_program)) { remove(tmp); return; }\n");
    cg_emit("    remove(tmp);\n");
    cg_emit("    strip_block_comments(&qs_interp_program);\n");
    cg_emit("    ast_normalize_source(&qs_interp_program);\n");
    cg_emit("    ParseError qs_cf_err = {0};\n");
    cg_emit("    collect_functions(&qs_interp_program, &qs_cf_err);\n");
    cg_emit("}\n\n");

    for (int i = 0; i < cg_func_count; i++) {
        cg_emit("static Value %s(Environment *env, Value *args, int argc);\n", cg_funcs[i].c_name);
    }
    cg_emit("\n");

    cg_emit("int main(void) {\n");
    cg_emit("    object_registry_init();\n");
    cg_emit("    task_queue_init();\n");
    cg_emit("    Environment env_storage = {0};\n");
    cg_emit("    Environment *env = &env_storage;\n");
    cg_emit("    Environment local = {0};\n");
    cg_emit("    int qs_saved_try = g_qs_try_top;\n");
    cg_emit("    (void)qs_saved_try; (void)local;\n");
    cg_emit("    g_root_env = env;\n");
    cg_emit("    g_program = NULL;\n");
    cg_emit("    qs_load_interp_functions();\n");
    cg_emit("    g_vm_source_dir = ");
    if (program.dir && program.dir[0]) write_c_string(cg_out, program.dir);
    else cg_emit("NULL");
    cg_emit(";\n");

    for (int i = 0; i < cg_func_count; i++) {
        cg_emit("    qs_register(\"");
        for (const char *p = cg_funcs[i].reg_name; *p; p++) {
            if (*p == '\\' || *p == '"') cg_emit("\\%c", *p);
            else cg_emit("%c", *p);
        }
        cg_emit("\", %s, %d);\n", cg_funcs[i].c_name, cg_funcs[i].param_count);
    }

    cg_in_func = 0;
    cg_loop_top = 0;
    cg_block(ast);

    cg_emit("    g_qs_try_top = qs_saved_try;\n");
    cg_emit("    free_environment(&env_storage);\n");
    cg_emit("    g_root_env = NULL;\n");
    cg_emit("    object_registry_free_all();\n");
    cg_emit("    free_functions();\n");
    cg_emit("    free_modules();\n");
    cg_emit("    free_call_stack();\n");
    cg_emit("    task_cleanup();\n");
    cg_emit("    return 0;\n");
    cg_emit("}\n\n");

    for (int i = 0; i < cg_func_count; i++) {
        cg_emit_func(&cg_funcs[i]);
    }

    fclose(out);
    cg_out = NULL;

    for (int i = 0; i < cg_func_count; i++) {
        free(cg_funcs[i].c_name);
        free(cg_funcs[i].reg_name);
        for (int p = 0; p < cg_funcs[i].param_count; p++) free(cg_funcs[i].params[p]);
        free(cg_funcs[i].params);
        if (cg_funcs[i].param_types) {
            for (int p = 0; p < cg_funcs[i].param_count; p++) free(cg_funcs[i].param_types[p]);
            free(cg_funcs[i].param_types);
        }
    }
    for (int i = 0; i < cg_class_count; i++) free(cg_classes[i]);
    cg_func_count = 0;
    cg_class_count = 0;

    ast_free(ast);
    free_program(&program);
    g_program = NULL;
    return 0;
}

static QS_UNUSED_FN int native_build_c(const char *c_path, const char *output_path) {
    char runtime_dir[4096];
    if (!find_runtime_dir(runtime_dir, sizeof(runtime_dir))) {
        fprintf(stderr, "QuantoScript: cannot find the C runtime sources (quanto.c and src/parts).\n");
        fprintf(stderr, "  Set QS_SOURCE_DIR to the folder that holds quanto.c, or run from the project root.\n");
        return 1;
    }

    char command[8192];
#ifdef _WIN32
#if QS_HAVE_POSIX_REGEX
    const char *regex_flag = " -l:libregex.a -l:libtre.a -l:libintl.a -l:libiconv.a";
#else
    const char *regex_flag = "";
#endif
    snprintf(command, sizeof(command),
             "gcc -std=c99 -O2 -DNDEBUG -static -I\"%s\" \"%s\" -o \"%s\" -lssl -lcrypto -lws2_32 -lwinhttp -lwininet -lcrypt32 -lbcrypt%s",
             runtime_dir, c_path, output_path, regex_flag);
#else
    snprintf(command, sizeof(command),
             "gcc -std=c99 -O2 -DNDEBUG -I\"%s\" \"%s\" -o \"%s\" -lssl -lcrypto",
             runtime_dir, c_path, output_path);
#endif
    int status = system(command);
    if (status != 0) {
        fprintf(stderr, "QuantoScript: native C build failed\n");
        fprintf(stderr, "  runtime dir: %s\n", runtime_dir);
        fprintf(stderr, "  generated C kept at: %s\n", c_path);
        return 1;
    }
    printf("compiled %s -> %s (native)\n", c_path, output_path);
    return 0;
}
