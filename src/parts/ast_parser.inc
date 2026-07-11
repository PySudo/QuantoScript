// QuantoScript AST Parser — Produces AST nodes from source text
// Mirrors the precedence chain of 07_parser.inc but returns ASTNode* instead of Values.

// Parser state for expression parsing
typedef struct {
    const char *text;
    size_t pos;
    int line;
} ASTParser;

static void ast_skip(ASTParser *p) {
    while (p->text[p->pos] && (p->text[p->pos] == ' ' || p->text[p->pos] == '\t')) p->pos++;
}

static int ast_is_name_char(char c) { return isalnum((unsigned char)c) || c == '_'; }
static int ast_is_name_part_char(char c) { return isalnum((unsigned char)c) || c == '_'; }

static char *ast_parse_name(ASTParser *p) {
    ast_skip(p);
    size_t start = p->pos;
    while (p->text[p->pos] && ast_is_name_char(p->text[p->pos])) p->pos++;
    if (p->pos == start) return NULL;
    return qs_strndup(p->text + start, p->pos - start);
}

// Forward declarations
static ASTNode *ast_parse_expression(ASTParser *p);
static ASTNode *ast_parse_block_stmts(Program *source, size_t start, size_t end);

// ---- Primary ----
static ASTNode *ast_parse_primary(ASTParser *p) {
    ast_skip(p);
    char ch = p->text[p->pos];

    // Number
    if (isdigit((unsigned char)ch)) {
        // Check if it looks like a float (contains '.' or 'e'/'E')
        size_t scan = p->pos;
        while (isdigit((unsigned char)p->text[scan])) scan++;
        int is_float = (p->text[scan] == '.' || p->text[scan] == 'e' || p->text[scan] == 'E');
        char *end = NULL;
        if (is_float) {
            double fv = strtod(p->text + p->pos, &end);
            if (end > p->text + p->pos) {
                p->pos = (size_t)(end - p->text);
                return ast_float(fv, p->line);
            }
        }
        long iv = strtol(p->text + p->pos, &end, 10);
        if (end > p->text + p->pos) {
            p->pos = (size_t)(end - p->text);
            return ast_int(iv, p->line);
        }
    }

    // String
    if (ch == '\'' || ch == '"') {
        char quote = ch;
        p->pos++;
        size_t start = p->pos;
        while (p->text[p->pos]) {
            if (p->text[p->pos] == '\\' && p->text[p->pos + 1]) {
                p->pos += 2; // skip escaped char (always, including closing quote)
                continue;
            }
            if (p->text[p->pos] == quote) break;
            p->pos++;
        }
        if (p->text[p->pos] != quote) {
            // Unterminated string — report error
            fprintf(stderr, "SyntaxError: unterminated string literal\n");
            fprintf(stderr, "line %d, column %d\n", p->line, (int)start);
            return ast_null(p->line);
        }
        char *raw = qs_strndup(p->text + start, p->pos - start);
        // Decode escape sequences
        ParseError uerr = {0};
        char *str = unescape_string(raw, p->pos - start, &uerr);
        free(raw);
        if (p->text[p->pos] == quote) p->pos++;
        return ast_string(str, p->line);
    }

    // F-string: f"..." or f'...'
    if (ch == 'f' && (p->text[p->pos + 1] == '"' || p->text[p->pos + 1] == '\'')) {
        p->pos++; // skip f
        char quote = p->text[p->pos];
        p->pos++;
        ASTNode *node = ast_new(AST_FSTRING, p->line);
        node->as.fstring.parts = NULL;
        node->as.fstring.count = 0;
        while (p->text[p->pos] && p->text[p->pos] != quote) {
            if (p->text[p->pos] == '{') {
                p->pos++; // skip {
                ASTNode *expr = ast_parse_expression(p);
                ast_skip(p);
                if (p->text[p->pos] == '}') p->pos++;
                // grow array
                node->as.fstring.parts = realloc(node->as.fstring.parts, (node->as.fstring.count + 1) * sizeof(ASTNode *));
                node->as.fstring.parts[node->as.fstring.count++] = expr;
            } else {
                // Collect text until { or end quote
                size_t tstart = p->pos;
                while (p->text[p->pos] && p->text[p->pos] != quote && p->text[p->pos] != '{') p->pos++;
                char *t = qs_strndup(p->text + tstart, p->pos - tstart);
                node->as.fstring.parts = realloc(node->as.fstring.parts, (node->as.fstring.count + 1) * sizeof(ASTNode *));
                node->as.fstring.parts[node->as.fstring.count++] = ast_string(t, p->line);
            }
        }
        if (p->text[p->pos] == quote) p->pos++;
        return node;
    }

    // Identifier or keyword
        if (isalpha((unsigned char)ch) || ch == '_') {
            char *name = ast_parse_name(p);
            if (strcmp(name, "true") == 0) { free(name); return ast_bool(1, p->line); }
            if (strcmp(name, "false") == 0) { free(name); return ast_bool(0, p->line); }
            if (strcmp(name, "null") == 0) { free(name); return ast_null(p->line); }
            ast_skip(p);
            if (p->text[p->pos] == '(') {
                p->pos++; // skip (
            ASTNode **args = NULL;
            int argc = 0;
            ast_skip(p);
            while (p->text[p->pos] && p->text[p->pos] != ')') {
                ASTNode *arg = ast_parse_expression(p);
                argc++;
                args = realloc(args, argc * sizeof(ASTNode *));
                args[argc - 1] = arg;
                ast_skip(p);
                if (p->text[p->pos] == ',') p->pos++;
                ast_skip(p);
            }
            if (p->text[p->pos] == ')') p->pos++;
            return ast_call(name, args, argc, p->line);
        }

        // Check for lambda: fn(params) -> body
        if (strcmp(name, "fn") == 0 && p->text[p->pos] == '(') {
            free(name);
            p->pos++; // skip (
            char **params = NULL;
            char **param_types = NULL;
            int param_count = 0;
            ast_skip(p);
            while (p->text[p->pos] && p->text[p->pos] != ')') {
                char *pname = ast_parse_name(p);
                if (!pname) break;
                char *ptype = NULL;
                ast_skip(p);
                if (p->text[p->pos] == ':') {
                    p->pos++;
                    ptype = ast_parse_name(p);
                    if (!ptype) ptype = qs_strdup("any");
                }
                param_count++;
                params = realloc(params, param_count * sizeof(char *));
                params[param_count - 1] = pname;
                param_types = realloc(param_types, param_count * sizeof(char *));
                param_types[param_count - 1] = ptype;
                ast_skip(p);
                if (p->text[p->pos] == ',') p->pos++;
                ast_skip(p);
            }
            if (p->text[p->pos] == ')') p->pos++;
            ast_skip(p);
            if (p->text[p->pos] == '-' && p->text[p->pos + 1] == '>') {
                p->pos += 2;
                // Parse body text until we reach end or appropriate boundary
                size_t body_start = p->pos;
                ast_skip(p);
                if (p->text[p->pos] == '{') {
                    // Brace-delimited body
                    int depth = 1;
                    p->pos++;
                    while (p->text[p->pos] && depth > 0) {
                        if (p->text[p->pos] == '{') depth++;
                        else if (p->text[p->pos] == '}') depth--;
                        if (depth > 0) p->pos++;
                    }
                    if (p->text[p->pos] == '}') p->pos++;
                } else {
                    // Single expression body: scan until , or ) or end at depth 0
                    int depth = 0;
                    while (p->text[p->pos]) {
                        if (p->text[p->pos] == '(' || p->text[p->pos] == '[' || p->text[p->pos] == '{') depth++;
                        else if (p->text[p->pos] == ')' || p->text[p->pos] == ']' || p->text[p->pos] == '}') {
                            if (depth == 0) break;
                            depth--;
                        }
                        else if (p->text[p->pos] == ',' && depth == 0) break;
                        p->pos++;
                    }
                }
                char *body = qs_strndup(p->text + body_start, p->pos - body_start);
                ASTNode *node = ast_new(AST_LAMBDA, p->line);
                node->as.lambda.params = params;
                node->as.lambda.param_types = param_types;
                node->as.lambda.param_count = param_count;
                node->as.lambda.body_text = body;
                return node;
            }
            // fn without -> is a function definition in expression position — shouldn't happen
            // but handle gracefully
            for (int i = 0; i < param_count; i++) { free(params[i]); free(param_types[i]); }
            free(params); free(param_types);
            return ast_null(p->line);
        }

        return ast_ident(name, p->line);
    }

    // Parenthesized expression
    if (ch == '(') {
        p->pos++;
        ASTNode *inner = ast_parse_expression(p);
        ast_skip(p);
        if (p->text[p->pos] == ')') p->pos++;
        return inner;
    }

    // List literal
    if (ch == '[') {
        p->pos++;
        ASTNode *node = ast_new(AST_LIST_LIT, p->line);
        node->as.list.items = NULL;
        node->as.list.count = 0;
        ast_skip(p);
        while (p->text[p->pos] && p->text[p->pos] != ']') {
            ASTNode *item = ast_parse_expression(p);
            node->as.list.items = realloc(node->as.list.items, (node->as.list.count + 1) * sizeof(ASTNode *));
            node->as.list.items[node->as.list.count++] = item;
            ast_skip(p);
            if (p->text[p->pos] == ',') p->pos++;
            ast_skip(p);
        }
        if (p->text[p->pos] == ']') p->pos++;
        return node;
    }

    // Map literal
    if (ch == '{') {
        p->pos++;
        ASTNode *node = ast_new(AST_MAP_LIT, p->line);
        node->as.map.keys = NULL;
        node->as.map.vals = NULL;
        node->as.map.count = 0;
        ast_skip(p);
        while (p->text[p->pos] && p->text[p->pos] != '}') {
            ASTNode *key = ast_parse_expression(p);
            ast_skip(p);
            if (p->text[p->pos] == ':') p->pos++;
            ast_skip(p);
            ASTNode *val = ast_parse_expression(p);
            node->as.map.keys = realloc(node->as.map.keys, (node->as.map.count + 1) * sizeof(ASTNode *));
            node->as.map.vals = realloc(node->as.map.vals, (node->as.map.count + 1) * sizeof(ASTNode *));
            node->as.map.keys[node->as.map.count] = key;
            node->as.map.vals[node->as.map.count] = val;
            node->as.map.count++;
            ast_skip(p);
            if (p->text[p->pos] == ',') p->pos++;
            ast_skip(p);
        }
        if (p->text[p->pos] == '}') p->pos++;
        return node;
    }

    return NULL;
}

