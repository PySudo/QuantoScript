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

#define CG_MAX_CONSTS 1024
typedef struct {
    char *name;
    Value value;
} CgConstVar;
static CgConstVar cg_consts[CG_MAX_CONSTS];
static int cg_const_count = 0;

static void cg_const_clear_all(void) {
    for (int i = 0; i < cg_const_count; i++) {
        free(cg_consts[i].name);
        free_value(cg_consts[i].value);
    }
    cg_const_count = 0;
}

static void cg_const_clear(const char *name) {
    if (!name) return;
    for (int i = 0; i < cg_const_count; i++) {
        if (strcmp(cg_consts[i].name, name) == 0) {
            free(cg_consts[i].name);
            free_value(cg_consts[i].value);
            cg_consts[i] = cg_consts[cg_const_count - 1];
            cg_const_count--;
            return;
        }
    }
}

static void cg_const_set(const char *name, Value value) {
    if (!name) { free_value(value); return; }
    for (int i = 0; i < cg_const_count; i++) {
        if (strcmp(cg_consts[i].name, name) == 0) {
            free_value(cg_consts[i].value);
            cg_consts[i].value = value;
            return;
        }
    }
    if (cg_const_count < CG_MAX_CONSTS) {
        cg_consts[cg_const_count].name = qs_strdup(name);
        cg_consts[cg_const_count].value = value;
        cg_const_count++;
    } else {
        free_value(value);
    }
}

static Value *cg_const_get(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < cg_const_count; i++) {
        if (strcmp(cg_consts[i].name, name) == 0) return &cg_consts[i].value;
    }
    return NULL;
}

static int cg_value_scalar(const Value *v) {
    return v && (v->type == VALUE_INTEGER || v->type == VALUE_FLOAT ||
                 v->type == VALUE_BOOL || v->type == VALUE_NULL);
}

static int cg_const_scalar(const char *name) {
    return cg_value_scalar(cg_const_get(name));
}

#define CG_MAX_READS 8192
static char *cg_reads[CG_MAX_READS];
static int cg_read_count = 0;
static int cg_reads_overflow = 0;
static int cg_dse = 0;
static int cg_has_import = 0;
static int cg_has_lambda = 0;

static void cg_mark_read(const char *name) {
    if (!name || !name[0]) return;
    for (int i = 0; i < cg_read_count; i++) {
        if (strcmp(cg_reads[i], name) == 0) return;
    }
    if (cg_read_count >= CG_MAX_READS) {
        cg_reads_overflow = 1;
        return;
    }
    cg_reads[cg_read_count++] = qs_strdup(name);
}

static int cg_is_read(const char *name) {
    if (cg_reads_overflow) return 1;
    if (!name) return 1;
    for (int i = 0; i < cg_read_count; i++) {
        if (strcmp(cg_reads[i], name) == 0) return 1;
    }
    return 0;
}

static void cg_free_reads(void) {
    for (int i = 0; i < cg_read_count; i++) free(cg_reads[i]);
    cg_read_count = 0;
    cg_reads_overflow = 0;
}

#define CG_MAX_PROMO 256
static char *cg_promo[CG_MAX_PROMO];
static int cg_promo_count = 0;
static int cg_int_enabled = 0;
static int cg_is_class(const char *name);
static int cg_find_int_func(const char *name);

static int cg_promo_find(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < cg_promo_count; i++) {
        if (strcmp(cg_promo[i], name) == 0) return i;
    }
    return -1;
}

static int cg_is_promo(const char *name) {
    return cg_int_enabled && cg_promo_find(name) >= 0;
}

static void cg_promo_add(const char *name) {
    if (!name || !name[0]) return;
    if (cg_promo_find(name) >= 0) return;
    if (cg_promo_count < CG_MAX_PROMO) cg_promo[cg_promo_count++] = qs_strdup(name);
}

static void cg_promo_remove(const char *name) {
    int i = cg_promo_find(name);
    if (i < 0) return;
    free(cg_promo[i]);
    cg_promo[i] = cg_promo[cg_promo_count - 1];
    cg_promo_count--;
}

static void cg_promo_reset(void) {
    for (int i = 0; i < cg_promo_count; i++) free(cg_promo[i]);
    cg_promo_count = 0;
    cg_int_enabled = 0;
}

typedef struct { char **names; int count; int cap; } CgNameSet;
static void cg_nameset_add(CgNameSet *s, const char *name) {
    if (!name || !name[0]) return;
    for (int i = 0; i < s->count; i++) {
        if (strcmp(s->names[i], name) == 0) return;
    }
    if (s->count >= s->cap) {
        s->cap = s->cap ? s->cap * 2 : 16;
        s->names = realloc(s->names, (size_t)s->cap * sizeof(char *));
    }
    s->names[s->count++] = qs_strdup(name);
}
static int cg_nameset_has(CgNameSet *s, const char *name) {
    if (!name) return 0;
    for (int i = 0; i < s->count; i++) {
        if (strcmp(s->names[i], name) == 0) return 1;
    }
    return 0;
}
static void cg_nameset_free(CgNameSet *s) {
    for (int i = 0; i < s->count; i++) free(s->names[i]);
    free(s->names);
    s->names = NULL;
    s->count = 0;
    s->cap = 0;
}

typedef struct {
    void (*read)(void *ctx, const char *name);
    void (*write)(void *ctx, const char *name);
    void (*assign)(void *ctx, const char *name, char op, ASTNode *value);
    void (*call)(void *ctx, const char *name);
    void (*ret)(void *ctx, ASTNode *value);
    void *ctx;
    int stop_funcs;
    int unsupported;
} CgWalk;

static void cg_walk_node(ASTNode *n, CgWalk *w) {
    if (!n) return;
    switch (n->type) {
        case AST_INT_LIT: case AST_FLOAT_LIT: case AST_STR_LIT:
        case AST_BOOL_LIT: case AST_NULL_LIT:
        case AST_BREAK: case AST_CONTINUE:
        case AST_FIELD_DEF:
            break;
        case AST_IDENT:
            if (w->read) w->read(w->ctx, n->as.string);
            break;
        case AST_UNARY_NEG: case AST_UNARY_NOT:
        case AST_THROW: case AST_EXPR_STMT:
            cg_walk_node(n->as.unary.operand, w);
            break;
        case AST_RETURN:
            if (w->ret) w->ret(w->ctx, n->as.unary.operand);
            cg_walk_node(n->as.unary.operand, w);
            break;
        case AST_BINARY_ADD: case AST_BINARY_SUB: case AST_BINARY_MUL:
        case AST_BINARY_DIV: case AST_BINARY_MOD:
        case AST_BINARY_EQ: case AST_BINARY_NEQ:
        case AST_BINARY_LT: case AST_BINARY_GT:
        case AST_BINARY_LTE: case AST_BINARY_GTE:
        case AST_BINARY_AND: case AST_BINARY_OR:
        case AST_BINARY_IN:
            cg_walk_node(n->as.binary.left, w);
            cg_walk_node(n->as.binary.right, w);
            break;
        case AST_CHAINED_CMP:
            for (int i = 0; i < n->as.chained.count; i++) cg_walk_node(n->as.chained.operands[i], w);
            break;
        case AST_INDEX:
            cg_walk_node(n->as.index.object, w);
            cg_walk_node(n->as.index.key, w);
            break;
        case AST_SLICE:
            cg_walk_node(n->as.slice.object, w);
            cg_walk_node(n->as.slice.start, w);
            cg_walk_node(n->as.slice.end, w);
            break;
        case AST_CALL:
            if (w->call) w->call(w->ctx, n->as.call.name);
            for (int i = 0; i < n->as.call.argc; i++) cg_walk_node(n->as.call.args[i], w);
            break;
        case AST_METHOD_CALL:
            cg_walk_node(n->as.method_call.object, w);
            for (int i = 0; i < n->as.method_call.argc; i++) cg_walk_node(n->as.method_call.args[i], w);
            break;
        case AST_LIST_LIT:
            for (int i = 0; i < n->as.list.count; i++) cg_walk_node(n->as.list.items[i], w);
            break;
        case AST_LIST_COMP:
            cg_walk_node(n->as.comp.iterable, w);
            if (w->write) w->write(w->ctx, n->as.comp.var);
            cg_walk_node(n->as.comp.cond, w);
            cg_walk_node(n->as.comp.expr, w);
            break;
        case AST_MAP_LIT:
            for (int i = 0; i < n->as.map.count; i++) {
                cg_walk_node(n->as.map.keys[i], w);
                cg_walk_node(n->as.map.vals[i], w);
            }
            break;
        case AST_FSTRING:
            for (int i = 0; i < n->as.fstring.count; i++) cg_walk_node(n->as.fstring.parts[i], w);
            break;
        case AST_LAMBDA:
            w->unsupported = 1;
            break;
        case AST_ASSIGN:
            if (n->as.assign.op != '=' && w->read) w->read(w->ctx, n->as.assign.name);
            cg_walk_node(n->as.assign.value, w);
            if (w->write) w->write(w->ctx, n->as.assign.name);
            if (w->assign) w->assign(w->ctx, n->as.assign.name, n->as.assign.op, n->as.assign.value);
            break;
        case AST_PRINT:
            for (int i = 0; i < n->as.print.argc; i++) cg_walk_node(n->as.print.args[i], w);
            break;
        case AST_IF:
            cg_walk_node(n->as.if_stmt.cond, w);
            cg_walk_node(n->as.if_stmt.then_body, w);
            cg_walk_node(n->as.if_stmt.elif_branch, w);
            cg_walk_node(n->as.if_stmt.else_body, w);
            break;
        case AST_WHILE:
            cg_walk_node(n->as.while_stmt.cond, w);
            cg_walk_node(n->as.while_stmt.body, w);
            break;
        case AST_REPEAT:
            cg_walk_node(n->as.repeat.count_or_coll, w);
            if (w->write) w->write(w->ctx, n->as.repeat.index_name);
            if (w->write) w->write(w->ctx, n->as.repeat.var_name);
            cg_walk_node(n->as.repeat.body, w);
            break;
        case AST_TRY:
            w->unsupported = 1;
            cg_walk_node(n->as.try_stmt.try_body, w);
            if (w->write) w->write(w->ctx, n->as.try_stmt.err_var);
            cg_walk_node(n->as.try_stmt.oops_body, w);
            break;
        case AST_IMPORT:
            w->unsupported = 1;
            break;
        case AST_BLOCK: case AST_PROGRAM:
            for (int i = 0; i < n->as.block.count; i++) cg_walk_node(n->as.block.stmts[i], w);
            break;
        case AST_FUNC_DEF:
            if (!w->stop_funcs) cg_walk_node(n->as.func_def.body, w);
            break;
        case AST_METHOD_DEF:
            if (!w->stop_funcs) cg_walk_node(n->as.method_def.body, w);
            break;
        case AST_CLASS_DEF:
            w->unsupported = 1;
            break;
        case AST_NEW:
            w->unsupported = 1;
            for (int i = 0; i < n->as.new_expr.argc; i++) cg_walk_node(n->as.new_expr.args[i], w);
            break;
        case AST_FIELD_ACCESS:
            w->unsupported = 1;
            cg_walk_node(n->as.field_access.object, w);
            break;
        case AST_FIELD_ASSIGN:
            w->unsupported = 1;
            cg_walk_node(n->as.field_assign.object, w);
            cg_walk_node(n->as.field_assign.value, w);
            break;
        default:
            w->unsupported = 1;
            break;
    }
}

