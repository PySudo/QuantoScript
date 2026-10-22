static int g_dump_bytecode = 0;
static int g_trace_vm = 0;

typedef enum {
    OP_LOAD_CONST,
    OP_LOAD_VAR,
    OP_STORE_VAR,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_NEG,
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_LESS,
    OP_GREATER,
    OP_LESS_EQUAL,
    OP_GREATER_EQUAL,
    OP_AND,
    OP_OR,
    OP_NULLCOAL,
    OP_NOT,
    OP_IN,
    OP_PRINT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_BUILD_LIST,
    OP_BUILD_MAP,
    OP_LOAD_INDEX,
    OP_STORE_INDEX,
    OP_CALL,
    OP_RETURN,
    OP_POP,
    OP_DUP,
    OP_HALT,
    OP_CALL_METHOD,
    OP_CALL_FUNC,
    OP_BUILD_STRING,
    OP_PRINT_MULTI,
    OP_EVAL_STMT,
    OP_SLICE,
    OP_JUMP_IF_TRUE,
    OP_JUMP_IF_NOT_NULL,
    OP_CALL_USER_FUNC,
    OP_TRY_PUSH,
    OP_CATCH_END,
    OP_LOAD_LOCAL,
    OP_STORE_LOCAL,

    OP_NEW,
    OP_GET_FIELD,
    OP_SET_FIELD,
    OP_THROW,
    OP_CALL_KW,
    OP_OPT_FIELD
} Opcode;

typedef struct {
    Opcode op;
    Value arg;
    int arg_i;
    int line;
} Instruction;

typedef struct {
    Instruction *instructions;
    size_t count;
    size_t capacity;
    Value *constants;
    size_t const_count;
    size_t const_capacity;
} BytecodeProgram;

#define MAX_INLINE_SLOTS 8
typedef struct {
    BytecodeProgram *program;
    Environment *env;
    int saved_ip;
    int saved_stack_top;
    int base;
    int try_stack_top;
    Value local_slots[MAX_INLINE_SLOTS];
    int local_count;
} CallFrame;

typedef struct {
    BytecodeProgram *program;
    Environment *env;
    Value stack[1024];
    int stack_top;
    int ip;
    int try_stack[64];
    int try_stack_top;
    int try_stack_depths[64];
    BytecodeProgram *try_stack_prog[64];
    int pending_exception;
    char *exception_msg;

    CallFrame frames[256];
    int frame_count;
} VM;

static void bytecode_init(BytecodeProgram *prog) {
    prog->instructions = NULL;
    prog->count = 0;
    prog->capacity = 0;
    prog->constants = NULL;
    prog->const_count = 0;
    prog->const_capacity = 0;
}

static void bytecode_free(BytecodeProgram *prog) {
    for (size_t i = 0; i < prog->count; i++) {
        free_value(prog->instructions[i].arg);
    }
    free(prog->instructions);
    for (size_t i = 0; i < prog->const_count; i++) {
        free_value(prog->constants[i]);
    }
    free(prog->constants);
}

#define MAX_COMPILED_FUNCS 256
typedef struct {
    char *name;
    BytecodeProgram body;
    char **param_names;
    int param_count;
    int slot_count;
} CompiledFunc;
static CompiledFunc g_compiled_funcs[MAX_COMPILED_FUNCS];
static int g_compiled_func_count = 0;

static void compiled_funcs_init(void) {
    g_compiled_func_count = 0;
}

static void compiled_funcs_add(const char *name, BytecodeProgram body, char **param_names, int param_count, int slot_count) {
    if (g_compiled_func_count >= MAX_COMPILED_FUNCS) return;
    g_compiled_funcs[g_compiled_func_count].name = qs_strdup(name);
    g_compiled_funcs[g_compiled_func_count].body = body;
    g_compiled_funcs[g_compiled_func_count].param_names = NULL;
    g_compiled_funcs[g_compiled_func_count].param_count = param_count;
    g_compiled_funcs[g_compiled_func_count].slot_count = slot_count;
    if (param_count > 0) {
        g_compiled_funcs[g_compiled_func_count].param_names = malloc(param_count * sizeof(char *));
        for (int i = 0; i < param_count; i++)
            g_compiled_funcs[g_compiled_func_count].param_names[i] = qs_strdup(param_names[i]);
    }
    g_compiled_func_count++;
}