// ---- Postfix (method calls, indexing, slicing) ----
static ASTNode *ast_parse_postfix(ASTParser *p) {
    ASTNode *node = ast_parse_primary(p);
    if (!node) return NULL;

    while (1) {
        ast_skip(p);

        // Method call: .method(args)
        if (p->text[p->pos] == '.') {
            p->pos++;
            char *method = ast_parse_name(p);
            if (!method) { ast_free(node); return NULL; }
            ast_skip(p);
            if (p->text[p->pos] == '(') {
                p->pos++;
                ASTNode **args = NULL;
                int argc = 0;
                ast_skip(p);
                while (p->text[p->pos] && p->text[p->pos] != ')') {
                    ASTNode *arg = ast_parse_expression(p);
                    argc++;
                    args = realloc(args, argc * sizeof(ASTNode *));
                    args[argc - 1] = arg;
                    ast_skip(p);
                    if (p->text[p->pos] == ',') p->pos++;
                    ast_skip(p);
                }
                if (p->text[p->pos] == ')') p->pos++;
                node = ast_method_call(node, method, args, argc, p->line);
            } else {
                // Property access — treat as method call with 0 args
                node = ast_method_call(node, method, NULL, 0, p->line);
            }
            continue;
        }

        // Index/Slice: [expr] or [expr:expr]
        if (p->text[p->pos] == '[') {
            p->pos++;
            ast_skip(p);
            ASTNode *start = NULL;
            if (p->text[p->pos] != ':') {
                start = ast_parse_expression(p);
            }
            ast_skip(p);
            if (p->text[p->pos] == ':') {
                // Slice
                p->pos++;
                ast_skip(p);
                ASTNode *end = NULL;
                if (p->text[p->pos] != ']') {
                    end = ast_parse_expression(p);
                }
                ast_skip(p);
                if (p->text[p->pos] == ']') p->pos++;
                ASTNode *s = ast_new(AST_SLICE, p->line);
                s->as.slice.object = node;
                s->as.slice.start = start;
                s->as.slice.end = end;
                node = s;
            } else {
                // Index
                ast_skip(p);
                if (p->text[p->pos] == ']') p->pos++;
                ASTNode *idx = ast_new(AST_INDEX, p->line);
                idx->as.index.object = node;
                idx->as.index.key = start;
                node = idx;
            }
            continue;
        }

        break;
    }
    return node;
}

// ---- Unary ----
static ASTNode *ast_parse_unary(ASTParser *p) {
    ast_skip(p);
    if (p->text[p->pos] == '-' && p->pos + 1 < strlen(p->text)) {
        p->pos++; // skip the minus sign
        ASTNode *operand = ast_parse_unary(p);
        return ast_unary(AST_UNARY_NEG, operand, p->line);
    }
    if (strncmp(p->text + p->pos, "not", 3) == 0 && !ast_is_name_part_char(p->text[p->pos + 3])) {
        p->pos += 3;
        ASTNode *operand = ast_parse_unary(p);
        return ast_unary(AST_UNARY_NOT, operand, p->line);
    }
    return ast_parse_postfix(p);
}

// ---- Binary operators with precedence ----
static int ast_get_binary_type(const char *op) {
    if (strcmp(op, "+") == 0) return AST_BINARY_ADD;
    if (strcmp(op, "-") == 0) return AST_BINARY_SUB;
    if (strcmp(op, "*") == 0) return AST_BINARY_MUL;
    if (strcmp(op, "/") == 0) return AST_BINARY_DIV;
    if (strcmp(op, "%") == 0) return AST_BINARY_MOD;
    if (strcmp(op, "==") == 0) return AST_BINARY_EQ;
    if (strcmp(op, "!=") == 0) return AST_BINARY_NEQ;
    if (strcmp(op, "<") == 0) return AST_BINARY_LT;
    if (strcmp(op, ">") == 0) return AST_BINARY_GT;
    if (strcmp(op, "<=") == 0) return AST_BINARY_LTE;
    if (strcmp(op, ">=") == 0) return AST_BINARY_GTE;
    if (strcmp(op, "and") == 0) return AST_BINARY_AND;
    if (strcmp(op, "or") == 0) return AST_BINARY_OR;
    return -1;
}

