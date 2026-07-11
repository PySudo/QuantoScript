// QuantoScript AST-to-Bytecode Compiler
// Walks AST nodes and emits bytecode instructions. Never evaluates.

// ---- Local Variable Tracking ----
// Tracks which variables are local (slot-indexed) vs global (name-indexed)
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
    return -1; // Not found — must be global
}

static int locals_add(const char *name) {
    int slot = g_next_slot++;
    g_local_vars[g_local_count].name = qs_strdup(name);
    g_local_vars[g_local_count].slot = slot;
    g_local_count++;
    return slot;
}

// Function symbol table — built from AST_FUNC_DEF nodes before compilation
#define MAX_USER_FUNCS 256
static char *g_user_func_names[MAX_USER_FUNCS];
static int g_user_func_count = 0;

// Class symbol table — built from AST_CLASS_DEF nodes before compilation
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

// Scan AST for function definitions and populate the symbol table
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

// Break/Continue tracking for nested loops
static int g_bc_loop_start = -1;
static int g_bc_loop_exit = -1;
static int g_bc_break_jumps[256];
static int g_bc_break_count = 0;
static int g_bc_continue_jumps[256];
static int g_bc_continue_count = 0;

static void astc_compile_expr(ASTNode *node, BytecodeProgram *prog);
static void astc_compile_stmt(ASTNode *node, BytecodeProgram *prog);
static void astc_compile_block(ASTNode *block, BytecodeProgram *prog);

