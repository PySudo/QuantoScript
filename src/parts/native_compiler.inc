// QuantoScript Native Compiler - MVP
// Generates standalone C code without runtime dependencies

// ---- IR Data Structures ----

typedef enum {
    IR_INT, IR_FLOAT, IR_BOOL, IR_STRING, IR_VOID, IR_UNKNOWN
} IRType;

typedef enum {
    IR_ADD, IR_SUB, IR_MUL, IR_DIV, IR_MOD, IR_NEG,
    IR_EQ, IR_NEQ, IR_LT, IR_GT, IR_LTE, IR_GTE,
    IR_AND, IR_OR, IR_NOT,
    IR_LOAD_LOCAL, IR_STORE_LOCAL,
    IR_LOAD_GLOBAL, IR_STORE_GLOBAL,
    IR_CONST_INT, IR_CONST_FLOAT, IR_CONST_BOOL, IR_CONST_STRING,
    IR_JMP, IR_JMP_IF_FALSE, IR_JMP_IF_TRUE,
    IR_CALL, IR_RETURN,
    IR_PRINT, IR_PRINT_INT,
    IR_LABEL, IR_FUNCTION,
    IR_NOP
} IROpcode;

typedef struct {
    IROpcode op;
    IRType type;
    int dst;        // destination register/label
    int src1;       // source operand 1
    int src2;       // source operand 2
    long ival;      // integer constant
    double fval;    // float constant
    char *sval;     // string constant
    int line;       // source line
} IRInstruction;

typedef struct {
    IRInstruction *instrs;
    int count;
    int capacity;
    int next_reg;
    int next_label;
    // Constant pools
    long *int_consts;
    int int_const_count;
    double *float_consts;
    int float_const_count;
    char **string_consts;
    int string_const_count;
    // Local variable tracking
    char **local_names;
    IRType *local_types;
    int local_count;
    int local_capacity;
    // Register type tracking
    IRType *reg_types;
    int reg_capacity;
    // Error tracking
    int has_error;
    char error_msg[256];
} IRProgram;

// ---- IR Helper Functions ----

static void ir_init(IRProgram *prog) {
    prog->instrs = NULL;
    prog->count = 0;
    prog->capacity = 0;
    prog->next_reg = 0;
    prog->next_label = 0;
    prog->int_consts = NULL;
    prog->int_const_count = 0;
    prog->float_consts = NULL;
    prog->float_const_count = 0;
    prog->string_consts = NULL;
    prog->string_const_count = 0;
    prog->local_names = NULL;
    prog->local_types = NULL;
    prog->local_count = 0;
    prog->local_capacity = 0;
    prog->reg_types = NULL;
    prog->reg_capacity = 0;
    prog->has_error = 0;
    prog->error_msg[0] = '\0';
}

static void ir_free(IRProgram *prog) {
    free(prog->instrs);
    free(prog->int_consts);
    free(prog->float_consts);
    for (int i = 0; i < prog->string_const_count; i++) free(prog->string_consts[i]);
    free(prog->string_consts);
    for (int i = 0; i < prog->local_count; i++) free(prog->local_names[i]);
    free(prog->local_names);
    free(prog->local_types);
    free(prog->reg_types);
}

static int ir_alloc_reg(IRProgram *prog) {
    int reg = prog->next_reg++;
    // Ensure reg_types array is large enough
    if (reg >= prog->reg_capacity) {
        int new_cap = prog->reg_capacity ? prog->reg_capacity * 2 : 16;
        prog->reg_types = realloc(prog->reg_types, new_cap * sizeof(IRType));
        for (int i = prog->reg_capacity; i < new_cap; i++) prog->reg_types[i] = IR_UNKNOWN;
        prog->reg_capacity = new_cap;
    }
    return reg;
}

static int ir_alloc_label(IRProgram *prog) {
    return prog->next_label++;
}

static int ir_add_int_const(IRProgram *prog, long val) {
    int idx = prog->int_const_count++;
    prog->int_consts = realloc(prog->int_consts, prog->int_const_count * sizeof(long));
    prog->int_consts[idx] = val;
    return idx;
}

static int ir_add_string_const(IRProgram *prog, const char *val) {
    int idx = prog->string_const_count++;
    prog->string_consts = realloc(prog->string_consts, prog->string_const_count * sizeof(char*));
    prog->string_consts[idx] = qs_strdup(val);
    return idx;
}

static void ir_set_error(IRProgram *prog, const char *msg) {
    if (!prog->has_error) {
        prog->has_error = 1;
        snprintf(prog->error_msg, sizeof(prog->error_msg), "%s", msg);
    }
}

static void ir_emit(IRProgram *prog, IROpcode op, IRType type, int dst, int src1, int src2) {
    if (prog->count >= prog->capacity) {
        prog->capacity = prog->capacity ? prog->capacity * 2 : 64;
        prog->instrs = realloc(prog->instrs, prog->capacity * sizeof(IRInstruction));
    }
    IRInstruction *ins = &prog->instrs[prog->count++];
    memset(ins, 0, sizeof(IRInstruction));
    ins->op = op;
    ins->type = type;
    ins->dst = dst;
    ins->src1 = src1;
    ins->src2 = src2;

    // Track register type for dst
    if (dst >= 0 && dst < prog->reg_capacity) {
        prog->reg_types[dst] = type;
    }
}

static QS_UNUSED_FN void ir_emit_int(IRProgram *prog, int dst, long val) {
    (void)dst;
    ir_add_int_const(prog, val);
    IRInstruction *ins = &prog->instrs[prog->count - 1];
    ins->ival = val;
}

// ---- AST → IR Lowering ----

static void ir_lower_block(ASTNode *block, IRProgram *prog);
static int ir_lower_expr(ASTNode *node, IRProgram *prog);