static ASTNode *ast_parse_term(ASTParser *p) {
    ASTNode *left = ast_parse_unary(p);
    if (!left) return NULL;
    while (1) {
        ast_skip(p);
        char opch = p->text[p->pos];
        if (opch == '*' || opch == '/' || opch == '%') {
            p->pos++;
            ASTNode *right = ast_parse_unary(p);
            if (!right) { ast_free(left); return NULL; }
            char ops[2] = {opch, 0};
            left = ast_binary(ast_get_binary_type(ops), left, right, p->line);
        } else break;
    }
    return left;
}

static ASTNode *ast_parse_addition(ASTParser *p) {
    ASTNode *left = ast_parse_term(p);
    if (!left) return NULL;
    while (1) {
        ast_skip(p);
        char opch = p->text[p->pos];
        if (opch == '+' || opch == '-') {
            p->pos++;
            ASTNode *right = ast_parse_term(p);
            if (!right) { ast_free(left); return NULL; }
            char ops[2] = {opch, 0};
            left = ast_binary(ast_get_binary_type(ops), left, right, p->line);
        } else break;
    }
    return left;
}

static ASTNode *ast_parse_comparison(ASTParser *p) {
    ASTNode *left = ast_parse_addition(p);
    if (!left) return NULL;

    ast_skip(p);
    // Check for comparison operators
    const char *op = NULL;
    if (p->text[p->pos] == '<' && p->text[p->pos + 1] == '=') { op = "<="; p->pos += 2; }
    else if (p->text[p->pos] == '>' && p->text[p->pos + 1] == '=') { op = ">="; p->pos += 2; }
    else if (p->text[p->pos] == '<') { op = "<"; p->pos += 1; }
    else if (p->text[p->pos] == '>') { op = ">"; p->pos += 1; }

    if (!op) return left;

    ASTNode *right = ast_parse_addition(p);
    if (!right) { ast_free(left); return NULL; }

    // Check for chained comparison: a < b < c
    ast_skip(p);
    const char *op2 = NULL;
    if (p->text[p->pos] == '<' && p->text[p->pos + 1] == '=') { op2 = "<="; }
    else if (p->text[p->pos] == '>' && p->text[p->pos + 1] == '=') { op2 = ">="; }
    else if (p->text[p->pos] == '<') { op2 = "<"; }
    else if (p->text[p->pos] == '>') { op2 = ">"; }

    if (op2) {
        // Chained: collect all operands and operators
        ASTNode **operands = malloc(3 * sizeof(ASTNode *));
        operands[0] = left;
        operands[1] = right;
        char **ops = malloc(2 * sizeof(char *));
        ops[0] = qs_strdup(op);
        int count = 2;
        int op_count = 1;

        while (1) {
            // Skip the operator we just peeked at
            if (strcmp(op2, "<=") == 0 || strcmp(op2, ">=") == 0) p->pos += 2;
            else p->pos += 1;

            ASTNode *next = ast_parse_addition(p);
            if (!next) break;
            count++;
            operands = realloc(operands, count * sizeof(ASTNode *));
            operands[count - 1] = next;
            ops = realloc(ops, (op_count + 1) * sizeof(char *));
            ops[op_count] = qs_strdup(op2);
            op_count++;

            ast_skip(p);
            op2 = NULL;
            if (p->text[p->pos] == '<' && p->text[p->pos + 1] == '=') op2 = "<=";
            else if (p->text[p->pos] == '>' && p->text[p->pos + 1] == '=') op2 = ">=";
            else if (p->text[p->pos] == '<') op2 = "<";
            else if (p->text[p->pos] == '>') op2 = ">";
            if (!op2) break;
        }

        ASTNode *chain = ast_new(AST_CHAINED_CMP, p->line);
        chain->as.chained.operands = operands;
        chain->as.chained.ops = ops;
        chain->as.chained.count = count;
        return chain;
    }

    return ast_binary(ast_get_binary_type(op), left, right, p->line);
}

static ASTNode *ast_parse_equality(ASTParser *p) {
    ASTNode *left = ast_parse_comparison(p);
    if (!left) return NULL;
    while (1) {
        ast_skip(p);
        const char *op = NULL;
        if (p->text[p->pos] == '=' && p->text[p->pos + 1] == '=') { op = "=="; p->pos += 2; }
        else if (p->text[p->pos] == '!' && p->text[p->pos + 1] == '=') { op = "!="; p->pos += 2; }
        if (!op) break;
        ASTNode *right = ast_parse_comparison(p);
        if (!right) { ast_free(left); return NULL; }
        left = ast_binary(ast_get_binary_type(op), left, right, p->line);
    }
    return left;
}

static ASTNode *ast_parse_and(ASTParser *p) {
    ASTNode *left = ast_parse_equality(p);
    if (!left) return NULL;
    while (1) {
        ast_skip(p);
        if (strncmp(p->text + p->pos, "and", 3) == 0 && !ast_is_name_part_char(p->text[p->pos + 3])) {
            p->pos += 3;
            ASTNode *right = ast_parse_equality(p);
            if (!right) { ast_free(left); return NULL; }
            left = ast_binary(AST_BINARY_AND, left, right, p->line);
        } else break;
    }
    return left;
}

static ASTNode *ast_parse_expression(ASTParser *p) {
    ASTNode *left = ast_parse_and(p);
    if (!left) return NULL;
    while (1) {
        ast_skip(p);
        if (strncmp(p->text + p->pos, "or", 2) == 0 && !ast_is_name_part_char(p->text[p->pos + 2])) {
            p->pos += 2;
            ASTNode *right = ast_parse_and(p);
            if (!right) { ast_free(left); return NULL; }
            left = ast_binary(AST_BINARY_OR, left, right, p->line);
        } else break;
    }
    return left;
}

// ---- Statement Parser ----

// Find matching close brace starting from source[start] which contains '{'
static int ast_find_block_end(Program *source, size_t start, size_t end, size_t *out) {
    int depth = 0;
    for (size_t i = start; i < end; i++) {
        char *line = source->items[i];
        size_t len = strlen(line);
        for (size_t j = 0; j < len; j++) {
            if (line[j] == '{') depth++;
            else if (line[j] == '}') {
                depth--;
                if (depth == 0) {
                    *out = i;
                    return 1;
                }
            }
        }
    }
    return 0;
}

// Check if line starts with keyword
static int line_starts_kw(const char *line, const char *kw) {
    size_t pos = 0;
    while (line[pos] == ' ' || line[pos] == '\t') pos++;
    size_t klen = strlen(kw);
    if (strncmp(line + pos, kw, klen) == 0) {
        char next = line[pos + klen];
        return next == '\0' || next == ' ' || next == '\t' || next == '(' || next == '{';
    }
    return 0;
}

