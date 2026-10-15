static int compile_file_to_bytecode(const char *path, BytecodeProgram *prog) {
    Program *program = calloc(1, sizeof(Program));
    if (!load_program_file(path, program)) { free(program); return 0; }

    strip_block_comments(program);

    ast_normalize_source(program);

    g_program = program;

    ParseError ferr = {0};
    collect_functions(program, &ferr);

    ast_parse_failed = 0;
    ast_parse_failed_line = 0;
    ast_parse_failed_msg[0] = '\0';
    ASTNode *ast = ast_parse_program(program);
    if (ast_parse_failed) {
        if (ast_parse_failed_msg[0] != '\0') {
            fprintf(stderr, "SyntaxError: %s\n", ast_parse_failed_msg);
        } else {
            fprintf(stderr, "SyntaxError: expected a value\n");
        }
        ast_parse_failed = 0;
        ast_parse_failed_line = 0;
        ast_parse_failed_msg[0] = '\0';
        if (ast) ast_free(ast);
        free_program(program); free(program);
        return 0;
    }
    if (!ast) {
        fprintf(stderr, "Parse error\n");
        free_program(program); free(program);
        return 0;
    }

    user_func_table_init();
    user_func_table_collect(ast);

    compiled_funcs_init();
    bytecode_init(prog);
    astc_compile_program(ast, prog);

    if (g_dump_bytecode) {
        QVMFile tmp = {0};
        tmp.main_prog = *prog;

        tmp.pool = NULL;
        tmp.pool_count = 0;
        tmp.source_path = NULL;

        tmp.func_count = g_compiled_func_count;
        tmp.funcs = g_compiled_funcs;
        qvm_disassemble(&tmp, "main");

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
    if (g_vm_source_dir) { free(g_vm_source_dir); g_vm_source_dir = NULL; }
    g_vm_source_dir = path_dirname(path);
    int result = vm_execute(&vm);
    if (vm.pending_exception) {
        fprintf(stderr, "%s\n", vm.exception_msg ? vm.exception_msg : "unhandled exception");
        result = 1;
    }
    free(vm.exception_msg);
    vm.exception_msg = NULL;
    g_vm_trace = 0;
    object_registry_free_all();
    free_environment(&env);
    bytecode_free(&prog);
    free_program(g_program);
    free(g_program);
    g_program = NULL;
    return result;
}
