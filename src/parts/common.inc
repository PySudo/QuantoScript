static char *qs_strdup(const char *text) {
    size_t len = strlen(text);
    char *copy = malloc(len + 1);
    if (!copy) {
        fprintf(stderr, "out of memory\n");
        exit(2);
    }
    memcpy(copy, text, len + 1);
    return copy;
}

static char *qs_strndup(const char *text, size_t len) {
    char *copy = malloc(len + 1);
    if (!copy) {
        fprintf(stderr, "out of memory\n");
        exit(2);
    }
    memcpy(copy, text, len);
    copy[len] = '\0';
    return copy;
}

static void set_error(ParseError *err, int line, int column, const char *message) {
    err->line = line;
    err->column = column;
    snprintf(err->message, sizeof(err->message), "%s", message);
}

static void parser_error(Parser *parser, const char *message) {
    set_error(parser->err, parser->line, (int)parser->pos + 1, message);
}

static void skip_spaces_text(const char *text, size_t *pos) {
    while (text[*pos] && isspace((unsigned char)text[*pos])) {
        (*pos)++;
    }
}

static void skip_spaces(Parser *parser) {
    skip_spaces_text(parser->text, &parser->pos);
}

static int is_name_start(char ch) {
    return isalpha((unsigned char)ch) || ch == '_';
}

static int is_name_part(char ch) {
    return isalnum((unsigned char)ch) || ch == '_';
}

static int starts_with_word(Parser *parser, const char *word) {
    size_t len = strlen(word);
    if (strncmp(parser->text + parser->pos, word, len) != 0) {
        return 0;
    }
    return !is_name_part(parser->text[parser->pos + len]);
}

static int match_word(Parser *parser, const char *word) {
    skip_spaces(parser);
    if (starts_with_word(parser, word)) {
        parser->pos += strlen(word);
        return 1;
    }
    return 0;
}

static int match_char(Parser *parser, char ch) {
    skip_spaces(parser);
    if (parser->text[parser->pos] == ch) {
        parser->pos++;
        return 1;
    }
    return 0;
}

static int match_op(Parser *parser, const char *op) {
    size_t len = strlen(op);
    skip_spaces(parser);
    if (strncmp(parser->text + parser->pos, op, len) == 0) {
        parser->pos += len;
        return 1;
    }
    return 0;
}

static int is_define_name(const char *name) {
    int has_alpha = 0;
    for (size_t i = 0; name[i]; i++) {
        if (isalpha((unsigned char)name[i])) {
            has_alpha = 1;
            if (!isupper((unsigned char)name[i])) {
                return 0;
            }
        }
    }
    return has_alpha;
}
