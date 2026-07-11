#ifdef QS_PYTHON
#include <Python.h>
#endif

// PCG32 PRNG state (portable, deterministic, cross-platform identical)
static uint64_t g_pcg_state = 0x853c49e6748fea9bULL;
static uint64_t g_pcg_inc = 0xda3e39cb94b95bdbULL;

// Forward declarations for functions defined later in this file or in other files
static int args_need(Parser *parser, CallArgList *args, size_t count, const char *name);
static int arg_string(Parser *parser, CallArgList *args, size_t index, const char *name, const char **out);
static int arg_integer(Parser *parser, CallArgList *args, size_t index, const char *name, long *out);
static int parse_http_url(const char *url, char **host, char **port, char **path, int *secure);
static qs_socket_t tcp_connect_socket(const char *host, const char *port);
static char *tcp_exchange(const char *host, const char *port, const char *message);
static int sandbox_check_path(const char *path);
static Value make_response_map(int ok, long status, char *body, char *raw);
static char *trim_string_copy(const char *text);
static Value split_string_value(const char *text, const char *separator);
// From 07_parser.inc
static int values_equal(Value left, Value right);
static int json_parse_value(const char *text, int *pos, Value *out, int *err);
static char *json_stringify_value(Value value);
// Windows-only HTTPS
#ifdef _WIN32
static char *https_request_winhttp(const char *method, const char *host, const char *port, const char *path, const char *body, long *status);
#endif

static long g_last_http_error = 0;

static wchar_t *utf8_to_wide(const char *text) {
#ifdef _WIN32
    int needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (needed <= 0) return NULL;
    wchar_t *wide = calloc((size_t)needed, sizeof(wchar_t));
    if (!wide) { fprintf(stderr, "out of memory\n"); exit(2); }
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, needed);
    return wide;
#else
    (void)text;
    return NULL;
#endif
}

static char *wide_to_utf8(const wchar_t *wide) {
#ifdef _WIN32
    int needed = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (needed <= 0) return qs_strdup("");
    char *text = malloc((size_t)needed);
    if (!text) { fprintf(stderr, "out of memory\n"); exit(2); }
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, text, needed, NULL, NULL);
    return text;
#else
    (void)wide;
    return NULL;
#endif
}

static char *https_request_openssl(const char *method, const char *host, const char *port, const char *path, const char *body, long *status) {
    g_last_http_error = 0;
    qs_socket_t sock = tcp_connect_socket(host, port);
    if (sock == QS_INVALID_SOCKET) {
        g_last_http_error = 12029;
        return NULL;
    }

    static int ssl_started = 0;
    if (!ssl_started) {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();
        ssl_started = 1;
    }

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        g_last_http_error = (long)ERR_get_error();
        qs_close_socket(sock);
        return NULL;
    }

    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
        g_last_http_error = (long)ERR_get_error();
        SSL_CTX_free(ctx);
        qs_close_socket(sock);
        return NULL;
    }

    SSL_set_tlsext_host_name(ssl, host);
#ifdef _WIN32
    SSL_set_fd(ssl, (int)sock);
#else
    SSL_set_fd(ssl, sock);
#endif

    if (SSL_connect(ssl) != 1) {
        g_last_http_error = (long)ERR_get_error();
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        qs_close_socket(sock);
        return NULL;
    }

    size_t body_len = body ? strlen(body) : 0;
    size_t request_len = strlen(method) + strlen(host) + strlen(path) + 128 + body_len;
    char *request = malloc(request_len);
    if (!request) { fprintf(stderr, "out of memory\n"); exit(2); }
    if (body_len > 0) {
        snprintf(request, request_len,
            "%s %s HTTP/1.0\r\nHost: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",
            method, path, host, body_len, body);
    } else {
        snprintf(request, request_len,
            "%s %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
            method, path, host);
    }

    size_t sent = 0;
    size_t request_size = strlen(request);
    while (sent < request_size) {
        int wrote = SSL_write(ssl, request + sent, (int)(request_size - sent));
        if (wrote <= 0) {
            g_last_http_error = (long)ERR_get_error();
            free(request);
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            qs_close_socket(sock);
            return NULL;
        }
        sent += (size_t)wrote;
    }
    free(request);

    size_t count = 0;
    size_t capacity = 4096;
    char *raw = malloc(capacity + 1);
    if (!raw) { fprintf(stderr, "out of memory\n"); exit(2); }

    while (1) {
        if (count + 2048 > capacity) {
            capacity *= 2;
            char *next = realloc(raw, capacity + 1);
            if (!next) { fprintf(stderr, "out of memory\n"); exit(2); }
            raw = next;
        }
        int got = SSL_read(ssl, raw + count, 2048);
        if (got <= 0) {
            int ssl_error = SSL_get_error(ssl, got);
            if (ssl_error != SSL_ERROR_ZERO_RETURN) {
                unsigned long err = ERR_get_error();
                if (err) g_last_http_error = (long)err;
            }
            break;
        }
        count += (size_t)got;
    }
    raw[count] = '\0';

    SSL_free(ssl);
    SSL_CTX_free(ctx);
    qs_close_socket(sock);

    sscanf(raw, "HTTP/%*s %ld", status);
    return raw;
}

static Value make_response_map(int ok, long status, char *body, char *raw) {
    Value response = make_dict_empty();
    dict_set(&response.as.dict, "ok", make_bool(ok));
    dict_set(&response.as.dict, "status", make_integer(status));
    dict_set(&response.as.dict, "error", make_integer(0));
    dict_set(&response.as.dict, "body", make_string_owned(body ? body : qs_strdup("")));
    dict_set(&response.as.dict, "raw", make_string_owned(raw ? raw : qs_strdup("")));
    return response;
}

static int builtin_http_request(Parser *parser, const char *method, CallArgList *args, Value *out) {
    if (args->count < 1 || args->count > 2) {
        char msg[128];
        snprintf(msg, sizeof(msg), "sys_http_%s needs 1-2 values (url, body)", method);
        parser_error(parser, msg);
        return 0;
    }
    const char *url = NULL;
    if (!arg_string(parser, args, 0, "sys_http_request", &url)) return 0;
    const char *body = NULL;
    if (args->count == 2) {
        body = value_to_string(args->items[1].value);
    }

    char *host = NULL, *port = NULL, *path = NULL;
    int secure = 0;
    if (!parse_http_url(url, &host, &port, &path, &secure)) {
        parser_error(parser, "HTTP request supports only http:// or https:// URLs");
        free(host); free(port); free(path);
        if (args->count == 2) free((char *)body);
        return 0;
    }

    long status = 0;
    char *raw = NULL;
    if (secure) {
#ifdef _WIN32
        raw = https_request_openssl(method, host, port, path, body, &status);
        if (!raw) {
            raw = https_request_winhttp(method, host, port, path, body, &status);
        }
#else
        raw = https_request_openssl(method, host, port, path, body, &status);
#endif
    } else {
        size_t body_len = body ? strlen(body) : 0;
        size_t request_len = strlen(method) + strlen(host) + strlen(path) + 128 + body_len;
        char *request = malloc(request_len);
        if (!request) { fprintf(stderr, "out of memory\n"); exit(2); }
        if (body_len > 0) {
            snprintf(request, request_len,
                "%s %s HTTP/1.0\r\nHost: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",
                method, path, host, body_len, body);
        } else {
            snprintf(request, request_len,
                "%s %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
                method, path, host);
        }
        raw = tcp_exchange(host, port, request);
        free(request);
        if (raw) sscanf(raw, "HTTP/%*s %ld", &status);
    }
    free(host); free(port); free(path);
    if (args->count == 2) free((char *)body);

    if (!raw) {
        *out = make_response_map(0, 0, qs_strdup(""), qs_strdup(""));
        return 1;
    }

    char *body_start = strstr(raw, "\r\n\r\n");
    char *resp_body = qs_strdup(body_start ? body_start + 4 : "");
    *out = make_response_map(status >= 200 && status < 400, status, resp_body, raw);
    return 1;
}

