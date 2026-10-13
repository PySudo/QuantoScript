typedef enum {
    AST_INT_LIT, AST_FLOAT_LIT, AST_STR_LIT, AST_BOOL_LIT, AST_NULL_LIT,

    AST_IDENT,

    AST_UNARY_NEG, AST_UNARY_NOT,

    AST_BINARY_ADD, AST_BINARY_SUB, AST_BINARY_MUL, AST_BINARY_DIV, AST_BINARY_MOD,
    AST_BINARY_EQ, AST_BINARY_NEQ,
    AST_BINARY_LT, AST_BINARY_GT, AST_BINARY_LTE, AST_BINARY_GTE,
    AST_BINARY_AND, AST_BINARY_OR,
    AST_BINARY_IN,

    AST_CHAINED_CMP,

    AST_INDEX,
    AST_SLICE,

    AST_CALL,
    AST_METHOD_CALL,

    AST_LIST_LIT,
    AST_MAP_LIT,

    AST_FSTRING,

    AST_LAMBDA,

    AST_ASSIGN,
    AST_PRINT,
    AST_RETURN,
    AST_BREAK,
    AST_CONTINUE,
    AST_IF,
    AST_WHILE,
    AST_REPEAT,
    AST_FUNC_DEF,
    AST_TRY,
    AST_THROW,
    AST_IMPORT,
    AST_EXPR_STMT,
    AST_BLOCK,
    AST_PROGRAM,

    AST_CLASS_DEF,
    AST_METHOD_DEF,
    AST_FIELD_DEF,
    AST_NEW,
    AST_FIELD_ACCESS,
    AST_FIELD_ASSIGN,
} ASTNodeType;

typedef struct ASTNode ASTNode;

struct ASTNode {
    ASTNodeType type;
    int line;
    union {
        long integer;
        double floating;
        char *string;
        int boolean;

        struct { ASTNode *operand; } unary;

        struct { ASTNode *left; ASTNode *right; } binary;

        struct { ASTNode **operands; char **ops; int count; } chained;

        struct { char *name; ASTNode **args; char **arg_names; int argc; } call;

        struct { ASTNode *object; char *method; ASTNode **args; int argc; } method_call;

        struct { ASTNode *object; ASTNode *key; } index;

        struct { ASTNode *object; ASTNode *start; ASTNode *end; } slice;

        struct { ASTNode **items; int count; } list;

        struct { ASTNode **keys; ASTNode **vals; int count; } map;

        struct { ASTNode **parts; int count; } fstring;

        struct { char **params; char **param_types; int param_count; char *body_text; } lambda;

        struct { char *name; char op;  ASTNode *value; } assign;

        struct { ASTNode **args; int argc; } print;

        struct { ASTNode *cond; ASTNode *then_body; ASTNode *elif_branch; ASTNode *else_body; } if_stmt;

        struct { ASTNode *cond; ASTNode *body; } while_stmt;

        struct {
            int is_counted;
            ASTNode *count_or_coll;
            char *var_name;
            char *index_name;
            ASTNode *body;
        } repeat;

        struct { char *name; char **params; char **param_types; int param_count; char *return_type; ASTNode *body; char *body_text; } func_def;

        struct { ASTNode *try_body; char *err_var; ASTNode *oops_body; } try_stmt;

        struct { ASTNode **stmts; int count; int capacity; } block;

        struct { char *name; ASTNode **fields; int field_count; ASTNode **methods; int method_count; } class_def;

        struct { char *name; char **params; char **param_types; int param_count; char *return_type; ASTNode *body; } method_def;

        struct { char *name; char *type; } field_def;

        struct { char *class_name; ASTNode **args; int argc; } new_expr;

        struct { ASTNode *object; char *field; } field_access;

        struct { ASTNode *object; char *field; ASTNode *value; } field_assign;
    } as;
};

static ASTNode *ast_new(ASTNodeType type, int line) {
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = type;
    n->line = line;
    return n;
}

static ASTNode *ast_int(long val, int line) {
    ASTNode *n = ast_new(AST_INT_LIT, line);
    n->as.integer = val;
    return n;
}

static ASTNode *ast_float(double val, int line) {
    ASTNode *n = ast_new(AST_FLOAT_LIT, line);
    n->as.floating = val;
    return n;
}

static ASTNode *ast_string(char *val, int line) {
    ASTNode *n = ast_new(AST_STR_LIT, line);
    n->as.string = val;
    return n;
}

static ASTNode *ast_bool(int val, int line) {
    ASTNode *n = ast_new(AST_BOOL_LIT, line);
    n->as.boolean = val;
    return n;
}

static ASTNode *ast_null(int line) {
    return ast_new(AST_NULL_LIT, line);
}

static ASTNode *ast_ident(char *name, int line) {
    ASTNode *n = ast_new(AST_IDENT, line);
    n->as.string = name;
    return n;
}

static ASTNode *ast_unary(ASTNodeType type, ASTNode *operand, int line) {
    ASTNode *n = ast_new(type, line);
    n->as.unary.operand = operand;
    return n;
}

