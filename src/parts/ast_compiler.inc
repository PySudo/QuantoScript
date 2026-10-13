#define MAX_LOCAL_VARS 256
typedef struct {
    char *name;
    int slot;
} LocalVar;

static LocalVar g_local_vars[MAX_LOCAL_VARS];
static int g_local_count = 0;
static int g_next_slot = 0;
static int g_top_level_slots = 0;

static void locals_init(void) {
    g_local_count = 0;
    g_next_slot = 0;
}

static int locals_find(const char *name) {
    for (int i = g_local_count - 1; i >= 0; i--) {
        if (strcmp(g_local_vars[i].name, name) == 0) {
            return g_local_vars[i].slot;
        }
    }
    return -1;
}

static int locals_add(const char *name) {
    int slot = g_next_slot++;
    g_local_vars[g_local_count].name = qs_strdup(name);
    g_local_vars[g_local_count].slot = slot;
    g_local_count++;
    return slot;
}

#define MAX_USER_FUNCS 256
static char *g_user_func_names[MAX_USER_FUNCS];
static int g_user_func_count = 0;

#define MAX_USER_CLASSES 256
static char *g_user_class_names[MAX_USER_CLASSES];
static int g_user_class_count = 0;

static void user_func_table_init(void) {
    g_user_func_count = 0;
    g_user_class_count = 0;
}

static void user_func_table_register(const char *name) {
    if (g_user_func_count >= MAX_USER_FUNCS) return;
    g_user_func_names[g_user_func_count++] = qs_strdup(name);
}

static int user_func_table_has(const char *name) {
    for (int i = 0; i < g_user_func_count; i++) {
        if (strcmp(g_user_func_names[i], name) == 0) return 1;
    }
    return 0;
}

static void user_class_table_register(const char *name) {
    if (g_user_class_count >= MAX_USER_CLASSES) return;
    g_user_class_names[g_user_class_count++] = qs_strdup(name);
}

static int user_class_table_has(const char *name) {
    for (int i = 0; i < g_user_class_count; i++) {
        if (strcmp(g_user_class_names[i], name) == 0) return 1;
    }
    return 0;
}

static void user_func_table_collect(ASTNode *node) {
    if (!node) return;
    switch (node->type) {
        case AST_FUNC_DEF:
            user_func_table_register(node->as.func_def.name);
            break;
        case AST_CLASS_DEF:
            user_class_table_register(node->as.class_def.name);
            break;
        case AST_BLOCK:
        case AST_PROGRAM:
            for (int i = 0; i < node->as.block.count; i++)
                user_func_table_collect(node->as.block.stmts[i]);
            break;
        case AST_IF:
            user_func_table_collect(node->as.if_stmt.then_body);
            user_func_table_collect(node->as.if_stmt.elif_branch);
            user_func_table_collect(node->as.if_stmt.else_body);
            break;
        case AST_WHILE:
            user_func_table_collect(node->as.while_stmt.body);
            break;
        case AST_REPEAT:
            user_func_table_collect(node->as.repeat.body);
            break;
        default:
            break;
    }
}

static int g_bc_loop_start = -1;
static int g_bc_loop_exit = -1;
static int g_bc_break_jumps[256];
static int g_bc_break_count = 0;
static int g_bc_continue_jumps[256];
static int g_bc_continue_count = 0;

static void astc_compile_expr(ASTNode *node, BytecodeProgram *prog);
static void astc_compile_stmt(ASTNode *node, BytecodeProgram *prog);
static void astc_compile_block(ASTNode *block, BytecodeProgram *prog);