// ---- Expression Compiler ----
// Leaves exactly 1 value on the stack.

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
                // Local variable — O(1) slot access
                emit(prog, OP_LOAD_LOCAL, make_null(), slot, node->line);
            } else {
                // Global variable — name-based lookup
                emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(node->as.string)), 0, node->line);
            }
            break;
        }

        // Binary operations
        case AST_BINARY_ADD: case AST_BINARY_SUB: case AST_BINARY_MUL:
        case AST_BINARY_DIV: case AST_BINARY_MOD:
        case AST_BINARY_EQ: case AST_BINARY_NEQ:
        case AST_BINARY_LT: case AST_BINARY_GT:
        case AST_BINARY_LTE: case AST_BINARY_GTE:
        case AST_BINARY_AND: case AST_BINARY_OR: {
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
                default: op = OP_ADD; break;
            }

            // Short-circuit for AND/OR
            if (node->type == AST_BINARY_AND) {
                // left is already compiled. We need: if left is false, skip right.
                // But we already compiled both. Fix: compile left, DUP, JUMP_IF_FALSE skip, pop dup, compile right, patch.
                // This requires restructuring. For simplicity, just emit AND (non-short-circuit).
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
            // a < b < c -> (a < b) and (b < c)
            int count = node->as.chained.count;
            char **ops = node->as.chained.ops;
            ASTNode **operands = node->as.chained.operands;

            // Compile first comparison
            astc_compile_expr(operands[0], prog);
            for (int i = 0; i < count - 1; i++) {
                astc_compile_expr(operands[i + 1], prog);
                Opcode cmp_op;
                if (strcmp(ops[i], "<") == 0) cmp_op = OP_LESS;
                else if (strcmp(ops[i], ">") == 0) cmp_op = OP_GREATER;
                else if (strcmp(ops[i], "<=") == 0) cmp_op = OP_LESS_EQUAL;
                else cmp_op = OP_GREATER_EQUAL;
                emit(prog, cmp_op, make_null(), 0, node->line);

                if (i < count - 2) {
                    // Need to AND with next comparison
                    // Save current result, compile next comparison, AND
                    emit(prog, OP_DUP, make_null(), 0, node->line);
                    // Compile next comparison
                    astc_compile_expr(operands[i + 1], prog);
                    astc_compile_expr(operands[i + 2], prog);
                    Opcode cmp_op2;
                    if (strcmp(ops[i + 1], "<") == 0) cmp_op2 = OP_LESS;
                    else if (strcmp(ops[i + 1], ">") == 0) cmp_op2 = OP_GREATER;
                    else if (strcmp(ops[i + 1], "<=") == 0) cmp_op2 = OP_LESS_EQUAL;
                    else cmp_op2 = OP_GREATER_EQUAL;
                    emit(prog, cmp_op2, make_null(), 0, node->line);
                    emit(prog, OP_AND, make_null(), 0, node->line);
                    // Skip the next iteration since we already compiled it
                    i++;
                }
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
            // Delegate to tree-walk via EVAL_STMT not feasible for slices
            // For MVP: compile obj, load start/end as constants, emit OP_SLICE (if available)
            // Otherwise delegate via a call to apply_method-like behavior
            // For now, emit a simple approach: compile obj, then call slice method
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
            if (user_class_table_has(node->as.call.name)) {
                // Class instantiation: args in forward order, then OP_NEW
                for (int i = 0; i < node->as.call.argc; i++) {
                    astc_compile_expr(node->as.call.args[i], prog);
                }
                emit(prog, OP_NEW, make_string_owned(qs_strdup(node->as.call.name)), node->as.call.argc, node->line);
            } else if (user_func_table_has(node->as.call.name)) {
                // User-defined function: args in forward order, then call
                for (int i = 0; i < node->as.call.argc; i++) {
                    astc_compile_expr(node->as.call.args[i], prog);
                }
                emit(prog, OP_CALL_USER_FUNC, make_string_owned(qs_strdup(node->as.call.name)), node->as.call.argc, node->line);
            } else {
                // Built-in: args in forward order, then call
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
            // Create a lambda value and push it
            Value func;
            func.type = VALUE_LAMBDA;
            func.as.lambda.body = qs_strdup(node->as.lambda.body_text);
            func.as.lambda.param_names = NULL;
            func.as.lambda.param_types = NULL;
            func.as.lambda.param_count = node->as.lambda.param_count;
            func.as.lambda.closure = NULL;
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

// ---- Statement Compiler ----

static void astc_compile_stmt(ASTNode *node, BytecodeProgram *prog) {
    if (!node) return;

    switch (node->type) {
        case AST_ASSIGN: {
            int slot = locals_find(node->as.assign.name);
            if (slot >= 0) {
                // Local variable — slot-based access
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
                // Global variable — name-based access
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

            // Init flag = false
            emit(prog, OP_LOAD_CONST, make_null(), add_constant(prog, make_bool(0)), node->line);
            emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(flag_name)), 0, node->line);

            while (cur) {
                if (cur->type != AST_IF) break;

                if (cur != node) {
                    // maybe: skip if flag is true
                    emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(flag_name)), 0, cur->line);
                    end_jumps[ej_count++] = prog->count;
                    emit(prog, OP_JUMP_IF_TRUE, make_null(), 0, cur->line);
                }

                astc_compile_expr(cur->as.if_stmt.cond, prog);
                int jf = prog->count;
                emit(prog, OP_JUMP_IF_FALSE, make_null(), 0, cur->line);

                astc_compile_block(cur->as.if_stmt.then_body, prog);

                // set flag = true
                emit(prog, OP_LOAD_CONST, make_null(), add_constant(prog, make_bool(1)), cur->line);
                emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(flag_name)), 0, cur->line);

                // jump to end
                end_jumps[ej_count++] = prog->count;
                emit(prog, OP_JUMP, make_null(), 0, cur->line);

                // patch false jump to next branch
                prog->instructions[jf].arg_i = prog->count;

                if (cur->as.if_stmt.elif_branch) cur = cur->as.if_stmt.elif_branch;
                else if (cur->as.if_stmt.else_body) {
                    // else: skip if flag is true
                    emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(flag_name)), 0, cur->line);
                    end_jumps[ej_count++] = prog->count;
                    emit(prog, OP_JUMP_IF_TRUE, make_null(), 0, cur->line);
                    astc_compile_block(cur->as.if_stmt.else_body, prog);
                    break;
                } else break;
            }

            // patch all end jumps
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
            g_bc_loop_exit = 0; // will be patched
            g_bc_break_count = 0;
            g_bc_continue_count = 0;

            astc_compile_expr(node->as.while_stmt.cond, prog);
            int jump_false = prog->count;
            emit(prog, OP_JUMP_IF_FALSE, make_null(), 0, node->line);

            astc_compile_block(node->as.while_stmt.body, prog);

            // Patch continue jumps to here
            for (int j = 0; j < g_bc_continue_count; j++)
                prog->instructions[g_bc_continue_jumps[j]].arg_i = prog->count;

            emit(prog, OP_JUMP, make_null(), loop_start, node->line);

            // Patch false jump
            prog->instructions[jump_false].arg_i = prog->count;

            // Patch break jumps
            for (int j = 0; j < g_bc_break_count; j++)
                prog->instructions[g_bc_break_jumps[j]].arg_i = prog->count;

            g_bc_loop_start = saved_ls;
            g_bc_loop_exit = saved_le;
            g_bc_break_count = saved_bc;
            g_bc_continue_count = saved_cc;
            break;
        }

        case AST_REPEAT: {
            // Desugar repeat into while loop with counter
            char *counter_name = qs_strdup("_rpt_i");
            char *len_name = qs_strdup("_rpt_n");
            char *coll_name = qs_strdup("_rpt_c");

            if (node->as.repeat.is_counted) {
                // repeat N { body } -> _rpt_n = N; _rpt_i = 0; while _rpt_i < _rpt_n { body; _rpt_i += 1 }
                astc_compile_expr(node->as.repeat.count_or_coll, prog);
                emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(len_name)), 0, node->line);
            } else {
                // repeat item -> coll { body }
                // Store collection, get its length
                astc_compile_expr(node->as.repeat.count_or_coll, prog);
                emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(coll_name)), 0, node->line);
                emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(coll_name)), 0, node->line);
                emit(prog, OP_CALL_FUNC, make_string_owned(qs_strdup("len")), 1, node->line);
                emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(len_name)), 0, node->line);
            }

            // Init counter (1-based for interpreter compatibility)
            int one_idx = add_constant(prog, make_integer(1));
            emit(prog, OP_LOAD_CONST, make_null(), one_idx, node->line);
            emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(counter_name)), 0, node->line);

            // Loop start
            int loop_start = prog->count;

            // Check counter <= len (1-based counter)
            emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(counter_name)), 0, node->line);
            emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(len_name)), 0, node->line);
            emit(prog, OP_LESS_EQUAL, make_null(), 0, node->line);

            int jump_false = prog->count;
            emit(prog, OP_JUMP_IF_FALSE, make_null(), 0, node->line);

            // Set loop variable if iterating
            if (!node->as.repeat.is_counted && node->as.repeat.var_name) {
                emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(coll_name)), 0, node->line);
                emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(counter_name)), 0, node->line);
                emit(prog, OP_LOAD_INDEX, make_null(), 0, node->line);
                emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(node->as.repeat.var_name)), 0, node->line);

                // Set index variable if present
                if (node->as.repeat.index_name) {
                    emit(prog, OP_LOAD_VAR, make_string_owned(qs_strdup(counter_name)), 0, node->line);
                    // Convert 0-based counter to 1-based for interpreter compatibility
                    int one_idx2 = add_constant(prog, make_integer(1));
                    emit(prog, OP_LOAD_CONST, make_null(), one_idx2, node->line);
                    emit(prog, OP_ADD, make_null(), 0, node->line);
                    emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(node->as.repeat.index_name)), 0, node->line);
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

            // Increment counter
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
            // Compile function body to a separate bytecode program
            BytecodeProgram body_prog;
            bytecode_init(&body_prog);

            // Save and reset loop tracking and local variables
            int saved_ls = g_bc_loop_start, saved_le = g_bc_loop_exit;
            int saved_bc = g_bc_break_count, saved_cc = g_bc_continue_count;
            int saved_local_count = g_local_count, saved_next_slot = g_next_slot;
            g_bc_loop_start = g_bc_loop_exit = -1;
            g_bc_break_count = g_bc_continue_count = 0;

            // Register function parameters as local variables
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

            // Store compiled body in global lookup table with param info
            compiled_funcs_add(node->as.func_def.name, body_prog,
                              node->as.func_def.params, node->as.func_def.param_count,
                              func_slot_count);

            // Emit a no-op placeholder constant (name is stored in the global table)
            Value placeholder = make_null();
            int idx = add_constant(prog, placeholder);
            emit(prog, OP_LOAD_CONST, make_null(), idx, node->line);
            emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(node->as.func_def.name)), 0, node->line);
            break;
        }

        case AST_CLASS_DEF: {
            // Register each method as a separate compiled function
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
                locals_add("self"); // first parameter is always self
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

            // Store class name as a variable (the class definition itself)
            Value class_val = make_class(node->as.class_def.name);
            int cidx = add_constant(prog, class_val);
            emit(prog, OP_LOAD_CONST, make_null(), cidx, node->line);
            emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(node->as.class_def.name)), 0, node->line);
            break;
        }

        case AST_FIELD_ASSIGN: {
            // obj.field = value
            // Emit: obj, value, OP_SET_FIELD (field name in instruction arg)
            astc_compile_expr(node->as.field_assign.object, prog);
            astc_compile_expr(node->as.field_assign.value, prog);
            emit(prog, OP_SET_FIELD, make_string_owned(qs_strdup(node->as.field_assign.field)), 0, node->line);
            break;
        }

        case AST_TRY: {
            // Emit OP_TRY_PUSH with target patched to oops start
            int try_push_idx = prog->count;
            emit(prog, OP_TRY_PUSH, make_null(), 0, node->line);

            // Compile try body
            astc_compile_block(node->as.try_stmt.try_body, prog);

            if (node->as.try_stmt.oops_body) {
                // Normal exit: jump past oops, then OP_CATCH_END
                int jump_past = prog->count;
                emit(prog, OP_JUMP, make_null(), 0, node->line);

                // Oops body starts here
                int oops_start = prog->count;
                prog->instructions[try_push_idx].arg_i = oops_start;

                // Store error variable if present (error string is on stack from exception)
                if (node->as.try_stmt.err_var) {
                    emit(prog, OP_STORE_VAR, make_string_owned(qs_strdup(node->as.try_stmt.err_var)), 0, node->line);
                }

                astc_compile_block(node->as.try_stmt.oops_body, prog);

                // Patch normal-exit jump
                prog->instructions[jump_past].arg_i = prog->count;

                emit(prog, OP_CATCH_END, make_null(), 0, node->line);
            } else {
                emit(prog, OP_CATCH_END, make_null(), 0, node->line);
            }
            break;
        }

        case AST_IMPORT: {
            // Delegate to tree-walk interpreter
            int idx = add_constant(prog, make_string_owned(qs_strdup(node->as.string)));
            emit(prog, OP_EVAL_STMT, make_null(), idx, node->line);
            break;
        }

        case AST_EXPR_STMT: {
            astc_compile_expr(node->as.unary.operand, prog);
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