// Get position after keyword
static size_t after_keyword(const char *line, const char *kw) {
    size_t pos = 0;
    while (line[pos] == ' ' || line[pos] == '\t') pos++;
    return pos + strlen(kw);
}

// Check if keyword appears at specific position in line
// Normalize source: split lines where } is followed by maybe/else on the same line
// This ensures the parser never sees mixed } + branch-keyword content on one line
static void ast_normalize_source(Program *source) {
    for (size_t i = 0; i < source->count; i++) {
        char *line = source->items[i];
        if (!line) continue;
        // Skip comment lines — never modify them
        size_t skip = 0;
        while (line[skip] == ' ' || line[skip] == '\t') skip++;
        if (line[skip] == '#') continue;
        // Only scan lines that start with } — these are the only lines that can have
        // } maybe / } else on the same line in valid QuantoScript syntax
        if (line[skip] != '}') continue;
        // Find } followed by maybe or else on same line
        size_t len = strlen(line);
        for (size_t j = 0; j < len; j++) {
            if (line[j] == '}') {
                size_t k = j + 1;
                while (k < len && (line[k] == ' ' || line[k] == '\t')) k++;
                if (strncmp(line + k, "maybe", 5) == 0 && !is_name_part(line[k + 5])) {
                    // Split: } and maybe ... on separate lines
                    char *after_brace = qs_strdup(line + j + 1);
                    line[j + 1] = '\0';
                    // Insert new line after current one
                    source->items = realloc(source->items, (source->count + 1) * sizeof(char *));
                    for (size_t m = source->count; m > i + 1; m--)
                        source->items[m] = source->items[m - 1];
                    source->items[i + 1] = after_brace;
                    source->count++;
                    break;
                }
                if (strncmp(line + k, "else", 4) == 0 && !is_name_part(line[k + 4])) {
                    char *after_brace = qs_strdup(line + j + 1);
                    line[j + 1] = '\0';
                    source->items = realloc(source->items, (source->count + 1) * sizeof(char *));
                    for (size_t m = source->count; m > i + 1; m--)
                        source->items[m] = source->items[m - 1];
                    source->items[i + 1] = after_brace;
                    source->count++;
                    break;
                }
            }
        }
    }
}

// Parse if/maybe/else chain (assumes source is normalized — } maybe/else split onto separate lines)
static ASTNode *ast_parse_if_chain(Program *source, size_t *index, size_t end) {
    char *line = source->items[*index];
    size_t after;
    if (line_starts_kw(line, "maybe")) {
        after = after_keyword(line, "maybe");
    } else {
        after = after_keyword(line, "if");
    }

    // Find '{'
    size_t brace = after;
    while (line[brace] && line[brace] != '{') brace++;

    // Parse condition
    ASTParser cp = {line, after, (int)*index + 1};
    ASTNode *cond = ast_parse_expression(&cp);
    if (!cond) return NULL;

    // Find block end
    size_t block_close = 0;
    if (!ast_find_block_end(source, *index, end, &block_close)) { ast_free(cond); return NULL; }

    // Parse body
    ASTNode *body = ast_parse_block_stmts(source, *index + 1, block_close);

    ASTNode *node = ast_new(AST_IF, (int)*index + 1);
    node->as.if_stmt.cond = cond;
    node->as.if_stmt.then_body = body;
    node->as.if_stmt.elif_branch = NULL;
    node->as.if_stmt.else_body = NULL;

    // After normalization, maybe/else are always on the NEXT line
    size_t next = block_close + 1;
    if (next < end && source->items[next]) {
        char *nl = source->items[next];
        if (line_starts_kw(nl, "maybe")) {
            node->as.if_stmt.elif_branch = ast_parse_if_chain(source, &next, end);
            *index = next;
            return node;
        } else if (line_starts_kw(nl, "else")) {
            size_t else_close = 0;
            if (ast_find_block_end(source, next, end, &else_close)) {
                node->as.if_stmt.else_body = ast_parse_block_stmts(source, next + 1, else_close);
                *index = else_close;
            }
            return node;
        }
    }

    *index = block_close;
    return node;
}

// Parse while loop
static ASTNode *ast_parse_while_loop(Program *source, size_t *index, size_t end) {
    char *line = source->items[*index];
    size_t after = after_keyword(line, "while");
    size_t brace = after;
    while (line[brace] && line[brace] != '{') brace++;

    ASTParser cp = {line, after, (int)*index + 1};
    ASTNode *cond = ast_parse_expression(&cp);
    if (!cond) return NULL;

    // Check if body is on the same line (single-line body)
    size_t body_start = brace + 1;
    size_t blen = strlen(line);
    int depth = 0;
    size_t close_brace = 0;
    for (size_t j = body_start; j < blen; j++) {
        if (line[j] == '{') depth++;
        else if (line[j] == '}') {
            if (depth == 0) { close_brace = j; break; }
            depth--;
        }
    }

    ASTNode *body;
    if (close_brace > 0) {
        // Single-line body: extract text between { and } and parse as block
        while (body_start < close_brace && (line[body_start] == ' ' || line[body_start] == '\t' || line[body_start] == '\n')) body_start++;
        size_t body_len = close_brace - body_start;
        char *body_text = malloc(body_len + 1);
        if (body_len > 0) memcpy(body_text, line + body_start, body_len);
        body_text[body_len] = '\0';
        Program body_prog = {0};
        body_prog.path = qs_strdup("<while_body>");
        body_prog.dir = qs_strdup(".");
        split_text_to_program(body_text, &body_prog);
        body = ast_parse_block_stmts(&body_prog, 0, body_prog.count);
        free_program(&body_prog);
        free(body_text);
    } else {
        // Multi-line body
        size_t block_close = 0;
        if (!ast_find_block_end(source, *index, end, &block_close)) { ast_free(cond); return NULL; }
        body = ast_parse_block_stmts(source, *index + 1, block_close);
        *index = block_close;
    }

    ASTNode *node = ast_new(AST_WHILE, (int)*index + 1);
    node->as.while_stmt.cond = cond;
    node->as.while_stmt.body = body;
    return node;
}