static void astc_compile_expr(ASTNode *node, BytecodeProgram *prog) {
    if (!node) {
        emit(prog, OP_LOAD_CONST, make_null(), 0, 0);
        return;
    }

    switch (node->type) {
        case AST_INT_LIT: {
            int idx = add_constant(prog, make_integer(node->as.integer));
            emit(prog, OP_LOAD_CONST, make_null(), idx, node->line);
            break;
        }
        case AST_FLOAT_LIT: {
            int idx = add_constant(prog, make_float(node->as.floating));
            emit(prog, OP_LOAD_CONST, make_null(), idx, node->line);
            break;
        }
        case AST_STR_LIT: {
            int idx = add_constant(prog, make_string_owned(qs_strdup(node->as.string)));
            emit(prog, OP_LOAD_CONST, make_null(), idx, node->line);
            break;
        }
        case AST_BOOL_LIT: {
            int idx = add_constant(prog, make_bool(node->as.boolean));
            emit(prog, OP_LOAD_CONST, make_null(), idx, node->line);
            break;
        }
        case AST_NULL_LIT: {
            int idx = add_constant(prog, make_null());
            emit(prog, OP_LOAD_CONST, make_null(), idx, node->line);
            break;
        }
        case AST_IDENT: {
            int slot = locals_find(node->as.string);
            if (slot >= 0) {
                emit(prog, OP_LOAD_LOCAL, make_null(), slot, node->line);
            } else {
                emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(node->as.string)), 0, node->line);
            }
            break;
        }

        case AST_BINARY_ADD: case AST_BINARY_SUB: case AST_BINARY_MUL:
        case AST_BINARY_DIV: case AST_BINARY_MOD:
        case AST_BINARY_EQ: case AST_BINARY_NEQ:
        case AST_BINARY_LT: case AST_BINARY_GT:
        case AST_BINARY_LTE: case AST_BINARY_GTE:
        case AST_BINARY_AND: case AST_BINARY_OR: case AST_BINARY_IN: {
            astc_compile_expr(node->as.binary.left, prog);
            astc_compile_expr(node->as.binary.right, prog);

            Opcode op;
            switch (node->type) {
                case AST_BINARY_ADD: op = OP_ADD; break;
                case AST_BINARY_SUB: op = OP_SUB; break;
                case AST_BINARY_MUL: op = OP_MUL; break;
                case AST_BINARY_DIV: op = OP_DIV; break;
                case AST_BINARY_MOD: op = OP_MOD; break;
                case AST_BINARY_EQ: op = OP_EQUAL; break;
                case AST_BINARY_NEQ: op = OP_NOT_EQUAL; break;
                case AST_BINARY_LT: op = OP_LESS; break;
                case AST_BINARY_GT: op = OP_GREATER; break;
                case AST_BINARY_LTE: op = OP_LESS_EQUAL; break;
                case AST_BINARY_GTE: op = OP_GREATER_EQUAL; break;
                case AST_BINARY_AND: op = OP_AND; break;
                case AST_BINARY_OR: op = OP_OR; break;
                case AST_BINARY_IN: op = OP_IN; break;
                default: op = OP_ADD; break;
            }

            if (node->type == AST_BINARY_AND) {
                emit(prog, op, make_null(), 0, node->line);
            } else if (node->type == AST_BINARY_OR) {
                emit(prog, op, make_null(), 0, node->line);
            } else {
                emit(prog, op, make_null(), 0, node->line);
            }
            break;
        }

        case AST_UNARY_NEG: {
            astc_compile_expr(node->as.unary.operand, prog);
            emit(prog, OP_NEG, make_null(), 0, node->line);
            break;
        }
        case AST_UNARY_NOT: {
            astc_compile_expr(node->as.unary.operand, prog);
            emit(prog, OP_NOT, make_null(), 0, node->line);
            break;
        }

        case AST_CHAINED_CMP: {
            int count = node->as.chained.count;
            char **ops = node->as.chained.ops;
            ASTNode **operands = node->as.chained.operands;

            astc_compile_expr(operands[0], prog);
            astc_compile_expr(operands[1], prog);
            Opcode cmp_op;
            if (strcmp(ops[0], "<") == 0) cmp_op = OP_LESS;
            else if (strcmp(ops[0], ">") == 0) cmp_op = OP_GREATER;
            else if (strcmp(ops[0], "<=") == 0) cmp_op = OP_LESS_EQUAL;
            else cmp_op = OP_GREATER_EQUAL;
            emit(prog, cmp_op, make_null(), 0, node->line);

            for (int i = 1; i < count - 1; i++) {
                astc_compile_expr(operands[i], prog);
                astc_compile_expr(operands[i + 1], prog);
                Opcode cmp_op2;
                if (strcmp(ops[i], "<") == 0) cmp_op2 = OP_LESS;
                else if (strcmp(ops[i], ">") == 0) cmp_op2 = OP_GREATER;
                else if (strcmp(ops[i], "<=") == 0) cmp_op2 = OP_LESS_EQUAL;
                else cmp_op2 = OP_GREATER_EQUAL;
                emit(prog, cmp_op2, make_null(), 0, node->line);
                emit(prog, OP_AND, make_null(), 0, node->line);
            }
            break;
        }

        case AST_INDEX: {
            astc_compile_expr(node->as.index.object, prog);
            astc_compile_expr(node->as.index.key, prog);
            emit(prog, OP_LOAD_INDEX, make_null(), 0, node->line);
            break;
        }

        case AST_SLICE: {
            astc_compile_expr(node->as.slice.object, prog);
            if (node->as.slice.start) astc_compile_expr(node->as.slice.start, prog);
            else { int idx = add_constant(prog, make_integer(0)); emit(prog, OP_LOAD_CONST, make_null(), idx, node->line); }
            if (node->as.slice.end) astc_compile_expr(node->as.slice.end, prog);
            else emit(prog, OP_LOAD_CONST, make_null(), add_constant(prog, make_null()), node->line);
            emit(prog, OP_SLICE, make_null(), 2, node->line);
            break;
        }

        case AST_LIST_LIT: {
            for (int i = 0; i < node->as.list.count; i++) {
                astc_compile_expr(node->as.list.items[i], prog);
            }
            emit(prog, OP_BUILD_LIST, make_null(), node->as.list.count, node->line);
            break;
        }

        case AST_MAP_LIT: {
            for (int i = 0; i < node->as.map.count; i++) {
                astc_compile_expr(node->as.map.keys[i], prog);
                astc_compile_expr(node->as.map.vals[i], prog);
            }
            emit(prog, OP_BUILD_MAP, make_null(), node->as.map.count * 2, node->line);
            break;
        }

        case AST_FSTRING: {
            for (int i = 0; i < node->as.fstring.count; i++) {
                astc_compile_expr(node->as.fstring.parts[i], prog);
            }
            emit(prog, OP_BUILD_STRING, make_null(), node->as.fstring.count, node->line);
            break;
        }

        case AST_CALL: {
            int has_kw = 0;
            if (node->as.call.arg_names) {
                for (int i = 0; i < node->as.call.argc; i++)
                    if (node->as.call.arg_names[i]) { has_kw = 1; break; }
            }
            if (has_kw && !user_class_table_has(node->as.call.name)) {
                for (int i = 0; i < node->as.call.argc; i++) {
                    if (node->as.call.arg_names[i]) {
                        int nidx = add_constant(prog, make_string_owned(qs_strdup(node->as.call.arg_names[i])));
                        emit(prog, OP_LOAD_CONST, make_null(), nidx, node->line);
                    } else {
                        int nidx = add_constant(prog, make_null());
                        emit(prog, OP_LOAD_CONST, make_null(), nidx, node->line);
                    }
                    astc_compile_expr(node->as.call.args[i], prog);
                }
                emit(prog, OP_CALL_KW, make_string_owned(qs_strdup(node->as.call.name)), node->as.call.argc, node->line);
            } else if (user_class_table_has(node->as.call.name)) {
                for (int i = 0; i < node->as.call.argc; i++) {
                    astc_compile_expr(node->as.call.args[i], prog);
                }
                emit(prog, OP_NEW, make_string_owned(qs_strdup(node->as.call.name)), node->as.call.argc, node->line);
            } else if (user_func_table_has(node->as.call.name)) {
                for (int i = 0; i < node->as.call.argc; i++) {
                    astc_compile_expr(node->as.call.args[i], prog);
                }
                emit(prog, OP_CALL_USER_FUNC, make_string_owned(qs_strdup(node->as.call.name)), node->as.call.argc, node->line);
            } else {
                for (int i = 0; i < node->as.call.argc; i++) {
                    astc_compile_expr(node->as.call.args[i], prog);
                }
                emit(prog, OP_CALL_FUNC, make_string_owned(qs_strdup(node->as.call.name)), node->as.call.argc, node->line);
            }
            break;
        }

        case AST_METHOD_CALL: {
            astc_compile_expr(node->as.method_call.object, prog);
            for (int i = 0; i < node->as.method_call.argc; i++) {
                astc_compile_expr(node->as.method_call.args[i], prog);
            }
            emit(prog, OP_CALL_METHOD, make_string_owned(qs_strdup(node->as.method_call.method)), node->as.method_call.argc, node->line);
            break;
        }

        case AST_LAMBDA: {
            Value func;
            func.type = VALUE_LAMBDA;
            func.as.lambda.body = qs_strdup(node->as.lambda.body_text);
            func.as.lambda.param_names = NULL;
            func.as.lambda.param_types = NULL;
            func.as.lambda.param_count = node->as.lambda.param_count;
            func.as.lambda.closure = NULL;
            func.as.lambda.is_module = 0;
            if (node->as.lambda.param_count > 0) {
                func.as.lambda.param_names = malloc(node->as.lambda.param_count * sizeof(char *));
                func.as.lambda.param_types = calloc(node->as.lambda.param_count, sizeof(char *));
                for (int i = 0; i < node->as.lambda.param_count; i++) {
                    func.as.lambda.param_names[i] = qs_strdup(node->as.lambda.params[i]);
                    if (node->as.lambda.param_types && node->as.lambda.param_types[i])
                        func.as.lambda.param_types[i] = qs_strdup(node->as.lambda.param_types[i]);
                }
            }
            int idx = add_constant(prog, func);
            emit(prog, OP_LOAD_CONST, make_null(), idx, node->line);
            break;
        }

        default:
            emit(prog, OP_LOAD_CONST, make_null(), 0, node->line);
            break;
    }
}

