static int run_program(Program *program) {
    int ok = 1;
    Environment env = {0};
    g_program = program;
    g_root_env = &env;
    strip_block_comments(program);
    task_queue_init();
    object_registry_init();

    ParseError err = {0};
    if (!collect_imports(program, &env, &err)) {
        fprintf(stderr, "Error: %s\n", err.message);
        if (err.line > 0) fprintf(stderr, "  at %s:%d:%d\n", program->path ? program->path : "<unknown>", err.line, err.column > 0 ? err.column : 1);
        if (g_call_stack_count) {
            fprintf(stderr, "Stack trace:\n");
            for (size_t i = g_call_stack_count; i > 0; i--) {
                fprintf(stderr, "  at %s\n", g_call_stack[i - 1]);
            }
        }
        ok = 0;
    } else if (!collect_functions(program, &err)) {
        fprintf(stderr, "Error: %s\n", err.message);
        if (err.line > 0) fprintf(stderr, "  at %s:%d:%d\n", program->path ? program->path : "<unknown>", err.line, err.column > 0 ? err.column : 1);
        if (g_call_stack_count) {
            fprintf(stderr, "Stack trace:\n");
            for (size_t i = g_call_stack_count; i > 0; i--) {
                fprintf(stderr, "  at %s\n", g_call_stack[i - 1]);
            }
        }
        ok = 0;
    } else if (!execute_range(program, &env, 0, program->count, &err)) {
        fprintf(stderr, "Error: %s\n", err.message);
        if (err.line > 0) fprintf(stderr, "  at %s:%d:%d\n", program->path ? program->path : "<unknown>", err.line, err.column > 0 ? err.column : 1);
        if (g_call_stack_count) {
            fprintf(stderr, "Stack trace:\n");
            for (size_t i = g_call_stack_count; i > 0; i--) {
                fprintf(stderr, "  at %s\n", g_call_stack[i - 1]);
            }
        }
        ok = 0;
    }

    free_environment(&env);
    g_root_env = NULL;
    object_registry_free_all();
    free_functions();
    free_modules();
    free_call_stack();
    task_cleanup();
    return ok ? 0 : 1;
}

static QS_UNUSED_FN int run_file(const char *path) {
    Program program = {0};
    int loaded = load_program_file(path, &program);
    int result = loaded ? run_program(&program) : 1;
    free_program(&program);
    return result;
}

static int check_program_blocks(Program *program, ParseError *err) {
    for (size_t i = 0; i < program->count; i++) {
        size_t after = 0;
        size_t brace = 0;
        if ((line_starts_with(program->items[i], "repeat", &after) ||
             line_starts_with(program->items[i], "while", &after) ||
             line_starts_with(program->items[i], "if", &after) ||
             line_starts_with(program->items[i], "try", &after) ||
             line_starts_with(program->items[i], "func", &after) ||
             line_starts_with(program->items[i], "class", &after) ||
             line_starts_with(program->items[i], "maybe", &after) ||
             line_starts_with(program->items[i], "else", &after) ||
             line_starts_with(program->items[i], "oops", &after)) &&
            line_has_open_brace(program->items[i], after, &brace)) {
            size_t close_line = 0;
            if (!find_block_end(program, i, program->count, &close_line, err)) {
                return 0;
            }
        }
    }
    return 1;
}

static QS_UNUSED_FN int check_file(const char *path) {
    Program program = {0};
    if (!load_program_file(path, &program)) {
        return 1;
    }
    g_program = &program;
    strip_block_comments(&program);
    Environment env = {0};
    ParseError err = {0};
    int ok = check_program_blocks(&program, &err) &&
             collect_imports(&program, &env, &err) &&
             collect_functions(&program, &err);
    if (ok) {
        printf("ok: %s\n", path);
    } else {
        fprintf(stderr, "Error: %s\n", err.message);
        if (err.line > 0) fprintf(stderr, "  at %s:%d:%d\n", program.path ? program.path : path, err.line, err.column > 0 ? err.column : 1);
    }
    free_environment(&env);
    object_registry_free_all();
    free_functions();
    free_modules();
    free_call_stack();
    free_program(&program);
    return ok ? 0 : 1;
}

