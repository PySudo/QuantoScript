/* Minimal Language Server Protocol implementation for QuantoScript.
 *
 * Speaks JSON-RPC 2.0 over stdin/stdout using LSP's Content-Length framing.
 * This is deliberately dependency-free: it parses only the fields it needs
 * and emits diagnostics by reusing the existing block/import/function checks.
 */

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

typedef struct {
    char *uri;
    char *text;
} LspDocument;

static LspDocument *g_lsp_docs = NULL;
static int g_lsp_doc_count = 0;
static int g_lsp_doc_capacity = 0;
static int g_lsp_initialized = 0;
static int g_lsp_shutdown = 0;

static LspDocument *lsp_find_doc(const char *uri) {
    if (!uri) return NULL;
    for (int i = 0; i < g_lsp_doc_count; i++) {
        if (strcmp(g_lsp_docs[i].uri, uri) == 0) return &g_lsp_docs[i];
    }
    return NULL;
}

static LspDocument *lsp_upsert_doc(const char *uri) {
    LspDocument *doc = lsp_find_doc(uri);
    if (doc) return doc;
    if (g_lsp_doc_count == g_lsp_doc_capacity) {
        int next = g_lsp_doc_capacity ? g_lsp_doc_capacity * 2 : 8;
        LspDocument *grown = realloc(g_lsp_docs, (size_t)next * sizeof(LspDocument));
        if (!grown) { fprintf(stderr, "out of memory\n"); exit(2); }
        g_lsp_docs = grown;
        g_lsp_doc_capacity = next;
    }
    doc = &g_lsp_docs[g_lsp_doc_count++];
    doc->uri = qs_strdup(uri);
    doc->text = NULL;
    return doc;
}

static void lsp_remove_doc(const char *uri) {
    for (int i = 0; i < g_lsp_doc_count; i++) {
        if (strcmp(g_lsp_docs[i].uri, uri) == 0) {
            free(g_lsp_docs[i].uri);
            free(g_lsp_docs[i].text);
            g_lsp_docs[i] = g_lsp_docs[g_lsp_doc_count - 1];
            g_lsp_doc_count--;
            return;
        }
    }
}

static void lsp_set_doc_text(LspDocument *doc, const char *text) {
    free(doc->text);
    doc->text = qs_strdup(text ? text : "");
}

static void lsp_write_raw(const char *bytes, size_t len) {
    fwrite(bytes, 1, len, stdout);
    fflush(stdout);
}

static void lsp_send(const char *json_body) {
    char header[64];
    int n = snprintf(header, sizeof(header), "Content-Length: %zu\r\n\r\n", strlen(json_body));
    lsp_write_raw(header, (size_t)n);
    lsp_write_raw(json_body, strlen(json_body));
}

/* Reads one LSP message from stdin into a heap buffer (caller frees). Returns
 * NULL on EOF. Only understands the Content-Length header, which is all the
 * protocol sends. */
static char *lsp_read_message(void) {
    size_t content_length = 0;
    int have_length = 0;
    char line[512];

    while (1) {
        if (!fgets(line, sizeof(line), stdin)) {
            return NULL;
        }
        size_t llen = strlen(line);
        while (llen > 0 && (line[llen - 1] == '\n' || line[llen - 1] == '\r')) {
            line[--llen] = '\0';
        }
        if (llen == 0) break;
        if (strncmp(line, "Content-Length:", 15) == 0) {
            content_length = (size_t)strtoul(line + 15, NULL, 10);
            have_length = 1;
        }
    }

    if (!have_length) {
        return NULL;
    }
    char *body = malloc(content_length + 1);
    if (!body) { fprintf(stderr, "out of memory\n"); exit(2); }
    size_t read_total = fread(body, 1, content_length, stdin);
    body[read_total] = '\0';
    return body;
}

/* ---- tiny JSON helpers -------------------------------------------------- */

/* Extracts a string value for a top-level-ish "key":"..." pair. Not a general
 * JSON parser, but robust enough for the LSP requests we handle. Returns a
 * heap string or NULL. */