static BytecodeProgram *compiled_funcs_find(const char *name) {
    for (int i = 0; i < g_compiled_func_count; i++) {
        if (strcmp(g_compiled_funcs[i].name, name) == 0) {
            return &g_compiled_funcs[i].body;
        }
    }
    return NULL;
}

static void emit(BytecodeProgram *prog, Opcode op, Value arg, int arg_i, int line) {
    if (prog->count >= prog->capacity) {
        size_t new_cap = prog->capacity ? prog->capacity * 2 : 64;
        Instruction *new_instr = realloc(prog->instructions, new_cap * sizeof(Instruction));
        if (!new_instr) { fprintf(stderr, "out of memory\n"); exit(2); }
        prog->instructions = new_instr;
        prog->capacity = new_cap;
    }
    prog->instructions[prog->count].op = op;
    prog->instructions[prog->count].arg = arg;
    prog->instructions[prog->count].arg_i = arg_i;
    prog->instructions[prog->count].line = line;
    prog->count++;
}

static int add_constant(BytecodeProgram *prog, Value value) {
    if (prog->const_count >= prog->const_capacity) {
        size_t new_cap = prog->const_capacity ? prog->const_capacity * 2 : 16;
        Value *new_consts = realloc(prog->constants, new_cap * sizeof(Value));
        if (!new_consts) { fprintf(stderr, "out of memory\n"); exit(2); }
        prog->constants = new_consts;
        prog->const_capacity = new_cap;
    }
    prog->constants[prog->const_count] = value;
    return (int)prog->const_count++;
}

static void vm_init(VM *vm, BytecodeProgram *prog, Environment *env) {
    vm->program = prog;
    vm->env = env;
    vm->stack_top = 0;
    vm->ip = 0;
    vm->try_stack_top = 0;
    vm->frame_count = 0;
    vm->pending_exception = 0;
    vm->exception_msg = NULL;
}

#define MAX_FRAMES 256

static int frame_push(VM *vm, BytecodeProgram *prog, Environment *env) {
    if (vm->frame_count >= MAX_FRAMES) {
        fprintf(stderr, "maximum recursion depth exceeded (256)\n");
        return 0;
    }
    CallFrame *f = &vm->frames[vm->frame_count++];
    f->program = vm->program;
    f->env = vm->env;
    f->saved_ip = vm->ip;
    f->saved_stack_top = vm->stack_top;
    f->base = vm->stack_top;
    f->try_stack_top = vm->try_stack_top;
    vm->program = prog;
    vm->env = env;
    vm->ip = 0;
    return 1;
}

static void frame_pop(VM *vm) {
    if (vm->frame_count <= 0) {
        fprintf(stderr, "frame_pop: no frame to pop\n");
        return;
    }
    CallFrame *f = &vm->frames[--vm->frame_count];
    vm->program = f->program;
    vm->env = f->env;
    vm->ip = f->saved_ip;
    vm->stack_top = f->saved_stack_top;
    vm->try_stack_top = f->try_stack_top;
}

static QS_UNUSED_FN CallFrame *frame_current(VM *vm) {
    if (vm->frame_count <= 0) return NULL;
    return &vm->frames[vm->frame_count - 1];
}

static void vm_push(VM *vm, Value value) {
    if (vm->stack_top >= 1024) {
        fprintf(stderr, "VM stack overflow\n");
        exit(2);
    }
    vm->stack[vm->stack_top++] = value;
}

static Value vm_pop(VM *vm) {
    if (vm->stack_top <= 0) {
        fprintf(stderr, "VM stack underflow\n");
        exit(2);
    }
    return vm->stack[--vm->stack_top];
}

static Value vm_peek(VM *vm) {
    return vm->stack[vm->stack_top - 1];
}