static void cg_nameset_add_cb(void *ctx, const char *name) {
    cg_nameset_add((CgNameSet *)ctx, name);
}

typedef struct { char *name; char op; ASTNode *rhs; } CgAssign;
static CgAssign *cg_assign_list = NULL;
static int cg_assign_list_count = 0;
static int cg_assign_list_cap = 0;

static void cg_assign_list_free(void) {
    for (int i = 0; i < cg_assign_list_count; i++) free(cg_assign_list[i].name);
    cg_assign_list_count = 0;
}

static void cg_assign_cb(void *ctx, const char *name, char op, ASTNode *value) {
    (void)ctx;
    if (!name) return;
    if (cg_assign_list_count >= cg_assign_list_cap) {
        cg_assign_list_cap = cg_assign_list_cap ? cg_assign_list_cap * 2 : 32;
        cg_assign_list = realloc(cg_assign_list, (size_t)cg_assign_list_cap * sizeof(CgAssign));
    }
    cg_assign_list[cg_assign_list_count].name = qs_strdup(name);
    cg_assign_list[cg_assign_list_count].op = op;
    cg_assign_list[cg_assign_list_count].rhs = value;
    cg_assign_list_count++;
}

static int cg_expr_is_int(ASTNode *n) {
    if (!n) return 0;
    switch (n->type) {
        case AST_INT_LIT:
            return 1;
        case AST_IDENT:
            return cg_promo_find(n->as.string) >= 0;
        case AST_UNARY_NEG:
            return cg_expr_is_int(n->as.unary.operand);
        case AST_BINARY_ADD: case AST_BINARY_SUB: case AST_BINARY_MUL:
            return cg_expr_is_int(n->as.binary.left) && cg_expr_is_int(n->as.binary.right);
        case AST_BINARY_DIV: case AST_BINARY_MOD: {
            ASTNode *r = n->as.binary.right;
            if (!r || r->type != AST_INT_LIT || r->as.integer == 0) return 0;
            return cg_expr_is_int(n->as.binary.left);
        }
        case AST_CALL: {
            int fi = cg_find_int_func(n->as.call.name);
            if (fi < 0) return 0;
            if (n->as.call.argc != cg_funcs[fi].param_count) return 0;
            for (int i = 0; i < n->as.call.argc; i++) {
                if (!cg_expr_is_int(n->as.call.args[i])) return 0;
            }
            return 1;
        }
        default:
            return 0;
    }
}

static char *cg_promo_cname(const char *name) {
    size_t len = strlen(name) + 3;
    char *out = malloc(len);
    snprintf(out, len, "v_%s", name);
    return out;
}

static char *cg_int_expr(ASTNode *n) {
    if (!n) return NULL;
    switch (n->type) {
        case AST_INT_LIT: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%ldL", n->as.integer);
            return qs_strdup(buf);
        }
        case AST_IDENT:
            if (cg_promo_find(n->as.string) >= 0) return cg_promo_cname(n->as.string);
            return NULL;
        case AST_UNARY_NEG: {
            char *inner = cg_int_expr(n->as.unary.operand);
            if (!inner) return NULL;
            size_t len = strlen(inner) + 4;
            char *out = malloc(len);
            snprintf(out, len, "(-%s)", inner);
            free(inner);
            return out;
        }
        case AST_BINARY_ADD: case AST_BINARY_SUB: case AST_BINARY_MUL:
        case AST_BINARY_DIV: case AST_BINARY_MOD: {
            char *l = cg_int_expr(n->as.binary.left);
            if (!l) return NULL;
            char *r = cg_int_expr(n->as.binary.right);
            if (!r) { free(l); return NULL; }
            const char *op = "+";
            switch (n->type) {
                case AST_BINARY_SUB: op = "-"; break;
                case AST_BINARY_MUL: op = "*"; break;
                case AST_BINARY_DIV: op = "/"; break;
                case AST_BINARY_MOD: op = "%"; break;
                default: break;
            }
            size_t len = strlen(l) + strlen(r) + 8;
            char *out = malloc(len);
            snprintf(out, len, "(%s %s %s)", l, op, r);
            free(l);
            free(r);
            return out;
        }
        case AST_CALL: {
            int fi = cg_find_int_func(n->as.call.name);
            if (fi < 0 || n->as.call.argc != cg_funcs[fi].param_count) return NULL;
            size_t cap = 64;
            char *out = malloc(cap);
            int len = snprintf(out, cap, "qs_int_%d(", fi);
            for (int i = 0; i < n->as.call.argc; i++) {
                char *a = cg_int_expr(n->as.call.args[i]);
                if (!a) { free(out); return NULL; }
                size_t need = (size_t)len + strlen(a) + 4;
                if (need > cap) {
                    cap = need * 2;
                    out = realloc(out, cap);
                }
                if (i) { out[len++] = ','; out[len++] = ' '; }
                memcpy(out + len, a, strlen(a));
                len += (int)strlen(a);
                free(a);
            }
            out[len++] = ')';
            out[len] = '\0';
            return out;
        }
        default:
            return NULL;
    }
}

static int cg_is_func_name(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < cg_func_count; i++) {
        if (cg_funcs[i].reg_name && strcmp(cg_funcs[i].reg_name, name) == 0) return 1;
    }
    return 0;
}

static int cg_node_reads_name(ASTNode *n, const char *name) {
    if (!n) return 0;
    CgNameSet s = {0};
    CgWalk w = {0};
    w.stop_funcs = 1;
    w.read = cg_nameset_add_cb;
    w.ctx = &s;
    cg_walk_node(n, &w);
    int found = cg_nameset_has(&s, name);
    cg_nameset_free(&s);
    return found;
}