static char *lsp_json_get_string(const char *json, const char *key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = json;
    while ((p = strstr(p, pattern)) != NULL) {
        p += strlen(pattern);
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (*p != ':') continue;
        p++;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (*p != '"') continue;
        p++;
        size_t cap = 64, len = 0;
        char *out = malloc(cap);
        if (!out) { fprintf(stderr, "out of memory\n"); exit(2); }
        while (*p && *p != '"') {
            char c = *p++;
            if (c == '\\' && *p) {
                char esc = *p++;
                switch (esc) {
                    case 'n': c = '\n'; break;
                    case 't': c = '\t'; break;
                    case 'r': c = '\r'; break;
                    case '"': c = '"'; break;
                    case '\\': c = '\\'; break;
                    case '/': c = '/'; break;
                    case 'u': {
                        char hex[5] = {0,0,0,0,0};
                        for (int i = 0; i < 4 && *p; i++) hex[i] = *p++;
                        unsigned int code = (unsigned int)strtoul(hex, NULL, 16);
                        if (code >= 0x80) {
                            c = '?';
                        } else {
                            c = (char)code;
                        }
                        break;
                    }
                    default: c = esc; break;
                }
            }
            if (len + 2 > cap) {
                cap *= 2;
                char *grown = realloc(out, cap);
                if (!grown) { fprintf(stderr, "out of memory\n"); exit(2); }
                out = grown;
            }
            out[len++] = c;
        }
        out[len] = '\0';
        return out;
    }
    return NULL;
}

/* Reads the JSON-RPC "id" field verbatim. LSP ids are integers or strings.
 * Returns a heap string, or NULL when the message is a notification. */
static char *lsp_json_get_id(const char *json) {
    const char *p = strstr(json, "\"id\"");
    if (!p) return NULL;
    p += 4;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != ':') return NULL;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p == '"') {
        p++;
        size_t cap = 32, len = 0;
        char *out = malloc(cap);
        if (!out) { fprintf(stderr, "out of memory\n"); exit(2); }
        out[len++] = '"';
        while (*p && *p != '"') {
            if (len + 2 >= cap) { cap *= 2; out = realloc(out, cap); if (!out) exit(2); }
            out[len++] = *p++;
        }
        out[len++] = '"';
        out[len] = '\0';
        return out;
    }
    if (*p == 'n') return NULL; /* null */
    const char *start = p;
    while (*p && ((*p >= '0' && *p <= '9') || *p == '-')) p++;
    size_t len = (size_t)(p - start);
    char *out = malloc(len + 1);
    if (!out) { fprintf(stderr, "out of memory\n"); exit(2); }
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

static const char *lsp_json_get_method(const char *json) {
    static char method[128];
    char *m = lsp_json_get_string(json, "method");
    if (!m) return NULL;
    snprintf(method, sizeof(method), "%s", m);
    free(m);
    return method;
}

/* ---- responses ---------------------------------------------------------- */

static void lsp_reply_result(const char *id, const char *result_json) {
    char *body = malloc(strlen(result_json) + strlen(id) + 64);
    if (!body) { fprintf(stderr, "out of memory\n"); exit(2); }
    sprintf(body, "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":%s}", id, result_json);
    lsp_send(body);
    free(body);
}

static void lsp_reply_error(const char *id, int code, const char *message) {
    char *body = malloc(strlen(message) * 6 + strlen(id) + 128);
    if (!body) { fprintf(stderr, "out of memory\n"); exit(2); }
    sprintf(body, "{\"jsonrpc\":\"2.0\",\"id\":%s,\"error\":{\"code\":%d,\"message\":\"", id, code);
    size_t used = strlen(body);
    char *esc = malloc(strlen(message) * 6 + 1);
    if (!esc) { fprintf(stderr, "out of memory\n"); exit(2); }
    size_t e = 0;
    for (const unsigned char *p = (const unsigned char *)message; *p; p++) {
        if (*p == '"' || *p == '\\') { esc[e++] = '\\'; esc[e++] = (char)*p; }
        else if (*p < 0x20) { e += (size_t)sprintf(esc + e, "\\u%04x", *p); }
        else esc[e++] = (char)*p;
    }
    esc[e] = '\0';
    strcpy(body + used, esc);
    strcat(body, "\"}}");
    free(esc);
    lsp_send(body);
    free(body);
}

static void lsp_notify(const char *method, const char *params_json) {
    char *body = malloc(strlen(method) + strlen(params_json) + 64);
    if (!body) { fprintf(stderr, "out of memory\n"); exit(2); }
    sprintf(body, "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}", method, params_json);
    lsp_send(body);
    free(body);
}

/* ---- diagnostics -------------------------------------------------------- */

/* Smart-quote rule; a quote closes when the following non-space character on
 * the same line ends a token. Mirrors quote_can_close but never crosses a
 * newline (the raw document is not split into physical lines here). */