static const char *opcode_name(Opcode op) {
    switch (op) {
        case OP_LOAD_CONST: return "LOAD_CONST";
        case OP_LOAD_VAR: return "LOAD_VAR";
        case OP_STORE_VAR: return "STORE_VAR";
        case OP_ADD: return "ADD";
        case OP_SUB: return "SUB";
        case OP_MUL: return "MUL";
        case OP_DIV: return "DIV";
        case OP_MOD: return "MOD";
        case OP_NEG: return "NEG";
        case OP_EQUAL: return "EQUAL";
        case OP_NOT_EQUAL: return "NOT_EQUAL";
        case OP_LESS: return "LESS";
        case OP_GREATER: return "GREATER";
        case OP_LESS_EQUAL: return "LESS_EQUAL";
        case OP_GREATER_EQUAL: return "GREATER_EQUAL";
        case OP_AND: return "AND";
        case OP_OR: return "OR";
        case OP_NOT: return "NOT";
        case OP_IN: return "IN";
        case OP_PRINT: return "PRINT";
        case OP_JUMP: return "JUMP";
        case OP_JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OP_BUILD_LIST: return "BUILD_LIST";
        case OP_BUILD_MAP: return "BUILD_MAP";
        case OP_LOAD_INDEX: return "LOAD_INDEX";
        case OP_STORE_INDEX: return "STORE_INDEX";
        case OP_CALL: return "CALL";
        case OP_RETURN: return "RETURN";
        case OP_POP: return "POP";
        case OP_DUP: return "DUP";
        case OP_HALT: return "HALT";
        case OP_CALL_METHOD: return "CALL_METHOD";
        case OP_CALL_FUNC: return "CALL_FUNC";
        case OP_BUILD_STRING: return "BUILD_STRING";
        case OP_PRINT_MULTI: return "PRINT_MULTI";
        case OP_EVAL_STMT: return "EVAL_STMT";
        case OP_SLICE: return "SLICE";
        case OP_JUMP_IF_TRUE: return "JUMP_IF_TRUE";
        case OP_JUMP_IF_NOT_NULL: return "JUMP_IF_NOT_NULL";
        case OP_NULLCOAL: return "NULLCOAL";
        case OP_CALL_USER_FUNC: return "CALL_USER_FUNC";
        case OP_TRY_PUSH: return "TRY_PUSH";
        case OP_CATCH_END: return "CATCH_END";
        case OP_LOAD_LOCAL: return "LOAD_LOCAL";
        case OP_STORE_LOCAL: return "STORE_LOCAL";
        case OP_NEW: return "NEW";
        case OP_GET_FIELD: return "GET_FIELD";
        case OP_SET_FIELD: return "SET_FIELD";
        case OP_THROW: return "THROW";
        case OP_CALL_KW: return "CALL_KW";
        case OP_OPT_FIELD: return "OPT_FIELD";
    }
    return "UNKNOWN";
}

static QS_UNUSED_FN void bytecode_dump(BytecodeProgram *prog, const char *label) {
    fprintf(stderr, "\n=== Bytecode: %s (%zu instructions, %zu constants) ===\n", label, prog->count, prog->const_count);
    for (size_t i = 0; i < prog->count; i++) {
        Instruction *instr = &prog->instructions[i];
        fprintf(stderr, "  [%3zu] %-16s", i, opcode_name(instr->op));
        switch (instr->op) {
            case OP_LOAD_CONST:
                fprintf(stderr, " const[%d]", instr->arg_i);
                if (instr->arg.type == VALUE_INTEGER) fprintf(stderr, " = %ld", instr->arg.as.integer);
                else if (instr->arg.type == VALUE_FLOAT) fprintf(stderr, " = %g", instr->arg.as.floating);
                else if (instr->arg.type == VALUE_STRING && instr->arg.as.string) fprintf(stderr, " = \"%s\"", instr->arg.as.string);
                else if (instr->arg.type == VALUE_BOOL) fprintf(stderr, " = %s", instr->arg.as.boolean ? "true" : "false");
                break;
            case OP_LOAD_VAR: case OP_STORE_VAR:
                if (instr->arg.type == VALUE_STRING && instr->arg.as.string)
                    fprintf(stderr, " '%s'", instr->arg.as.string);
                break;
            case OP_JUMP: case OP_JUMP_IF_FALSE: case OP_JUMP_IF_TRUE: case OP_JUMP_IF_NOT_NULL:
                printf(" %d", instr->arg_i);
                break;
            case OP_TRY_PUSH:
                fprintf(stderr, " -> [%d] (catch target)", instr->arg_i);
                break;
            case OP_CALL_USER_FUNC: case OP_CALL_FUNC: case OP_CALL_METHOD: case OP_CALL_KW:
                if (instr->arg.type == VALUE_STRING && instr->arg.as.string)
                    fprintf(stderr, " '%s' argc=%d", instr->arg.as.string, instr->arg_i);
                break;
            case OP_NEW:
                if (instr->arg.type == VALUE_STRING && instr->arg.as.string)
                    fprintf(stderr, " '%s' argc=%d", instr->arg.as.string, instr->arg_i);
                break;
            case OP_GET_FIELD: case OP_SET_FIELD: case OP_OPT_FIELD:
                if (instr->arg.type == VALUE_STRING && instr->arg.as.string)
                    fprintf(stderr, " '%s'", instr->arg.as.string);
                break;
            case OP_BUILD_LIST: case OP_PRINT_MULTI:
                fprintf(stderr, " count=%d", instr->arg_i);
                break;
            case OP_BUILD_MAP:
                fprintf(stderr, " pairs=%d", instr->arg_i / 2);
                break;
            case OP_PRINT: case OP_RETURN: case OP_POP: case OP_DUP:
            case OP_HALT: case OP_CATCH_END: case OP_ADD: case OP_SUB:
            case OP_MUL: case OP_DIV: case OP_MOD: case OP_NEG:
            case OP_EQUAL: case OP_NOT_EQUAL: case OP_LESS: case OP_GREATER:
            case OP_LESS_EQUAL: case OP_GREATER_EQUAL: case OP_AND:
            case OP_OR: case OP_NOT: case OP_LOAD_INDEX: case OP_STORE_INDEX:
            case OP_THROW: case OP_IN:
                break;
            default:
                fprintf(stderr, " arg_i=%d", instr->arg_i);
                break;
        }
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "=== End %s ===\n\n", label);
}