static ASTNode *ast_binary(ASTNodeType type, ASTNode *left, ASTNode *right, int line) {
    ASTNode *n = ast_new(type, line);
    n->as.binary.left = left;
    n->as.binary.right = right;
    return n;
}

static ASTNode *ast_call(char *name, ASTNode **args, int argc, int line) {
    ASTNode *n = ast_new(AST_CALL, line);
    n->as.call.name = name;
    n->as.call.args = args;
    n->as.call.arg_names = NULL;
    n->as.call.argc = argc;
    return n;
}

static ASTNode *ast_call_named(char *name, ASTNode **args, char **arg_names, int argc, int line) {
    ASTNode *n = ast_new(AST_CALL, line);
    n->as.call.name = name;
    n->as.call.args = args;
    n->as.call.arg_names = arg_names;
    n->as.call.argc = argc;
    return n;
}

static ASTNode *ast_method_call(ASTNode *object, char *method, ASTNode **args, int argc, int line) {
    ASTNode *n = ast_new(AST_METHOD_CALL, line);
    n->as.method_call.object = object;
    n->as.method_call.method = method;
    n->as.method_call.args = args;
    n->as.method_call.argc = argc;
    return n;
}

static ASTNode *ast_class_def(char *name, ASTNode **fields, int field_count, ASTNode **methods, int method_count, int line) {
    ASTNode *n = ast_new(AST_CLASS_DEF, line);
    n->as.class_def.name = name;
    n->as.class_def.fields = fields;
    n->as.class_def.field_count = field_count;
    n->as.class_def.methods = methods;
    n->as.class_def.method_count = method_count;
    return n;
}

static ASTNode *ast_method_def(char *name, char **params, char **param_types, int param_count, char *return_type, ASTNode *body, int line) {
    ASTNode *n = ast_new(AST_METHOD_DEF, line);
    n->as.method_def.name = name;
    n->as.method_def.params = params;
    n->as.method_def.param_types = param_types;
    n->as.method_def.param_count = param_count;
    n->as.method_def.return_type = return_type;
    n->as.method_def.body = body;
    return n;
}

static QS_UNUSED_FN ASTNode *ast_new_expr(char *class_name, ASTNode **args, int argc, int line) {
    ASTNode *n = ast_new(AST_NEW, line);
    n->as.new_expr.class_name = class_name;
    n->as.new_expr.args = args;
    n->as.new_expr.argc = argc;
    return n;
}

static QS_UNUSED_FN ASTNode *ast_field_access(ASTNode *object, char *field, int line) {
    ASTNode *n = ast_new(AST_FIELD_ACCESS, line);
    n->as.field_access.object = object;
    n->as.field_access.field = field;
    return n;
}

static ASTNode *ast_field_assign(ASTNode *object, char *field, ASTNode *value, int line) {
    ASTNode *n = ast_new(AST_FIELD_ASSIGN, line);
    n->as.field_assign.object = object;
    n->as.field_assign.field = field;
    n->as.field_assign.value = value;
    return n;
}