static int lsp_quote_closes(const char *text, size_t pos, size_t line_end) {
    size_t scan = pos + 1;
    while (scan < line_end && (text[scan] == ' ' || text[scan] == '\t')) scan++;
    if (scan >= line_end) return 1;
    char c = text[scan];
    if (c == '#' || c == '\r' || c == ',' || c == ':' || c == ')' || c == ']' ||
        c == '}' || c == '{' || c == '[' || c == '.') {
        return 1;
    }
    if (strchr("+-*/%=<>!", c) != NULL) return 1;
    if (strncmp(text + scan, "in", 2) == 0 && !is_name_part(text[scan + 2])) return 1;
    if (strncmp(text + scan, "if", 2) == 0 && !is_name_part(text[scan + 2])) return 1;
    if (strncmp(text + scan, "or", 2) == 0 && !is_name_part(text[scan + 2])) return 1;
    if (strncmp(text + scan, "and", 3) == 0 && !is_name_part(text[scan + 3])) return 1;
    if (strncmp(text + scan, "import", 6) == 0 && !is_name_part(text[scan + 6])) return 1;
    return 0;
}

/* Light structural scan that catches the errors the block/import/function
 * checks miss: unbalanced (), [] and stray closers. Line-aware, so quote
 * strings never span a newline. Returns 1 if balanced, else 0 (fills *err). */
static int lsp_check_balance(const char *text, ParseError *err) {
    long paren = 0, bracket = 0;
    long paren_line = 0, bracket_line = 0;
    size_t line_start = 0;
    int line = 1;

    while (1) {
        size_t nl = line_start;
        while (text[nl] && text[nl] != '\n') nl++;

        int in_string = 0;
        char quote = '\0';

        for (size_t i = line_start; i < nl; i++) {
            char c = text[i];
            int col = (int)(i - line_start) + 1;

            if (in_string) {
                if (c == '\\' && i + 1 < nl) { i++; continue; }
                if (c == quote && lsp_quote_closes(text, i, nl)) {
                    in_string = 0;
                    quote = '\0';
                }
                continue;
            }

            if (c == '#') break;
            if (c == '"' || c == '\'') {
                in_string = 1;
                quote = c;
                continue;
            }
            if (c == '(') {
                if (paren == 0) paren_line = line;
                paren++;
            } else if (c == ')') {
                if (paren == 0) {
                    snprintf(err->message, sizeof(err->message), "unexpected ')'");
                    err->line = line;
                    err->column = col;
                    return 0;
                }
                paren--;
            } else if (c == '[') {
                if (bracket == 0) bracket_line = line;
                bracket++;
            } else if (c == ']') {
                if (bracket == 0) {
                    snprintf(err->message, sizeof(err->message), "unexpected ']'");
                    err->line = line;
                    err->column = col;
                    return 0;
                }
                bracket--;
            }
        }

        if (in_string) {
            snprintf(err->message, sizeof(err->message), "unterminated string literal");
            err->line = line;
            err->column = (int)(nl - line_start) + 1;
            return 0;
        }

        if (!text[nl]) break;
        line_start = nl + 1;
        line++;
    }

    if (paren > 0) {
        snprintf(err->message, sizeof(err->message), "missing ')'");
        err->line = paren_line;
        err->column = 1;
        return 0;
    }
    if (bracket > 0) {
        snprintf(err->message, sizeof(err->message), "missing ']'");
        err->line = bracket_line;
        err->column = 1;
        return 0;
    }
    return 1;
}