#ifdef _WIN32
static char *https_request_winhttp(const char *method, const char *host, const char *port, const char *path, const char *body, long *status) {
    g_last_http_error = 0;
    wchar_t *host_w = utf8_to_wide(host);
    wchar_t *path_w = utf8_to_wide(path);
    wchar_t *method_w = utf8_to_wide(method);
    if (!host_w || !path_w || !method_w) {
        g_last_http_error = GetLastError();
        free(host_w); free(path_w); free(method_w);
        return NULL;
    }

    long port_number = strtol(port, NULL, 10);
    if (port_number <= 0 || port_number > 65535) {
        g_last_http_error = 87;
        free(host_w); free(path_w); free(method_w);
        return NULL;
    }

    HINTERNET session = WinHttpOpen(L"QuantoScript/1.0.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                   WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        g_last_http_error = GetLastError();
        free(host_w); free(path_w); free(method_w);
        return NULL;
    }
    WinHttpSetTimeouts(session, 1500, 1500, 3000, 3000);

    HINTERNET connection = WinHttpConnect(session, host_w, (INTERNET_PORT)port_number, 0);
    if (!connection) {
        g_last_http_error = GetLastError();
        WinHttpCloseHandle(session);
        free(host_w); free(path_w); free(method_w);
        return NULL;
    }

    HINTERNET request = WinHttpOpenRequest(connection, method_w, path_w, NULL, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    free(host_w); free(path_w); free(method_w);
    if (!request) {
        g_last_http_error = GetLastError();
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return NULL;
    }
    WinHttpSetOption(request, WINHTTP_OPTION_CLIENT_CERT_CONTEXT, WINHTTP_NO_CLIENT_CERT_CONTEXT, 0);

    LPVOID body_data = WINHTTP_NO_REQUEST_DATA;
    DWORD body_len = 0;
    if (body && body[0]) {
        body_data = (LPVOID)body;
        body_len = (DWORD)strlen(body);
    }

    BOOL ok = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                 body_data, body_len, body_len, 0) &&
              WinHttpReceiveResponse(request, NULL);
    if (!ok) {
        g_last_http_error = GetLastError();
        if (g_last_http_error == ERROR_WINHTTP_SECURE_FAILURE) {
            DWORD security_flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                                   SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                                   SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                                   SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
            WinHttpSetOption(request, WINHTTP_OPTION_SECURITY_FLAGS, &security_flags, sizeof(security_flags));
            ok = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    body_data, body_len, body_len, 0) &&
                 WinHttpReceiveResponse(request, NULL);
        }
        if (!ok) {
            g_last_http_error = GetLastError();
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return NULL;
        }
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size,
                            WINHTTP_NO_HEADER_INDEX)) {
        *status = (long)status_code;
    } else {
        *status = 0;
    }

    DWORD header_size = 0;
    WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
                        NULL, &header_size, WINHTTP_NO_HEADER_INDEX);
    wchar_t *headers_w = NULL;
    if (header_size > 0) {
        headers_w = (wchar_t *)malloc(header_size * sizeof(wchar_t));
        if (headers_w) {
            WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
                                headers_w, &header_size, WINHTTP_NO_HEADER_INDEX);
        }
    }
    char *headers = qs_strdup("");
    if (headers_w) {
        free(headers);
        headers = wide_to_utf8(headers_w);
        free(headers_w);
    }

    size_t body_capacity = 4096;
    size_t body_count = 0;
    char *response_body = malloc(body_capacity + 1);
    if (!response_body) { fprintf(stderr, "out of memory\n"); exit(2); }

    while (1) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available) || available == 0) break;
        if (body_count + available > body_capacity) {
            while (body_count + available > body_capacity) body_capacity *= 2;
            char *next = realloc(response_body, body_capacity + 1);
            if (!next) { fprintf(stderr, "out of memory\n"); exit(2); }
            response_body = next;
        }
        DWORD read = 0;
        if (!WinHttpReadData(request, response_body + body_count, available, &read) || read == 0) break;
        body_count += read;
    }
    response_body[body_count] = '\0';

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    size_t raw_len = strlen(headers) + 4 + strlen(response_body);
    char *raw = malloc(raw_len + 1);
    if (!raw) { fprintf(stderr, "out of memory\n"); exit(2); }
    raw[0] = '\0';
    strcat(raw, headers);
    strcat(raw, "\r\n");
    strcat(raw, response_body);
    free(headers);
    free(response_body);
    return raw;
}
#endif
static int builtin_http_get(Parser *parser, CallArgList *args, Value *out) {
    return builtin_http_request(parser, "GET", args, out);
}

static int builtin_http_post(Parser *parser, CallArgList *args, Value *out) {
    return builtin_http_request(parser, "POST", args, out);
}

static int builtin_http_put(Parser *parser, CallArgList *args, Value *out) {
    return builtin_http_request(parser, "PUT", args, out);
}

static int builtin_http_delete(Parser *parser, CallArgList *args, Value *out) {
    return builtin_http_request(parser, "DELETE", args, out);
}

static int builtin_http_patch(Parser *parser, CallArgList *args, Value *out) {
    return builtin_http_request(parser, "PATCH", args, out);
}

static int sandbox_check_path(const char *path) {
    if (!g_sandbox_mode) return 1;
    if (g_sandbox_path[0] == '\0') return 1;
    return strncmp(path, g_sandbox_path, strlen(g_sandbox_path)) == 0;
}

static int args_need(Parser *parser, CallArgList *args, size_t count, const char *name) {
    if (args->count == count) {
        return 1;
    }
    char msg[128];
    snprintf(msg, sizeof(msg), "%s needs %ld value%s", name, (long)count, count == 1 ? "" : "s");
    parser_error(parser, msg);
    return 0;
}

static int arg_string(Parser *parser, CallArgList *args, size_t index, const char *name, const char **out) {
    if (args->items[index].value.type != VALUE_STRING) {
        char msg[128];
        snprintf(msg, sizeof(msg), "%s argument %ld must be a string", name, (long)index + 1);
        parser_error(parser, msg);
        return 0;
    }
    *out = args->items[index].value.as.string;
    return 1;
}

static int arg_integer(Parser *parser, CallArgList *args, size_t index, const char *name, long *out) {
    if (args->items[index].value.type != VALUE_INTEGER) {
        char msg[128];
        snprintf(msg, sizeof(msg), "%s argument %ld must be a number", name, (long)index + 1);
        parser_error(parser, msg);
        return 0;
    }
    *out = args->items[index].value.as.integer;
    return 1;
}

static int parse_http_url(const char *url, char **host, char **port, char **path, int *secure) {
    const char *prefix = NULL;
    const char *default_port = NULL;
    if (strncmp(url, "http://", 7) == 0) {
        prefix = "http://"; default_port = "80"; *secure = 0;
    } else if (strncmp(url, "https://", 8) == 0) {
        prefix = "https://"; default_port = "443"; *secure = 1;
    } else if (strncmp(url, "ws://", 5) == 0) {
        prefix = "ws://"; default_port = "80"; *secure = 0;
    } else if (strncmp(url, "wss://", 6) == 0) {
        prefix = "wss://"; default_port = "443"; *secure = 1;
    } else {
        return 0;
    }
    size_t prefix_len = strlen(prefix);
    const char *start = url + prefix_len;
    const char *slash = strchr(start, '/');
    const char *host_end = slash ? slash : url + strlen(url);
    const char *colon = NULL;
    for (const char *p = start; p < host_end; p++) {
        if (*p == ':') { colon = p; break; }
    }
    if (colon) {
        *host = qs_strndup(start, (size_t)(colon - start));
        *port = qs_strndup(colon + 1, (size_t)(host_end - colon - 1));
    } else {
        *host = qs_strndup(start, (size_t)(host_end - start));
        *port = qs_strdup(default_port);
    }
    *path = qs_strdup(slash ? slash : "/");
    return (*host)[0] != '\0' && (*port)[0] != '\0';
}

static int socket_startup(void) {
#ifdef _WIN32
    static int started = 0;
    if (!started) {
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 0;
        started = 1;
    }
#endif
    return 1;
}

static void set_socket_timeout(qs_socket_t sock, int milliseconds) {
#ifdef _WIN32
    DWORD timeout = (DWORD)milliseconds;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));
#else
    struct timeval timeout;
    timeout.tv_sec = milliseconds / 1000;
    timeout.tv_usec = (milliseconds % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
}

static int connect_with_timeout(qs_socket_t sock, const struct sockaddr *addr, socklen_t addrlen, int milliseconds) {
#ifdef _WIN32
    u_long nonblocking = 1;
    ioctlsocket(sock, FIONBIO, &nonblocking);
    int result = connect(sock, addr, (int)addrlen);
    if (result == 0) {
        nonblocking = 0;
        ioctlsocket(sock, FIONBIO, &nonblocking);
        return 1;
    }
    int err = WSAGetLastError();
    if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS && err != WSAEINVAL) {
        nonblocking = 0;
        ioctlsocket(sock, FIONBIO, &nonblocking);
        return 0;
    }
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    int result = connect(sock, addr, addrlen);
    if (result == 0) {
        fcntl(sock, F_SETFL, flags);
        return 1;
    }
    if (errno != EINPROGRESS) {
        fcntl(sock, F_SETFL, flags);
        return 0;
    }
#endif

    fd_set write_set;
    FD_ZERO(&write_set);
    FD_SET(sock, &write_set);
    struct timeval timeout;
    timeout.tv_sec = milliseconds / 1000;
    timeout.tv_usec = (milliseconds % 1000) * 1000;
    result = select((int)sock + 1, NULL, &write_set, NULL, &timeout);
    if (result <= 0) {
#ifdef _WIN32
        nonblocking = 0;
        ioctlsocket(sock, FIONBIO, &nonblocking);
#else
        fcntl(sock, F_SETFL, flags);
#endif
        return 0;
    }
    int socket_error = 0;
    socklen_t socket_error_len = sizeof(socket_error);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&socket_error, &socket_error_len);
#ifdef _WIN32
    nonblocking = 0;
    ioctlsocket(sock, FIONBIO, &nonblocking);
#else
    fcntl(sock, F_SETFL, flags);
#endif
    return socket_error == 0;
}

static qs_socket_t tcp_connect_socket(const char *host, const char *port) {
    if (!socket_startup()) return QS_INVALID_SOCKET;
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *result = NULL;
    if (getaddrinfo(host, port, &hints, &result) != 0) return QS_INVALID_SOCKET;
    qs_socket_t sock = QS_INVALID_SOCKET;
    for (struct addrinfo *addr = result; addr; addr = addr->ai_next) {
        sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sock == QS_INVALID_SOCKET) continue;
        if (connect_with_timeout(sock, addr->ai_addr, (socklen_t)addr->ai_addrlen, 3000)) {
            set_socket_timeout(sock, 3000);
            break;
        }
        qs_close_socket(sock);
        sock = QS_INVALID_SOCKET;
    }
    freeaddrinfo(result);
    return sock;
}

static char *tcp_exchange(const char *host, const char *port, const char *message) {
    qs_socket_t sock = tcp_connect_socket(host, port);
    if (sock == QS_INVALID_SOCKET) return NULL;
    size_t sent = 0;
    size_t message_len = strlen(message);
    while (sent < message_len) {
        int chunk = send(sock, message + sent, (int)(message_len - sent), 0);
        if (chunk <= 0) { qs_close_socket(sock); return NULL; }
        sent += (size_t)chunk;
    }
    size_t count = 0;
    size_t capacity = 4096;
    char *response = malloc(capacity + 1);
    if (!response) { fprintf(stderr, "out of memory\n"); exit(2); }
    while (1) {
        if (count + 2048 > capacity) {
            capacity *= 2;
            char *next = realloc(response, capacity + 1);
            if (!next) { fprintf(stderr, "out of memory\n"); exit(2); }
            response = next;
        }
        int got = recv(sock, response + count, 2048, 0);
        if (got <= 0) break;
        count += (size_t)got;
    }
    qs_close_socket(sock);
    response[count] = '\0';
    return response;
}

static char *trim_string_copy(const char *text) {
    const char *start = text;
    while (*start && isspace((unsigned char)*start)) start++;
    const char *end = text + strlen(text);
    while (end > start && isspace((unsigned char)end[-1])) end--;
    return qs_strndup(start, (size_t)(end - start));
}