// Parse repeat loop
static ASTNode *ast_parse_repeat_loop(Program *source, size_t *index, size_t end) {
    char *line = source->items[*index];
    size_t after = after_keyword(line, "repeat");
    size_t len = strlen(line);

    // Find '->' or '{'
    size_t arrow = 0;
    for (size_t s = after; s < len; s++) {
        if (line[s] == '-' && s + 1 < len && line[s + 1] == '>') { arrow = s; break; }
        if (line[s] == '{') { break; }
    }

    size_t block_close = 0;
    if (!ast_find_block_end(source, *index, end, &block_close)) return NULL;

    ASTNode *node = ast_new(AST_REPEAT, (int)*index + 1);

    if (arrow) {
        // repeat item -> collection { body }
        node->as.repeat.is_counted = 0;
        // Parse variable names before arrow
        char *vars_text = qs_strndup(line + after, arrow - after);
        ASTParser vp = {vars_text, 0, (int)*index + 1};
        ast_skip(&vp);
        char *v1 = ast_parse_name(&vp);
        node->as.repeat.var_name = v1;
        node->as.repeat.index_name = NULL;
        ast_skip(&vp);
        if (vp.text[vp.pos] == ',') {
            vp.pos++;
            char *v2 = ast_parse_name(&vp);
            // Swap: index is first, item is second
            node->as.repeat.index_name = v1;
            node->as.repeat.var_name = v2;
        }
        free(vars_text);

        // Parse collection expression
        ASTParser cp = {line, arrow + 2, (int)*index + 1};
        node->as.repeat.count_or_coll = ast_parse_expression(&cp);
    } else {
        // repeat N { body }
        node->as.repeat.is_counted = 1;
        ASTParser cp = {line, after, (int)*index + 1};
        node->as.repeat.count_or_coll = ast_parse_expression(&cp);
        node->as.repeat.var_name = NULL;
        node->as.repeat.index_name = NULL;
    }

    ASTNode *body;
    if (block_close > *index) {
        // Multi-line body
        body = ast_parse_block_stmts(source, *index + 1, block_close);
    } else {
        // Single-line body: extract text between { and } on the same line
        size_t body_start = after;
        while (body_start < len && line[body_start] != '{') body_start++;
        body_start++; // skip '{'

        size_t close_brace = 0;
        size_t depth = 1;
        for (size_t j = body_start; j < len; j++) {
            if (line[j] == '{') depth++;
            else if (line[j] == '}') {
                depth--;
                if (depth == 0) { close_brace = j; break; }
            }
        }

        if (close_brace > body_start) {
            while (body_start < close_brace && (line[body_start] == ' ' || line[body_start] == '\t' || line[body_start] == '\n')) body_start++;
            size_t body_len = close_brace - body_start;
            char *body_text = malloc(body_len + 1);
            if (body_len > 0) memcpy(body_text, line + body_start, body_len);
            body_text[body_len] = '\0';
            Program body_prog = {0};
            body_prog.path = qs_strdup("<repeat_body>");
            body_prog.dir = qs_strdup(".");
            split_text_to_program(body_text, &body_prog);
            body = ast_parse_block_stmts(&body_prog, 0, body_prog.count);
            free_program(&body_prog);
            free(body_text);
        } else {
            body = ast_new(AST_BLOCK, 0);
            body->as.block.stmts = NULL;
            body->as.block.count = 0;
            body->as.block.capacity = 0;
        }
    }

    node->as.repeat.body = body;
    *index = block_close;
    return node;
}

// Generalized block body parser: handles both inline and multi-line blocks.
// Given a source, a block_close line index (the line containing the matching }),
// and the line containing the opening {, extracts and parses the block body.
// If block_close == open_line (single-line block), extracts inline content.
// If block_close > open_line (multi-line block), uses line-based parsing.
static ASTNode *ast_parse_block_body(Program *source, size_t open_line, size_t block_close) {
    if (block_close > open_line) {
        // Multi-line: parse lines between opening and closing
        return ast_parse_block_stmts(source, open_line + 1, block_close);
    }

    // Single-line: extract content between { and } on the same line
    char *line = source->items[open_line];
    size_t len = strlen(line);

    // Find the opening {
    size_t open_brace = 0;
    while (open_brace < len && line[open_brace] != '{') open_brace++;
    if (open_brace >= len) return ast_new(AST_BLOCK, (int)open_line + 1);

    // Find the closing } (last one on this line)
    size_t close_brace = len;
    for (size_t s = len; s > open_brace; s--) {
        if (line[s - 1] == '}') { close_brace = s - 1; break; }
    }

    // Extract inline content between { and }
    size_t content_len = (close_brace > open_brace + 1) ? (close_brace - open_brace - 1) : 0;
    char *content = malloc(content_len + 2);
    if (content_len > 0) {
        memcpy(content, line + open_brace + 1, content_len);
    }
    content[content_len] = '\n';
    content[content_len + 1] = '\0';

    // Create a temporary Program, splitting merged content into individual lines
    Program temp = {0};
    temp.path = qs_strdup("<inline>");
    temp.dir = qs_strdup(".");
    split_text_to_program(content, &temp);
    free(content);

    ASTNode *result = ast_parse_block_stmts(&temp, 0, temp.count);

    free_program(&temp);
    return result;
}

// Parse function definition: fn name(params) { body }
static ASTNode *ast_parse_func_def(Program *source, size_t *index) {
    char *line = source->items[*index];
    size_t after;
    if (line_starts_kw(line, "func")) {
        after = after_keyword(line, "func");
    } else {
        after = after_keyword(line, "fn");
    }

    // Parse function name
    ASTParser np = {line, after, (int)*index + 1};
    char *name = ast_parse_name(&np);
    if (!name) return NULL;

    // Parse params
    ast_skip(&np);
    if (np.text[np.pos] != '(') { free(name); return NULL; }
    np.pos++; // skip (

    char **params = NULL;
    char **param_types = NULL;
    int param_count = 0;
    ast_skip(&np);
    while (np.text[np.pos] && np.text[np.pos] != ')') {
        char *pname = ast_parse_name(&np);
        if (!pname) break;
        char *ptype = NULL;
        ast_skip(&np);
        if (np.text[np.pos] == ':') {
            np.pos++;
            ptype = ast_parse_name(&np);
            if (!ptype) ptype = qs_strdup("any");
        }
        param_count++;
        params = realloc(params, param_count * sizeof(char *));
        params[param_count - 1] = pname;
        param_types = realloc(param_types, param_count * sizeof(char *));
        param_types[param_count - 1] = ptype;
        ast_skip(&np);
        if (np.text[np.pos] == ',') np.pos++;
        ast_skip(&np);
    }
    if (np.text[np.pos] == ')') np.pos++;

    // Parse optional return type: -> type
    char *return_type = NULL;
    ast_skip(&np);
    if (np.text[np.pos] == '-' && np.pos + 1 < strlen(np.text) && np.text[np.pos + 1] == '>') {
        // Only parse return type if NOT followed by '{' (that would be lambda body syntax)
        size_t save_pos = np.pos;
        np.pos += 2;
        ast_skip(&np);
        if (np.text[np.pos] != '{') {
            // Parse union type for return type
            char *first = ast_parse_name(&np);
            if (first) {
                size_t cap = strlen(first) + 1;
                return_type = malloc(cap);
                strcpy(return_type, first);
                ast_skip(&np);
                while (np.text[np.pos] == '|') {
                    np.pos++;
                    ast_skip(&np);
                    char *alt = ast_parse_name(&np);
                    if (alt) {
                        cap = strlen(return_type) + 1 + strlen(alt) + 1;
                        char *new_rt = malloc(cap);
                        snprintf(new_rt, cap, "%s|%s", return_type, alt);
                        free(return_type);
                        return_type = new_rt;
                        free(alt);
                    }
                    ast_skip(&np);
                }
                free(first);
            }
        } else {
            np.pos = save_pos; // restore — this is lambda body syntax
        }
    }

    // Find block end
    size_t block_close = 0;
    if (!ast_find_block_end(source, *index, source->count, &block_close)) {
        free(name);
        for (int i = 0; i < param_count; i++) { free(params[i]); free(param_types[i]); }
        free(params); free(param_types);
        return NULL;
    }

    // Parse body using generalized block parser
    ASTNode *body = ast_parse_block_body(source, *index, block_close);

    // Capture raw body text for sub-VM compilation
    char *body_text = NULL;
    if (block_close == *index) {
        // Single-line: extract inline content
        char *src_line = source->items[*index];
        size_t len = strlen(src_line);
        size_t open_brace = 0;
        while (open_brace < len && src_line[open_brace] != '{') open_brace++;
        size_t close_brace = len;
        for (size_t s = len; s > open_brace; s--) {
            if (src_line[s - 1] == '}') { close_brace = s - 1; break; }
        }
        size_t content_len = (close_brace > open_brace + 1) ? (close_brace - open_brace - 1) : 0;
        body_text = malloc(content_len + 4);
        body_text[0] = '{'; body_text[1] = '\n';
        if (content_len > 0) memcpy(body_text + 2, src_line + open_brace + 1, content_len);
        body_text[content_len + 2] = '\n';
        body_text[content_len + 3] = '\0';
    } else {
        // Multi-line: concatenate lines
        size_t total = 0;
        for (size_t b = *index + 1; b < block_close; b++)
            total += strlen(source->items[b]) + 1;
        body_text = malloc(total + 4);
        body_text[0] = '{'; body_text[1] = '\n';
        size_t p = 2;
        for (size_t b = *index + 1; b < block_close; b++) {
            size_t slen = strlen(source->items[b]);
            memcpy(body_text + p, source->items[b], slen);
            p += slen;
            body_text[p] = '\n'; p++;
        }
        body_text[p] = '}';
        body_text[p + 1] = '\n';
        body_text[p + 2] = '\0';
    }

    ASTNode *node = ast_new(AST_FUNC_DEF, (int)*index + 1);
    node->as.func_def.name = name;
    node->as.func_def.params = params;
    node->as.func_def.param_types = param_types;
    node->as.func_def.param_count = param_count;
    node->as.func_def.return_type = return_type;
    node->as.func_def.body = body;
    node->as.func_def.body_text = body_text;
    *index = block_close;
    return node;
}