static QS_UNUSED_FN int fmt_file(const char *path) {
    Program program = {0};
    if (!load_program_file(path, &program)) {
        return 1;
    }
    strip_block_comments(&program);
    int indent = 0;
    int prev_blank = 0;
    for (size_t i = 0; i < program.count; i++) {
        char *trimmed = trim_copy(program.items[i], strlen(program.items[i]));

        if (trimmed[0] == '\0') {
            if (!prev_blank) {
                printf("\n");
                prev_blank = 1;
            }
            free(trimmed);
            continue;
        }
        prev_blank = 0;

        if (trimmed[0] == '}') {
            indent--;
            if (indent < 0) {
                indent = 0;
            }
        }

        for (int n = 0; n < indent; n++) {
            printf("    ");
        }

        printf("%s\n", trimmed);

        size_t brace = 0;
        if (line_has_open_brace(trimmed, 0, &brace) && trimmed[0] != '}') {
            indent++;
        }
        free(trimmed);
    }
    free_program(&program);
    return 0;
}

static QS_UNUSED_FN int run_source(const char *source, const char *virtual_path) {
    Program program = {0};
    program.path = qs_strdup(virtual_path ? virtual_path : "<compiled>");
    program.dir = path_dirname(program.path);

    const char *start = source;
    const char *cursor = source;
    while (1) {
        if (*cursor == '\n' || *cursor == '\0') {
            size_t len = (size_t)(cursor - start);
            if (len && start[len - 1] == '\r') {
                len--;
            }
            program_push(&program, qs_strndup(start, len));
            if (*cursor == '\0') {
                break;
            }
            start = cursor + 1;
        }
        cursor++;
    }

    int result = run_program(&program);
    free_program(&program);
    return result;
}