static Value split_string_value(const char *text, const char *separator) {
    Value value = make_list_empty();
    size_t separator_len = strlen(separator);
    if (separator_len == 0) {
        for (size_t i = 0; text[i]; i++) {
            char single[2] = {text[i], '\0'};
            list_push(&value.as.list, make_string_owned(qs_strdup(single)));
        }
        return value;
    }
    const char *start = text;
    const char *found = NULL;
    while ((found = strstr(start, separator)) != NULL) {
        list_push(&value.as.list, make_string_owned(qs_strndup(start, (size_t)(found - start))));
        start = found + separator_len;
    }
    list_push(&value.as.list, make_string_owned(qs_strdup(start)));
    return value;
}

static int call_builtin(Parser *parser, const char *name, CallArgList *args, Value *out) {
    // Gate sys_* internal functions — only accessible from stdlib module context
    if (strncmp(name, "sys_", 4) == 0 && g_native_access_depth <= 0 && g_module_mode <= 0) {
        return 0;
    }
    if (strcmp(name, "exit") == 0) {
        long code = 0;
        if (args->count > 1) {
            parser_error(parser, "exit needs zero or one number");
            return 0;
        }
        if (args->count == 1 && !arg_integer(parser, args, 0, name, &code)) {
            return 0;
        }
        (void)out;
        exit((int)code);
    }
    if (strcmp(name, "type") == 0) {
        if (!args_need(parser, args, 1, name)) {
            return 0;
        }
        *out = make_string_owned(qs_strdup(value_type_name(args->items[0].value)));
        return 1;
    }
    if (strcmp(name, "abs") == 0) {
        long n = 0;
        if (!args_need(parser, args, 1, name) || !arg_integer(parser, args, 0, name, &n)) {
            return 0;
        }
        *out = make_integer(n < 0 ? -n : n);
        return 1;
    }
    if (strcmp(name, "len") == 0) {
        if (!args_need(parser, args, 1, name)) {
            return 0;
        }
        Value value = args->items[0].value;
        if (value.type == VALUE_STRING) {
            *out = make_integer((long)strlen(value.as.string));
        } else if (value.type == VALUE_LIST) {
            *out = make_integer((long)value.as.list.count);
        } else if (value.type == VALUE_DICT) {
            *out = make_integer((long)value.as.dict.count);
        } else {
            parser_error(parser, "len needs a string, list, or map");
            return 0;
        }
        return 1;
    }
    if (strcmp(name, "isinstance") == 0) {
        if (!args_need(parser, args, 2, name)) {
            return 0;
        }
        if (args->items[1].value.type != VALUE_STRING) {
            parser_error(parser, "isinstance type must be a string");
            return 0;
        }
        *out = make_bool(type_matches(args->items[1].value.as.string, args->items[0].value));
        return 1;
    }
    if (strcmp(name, "str") == 0) {
        if (!args_need(parser, args, 1, name)) {
            return 0;
        }
        *out = make_string_owned(value_to_string(args->items[0].value));
        return 1;
    }
    if (strcmp(name, "range") == 0) {
        if (args->count < 1 || args->count > 3) {
            parser_error(parser, "range needs one, two, or three numbers");
            return 0;
        }
        long start = 1;
        long stop = 0;
        long step = 1;
        if (args->count == 1) {
            if (!arg_integer(parser, args, 0, name, &stop)) {
                return 0;
            }
        } else {
            if (!arg_integer(parser, args, 0, name, &start) ||
                !arg_integer(parser, args, 1, name, &stop)) {
                return 0;
            }
            if (args->count == 3 && !arg_integer(parser, args, 2, name, &step)) {
                return 0;
            }
        }
        if (step == 0) {
            parser_error(parser, "range step cannot be zero");
            return 0;
        }
        Value list = make_list_empty();
        if (step > 0) {
            for (long n = start; n <= stop; n += step) {
                list_push(&list.as.list, make_integer(n));
            }
        } else {
            for (long n = start; n >= stop; n += step) {
                list_push(&list.as.list, make_integer(n));
            }
        }
        *out = list;
        return 1;
    }
    if (strcmp(name, "assert") == 0) {
        if (args->count < 1 || args->count > 2) {
            parser_error(parser, "assert needs one or two values");
            return 0;
        }
        if (!value_truthy(args->items[0].value)) {
            char *msg = "assertion failed";
            if (args->count == 2) {
                msg = value_to_string(args->items[1].value);
            }
            fprintf(stderr, "AssertionError: %s\n", msg);
            if (args->count == 2) {
                free(msg);
            }
            exit(1);
        }
        *out = make_null();
        return 1;
    }
    if (strcmp(name, "pow") == 0) {
        if (args->count != 2) { parser_error(parser, "pow needs 2 args"); return 0; }
        double base = value_to_double(args->items[0].value);
        double exp = value_to_double(args->items[1].value);
        *out = make_float(pow(base, exp));
        return 1;
    }
    if (strcmp(name, "sqrt") == 0) {
        if (args->count != 1) { parser_error(parser, "sqrt needs 1 arg"); return 0; }
        *out = make_float(sqrt(value_to_double(args->items[0].value)));
        return 1;
    }
    if (strcmp(name, "sin") == 0) {
        if (args->count != 1) { parser_error(parser, "sin needs 1 arg"); return 0; }
        *out = make_float(sin(value_to_double(args->items[0].value)));
        return 1;
    }
    if (strcmp(name, "cos") == 0) {
        if (args->count != 1) { parser_error(parser, "cos needs 1 arg"); return 0; }
        *out = make_float(cos(value_to_double(args->items[0].value)));
        return 1;
    }
    if (strcmp(name, "tan") == 0) {
        if (args->count != 1) { parser_error(parser, "tan needs 1 arg"); return 0; }
        *out = make_float(tan(value_to_double(args->items[0].value)));
        return 1;
    }
    if (strcmp(name, "log") == 0) {
        if (args->count != 1) { parser_error(parser, "log needs 1 arg"); return 0; }
        *out = make_float(log(value_to_double(args->items[0].value)));
        return 1;
    }
    if (strcmp(name, "log2") == 0) {
        if (args->count != 1) { parser_error(parser, "log2 needs 1 arg"); return 0; }
        *out = make_float(log2(value_to_double(args->items[0].value)));
        return 1;
    }
    if (strcmp(name, "log10") == 0) {
        if (args->count != 1) { parser_error(parser, "log10 needs 1 arg"); return 0; }
        *out = make_float(log10(value_to_double(args->items[0].value)));
        return 1;
    }
    if (strcmp(name, "ceil") == 0) {
        if (args->count != 1) { parser_error(parser, "ceil needs 1 arg"); return 0; }
        *out = make_float(ceil(value_to_double(args->items[0].value)));
        return 1;
    }
    if (strcmp(name, "floor") == 0) {
        if (args->count != 1) { parser_error(parser, "floor needs 1 arg"); return 0; }
        *out = make_float(floor(value_to_double(args->items[0].value)));
        return 1;
    }
    // ---- PCG32 PRNG (portable, deterministic) ----
    if (strcmp(name, "random") == 0) {
        // PCG32 next double in [0, 1)
        extern uint64_t g_pcg_state;
        extern uint64_t g_pcg_inc;
        uint64_t oldstate = g_pcg_state;
        g_pcg_state = oldstate * 6364136223846793005ULL + g_pcg_inc;
        uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
        uint32_t rot = (uint32_t)(oldstate >> 59u);
        uint32_t rng = (xorshifted >> rot) | (xorshifted << ((-(int)rot) & 31));
        *out = make_float((double)rng / (double)0x100000000ULL);
        return 1;
    }
    if (strcmp(name, "random_seed") == 0) {
        if (args->count != 1) { parser_error(parser, "random_seed needs 1 arg"); return 0; }
        extern uint64_t g_pcg_state;
        extern uint64_t g_pcg_inc;
        uint64_t seed = (uint64_t)args->items[0].value.as.integer;
        g_pcg_state = 0;
        g_pcg_inc = (seed << 1u) | 1u;
        g_pcg_state = g_pcg_state * 6364136223846793005ULL + g_pcg_inc;
        g_pcg_state += seed;
        g_pcg_state = g_pcg_state * 6364136223846793005ULL + g_pcg_inc;
        *out = make_null();
        return 1;
    }
    if (strcmp(name, "sys_pcg_next_u32") == 0) {
        extern uint64_t g_pcg_state;
        extern uint64_t g_pcg_inc;
        uint64_t oldstate = g_pcg_state;
        g_pcg_state = oldstate * 6364136223846793005ULL + g_pcg_inc;
        uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
        uint32_t rot = (uint32_t)(oldstate >> 59u);
        uint32_t rng = (xorshifted >> rot) | (xorshifted << ((-(int)rot) & 31));
        // Use unsigned cast to avoid sign extension on LLP64 (Windows 64-bit where long=32bit)
        *out = make_integer((long)(int32_t)rng);
        return 1;
    }
    // Bounded random: returns min + (rng % span) where span = max - min + 1
    // Computed entirely in C with unsigned arithmetic to avoid signed modulo issues.
    if (strcmp(name, "sys_pcg_u32_range") == 0) {
        if (args->count != 2) { parser_error(parser, "sys_pcg_u32_range needs (min, max)"); return 0; }
        long min_val = args->items[0].value.as.integer;
        long max_val = args->items[1].value.as.integer;
        if (max_val < min_val) {
            parser_error(parser, "range: max must be >= min");
            return 0;
        }
        extern uint64_t g_pcg_state;
        extern uint64_t g_pcg_inc;
        uint64_t oldstate = g_pcg_state;
        g_pcg_state = oldstate * 6364136223846793005ULL + g_pcg_inc;
        uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
        uint32_t rot = (uint32_t)(oldstate >> 59u);
        uint32_t rng = (xorshifted >> rot) | (xorshifted << ((-(int)rot) & 31));
        uint32_t span = (uint32_t)(max_val - min_val + 1);
        uint32_t offset = rng % span;
        *out = make_integer(min_val + (long)offset);
        return 1;
    }
    // Regex: match pattern against string
    if (strcmp(name, "regex_match") == 0) {
        if (args->count != 2) { parser_error(parser, "regex_match needs pattern and string"); return 0; }
        const char *pattern = args->items[0].value.as.string;
        const char *text = args->items[1].value.as.string;
#ifdef _WIN32
        // Simple substring match on Windows (no regex support)
        *out = make_bool(strstr(text, pattern) != NULL);
#else
        regex_t re;
        int ret = regcomp(&re, pattern, REG_EXTENDED);
        if (ret != 0) { parser_error(parser, "invalid regex pattern"); return 0; }
        ret = regexec(&re, text, 0, NULL, 0);
        regfree(&re);
        *out = make_bool(ret == 0);
#endif
        return 1;
    }
    // Regex: find all matches
    if (strcmp(name, "regex_find") == 0) {
        if (args->count != 2) { parser_error(parser, "regex_find needs pattern and string"); return 0; }
        const char *pattern = args->items[0].value.as.string;
        const char *text = args->items[1].value.as.string;
        ValueList matches = {0};
#ifdef _WIN32
        // Simple substring search on Windows
        const char *search = text;
        size_t pat_len = strlen(pattern);
        while ((search = strstr(search, pattern)) != NULL) {
            list_push(&matches, make_string_owned(qs_strndup(search, pat_len)));
            search += pat_len;
        }
#else
        regex_t re;
        int ret = regcomp(&re, pattern, REG_EXTENDED);
        if (ret != 0) { parser_error(parser, "invalid regex pattern"); return 0; }
        regmatch_t match;
        const char *search = text;
        while (regexec(&re, search, 1, &match, 0) == 0) {
            list_push(&matches, make_string_owned(qs_strndup(search + match.rm_so, match.rm_eo - match.rm_so)));
            search += match.rm_eo;
            if (match.rm_eo == 0) break;
        }
        regfree(&re);
#endif
        *out = (Value){.type = VALUE_LIST, .as.list = matches};
        return 1;
    }
    // Regex: replace all matches
    if (strcmp(name, "regex_replace") == 0) {
        if (args->count != 3) { parser_error(parser, "regex_replace needs pattern, replacement, and string"); return 0; }
        const char *pattern = args->items[0].value.as.string;
        const char *replacement = args->items[1].value.as.string;
        const char *text = args->items[2].value.as.string;
        // Use simple string replacement (works on both platforms)
        char *result = NULL;
        size_t text_len = strlen(text);
        size_t pat_len = strlen(pattern);
        size_t rep_len = strlen(replacement);
        size_t cap = text_len + 1;
        result = malloc(cap);
        result[0] = '\0';
        const char *search = text;
        while (1) {
            const char *found = strstr(search, pattern);
            if (!found) { strcat(result, search); break; }
            size_t prefix = (size_t)(found - search);
            if (strlen(result) + prefix + rep_len + 1 > cap) {
                cap = (strlen(result) + prefix + rep_len + 1) * 2;
                result = realloc(result, cap);
            }
            strncat(result, search, prefix);
            strcat(result, replacement);
            search = found + pat_len;
        }
        *out = make_string_owned(result);
        return 1;
    }
    if (strcmp(name, "assert_eq") == 0) {
        if (args->count < 2 || args->count > 3) { parser_error(parser, "assert_eq needs 2-3 args"); return 0; }
        if (!values_equal(args->items[0].value, args->items[1].value)) {
            char *a = value_to_string(args->items[0].value);
            char *b = value_to_string(args->items[1].value);
            if (args->count == 3) {
                char *msg = value_to_string(args->items[2].value);
                fprintf(stderr, "FAILED: %s\n  expected: %s\n  got:      %s\n", msg, a, b);
                free(msg);
            } else {
                fprintf(stderr, "FAILED: expected %s, got %s\n", a, b);
            }
            free(a);
            free(b);
            exit(1);
        }
        printf(".");
        *out = make_null();
        return 1;
    }
    if (strcmp(name, "assert_ne") == 0) {
        if (args->count < 2 || args->count > 3) { parser_error(parser, "assert_ne needs 2-3 args"); return 0; }
        if (values_equal(args->items[0].value, args->items[1].value)) {
            char *a = value_to_string(args->items[0].value);
            if (args->count == 3) {
                char *msg = value_to_string(args->items[2].value);
                fprintf(stderr, "FAILED: %s\n  values are equal: %s\n", msg, a);
                free(msg);
            } else {
                fprintf(stderr, "FAILED: values are equal: %s\n", a);
            }
            free(a);
            exit(1);
        }
        printf(".");
        *out = make_null();
        return 1;
    }
    // WebSocket: Connect to a WebSocket server
    if (strcmp(name, "ws_connect") == 0) {
        if (args->count != 1) { parser_error(parser, "ws_connect needs a URL"); return 0; }
        const char *url = NULL;
        if (!arg_string(parser, args, 0, name, &url)) return 0;

        // Parse URL
        char *host = NULL, *port = NULL, *path = NULL;
        int secure = 0;
        if (!parse_http_url(url, &host, &port, &path, &secure)) {
            parser_error(parser, "ws_connect needs http:// or https:// URL");
            free(host); free(port); free(path);
            return 0;
        }

        // Generate WebSocket key (16 random bytes, base64-encoded per RFC 6455)
        unsigned char key_raw[16];
        srand((unsigned int)time(NULL));
        for (int i = 0; i < 16; i++) {
            key_raw[i] = (unsigned char)(rand() & 0xFF);
        }
        static const char b64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        char key[28];
        int k = 0;
        for (int i = 0; i < 16; i += 3) {
            unsigned int val = ((unsigned int)key_raw[i] << 16);
            if (i + 1 < 16) val |= ((unsigned int)key_raw[i+1] << 8);
            if (i + 2 < 16) val |= ((unsigned int)key_raw[i+2]);
            key[k++] = b64chars[(val >> 18) & 0x3F];
            key[k++] = b64chars[(val >> 12) & 0x3F];
            key[k++] = (i + 1 < 16) ? b64chars[(val >> 6) & 0x3F] : '=';
            key[k++] = (i + 2 < 16) ? b64chars[val & 0x3F] : '=';
        }
        key[k] = '\0';

        // Connect to server
        qs_socket_t sock = tcp_connect_socket(host, port);
        if (sock == QS_INVALID_SOCKET) {
            free(host); free(port); free(path);
            *out = make_null();
            return 1;
        }

        // Build upgrade request
        size_t req_len = strlen(host) + strlen(path) + strlen(key) + 256;
        char *request = malloc(req_len);
        snprintf(request, req_len,
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: %s\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n",
            path, host, key);

        // Send upgrade request
        size_t sent = 0;
        size_t request_size = strlen(request);
        while (sent < request_size) {
            int chunk = send(sock, request + sent, (int)(request_size - sent), 0);
            if (chunk <= 0) {
                free(request);
                free(host); free(port); free(path);
                qs_close_socket(sock);
                *out = make_null();
                return 1;
            }
            sent += chunk;
        }
        free(request);

        // Read response
        char response_buf[4096];
        int response_len = recv(sock, response_buf, sizeof(response_buf) - 1, 0);
        if (response_len <= 0) {
            free(host); free(port); free(path);
            qs_close_socket(sock);
            *out = make_null();
            return 1;
        }
        response_buf[response_len] = '\0';

        // Check for 101 Switching Protocols
        if (strstr(response_buf, "101") == NULL) {
            free(host); free(port); free(path);
            qs_close_socket(sock);
            *out = make_null();
            return 1;
        }

        free(host); free(port); free(path);
        *out = make_float((double)(intptr_t)sock);
        return 1;
    }
    // WebSocket: Send a text message
    if (strcmp(name, "ws_send") == 0) {
        if (args->count != 2) { parser_error(parser, "ws_send needs socket and message"); return 0; }
        qs_socket_t sock = (qs_socket_t)(intptr_t)args->items[0].value.as.floating;
        const char *msg = args->items[1].value.as.string;
        size_t msg_len = strlen(msg);

        // Generate mask key (RFC 6455: client frames MUST be masked)
        unsigned char mask[4];
        for (int i = 0; i < 4; i++) mask[i] = (unsigned char)(rand() & 0xFF);

        // Build WebSocket frame (text, FIN set, masked)
        size_t frame_len = 2 + 4 + (msg_len > 125 ? (msg_len > 65535 ? 8 : 2) : 0) + msg_len;
        char *frame = malloc(frame_len);
        frame[0] = (char)0x81; // FIN + TEXT opcode

        if (msg_len <= 125) {
            frame[1] = (char)(0x80 | msg_len); // MASK bit set
            frame[2] = mask[0]; frame[3] = mask[1]; frame[4] = mask[2]; frame[5] = mask[3];
            for (size_t i = 0; i < msg_len; i++)
                frame[6 + i] = msg[i] ^ mask[i % 4];
        } else if (msg_len <= 65535) {
            frame[1] = (char)(0x80 | 126);
            frame[2] = (msg_len >> 8) & 0xFF;
            frame[3] = msg_len & 0xFF;
            frame[4] = mask[0]; frame[5] = mask[1]; frame[6] = mask[2]; frame[7] = mask[3];
            for (size_t i = 0; i < msg_len; i++)
                frame[8 + i] = msg[i] ^ mask[i % 4];
        } else {
            frame[1] = (char)(0x80 | 127);
            for (int i = 0; i < 8; i++)
                frame[2 + i] = (msg_len >> ((7 - i) * 8)) & 0xFF;
            frame[10] = mask[0]; frame[11] = mask[1]; frame[12] = mask[2]; frame[13] = mask[3];
            for (size_t i = 0; i < msg_len; i++)
                frame[14 + i] = msg[i] ^ mask[i % 4];
        }

        size_t sent = 0;
        while (sent < frame_len) {
            int chunk = send(sock, frame + sent, (int)(frame_len - sent), 0);
            if (chunk <= 0) { free(frame); *out = make_bool(0); return 1; }
            sent += chunk;
        }
        free(frame);
        *out = make_bool(1);
        return 1;
    }
    // WebSocket: Receive a text message
    if (strcmp(name, "ws_recv") == 0) {
        if (args->count != 1) { parser_error(parser, "ws_recv needs a socket"); return 0; }
        qs_socket_t sock = (qs_socket_t)(intptr_t)args->items[0].value.as.floating;

        // Read frame header
        unsigned char header[2];
        int got = recv(sock, (char *)header, 2, 0);
        if (got <= 0) { *out = make_null(); return 1; }

        int opcode = header[0] & 0x0F;
        int masked = header[1] & 0x80;
        uint64_t payload_len = header[1] & 0x7F;

        if (payload_len == 126) {
            unsigned char ext[2];
            recv(sock, (char *)ext, 2, 0);
            payload_len = (ext[0] << 8) | ext[1];
        } else if (payload_len == 127) {
            unsigned char ext[8];
            recv(sock, (char *)ext, 8, 0);
            payload_len = 0;
            for (int i = 0; i < 8; i++) {
                payload_len = (payload_len << 8) | ext[i];
            }
        }

        // Read mask key if present
        unsigned char mask_key[4] = {0};
        if (masked) {
            recv(sock, (char *)mask_key, 4, 0);
        }

        // Read payload
        char *payload = malloc(payload_len + 1);
        size_t received = 0;
        while (received < payload_len) {
            int r = recv(sock, payload + received, (int)(payload_len - received), 0);
            if (r <= 0) { free(payload); *out = make_null(); return 1; }
            received += r;
        }

        // Unmask if needed
        if (masked) {
            for (uint64_t i = 0; i < payload_len; i++) {
                payload[i] ^= mask_key[i % 4];
            }
        }
        payload[payload_len] = '\0';

        // Handle ping/pong
        if (opcode == 9) { // Ping
            // Send pong
            char pong[2] = {(char)0x8A, 0x00};
            send(sock, pong, 2, 0);
            free(payload);
            *out = make_null();
            return 1;
        }
        if (opcode == 8) { // Close
            free(payload);
            *out = make_null();
            return 1;
        }

        *out = make_string_owned(payload);
        return 1;
    }
    // WebSocket: Close connection
    if (strcmp(name, "ws_close") == 0) {
        if (args->count != 1) { parser_error(parser, "ws_close needs a socket"); return 0; }
        qs_socket_t sock = (qs_socket_t)(intptr_t)args->items[0].value.as.floating;
        // Send masked close frame (RFC 6455: client frames must be masked)
        unsigned char mask[4];
        for (int i = 0; i < 4; i++) mask[i] = (unsigned char)(rand() & 0xFF);
        char close_frame[6] = {(char)0x88, (char)0x80, (char)mask[0], (char)mask[1], (char)mask[2], (char)mask[3]};
        send(sock, close_frame, 6, 0);
        qs_close_socket(sock);
        *out = make_null();
        return 1;
    }
    // WebSocket: Get connection state
    if (strcmp(name, "ws_state") == 0) {
        if (args->count != 1) { parser_error(parser, "ws_state needs a socket"); return 0; }
        qs_socket_t sock = (qs_socket_t)(intptr_t)args->items[0].value.as.floating;
        if (sock == QS_INVALID_SOCKET || sock == 0) {
            *out = make_string_owned(qs_strdup("closed"));
        } else {
            // Try a zero-length peek to check if connection is still alive
            char peek_buf[1];
            int peek_result = recv(sock, peek_buf, 1, MSG_PEEK);
            if (peek_result == 0) {
                *out = make_string_owned(qs_strdup("closed"));
            } else if (peek_result < 0) {
                int err = qs_socket_errno();
                if (err == QS_WOULDBLOCK || err == QS_IN_PROGRESS || err == QS_TIMEDOUT) {
                    *out = make_string_owned(qs_strdup("open"));
                } else {
                    // Any other error means the connection is dead
                    *out = make_string_owned(qs_strdup("closed"));
                }
            } else {
                *out = make_string_owned(qs_strdup("open"));
            }
        }
        return 1;
    }
    // WebSocket: Set receive timeout in milliseconds
    if (strcmp(name, "ws_set_timeout") == 0) {
        if (args->count != 2) { parser_error(parser, "ws_set_timeout needs socket and ms"); return 0; }
        qs_socket_t sock = (qs_socket_t)(intptr_t)args->items[0].value.as.floating;
        int ms = (int)args->items[1].value.as.integer;
#ifdef _WIN32
        DWORD timeout_ms = (DWORD)ms;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
#else
        struct timeval tv;
        tv.tv_sec = ms / 1000;
        tv.tv_usec = (ms % 1000) * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
        *out = make_null();
        return 1;
    }
#ifdef QS_PYTHON
    // Python: initialize interpreter
    if (strcmp(name, "python_init") == 0) {
        if (!Py_IsInitialized()) {
            // Set PYTHONHOME to suppress platform warning
            if (!getenv("PYTHONHOME")) {
#ifdef _WIN32
                // Try common Windows Python locations
                const char *locations[] = {
                    "C:\\Python311",
                    "C:\\Python310",
                    "C:\\Python39",
                    NULL
                };
                for (int i = 0; locations[i]; i++) {
                    char check[256];
                    snprintf(check, sizeof(check), "%s\\python.exe", locations[i]);
                    FILE *f = fopen(check, "r");
                    if (f) {
                        fclose(f);
                        SetEnvironmentVariableA("PYTHONHOME", locations[i]);
                        break;
                    }
                }
#else
                setenv("PYTHONHOME", "/usr", 1);
#endif
            }
            Py_Initialize();
        }
        *out = make_bool(1);
        return 1;
    }
    // Python: finalize interpreter
    if (strcmp(name, "python_finalize") == 0) {
        if (Py_IsInitialized()) {
            Py_Finalize();
        }
        *out = make_bool(1);
        return 1;
    }
    // Python: evaluate code string
    if (strcmp(name, "python_eval") == 0) {
        if (args->count != 1) { parser_error(parser, "python_eval needs code string"); return 0; }
        if (!Py_IsInitialized()) { parser_error(parser, "python not initialized, call python_init() first"); return 0; }
        const char *code = args->items[0].value.as.string;
        PyObject *main_dict = PyModule_GetDict(PyImport_AddModule("__main__"));
        // Try eval first, fall back to exec
        PyObject *result = PyRun_String(code, Py_eval_input, main_dict, main_dict);
        if (!result) {
            PyErr_Clear();
            result = PyRun_String(code, Py_file_input, main_dict, main_dict);
        }
        if (!result) {
            PyErr_Print();
            *out = make_null();
            return 1;
        }
        // Convert Python result to QuantoScript value
        if (result == Py_None) {
            *out = make_null();
        } else if (PyLong_Check(result)) {
            *out = make_integer(PyLong_AsLong(result));
        } else if (PyFloat_Check(result)) {
            *out = make_float(PyFloat_AsDouble(result));
        } else if (PyUnicode_Check(result)) {
            const char *str = PyUnicode_AsUTF8(result);
            *out = make_string_owned(qs_strdup(str));
        } else if (PyBool_Check(result)) {
            *out = make_bool(result == Py_True);
        } else if (PyList_Check(result)) {
            ValueList list = {0};
            Py_ssize_t len = PyList_Size(result);
            for (Py_ssize_t i = 0; i < len; i++) {
                PyObject *item = PyList_GetItem(result, i);
                if (PyLong_Check(item)) {
                    list_push(&list, make_integer(PyLong_AsLong(item)));
                } else if (PyFloat_Check(item)) {
                    list_push(&list, make_float(PyFloat_AsDouble(item)));
                } else if (PyUnicode_Check(item)) {
                    list_push(&list, make_string_owned(qs_strdup(PyUnicode_AsUTF8(item))));
                } else {
                    list_push(&list, make_null());
                }
            }
            *out = (Value){.type = VALUE_LIST, .as.list = list};
        } else if (PyDict_Check(result)) {
            Value dict = make_dict_empty();
            PyObject *key, *value;
            Py_ssize_t pos = 0;
            while (PyDict_Next(result, &pos, &key, &value)) {
                const char *key_str = PyUnicode_AsUTF8(key);
                if (PyLong_Check(value)) {
                    dict_set(&dict.as.dict, key_str, make_integer(PyLong_AsLong(value)));
                } else if (PyFloat_Check(value)) {
                    dict_set(&dict.as.dict, key_str, make_float(PyFloat_AsDouble(value)));
                } else if (PyUnicode_Check(value)) {
                    dict_set(&dict.as.dict, key_str, make_string_owned(qs_strdup(PyUnicode_AsUTF8(value))));
                } else {
                    dict_set(&dict.as.dict, key_str, make_null());
                }
            }
            *out = dict;
        } else {
            PyObject *repr = PyObject_Repr(result);
            const char *str = PyUnicode_AsUTF8(repr);
            *out = make_string_owned(qs_strdup(str));
            Py_DECREF(repr);
        }
        Py_DECREF(result);
        return 1;
    }
    // Python: call a function
    if (strcmp(name, "python_call") == 0) {
        if (args->count < 1) { parser_error(parser, "python_call needs function name and args"); return 0; }
        if (!Py_IsInitialized()) { parser_error(parser, "python not initialized"); return 0; }
        const char *func_name = args->items[0].value.as.string;
        PyObject *main_dict = PyModule_GetDict(PyImport_AddModule("__main__"));
        PyObject *func = PyDict_GetItemString(main_dict, func_name);
        if (!func || !PyCallable_Check(func)) {
            parser_error(parser, "not a callable Python function");
            return 0;
        }
        PyObject *pargs = PyTuple_New(args->count - 1);
        for (size_t i = 1; i < args->count; i++) {
            PyObject *pobj = NULL;
            if (args->items[i].value.type == VALUE_INTEGER) {
                pobj = PyLong_FromLong(args->items[i].value.as.integer);
            } else if (args->items[i].value.type == VALUE_FLOAT) {
                pobj = PyFloat_FromDouble(args->items[i].value.as.floating);
            } else if (args->items[i].value.type == VALUE_STRING) {
                pobj = PyUnicode_FromString(args->items[i].value.as.string);
            } else if (args->items[i].value.type == VALUE_NULL) {
                Py_INCREF(Py_None);
                pobj = Py_None;
            } else {
                pobj = Py_None;
            }
            PyTuple_SetItem(pargs, i - 1, pobj);
        }
        PyObject *result = PyObject_CallObject(func, pargs);
        Py_DECREF(pargs);
        if (!result) {
            PyErr_Print();
            *out = make_null();
            return 1;
        }
        // Convert result
        if (result == Py_None) {
            *out = make_null();
        } else if (PyLong_Check(result)) {
            *out = make_integer(PyLong_AsLong(result));
        } else if (PyFloat_Check(result)) {
            *out = make_float(PyFloat_AsDouble(result));
        } else if (PyUnicode_Check(result)) {
            *out = make_string_owned(qs_strdup(PyUnicode_AsUTF8(result)));
        } else {
            *out = make_null();
        }
        Py_DECREF(result);
        return 1;
    }
    // Python: import module
    if (strcmp(name, "python_import") == 0) {
        if (args->count != 1) { parser_error(parser, "python_import needs module name"); return 0; }
        if (!Py_IsInitialized()) { parser_error(parser, "python not initialized"); return 0; }
        const char *module_name = args->items[0].value.as.string;
        PyObject *module = PyImport_ImportModule(module_name);
        if (!module) {
            PyErr_Print();
            *out = make_null();
            return 1;
        }
        // Store module in main dict for later access
        PyObject *main_dict = PyModule_GetDict(PyImport_AddModule("__main__"));
        PyDict_SetItemString(main_dict, module_name, module);
        Py_DECREF(module);
        *out = make_bool(1);
        return 1;
    }
    // Python: get attribute from module
    if (strcmp(name, "python_get") == 0) {
        if (args->count != 2) { parser_error(parser, "python_get needs module and attribute"); return 0; }
        if (!Py_IsInitialized()) { parser_error(parser, "python not initialized"); return 0; }
        const char *module_name = args->items[0].value.as.string;
        const char *attr_name = args->items[1].value.as.string;
        PyObject *main_dict = PyModule_GetDict(PyImport_AddModule("__main__"));
        PyObject *module = PyDict_GetItemString(main_dict, module_name);
        if (!module) {
            *out = make_null();
            return 1;
        }
        PyObject *attr = PyObject_GetAttrString(module, attr_name);
        if (!attr) {
            *out = make_null();
            return 1;
        }
        if (attr == Py_None) {
            *out = make_null();
        } else if (PyLong_Check(attr)) {
            *out = make_integer(PyLong_AsLong(attr));
        } else if (PyFloat_Check(attr)) {
            *out = make_float(PyFloat_AsDouble(attr));
        } else if (PyUnicode_Check(attr)) {
            *out = make_string_owned(qs_strdup(PyUnicode_AsUTF8(attr)));
        } else {
            *out = make_null();
        }
        Py_DECREF(attr);
        return 1;
    }
#endif
    // Thread: spawn a function in a new OS thread
    if (strcmp(name, "thread_spawn") == 0) {
        if (args->count != 1) { parser_error(parser, "thread_spawn needs a function"); return 0; }
        if (args->items[0].value.type != VALUE_LAMBDA) {
            parser_error(parser, "thread_spawn needs a lambda");
            return 0;
        }
        // Execute the lambda synchronously (full threading requires interpreter rewrite)
        CallArgList empty_args = {0};
        Value result;
        if (!call_lambda(parser, &args->items[0].value, &empty_args, &result)) {
            return 0;
        }
        *out = make_integer(1);
        free_value(result);
        return 1;
    }
    // Async: create a new task
    if (strcmp(name, "spawn") == 0 || strcmp(name, "sys_spawn") == 0) {
        if (args->count != 1) { parser_error(parser, "spawn needs a function"); return 0; }
        if (args->items[0].value.type != VALUE_LAMBDA) {
            parser_error(parser, "spawn needs a lambda");
            return 0;
        }
        int task_id = task_create(args->items[0].value);
        *out = make_integer(task_id);
        return 1;
    }
    // Async: execute a task
    if (strcmp(name, "task_run") == 0 || strcmp(name, "sys_task_run") == 0) {
        if (args->count != 1) { parser_error(parser, "task_run needs a task id"); return 0; }
        int task_id = (int)args->items[0].value.as.integer;
        Task *task = task_find(task_id);
        if (!task) { *out = make_bool(0); return 1; }
        if (task->status != TASK_READY) { *out = make_bool(0); return 1; }

        task->status = TASK_RUNNING;
        CallArgList empty_args = {0};
        Value result;
        if (!call_lambda(parser, &task->function, &empty_args, &result)) {
            task_set_error(task_id, "task execution failed");
            *out = make_bool(0);
            return 1;
        }
        task_set_result(task_id, result);
        *out = make_bool(1);
        return 1;
    }
    // Async: get task result
    if (strcmp(name, "task_result") == 0 || strcmp(name, "sys_task_result") == 0) {
        if (args->count != 1) { parser_error(parser, "task_result needs a task id"); return 0; }
        int task_id = (int)args->items[0].value.as.integer;
        Task *task = task_find(task_id);
        if (!task || !task->has_result) { *out = make_null(); return 1; }
        *out = clone_value(task->result);
        return 1;
    }
    // Async: check task status
    if (strcmp(name, "task_status") == 0 || strcmp(name, "sys_task_status") == 0) {
        if (args->count != 1) { parser_error(parser, "task_status needs a task id"); return 0; }
        int task_id = (int)args->items[0].value.as.integer;
        Task *task = task_find(task_id);
        if (!task) { *out = make_string_owned(qs_strdup("not_found")); return 1; }
        const char *status_str = "unknown";
        switch (task->status) {
            case TASK_READY: status_str = "ready"; break;
            case TASK_RUNNING: status_str = "running"; break;
            case TASK_WAITING: status_str = "waiting"; break;
            case TASK_DONE: status_str = "done"; break;
            case TASK_ERROR: status_str = "error"; break;
        }
        *out = make_string_owned(qs_strdup(status_str));
        return 1;
    }
    // Async: run all ready tasks cooperatively
    if (strcmp(name, "task_run_all") == 0 || strcmp(name, "sys_task_run_all") == 0) {
        int ran = 0;
        for (size_t i = 0; i < g_task_queue.count; i++) {
            if (g_task_queue.items[i].status == TASK_READY) {
                Task *task = &g_task_queue.items[i];
                task->status = TASK_RUNNING;
                CallArgList empty_args = {0};
                Value result;
                if (call_lambda(parser, &task->function, &empty_args, &result)) {
                    task_set_result(task->id, result);
                    ran++;
                } else {
                    task_set_error(task->id, "task failed");
                }
            }
        }
        *out = make_integer(ran);
        return 1;
    }
    // FFI: Load a shared library
    if (strcmp(name, "ffi_open") == 0) {
        if (args->count != 1) { parser_error(parser, "ffi_open needs a library path"); return 0; }
        const char *lib_path = NULL;
        if (!arg_string(parser, args, 0, name, &lib_path)) return 0;
#ifdef _WIN32
        HMODULE h = LoadLibraryA(lib_path);
        if (!h) { *out = make_null(); return 1; }
        *out = make_float((double)(intptr_t)h);
#else
        void *h = dlopen(lib_path, RTLD_NOW);
        if (!h) { *out = make_null(); return 1; }
        *out = make_float((double)(intptr_t)h);
#endif
        return 1;
    }
    // FFI: Get a function pointer
    if (strcmp(name, "ffi_symbol") == 0) {
        if (args->count != 2) { parser_error(parser, "ffi_symbol needs lib and name"); return 0; }
        intptr_t lib_handle = (intptr_t)args->items[0].value.as.floating;
        const char *func_name = NULL;
        if (!arg_string(parser, args, 1, name, &func_name)) return 0;
#ifdef _WIN32
        void *sym = (void *)(intptr_t)GetProcAddress((HMODULE)(intptr_t)lib_handle, func_name);
#else
        void *sym = dlsym((void *)lib_handle, func_name);
#endif
        if (!sym) { *out = make_null(); return 1; }
        *out = make_float((double)(intptr_t)sym);
        return 1;
    }
    // FFI: Call a C function with int args and int return
    if (strcmp(name, "ffi_call_i") == 0) {
        if (args->count < 1) { parser_error(parser, "ffi_call_i needs func and args"); return 0; }
        intptr_t func_ptr = (intptr_t)args->items[0].value.as.floating;
        typedef long (*ffi_func_i)();
        ffi_func_i fn = (ffi_func_i)func_ptr;
        long a0 = args->count > 1 ? args->items[1].value.as.integer : 0;
        long a1 = args->count > 2 ? args->items[2].value.as.integer : 0;
        long a2 = args->count > 3 ? args->items[3].value.as.integer : 0;
        long a3 = args->count > 4 ? args->items[4].value.as.integer : 0;
        long a4 = args->count > 5 ? args->items[5].value.as.integer : 0;
        long a5 = args->count > 6 ? args->items[6].value.as.integer : 0;
        long result = fn(a0, a1, a2, a3, a4, a5);
        *out = make_integer(result);
        return 1;
    }
    // FFI: Call a C function with float args and float return
    if (strcmp(name, "ffi_call_f") == 0) {
        if (args->count < 1) { parser_error(parser, "ffi_call_f needs func and args"); return 0; }
        intptr_t func_ptr = (intptr_t)args->items[0].value.as.floating;
        typedef double (*ffi_func_f)();
        ffi_func_f fn = (ffi_func_f)func_ptr;
        double a0 = args->count > 1 ? value_to_double(args->items[1].value) : 0;
        double a1 = args->count > 2 ? value_to_double(args->items[2].value) : 0;
        double a2 = args->count > 3 ? value_to_double(args->items[3].value) : 0;
        double a3 = args->count > 4 ? value_to_double(args->items[4].value) : 0;
        double result = fn(a0, a1, a2, a3);
        *out = make_float(result);
        return 1;
    }
    // FFI: Call a C function with string arg and string return
    if (strcmp(name, "ffi_call_s") == 0) {
        if (args->count < 2) { parser_error(parser, "ffi_call_s needs func and string arg"); return 0; }
        intptr_t func_ptr = (intptr_t)args->items[0].value.as.floating;
        const char *str_arg = args->items[1].value.as.string;
        typedef char *(*ffi_func_s)(const char *);
        ffi_func_s fn = (ffi_func_s)func_ptr;
        char *result = fn(str_arg);
        *out = result ? make_string_owned(qs_strdup(result)) : make_null();
        return 1;
    }
    // FFI: Close a library
    if (strcmp(name, "ffi_close") == 0) {
        if (args->count != 1) { parser_error(parser, "ffi_close needs a library handle"); return 0; }
        intptr_t lib_handle = (intptr_t)args->items[0].value.as.floating;
#ifdef _WIN32
        FreeLibrary((HMODULE)lib_handle);
#else
        dlclose((void *)lib_handle);
#endif
        *out = make_null();
        return 1;
    }
    // FFI: Allocate memory
    if (strcmp(name, "ffi_alloc") == 0) {
        if (args->count != 1) { parser_error(parser, "ffi_alloc needs a size"); return 0; }
        long size = args->items[0].value.as.integer;
        void *ptr = malloc(size);
        *out = ptr ? make_float((double)(intptr_t)ptr) : make_null();
        return 1;
    }
    // FFI: Free memory
    if (strcmp(name, "ffi_free") == 0) {
        if (args->count != 1) { parser_error(parser, "ffi_free needs a pointer"); return 0; }
        intptr_t ptr = (intptr_t)args->items[0].value.as.floating;
        free((void *)ptr);
        *out = make_null();
        return 1;
    }
    // FFI: Read string from pointer
    if (strcmp(name, "ffi_read_string") == 0) {
        if (args->count != 1) { parser_error(parser, "ffi_read_string needs a pointer"); return 0; }
        intptr_t ptr = (intptr_t)args->items[0].value.as.floating;
        char *str = (char *)ptr;
        *out = str ? make_string_owned(qs_strdup(str)) : make_null();
        return 1;
    }
    // FFI: Write string to pointer
    if (strcmp(name, "ffi_write_string") == 0) {
        if (args->count != 2) { parser_error(parser, "ffi_write_string needs ptr and string"); return 0; }
        intptr_t ptr = (intptr_t)args->items[0].value.as.floating;
        const char *str = args->items[1].value.as.string;
        char *dest = (char *)ptr;
        strcpy(dest, str);
        *out = make_null();
        return 1;
    }
    if (strcmp(name, "input") == 0) {
        if (args->count > 1) {
            parser_error(parser, "input needs zero or one prompt");
            return 0;
        }
        if (args->count == 1) {
            char *prompt = value_to_string(args->items[0].value);
            fputs(prompt, stdout);
            fflush(stdout);
            free(prompt);
        }
        size_t capacity = 128;
        size_t count = 0;
        char *line = malloc(capacity);
        if (!line) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        int ch = 0;
        while ((ch = getchar()) != EOF && ch != '\n') {
            if (ch == '\r') {
                continue;
            }
            if (count + 1 >= capacity) {
                capacity *= 2;
                char *next = realloc(line, capacity);
                if (!next) {
                    fprintf(stderr, "out of memory\n");
                    exit(2);
                }
                line = next;
            }
            line[count++] = (char)ch;
        }
        line[count] = '\0';
        *out = make_string_owned(line);
        return 1;
    }
    if (strcmp(name, "sys_lower") == 0 || strcmp(name, "sys_upper") == 0 || strcmp(name, "sys_trim") == 0) {
        const char *text = NULL;
        if (!args_need(parser, args, 1, name) || !arg_string(parser, args, 0, name, &text)) {
            return 0;
        }
        if (strcmp(name, "sys_trim") == 0) {
            *out = make_string_owned(trim_string_copy(text));
            return 1;
        }
        char *copy = qs_strdup(text);
        for (size_t i = 0; copy[i]; i++) {
            copy[i] = strcmp(name, "sys_lower") == 0 ?
                      (char)tolower((unsigned char)copy[i]) :
                      (char)toupper((unsigned char)copy[i]);
        }
        *out = make_string_owned(copy);
        return 1;
    }
    if (strcmp(name, "sys_split") == 0) {
        const char *text = NULL;
        const char *separator = NULL;
        if (!args_need(parser, args, 2, name) ||
            !arg_string(parser, args, 0, name, &text) ||
            !arg_string(parser, args, 1, name, &separator)) {
            return 0;
        }
        *out = split_string_value(text, separator);
        return 1;
    }
    if (strcmp(name, "sys_time") == 0) {
        if (!args_need(parser, args, 0, name)) {
            return 0;
        }
        *out = make_integer((long)time(NULL));
        return 1;
    }
    if (strcmp(name, "sys_localtime") == 0) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        Value map = make_dict_empty();
        dict_set(&map.as.dict, "year", make_integer(t->tm_year + 1900));
        dict_set(&map.as.dict, "month", make_integer(t->tm_mon + 1));
        dict_set(&map.as.dict, "day", make_integer(t->tm_mday));
        dict_set(&map.as.dict, "hour", make_integer(t->tm_hour));
        dict_set(&map.as.dict, "min", make_integer(t->tm_min));
        dict_set(&map.as.dict, "sec", make_integer(t->tm_sec));
        dict_set(&map.as.dict, "wday", make_integer(t->tm_wday));
        dict_set(&map.as.dict, "yday", make_integer(t->tm_yday));
        *out = map;
        return 1;
    }
    if (strcmp(name, "sys_format_time") == 0) {
        if (args->count != 2) { parser_error(parser, "sys_format_time needs timestamp and format"); return 0; }
        long ts = args->items[0].value.as.integer;
        const char *fmt = args->items[1].value.as.string;
        time_t t = (time_t)ts;
        struct tm *tm = localtime(&t);
        char buf[256];
        strftime(buf, sizeof(buf), fmt, tm);
        *out = make_string_owned(qs_strdup(buf));
        return 1;
    }
    if (strcmp(name, "sys_parse_time") == 0) {
        if (args->count != 2) { parser_error(parser, "sys_parse_time needs text and format"); return 0; }
        const char *text = args->items[0].value.as.string;
        (void)args->items[1].value.as.string; // fmt parameter reserved for future use
        struct tm tm = {0};
        int year = 0, month = 1, day = 1, hour = 0, min = 0, sec = 0;
        if (sscanf(text, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &min, &sec) >= 1) {
            tm.tm_year = year - 1900;
            tm.tm_mon = month - 1;
            tm.tm_mday = day;
            tm.tm_hour = hour;
            tm.tm_min = min;
            tm.tm_sec = sec;
            *out = make_integer((long)mktime(&tm));
        } else {
            *out = make_integer(0);
        }
        return 1;
    }
    if (strcmp(name, "sys_env") == 0) {
        const char *key = NULL;
        if (!args_need(parser, args, 1, name) || !arg_string(parser, args, 0, name, &key)) {
            return 0;
        }
        char *value = getenv(key);
        *out = value ? make_string_owned(qs_strdup(value)) : make_null();
        return 1;
    }
    if (strcmp(name, "sys_exists") == 0) {
        const char *path = NULL;
        if (!args_need(parser, args, 1, name) || !arg_string(parser, args, 0, name, &path)) {
            return 0;
        }
        FILE *file = fopen(path, "rb");
        if (file) {
            fclose(file);
            *out = make_bool(1);
        } else {
            *out = make_bool(0);
        }
        return 1;
    }
    if (strcmp(name, "sys_read") == 0) {
        const char *path = NULL;
        if (!args_need(parser, args, 1, name) || !arg_string(parser, args, 0, name, &path)) {
            return 0;
        }
        if (!sandbox_check_path(path)) {
            parser_error(parser, "access denied: sandbox mode");
            return 0;
        }
        FILE *file = fopen(path, "rb");
        if (!file) {
            parser_error(parser, "sys_read cannot open file");
            return 0;
        }
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        char *text = malloc((size_t)size + 1);
        if (!text) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        size_t read = fread(text, 1, (size_t)size, file);
        fclose(file);
        text[read] = '\0';
        *out = make_string_owned(text);
        return 1;
    }
    if (strcmp(name, "sys_write") == 0) {
        const char *path = NULL;
        const char *text = NULL;
        if (!args_need(parser, args, 2, name) ||
            !arg_string(parser, args, 0, name, &path) ||
            !arg_string(parser, args, 1, name, &text)) {
            return 0;
        }
        if (!sandbox_check_path(path)) {
            parser_error(parser, "access denied: sandbox mode");
            return 0;
        }
        FILE *file = fopen(path, "wb");
        if (!file) {
            *out = make_bool(0);
            return 1;
        }
        size_t len = strlen(text);
        size_t wrote = fwrite(text, 1, len, file);
        fclose(file);
        *out = make_bool(wrote == len);
        return 1;
    }
    if (strcmp(name, "sys_mkdir") == 0) {
        const char *path = NULL;
        if (!args_need(parser, args, 1, name) || !arg_string(parser, args, 0, name, &path)) return 0;
#ifdef _WIN32
        *out = make_bool(_mkdir(path) == 0);
#else
        *out = make_bool(mkdir(path, 0755) == 0);
#endif
        return 1;
    }
    if (strcmp(name, "sys_remove") == 0) {
        const char *path = NULL;
        if (!args_need(parser, args, 1, name) || !arg_string(parser, args, 0, name, &path)) return 0;
        *out = make_bool(remove(path) == 0);
        return 1;
    }
    if (strcmp(name, "sys_rename") == 0) {
        const char *old_path = NULL;
        const char *new_path = NULL;
        if (!args_need(parser, args, 2, name) ||
            !arg_string(parser, args, 0, name, &old_path) ||
            !arg_string(parser, args, 1, name, &new_path)) return 0;
        *out = make_bool(rename(old_path, new_path) == 0);
        return 1;
    }
    if (strcmp(name, "sys_is_dir") == 0) {
        const char *path = NULL;
        if (!args_need(parser, args, 1, name) || !arg_string(parser, args, 0, name, &path)) return 0;
#ifdef _WIN32
        DWORD attrs = GetFileAttributesA(path);
        *out = make_bool(attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY));
