static void program_push(Program *program, char *line) {
    if (program->count == program->capacity) {
        size_t next_capacity = program->capacity ? program->capacity * 2 : 32;
        char **next = realloc(program->items, next_capacity * sizeof(char *));
        if (!next) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        program->items = next;
        program->capacity = next_capacity;
    }
    program->items[program->count++] = line;
}

// Split merged statement text into individual lines on a Program.
// Boundary detection: at brace depth 0, after }, ), digit, quote, or name-end,
// check if the next name is followed by =, (, or is a block keyword (while/if/repeat/try/func/class/from).
// Brace depth > 0 suppresses all boundary detection (preserves while/if/repeat bodies).
// Statement splitter: split merged single-line text into individual statements.
//
// The file loader merges multi-line blocks into single lines. This function
// reverses that for method bodies, loop bodies, etc.
//
// Algorithm: deterministic state machine (SCAN / STR_DQ / STR_SQ) with
// brace-depth tracking. A statement boundary is detected at every name-start
// character at brace depth 0 using:
//
//   1) BACKWARD: prev non-ws is } ) ] " ' digit → boundary
//   2) BACKWARD: start of text → boundary
//   3) BACKWARD: prev is name AND current word is block keyword → boundary
//   4) FORWARD:  current name is followed by [field accesses] = → boundary
//                (but NOT if current name is "return" — it takes a value)
//
// This correctly handles all cases including:
//   return expr          (rule 4 blocked: current is "return" → no split)
//   x = helper() return  (rule 1: prev=')' → split)
//   self.total + i i = 1 (rule 4: peek sees "= " → split)
//   while{...} y = 2     (rule 1: prev='}' → split)
//   print("a") print("b")(rule 1: prev=')' → split)
static void split_text_to_program(const char *text, Program *prog) {
    enum { SCAN, STR_DQ, STR_SQ } state = SCAN;
    int bd = 0;
    size_t ls = 0;
    size_t blen = strlen(text);

    #define EMIT(end) do { \
        size_t le = (end); \
        while (le > ls && (text[le-1] == ' ' || text[le-1] == '\t')) le--; \
        if (le > ls) { \
            char *ln = malloc(le - ls + 1); \
            memcpy(ln, text + ls, le - ls); \
            ln[le - ls] = '\0'; \
            program_push(prog, ln); \
        } \
        ls = (end); \
        while (ls < blen && (text[ls] == ' ' || text[ls] == '\t')) ls++; \
    } while(0)

    for (size_t bi = 0; bi <= blen; bi++) {
        char ch = (bi < blen) ? text[bi] : '\0';

        if (state == STR_DQ) {
            if (ch == '\\') { bi++; continue; }
            if (ch == '"')  state = SCAN;
            continue;
        }
        if (state == STR_SQ) {
            if (ch == '\\') { bi++; continue; }
            if (ch == '\'') state = SCAN;
            continue;
        }

        if (ch == '"')  { state = STR_DQ; continue; }
        if (ch == '\'') { state = STR_SQ; continue; }
        if (ch == '{')  { bd++; continue; }
        if (ch == '}')  { if (bd > 0) bd--; continue; }
        if (ch == '\0') { EMIT(bi); break; }

        if (bd > 0 || !is_name_start(ch)) continue;
        // Only at the FIRST character of a new identifier (not mid-name or after '.')
        if (bi > 0 && (is_name_part(text[bi - 1]) || text[bi - 1] == '.')) continue;

        // --- Statement boundary check at name-start, bd == 0 ---

        // Rule 1+2: backward — prev non-ws char determines boundary
        size_t prev = bi;
        while (prev > 0 && (text[prev-1] == ' ' || text[prev-1] == '\t')) prev--;
        char pc = (prev > 0) ? text[prev - 1] : '\0';

        int boundary = 0;
        if (prev == 0) {
            boundary = 1;
        } else if (pc == '}' || pc == ')' || pc == ']') {
            boundary = 1;
        } else if (pc == '"' || pc == '\'') {
            boundary = 1;
        } else if (pc >= '0' && pc <= '9') {
            boundary = 1;
        } else if (is_name_part(pc)) {
            // Rule 3: prev is name → boundary only if current is block keyword
            size_t ni = bi;
            while (ni < blen && is_name_part(text[ni])) ni++;
            size_t kl = ni - bi;
            boundary = ((kl == 5 && !strncmp(text+bi,"while",5)) ||
                        (kl == 2 && !strncmp(text+bi,"if",2)) ||
                        (kl == 6 && !strncmp(text+bi,"repeat",6)) ||
                        (kl == 3 && !strncmp(text+bi,"try",3)) ||
                        (kl == 4 && !strncmp(text+bi,"func",4)) ||
                        (kl == 5 && !strncmp(text+bi,"class",5)) ||
                        (kl == 4 && !strncmp(text+bi,"from",4)));
        }

        // Rule 4: forward — peek at what follows the current name
        if (!boundary) {
            size_t ni = bi;
            while (ni < blen && is_name_part(text[ni])) ni++;
            // Skip field accesses: .name
            size_t pi = ni;
            while (pi < blen && text[pi] == '.') {
                pi++;
                while (pi < blen && is_name_part(text[pi])) pi++;
            }
            // Skip whitespace
            while (pi < blen && (text[pi] == ' ' || text[pi] == '\t')) pi++;
            if (pi < blen && text[pi] == '=') {
                // Don't split "return expr" — return takes a value
                size_t kl = ni - bi;
                if (!(kl == 6 && !strncmp(text+bi,"return",6)))
                    boundary = 1;
            }
        }

        if (boundary) EMIT(bi);
    }

    if (ls < blen) EMIT(blen);
    if (prog->count == 0) program_push(prog, qs_strdup(""));

    #undef EMIT
}