static int ir_lower_expr(ASTNode *node, IRProgram *prog) {
    if (!node || prog->has_error) {
        return -1;
    }

    switch (node->type) {
        case AST_INT_LIT: {
            int r = ir_alloc_reg(prog);
            ir_emit(prog, IR_CONST_INT, IR_INT, r, 0, 0);
            prog->instrs[prog->count - 1].ival = node->as.integer;
            return r;
        }
        case AST_FLOAT_LIT: {
            int r = ir_alloc_reg(prog);
            ir_emit(prog, IR_CONST_FLOAT, IR_FLOAT, r, 0, 0);
            prog->instrs[prog->count - 1].fval = node->as.floating;
            return r;
        }
        case AST_BOOL_LIT: {
            int r = ir_alloc_reg(prog);
            ir_emit(prog, IR_CONST_BOOL, IR_BOOL, r, 0, 0);
            prog->instrs[prog->count - 1].ival = node->as.boolean;
            return r;
        }
        case AST_STR_LIT: {
            int r = ir_alloc_reg(prog);
            int sidx = ir_add_string_const(prog, node->as.string);
            ir_emit(prog, IR_CONST_STRING, IR_STRING, r, sidx, 0);
            return r;
        }
        case AST_IDENT: {
            // Find local slot for this variable
            int slot = -1;
            for (int i = 0; i < prog->local_count; i++) {
                if (strcmp(prog->local_names[i], node->as.string) == 0) {
                    slot = i;
                    break;
                }
            }
            if (slot < 0) {
                // Variable not found - create it as local
                slot = prog->local_count++;
                if (prog->local_count > prog->local_capacity) {
                    prog->local_capacity = prog->local_capacity ? prog->local_capacity * 2 : 16;
                    prog->local_names = realloc(prog->local_names, prog->local_capacity * sizeof(char*));
                    prog->local_types = realloc(prog->local_types, prog->local_capacity * sizeof(IRType));
                }
                prog->local_names[slot] = qs_strdup(node->as.string);
                prog->local_types[slot] = IR_INT;
            }
            int r = ir_alloc_reg(prog);
            ir_emit(prog, IR_LOAD_LOCAL, IR_INT, r, slot, 0);
            return r;
        }
        case AST_BINARY_ADD: case AST_BINARY_SUB: case AST_BINARY_MUL:
        case AST_BINARY_DIV: case AST_BINARY_MOD: {
            int left = ir_lower_expr(node->as.binary.left, prog);
            int right = ir_lower_expr(node->as.binary.right, prog);
            int r = ir_alloc_reg(prog);
            IROpcode op;
            switch (node->type) {
                case AST_BINARY_ADD: op = IR_ADD; break;
                case AST_BINARY_SUB: op = IR_SUB; break;
                case AST_BINARY_MUL: op = IR_MUL; break;
                case AST_BINARY_DIV: op = IR_DIV; break;
                default: op = IR_MOD; break;
            }
            ir_emit(prog, op, IR_UNKNOWN, r, left, right);
            return r;
        }
        case AST_BINARY_EQ: case AST_BINARY_NEQ:
        case AST_BINARY_LT: case AST_BINARY_GT:
        case AST_BINARY_LTE: case AST_BINARY_GTE: {
            int left = ir_lower_expr(node->as.binary.left, prog);
            int right = ir_lower_expr(node->as.binary.right, prog);
            int r = ir_alloc_reg(prog);
            IROpcode op;
            switch (node->type) {
                case AST_BINARY_EQ: op = IR_EQ; break;
                case AST_BINARY_NEQ: op = IR_NEQ; break;
                case AST_BINARY_LT: op = IR_LT; break;
                case AST_BINARY_GT: op = IR_GT; break;
                case AST_BINARY_LTE: op = IR_LTE; break;
                default: op = IR_GTE; break;
            }
            ir_emit(prog, op, IR_BOOL, r, left, right);
            return r;
        }
        case AST_UNARY_NEG: {
            int operand = ir_lower_expr(node->as.unary.operand, prog);
            int r = ir_alloc_reg(prog);
            ir_emit(prog, IR_NEG, IR_INT, r, operand, 0);
            return r;
        }
        case AST_UNARY_NOT: {
            int operand = ir_lower_expr(node->as.unary.operand, prog);
            int r = ir_alloc_reg(prog);
            ir_emit(prog, IR_NOT, IR_BOOL, r, operand, 0);
            return r;
        }
        default:
            ir_set_error(prog, "unsupported expression type in native compiler");
            return -1;
    }
}