#else
        struct stat st;
        *out = make_bool(stat(path, &st) == 0 && S_ISDIR(st.st_mode));
#endif
        return 1;
    }
    if (strcmp(name, "sys_list_dir") == 0) {
        const char *path = NULL;
        if (!args_need(parser, args, 1, name) || !arg_string(parser, args, 0, name, &path)) return 0;
#ifdef _WIN32
        WIN32_FIND_DATAA find_data;
        char pattern[1024];
        snprintf(pattern, sizeof(pattern), "%s\\*", path);
        HANDLE hFind = FindFirstFileA(pattern, &find_data);
        ValueList list = {0};
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (strcmp(find_data.cFileName, ".") != 0 && strcmp(find_data.cFileName, "..") != 0) {
                    list_push(&list, make_string_owned(qs_strdup(find_data.cFileName)));
                }
            } while (FindNextFileA(hFind, &find_data));
            FindClose(hFind);
        }
#else
        DIR *dir = opendir(path);
        ValueList list = {0};
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                    list_push(&list, make_string_owned(qs_strdup(entry->d_name)));
                }
            }
            closedir(dir);
        }
#endif
        *out = make_list_empty();
        *out = (Value){.type = VALUE_LIST, .as.list = list};
        return 1;
    }
    if (strcmp(name, "sys_tcp_send") == 0) {
        const char *host = NULL;
        const char *message = NULL;
        long port_number = 0;
        if (!args_need(parser, args, 3, name) ||
            !arg_string(parser, args, 0, name, &host) ||
            !arg_integer(parser, args, 1, name, &port_number) ||
            !arg_string(parser, args, 2, name, &message)) {
            return 0;
        }
        char port[32];
        snprintf(port, sizeof(port), "%ld", port_number);
        char *response = tcp_exchange(host, port, message);
        *out = make_string_owned(response ? response : qs_strdup(""));
        return 1;
    }
    if (strcmp(name, "sys_http_get") == 0) {
        return builtin_http_get(parser, args, out);
    }
    if (strcmp(name, "sys_http_post") == 0) {
        return builtin_http_post(parser, args, out);
    }
    if (strcmp(name, "sys_http_put") == 0) {
        return builtin_http_put(parser, args, out);
    }
    if (strcmp(name, "sys_http_delete") == 0) {
        return builtin_http_delete(parser, args, out);
    }
    if (strcmp(name, "sys_http_patch") == 0) {
        return builtin_http_patch(parser, args, out);
    }
    if (strcmp(name, "sys_json_parse") == 0) {
        if (!args_need(parser, args, 1, "sys_json_parse")) return 0;
        const char *text = NULL;
        if (!arg_string(parser, args, 0, "sys_json_parse", &text)) return 0;
        const char *p = text;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '\0') { *out = make_null(); return 1; }
        int pos = 0;
        int err = 0;
        Value result;
        if (!json_parse_value(text, &pos, &result, &err)) {
            parser_error(parser, "json_parse: invalid JSON");
            return 0;
        }
        *out = result;
        return 1;
    }
    if (strcmp(name, "sys_json_stringify") == 0) {
        if (!args_need(parser, args, 1, "sys_json_stringify")) return 0;
        char *json = json_stringify_value(args->items[0].value);
        *out = make_string_owned(json);
        return 1;
    }
    // ---- Process execution ----
    if (strcmp(name, "sys_run") == 0) {
        if (!args_need(parser, args, 1, "sys_run")) return 0;
        const char *cmd = NULL;
        if (!arg_string(parser, args, 0, name, &cmd)) return 0;
        int rc = system(cmd);
        *out = make_integer((long long)rc);
        return 1;
    }
    if (strcmp(name, "sys_capture") == 0) {
        if (!args_need(parser, args, 1, "sys_capture")) return 0;
        const char *cmd = NULL;
        if (!arg_string(parser, args, 0, name, &cmd)) return 0;
        FILE *pipe = popen(cmd, "r");
        if (!pipe) {
            *out = make_null();
            return 1;
        }
        size_t cap = 4096, len = 0;
        char *buf = malloc(cap);
        if (!buf) { pclose(pipe); *out = make_null(); return 1; }
        char tmp[1024];
        while (fgets(tmp, sizeof(tmp), pipe)) {
            size_t tlen = strlen(tmp);
            if (len + tlen + 1 > cap) {
                cap = cap * 2;
                buf = realloc(buf, cap);
                if (!buf) { pclose(pipe); *out = make_null(); return 1; }
            }
            memcpy(buf + len, tmp, tlen);
            len += tlen;
        }
        buf[len] = '\0';
        pclose(pipe);
        // Strip trailing newline
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
            buf[--len] = '\0';
        *out = make_string_owned(buf);
        return 1;
    }
    if (strcmp(name, "sys_cwd") == 0) {
        char buf[4096];
#ifdef _WIN32
        if (_getcwd(buf, sizeof(buf))) {
#else
        if (getcwd(buf, sizeof(buf))) {
#endif
            *out = make_string_owned(qs_strdup(buf));
        } else {
            *out = make_null();
        }
        return 1;
    }
    if (strcmp(name, "sys_chdir") == 0) {
        if (!args_need(parser, args, 1, "sys_chdir")) return 0;
        const char *path = NULL;
        if (!arg_string(parser, args, 0, name, &path)) return 0;
#ifdef _WIN32
        int ok = (_chdir(path) == 0);
#else
        int ok = (chdir(path) == 0);
#endif
        *out = make_bool(ok);
        return 1;
    }
    if (strcmp(name, "sys_exec_exists") == 0) {
        if (!args_need(parser, args, 1, "sys_exec_exists")) return 0;
        const char *cmd = NULL;
        if (!arg_string(parser, args, 0, name, &cmd)) return 0;
        char probe[4096];
#ifdef _WIN32
        snprintf(probe, sizeof(probe), "%s --version >nul 2>&1 || %s -h >nul 2>&1 || %s /? >nul 2>&1", cmd, cmd, cmd);
#else
        snprintf(probe, sizeof(probe), "command -v %s >/dev/null 2>&1", cmd);
#endif
        *out = make_bool(system(probe) == 0);
        return 1;
    }
    return 0;
}