static char *path_dirname(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *last = slash > backslash ? slash : backslash;
    if (!last) {
        return qs_strdup(".");
    }
    return qs_strndup(path, (size_t)(last - path));
}

static QS_UNUSED_FN char *join_path(const char *dir, const char *path) {
    if ((isalpha((unsigned char)path[0]) && path[1] == ':') || path[0] == '/' || path[0] == '\\') {
        return qs_strdup(path);
    }

    size_t dir_len = strlen(dir);
    size_t path_len = strlen(path);
    int needs_slash = dir_len && dir[dir_len - 1] != '/' && dir[dir_len - 1] != '\\';
    char *joined = malloc(dir_len + (needs_slash ? 1 : 0) + path_len + 1);
    if (!joined) {
        fprintf(stderr, "out of memory\n");
        exit(2);
    }
    memcpy(joined, dir, dir_len);
    if (needs_slash) {
        joined[dir_len++] = '\\';
    }
    memcpy(joined + dir_len, path, path_len + 1);
    return joined;
}

static char *trim_copy(const char *start, size_t len);
static int quote_can_close(const char *text, size_t quote_pos, char list_end);

static void append_line_piece(char **target, const char *piece) {
    char *trimmed = trim_copy(piece, strlen(piece));
    if (!*target) {
        *target = trimmed;
        return;
    }
    size_t old_len = strlen(*target);
    size_t piece_len = strlen(trimmed);
    char *joined = malloc(old_len + piece_len + 2);
    if (!joined) {
        fprintf(stderr, "out of memory\n");
        exit(2);
    }
    memcpy(joined, *target, old_len);
    joined[old_len] = ' ';
    memcpy(joined + old_len + 1, trimmed, piece_len + 1);
    free(*target);
    free(trimmed);
    *target = joined;
}

static int line_opens_statement_block(const char *line) {
    size_t pos = 0;
    skip_spaces_text(line, &pos);
    if (line[pos] == '}') {
        pos++;
        skip_spaces_text(line, &pos);
    }
    const char *words[] = {"repeat", "while", "if", "try", "func", "maybe", "else", "oops"};
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
        size_t len = strlen(words[i]);
        if (strncmp(line + pos, words[i], len) == 0 && !is_name_part(line[pos + len])) {
            return strchr(line + pos + len, '{') != NULL;
        }
    }
    return 0;
}

static void update_multiline_depth(const char *line, int *paren_depth, int *bracket_depth, int *brace_depth, int track_braces) {
    int in_string = 0;
    char quote = '\0';
    for (size_t i = 0; line[i]; i++) {
        char ch = line[i];
        if (in_string && ch == quote && quote_can_close(line, i, ']')) {
            in_string = 0;
            quote = '\0';
            continue;
        }
        if (!in_string && (ch == '\'' || ch == '"')) {
            in_string = 1;
            quote = ch;
            continue;
        }
        if (in_string) {
            continue;
        }
        if (ch == '#') {
            break;
        }
        if (ch == '(') {
            (*paren_depth)++;
        } else if (ch == ')' && *paren_depth > 0) {
            (*paren_depth)--;
        } else if (ch == '[') {
            (*bracket_depth)++;
        } else if (ch == ']' && *bracket_depth > 0) {
            (*bracket_depth)--;
        } else if (track_braces && ch == '{') {
            (*brace_depth)++;
        } else if (track_braces && ch == '}' && *brace_depth > 0) {
            (*brace_depth)--;
        }
    }
}