static void cg_analyze_int(ASTNode *root) {
    cg_promo_reset();
    cg_assign_list_free();
    if (!root) return;
    if (cg_has_lambda || cg_has_import) return;
    if (!(root->type == AST_PROGRAM || root->type == AST_BLOCK)) return;

    CgNameSet blocked = {0};
    {
        CgWalk bw = {0};
        bw.stop_funcs = 0;
        bw.read = cg_nameset_add_cb;
        bw.write = cg_nameset_add_cb;
        bw.ctx = &blocked;
        for (int i = 0; i < root->as.block.count; i++) {
            ASTNode *s = root->as.block.stmts[i];
            if (!s) continue;
            if (s->type == AST_FUNC_DEF) {
                cg_walk_node(s->as.func_def.body, &bw);
            } else if (s->type == AST_METHOD_DEF) {
                cg_walk_node(s->as.method_def.body, &bw);
            } else if (s->type == AST_CLASS_DEF) {
                for (int m = 0; m < s->as.class_def.method_count; m++) {
                    ASTNode *md = s->as.class_def.methods[m];
                    if (md) cg_walk_node(md->as.method_def.body, &bw);
                }
            }
        }
    }

    CgWalk tw = {0};
    tw.stop_funcs = 1;
    tw.assign = cg_assign_cb;
    cg_walk_node(root, &tw);
    if (tw.unsupported) { cg_nameset_free(&blocked); return; }

    CgNameSet cand = {0};
    for (int i = 0; i < cg_assign_list_count; i++) {
        cg_nameset_add(&cand, cg_assign_list[i].name);
    }

    int changed = 1;
    while (changed) {
        changed = 0;
        for (int c = 0; c < cand.count; c++) {
            const char *nm = cand.names[c];
            if (cg_promo_find(nm) >= 0) continue;
            if (cg_nameset_has(&blocked, nm)) continue;
            if (is_define_name(nm)) continue;
            if (cg_is_func_name(nm) || cg_is_class(nm)) continue;
            int ok = 1, any = 0;
            for (int i = 0; i < cg_assign_list_count; i++) {
                if (strcmp(cg_assign_list[i].name, nm) != 0) continue;
                any = 1;
                ASTNode *rhs = cg_assign_list[i].rhs;
                char op = cg_assign_list[i].op;
                int ri = cg_expr_is_int(rhs);
                if (ri && op != '=' && (op == '/' || op == '%')) {
                    if (!rhs || rhs->type != AST_INT_LIT || rhs->as.integer == 0) ri = 0;
                }
                if (!ri) { ok = 0; break; }
            }
            if (any && ok) { cg_promo_add(nm); changed = 1; }
        }
    }

    CgNameSet decided = {0};
    for (int i = 0; i < root->as.block.count; i++) {
        ASTNode *s = root->as.block.stmts[i];
        if (!s) continue;
        if (s->type == AST_FUNC_DEF || s->type == AST_METHOD_DEF ||
            s->type == AST_CLASS_DEF || s->type == AST_FIELD_DEF) continue;
        CgNameSet names = {0};
        {
            CgWalk nw = {0};
            nw.stop_funcs = 1;
            nw.read = cg_nameset_add_cb;
            nw.write = cg_nameset_add_cb;
            nw.ctx = &names;
            cg_walk_node(s, &nw);
        }
        for (int k = 0; k < names.count; k++) {
            const char *nm = names.names[k];
            if (cg_nameset_has(&decided, nm)) continue;
            cg_nameset_add(&decided, nm);
            if (cg_promo_find(nm) < 0) continue;
            int good = (s->type == AST_ASSIGN && strcmp(s->as.assign.name, nm) == 0 &&
                        s->as.assign.op == '=' &&
                        !cg_node_reads_name(s->as.assign.value, nm));
            if (!good) cg_promo_remove(nm);
        }
        cg_nameset_free(&names);
    }

    cg_nameset_free(&decided);
    cg_nameset_free(&cand);
    cg_nameset_free(&blocked);
    cg_int_enabled = (cg_promo_count > 0);
}

static int cg_fi_int[CG_MAX_FUNCS];
static char **cg_fi_promo[CG_MAX_FUNCS];
static int cg_fi_promo_count[CG_MAX_FUNCS];
static int cg_int_func_ret = 0;

static int cg_promo_save_count = 0;
static int cg_promo_save_enabled = 0;
static char *cg_promo_save[CG_MAX_PROMO];

static void cg_promo_push(void) {
    cg_promo_save_count = cg_promo_count;
    cg_promo_save_enabled = cg_int_enabled;
    for (int i = 0; i < cg_promo_count; i++) cg_promo_save[i] = cg_promo[i];
    cg_promo_count = 0;
    cg_int_enabled = 0;
}

static void cg_promo_pop(void) {
    cg_promo_count = cg_promo_save_count;
    cg_int_enabled = cg_promo_save_enabled;
    for (int i = 0; i < cg_promo_save_count; i++) cg_promo[i] = cg_promo_save[i];
}

static int cg_func_index(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < cg_func_count; i++) {
        if (cg_funcs[i].reg_name && strcmp(cg_funcs[i].reg_name, name) == 0) return i;
    }
    return -1;
}

static int cg_find_int_func(const char *name) {
    int i = cg_func_index(name);
    return (i >= 0 && cg_fi_int[i]) ? i : -1;
}

static void cg_fi_free(void) {
    for (int i = 0; i < CG_MAX_FUNCS; i++) {
        if (cg_fi_promo[i]) {
            for (int j = 0; j < cg_fi_promo_count[i]; j++) free(cg_fi_promo[i][j]);
            free(cg_fi_promo[i]);
            cg_fi_promo[i] = NULL;
        }
        cg_fi_promo_count[i] = 0;
        cg_fi_int[i] = 0;
    }
}

static int cg_func_body_safe(ASTNode *n, int *func_int) {
    if (!n) return 1;
    switch (n->type) {
        case AST_INT_LIT: case AST_BOOL_LIT:
        case AST_BREAK: case AST_CONTINUE:
            return 1;
        case AST_IDENT:
            return cg_promo_find(n->as.string) >= 0;
        case AST_UNARY_NEG: case AST_UNARY_NOT:
        case AST_RETURN: case AST_THROW: case AST_EXPR_STMT:
            return cg_func_body_safe(n->as.unary.operand, func_int);
        case AST_BINARY_ADD: case AST_BINARY_SUB: case AST_BINARY_MUL:
        case AST_BINARY_DIV: case AST_BINARY_MOD:
        case AST_BINARY_EQ: case AST_BINARY_NEQ:
        case AST_BINARY_LT: case AST_BINARY_GT:
        case AST_BINARY_LTE: case AST_BINARY_GTE:
        case AST_BINARY_AND: case AST_BINARY_OR:
            return cg_func_body_safe(n->as.binary.left, func_int) &&
                   cg_func_body_safe(n->as.binary.right, func_int);
        case AST_CALL: {
            int idx = cg_func_index(n->as.call.name);
            if (idx < 0 || !func_int[idx]) return 0;
            if (n->as.call.argc != cg_funcs[idx].param_count) return 0;
            for (int i = 0; i < n->as.call.argc; i++) {
                if (!cg_func_body_safe(n->as.call.args[i], func_int)) return 0;
            }
            return 1;
        }
        case AST_ASSIGN:
            if (cg_promo_find(n->as.assign.name) < 0) return 0;
            return cg_func_body_safe(n->as.assign.value, func_int);
        case AST_IF:
            return cg_func_body_safe(n->as.if_stmt.cond, func_int) &&
                   cg_func_body_safe(n->as.if_stmt.then_body, func_int) &&
                   cg_func_body_safe(n->as.if_stmt.elif_branch, func_int) &&
                   cg_func_body_safe(n->as.if_stmt.else_body, func_int);
        case AST_WHILE:
            return cg_func_body_safe(n->as.while_stmt.cond, func_int) &&
                   cg_func_body_safe(n->as.while_stmt.body, func_int);
        case AST_BLOCK: case AST_PROGRAM:
            for (int i = 0; i < n->as.block.count; i++) {
                if (!cg_func_body_safe(n->as.block.stmts[i], func_int)) return 0;
            }
            return 1;
        default:
            return 0;
    }
}

typedef struct { int count; int ok; } CgRetScan;
static void cg_ret_cb(void *ctx, ASTNode *value) {
    CgRetScan *rs = (CgRetScan *)ctx;
    rs->count++;
    if (!value || !cg_expr_is_int(value)) rs->ok = 0;
}

static int cg_node_returns(ASTNode *n) {
    if (!n) return 0;
    switch (n->type) {
        case AST_RETURN:
            return 1;
        case AST_BLOCK: case AST_PROGRAM:
            for (int i = 0; i < n->as.block.count; i++) {
                if (cg_node_returns(n->as.block.stmts[i])) return 1;
            }
            return 0;
        case AST_IF:
            if (n->as.if_stmt.elif_branch || !n->as.if_stmt.else_body) return 0;
            return cg_node_returns(n->as.if_stmt.then_body) &&
                   cg_node_returns(n->as.if_stmt.else_body);
        default:
            return 0;
    }
}