// Like ast_find_block_end but handles } opening on same line as closing }
// For lines like "} oops {" — finds the matching } for the { after "oops"
static int ast_find_block_end_from_brace(Program *source, size_t start, size_t end, size_t *out) {
    int depth = 0;
    int found_open = 0;
    for (size_t i = start; i < end; i++) {
        char *line = source->items[i];
        size_t len = strlen(line);
        for (size_t j = 0; j < len; j++) {
            if (line[j] == '{') {
                depth++;
                found_open = 1;
            } else if (line[j] == '}' && found_open) {
                depth--;
                if (depth == 0) { *out = i; return 1; }
            }
        }
    }
    return 0;
}

// Parse try/oops
static ASTNode *ast_parse_try_block(Program *source, size_t *index, size_t end) {
    size_t block_close = 0;
    if (!ast_find_block_end(source, *index, end, &block_close)) return NULL;

    ASTNode *try_body = ast_parse_block_stmts(source, *index + 1, block_close);

    ASTNode *node = ast_new(AST_TRY, (int)*index + 1);
    node->as.try_stmt.try_body = try_body;
    node->as.try_stmt.err_var = NULL;
    node->as.try_stmt.oops_body = NULL;

    // Check for oops after block — may be on same line as } or next line
    {
        char *cl = source->items[block_close];
        size_t clen = strlen(cl);
        size_t after_brace = 0;
        int d = 1;
        for (size_t s = 0; s < clen; s++) {
            if (cl[s] == '{') d++;
            else if (cl[s] == '}') { d--; if (d == 0) { after_brace = s + 1; break; } }
        }
        while (after_brace < clen && (cl[after_brace] == ' ' || cl[after_brace] == '\t')) after_brace++;
        if (after_brace < clen) {
            char *oops_sub = cl + after_brace;
            if (line_starts_kw(oops_sub, "oops")) {
                size_t oops_kw_len = strlen("oops");
                size_t oops_pos = oops_kw_len;
                while (oops_pos < strlen(oops_sub) && (oops_sub[oops_pos] == ' ' || oops_sub[oops_pos] == '\t')) oops_pos++;
                ASTParser op = {oops_sub, oops_pos, (int)block_close + 1};
                ast_skip(&op);
                if (isalpha((unsigned char)op.text[op.pos]) || op.text[op.pos] == '_') {
                    node->as.try_stmt.err_var = ast_parse_name(&op);
                }
                size_t oops_close = 0;
                if (ast_find_block_end_from_brace(source, block_close, end, &oops_close)) {
                    node->as.try_stmt.oops_body = ast_parse_block_stmts(source, block_close + 1, oops_close);
                    *index = oops_close;
                    return node;
                }
            }
        }
    }

    // Check next line
    size_t next = block_close + 1;
    if (next < end && source->items[next] && line_starts_kw(source->items[next], "oops")) {
        char *oops_line = source->items[next];
        size_t oops_after = after_keyword(oops_line, "oops");
        ASTParser op = {oops_line, oops_after, (int)next + 1};
        ast_skip(&op);
        // Optional error variable name
        if (isalpha((unsigned char)op.text[op.pos]) || op.text[op.pos] == '_') {
            node->as.try_stmt.err_var = ast_parse_name(&op);
        }
        size_t oops_close = 0;
        if (ast_find_block_end(source, next, end, &oops_close)) {
            node->as.try_stmt.oops_body = ast_parse_block_stmts(source, next + 1, oops_close);
            *index = oops_close;
            return node;
        }
    }

    *index = block_close;
    return node;
}

// Parse a single statement line — updates end_index to point past multi-line constructs
// ---- Statement Parser (forward declaration) ----
static ASTNode *ast_parse_statement(Program *source, size_t i, size_t end, size_t *end_index);