static void ast_free(ASTNode *n) {
    if (!n) return;
    switch (n->type) {
        case AST_INT_LIT: case AST_FLOAT_LIT: case AST_BOOL_LIT: case AST_NULL_LIT:
            break;
        case AST_IDENT: case AST_STR_LIT:
            free(n->as.string);
            break;
        case AST_UNARY_NEG: case AST_UNARY_NOT: case AST_THROW:
            ast_free(n->as.unary.operand);
            break;
        case AST_BINARY_ADD: case AST_BINARY_SUB: case AST_BINARY_MUL:
        case AST_BINARY_DIV: case AST_BINARY_MOD:
        case AST_BINARY_EQ: case AST_BINARY_NEQ:
        case AST_BINARY_LT: case AST_BINARY_GT:
        case AST_BINARY_LTE: case AST_BINARY_GTE:
        case AST_BINARY_AND: case AST_BINARY_OR:
            ast_free(n->as.binary.left);
            ast_free(n->as.binary.right);
            break;
        case AST_BINARY_IN:
            ast_free(n->as.binary.left);
            ast_free(n->as.binary.right);
            break;
        case AST_CHAINED_CMP:
            for (int i = 0; i < n->as.chained.count; i++) ast_free(n->as.chained.operands[i]);
            free(n->as.chained.operands);
            for (int i = 0; i < n->as.chained.count - 1; i++) free(n->as.chained.ops[i]);
            free(n->as.chained.ops);
            break;
        case AST_INDEX:
            ast_free(n->as.index.object);
            ast_free(n->as.index.key);
            break;
        case AST_SLICE:
            ast_free(n->as.slice.object);
            ast_free(n->as.slice.start);
            ast_free(n->as.slice.end);
            break;
        case AST_CALL:
            free(n->as.call.name);
            for (int i = 0; i < n->as.call.argc; i++) ast_free(n->as.call.args[i]);
            free(n->as.call.args);
            if (n->as.call.arg_names) {
                for (int i = 0; i < n->as.call.argc; i++) free(n->as.call.arg_names[i]);
                free(n->as.call.arg_names);
            }
            break;
        case AST_METHOD_CALL:
            ast_free(n->as.method_call.object);
            free(n->as.method_call.method);
            for (int i = 0; i < n->as.method_call.argc; i++) ast_free(n->as.method_call.args[i]);
            free(n->as.method_call.args);
            break;
        case AST_LIST_LIT:
            for (int i = 0; i < n->as.list.count; i++) ast_free(n->as.list.items[i]);
            free(n->as.list.items);
            break;
        case AST_MAP_LIT:
            for (int i = 0; i < n->as.map.count; i++) {
                ast_free(n->as.map.keys[i]);
                ast_free(n->as.map.vals[i]);
            }
            free(n->as.map.keys);
            free(n->as.map.vals);
            break;
        case AST_FSTRING:
            for (int i = 0; i < n->as.fstring.count; i++) ast_free(n->as.fstring.parts[i]);
            free(n->as.fstring.parts);
            break;
        case AST_LAMBDA:
            for (int i = 0; i < n->as.lambda.param_count; i++) {
                free(n->as.lambda.params[i]);
                if (n->as.lambda.param_types && n->as.lambda.param_types[i])
                    free(n->as.lambda.param_types[i]);
            }
            free(n->as.lambda.params);
            free(n->as.lambda.param_types);
            free(n->as.lambda.body_text);
            break;
        case AST_ASSIGN:
            free(n->as.assign.name);
            ast_free(n->as.assign.value);
            break;
        case AST_PRINT:
            for (int i = 0; i < n->as.print.argc; i++) ast_free(n->as.print.args[i]);
            free(n->as.print.args);
            break;
        case AST_RETURN:
            ast_free(n->as.unary.operand);
            break;
        case AST_IF:
            ast_free(n->as.if_stmt.cond);
            ast_free(n->as.if_stmt.then_body);
            ast_free(n->as.if_stmt.elif_branch);
            ast_free(n->as.if_stmt.else_body);
            break;
        case AST_WHILE:
            ast_free(n->as.while_stmt.cond);
            ast_free(n->as.while_stmt.body);
            break;
        case AST_REPEAT:
            ast_free(n->as.repeat.count_or_coll);
            free(n->as.repeat.var_name);
            free(n->as.repeat.index_name);
            ast_free(n->as.repeat.body);
            break;
        case AST_FUNC_DEF:
            free(n->as.func_def.name);
            for (int i = 0; i < n->as.func_def.param_count; i++) {
                free(n->as.func_def.params[i]);
                if (n->as.func_def.param_types && n->as.func_def.param_types[i])
                    free(n->as.func_def.param_types[i]);
            }
            free(n->as.func_def.params);
            free(n->as.func_def.param_types);
            free(n->as.func_def.body_text);
            ast_free(n->as.func_def.body);
            break;
        case AST_TRY:
            ast_free(n->as.try_stmt.try_body);
            free(n->as.try_stmt.err_var);
            ast_free(n->as.try_stmt.oops_body);
            break;
        case AST_IMPORT:
            free(n->as.string);
            break;
        case AST_EXPR_STMT:
            ast_free(n->as.unary.operand);
            break;
        case AST_BREAK: case AST_CONTINUE:
            break;
        case AST_BLOCK: case AST_PROGRAM:
            for (int i = 0; i < n->as.block.count; i++) ast_free(n->as.block.stmts[i]);
            free(n->as.block.stmts);
            break;
        case AST_CLASS_DEF:
            free(n->as.class_def.name);
            for (int i = 0; i < n->as.class_def.method_count; i++)
                ast_free(n->as.class_def.methods[i]);
            free(n->as.class_def.methods);
            break;
        case AST_METHOD_DEF:
            free(n->as.method_def.name);
            for (int i = 0; i < n->as.method_def.param_count; i++) {
                free(n->as.method_def.params[i]);
                if (n->as.method_def.param_types && n->as.method_def.param_types[i])
                    free(n->as.method_def.param_types[i]);
            }
            free(n->as.method_def.params);
            free(n->as.method_def.param_types);
            ast_free(n->as.method_def.body);
            break;
        case AST_NEW:
            free(n->as.new_expr.class_name);
            for (int i = 0; i < n->as.new_expr.argc; i++) ast_free(n->as.new_expr.args[i]);
            free(n->as.new_expr.args);
            break;
        case AST_FIELD_ACCESS:
            ast_free(n->as.field_access.object);
            free(n->as.field_access.field);
            break;
        case AST_FIELD_DEF:
            free(n->as.field_def.name);
            free(n->as.field_def.type);
            break;
        case AST_FIELD_ASSIGN:
            ast_free(n->as.field_assign.object);
            free(n->as.field_assign.field);
            ast_free(n->as.field_assign.value);
            break;
    }
    free(n);
}