static int cg_func_is_int_impl(int fi, int *func_int, char ***out_names, int *out_count) {
    CgFunc *f = &cg_funcs[fi];
    *out_names = NULL;
    *out_count = 0;
    if (f->is_method || !f->body) return 0;

    cg_promo_reset();
    cg_assign_list_free();
    for (int i = 0; i < f->param_count; i++) cg_promo_add(f->params[i]);

    {
        CgWalk w = {0};
        w.stop_funcs = 1;
        w.assign = cg_assign_cb;
        cg_walk_node(f->body, &w);
        if (w.unsupported) return 0;
    }

    CgNameSet cand = {0};
    for (int i = 0; i < cg_assign_list_count; i++) cg_nameset_add(&cand, cg_assign_list[i].name);

    int changed = 1;
    while (changed) {
        changed = 0;
        for (int c = 0; c < cand.count; c++) {
            const char *nm = cand.names[c];
            if (cg_promo_find(nm) >= 0) continue;
            if (is_define_name(nm)) continue;
            if (cg_is_func_name(nm) || cg_is_class(nm)) continue;
            int ok = 1, any = 0;
            for (int i = 0; i < cg_assign_list_count; i++) {
                if (strcmp(cg_assign_list[i].name, nm) != 0) continue;
                any = 1;
                ASTNode *rhs = cg_assign_list[i].rhs;
                char op = cg_assign_list[i].op;
                int ri = cg_expr_is_int(rhs);
                if (ri && op != '=' && (op == '/' || op == '%')) {
                    if (!rhs || rhs->type != AST_INT_LIT || rhs->as.integer == 0) ri = 0;
                }
                if (!ri) { ok = 0; break; }
            }
            if (any && ok) { cg_promo_add(nm); changed = 1; }
        }
    }

    for (int i = 0; i < f->param_count; i++) {
        const char *p = f->params[i];
        for (int a = 0; a < cg_assign_list_count; a++) {
            if (strcmp(cg_assign_list[a].name, p) != 0) continue;
            ASTNode *rhs = cg_assign_list[a].rhs;
            char op = cg_assign_list[a].op;
            int ri = cg_expr_is_int(rhs);
            if (ri && op != '=' && (op == '/' || op == '%')) {
                if (!rhs || rhs->type != AST_INT_LIT || rhs->as.integer == 0) ri = 0;
            }
            if (!ri) { cg_nameset_free(&cand); return 0; }
        }
    }

    int all_ok = 1;
    {
        CgRetScan rs = {0, 1};
        CgWalk w = {0};
        w.stop_funcs = 1;
        w.ret = cg_ret_cb;
        w.ctx = &rs;
        cg_walk_node(f->body, &w);
        if (rs.count == 0 || !rs.ok) all_ok = 0;
    }
    if (all_ok && !cg_func_body_safe(f->body, func_int)) all_ok = 0;
    if (all_ok && !cg_node_returns(f->body)) all_ok = 0;
    if (!all_ok) { cg_nameset_free(&cand); return 0; }

    *out_names = malloc((size_t)cg_promo_count * sizeof(char *));
    *out_count = cg_promo_count;
    for (int i = 0; i < cg_promo_count; i++) (*out_names)[i] = qs_strdup(cg_promo[i]);
    cg_nameset_free(&cand);
    return 1;
}

static void cg_analyze_int_funcs(void) {
    cg_fi_free();
    for (int i = 0; i < cg_func_count; i++) {
        cg_fi_int[i] = (!cg_funcs[i].is_method && cg_funcs[i].body) ? 1 : 0;
    }
    int changed = 1;
    while (changed) {
        changed = 0;
        for (int i = 0; i < cg_func_count; i++) {
            if (!cg_fi_int[i]) continue;
            if (cg_fi_promo[i]) {
                for (int j = 0; j < cg_fi_promo_count[i]; j++) free(cg_fi_promo[i][j]);
                free(cg_fi_promo[i]);
                cg_fi_promo[i] = NULL;
                cg_fi_promo_count[i] = 0;
            }
            char **names = NULL;
            int count = 0;
            if (cg_func_is_int_impl(i, cg_fi_int, &names, &count)) {
                cg_fi_promo[i] = names;
                cg_fi_promo_count[i] = count;
            } else {
                cg_fi_int[i] = 0;
                free(names);
                changed = 1;
            }
        }
    }
    cg_promo_reset();
    cg_assign_list_free();
}

static void cg_invalidate_assignments(ASTNode *node);

static void cg_stmt(ASTNode *node);
static void cg_block(ASTNode *block);
static char *cg_expr(ASTNode *node);
static char *cg_method_call(ASTNode *object, const char *method, ASTNode **args, int argc);
static int cg_ptr_ident(ASTNode *node);
static char *cg_ident_ptr(ASTNode *node);
static int cg_is_mutating_method(const char *m);

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
    if (cg_int_enabled) {
        int code = -1;
        switch (node->type) {
            case AST_BINARY_EQ: code = 0; break;
            case AST_BINARY_NEQ: code = 1; break;
            case AST_BINARY_LT: code = 2; break;
            case AST_BINARY_GT: code = 3; break;
            case AST_BINARY_LTE: code = 4; break;
            case AST_BINARY_GTE: code = 5; break;
            default: break;
        }
        if (code >= 0) {
            if (cg_expr_is_int(node->as.binary.left) && cg_expr_is_int(node->as.binary.right)) {
                static const char *cops[6] = { "==", "!=", "<", ">", "<=", ">=" };
                char *l = cg_int_expr(node->as.binary.left);
                char *r = cg_int_expr(node->as.binary.right);
                char *t = cg_newtemp();
                cg_emit("    Value %s = make_bool((%s) %s (%s));\n", t, l, cops[code], r);
                free(l);
                free(r);
                return t;
            }
            if (cg_expr_is_int(node->as.binary.left) || cg_expr_is_int(node->as.binary.right)) {
                char *left = cg_expr(node->as.binary.left);
                char *right = cg_expr(node->as.binary.right);
                char *t = cg_newtemp();
                cg_emit("    Value %s = qs_bool_cmp(%s, %s, %d);\n", t, left, right, code);
                return t;
            }
        }
    }
    if (node->type == AST_BINARY_IN && cg_ptr_ident(node->as.binary.left)) {
        ASTNode *cont = node->as.binary.left;
        char *right = cg_expr(node->as.binary.right);
        cg_const_clear_all();
        char *p = cg_ident_ptr(cont);
        char *t = cg_newtemp();
        cg_emit("    Value %s = %s ? qs_in_ref(%s, %s) : qs_in(qs_get(env, &local, ", t, p, p, right);
        write_c_string(cg_out, cont->as.string);
        cg_emit("), %s);\n", right);
        return t;
    }
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
    cg_has_lambda = 1;
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
    cg_mark_read(node->as.call.name);

    if (argc == 1 && strcmp(node->as.call.name, "len") == 0 &&
        cg_func_index("len") < 0 && !cg_is_class("len")) {
        ASTNode *arg_node = node->as.call.args[0];
        char *t = cg_newtemp();
        if (cg_ptr_ident(arg_node)) {
            char *p = cg_ident_ptr(arg_node);
            cg_emit("    Value %s = %s ? qs_len_ref(%s) : qs_len_value(qs_get(env, &local, ", t, p, p);
            write_c_string(cg_out, arg_node->as.string);
            cg_emit("));\n");
            cg_const_clear_all();
        } else {
            char *arg = cg_expr(arg_node);
            cg_const_clear_all();
            cg_emit("    Value %s = qs_len_value(%s);\n", t, arg);
        }
        return t;
    }

    {
        int fi = cg_find_int_func(node->as.call.name);
        if (fi >= 0 && argc == cg_funcs[fi].param_count) {
            char *ie = cg_int_expr(node);
            if (ie) {
                char *tt = cg_newtemp();
                cg_emit("    Value %s = make_integer(%s);\n", tt, ie);
                free(ie);
                return tt;
            }
        }
    }

    char **temps = argc > 0 ? malloc((size_t)argc * sizeof(char *)) : NULL;
    for (int i = 0; i < argc; i++) temps[i] = cg_expr(node->as.call.args[i]);

    cg_const_clear_all();
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

typedef struct {
    const char *name;
    int argc;
    const char *helper;
    const char *receiver;
} CgFastMethod;

static const CgFastMethod cg_fast_methods[] = {
    {"has", 1, "qs_dict_has", "VALUE_DICT"},
    {"get", 1, "qs_dict_get", "VALUE_DICT"},
    {"get", 2, "qs_dict_get_or", "VALUE_DICT"},
    {"keys", 0, "qs_dict_keys", "VALUE_DICT"},
    {"values", 0, "qs_dict_values", "VALUE_DICT"},
    {"items", 0, "qs_dict_items", "VALUE_DICT"},
    {"split", 1, "qs_str_split", "VALUE_STRING"},
    {"replace", 2, "qs_str_replace", "VALUE_STRING"},
    {"substring", 1, "qs_str_substring", "VALUE_STRING"},
    {"substring", 2, "qs_str_substring2", "VALUE_STRING"},
    {"indexOf", 1, "qs_str_index_of", "VALUE_STRING"},
    {"startsWith", 1, "qs_str_starts_with", "VALUE_STRING"},
    {"endsWith", 1, "qs_str_ends_with", "VALUE_STRING"},
    {"trimStart", 0, "qs_str_trim_start", "VALUE_STRING"},
    {"trimEnd", 0, "qs_str_trim_end", "VALUE_STRING"},
    {"title", 0, "qs_str_title", "VALUE_STRING"},
    {"reverse", 0, "qs_str_reverse", "VALUE_STRING"},
    {"repeat", 1, "qs_str_repeat", "VALUE_STRING"},
    {"contains", 1, "qs_str_contains", "VALUE_STRING"},
    {"join", 1, "qs_str_join", "VALUE_STRING"},
    {NULL, 0, NULL, NULL}
};