static void ir_lower_stmt(ASTNode *node, IRProgram *prog) {
    if (!node || prog->has_error) return;

    switch (node->type) {
        case AST_ASSIGN: {
            int val = ir_lower_expr(node->as.assign.value, prog);
            // Use register-based storage: find or create local slot
            int slot = -1;
            for (int i = 0; i < prog->local_count; i++) {
                if (strcmp(prog->local_names[i], node->as.assign.name) == 0) {
                    slot = i;
                    break;
                }
            }
            if (slot < 0) {
                slot = prog->local_count++;
                if (prog->local_count > prog->local_capacity) {
                    prog->local_capacity = prog->local_capacity ? prog->local_capacity * 2 : 16;
                    prog->local_names = realloc(prog->local_names, prog->local_capacity * sizeof(char*));
                    prog->local_types = realloc(prog->local_types, prog->local_capacity * sizeof(IRType));
                }
                prog->local_names[slot] = qs_strdup(node->as.assign.name);
                prog->local_types[slot] = IR_INT; // default to int
            }
            ir_emit(prog, IR_STORE_LOCAL, IR_INT, slot, val, 0);
            break;
        }
        case AST_PRINT: {
            for (int i = 0; i < node->as.print.argc; i++) {
                int val = ir_lower_expr(node->as.print.args[i], prog);
                // Determine type from register type tracking
                IRType print_type = IR_INT;
                if (val >= 0 && val < prog->reg_capacity) {
                    print_type = prog->reg_types[val];
                }
                ir_emit(prog, IR_PRINT_INT, print_type, 0, val, 0);
                // Add space between multiple print arguments
                if (i < node->as.print.argc - 1) {
                    int sp_reg = ir_alloc_reg(prog);
                    int sp_sidx = ir_add_string_const(prog, " ");
                    ir_emit(prog, IR_CONST_STRING, IR_STRING, sp_reg, sp_sidx, 0);
                    ir_emit(prog, IR_PRINT_INT, IR_STRING, 0, sp_reg, 0);
                }
            }
            // Add newline after print (matches VM behavior)
            int nl_reg = ir_alloc_reg(prog);
            int nl_sidx = ir_add_string_const(prog, "\n");
            ir_emit(prog, IR_CONST_STRING, IR_STRING, nl_reg, nl_sidx, 0);
            ir_emit(prog, IR_PRINT_INT, IR_STRING, 0, nl_reg, 0);
            break;
        }
        case AST_RETURN: {
            if (node->as.unary.operand) {
                int val = ir_lower_expr(node->as.unary.operand, prog);
                ir_emit(prog, IR_RETURN, IR_VOID, val, 0, 0);
            }
            break;
        }
        case AST_IF: {
            int cond = ir_lower_expr(node->as.if_stmt.cond, prog);
            if (node->as.if_stmt.else_body) {
                // if-else: then_body, JMP end, else_label: else_body, end:
                int else_label = ir_alloc_label(prog);
                int end_label = ir_alloc_label(prog);
                ir_emit(prog, IR_JMP_IF_FALSE, IR_BOOL, else_label, cond, 0);
                ir_lower_block(node->as.if_stmt.then_body, prog);
                ir_emit(prog, IR_JMP, IR_VOID, end_label, 0, 0);
                ir_emit(prog, IR_LABEL, IR_VOID, else_label, 0, 0);
                ir_lower_block(node->as.if_stmt.else_body, prog);
                ir_emit(prog, IR_LABEL, IR_VOID, end_label, 0, 0);
            } else {
                // if-only: then_body, else_label:
                int else_label = ir_alloc_label(prog);
                ir_emit(prog, IR_JMP_IF_FALSE, IR_BOOL, else_label, cond, 0);
                ir_lower_block(node->as.if_stmt.then_body, prog);
                ir_emit(prog, IR_LABEL, IR_VOID, else_label, 0, 0);
            }
            break;
        }
        case AST_WHILE: {
            int loop_label = ir_alloc_label(prog);
            ir_emit(prog, IR_LABEL, IR_VOID, loop_label, 0, 0);
            int cond = ir_lower_expr(node->as.while_stmt.cond, prog);
            int exit_label = ir_alloc_label(prog);
            ir_emit(prog, IR_JMP_IF_FALSE, IR_BOOL, exit_label, cond, 0);
            ir_lower_block(node->as.while_stmt.body, prog);
            ir_emit(prog, IR_JMP, IR_VOID, loop_label, 0, 0);
            ir_emit(prog, IR_LABEL, IR_VOID, exit_label, 0, 0);
            break;
        }
        case AST_REPEAT: {
            if (node->as.repeat.is_counted) {
                // repeat N { body } → for(i=0; i<N; i++) { body }
                int count_val = ir_lower_expr(node->as.repeat.count_or_coll, prog);
                int counter_slot = prog->local_count++;
                if (prog->local_count > prog->local_capacity) {
                    prog->local_capacity = prog->local_capacity ? prog->local_capacity * 2 : 16;
                    prog->local_names = realloc(prog->local_names, prog->local_capacity * sizeof(char*));
                    prog->local_types = realloc(prog->local_types, prog->local_capacity * sizeof(IRType));
                }
                // Generate unique counter name
                static int repeat_counter_id = 0;
                char counter_name[32];
                snprintf(counter_name, sizeof(counter_name), "_i%d", repeat_counter_id++);
                prog->local_names[counter_slot] = qs_strdup(counter_name);
                prog->local_types[counter_slot] = IR_INT;
                // Initialize counter to 0
                int zero_reg = ir_alloc_reg(prog);
                ir_emit(prog, IR_CONST_INT, IR_INT, zero_reg, 0, 0);
                prog->instrs[prog->count - 1].ival = 0;
                ir_emit(prog, IR_STORE_LOCAL, IR_INT, counter_slot, zero_reg, 0);
                // Loop
                int loop_label = ir_alloc_label(prog);
                ir_emit(prog, IR_LABEL, IR_VOID, loop_label, 0, 0);
                // Check counter < count
                int load_reg = ir_alloc_reg(prog);
                ir_emit(prog, IR_LOAD_LOCAL, IR_INT, load_reg, counter_slot, 0);
                int cmp_reg = ir_alloc_reg(prog);
                ir_emit(prog, IR_LT, IR_BOOL, cmp_reg, load_reg, count_val);
                int exit_label = ir_alloc_label(prog);
                ir_emit(prog, IR_JMP_IF_FALSE, IR_BOOL, exit_label, cmp_reg, 0);
                // Body
                ir_lower_block(node->as.repeat.body, prog);
                // Increment counter
                int one_reg = ir_alloc_reg(prog);
                ir_emit(prog, IR_CONST_INT, IR_INT, one_reg, 0, 0);
                prog->instrs[prog->count - 1].ival = 1;
                int inc_reg = ir_alloc_reg(prog);
                ir_emit(prog, IR_ADD, IR_INT, inc_reg, load_reg, one_reg);
                ir_emit(prog, IR_STORE_LOCAL, IR_INT, counter_slot, inc_reg, 0);
                // Jump back
                ir_emit(prog, IR_JMP, IR_VOID, loop_label, 0, 0);
                ir_emit(prog, IR_LABEL, IR_VOID, exit_label, 0, 0);
            } else {
                ir_set_error(prog, "repeat-for-each not supported in native compiler");
            }
            break;
        }
        case AST_BLOCK: {
            ir_lower_block(node, prog);
            break;
        }
        case AST_FUNC_DEF: {
            ir_set_error(prog, "functions are not supported in native compiler");
            break;
        }
        case AST_EXPR_STMT: {
            ir_lower_expr(node->as.unary.operand, prog);
            break;
        }
        default:
            ir_set_error(prog, "unsupported statement type in native compiler");
            break;
    }
}