static QS_UNUSED_FN int run_repl(void) {
    printf("QuantoScript 1.0 (type \"exit\" to quit)\n");
    Environment env = {0};
    ParseError err = {0};
    char line[4096];
    char buffer[65536];
    int in_block = 0;

    Program repl_prog = {0};
    repl_prog.path = qs_strdup("<repl>");
    repl_prog.dir = qs_strdup(".");

    while (1) {
        if (in_block) {
            printf("... ");
        } else {
            printf(">>> ");
        }
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (!in_block && (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0)) {
            break;
        }
        if (len == 0 && !in_block) {
            continue;
        }

        if (in_block) {
            size_t buf_len = strlen(buffer);
            if (buf_len + len + 2 < sizeof(buffer)) {
                strcat(buffer, "\n");
                strcat(buffer, line);
            }

            int depth = 0;
            for (size_t i = 0; buffer[i]; i++) {
                if (buffer[i] == '{') depth++;
                else if (buffer[i] == '}') depth--;
            }
            if (depth <= 0) {
                in_block = 0;

                err.line = 0;
                err.column = 0;
                err.message[0] = '\0';

                size_t check_pos = 0;
                if (line_starts_with(buffer, "from", &check_pos) || line_starts_with(buffer, "import", &check_pos)) {
                    program_push(&repl_prog, qs_strdup(buffer));
                    if (!collect_imports(&repl_prog, &env, &err)) {
                    fprintf(stderr, "Error: %s\n", err.message);
                    if (err.line > 0) {
                        fprintf(stderr, "  at <repl>:%d:%d\n", err.line, err.column > 0 ? err.column : 1);
                    }
                    }
                } else {
                    size_t start_line = repl_prog.count;

                    size_t check_kw = 0;
                    if (line_starts_with(buffer, "class", &check_kw)) {
                        char *merged = qs_strdup(buffer);
                        for (size_t m = 0; merged[m]; m++) {
                            if (merged[m] == '\n') merged[m] = ' ';
                        }
                        program_push(&repl_prog, merged);
                    } else {
                        char *buf_copy = qs_strdup(buffer);
                        char *saveptr = NULL;
                        char *tok = strtok_r(buf_copy, "\n", &saveptr);
                        while (tok) {
                            program_push(&repl_prog, qs_strdup(tok));
                            tok = strtok_r(NULL, "\n", &saveptr);
                        }
                        free(buf_copy);
                    }

                    ParseError collect_err = {0};
                    collect_functions(&repl_prog, &collect_err);

                    ParseError exec_err = {0};
                    if (!execute_range(&repl_prog, &env, start_line, repl_prog.count, &exec_err)) {
                        fprintf(stderr, "Error: %s\n", exec_err.message);
                        if (exec_err.line > 0) {
                            fprintf(stderr, "  at <repl>:%d:%d\n", exec_err.line, exec_err.column > 0 ? exec_err.column : 1);
                        }
                        if (g_call_stack_count) {
                            fprintf(stderr, "Stack trace:\n");
                            for (size_t i = g_call_stack_count; i > 0; i--) {
                                fprintf(stderr, "  at %s\n", g_call_stack[i - 1]);
                            }
                        }
                    }
                }
                buffer[0] = '\0';
            }
            continue;
        }

        {
            int depth = 0;
            for (size_t i = 0; i < len; i++) {
                if (line[i] == '{') depth++;
                else if (line[i] == '}') depth--;
            }
            if (depth > 0) {
                in_block = 1;
                strcpy(buffer, line);
                continue;
            }
        }

        err.line = 0;
        err.column = 0;
        err.message[0] = '\0';

        size_t assignment_pos = 0;
        char assign_op = 0;

        size_t kw_check = 0;
        int is_statement = (line_starts_with(line, "func", &kw_check) ||
                           line_starts_with(line, "class", &kw_check) ||
                           line_starts_with(line, "if", &kw_check) ||
                           line_starts_with(line, "while", &kw_check) ||
                           line_starts_with(line, "repeat", &kw_check) ||
                           line_starts_with(line, "try", &kw_check) ||
                           line_starts_with(line, "from", &kw_check) ||
                           line_starts_with(line, "import", &kw_check) ||
                           line_starts_with(line, "print", &kw_check) ||
                           line_starts_with(line, "oops", &kw_check));
        if (is_statement) {
            size_t start_line = repl_prog.count;
            program_push(&repl_prog, qs_strdup(line));

            if (line_starts_with(line, "from", &kw_check) || line_starts_with(line, "import", &kw_check)) {
                if (!collect_imports(&repl_prog, &env, &err)) {
                    fprintf(stderr, "Error: %s\n", err.message);
                    if (err.line > 0) {
                        fprintf(stderr, "  at <repl>:%d:%d\n", err.line, err.column > 0 ? err.column : 1);
                    }
                }
            } else {
                ParseError collect_err = {0};
                collect_functions(&repl_prog, &collect_err);
                ParseError exec_err = {0};
                if (!execute_range(&repl_prog, &env, start_line, repl_prog.count, &exec_err)) {
                    fprintf(stderr, "Error: %s\n", exec_err.message);
                    if (exec_err.line > 0) {
                        fprintf(stderr, "  at <repl>:%d:%d\n", exec_err.line, exec_err.column > 0 ? exec_err.column : 1);
                    }
                    if (g_call_stack_count) {
                        fprintf(stderr, "Stack trace:\n");
                        for (size_t i = g_call_stack_count; i > 0; i--) {
                            fprintf(stderr, "  at %s\n", g_call_stack[i - 1]);
                        }
                    }
                }
            }
        } else if (find_assignment(line, &assignment_pos, &assign_op)) {
            if (execute_assignment(&env, line, assignment_pos, assign_op, 1, &err)) {
            } else {
                fprintf(stderr, "Error: %s\n", err.message);
                if (err.line > 0) {
                    fprintf(stderr, "  at <repl>:%d:%d\n", err.line, err.column > 0 ? err.column : 1);
                }
            }
        } else {
            size_t start_line = repl_prog.count;
            program_push(&repl_prog, qs_strdup(line));

            size_t imp_check = 0;
            if (line_starts_with(line, "from", &imp_check) || line_starts_with(line, "import", &imp_check)) {
                if (!collect_imports(&repl_prog, &env, &err)) {
                    fprintf(stderr, "Error: %s\n", err.message);
                    if (err.line > 0) {
                        fprintf(stderr, "  at <repl>:%d:%d\n", err.line, err.column > 0 ? err.column : 1);
                    }
                }
            } else {
                ParseError collect_err = {0};
                collect_functions(&repl_prog, &collect_err);
                ParseError exec_err = {0};
                if (!execute_range(&repl_prog, &env, start_line, repl_prog.count, &exec_err)) {
                    fprintf(stderr, "Error: %s\n", exec_err.message);
                    if (exec_err.line > 0) {
                        fprintf(stderr, "  at <repl>:%d:%d\n", exec_err.line, exec_err.column > 0 ? exec_err.column : 1);
                    }
                    if (g_call_stack_count) {
                        fprintf(stderr, "Stack trace:\n");
                        for (size_t i = g_call_stack_count; i > 0; i--) {
                            fprintf(stderr, "  at %s\n", g_call_stack[i - 1]);
                        }
                    }
                }
            }
        }
    }

    free_program(&repl_prog);
    free_environment(&env);
    return 0;
}

static int pkg_install(const char *url);
static int pkg_install_deps(const char *pkg_dir, int depth);