static const CgFastMethod *cg_fast_method_find(const char *name, int argc) {
    for (int i = 0; cg_fast_methods[i].name; i++) {
        if (cg_fast_methods[i].argc == argc && strcmp(cg_fast_methods[i].name, name) == 0) {
            return &cg_fast_methods[i];
        }
    }
    return NULL;
}

static int cg_ptr_ident(ASTNode *node) {
    return node && node->type == AST_IDENT && node->as.string && node->as.string[0] &&
               !cg_is_promo(node->as.string) && !cg_const_scalar(node->as.string);
}

static char *cg_ident_ptr(ASTNode *node) {
    cg_mark_read(node->as.string);
    char *p = cg_newtemp();
    cg_emit("    Value *%s = qs_get_ptr(env, &local, ", p);
    write_c_string(cg_out, node->as.string);
    cg_emit(");\n");
    return p;
}

static char *cg_method_call(ASTNode *object, const char *method, ASTNode **args, int argc) {
    const CgFastMethod *fast = cg_fast_method_find(method, argc);
    int by_ptr = fast && cg_ptr_ident(object);
    char *obj = by_ptr ? NULL : cg_expr(object);
    char **temps = argc > 0 ? malloc((size_t)argc * sizeof(char *)) : NULL;
    for (int i = 0; i < argc; i++) temps[i] = cg_expr(args[i]);

    cg_const_clear_all();
    char *ptr = by_ptr ? cg_ident_ptr(object) : NULL;
    char *t = cg_newtemp();
    if (fast) {
        if (by_ptr) {
            cg_emit("    Value %s = (%s && %s->type == %s", t, ptr, ptr, fast->receiver);
            if (strcmp(fast->helper, "qs_str_join") == 0) {
                cg_emit(" && %s.type == VALUE_LIST", temps[0]);
            }
            cg_emit(") ? %s(%s", fast->helper, ptr);
            for (int i = 0; i < argc; i++) cg_emit(", %s", temps[i]);
            cg_emit(") : qs_method_call(env, qs_get(env, &local, ");
            write_c_string(cg_out, object->as.string);
            cg_emit("), ");
            write_c_string(cg_out, method);
            cg_emit(", ");
            cg_emit_args_array(temps, argc);
            cg_emit(", %d);\n", argc);
        } else {
            cg_emit("    Value %s;\n", t);
            cg_emit("    if (%s.type == %s", obj, fast->receiver);
            if (strcmp(fast->helper, "qs_str_join") == 0) {
                cg_emit(" && %s.type == VALUE_LIST", temps[0]);
            }
            cg_emit(") { %s = %s(&%s", t, fast->helper, obj);
            for (int i = 0; i < argc; i++) cg_emit(", %s", temps[i]);
            cg_emit("); free_value(%s); } else { %s = qs_method_call(env, %s, ", obj, t, obj);
            write_c_string(cg_out, method);
            cg_emit(", ");
            cg_emit_args_array(temps, argc);
            cg_emit(", %d); }\n", argc);
        }
    } else {
        cg_emit("    Value %s = qs_method_call(env, %s, ", t, obj);
        write_c_string(cg_out, method);
        cg_emit(", ");
        cg_emit_args_array(temps, argc);
        cg_emit(", %d);\n", argc);
    }
    free(temps);
    return t;
}

static int cg_is_pure_builtin(const char *name) {
    if (!name) return 0;
    static const char *pure[] = {
        "type", "abs", "len", "isinstance", "str", "morph",
        "range", "pow", "sqrt", "sin", "cos", "tan",
        "log", "log2", "log10", "ceil", "floor",
        "_iter_items", "_iter_items2", NULL
    };
    for (int i = 0; pure[i]; i++) {
        if (strcmp(name, pure[i]) == 0) return 1;
    }
    return 0;
}

#define CG_MAX_FOLD_ELEMS 256
#define CG_MAX_FOLD_STR 8192

static int cg_value_small(Value v) {
    if (v.type == VALUE_STRING) {
        return !v.as.string || strlen(v.as.string) <= CG_MAX_FOLD_STR;
    }
    if (v.type == VALUE_LIST) {
        if (v.as.list.count > CG_MAX_FOLD_ELEMS) return 0;
        for (size_t i = 0; i < v.as.list.count; i++) {
            if (!cg_value_small(v.as.list.items[i])) return 0;
        }
        return 1;
    }
    if (v.type == VALUE_DICT) {
        if (v.as.dict.count > CG_MAX_FOLD_ELEMS) return 0;
        for (size_t i = 0; i < v.as.dict.count; i++) {
            if (v.as.dict.entries[i].key &&
                strlen(v.as.dict.entries[i].key) > CG_MAX_FOLD_STR) return 0;
            if (!cg_value_small(v.as.dict.entries[i].value)) return 0;
        }
        return 1;
    }
    return 1;
}

static int cg_value_finite(Value v) {
    if (v.type == VALUE_FLOAT) {
        return isfinite(v.as.floating);
    }
    if (v.type == VALUE_LIST) {
        for (size_t i = 0; i < v.as.list.count; i++) {
            if (!cg_value_finite(v.as.list.items[i])) return 0;
        }
    } else if (v.type == VALUE_DICT) {
        for (size_t i = 0; i < v.as.dict.count; i++) {
            if (!cg_value_finite(v.as.dict.entries[i].value)) return 0;
        }
    }
    return 1;
}

static int cg_runtime_index(Value obj, Value key, Value *out) {
    QsTryFrame frame;
    if (QS_TRY_SET(&frame) == 0) {
        qs_push_try(&frame);
        Value r = qs_index(obj, key);
        qs_pop_try();
        *out = r;
        return 1;
    }
    return 0;
}

static void cg_emit_value_literal(char *t, Value v);

static char *cg_const_value_temp(Value v) {
    char *t = cg_newtemp();
    cg_emit_value_literal(t, v);
    return t;
}

static void cg_emit_value_literal(char *t, Value v) {
    switch (v.type) {
        case VALUE_INTEGER:
            cg_emit("    Value %s = make_integer(%ldL);\n", t, v.as.integer);
            break;
        case VALUE_FLOAT:
            cg_emit("    Value %s = make_float(%.17g);\n", t, v.as.floating);
            break;
        case VALUE_BOOL:
            cg_emit("    Value %s = make_bool(%d);\n", t, v.as.boolean ? 1 : 0);
            break;
        case VALUE_STRING:
            cg_emit("    Value %s = make_string_owned(qs_strdup(", t);
            write_c_string(cg_out, v.as.string ? v.as.string : "");
            cg_emit("));\n");
            break;
        case VALUE_LIST:
            cg_emit("    Value %s = make_list_empty();\n", t);
            for (size_t i = 0; i < v.as.list.count; i++) {
                char *item = cg_const_value_temp(v.as.list.items[i]);
                cg_emit("    list_push(&%s.as.list, %s);\n", t, item);
                free(item);
            }
            break;
        case VALUE_DICT:
            cg_emit("    Value %s = make_dict_empty();\n", t);
            for (size_t i = 0; i < v.as.dict.count; i++) {
                char *val = cg_const_value_temp(v.as.dict.entries[i].value);
                cg_emit("    dict_set(&%s.as.dict, ", t);
                write_c_string(cg_out, v.as.dict.entries[i].key);
                cg_emit(", %s);\n", val);
                free(val);
            }
            break;
        case VALUE_NULL:
        default:
            cg_emit("    Value %s = make_null();\n", t);
            break;
    }
}