static void astc_compile_stmt(ASTNode *node, BytecodeProgram *prog) {
    if (!node) return;

    switch (node->type) {
        case AST_ASSIGN: {
            int slot = locals_find(node->as.assign.name);
            if (slot >= 0) {
                if (node->as.assign.op != '=') {
                    emit(prog, OP_LOAD_LOCAL, make_null(), slot, node->line);
                }
                astc_compile_expr(node->as.assign.value, prog);
                if (node->as.assign.op != '=') {
                    Opcode op;
                    switch (node->as.assign.op) {
                        case '+': op = OP_ADD; break;
                        case '-': op = OP_SUB; break;
                        case '*': op = OP_MUL; break;
                        case '/': op = OP_DIV; break;
                        case '%': op = OP_MOD; break;
                        default: op = OP_ADD; break;
                    }
                    emit(prog, op, make_null(), 0, node->line);
                }
                emit(prog, OP_STORE_LOCAL, make_null(), slot, node->line);
            } else {
                if (node->as.assign.op != '=') {
                    emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(node->as.assign.name)), 0, node->line);
                }
                astc_compile_expr(node->as.assign.value, prog);
                if (node->as.assign.op != '=') {
                    Opcode op;
                    switch (node->as.assign.op) {
                        case '+': op = OP_ADD; break;
                        case '-': op = OP_SUB; break;
                        case '*': op = OP_MUL; break;
                        case '/': op = OP_DIV; break;
                        case '%': op = OP_MOD; break;
                        default: op = OP_ADD; break;
                    }
                    emit(prog, op, make_null(), 0, node->line);
                }
                emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(node->as.assign.name)), 0, node->line);
            }
            break;
        }

        case AST_PRINT: {
            for (int i = 0; i < node->as.print.argc; i++) {
                astc_compile_expr(node->as.print.args[i], prog);
            }
            if (node->as.print.argc <= 1) {
                emit(prog, OP_PRINT, make_null(), 0, node->line);
            } else {
                emit(prog, OP_PRINT_MULTI, make_null(), node->as.print.argc, node->line);
            }
            break;
        }

        case AST_RETURN: {
            if (node->as.unary.operand) {
                astc_compile_expr(node->as.unary.operand, prog);
            } else {
                int idx = add_constant(prog, make_null());
                emit(prog, OP_LOAD_CONST, make_null(), idx, node->line);
            }
            emit(prog, OP_RETURN, make_null(), 0, node->line);
            break;
        }

        case AST_THROW: {
            if (node->as.unary.operand) {
                astc_compile_expr(node->as.unary.operand, prog);
            } else {
                int idx = add_constant(prog, make_null());
                emit(prog, OP_LOAD_CONST, make_null(), idx, node->line);
            }
            emit(prog, OP_THROW, make_null(), 0, node->line);
            break;
        }

        case AST_BREAK: {
            if (g_bc_loop_exit >= 0) {
                g_bc_break_jumps[g_bc_break_count++] = prog->count;
                emit(prog, OP_JUMP, make_null(), 0, node->line);
            }
            break;
        }

        case AST_CONTINUE: {
            if (g_bc_loop_start >= 0) {
                g_bc_continue_jumps[g_bc_continue_count++] = prog->count;
                emit(prog, OP_JUMP, make_null(), 0, node->line);
            }
            break;
        }

        case AST_IF: {
            ASTNode *cur = node;
            char *flag_name = qs_strdup("_br");
            int end_jumps[64];
            int ej_count = 0;

            emit(prog, OP_LOAD_CONST, make_null(), add_constant(prog, make_bool(0)), node->line);
            emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(flag_name)), 0, node->line);

            while (cur) {
                if (cur->type != AST_IF) break;

                if (cur != node) {
                    emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(flag_name)), 0, cur->line);
                    end_jumps[ej_count++] = prog->count;
                    emit(prog, OP_JUMP_IF_TRUE, make_null(), 0, cur->line);
                }

                astc_compile_expr(cur->as.if_stmt.cond, prog);
                int jf = prog->count;
                emit(prog, OP_JUMP_IF_FALSE, make_null(), 0, cur->line);

                astc_compile_block(cur->as.if_stmt.then_body, prog);

                emit(prog, OP_LOAD_CONST, make_null(), add_constant(prog, make_bool(1)), cur->line);
                emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(flag_name)), 0, cur->line);

                end_jumps[ej_count++] = prog->count;
                emit(prog, OP_JUMP, make_null(), 0, cur->line);

                prog->instructions[jf].arg_i = prog->count;

                if (cur->as.if_stmt.elif_branch) cur = cur->as.if_stmt.elif_branch;
                else if (cur->as.if_stmt.else_body) {
                    emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(flag_name)), 0, cur->line);
                    end_jumps[ej_count++] = prog->count;
                    emit(prog, OP_JUMP_IF_TRUE, make_null(), 0, cur->line);
                    astc_compile_block(cur->as.if_stmt.else_body, prog);
                    break;
                } else break;
            }

            for (int j = 0; j < ej_count; j++)
                prog->instructions[end_jumps[j]].arg_i = prog->count;

            free(flag_name);
            break;
        }

        case AST_WHILE: {
            int loop_start = prog->count;

            int saved_ls = g_bc_loop_start, saved_le = g_bc_loop_exit;
            int saved_bc = g_bc_break_count, saved_cc = g_bc_continue_count;
            g_bc_loop_start = loop_start;
            g_bc_loop_exit = 0;
            g_bc_break_count = 0;
            g_bc_continue_count = 0;

            astc_compile_expr(node->as.while_stmt.cond, prog);
            int jump_false = prog->count;
            emit(prog, OP_JUMP_IF_FALSE, make_null(), 0, node->line);

            astc_compile_block(node->as.while_stmt.body, prog);

            for (int j = 0; j < g_bc_continue_count; j++)
                prog->instructions[g_bc_continue_jumps[j]].arg_i = prog->count;

            emit(prog, OP_JUMP, make_null(), loop_start, node->line);

            prog->instructions[jump_false].arg_i = prog->count;

            for (int j = 0; j < g_bc_break_count; j++)
                prog->instructions[g_bc_break_jumps[j]].arg_i = prog->count;

            g_bc_loop_start = saved_ls;
            g_bc_loop_exit = saved_le;
            g_bc_break_count = saved_bc;
            g_bc_continue_count = saved_cc;
            break;
        }

        case AST_REPEAT: {
            char *counter_name = qs_strdup("_rpt_i");
            char *len_name = qs_strdup("_rpt_n");
            char *coll_name = qs_strdup("_rpt_c");
            char *pair_name = qs_strdup("_rpt_p");

            if (node->as.repeat.is_counted) {
                astc_compile_expr(node->as.repeat.count_or_coll, prog);
                emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(len_name)), 0, node->line);
            } else {
                astc_compile_expr(node->as.repeat.count_or_coll, prog);
                const char *iter_fn = node->as.repeat.index_name ? "_iter_items2" : "_iter_items";
                emit(prog, OP_CALL_FUNC, make_string_owned(qs_strdup(iter_fn)), 1, node->line);
                emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(coll_name)), 0, node->line);
                emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(coll_name)), 0, node->line);
                emit(prog, OP_CALL_FUNC, make_string_owned(qs_strdup("len")), 1, node->line);
                emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(len_name)), 0, node->line);
            }

            int one_idx = add_constant(prog, make_integer(1));
            emit(prog, OP_LOAD_CONST, make_null(), one_idx, node->line);
            emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(counter_name)), 0, node->line);

            int loop_start = prog->count;

            emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(counter_name)), 0, node->line);
            emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(len_name)), 0, node->line);
            emit(prog, OP_LESS_EQUAL, make_null(), 0, node->line);

            int jump_false = prog->count;
            emit(prog, OP_JUMP_IF_FALSE, make_null(), 0, node->line);

            if (!node->as.repeat.is_counted && node->as.repeat.var_name) {
                if (node->as.repeat.index_name) {
                    emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(coll_name)), 0, node->line);
                    emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(counter_name)), 0, node->line);
                    emit(prog, OP_LOAD_INDEX, make_null(), 0, node->line);
                    emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(pair_name)), 0, node->line);

                    emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(pair_name)), 0, node->line);
                    int pair_first = add_constant(prog, make_integer(1));
                    emit(prog, OP_LOAD_CONST, make_null(), pair_first, node->line);
                    emit(prog, OP_LOAD_INDEX, make_null(), 0, node->line);
                    emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(node->as.repeat.index_name)), 0, node->line);

                    emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(pair_name)), 0, node->line);
                    int pair_second = add_constant(prog, make_integer(2));
                    emit(prog, OP_LOAD_CONST, make_null(), pair_second, node->line);
                    emit(prog, OP_LOAD_INDEX, make_null(), 0, node->line);
                    emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(node->as.repeat.var_name)), 0, node->line);
                } else {
                    emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(coll_name)), 0, node->line);
                    emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(counter_name)), 0, node->line);
                    emit(prog, OP_LOAD_INDEX, make_null(), 0, node->line);
                    emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(node->as.repeat.var_name)), 0, node->line);
                }
            }

            int saved_ls = g_bc_loop_start, saved_le = g_bc_loop_exit;
            int saved_bc = g_bc_break_count, saved_cc = g_bc_continue_count;
            g_bc_loop_start = loop_start;
            g_bc_loop_exit = 0;
            g_bc_break_count = 0;
            g_bc_continue_count = 0;

            astc_compile_block(node->as.repeat.body, prog);

            for (int j = 0; j < g_bc_continue_count; j++)
                prog->instructions[g_bc_continue_jumps[j]].arg_i = prog->count;

            emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(counter_name)), 0, node->line);
            int incr_one = add_constant(prog, make_integer(1));
            emit(prog, OP_LOAD_CONST, make_null(), incr_one, node->line);
            emit(prog, OP_ADD, make_null(), 0, node->line);
            emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(counter_name)), 0, node->line);

            emit(prog, OP_JUMP, make_null(), loop_start, node->line);

            prog->instructions[jump_false].arg_i = prog->count;

            for (int j = 0; j < g_bc_break_count; j++)
                prog->instructions[g_bc_break_jumps[j]].arg_i = prog->count;

            g_bc_loop_start = saved_ls;
            g_bc_loop_exit = saved_le;
            g_bc_break_count = saved_bc;
            g_bc_continue_count = saved_cc;

            free(counter_name);
            free(len_name);
            free(coll_name);
            break;
        }

        case AST_FUNC_DEF: {
            BytecodeProgram body_prog;
            bytecode_init(&body_prog);

            int saved_ls = g_bc_loop_start, saved_le = g_bc_loop_exit;
            int saved_bc = g_bc_break_count, saved_cc = g_bc_continue_count;
            int saved_local_count = g_local_count, saved_next_slot = g_next_slot;
            g_bc_loop_start = g_bc_loop_exit = -1;
            g_bc_break_count = g_bc_continue_count = 0;

            locals_init();
            for (int i = 0; i < node->as.func_def.param_count; i++) {
                locals_add(node->as.func_def.params[i]);
            }

            astc_compile_block(node->as.func_def.body, &body_prog);
            emit(&body_prog, OP_HALT, make_null(), 0, node->line);

            int func_slot_count = g_next_slot;

            g_bc_loop_start = saved_ls; g_bc_loop_exit = saved_le;
            g_bc_break_count = saved_bc; g_bc_continue_count = saved_cc;
            g_local_count = saved_local_count; g_next_slot = saved_next_slot;

            compiled_funcs_add(node->as.func_def.name, body_prog,
                              node->as.func_def.params, node->as.func_def.param_count,
                              func_slot_count);

            Value placeholder = make_null();
            int idx = add_constant(prog, placeholder);
            emit(prog, OP_LOAD_CONST, make_null(), idx, node->line);
            emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(node->as.func_def.name)), 0, node->line);
            break;
        }

        case AST_CLASS_DEF: {
            for (int m = 0; m < node->as.class_def.method_count; m++) {
                ASTNode *method = node->as.class_def.methods[m];
                char method_name[256];
                snprintf(method_name, sizeof(method_name), "%s.%s",
                         node->as.class_def.name, method->as.method_def.name);

                BytecodeProgram body_prog;
                bytecode_init(&body_prog);

                int saved_ls = g_bc_loop_start, saved_le = g_bc_loop_exit;
                int saved_bc = g_bc_break_count, saved_cc = g_bc_continue_count;
                int saved_local_count = g_local_count, saved_next_slot = g_next_slot;
                g_bc_loop_start = g_bc_loop_exit = -1;
                g_bc_break_count = g_bc_continue_count = 0;

                locals_init();
                locals_add("self");
                for (int p = 0; p < method->as.method_def.param_count; p++) {
                    locals_add(method->as.method_def.params[p]);
                }

                astc_compile_block(method->as.method_def.body, &body_prog);
                emit(&body_prog, OP_HALT, make_null(), 0, method->line);
                emit(&body_prog, OP_HALT, make_null(), 0, method->line);

                int slot_count = g_next_slot;

                g_bc_loop_start = saved_ls; g_bc_loop_exit = saved_le;
                g_bc_break_count = saved_bc; g_bc_continue_count = saved_cc;
                g_local_count = saved_local_count; g_next_slot = saved_next_slot;

                compiled_funcs_add(method_name, body_prog,
                                   method->as.method_def.params,
                                   method->as.method_def.param_count,
                                   slot_count);
            }

            Value class_val = make_class(node->as.class_def.name);
            int cidx = add_constant(prog, class_val);
            emit(prog, OP_LOAD_CONST, make_null(), cidx, node->line);
            emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(node->as.class_def.name)), 0, node->line);
            break;
        }

        case AST_FIELD_ASSIGN: {
            astc_compile_expr(node->as.field_assign.object, prog);
            astc_compile_expr(node->as.field_assign.value, prog);
            emit(prog, OP_SET_FIELD, make_string_owned(qs_strdup(node->as.field_assign.field)), 0, node->line);
            break;
        }

        case AST_TRY: {
            int try_push_idx = prog->count;
            emit(prog, OP_TRY_PUSH, make_null(), 0, node->line);

            astc_compile_block(node->as.try_stmt.try_body, prog);

            if (node->as.try_stmt.oops_body) {
                int jump_past = prog->count;
                emit(prog, OP_JUMP, make_null(), 0, node->line);

                int oops_start = prog->count;
                prog->instructions[try_push_idx].arg_i = oops_start;

                if (node->as.try_stmt.err_var) {
                    emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(node->as.try_stmt.err_var)), 0, node->line);
                }

                astc_compile_block(node->as.try_stmt.oops_body, prog);

                prog->instructions[jump_past].arg_i = prog->count;

                emit(prog, OP_CATCH_END, make_null(), 0, node->line);
            } else {
                emit(prog, OP_CATCH_END, make_null(), 0, node->line);
            }
            break;
        }

        case AST_IMPORT: {
            int idx = add_constant(prog, make_string_owned(qs_strdup(node->as.string)));
            emit(prog, OP_EVAL_STMT, make_null(), idx, node->line);
            break;
        }

        case AST_EXPR_STMT: {
            ASTNode *e = node->as.unary.operand;

            if (e && e->type == AST_METHOD_CALL &&
                e->as.method_call.object && e->as.method_call.object->type == AST_IDENT) {
                const char *m = e->as.method_call.method;
                int mutates = (strcmp(m, "append") == 0 || strcmp(m, "push") == 0 ||
                               strcmp(m, "pop") == 0 || strcmp(m, "remove") == 0 ||
                               strcmp(m, "extend") == 0 || strcmp(m, "sort") == 0 ||
                               strcmp(m, "insert") == 0 || strcmp(m, "clear") == 0 ||
                               strcmp(m, "shuffle") == 0 || strcmp(m, "reverse") == 0 ||
                               strcmp(m, "fill") == 0 || strcmp(m, "splice") == 0);
                if (mutates) {
                    const char *vname = e->as.method_call.object->as.string;
                    astc_compile_expr(e, prog);
                    int slot = locals_find(vname);
                    if (slot >= 0) {
                        emit(prog, OP_STORE_LOCAL, make_null(), slot, node->line);
                    } else {
                        emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(vname)), 0, node->line);
                    }
                    break;
                }
            }
            astc_compile_expr(e, prog);
            emit(prog, OP_POP, make_null(), 0, node->line);
            break;
        }

        case AST_BLOCK: {
            astc_compile_block(node, prog);
            break;
        }

        default:
            break;
    }
}

static void astc_compile_block(ASTNode *block, BytecodeProgram *prog) {
    if (!block || (block->type != AST_BLOCK && block->type != AST_PROGRAM)) return;
    for (int i = 0; i < block->as.block.count; i++) {
        astc_compile_stmt(block->as.block.stmts[i], prog);
    }
}

static void astc_compile_program(ASTNode *program, BytecodeProgram *prog) {
    locals_init();
    astc_compile_block(program, prog);
    emit(prog, OP_HALT, make_null(), 0, 0);
    g_top_level_slots = g_next_slot;
}