static int pkg_install(const char *url) {
    char *repo_name = NULL;

    const char *github_prefix = "https://github.com/";
    if (strncmp(url, github_prefix, strlen(github_prefix)) == 0) {
        const char *path = url + strlen(github_prefix);

        size_t path_len = strlen(path);
        if (path_len > 4 && strcmp(path + path_len - 4, ".git") == 0) {
            path_len -= 4;
        }

        while (path_len > 0 && path[path_len - 1] == '/') {
            path_len--;
        }
        repo_name = qs_strndup(path, path_len);
    }

    else if (strncmp(url, "git@github.com:", 15) == 0) {
        const char *path = url + 15;
        size_t path_len = strlen(path);
        if (path_len > 4 && strcmp(path + path_len - 4, ".git") == 0) {
            path_len -= 4;
        }
        repo_name = qs_strndup(path, path_len);
    }
    else {
        fprintf(stderr, "Error: only GitHub URLs are supported\n");
        fprintf(stderr, "Usage: qs install https://github.com/owner/repo\n");
        return 1;
    }

    for (size_t i = 0; repo_name[i]; i++) {
        if (repo_name[i] == '/') {
            repo_name[i] = '_';
        }
    }

#if QS_WASM
    /* Package installation shells out to git; not available under WASI. */
#elif defined(_WIN32)
    _mkdir("packages");
#else
    mkdir("packages", 0755);
#endif

    char pkg_dir[1024];
    snprintf(pkg_dir, sizeof(pkg_dir), "packages/%s", repo_name);
#if QS_WASM
    if (0) {
#elif defined(_WIN32)
    DWORD attrs = GetFileAttributesA(pkg_dir);
    if (attrs != INVALID_FILE_ATTRIBUTES) {
#else
    struct stat st;
    if (stat(pkg_dir, &st) == 0) {
#endif
        printf("Package '%s' is already installed\n", repo_name);
        free(repo_name);
        return 0;
    }

    printf("Installing %s...\n", repo_name);
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "git clone --depth 1 %s.git packages/%s", url, repo_name);

    int result = system(cmd);
    if (result != 0) {
        fprintf(stderr, "Error: failed to clone repository\n");
        free(repo_name);
        return 1;
    }

    printf("Package '%s' installed successfully\n", repo_name);

    pkg_install_deps(pkg_dir, 0);

    printf("Import with: from \"packages/%s/main.qs\" import ...\n", repo_name);
    free(repo_name);
    return 0;
}

static int pkg_install_deps(const char *pkg_dir, int depth) {
    if (depth > 5) {
        fprintf(stderr, "Warning: dependency depth exceeded, skipping\n");
        return 0;
    }

    char manifest_path[2048];
    snprintf(manifest_path, sizeof(manifest_path), "%s/quanto.json", pkg_dir);

    FILE *f = fopen(manifest_path, "r");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *content = malloc(len + 1);
    fread(content, 1, len, f);
    content[len] = '\0';
    fclose(f);

    char *deps_start = strstr(content, "\"dependencies\"");
    if (!deps_start) {
        free(content);
        return 0;
    }
    deps_start += 14;
    while (*deps_start && (*deps_start == ' ' || *deps_start == ':' || *deps_start == '{')) deps_start++;

    int installed = 0;
    while (*deps_start && *deps_start != '}') {
        while (*deps_start && (*deps_start == ' ' || *deps_start == ',' || *deps_start == '\n')) deps_start++;
        if (*deps_start == '}') break;
        if (*deps_start != '"') break;
        deps_start++;
        char *key_end = strchr(deps_start, '"');
        if (!key_end) break;

        char dep_url[2048];
        snprintf(dep_url, sizeof(dep_url), "https://github.com/%.*s", (int)(key_end - deps_start), deps_start);
        deps_start = key_end + 1;
        while (*deps_start && *deps_start != ',' && *deps_start != '}') deps_start++;

        char *slash = strrchr(dep_url, '/');
        if (slash) {
            char dir_name[512];
            snprintf(dir_name, sizeof(dir_name), "%s", slash + 1);
            for (size_t i = 0; dir_name[i]; i++) {
                if (dir_name[i] == '/') dir_name[i] = '_';
            }
            char pkg_check[1024];
            snprintf(pkg_check, sizeof(pkg_check), "packages/%s", dir_name);

#if QS_WASM
            if (1) {
#elif defined(_WIN32)
            DWORD attrs = GetFileAttributesA(pkg_check);
            if (attrs == INVALID_FILE_ATTRIBUTES) {
#else
            struct stat st;
            if (stat(pkg_check, &st) != 0) {
#endif
                printf("  Installing dependency: %s\n", dir_name);
                pkg_install(dep_url);
                installed++;
            }
        }
    }

    free(content);
    return installed;
}

static QS_UNUSED_FN int pkg_list(void) {
#if QS_WASM
    printf("No packages installed\n");
    return 0;
#else
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA("packages");
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
#else
    struct stat st;
    if (stat("packages", &st) != 0 || !S_ISDIR(st.st_mode)) {
#endif
        printf("No packages installed\n");
        return 0;
    }

    printf("Installed packages:\n");
#ifdef _WIN32
    WIN32_FIND_DATAA find_data;
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "packages\\*");
    HANDLE hFind = FindFirstFileA(pattern, &find_data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (find_data.cFileName[0] != '.') {
                printf("  %s\n", find_data.cFileName);
            }
        } while (FindNextFileA(hFind, &find_data));
        FindClose(hFind);
    }
#else
    DIR *dir = opendir("packages");
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] != '.') {
                printf("  %s\n", entry->d_name);
            }
        }
        closedir(dir);
    }