static int cg_const_eval(ASTNode *node, Value *out) {
    if (!node) return 0;
    switch (node->type) {
        case AST_INT_LIT:
            *out = make_integer(node->as.integer);
            return 1;
        case AST_FLOAT_LIT:
            *out = make_float(node->as.floating);
            return 1;
        case AST_STR_LIT:
            *out = make_string_owned(qs_strdup(node->as.string ? node->as.string : ""));
            return 1;
        case AST_BOOL_LIT:
            *out = make_bool(node->as.boolean ? 1 : 0);
            return 1;
        case AST_NULL_LIT:
            *out = make_null();
            return 1;
        case AST_UNARY_NEG: {
            Value v;
            if (!cg_const_eval(node->as.unary.operand, &v)) return 0;
            *out = qs_neg(v);
            return 1;
        }
        case AST_UNARY_NOT: {
            Value v;
            if (!cg_const_eval(node->as.unary.operand, &v)) return 0;
            *out = qs_not(v);
            return 1;
        }
        case AST_BINARY_ADD:
        case AST_BINARY_SUB:
        case AST_BINARY_MUL: {
            Value l, r;
            if (!cg_const_eval(node->as.binary.left, &l)) return 0;
            if (!cg_const_eval(node->as.binary.right, &r)) { free_value(l); return 0; }
            if (node->type == AST_BINARY_ADD) *out = qs_add(l, r);
            else if (node->type == AST_BINARY_SUB) *out = qs_sub(l, r);
            else *out = qs_mul(l, r);
            return 1;
        }
        case AST_BINARY_DIV:
        case AST_BINARY_MOD: {
            Value l, r;
            if (!cg_const_eval(node->as.binary.left, &l)) return 0;
            if (!cg_const_eval(node->as.binary.right, &r)) { free_value(l); return 0; }
            int numeric = (l.type == VALUE_INTEGER || l.type == VALUE_FLOAT) &&
                          (r.type == VALUE_INTEGER || r.type == VALUE_FLOAT);
            int is_float = (l.type == VALUE_FLOAT || r.type == VALUE_FLOAT);
            if (!numeric || (!is_float && r.as.integer == 0)) {
                free_value(l);
                free_value(r);
                return 0;
            }
            *out = (node->type == AST_BINARY_DIV) ? qs_div(l, r) : qs_mod(l, r);
            return 1;
        }
        case AST_BINARY_EQ:
        case AST_BINARY_NEQ: {
            Value l, r;
            if (!cg_const_eval(node->as.binary.left, &l)) return 0;
            if (!cg_const_eval(node->as.binary.right, &r)) { free_value(l); return 0; }
            *out = (node->type == AST_BINARY_EQ) ? qs_eq(l, r) : qs_neq(l, r);
            return 1;
        }
        case AST_BINARY_LT:
        case AST_BINARY_GT:
        case AST_BINARY_LTE:
        case AST_BINARY_GTE: {
            Value l, r;
            if (!cg_const_eval(node->as.binary.left, &l)) return 0;
            if (!cg_const_eval(node->as.binary.right, &r)) { free_value(l); return 0; }
            int ln = (l.type == VALUE_INTEGER || l.type == VALUE_FLOAT);
            int rn = (r.type == VALUE_INTEGER || r.type == VALUE_FLOAT);
            if (!ln || !rn) { free_value(l); free_value(r); return 0; }
            if (node->type == AST_BINARY_LT) *out = qs_lt(l, r);
            else if (node->type == AST_BINARY_GT) *out = qs_gt(l, r);
            else if (node->type == AST_BINARY_LTE) *out = qs_lte(l, r);
            else *out = qs_gte(l, r);
            return 1;
        }
        case AST_BINARY_AND:
        case AST_BINARY_OR: {
            Value l, r;
            if (!cg_const_eval(node->as.binary.left, &l)) return 0;
            if (!cg_const_eval(node->as.binary.right, &r)) { free_value(l); return 0; }
            int lt = qs_truthy(l);
            int rt = qs_truthy(r);
            free_value(l);
            free_value(r);
            *out = make_bool(node->type == AST_BINARY_AND ? (lt && rt) : (lt || rt));
            return 1;
        }
        case AST_BINARY_IN: {
            Value l, r;
            if (!cg_const_eval(node->as.binary.left, &l)) return 0;
            if (!cg_const_eval(node->as.binary.right, &r)) { free_value(l); return 0; }
            *out = qs_in(l, r);
            return 1;
        }
        case AST_LIST_LIT: {
            Value list = make_list_empty();
            for (int i = 0; i < node->as.list.count; i++) {
                Value item;
                if (!cg_const_eval(node->as.list.items[i], &item)) {
                    free_value(list);
                    return 0;
                }
                list_push(&list.as.list, item);
            }
            if (!cg_value_small(list)) { free_value(list); return 0; }
            *out = list;
            return 1;
        }
        case AST_MAP_LIT: {
            Value dict = make_dict_empty();
            for (int i = 0; i < node->as.map.count; i++) {
                Value kv, vv;
                if (!cg_const_eval(node->as.map.keys[i], &kv)) {
                    free_value(dict);
                    return 0;
                }
                if (!cg_const_eval(node->as.map.vals[i], &vv)) {
                    free_value(kv);
                    free_value(dict);
                    return 0;
                }
                char *ks = value_to_string(kv);
                free_value(kv);
                dict_set(&dict.as.dict, ks, vv);
                free(ks);
            }
            if (!cg_value_small(dict)) { free_value(dict); return 0; }
            *out = dict;
            return 1;
        }
        case AST_INDEX: {
            Value obj, key;
            if (!cg_const_eval(node->as.index.object, &obj)) return 0;
            if (!cg_const_eval(node->as.index.key, &key)) { free_value(obj); return 0; }
            Value res;
            if (!cg_runtime_index(obj, key, &res)) return 0;
            if (!cg_value_small(res)) { free_value(res); return 0; }
            *out = res;
            return 1;
        }
        case AST_SLICE: {
            Value obj, start, end;
            if (!cg_const_eval(node->as.slice.object, &obj)) return 0;
            if (node->as.slice.start) {
                if (!cg_const_eval(node->as.slice.start, &start)) { free_value(obj); return 0; }
            } else {
                start = make_integer(0);
            }
            if (node->as.slice.end) {
                if (!cg_const_eval(node->as.slice.end, &end)) {
                    free_value(obj);
                    free_value(start);
                    return 0;
                }
            } else {
                end = make_null();
            }
            Value res = qs_slice(obj, start, end);
            if (!cg_value_small(res)) { free_value(res); return 0; }
            *out = res;
            return 1;
        }
        case AST_FSTRING: {
            int n = node->as.fstring.count;
            if (n <= 0) { *out = make_string_owned(qs_strdup("")); return 1; }
            Value *parts = malloc((size_t)n * sizeof(Value));
            int ok = 1;
            for (int i = 0; i < n; i++) {
                if (!cg_const_eval(node->as.fstring.parts[i], &parts[i])) {
                    for (int j = 0; j < i; j++) free_value(parts[j]);
                    ok = 0;
                    break;
                }
            }
            if (!ok) { free(parts); return 0; }
            Value res = qs_concat_strings(n, parts);
            free(parts);
            if (!cg_value_small(res)) { free_value(res); return 0; }
            *out = res;
            return 1;
        }
        case AST_CALL: {
            if (!cg_is_pure_builtin(node->as.call.name)) return 0;
            if (cg_is_class(node->as.call.name)) return 0;
            if (node->as.call.arg_names) {
                for (int i = 0; i < node->as.call.argc; i++) {
                    if (node->as.call.arg_names[i]) return 0;
                }
            }
            int argc = node->as.call.argc;
            CallArgList cargs = {0};
            cargs.count = (size_t)argc;
            cargs.capacity = (size_t)argc;
            cargs.items = argc > 0 ? calloc((size_t)argc, sizeof(CallArg)) : NULL;
            int ok = 1;
            for (int i = 0; i < argc; i++) {
                if (!cg_const_eval(node->as.call.args[i], &cargs.items[i].value)) {
                    ok = 0;
                    break;
                }
            }
            if (!ok) {
                free_call_args(&cargs);
                return 0;
            }
            if (strcmp(node->as.call.name, "range") == 0 && argc >= 1 && argc <= 3) {
                int allint = 1;
                for (int i = 0; i < argc; i++) {
                    if (cargs.items[i].value.type != VALUE_INTEGER) { allint = 0; break; }
                }
                if (allint) {
                    long start = 1, stop = 0, step = 1, span = 0;
                    if (argc == 1) {
                        stop = cargs.items[0].value.as.integer;
                    } else {
                        start = cargs.items[0].value.as.integer;
                        stop = cargs.items[1].value.as.integer;
                        if (argc == 3) step = cargs.items[2].value.as.integer;
                    }
                    if (step > 0) span = stop >= start ? (stop - start) / step + 1 : 0;
                    else if (step < 0) span = start >= stop ? (start - stop) / (-step) + 1 : 0;
                    if (span > CG_MAX_FOLD_ELEMS) {
                        free_call_args(&cargs);
                        return 0;
                    }
                }
            }
            ParseError perr = {0};
            Parser bparser = {"", 0, 0, NULL, &perr, -1};
            Value res = make_null();
            int built = call_builtin(&bparser, node->as.call.name, &cargs, &res);
            free_call_args(&cargs);
            if (!built) { free_value(res); return 0; }
            if (!cg_value_small(res)) { free_value(res); return 0; }
            *out = res;
            return 1;
        }
        case AST_IDENT: {
            Value *cv = cg_const_get(node->as.string);
            if (!cg_value_scalar(cv)) return 0;
            *out = clone_value(*cv);
            return 1;
        }
        default:
            return 0;
    }
}