static void ir_lower_block(ASTNode *block, IRProgram *prog) {
    if (!block || prog->has_error) return;
    if (block->type == AST_BLOCK || block->type == AST_PROGRAM) {
        for (int i = 0; i < block->as.block.count; i++) {
            if (prog->has_error) return;
            ir_lower_stmt(block->as.block.stmts[i], prog);
        }
    }
}

// ---- IR Optimization Passes ----

// Helper: check if instruction is a pure constant (has ival set, no side effects)
static int ir_is_pure_const(IRProgram *prog, int reg) {
    for (int i = 0; i < prog->count; i++) {
        IRInstruction *ins = &prog->instrs[i];
        if ((ins->op == IR_CONST_INT || ins->op == IR_CONST_BOOL) && ins->dst == reg) {
            return 1;
        }
    }
    return 0;
}

// Helper: get constant value from a register
static long ir_get_const_val(IRProgram *prog, int reg) {
    for (int i = 0; i < prog->count; i++) {
        IRInstruction *ins = &prog->instrs[i];
        if ((ins->op == IR_CONST_INT || ins->op == IR_CONST_BOOL) && ins->dst == reg) {
            return ins->ival;
        }
    }
    return 0;
}

// Helper: check if a register is only used once (for dead code elimination)
static QS_UNUSED_FN int ir_reg_used_count(IRProgram *prog, int reg) {
    int count = 0;
    for (int i = 0; i < prog->count; i++) {
        IRInstruction *ins = &prog->instrs[i];
        if (ins->dst == reg && ins->op != IR_LABEL) continue;
        if (ins->src1 == reg) count++;
        if (ins->src2 == reg) count++;
        if (ins->op == IR_RETURN && ins->dst == reg) count++;
        if (ins->op == IR_PRINT_INT && ins->src1 == reg) count++;
        if (ins->op == IR_PRINT && ins->src1 == reg) count++;
        if (ins->op == IR_JMP_IF_FALSE && ins->src1 == reg) count++;
        if (ins->op == IR_JMP_IF_TRUE && ins->src1 == reg) count++;
        if (ins->op == IR_STORE_LOCAL && ins->src1 == reg) count++;
    }
    return count;
}

// Pass 1: Constant Folding
// Fold CONST op CONST → CONST at IR level
static void ir_pass_constant_folding(IRProgram *prog) {
    int changed = 1;
    while (changed) {
        changed = 0;
        for (int i = 0; i < prog->count; i++) {
            IRInstruction *ins = &prog->instrs[i];
            // Look for binary ops where both sources are constants
            if (ins->op == IR_ADD || ins->op == IR_SUB || ins->op == IR_MUL ||
                ins->op == IR_DIV || ins->op == IR_MOD ||
                ins->op == IR_EQ || ins->op == IR_NEQ ||
                ins->op == IR_LT || ins->op == IR_GT ||
                ins->op == IR_LTE || ins->op == IR_GTE ||
                ins->op == IR_AND || ins->op == IR_OR) {
                if (ir_is_pure_const(prog, ins->src1) && ir_is_pure_const(prog, ins->src2)) {
                    long a = ir_get_const_val(prog, ins->src1);
                    long b = ir_get_const_val(prog, ins->src2);
                    long result;
                    switch (ins->op) {
                        case IR_ADD: result = a + b; break;
                        case IR_SUB: result = a - b; break;
                        case IR_MUL: result = a * b; break;
                        case IR_DIV: result = b != 0 ? a / b : 0; break;
                        case IR_MOD: result = b != 0 ? a % b : 0; break;
                        case IR_EQ:  result = a == b; break;
                        case IR_NEQ: result = a != b; break;
                        case IR_LT:  result = a < b; break;
                        case IR_GT:  result = a > b; break;
                        case IR_LTE: result = a <= b; break;
                        case IR_GTE: result = a >= b; break;
                        case IR_AND: result = a && b; break;
                        case IR_OR:  result = a || b; break;
                        default: result = 0; break;
                    }
                    ins->op = IR_CONST_INT;
                    ins->ival = result;
                    ins->src1 = 0;
                    ins->src2 = 0;
                    changed = 1;
                }
            }
            // Fold NOT of constant
            if (ins->op == IR_NOT && ir_is_pure_const(prog, ins->src1)) {
                long a = ir_get_const_val(prog, ins->src1);
                ins->op = IR_CONST_BOOL;
                ins->ival = !a;
                ins->src1 = 0;
                changed = 1;
            }
            // Fold NEG of constant
            if (ins->op == IR_NEG && ir_is_pure_const(prog, ins->src1)) {
                long a = ir_get_const_val(prog, ins->src1);
                ins->op = IR_CONST_INT;
                ins->ival = -a;
                ins->src1 = 0;
                changed = 1;
            }
        }
    }
}