// Parse class definition: class Name { method(params) { body } ... }
static ASTNode *ast_parse_class_def(Program *source, size_t *index, size_t end) {
    (void)end;
    char *line = source->items[*index];
    size_t after = after_keyword(line, "class");

    ASTParser np = {line, after, (int)*index + 1};
    ast_skip(&np);
    char *name = ast_parse_name(&np);
    if (!name) return NULL;

    // The class body is embedded in this line (file loader merges multi-line constructs).
    // Find the opening { after the class name.
    size_t open_pos = np.pos;
    while (line[open_pos] && line[open_pos] != '{') open_pos++;
    if (line[open_pos] != '{') { free(name); return NULL; }
    open_pos++; // skip {

    // Parse methods and fields from the embedded body text
    ASTNode **methods = NULL;
    int method_count = 0;
    ASTNode **fields = NULL;
    int field_count = 0;
    size_t body_len = strlen(line);

    while (open_pos < body_len) {
        // Skip whitespace and newlines
        while (open_pos < body_len && (line[open_pos] == ' ' || line[open_pos] == '\t' || line[open_pos] == '\n' || line[open_pos] == '\r')) open_pos++;
        if (open_pos >= body_len || line[open_pos] == '}') break;

        // Try to parse a name
        ASTParser mp = {line, open_pos, (int)*index + 1};
        ast_skip(&mp);
        char *mname = ast_parse_name(&mp);
        if (!mname) { open_pos++; continue; }
        ast_skip(&mp);

        // Check if this is a field declaration: name:type (no paren follows)
        if (mp.text[mp.pos] == ':') {
            mp.pos++;
            ast_skip(&mp);
            // Parse union type
            char *first = ast_parse_name(&mp);
            if (first) {
                size_t cap = strlen(first) + 1;
                char *ftype = malloc(cap);
                strcpy(ftype, first);
                ast_skip(&mp);
                while (mp.text[mp.pos] == '|') {
                    mp.pos++;
                    ast_skip(&mp);
                    char *alt = ast_parse_name(&mp);
                    if (alt) {
                        cap = strlen(ftype) + 1 + strlen(alt) + 1;
                        char *new_ft = malloc(cap);
                        snprintf(new_ft, cap, "%s|%s", ftype, alt);
                        free(ftype);
                        ftype = new_ft;
                        free(alt);
                    }
                    ast_skip(&mp);
                }
                free(first);

                ASTNode *field = ast_new(AST_FIELD_DEF, (int)*index + 1);
                field->as.field_def.name = mname;
                field->as.field_def.type = ftype;
                field_count++;
                fields = realloc(fields, field_count * sizeof(ASTNode *));
                fields[field_count - 1] = field;
                open_pos = mp.pos;
                continue;
            }
        }
        ast_skip(&mp);
        if (mp.text[mp.pos] != '(') { free(mname); open_pos = mp.pos; continue; }

        // Parse params
        mp.pos++;
        char **params = NULL;
        char **param_types = NULL;
        int param_count = 0;
        ast_skip(&mp);
        while (mp.text[mp.pos] && mp.text[mp.pos] != ')') {
            char *pname = ast_parse_name(&mp);
            if (!pname) break;
            char *ptype = NULL;
            ast_skip(&mp);
            if (mp.text[mp.pos] == ':') { mp.pos++; ptype = ast_parse_name(&mp); if (!ptype) ptype = qs_strdup("any"); }
            param_count++;
            params = realloc(params, param_count * sizeof(char *));
            params[param_count - 1] = pname;
            param_types = realloc(param_types, param_count * sizeof(char *));
            param_types[param_count - 1] = ptype;
            ast_skip(&mp);
            if (mp.text[mp.pos] == ',') mp.pos++;
            ast_skip(&mp);
        }
        if (mp.text[mp.pos] == ')') mp.pos++;
        ast_skip(&mp);

        // Parse optional return type: -> type
        char *return_type = NULL;
        if (mp.text[mp.pos] == '-' && mp.pos + 1 < strlen(mp.text) && mp.text[mp.pos + 1] == '>') {
            size_t save_pos = mp.pos;
            mp.pos += 2;
            ast_skip(&mp);
            if (mp.text[mp.pos] != '{') {
                char *first = ast_parse_name(&mp);
                if (first) {
                    size_t cap = strlen(first) + 1;
                    return_type = malloc(cap);
                    strcpy(return_type, first);
                    ast_skip(&mp);
                    while (mp.text[mp.pos] == '|') {
                        mp.pos++;
                        ast_skip(&mp);
                        char *alt = ast_parse_name(&mp);
                        if (alt) {
                            cap = strlen(return_type) + 1 + strlen(alt) + 1;
                            char *new_rt = malloc(cap);
                            snprintf(new_rt, cap, "%s|%s", return_type, alt);
                            free(return_type);
                            return_type = new_rt;
                            free(alt);
                        }
                        ast_skip(&mp);
                    }
                    free(first);
                }
            } else {
                mp.pos = save_pos;
            }
        }

        // Find method body { ... }
        if (mp.text[mp.pos] != '{') {
            free(mname);
            for (int p = 0; p < param_count; p++) { free(params[p]); free(param_types[p]); }
            free(params); free(param_types);
            open_pos = mp.pos;
            continue;
        }

        // Find matching }
        int mdepth = 1;
        size_t body_start = mp.pos + 1;
        size_t body_end = body_start;
        while (body_end < body_len && mdepth > 0) {
            if (line[body_end] == '{') mdepth++;
            else if (line[body_end] == '}') mdepth--;
            if (mdepth > 0) body_end++;
        }
        if (mdepth != 0) {
            free(mname);
            for (int p = 0; p < param_count; p++) { free(params[p]); free(param_types[p]); }
            free(params); free(param_types);
            break;
        }
        // body_end now points to the closing }

        // Extract body text (between { and }) and parse as block
        // Skip leading whitespace/newlines in body
        size_t body_content_start = body_start;
        while (body_content_start < body_end && (line[body_content_start] == ' ' || line[body_content_start] == '\t' || line[body_content_start] == '\n' || line[body_content_start] == '\r')) body_content_start++;
        size_t body_content_len = body_end - body_content_start;
        char *body_text = malloc(body_content_len + 2);
        if (body_content_len > 0) memcpy(body_text, line + body_content_start, body_content_len);
        body_text[body_content_len] = '\0';

        // Parse body: create a mini-Program and use the block parser.
        // Split merged body text into individual statement lines first,
        // since the file loader merges multi-line blocks into single lines.
        Program body_prog = {0};
        body_prog.path = qs_strdup("<class_method>");
        body_prog.dir = qs_strdup(".");
        split_text_to_program(body_text, &body_prog);
        ASTNode *body = ast_parse_block_stmts(&body_prog, 0, body_prog.count);
        free_program(&body_prog);
        free(body_text);

        ASTNode *method = ast_method_def(mname, params, param_types, param_count, return_type, body, (int)*index + 1);
        method_count++;
        methods = realloc(methods, method_count * sizeof(ASTNode *));
        methods[method_count - 1] = method;

        open_pos = body_end + 1; // skip past closing }
    }

    ASTNode *node = ast_class_def(name, fields, field_count, methods, method_count, (int)*index + 1);
    return node;
}