static void vm_dump_state(VM *vm, const char *label) {
    fprintf(stderr, "  [VM %s] ip=%d stack_top=%d try_depth=%d\n", label, vm->ip, vm->stack_top, vm->try_stack_top);
    fprintf(stderr, "  stack: [");
    for (int i = 0; i < vm->stack_top && i < 10; i++) {
        if (i > 0) fprintf(stderr, ", ");
        Value v = vm->stack[i];
        if (v.type == VALUE_INTEGER) fprintf(stderr, "%ld", v.as.integer);
        else if (v.type == VALUE_FLOAT) fprintf(stderr, "%g", v.as.floating);
        else if (v.type == VALUE_STRING && v.as.string) fprintf(stderr, "\"%.20s\"", v.as.string);
        else if (v.type == VALUE_BOOL) fprintf(stderr, "%s", v.as.boolean ? "true" : "false");
        else fprintf(stderr, "%s", "null");
    }
    fprintf(stderr, "]\n");
    if (vm->try_stack_top > 0) {
        fprintf(stderr, "  handlers: [");
        for (int i = 0; i < vm->try_stack_top; i++) {
            if (i > 0) fprintf(stderr, ", ");
            fprintf(stderr, "target=%d depth=%d", vm->try_stack[i], vm->try_stack_depths[i]);
        }
        fprintf(stderr, "]\n");
    }
}

static int bytecode_validate(BytecodeProgram *prog, const char *label) {
    int errors = 0;
    for (size_t i = 0; i < prog->count; i++) {
        Instruction *instr = &prog->instructions[i];
        switch (instr->op) {
            case OP_JUMP: case OP_JUMP_IF_FALSE: case OP_JUMP_IF_TRUE: case OP_JUMP_IF_NOT_NULL:
                if (instr->arg_i < 0 || (size_t)instr->arg_i >= prog->count) {
                    fprintf(stderr, "VALIDATE ERROR [%zu] %s: jump target %d out of range [0..%zu]\n", i, opcode_name(instr->op), instr->arg_i, prog->count - 1);
                    errors++;
                }
                break;
            case OP_TRY_PUSH:
                if (instr->arg_i < 0 || (size_t)instr->arg_i >= prog->count) {
                    fprintf(stderr, "VALIDATE ERROR [%zu] TRY_PUSH: handler target %d out of range\n", i, instr->arg_i);
                    errors++;
                }
                break;
            default:
                break;
        }
    }
    if (errors == 0) {
        if (g_dump_bytecode) fprintf(stderr, "VALIDATE [%s]: %zu instructions, no errors\n", label, prog->count);
    }
    else fprintf(stderr, "VALIDATE [%s]: %d errors found\n", label, errors);
    return errors;
}
