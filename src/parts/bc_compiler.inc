// QuantoScript Bytecode Compiler — Thin wrapper
// Pipeline: load → normalize → parse AST → register functions → compile → execute

static int compile_file_to_bytecode(const char *path, BytecodeProgram *prog) {
    // Step 1: Load raw source
    Program *program = calloc(1, sizeof(Program));
    if (!load_program_file(path, program)) { free(program); return 0; }

    // Step 2: Normalize source
    ast_normalize_source(program);

    // Step 3: Set global program pointer
    g_program = program;

    // Step 4: Parse to AST
    ASTNode *ast = ast_parse_program(program);
    if (!ast) {
        fprintf(stderr, "Parse error\n");
        free_program(program); free(program);
        return 0;
    }

    // Step 5: Register user-defined functions
    ParseError ferr = {0};
    collect_functions(program, &ferr);

    // Step 6: Collect function/class symbols from AST
    user_func_table_init();
    user_func_table_collect(ast);

    // Step 7: Compile AST to bytecode
    compiled_funcs_init();
    bytecode_init(prog);
    astc_compile_program(ast, prog);

    // Debug: dump bytecode and validate
    if (g_dump_bytecode) {
        // Build a temporary QVMFile for professional disassembly
        QVMFile tmp = {0};
        tmp.main_prog = *prog;
        // Don't free these in qvm_free — they belong to the caller
        tmp.pool = NULL;
        tmp.pool_count = 0;
        tmp.source_path = NULL;
        // Build a temporary function array
        tmp.func_count = g_compiled_func_count;
        tmp.funcs = g_compiled_funcs;
        qvm_disassemble(&tmp, "main");
        // Restore — don't let qvm_free free the caller's data
        tmp.main_prog.instructions = NULL;
        tmp.main_prog.constants = NULL;
        tmp.funcs = NULL;
    }
    bytecode_validate(prog, "main");
    for (int i = 0; i < g_compiled_func_count; i++) {
        char label[128];
        snprintf(label, sizeof(label), "func_%s", g_compiled_funcs[i].name);
        bytecode_validate(&g_compiled_funcs[i].body, label);
    }

    ast_free(ast);
    return 1;
}

static QS_UNUSED_FN int run_bytecode_file(const char *path) {
    audit_init();
    object_registry_init();
    BytecodeProgram prog = {0};
    if (!compile_file_to_bytecode(path, &prog)) {
        fprintf(stderr, "Compilation failed\n");
        return 1;
    }
    Environment env = {0};
    VM vm;
    vm_init(&vm, &prog, &env);
    g_vm_trace = g_trace_vm;
    int result = vm_execute(&vm);
    g_vm_trace = 0;
    audit_report();
    object_registry_free_all();
    free_environment(&env);
    bytecode_free(&prog);
    free_program(g_program);
    free(g_program);
    g_program = NULL;
    return result;
}