static int is_builtin_function(const char *name) {
    return strcmp(name, "exit") == 0 || strcmp(name, "ever_return") == 0 ||
           strcmp(name, "abs") == 0 || strcmp(name, "len") == 0 ||
           strcmp(name, "type") == 0 || strcmp(name, "isinstance") == 0 ||
           strcmp(name, "str") == 0 || strcmp(name, "range") == 0 ||
           strcmp(name, "assert") == 0 ||
           strcmp(name, "pow") == 0 || strcmp(name, "sqrt") == 0 ||
           strcmp(name, "sin") == 0 || strcmp(name, "cos") == 0 ||
           strcmp(name, "tan") == 0 || strcmp(name, "log") == 0 ||
           strcmp(name, "log2") == 0 || strcmp(name, "log10") == 0 ||
           strcmp(name, "ceil") == 0 || strcmp(name, "floor") == 0 ||
           strcmp(name, "random") == 0 || strcmp(name, "random_seed") == 0 ||
           strcmp(name, "regex_match") == 0 || strcmp(name, "regex_find") == 0 ||
           strcmp(name, "regex_replace") == 0 ||
           strcmp(name, "assert_eq") == 0 || strcmp(name, "assert_ne") == 0 ||
           strcmp(name, "ffi_open") == 0 || strcmp(name, "ffi_symbol") == 0 ||
           strcmp(name, "ffi_call_i") == 0 || strcmp(name, "ffi_call_f") == 0 ||
           strcmp(name, "ffi_call_s") == 0 || strcmp(name, "ffi_close") == 0 ||
           strcmp(name, "ffi_alloc") == 0 || strcmp(name, "ffi_free") == 0 ||
           strcmp(name, "ffi_read_string") == 0 || strcmp(name, "ffi_write_string") == 0 ||
           strcmp(name, "ws_connect") == 0 || strcmp(name, "ws_send") == 0 ||
           strcmp(name, "ws_recv") == 0 || strcmp(name, "ws_close") == 0 ||
           strcmp(name, "ws_state") == 0 || strcmp(name, "ws_set_timeout") == 0 ||
           strcmp(name, "python_init") == 0 || strcmp(name, "python_finalize") == 0 ||
           strcmp(name, "python_eval") == 0 || strcmp(name, "python_call") == 0 ||
           strcmp(name, "python_import") == 0 || strcmp(name, "python_get") == 0 ||
           strcmp(name, "thread_spawn") == 0 ||
           strcmp(name, "spawn") == 0 || strcmp(name, "sys_spawn") == 0 ||
           strcmp(name, "task_run") == 0 || strcmp(name, "sys_task_run") == 0 ||
           strcmp(name, "task_result") == 0 || strcmp(name, "sys_task_result") == 0 ||
           strcmp(name, "task_status") == 0 || strcmp(name, "sys_task_status") == 0 ||
           strcmp(name, "task_run_all") == 0 || strcmp(name, "sys_task_run_all") == 0 ||
           strcmp(name, "input") == 0 || strcmp(name, "sys_time") == 0 ||
           strcmp(name, "sys_localtime") == 0 || strcmp(name, "sys_format_time") == 0 ||
           strcmp(name, "sys_parse_time") == 0 ||
           strcmp(name, "sys_lower") == 0 || strcmp(name, "sys_upper") == 0 ||
           strcmp(name, "sys_trim") == 0 || strcmp(name, "sys_split") == 0 ||
           strcmp(name, "sys_env") == 0 || strcmp(name, "sys_exists") == 0 ||
           strcmp(name, "sys_read") == 0 || strcmp(name, "sys_write") == 0 ||
           strcmp(name, "sys_mkdir") == 0 || strcmp(name, "sys_remove") == 0 ||
           strcmp(name, "sys_rename") == 0 || strcmp(name, "sys_is_dir") == 0 ||
           strcmp(name, "sys_list_dir") == 0 ||
           strcmp(name, "sys_tcp_send") == 0 || strcmp(name, "sys_http_get") == 0 ||
           strcmp(name, "sys_http_post") == 0 || strcmp(name, "sys_http_put") == 0 ||
           strcmp(name, "sys_http_delete") == 0 || strcmp(name, "sys_http_patch") == 0 ||
           strcmp(name, "sys_json_parse") == 0 || strcmp(name, "sys_json_stringify") == 0 ||
           strcmp(name, "sys_run") == 0 || strcmp(name, "sys_capture") == 0 ||
           strcmp(name, "sys_cwd") == 0 || strcmp(name, "sys_chdir") == 0 ||
           strcmp(name, "sys_exec_exists") == 0 ||
           strcmp(name, "sys_pcg_next_u32") == 0 ||
            strcmp(name, "sys_pcg_u32_range") == 0;
}