#endif
    return 0;
#endif /* QS_WASM */
}

static QS_UNUSED_FN int pkg_remove(const char *name) {
    char pkg_dir[1024];
    snprintf(pkg_dir, sizeof(pkg_dir), "packages/%s", name);

#if QS_WASM
    (void)pkg_dir;
    fprintf(stderr, "Error: package '%s' is not installed\n", name);
    return 1;
#else
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(pkg_dir);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
#else
    struct stat st;
    if (stat(pkg_dir, &st) != 0) {
#endif
        fprintf(stderr, "Error: package '%s' is not installed\n", name);
        return 1;
    }

    char cmd[2048];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\"", pkg_dir);
#else
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", pkg_dir);
#endif

    int result = system(cmd);
    if (result != 0) {
        fprintf(stderr, "Error: failed to remove package\n");
        return 1;
    }

    printf("Package '%s' removed\n", name);
    return 0;
#endif /* QS_WASM */
}

static QS_UNUSED_FN int run_tests_in_dir(const char *dir) {
    int passed = 0;
    int failed = 0;
#if QS_WASM
    (void)dir;
    return 0;
#elif defined(_WIN32)
    WIN32_FIND_DATAA find_data;
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*.qs", dir);
    HANDLE hFind = FindFirstFileA(pattern, &find_data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            char path[1024];
            snprintf(path, sizeof(path), "%s\\%s", dir, find_data.cFileName);
            printf("Running %s...\n", find_data.cFileName);
            int result = run_file(path);
            if (result == 0) {
                printf("  PASS\n");
                passed++;
            } else {
                printf("  FAIL\n");
                failed++;
            }
        } while (FindNextFileA(hFind, &find_data));
        FindClose(hFind);
    }
#else
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            size_t len = strlen(entry->d_name);
            if (len > 6 && strcmp(entry->d_name + len - 6, "_test.qs") == 0) {
                char path[1024];
                snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
                printf("Running %s...\n", entry->d_name);
                int result = run_file(path);
                if (result == 0) { printf("  PASS\n"); passed++; }
                else { printf("  FAIL\n"); failed++; }
            }
        }
        closedir(d);
    }
#endif
    printf("\n%d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}

#ifndef QUANTO_NO_MAIN
int lint_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: cannot open '%s'\n", path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *content = malloc(len + 1);
    fread(content, 1, len, f);
    content[len] = '\0';
    fclose(f);

    int warnings = 0;
    int line_num = 1;
    const char *line = content;

    while (*line) {
        const char *p = line;
        while (*p && *p != '\n') {
            if (*p == '=' && *(p+1) != '=' && *(p+1) != '>' && p != line && *(p-1) != '!' && *(p-1) != '<' && *(p-1) != '=') {
                const char *rhs = p + 1;
                while (*rhs && *rhs != '\n') {
                    if (*rhs == '=' && *(rhs+1) == '=' && *(rhs-1) != '!' && *(rhs-1) != '<') {
                        fprintf(stderr, "%s:%d: warning: possible '=' instead of '=='\n", path, line_num);
                        warnings++;
                        break;
                    }
                    rhs++;
                }
            }
            p++;
        }

        if (strncmp(line, "} oops {", 8) == 0 || strncmp(line, "} oops{", 7) == 0) {
            const char *next = line;
            while (*next && *next != '\n') next++;
            if (*next == '\n') {
                next++;
                while (*next == ' ' || *next == '\t') next++;
                if (*next == '}') {
                    fprintf(stderr, "%s:%d: warning: empty oops block\n", path, line_num);
                    warnings++;
                }
            }
        }

        while (*line && *line != '\n') line++;
        if (*line == '\n') line++;
        line_num++;
    }

    free(content);

    if (warnings == 0) {
        printf("%s: no issues found\n", path);
    } else {
        printf("%s: %d warning(s)\n", path, warnings);
    }
    return warnings > 0 ? 1 : 0;
}