static void lsp_publish_diagnostics(const char *uri) {
    LspDocument *doc = lsp_find_doc(uri);
    if (!doc || !doc->text) {
        return;
    }

    ParseError err = {0};
    Environment env = {0};

    Program program = {0};
    int ok = lsp_check_balance(doc->text, &err);
    if (ok) {
        split_text_to_program(doc->text, &program);
        strip_block_comments(&program);
        ok = check_program_blocks(&program, &err);
    }

    char *buf = NULL;
    size_t cap = 256, len = 0;
    buf = malloc(cap);
    if (!buf) { fprintf(stderr, "out of memory\n"); exit(2); }

    #define LSP_APPEND(...) do { \
        int _need = snprintf(NULL, 0, __VA_ARGS__); \
        if (_need < 0) _need = 0; \
        while (len + (size_t)_need + 1 > cap) { cap *= 2; buf = realloc(buf, cap); if (!buf) exit(2); } \
        snprintf(buf + len, (size_t)_need + 1, __VA_ARGS__); \
        len += (size_t)_need; \
    } while (0)

    LSP_APPEND("{\"uri\":\"");
    char *esc = malloc(strlen(uri) * 6 + 1);
    if (!esc) { fprintf(stderr, "out of memory\n"); exit(2); }
    size_t e = 0;
    for (const unsigned char *p = (const unsigned char *)uri; *p; p++) {
        if (*p == '"' || *p == '\\') { esc[e++] = '\\'; esc[e++] = (char)*p; }
        else if (*p < 0x20) { e += (size_t)sprintf(esc + e, "\\u%04x", *p); }
        else esc[e++] = (char)*p;
    }
    esc[e] = '\0';
    LSP_APPEND("%s", esc);
    free(esc);
    LSP_APPEND("\",\"diagnostics\":[");

    if (!ok) {
        int line = err.line > 0 ? err.line - 1 : 0;
        int col = err.column > 0 ? err.column - 1 : 0;
        char escmsg[1600];
        size_t m = 0;
        for (const unsigned char *p = (const unsigned char *)err.message; *p && m + 7 < sizeof(escmsg); p++) {
            if (*p == '"' || *p == '\\') { escmsg[m++] = '\\'; escmsg[m++] = (char)*p; }
            else if (*p == '\n') { escmsg[m++] = '\\'; escmsg[m++] = 'n'; }
            else if (*p < 0x20) { m += (size_t)sprintf(escmsg + m, "\\u%04x", *p); }
            else escmsg[m++] = (char)*p;
        }
        escmsg[m] = '\0';
        LSP_APPEND("{\"range\":{\"start\":{\"line\":%d,\"character\":%d},\"end\":{\"line\":%d,\"character\":%d}},"
                   "\"severity\":1,\"source\":\"quanto\",\"message\":\"%s\"}",
                   line, col, line, col, escmsg);
    }

    LSP_APPEND("]}");
    #undef LSP_APPEND

    lsp_notify("textDocument/publishDiagnostics", buf);
    free(buf);

    free_environment(&env);
    object_registry_free_all();
    free_functions();
    free_modules();
    free_call_stack();
    free_program(&program);
}

/* ---- hover -------------------------------------------------------------- */

static const char *lsp_keyword_hover(const char *word) {
    static const struct { const char *kw; const char *doc; } table[] = {
        {"func", "**func** - declares a function."},
        {"class", "**class** - declares a class."},
        {"if", "**if** - conditional branch."},
        {"else", "**else** - alternative branch."},
        {"while", "**while** - loop while a condition holds."},
        {"repeat", "**repeat** - loop a fixed number of times or over a collection."},
        {"return", "**return** - return a value from a function."},
        {"and", "**and** - logical AND; both operands are evaluated."},
        {"or", "**or** - logical OR; both operands are evaluated."},
        {"not", "**not** - logical NOT."},
        {"true", "**true** - boolean true."},
        {"false", "**false** - boolean false."},
        {"null", "**null** - the absence of a value."},
        {"try", "**try** - guarded block."},
        {"oops", "**oops** - catch (error handler) block."},
        {"import", "**import** - import another module."},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (strcmp(table[i].kw, word) == 0) return table[i].doc;
    }
    return NULL;
}