// Pass 2: Copy Propagation + Dead Store Elimination
// Track: reg = const → replace uses of reg with const
// Track: reg = load_local → replace uses of reg with load_local
static void ir_pass_copy_propagation(IRProgram *prog) {
    // For each register, track if it holds a constant or a single load
    typedef struct { int is_const; long cval; int is_copy; int copy_reg; } RegInfo;
    RegInfo *info = calloc(prog->next_reg, sizeof(RegInfo));

    int changed = 1;
    while (changed) {
        changed = 0;
        // Reset tracking
        for (int i = 0; i < prog->next_reg; i++) {
            info[i] = (RegInfo){0};
        }

        // Forward scan: track what each register holds
        for (int i = 0; i < prog->count; i++) {
            IRInstruction *ins = &prog->instrs[i];
            switch (ins->op) {
                case IR_CONST_INT:
                case IR_CONST_BOOL:
                    info[ins->dst].is_const = 1;
                    info[ins->dst].cval = ins->ival;
                    info[ins->dst].is_copy = 0;
                    break;
                case IR_LOAD_LOCAL:
                    info[ins->dst].is_copy = 1;
                    info[ins->dst].copy_reg = ins->src1;
                    info[ins->dst].is_const = 0;
                    break;
                case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV: case IR_MOD:
                case IR_EQ: case IR_NEQ: case IR_LT: case IR_GT:
                case IR_LTE: case IR_GTE: case IR_AND: case IR_OR:
                case IR_NEG: case IR_NOT:
                    // Result is no longer a simple copy
                    info[ins->dst].is_const = 0;
                    info[ins->dst].is_copy = 0;
                    break;
                case IR_STORE_LOCAL:
                    // Storing to a local doesn't kill any register
                    break;
                default:
                    break;
            }
        }

        // Backward pass: replace uses of constant registers
        for (int i = 0; i < prog->count; i++) {
            IRInstruction *ins = &prog->instrs[i];
            // Replace src1/src2 with constants
            if (ins->op != IR_CONST_INT && ins->op != IR_CONST_BOOL &&
                ins->op != IR_CONST_FLOAT && ins->op != IR_CONST_STRING &&
                ins->op != IR_LABEL && ins->op != IR_NOP &&
                ins->op != IR_STORE_LOCAL && ins->op != IR_LOAD_LOCAL) {
                if (ins->src1 >= 0 && ins->src1 < prog->next_reg && info[ins->src1].is_const) {
                    // Can't directly replace src1 for binary ops that need registers
                    // But for constants, emit a new const instruction before this one
                    // Simpler approach: just fold constants in-place
                }
            }
        }

        // Simpler approach: eliminate dead stores (reg written but never read)
        for (int i = 0; i < prog->count; i++) {
            IRInstruction *ins = &prog->instrs[i];
            // Only eliminate CONST_INT/CONST_BOOL that are overwritten or unused
            if (ins->op == IR_CONST_INT || ins->op == IR_CONST_BOOL) {
                // Check if this reg is ever used as a source
                int used = 0;
                for (int j = i + 1; j < prog->count; j++) {
                    IRInstruction *later = &prog->instrs[j];
                    // Only check src1/src2 for instructions that actually use them as registers
                    int uses_src1 = (later->op == IR_ADD || later->op == IR_SUB ||
                                     later->op == IR_MUL || later->op == IR_DIV ||
                                     later->op == IR_MOD || later->op == IR_EQ ||
                                     later->op == IR_NEQ || later->op == IR_LT ||
                                     later->op == IR_GT || later->op == IR_LTE ||
                                     later->op == IR_GTE || later->op == IR_AND ||
                                     later->op == IR_OR || later->op == IR_NEG ||
                                     later->op == IR_NOT || later->op == IR_JMP_IF_FALSE ||
                                     later->op == IR_JMP_IF_TRUE);
                    if (uses_src1 && (later->src1 == ins->dst || later->src2 == ins->dst)) {
                        used = 1;
                        break;
                    }
                    if (later->op == IR_STORE_LOCAL && later->src1 == ins->dst) {
                        used = 1;
                        break;
                    }
                    if (later->op == IR_PRINT_INT && later->src1 == ins->dst) {
                        used = 1;
                        break;
                    }
                    if (later->op == IR_PRINT && later->src1 == ins->dst) {
                        used = 1;
                        break;
                    }
                    if (later->op == IR_RETURN && later->dst == ins->dst) {
                        used = 1;
                        break;
                    }
                }
                if (!used) {
                    ins->op = IR_NOP;
                    changed = 1;
                }
            }
        }
    }
    free(info);
}

// Pass 3: Peephole — eliminate NOP instructions
static void ir_pass_nop_elimination(IRProgram *prog) {
    int write = 0;
    for (int read = 0; read < prog->count; read++) {
        if (prog->instrs[read].op != IR_NOP) {
            if (write != read) {
                prog->instrs[write] = prog->instrs[read];
            }
            write++;
        }
    }
    prog->count = write;
}

// Main optimization pipeline
static void ir_optimize(IRProgram *prog) {
    ir_pass_constant_folding(prog);
    ir_pass_copy_propagation(prog);
    ir_pass_nop_elimination(prog);
}

// ---- IR Verifier ----