static int load_program_file(const char *path, Program *program) {
    FILE *file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "QuantoScript: cannot open '%s'\n", path);
        return 0;
    }

    memset(program, 0, sizeof(*program));
    program->path = qs_strdup(path);
    program->dir = path_dirname(path);

    char buffer[4096];
    char *pending = NULL;
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;
    while (fgets(buffer, sizeof(buffer), file)) {
        size_t len = strlen(buffer);
        if (len && buffer[len - 1] == '\n') {
            buffer[--len] = '\0';
        }
        if (len && buffer[len - 1] == '\r') {
            buffer[--len] = '\0';
        }
        append_line_piece(&pending, buffer);
        int track_braces = brace_depth > 0 || paren_depth > 0 || bracket_depth > 0 || !line_opens_statement_block(buffer);
        update_multiline_depth(buffer, &paren_depth, &bracket_depth, &brace_depth, track_braces);
        if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
            program_push(program, pending);
            pending = NULL;
        }
    }
    if (pending) {
        program_push(program, pending);
    }

    int ok = 1;
    if (ferror(file)) {
        fprintf(stderr, "QuantoScript: failed while reading '%s'\n", path);
        ok = 0;
    }
    fclose(file);
    return ok;
}

static int validate_variable_name(const char *name, int line, ParseError *err) {
    if (!name[0]) {
        set_error(err, line, 1, "missing variable name before '='");
        return 0;
    }
    if (!is_name_start(name[0])) {
        char msg[256];
        snprintf(msg, sizeof(msg), "variable name must start with a letter or '_', got '%c'", name[0]);
        set_error(err, line, 1, msg);
        return 0;
    }
    for (size_t i = 1; name[i]; i++) {
        if (!is_name_part(name[i])) {
            char msg[256];
            snprintf(msg, sizeof(msg), "invalid character '%c' in variable name", name[i]);
            set_error(err, line, (int)i + 1, msg);
            return 0;
        }
    }
    return 1;
}

static char *trim_copy(const char *start, size_t len) {
    while (len && isspace((unsigned char)*start)) {
        start++;
        len--;
    }
    while (len && isspace((unsigned char)start[len - 1])) {
        len--;
    }
    return qs_strndup(start, len);
}

static int quote_can_close(const char *text, size_t quote_pos, char list_end) {
    size_t pos = quote_pos + 1;
    skip_spaces_text(text, &pos);
    return text[pos] == '\0' || text[pos] == '#' || text[pos] == ',' || text[pos] == ':' ||
           text[pos] == list_end || text[pos] == ')' || text[pos] == '{' || text[pos] == '[' || text[pos] == '}' || text[pos] == '.' ||
           is_name_start(text[pos]) ||
           (strncmp(text + pos, "in", 2) == 0 && !is_name_part(text[pos + 2])) ||
           (strncmp(text + pos, "import", 6) == 0 && !is_name_part(text[pos + 6])) ||
           (strncmp(text + pos, "and", 3) == 0 && !is_name_part(text[pos + 3])) ||
           (strncmp(text + pos, "or", 2) == 0 && !is_name_part(text[pos + 2])) ||
           strchr("+-*/%=<>!", text[pos]) != NULL;
}

static int rest_is_space_or_comment(const char *text, size_t pos) {
    skip_spaces_text(text, &pos);
    return text[pos] == '\0' || text[pos] == '#';
}

static void strip_block_comments(Program *program) {
    int in_comment = 0;

    for (size_t line = 0; line < program->count; line++) {
        char *text = program->items[line];
        int in_string = 0;
        char quote = '\0';

        for (size_t pos = 0; text[pos]; pos++) {
            if (in_comment) {
                if (text[pos] == '*' && text[pos + 1] == '/') {
                    text[pos] = ' ';
                    text[pos + 1] = ' ';
                    pos++;
                    in_comment = 0;
                } else {
                    text[pos] = ' ';
                }
                continue;
            }

            if (in_string && text[pos] == quote && quote_can_close(text, pos, ']')) {
                in_string = 0;
                quote = '\0';
                continue;
            }
            if (!in_string && (text[pos] == '\'' || text[pos] == '"')) {
                in_string = 1;
                quote = text[pos];
                continue;
            }
            if (in_string) {
                continue;
            }

            if (text[pos] == '/' && text[pos + 1] == '*') {
                text[pos] = ' ';
                text[pos + 1] = ' ';
                pos++;
                in_comment = 1;
            }
        }
    }
}

static int parse_expression(Parser *parser, Value *out);
static int execute_range(Program *program, Environment *env, size_t start, size_t end, ParseError *err);
static void free_environment(Environment *env);
static char *value_to_string(Value value);

// Load source text into a Program (split by newlines)
static int load_source(const char *text, Program *program) {
    memset(program, 0, sizeof(*program));
    program->path = qs_strdup("<eval>");
    program->dir = qs_strdup(".");
    const char *p = text;
    while (*p) {
        const char *nl = p;
        while (*nl && *nl != '\n') nl++;
        size_t len = (size_t)(nl - p);
        char *line = malloc(len + 1);
        memcpy(line, p, len);
        line[len] = '\0';
        // Strip trailing \r
        if (len > 0 && line[len - 1] == '\r') line[--len] = '\0';
        program_push(program, line);
        if (*nl == '\n') nl++;
        p = nl;
    }
    return program->count > 0;
}