static int lsp_is_name_char(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static void lsp_handle_hover(const char *id, const char *uri, int line0, int char0) {
    LspDocument *doc = lsp_find_doc(uri);
    if (!doc || !doc->text) {
        lsp_reply_result(id, "null");
        return;
    }

    const char *p = doc->text;
    int cur_line = 0;
    while (*p && cur_line < line0) {
        if (*p == '\n') cur_line++;
        p++;
    }
    if (cur_line != line0) { lsp_reply_result(id, "null"); return; }

    const char *line_start = p;
    const char *line_end = p;
    while (*line_end && *line_end != '\n') line_end++;
    size_t line_len = (size_t)(line_end - line_start);
    if (char0 < 0 || (size_t)char0 > line_len) { lsp_reply_result(id, "null"); return; }

    size_t ws = (size_t)char0;
    while (ws > 0 && lsp_is_name_char((unsigned char)line_start[ws - 1])) ws--;
    size_t we = (size_t)char0;
    while (we < line_len && lsp_is_name_char((unsigned char)line_start[we])) we++;
    if (we == ws) { lsp_reply_result(id, "null"); return; }

    char word[128];
    size_t wl = we - ws;
    if (wl >= sizeof(word)) wl = sizeof(word) - 1;
    memcpy(word, line_start + ws, wl);
    word[wl] = '\0';

    const char *doc_text = lsp_keyword_hover(word);
    if (!doc_text) { lsp_reply_result(id, "null"); return; }

    char *body = malloc(strlen(doc_text) + 128);
    if (!body) { fprintf(stderr, "out of memory\n"); exit(2); }
    sprintf(body, "{\"contents\":{\"kind\":\"markdown\",\"value\":\"");
    size_t used = strlen(body);
    for (const char *q = doc_text; *q; q++) {
        if (*q == '"' || *q == '\\') body[used++] = '\\';
        body[used++] = *q;
    }
    strcpy(body + used, "\"}}");
    lsp_reply_result(id, body);
    free(body);
}

/* ---- request handlers --------------------------------------------------- */

static void lsp_handle_initialize(const char *id) {
    g_lsp_initialized = 1;
    lsp_reply_result(id,
        "{\"capabilities\":{\"textDocumentSync\":1,"
        "\"hoverProvider\":true},"
        "\"serverInfo\":{\"name\":\"quanto-lsp\",\"version\":\"" QS_VERSION "\"}}");
}

static void lsp_handle_text_document_did_open(const char *json) {
    char *uri = lsp_json_get_string(json, "uri");
    char *text = lsp_json_get_string(json, "text");
    if (uri) {
        LspDocument *doc = lsp_upsert_doc(uri);
        lsp_set_doc_text(doc, text ? text : "");
        lsp_publish_diagnostics(uri);
    }
    free(uri);
    free(text);
}

static void lsp_handle_text_document_did_change(const char *json) {
    char *uri = lsp_json_get_string(json, "uri");
    /* Full sync: the last "text" field in the change array is the whole doc. */
    char *text = lsp_json_get_string(json, "text");
    if (uri) {
        LspDocument *doc = lsp_upsert_doc(uri);
        if (text) lsp_set_doc_text(doc, text);
        lsp_publish_diagnostics(uri);
    }
    free(uri);
    free(text);
}

static void lsp_handle_text_document_did_save(const char *json) {
    char *uri = lsp_json_get_string(json, "uri");
    if (uri) lsp_publish_diagnostics(uri);
    free(uri);
}

static void lsp_handle_text_document_did_close(const char *json) {
    char *uri = lsp_json_get_string(json, "uri");
    if (uri) {
        lsp_publish_diagnostics(uri);
        lsp_remove_doc(uri);
    }
    free(uri);
}

/* Extracts an integer field such as position "line"/"character". */
static int lsp_json_get_int(const char *json, const char *key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != ':') return -1;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return (int)strtol(p, NULL, 10);
}

static int run_lsp_server(void) {
    /* The protocol is byte-exact, so suppress Windows text-mode newline
     * translation on both stdio streams. */
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    while (1) {
        char *msg = lsp_read_message();
        if (!msg) break;

        const char *method = lsp_json_get_method(msg);
        char *id = lsp_json_get_id(msg);

        if (method && strcmp(method, "initialize") == 0) {
            lsp_handle_initialize(id ? id : "null");
        } else if (method && strcmp(method, "initialized") == 0) {
            /* notification, no reply */
        } else if (method && strcmp(method, "shutdown") == 0) {
            g_lsp_shutdown = 1;
            lsp_reply_result(id ? id : "null", "null");
        } else if (method && strcmp(method, "exit") == 0) {
            free(id);
            free(msg);
            break;
        } else if (method && strcmp(method, "textDocument/didOpen") == 0) {
            lsp_handle_text_document_did_open(msg);
        } else if (method && strcmp(method, "textDocument/didChange") == 0) {
            lsp_handle_text_document_did_change(msg);
        } else if (method && strcmp(method, "textDocument/didSave") == 0) {
            lsp_handle_text_document_did_save(msg);
        } else if (method && strcmp(method, "textDocument/didClose") == 0) {
            lsp_handle_text_document_did_close(msg);
        } else if (method && strcmp(method, "textDocument/hover") == 0) {
            char *uri = lsp_json_get_string(msg, "uri");
            int line = lsp_json_get_int(msg, "line");
            int ch = lsp_json_get_int(msg, "character");
            lsp_handle_hover(id ? id : "null", uri ? uri : "", line, ch);
            free(uri);
        } else if (id) {
            lsp_reply_error(id, -32601, "method not found");
        }

        free(id);
        free(msg);
    }

    for (int i = 0; i < g_lsp_doc_count; i++) {
        free(g_lsp_docs[i].uri);
        free(g_lsp_docs[i].text);
    }
    free(g_lsp_docs);
    g_lsp_docs = NULL;
    g_lsp_doc_count = 0;
    g_lsp_doc_capacity = 0;
    return g_lsp_shutdown ? 0 : 0;
}