static int ir_verify(IRProgram *prog) {
    int errors = 0;
    // Collect label targets for jump validation
    int *label_defined = NULL;
    if (prog->next_label > 0) {
        label_defined = calloc(prog->next_label, sizeof(int));
    }

    // First pass: find all defined labels
    for (int i = 0; i < prog->count; i++) {
        IRInstruction *ins = &prog->instrs[i];
        if (ins->op == IR_LABEL) {
            if (ins->dst < 0 || ins->dst >= prog->next_label) {
                fprintf(stderr, "[IR VERIFY] ERROR [%d]: LABEL target %d out of range [0, %d)\n",
                        i, ins->dst, prog->next_label);
                errors++;
            } else {
                label_defined[ins->dst] = 1;
            }
        }
    }

    // Second pass: validate instructions
    for (int i = 0; i < prog->count; i++) {
        IRInstruction *ins = &prog->instrs[i];

        // Validate instruction index
        if (i < 0 || i >= prog->count) {
            fprintf(stderr, "[IR VERIFY] ERROR: instruction index %d out of range\n", i);
            errors++;
            continue;
        }

        switch (ins->op) {
            case IR_JMP:
            case IR_JMP_IF_FALSE:
            case IR_JMP_IF_TRUE: {
                // Validate jump target
                if (ins->dst < 0 || ins->dst >= prog->next_label) {
                    fprintf(stderr, "[IR VERIFY] ERROR [%d]: jump target L%d out of range [0, %d)\n",
                            i, ins->dst, prog->next_label);
                    errors++;
                }
                // Validate that target label exists
                if (ins->dst >= 0 && ins->dst < prog->next_label && !label_defined[ins->dst]) {
                    fprintf(stderr, "[IR VERIFY] WARNING [%d]: jump to undefined label L%d\n",
                            i, ins->dst);
                    errors++;
                }
                // Validate source register for conditional jumps
                if (ins->op != IR_JMP) {
                    if (ins->src1 < 0 || ins->src1 >= prog->next_reg) {
                        fprintf(stderr, "[IR VERIFY] ERROR [%d]: JMP source register r%d out of range [0, %d)\n",
                                i, ins->src1, prog->next_reg);
                        errors++;
                    }
                }
                break;
            }
            case IR_LOAD_LOCAL:
                if (ins->src1 < 0 || ins->src1 >= prog->local_count) {
                    fprintf(stderr, "[IR VERIFY] ERROR [%d]: LOAD_LOCAL slot %d out of range [0, %d)\n",
                            i, ins->src1, prog->local_count);
                    errors++;
                }
                if (ins->dst < 0 || ins->dst >= prog->next_reg) {
                    fprintf(stderr, "[IR VERIFY] ERROR [%d]: LOAD_LOCAL dest r%d out of range [0, %d)\n",
                            i, ins->dst, prog->next_reg);
                    errors++;
                }
                break;
            case IR_STORE_LOCAL:
                if (ins->dst < 0 || ins->dst >= prog->local_count) {
                    fprintf(stderr, "[IR VERIFY] ERROR [%d]: STORE_LOCAL slot %d out of range [0, %d)\n",
                            i, ins->dst, prog->local_count);
                    errors++;
                }
                if (ins->src1 < 0 || ins->src1 >= prog->next_reg) {
                    fprintf(stderr, "[IR VERIFY] ERROR [%d]: STORE_LOCAL source r%d out of range [0, %d)\n",
                            i, ins->src1, prog->next_reg);
                    errors++;
                }
                break;
            case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV: case IR_MOD:
            case IR_EQ: case IR_NEQ: case IR_LT: case IR_GT: case IR_LTE: case IR_GTE:
            case IR_AND: case IR_OR:
                if (ins->dst < 0 || ins->dst >= prog->next_reg) {
                    fprintf(stderr, "[IR VERIFY] ERROR [%d]: binary op dest r%d out of range\n", i, ins->dst);
                    errors++;
                }
                if (ins->src1 < 0 || ins->src1 >= prog->next_reg) {
                    fprintf(stderr, "[IR VERIFY] ERROR [%d]: binary op src1 r%d out of range\n", i, ins->src1);
                    errors++;
                }
                if (ins->src2 < 0 || ins->src2 >= prog->next_reg) {
                    fprintf(stderr, "[IR VERIFY] ERROR [%d]: binary op src2 r%d out of range\n", i, ins->src2);
                    errors++;
                }
                break;
            case IR_NEG: case IR_NOT:
                if (ins->dst < 0 || ins->dst >= prog->next_reg) {
                    fprintf(stderr, "[IR VERIFY] ERROR [%d]: unary op dest r%d out of range\n", i, ins->dst);
                    errors++;
                }
                if (ins->src1 < 0 || ins->src1 >= prog->next_reg) {
                    fprintf(stderr, "[IR VERIFY] ERROR [%d]: unary op src1 r%d out of range\n", i, ins->src1);
                    errors++;
                }
                break;
            case IR_CONST_INT: case IR_CONST_FLOAT: case IR_CONST_BOOL:
            case IR_CONST_STRING:
                if (ins->dst < 0 || ins->dst >= prog->next_reg) {
                    fprintf(stderr, "[IR VERIFY] ERROR [%d]: const dest r%d out of range\n", i, ins->dst);
                    errors++;
                }
                break;
            case IR_PRINT_INT: case IR_PRINT:
                if (ins->src1 < 0 || ins->src1 >= prog->next_reg) {
                    fprintf(stderr, "[IR VERIFY] ERROR [%d]: print source r%d out of range\n", i, ins->src1);
                    errors++;
                }
                break;
            case IR_RETURN:
                if (ins->dst < 0 || ins->dst >= prog->next_reg) {
                    fprintf(stderr, "[IR VERIFY] ERROR [%d]: return value r%d out of range\n", i, ins->dst);
                    errors++;
                }
                break;
            case IR_LABEL: case IR_NOP:
            case IR_LOAD_GLOBAL: case IR_STORE_GLOBAL:
            case IR_FUNCTION: case IR_CALL:
                break;
            default:
                fprintf(stderr, "[IR VERIFY] WARNING [%d]: unknown opcode %d\n", i, ins->op);
                errors++;
                break;
        }
    }

    free(label_defined);
    if (errors > 0) {
        fprintf(stderr, "[IR VERIFY] %d error(s) found\n", errors);
    }
    return errors == 0;
}

static void ir_lower_program(ASTNode *ast, IRProgram *prog) {
    ir_lower_block(ast, prog);
    ir_emit(prog, IR_NOP, IR_VOID, 0, 0, 0);
}

// ---- IR → C Code Generation ----