int doc_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: cannot open '%s'\n", path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *content = malloc(len + 1);
    fread(content, 1, len, f);
    content[len] = '\0';
    fclose(f);

    const char *filename = strrchr(path, '/');
    if (!filename) filename = strrchr(path, '\\');
    if (filename) filename++; else filename = path;

    printf("# %s\n\n", filename);

    int line_num = 1;
    const char *line = content;
    char prev_comment[4096] = {0};
    int has_comment = 0;

    while (*line) {
        const char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        if (*trimmed == '#') {
            if (has_comment) strcat(prev_comment, "\n");
            strncat(prev_comment, trimmed + 1, sizeof(prev_comment) - strlen(prev_comment) - 1);
            has_comment = 1;
        } else if (strncmp(trimmed, "func ", 5) == 0) {
            if (has_comment) {
                char *c = prev_comment;
                while (*c == '\n') c++;
                printf("%s\n", c);
                prev_comment[0] = '\0';
                has_comment = 0;
            }

            printf("```\n%s\n```\n\n", trimmed);
        }

        while (*line && *line != '\n') line++;
        if (*line == '\n') line++;
        line_num++;
    }

    free(content);
    return 0;
}

#ifdef _WIN32
static UINT g_console_output_cp = 0;
static UINT g_console_input_cp = 0;

static void qs_restore_console(void) {
    if (g_console_output_cp) {
        SetConsoleOutputCP(g_console_output_cp);
    }
    if (g_console_input_cp) {
        SetConsoleCP(g_console_input_cp);
    }
}
#endif

static void qs_init_console(void) {
#ifdef _WIN32
    g_console_output_cp = GetConsoleOutputCP();
    g_console_input_cp = GetConsoleCP();
    if (g_console_output_cp) {
        SetConsoleOutputCP(CP_UTF8);
    }
    if (g_console_input_cp) {
        SetConsoleCP(CP_UTF8);
    }
    if (g_console_output_cp || g_console_input_cp) {
        atexit(qs_restore_console);
    }
#endif
}

static int run_lsp_server(void);