static char *cg_expr(ASTNode *node) {
    if (!node) return cg_emit_null();

    {
        Value folded;
        if (cg_const_eval(node, &folded)) {
            if (!cg_value_finite(folded)) {
                free_value(folded);
            } else {
                char *t = cg_newtemp();
                cg_emit_value_literal(t, folded);
                free_value(folded);
                return t;
            }
        }
    }

    if (cg_int_enabled && cg_expr_is_int(node)) {
        char *ie = cg_int_expr(node);
        if (ie) {
            char *t = cg_newtemp();
            cg_emit("    Value %s = make_integer(%s);\n", t, ie);
            free(ie);
            return t;
        }
    }

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
            cg_mark_read(node->as.string);
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
            ASTNode *onode = node->as.index.object;
            int by_ptr = cg_ptr_ident(onode);
            char *obj = by_ptr ? NULL : cg_expr(onode);
            char *key = cg_expr(node->as.index.key);
            char *t = cg_newtemp();
            if (by_ptr) {
                char *p = cg_ident_ptr(onode);
                cg_emit("    Value %s = qs_index_ref(%s, %s);\n", t, p, key);
            } else {
                cg_emit("    Value %s = qs_index(%s, %s);\n", t, obj, key);
            }
            return t;
        }
        case AST_SLICE: {
            ASTNode *onode = node->as.slice.object;
            int by_ptr = cg_ptr_ident(onode);
            char *obj = by_ptr ? NULL : cg_expr(onode);
            char *start = node->as.slice.start ? cg_expr(node->as.slice.start) : NULL;
            if (!start) {
                start = cg_newtemp();
                cg_emit("    Value %s = make_integer(0);\n", start);
            }
            char *end = node->as.slice.end ? cg_expr(node->as.slice.end) : NULL;
            if (!end) end = cg_emit_null();
            char *t = cg_newtemp();
            if (by_ptr) {
                char *p = cg_ident_ptr(onode);
                cg_emit("    Value %s = qs_slice_ref(%s, %s, %s);\n", t, p, start, end);
            } else {
                cg_emit("    Value %s = qs_slice(%s, %s, %s);\n", t, obj, start, end);
            }
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

        case AST_METHOD_CALL: {
            ASTNode *obj = node->as.method_call.object;
            char *r = cg_method_call(obj, node->as.method_call.method,
                                     node->as.method_call.args, node->as.method_call.argc);
            if (obj && obj->type == AST_IDENT && obj->as.string &&
                cg_is_mutating_method(node->as.method_call.method)) {
                cg_const_clear(obj->as.string);
                cg_emit("    qs_set(env, &local, ");
                write_c_string(cg_out, obj->as.string);
                cg_emit(", clone_value(%s));\n", r);
            }
            return r;
        }

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

static int cg_try_fold_method_mutation(ASTNode *node, Value *out) {
    if (!node || node->type != AST_METHOD_CALL) return 0;
    ASTNode *obj = node->as.method_call.object;
    const char *m = node->as.method_call.method;
    if (!obj || obj->type != AST_IDENT || !obj->as.string || !m) return 0;
    if (strcmp(m, "append") != 0 && strcmp(m, "extend") != 0 &&
        strcmp(m, "sort") != 0 && strcmp(m, "remove") != 0) return 0;
    const char *name = obj->as.string;
    Value *cv = cg_const_get(name);
    if (!cv) return 0;
    if (cv->type != VALUE_LIST && cv->type != VALUE_DICT) return 0;

    int argc = node->as.method_call.argc;
    ValueList fargs = {0};
    for (int i = 0; i < argc; i++) {
        Value av;
        if (!cg_const_eval(node->as.method_call.args[i], &av)) {
            for (size_t k = 0; k < fargs.count; k++) free_value(fargs.items[k]);
            free(fargs.items);
            return 0;
        }
        list_push(&fargs, av);
    }

    Value newv = clone_value(*cv);
    ParseError perr = {0};
    Parser bparser = {"", 0, 0, NULL, &perr, -1};
    int ok = apply_method(&bparser, &newv, m, &fargs);
    for (size_t k = 0; k < fargs.count; k++) free_value(fargs.items[k]);
    free(fargs.items);
    if (!ok) {
        free_value(newv);
        return 0;
    }
    if (!cg_value_small(newv) || !cg_value_finite(newv)) {
        free_value(newv);
        return 0;
    }

    cg_const_set(name, clone_value(newv));
    *out = newv;
    return 1;
}

static void cg_invalidate_assignments(ASTNode *node) {
    if (!node) return;
    switch (node->type) {
        case AST_ASSIGN:
            cg_const_clear(node->as.assign.name);
            cg_invalidate_assignments(node->as.assign.value);
            break;
        case AST_FIELD_ASSIGN:
            cg_invalidate_assignments(node->as.field_assign.object);
            cg_invalidate_assignments(node->as.field_assign.value);
            break;
        case AST_EXPR_STMT:
        case AST_UNARY_NEG:
        case AST_UNARY_NOT:
        case AST_RETURN:
        case AST_THROW:
            cg_invalidate_assignments(node->as.unary.operand);
            break;
        case AST_METHOD_CALL: {
            ASTNode *obj = node->as.method_call.object;
            if (obj && obj->type == AST_IDENT && obj->as.string &&
                cg_is_mutating_method(node->as.method_call.method)) {
                cg_const_clear(obj->as.string);
            }
            cg_invalidate_assignments(obj);
            for (int i = 0; i < node->as.method_call.argc; i++)
                cg_invalidate_assignments(node->as.method_call.args[i]);
            break;
        }
        case AST_BINARY_ADD: case AST_BINARY_SUB: case AST_BINARY_MUL:
        case AST_BINARY_DIV: case AST_BINARY_MOD:
        case AST_BINARY_EQ: case AST_BINARY_NEQ:
        case AST_BINARY_LT: case AST_BINARY_GT: case AST_BINARY_LTE: case AST_BINARY_GTE:
        case AST_BINARY_AND: case AST_BINARY_OR: case AST_BINARY_IN:
            cg_invalidate_assignments(node->as.binary.left);
            cg_invalidate_assignments(node->as.binary.right);
            break;
        case AST_CHAINED_CMP:
            for (int i = 0; i < node->as.chained.count; i++)
                cg_invalidate_assignments(node->as.chained.operands[i]);
            break;
        case AST_INDEX:
            cg_invalidate_assignments(node->as.index.object);
            cg_invalidate_assignments(node->as.index.key);
            break;
        case AST_SLICE:
            cg_invalidate_assignments(node->as.slice.object);
            cg_invalidate_assignments(node->as.slice.start);
            cg_invalidate_assignments(node->as.slice.end);
            break;
        case AST_CALL:
            for (int i = 0; i < node->as.call.argc; i++)
                cg_invalidate_assignments(node->as.call.args[i]);
            break;
        case AST_NEW:
            for (int i = 0; i < node->as.new_expr.argc; i++)
                cg_invalidate_assignments(node->as.new_expr.args[i]);
            break;
        case AST_LIST_LIT:
            for (int i = 0; i < node->as.list.count; i++)
                cg_invalidate_assignments(node->as.list.items[i]);
            break;
        case AST_LIST_COMP:
            cg_invalidate_assignments(node->as.comp.expr);
            cg_invalidate_assignments(node->as.comp.iterable);
            cg_invalidate_assignments(node->as.comp.cond);
            break;
        case AST_MAP_LIT:
            for (int i = 0; i < node->as.map.count; i++) {
                cg_invalidate_assignments(node->as.map.keys[i]);
                cg_invalidate_assignments(node->as.map.vals[i]);
            }
            break;
        case AST_FSTRING:
            for (int i = 0; i < node->as.fstring.count; i++)
                cg_invalidate_assignments(node->as.fstring.parts[i]);
            break;
        case AST_FIELD_ACCESS:
            cg_invalidate_assignments(node->as.field_access.object);
            break;
        case AST_PRINT:
            for (int i = 0; i < node->as.print.argc; i++)
                cg_invalidate_assignments(node->as.print.args[i]);
            break;
        case AST_BLOCK:
        case AST_PROGRAM:
            for (int i = 0; i < node->as.block.count; i++)
                cg_invalidate_assignments(node->as.block.stmts[i]);
            break;
        case AST_IF:
            cg_invalidate_assignments(node->as.if_stmt.cond);
            cg_invalidate_assignments(node->as.if_stmt.then_body);
            cg_invalidate_assignments(node->as.if_stmt.elif_branch);
            cg_invalidate_assignments(node->as.if_stmt.else_body);
            break;
        case AST_WHILE:
            cg_invalidate_assignments(node->as.while_stmt.cond);
            cg_invalidate_assignments(node->as.while_stmt.body);
            break;
        case AST_REPEAT:
            if (node->as.repeat.var_name) cg_const_clear(node->as.repeat.var_name);
            if (node->as.repeat.index_name) cg_const_clear(node->as.repeat.index_name);
            cg_invalidate_assignments(node->as.repeat.count_or_coll);
            cg_invalidate_assignments(node->as.repeat.body);
            break;
        case AST_TRY:
            cg_invalidate_assignments(node->as.try_stmt.try_body);
            if (node->as.try_stmt.err_var) cg_const_clear(node->as.try_stmt.err_var);
            cg_invalidate_assignments(node->as.try_stmt.oops_body);
            break;
        default:
            break;
    }
}

static void cg_assign(ASTNode *node) {
    const char *name = node->as.assign.name;
    char op = node->as.assign.op;

    if (cg_is_promo(name)) {
        ASTNode *rhs = node->as.assign.value;
        char *in = cg_int_expr(rhs);
        if (in) {
            if (op == '=') {
                cg_emit("    v_%s = %s;\n", name, in);
            } else {
                const char *cop = "+";
                if (op == '-') cop = "-";
                else if (op == '*') cop = "*";
                else if (op == '/') cop = "/";
                else if (op == '%') cop = "%";
                cg_emit("    v_%s = v_%s %s %s;\n", name, name, cop, in);
            }
            free(in);
            cg_const_clear(name);
        }
        return;
    }

    if (cg_dse && op == '=' && !is_define_name(name) && !cg_is_read(name)) {
        Value cv;
        int folded = cg_const_eval(node->as.assign.value, &cv);
        if (folded && cg_value_scalar(&cv)) {
            cg_const_set(name, cv);
            return;
        }
        if (folded) {
            free_value(cv);
        } else {
            char *side = cg_expr(node->as.assign.value);
            cg_emit("    free_value(%s);\n", side);
            cg_const_clear(name);
            return;
        }
    }

    if (op == '+') {
        char *v = cg_expr(node->as.assign.value);
        cg_mark_read(name);
        cg_emit("    qs_append_var(env, &local, ");
        write_c_string(cg_out, name);
        cg_emit(", %s);\n", v);
        cg_const_clear(name);
        return;
    }

    char *value = cg_expr(node->as.assign.value);
    if (op != '=') {
        cg_mark_read(name);
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
    if (op == '=') {
        Value cv;
        if (cg_const_eval(node->as.assign.value, &cv)) cg_const_set(name, cv);
        else cg_const_clear(name);
    } else {
        cg_const_clear(name);
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
    cg_invalidate_assignments(node);
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
    cg_invalidate_assignments(node);
}

static void cg_while(ASTNode *node) {
    cg_invalidate_assignments(node);
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
    cg_invalidate_assignments(node);
}

static void cg_repeat(ASTNode *node) {
    cg_invalidate_assignments(node);
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
    cg_invalidate_assignments(node);
}

static void cg_try(ASTNode *node) {
    cg_invalidate_assignments(node);
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
    cg_invalidate_assignments(node);
}

static void cg_expr_stmt(ASTNode *node) {
    ASTNode *e = node->as.unary.operand;
    if (e && e->type == AST_METHOD_CALL &&
        e->as.method_call.object && e->as.method_call.object->type == AST_IDENT &&
        cg_is_mutating_method(e->as.method_call.method)) {
        const char *vname = e->as.method_call.object->as.string;
        int write_back = !cg_dse || is_define_name(vname) || cg_is_read(vname);
        Value fv;
        if (cg_try_fold_method_mutation(e, &fv)) {
            if (write_back) {
                char *r = cg_const_value_temp(fv);
                cg_emit("    qs_set(env, &local, ");
                write_c_string(cg_out, vname);
                cg_emit(", %s);\n", r);
            }
            free_value(fv);
            return;
        }
        cg_const_clear_all();
        cg_mark_read(vname);
        int argc = e->as.method_call.argc;
        char **temps = argc > 0 ? malloc((size_t)argc * sizeof(char *)) : NULL;
        for (int i = 0; i < argc; i++) temps[i] = cg_expr(e->as.method_call.args[i]);
        cg_emit("    qs_method_call_inplace(env, &local, ");
        write_c_string(cg_out, vname);
        cg_emit(", ");
        write_c_string(cg_out, e->as.method_call.method);
        cg_emit(", ");
        cg_emit_args_array(temps, argc);
        cg_emit(", %d);\n", argc);
        free(temps);
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
            if (cg_int_func_ret) {
                char *ie = node->as.unary.operand ? cg_int_expr(node->as.unary.operand) : NULL;
                cg_emit("    return %s;\n", ie ? ie : "0L");
                if (ie) free(ie);
                break;
            }
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
            cg_has_import = 1;
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
    cg_const_clear_all();
    cg_promo_push();
    cg_block(f->body);
    cg_promo_pop();
    cg_loop_top = saved_loop_top;
    cg_in_func = saved_in_func;
    cg_emit("qs_fn_end:\n");
    cg_emit("    g_qs_try_top = qs_saved_try;\n");
    cg_emit("    return qs_ret;\n");
    cg_emit("}\n\n");
}

static void cg_emit_int_func(int i) {
    CgFunc *f = &cg_funcs[i];
    cg_emit("static long qs_int_%d(", i);
    if (f->param_count == 0) cg_emit("void");
    for (int p = 0; p < f->param_count; p++) {
        if (p) cg_emit(", ");
        cg_emit("long v_%s", f->params[p]);
    }
    cg_emit(") {\n");
    for (int j = 0; j < cg_fi_promo_count[i]; j++) {
        const char *nm = cg_fi_promo[i][j];
        int is_param = 0;
        for (int p = 0; p < f->param_count; p++) {
            if (strcmp(f->params[p], nm) == 0) { is_param = 1; break; }
        }
        if (!is_param) cg_emit("    long v_%s = 0;\n", nm);
    }
    int saved_loop_top = cg_loop_top;
    int saved_in_func = cg_in_func;
    int saved_int_ret = cg_int_func_ret;
    cg_in_func = 1;
    cg_loop_top = 0;
    cg_int_func_ret = 1;
    cg_const_clear_all();
    cg_promo_push();
    for (int j = 0; j < cg_fi_promo_count[i]; j++) cg_promo_add(cg_fi_promo[i][j]);
    cg_int_enabled = 1;
    cg_block(f->body);
    cg_promo_pop();
    cg_int_func_ret = saved_int_ret;
    cg_loop_top = saved_loop_top;
    cg_in_func = saved_in_func;
    cg_emit("    return 0L;\n");
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
    cg_dse = 0;
    cg_has_import = 0;
    cg_has_lambda = 0;
    cg_free_reads();
    cg_const_clear_all();
    cg_collect(ast);
    cg_analyze_int_funcs();
    cg_analyze_int(ast);

    int scan_ok = 0;
    char scan_path[4352];
    snprintf(scan_path, sizeof(scan_path), "%s.scan", output_path);
    FILE *scan = fopen(scan_path, "w");
    if (scan) {
        cg_out = scan;
        cg_temp = 0;
        cg_label = 0;
        cg_loop = 0;
        cg_loop_top = 0;
        cg_in_func = 0;
        cg_const_clear_all();
        cg_block(ast);
        for (int i = 0; i < cg_func_count; i++) cg_emit_func(&cg_funcs[i]);
        for (int i = 0; i < cg_func_count; i++) if (cg_fi_int[i]) cg_emit_int_func(i);
        fclose(scan);
        cg_out = NULL;
        remove(scan_path);
        scan_ok = 1;
    }

    FILE *out = fopen(output_path, "w");
    if (!out) {
        fprintf(stderr, "Cannot write '%s'\n", output_path);
        cg_func_count = 0;
        cg_class_count = 0;
        cg_dse = 0;
        cg_free_reads();
        cg_const_clear_all();
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
    for (int i = 0; i < cg_func_count; i++) {
        if (!cg_fi_int[i]) continue;
        CgFunc *f = &cg_funcs[i];
        cg_emit("static long qs_int_%d(", i);
        if (f->param_count == 0) cg_emit("void");
        for (int p = 0; p < f->param_count; p++) {
            if (p) cg_emit(", ");
            cg_emit("long");
        }
        cg_emit(");\n");
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
    cg_temp = 0;
    cg_label = 0;
    cg_loop = 0;
    cg_const_clear_all();
    cg_dse = scan_ok && !cg_has_import && !cg_has_lambda;
    for (int i = 0; i < cg_promo_count; i++) {
        cg_emit("    long v_%s = 0;\n", cg_promo[i]);
    }
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
    for (int i = 0; i < cg_func_count; i++) {
        if (cg_fi_int[i]) cg_emit_int_func(i);
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
    cg_dse = 0;
    cg_free_reads();
    cg_const_clear_all();
    cg_promo_reset();
    cg_assign_list_free();
    cg_fi_free();

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