static void ir_dump(IRProgram *prog) {
    fprintf(stderr, "=== IR Dump ===\n");
    for (int i = 0; i < prog->count; i++) {
        IRInstruction *ins = &prog->instrs[i];
        const char *opname = "NOP";
        switch (ins->op) {
            case IR_ADD: opname = "ADD"; break;
            case IR_SUB: opname = "SUB"; break;
            case IR_MUL: opname = "MUL"; break;
            case IR_DIV: opname = "DIV"; break;
            case IR_MOD: opname = "MOD"; break;
            case IR_NEG: opname = "NEG"; break;
            case IR_EQ: opname = "EQ"; break;
            case IR_NEQ: opname = "NEQ"; break;
            case IR_LT: opname = "LT"; break;
            case IR_GT: opname = "GT"; break;
            case IR_LTE: opname = "LTE"; break;
            case IR_GTE: opname = "GTE"; break;
            case IR_AND: opname = "AND"; break;
            case IR_OR: opname = "OR"; break;
            case IR_NOT: opname = "NOT"; break;
            case IR_LOAD_LOCAL: opname = "LOAD_LOCAL"; break;
            case IR_STORE_LOCAL: opname = "STORE_LOCAL"; break;
            case IR_LOAD_GLOBAL: opname = "LOAD_GLOBAL"; break;
            case IR_STORE_GLOBAL: opname = "STORE_GLOBAL"; break;
            case IR_CONST_INT: opname = "CONST_INT"; break;
            case IR_CONST_FLOAT: opname = "CONST_FLOAT"; break;
            case IR_CONST_BOOL: opname = "CONST_BOOL"; break;
            case IR_CONST_STRING: opname = "CONST_STRING"; break;
            case IR_JMP: opname = "JMP"; break;
            case IR_JMP_IF_FALSE: opname = "JMP_IF_FALSE"; break;
            case IR_JMP_IF_TRUE: opname = "JMP_IF_TRUE"; break;
            case IR_CALL: opname = "CALL"; break;
            case IR_RETURN: opname = "RETURN"; break;
            case IR_PRINT: opname = "PRINT"; break;
            case IR_PRINT_INT: opname = "PRINT_INT"; break;
            case IR_LABEL: opname = "LABEL"; break;
            case IR_FUNCTION: opname = "FUNCTION"; break;
            case IR_NOP: opname = "NOP"; break;
        }
        fprintf(stderr, "  [%d] %s", i, opname);
        if (ins->op == IR_CONST_INT) fprintf(stderr, " %ld", ins->ival);
        else if (ins->op == IR_CONST_FLOAT) fprintf(stderr, " %f", ins->fval);
        else if (ins->op == IR_CONST_BOOL) fprintf(stderr, " %s", ins->ival ? "true" : "false");
        else if (ins->op == IR_CONST_STRING) fprintf(stderr, " \"%s\"", prog->string_consts[ins->src1]);
        else if (ins->op == IR_LOAD_GLOBAL || ins->op == IR_STORE_GLOBAL)
            fprintf(stderr, " \"%s\"", prog->string_consts[ins->src1]);
        else if (ins->op == IR_LOAD_LOCAL || ins->op == IR_STORE_LOCAL)
            fprintf(stderr, " %d", ins->src1);
        else if (ins->op == IR_PRINT_INT || ins->op == IR_PRINT)
            fprintf(stderr, " r%d", ins->src1);
        else if (ins->op == IR_JMP || ins->op == IR_JMP_IF_FALSE || ins->op == IR_JMP_IF_TRUE)
            fprintf(stderr, " -> %d", ins->dst);
        else if (ins->op == IR_LABEL)
            fprintf(stderr, " %d:", ins->dst);
        else
            fprintf(stderr, " r%d, r%d, r%d", ins->dst, ins->src1, ins->src2);
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "=== End IR ===\n");
}