int main(int argc, char **argv) {
    qs_init_console();
    if (argc == 1) {
        return run_repl();
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stderr, "usage:\n");
            fprintf(stderr, "  qs                    interactive REPL\n");
            fprintf(stderr, "  qs <file.qs>          run with tree-walk interpreter\n");
            fprintf(stderr, "  qs run <file.qs|qvm>  run with bytecode VM\n");
            fprintf(stderr, "  qs build <file.qs> [-o out.qvm]  compile to .qvm bytecode\n");
            fprintf(stderr, "  qs vm <file.qs>       run with bytecode VM (alias for run)\n");
            fprintf(stderr, "  qs init               create project scaffold\n");
            fprintf(stderr, "  qs check <file.qs>    validate without execution\n");
            fprintf(stderr, "  qs fmt <file.qs>      format source code\n");
            fprintf(stderr, "  qs lint <file.qs>     check for common issues\n");
            fprintf(stderr, "  qs doc <file.qs>      generate documentation\n");
            fprintf(stderr, "  qs profile <file.qs>  profile function calls\n");
            fprintf(stderr, "  qs native <file.qs> [--build]  compile to C (a_native.c); --build also produces a binary\n");
            fprintf(stderr, "  qs compile <file.qs> [output]  compile to native binary\n");
            fprintf(stderr, "  qs test [directory]   run *_test.qs files\n");
            fprintf(stderr, "  qs install <url>      install a package\n");
            fprintf(stderr, "  qs list               list installed packages\n");
            fprintf(stderr, "  qs remove <package>   remove a package\n");
            fprintf(stderr, "  qs home               show QUANTO_HOME directory\n");
            fprintf(stderr, "  qs version            show version\n");
            fprintf(stderr, "  qs lsp                run the language server (stdio)\n");
            fprintf(stderr, "\nflags:\n");
            fprintf(stderr, "  --dump-bytecode  print compiled bytecode to stderr\n");
            fprintf(stderr, "  --trace          print per-instruction VM trace\n");
            fprintf(stderr, "  --sandbox [path] restrict file access\n");
            return 0;
        }
    }

    int sandbox = 0;
    int sandbox_path_idx = 0;
    int native_build_flag = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sandbox") == 0) {
            sandbox = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                sandbox_path_idx = i + 1;
            }
        }
        if (strcmp(argv[i], "--dump-bytecode") == 0) g_dump_bytecode = 1;
        if (strcmp(argv[i], "--trace") == 0) g_trace_vm = 1;
        if (strcmp(argv[i], "--build") == 0) native_build_flag = 1;
    }
    if (sandbox) {
        g_sandbox_mode = 1;
        if (sandbox_path_idx > 0) {
            sandbox_set_root(argv[sandbox_path_idx]);
        }
    }

    const char *cmd = NULL;
    const char *file_arg = NULL;
    const char *extra_arg = NULL;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "--sandbox") == 0) {
                if (i + 1 < argc && argv[i + 1][0] != '-') i++;
            }
            continue;
        }
        if (cmd == NULL) {
            cmd = argv[i];
        } else if (file_arg == NULL) {
            file_arg = argv[i];
        } else if (extra_arg == NULL) {
            extra_arg = argv[i];
        }
    }

    if (cmd == NULL && file_arg == NULL) {
    }

    if (cmd != NULL && file_arg == NULL && cmd[0] != '-' &&
        (strlen(cmd) > 3 && strcmp(cmd + strlen(cmd) - 3, ".qs") == 0)) {
        return run_file(cmd);
    }
    if (cmd == NULL && file_arg != NULL) {
        return run_file(file_arg);
    }

    if (cmd && strcmp(cmd, "home") == 0) {
        printf("%s\n", get_quanto_home());
        return 0;
    }
    if (cmd && strcmp(cmd, "version") == 0) {
        printf("QuantoScript %s\n", QS_VERSION);
        return 0;
    }
    if (cmd && strcmp(cmd, "lsp") == 0) {
        return run_lsp_server();
    }
    if (cmd && strcmp(cmd, "init") == 0) {
#if QS_WASM
        /* No directory creation on WASI; files are written to the preopen. */
#elif defined(_WIN32)
        _mkdir("test");
#else
        mkdir("test", 0755);
#endif

        FILE *f = fopen("main.qs", "w");
        if (f) {
            fprintf(f, "# Main entry point\n\nprint(\"Hello from QuantoScript!\")\n");
            fclose(f);
        }

        f = fopen("test/example_test.qs", "w");
        if (f) {
            fprintf(f, "# Example test\nassert_eq(1 + 1, 2)\n");
            fclose(f);
        }

        f = fopen("quanto.json", "w");
        if (f) {
            fprintf(f, "{\n  \"name\": \"my-project\",\n  \"version\": \"%s\",\n  \"dependencies\": {}\n}\n", QS_VERSION);
            fclose(f);
        }
        printf("Project initialized!\n");
        printf("  main.qs          - entry point\n");
        printf("  test/            - test directory\n");
        printf("  quanto.json      - project manifest\n");
        printf("\nRun: qs main.qs\n");
        return 0;
    }
    if (cmd && strcmp(cmd, "lint") == 0) {
        return lint_file(file_arg ? file_arg : "main.qs");
    }
    if (cmd && strcmp(cmd, "doc") == 0) {
        return doc_file(file_arg ? file_arg : "main.qs");
    }
    if (cmd && strcmp(cmd, "profile") == 0) {
        g_profiling = 1;
        int result = run_file(file_arg ? file_arg : "main.qs");
        g_profiling = 0;
        return result;
    }
    if (cmd && (strcmp(cmd, "vm") == 0 || strcmp(cmd, "run") == 0)) {
        const char *path = file_arg ? file_arg : "main.qs";
        size_t len = strlen(path);
        if (len >= 4 && strcmp(path + len - 4, ".qvm") == 0) {
            return run_qvm_file(path);
        }
        return run_bytecode_file(path);
    }
    if (cmd && strcmp(cmd, "build") == 0) {
        if (file_arg == NULL) {
            fprintf(stderr, "build requires a .qs file\n");
            return 1;
        }
        BytecodeProgram prog = {0};
        if (!compile_file_to_bytecode(file_arg, &prog)) {
            fprintf(stderr, "Error: compilation failed\n");
            return 1;
        }

        const char *outpath = NULL;
        char derived[1024];
        if (extra_arg) {
            outpath = extra_arg;
        } else {
            snprintf(derived, sizeof(derived), "%s", file_arg);
            size_t plen = strlen(derived);
            if (plen >= 3 && strcmp(derived + plen - 3, ".qs") == 0) {
                strcpy(derived + plen - 3, ".qvm");
            } else {
                strcat(derived, ".qvm");
            }
            outpath = derived;
        }
        int ok = qvm_serialize(&prog, g_compiled_funcs, g_compiled_func_count, file_arg, outpath);
        if (ok) {
            printf("built %s -> %s\n", file_arg, outpath);
        } else {
            fprintf(stderr, "Error: failed to write %s\n", outpath);
        }
        bytecode_free(&prog);
        return ok ? 0 : 1;
    }
    if (cmd && strcmp(cmd, "test") == 0) {
        return run_tests_in_dir(file_arg ? file_arg : ".");
    }
    if (cmd && strcmp(cmd, "list") == 0) {
        return pkg_list();
    }
    if (cmd && strcmp(cmd, "install") == 0) {
        if (file_arg == NULL) {
            fprintf(stderr, "install requires a URL\n");
            return 1;
        }
        return pkg_install(file_arg);
    }
    if (cmd && strcmp(cmd, "remove") == 0) {
        if (file_arg == NULL) {
            fprintf(stderr, "remove requires a package name\n");
            return 1;
        }
        return pkg_remove(file_arg);
    }
    if (cmd && strcmp(cmd, "native") == 0) {
        if (file_arg == NULL) {
            fprintf(stderr, "native requires a .qs file\n");
            return 1;
        }
        const char *c_path = "a_native.c";
        int rc = native_compile(file_arg, c_path);
        if (rc != 0) return rc;
        if (!native_build_flag) {
            printf("wrote %s\n", c_path);
            return 0;
        }
        char out_path[1024];
        const char *base = file_arg;
        for (const char *p = file_arg; *p; p++) {
            if (*p == '/' || *p == '\\') base = p + 1;
        }
        snprintf(out_path, sizeof(out_path), "%s", base);
        char *dot = strrchr(out_path, '.');
        if (dot && strcmp(dot, ".qs") == 0) *dot = '\0';
#ifdef _WIN32
        strncat(out_path, ".exe", sizeof(out_path) - strlen(out_path) - 1);
#endif
        int brc = native_build_c(c_path, out_path);
        if (brc != 0) fprintf(stderr, "Error: failed to build native binary\n");
        return brc;
    }
    if (cmd && strcmp(cmd, "check") == 0) {
        if (file_arg == NULL) {
            fprintf(stderr, "check requires a .qs file\n");
            return 1;
        }
        return check_file(file_arg);
    }
    if (cmd && strcmp(cmd, "fmt") == 0) {
        if (file_arg == NULL) {
            fprintf(stderr, "fmt requires a .qs file\n");
            return 1;
        }
        return fmt_file(file_arg);
    }
    if (cmd && strcmp(cmd, "compile") == 0) {
        if (file_arg == NULL) {
            fprintf(stderr, "compile requires a .qs file\n");
            return 1;
        }
        const char *out = extra_arg;
        if (!out) {
#ifdef _WIN32
            out = "a.exe";
#else
            out = "a.out";
#endif
        }
        return compile_file(file_arg, out);
    }
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "  qs                  interactive REPL\n");
    fprintf(stderr, "  qs init             create project scaffold\n");
    fprintf(stderr, "  qs <file.qs>        run with tree-walk interpreter\n");
    fprintf(stderr, "  qs run <file.qs|qvm> run with bytecode VM\n");
    fprintf(stderr, "  qs build <file.qs> [-o out.qvm]  compile to .qvm bytecode\n");
    fprintf(stderr, "  qs vm <file.qs>     run with bytecode VM (alias for run)\n");
    fprintf(stderr, "  qs check <file.qs>  validate without execution\n");
    fprintf(stderr, "  qs fmt <file.qs>    format source code\n");
    fprintf(stderr, "  qs lint <file.qs>   check for common issues\n");
    fprintf(stderr, "  qs doc <file.qs>    generate documentation\n");
    fprintf(stderr, "  qs profile <file.qs>  profile function calls\n");
    fprintf(stderr, "  qs native <file.qs> [--build]  compile to C (a_native.c); --build also produces a binary\n");
    fprintf(stderr, "  qs compile <file.qs> [output]  compile to native binary\n");
    fprintf(stderr, "  qs test [directory]  run *_test.qs files\n");
    fprintf(stderr, "  qs install <github-url>  install a package\n");
    fprintf(stderr, "  qs list               list installed packages\n");
    fprintf(stderr, "  qs remove <package>   remove a package\n");
    fprintf(stderr, "  qs lsp                run the language server (stdio)\n");
    fprintf(stderr, "\nflags:\n");
    fprintf(stderr, "  --dump-bytecode    print compiled bytecode to stderr\n");
    fprintf(stderr, "  --trace            print per-instruction VM trace\n");
    fprintf(stderr, "  --sandbox [path]   restrict file access\n");
    fprintf(stderr, "  --build            with 'qs native', also build the C into a binary\n");
        return 1;
}
#endif