static ASTNode *ast_parse_statement(Program *source, size_t i, size_t end, size_t *end_index) {
    char *line = source->items[i];
    if (!line) return NULL;

    size_t len = strlen(line);
    size_t pos = 0;
    while (pos < len && (line[pos] == ' ' || line[pos] == '\t')) pos++;
    if (pos >= len || line[pos] == '\n' || line[pos] == '\r' || line[pos] == '#') return NULL;

    // Return
    if (line_starts_kw(line, "return")) {
        size_t after = after_keyword(line, "return");
        ASTParser rp = {line, after, (int)i + 1};
        ast_skip(&rp);
        ASTNode *node = ast_new(AST_RETURN, (int)i + 1);
        if (rp.text[rp.pos] && rp.text[rp.pos] != '#' && rp.text[rp.pos] != '\n') {
            node->as.unary.operand = ast_parse_expression(&rp);
        }
        *end_index = i;
        return node;
    }

    // Break
    if (line_starts_kw(line, "break")) { *end_index = i; return ast_new(AST_BREAK, (int)i + 1); }

    // Continue
    if (line_starts_kw(line, "continue")) { *end_index = i; return ast_new(AST_CONTINUE, (int)i + 1); }

    // If — parse entire chain, advance past all branches
    if (line_starts_kw(line, "if")) {
        size_t idx = i;
        ASTNode *node = ast_parse_if_chain(source, &idx, end);
        *end_index = idx;
        return node;
    }

    // While
    if (line_starts_kw(line, "while")) {
        size_t idx = i;
        ASTNode *node = ast_parse_while_loop(source, &idx, end);
        *end_index = idx;
        return node;
    }

    // Repeat
    if (line_starts_kw(line, "repeat")) {
        size_t idx = i;
        ASTNode *node = ast_parse_repeat_loop(source, &idx, end);
        *end_index = idx;
        return node;
    }

    // Class definition: class Name { ... }
    if (line_starts_kw(line, "class")) {
        size_t idx = i;
        ASTNode *node = ast_parse_class_def(source, &idx, end);
        *end_index = idx;
        return node;
    }

    // Function definition: fn name(params) { body }
    if (line_starts_kw(line, "fn") || line_starts_kw(line, "func")) {
        size_t idx = i;
        ASTNode *node = ast_parse_func_def(source, &idx);
        *end_index = idx;
        return node;
    }

    // Try
    if (line_starts_kw(line, "try")) {
        size_t idx = i;
        ASTNode *node = ast_parse_try_block(source, &idx, end);
        *end_index = idx;
        return node;
    }

    // Print (multi-arg): print(a, b, c)
    if (line_starts_kw(line, "print")) {
        size_t after = after_keyword(line, "print");
        ASTParser pp = {line, after, (int)i + 1};
        ast_skip(&pp);
        if (pp.text[pp.pos] == '(') {
            pp.pos++; // skip (
            ASTNode **args = NULL;
            int argc = 0;
            ast_skip(&pp);
            while (pp.text[pp.pos] && pp.text[pp.pos] != ')') {
                ASTNode *arg = ast_parse_expression(&pp);
                argc++;
                args = realloc(args, argc * sizeof(ASTNode *));
                args[argc - 1] = arg;
                ast_skip(&pp);
                if (pp.text[pp.pos] == ',') pp.pos++;
                ast_skip(&pp);
            }
            if (pp.text[pp.pos] == ')') pp.pos++;
            ASTNode *node = ast_new(AST_PRINT, (int)i + 1);
            node->as.print.args = args;
            node->as.print.argc = argc;
            *end_index = i;
            return node;
        }
    }

    // Import / from...import — store as expression statement
    if (line_starts_kw(line, "import") || line_starts_kw(line, "from")) {
        ASTNode *node = ast_new(AST_IMPORT, (int)i + 1);
        node->as.string = qs_strndup(line + pos, len - pos);
        *end_index = i;
        return node;
    }

    // Assignment: name = expr, name += expr, obj.field = expr, etc.
    {
        size_t saved_pos = pos;
        size_t nstart = pos;
        while (pos < len && (isalnum(line[pos]) || line[pos] == '_' || line[pos] == '.')) pos++;
        if (pos > nstart && pos < len) {
            size_t opos = pos;
            while (opos < len && (line[opos] == ' ' || line[opos] == '\t')) opos++;
            char aop = '\0';
            if (opos < len && line[opos] == '=' && (opos + 1 >= len || line[opos + 1] != '=')) aop = '=';
            else if (opos + 1 < len && line[opos] == '+' && line[opos + 1] == '=') aop = '+';
            else if (opos + 1 < len && line[opos] == '-' && line[opos + 1] == '=') aop = '-';
            else if (opos + 1 < len && line[opos] == '*' && line[opos + 1] == '=') aop = '*';
            else if (opos + 1 < len && line[opos] == '/' && line[opos + 1] == '=') aop = '/';
            else if (opos + 1 < len && line[opos] == '%' && line[opos + 1] == '=') aop = '%';

            if (aop) {
                char *fullname = qs_strndup(line + nstart, pos - nstart);
                // Check if this is a field assignment (contains a dot)
                char *dot = strchr(fullname, '.');
                if (dot && aop == '=') {
                    // Field assignment: obj.field = expr
                    size_t obj_len = dot - fullname;
                    char *field_name = dot + 1;
                    // Create object as simple identifier (up to the dot)
                    char *obj_name = qs_strndup(fullname, obj_len);
                    ASTNode *obj = ast_ident(obj_name, (int)i + 1);
                    if (obj) {
                        size_t rhs_start = opos + 1;
                        ASTParser vp = {line, rhs_start, (int)i + 1};
                        ASTNode *val = ast_parse_expression(&vp);
                        ASTNode *node = ast_field_assign(obj, qs_strdup(field_name), val, (int)i + 1);
                        free(fullname);
                        *end_index = i;
                        return node;
                    }
                }
                // Simple assignment
                size_t rhs_start = aop == '=' ? opos + 1 : opos + 2;
                ASTParser vp = {line, rhs_start, (int)i + 1};
                ASTNode *val = ast_parse_expression(&vp);
                ASTNode *node = ast_new(AST_ASSIGN, (int)i + 1);
                node->as.assign.name = fullname;
                node->as.assign.op = aop;
                node->as.assign.value = val;
                *end_index = i;
                return node;
            }
        }
        // No assignment found — restore pos so expression parser starts at beginning of line
        pos = saved_pos;
    }

    // Expression statement (method calls, function calls, etc.)
    {
        ASTParser ep = {line, pos, (int)i + 1};
        ASTNode *expr = ast_parse_expression(&ep);
        if (expr) {
            ASTNode *node = ast_new(AST_EXPR_STMT, (int)i + 1);
            node->as.unary.operand = expr;
            *end_index = i;
            return node;
        }
    }

    *end_index = i;
    return NULL;
}

// Parse a block of statements into AST_BLOCK
static ASTNode *ast_parse_block_stmts(Program *source, size_t start, size_t end) {
    ASTNode *block = ast_new(AST_BLOCK, 0);
    block->as.block.stmts = NULL;
    block->as.block.count = 0;
    block->as.block.capacity = 0;

    size_t i = start;
    while (i < end) {
        char *line = source->items[i];
        if (!line) { i++; continue; }

        size_t len = strlen(line);
        size_t pos = 0;
        while (pos < len && (line[pos] == ' ' || line[pos] == '\t')) pos++;
        if (pos >= len || line[pos] == '#' || line[pos] == '\n' || line[pos] == '\r') { i++; continue; }

        size_t stmt_end = i;
        ASTNode *stmt = ast_parse_statement(source, i, end, &stmt_end);
        if (stmt) {
            block->as.block.stmts = realloc(block->as.block.stmts, (block->as.block.count + 1) * sizeof(ASTNode *));
            block->as.block.stmts[block->as.block.count++] = stmt;
            i = stmt_end + 1;
        } else {
            i++;
        }
    }
    return block;
}

// ---- Top-level entry point ----
static ASTNode *ast_parse_program(Program *source) {
    ASTNode *prog = ast_new(AST_PROGRAM, 0);
    ASTNode *block = ast_parse_block_stmts(source, 0, source->count);
    prog->as.block.stmts = block->as.block.stmts;
    prog->as.block.count = block->as.block.count;
    prog->as.block.capacity = block->as.block.capacity;
    block->as.block.stmts = NULL; // transfer ownership
    block->as.block.count = 0;
    ast_free(block);
    return prog;
}