static int ir_emit_c(IRProgram *prog, const char *output_path) {
    FILE *out = fopen(output_path, "w");
    if (!out) {
        fprintf(stderr, "Cannot write '%s'\n", output_path);
        return 0;
    }

    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <stdlib.h>\n");
    fprintf(out, "#include <string.h>\n\n");

    // Emit string constants
    fprintf(out, "static const char* string_pool[] = {\n");
    for (int i = 0; i < prog->string_const_count; i++) {
        fprintf(out, "    ");
        write_c_string(out, prog->string_consts[i]);
        fprintf(out, ",\n");
    }
    fprintf(out, "};\n\n");

    fprintf(out, "int main(void) {\n");

    // Emit local variable declarations
    for (int i = 0; i < prog->local_count; i++) {
        fprintf(out, "    long local_%s = 0;\n", prog->local_names[i]);
    }

    // Emit register declarations
    fprintf(out, "    long r[%d];\n", prog->next_reg);
    fprintf(out, "    (void)r;\n");

    // Emit instructions
    for (int i = 0; i < prog->count; i++) {
        IRInstruction *ins = &prog->instrs[i];
        switch (ins->op) {
            case IR_CONST_INT:
                fprintf(out, "    r[%d] = %ld;\n", ins->dst, ins->ival);
                break;
            case IR_CONST_FLOAT:
                fprintf(out, "    r[%d] = (long)%f;\n", ins->dst, ins->fval);
                break;
            case IR_CONST_BOOL:
                fprintf(out, "    r[%d] = %ld;\n", ins->dst, ins->ival ? 1L : 0L);
                break;
            case IR_CONST_STRING:
                fprintf(out, "    r[%d] = %d; /* string_pool index */\n", ins->dst, ins->src1);
                break;
            case IR_LOAD_GLOBAL:
                fprintf(out, "    /* LOAD_GLOBAL %s -> r[%d] */\n",
                        prog->string_consts[ins->src1], ins->dst);
                break;
            case IR_STORE_GLOBAL:
                fprintf(out, "    /* STORE_GLOBAL r[%d] -> %s */\n",
                        ins->src1, prog->string_consts[ins->dst]);
                break;
            case IR_LOAD_LOCAL:
                fprintf(out, "    r[%d] = local_%s;\n", ins->dst, prog->local_names[ins->src1]);
                break;
            case IR_STORE_LOCAL:
                fprintf(out, "    local_%s = r[%d];\n", prog->local_names[ins->dst], ins->src1);
                break;
            case IR_ADD:
                fprintf(out, "    r[%d] = r[%d] + r[%d];\n", ins->dst, ins->src1, ins->src2);
                break;
            case IR_SUB:
                fprintf(out, "    r[%d] = r[%d] - r[%d];\n", ins->dst, ins->src1, ins->src2);
                break;
            case IR_MUL:
                fprintf(out, "    r[%d] = r[%d] * r[%d];\n", ins->dst, ins->src1, ins->src2);
                break;
            case IR_DIV:
                fprintf(out, "    r[%d] = r[%d] / r[%d];\n", ins->dst, ins->src1, ins->src2);
                break;
            case IR_MOD:
                fprintf(out, "    r[%d] = r[%d] %% r[%d];\n", ins->dst, ins->src1, ins->src2);
                break;
            case IR_NEG:
                fprintf(out, "    r[%d] = -r[%d];\n", ins->dst, ins->src1);
                break;
            case IR_EQ:
                fprintf(out, "    r[%d] = (r[%d] == r[%d]);\n", ins->dst, ins->src1, ins->src2);
                break;
            case IR_NEQ:
                fprintf(out, "    r[%d] = (r[%d] != r[%d]);\n", ins->dst, ins->src1, ins->src2);
                break;
            case IR_LT:
                fprintf(out, "    r[%d] = (r[%d] < r[%d]);\n", ins->dst, ins->src1, ins->src2);
                break;
            case IR_GT:
                fprintf(out, "    r[%d] = (r[%d] > r[%d]);\n", ins->dst, ins->src1, ins->src2);
                break;
            case IR_LTE:
                fprintf(out, "    r[%d] = (r[%d] <= r[%d]);\n", ins->dst, ins->src1, ins->src2);
                break;
            case IR_GTE:
                fprintf(out, "    r[%d] = (r[%d] >= r[%d]);\n", ins->dst, ins->src1, ins->src2);
                break;
            case IR_AND:
                fprintf(out, "    r[%d] = (r[%d] && r[%d]);\n", ins->dst, ins->src1, ins->src2);
                break;
            case IR_OR:
                fprintf(out, "    r[%d] = (r[%d] || r[%d]);\n", ins->dst, ins->src1, ins->src2);
                break;
            case IR_NOT:
                fprintf(out, "    r[%d] = !r[%d];\n", ins->dst, ins->src1);
                break;
            case IR_PRINT_INT:
                if (ins->type == IR_STRING) {
                    fprintf(out, "    printf(\"%%s\", string_pool[r[%d]]);\n", ins->src1);
                } else {
                    fprintf(out, "    printf(\"%%ld\", r[%d]);\n", ins->src1);
                }
                break;
            case IR_PRINT:
                fprintf(out, "    printf(\"%%s\", string_pool[r[%d]]);\n", ins->src1);
                break;
            case IR_JMP:
                fprintf(out, "    goto L%d;\n", ins->dst);
                break;
            case IR_JMP_IF_FALSE:
                fprintf(out, "    if (!r[%d]) goto L%d;\n", ins->src1, ins->dst);
                break;
            case IR_JMP_IF_TRUE:
                fprintf(out, "    if (r[%d]) goto L%d;\n", ins->src1, ins->dst);
                break;
            case IR_LABEL:
                fprintf(out, "L%d: ;\n", ins->dst);
                break;
            case IR_RETURN:
                fprintf(out, "    return r[%d];\n", ins->src1);
                break;
            case IR_NOP:
                break;
            default:
                fprintf(out, "    /* TODO: %d */\n", ins->op);
                break;
        }
    }

    fprintf(out, "    return 0;\n");
    fprintf(out, "}\n");
    fclose(out);
    return 1;
}

// ---- Native Compiler Entry Point ----

static int native_compile(const char *input_path, const char *output_path) {
    // Step 1: Parse source to AST
    Program program = {0};
    if (!load_program_file(input_path, &program)) {
        fprintf(stderr, "Cannot read '%s'\n", input_path);
        return 1;
    }
    ast_normalize_source(&program);
    g_program = &program;

    ASTNode *ast = ast_parse_program(&program);
    if (!ast) {
        fprintf(stderr, "[NATIVE] Parse error\n");
        free_program(&program);
        g_program = NULL;
        return 1;
    }

    // Step 2: Lower AST to IR
    IRProgram ir;
    ir_init(&ir);
    ir_lower_program(ast, &ir);

    // Verify IR
    if (!ir_verify(&ir)) {
        fprintf(stderr, "IR verification failed, aborting code generation\n");
        ir_free(&ir);
        ast_free(ast);
        free_program(&program);
        g_program = NULL;
        return 1;
    }

    // Step 2.5: Check for errors during IR lowering
    if (ir.has_error) {
        fprintf(stderr, "Native compilation error: %s\n", ir.error_msg);
        ir_free(&ir);
        ast_free(ast);
        free_program(&program);
        g_program = NULL;
        return 1;
    }

    // Step 2.6: Optimization passes
    ir_optimize(&ir);

    // Re-verify after optimization
    if (!ir_verify(&ir)) {
        fprintf(stderr, "IR verification failed after optimization, aborting code generation\n");
        ir_free(&ir);
        ast_free(ast);
        free_program(&program);
        g_program = NULL;
        return 1;
    }

    // Optional: dump IR
    if (g_dump_bytecode) {
        ir_dump(&ir);
    }

    // Step 3: Generate C code
    int ok = ir_emit_c(&ir, output_path);

    // Cleanup
    ir_free(&ir);
    ast_free(ast);
    free_program(&program);
    // g_program points to stack-allocated program, do NOT free it
    g_program = NULL;

    return ok ? 0 : 1;
}
