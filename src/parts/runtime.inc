#ifdef _WIN32
#include <sys/stat.h>
#include <io.h>
#define R_OK 4
static char *qs_get_exe_path(void) {
    char *buf = malloc(MAX_PATH);
    if (!buf) return NULL;
    DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) { free(buf); return NULL; }
    return buf;
}
static char *qs_dirname_dup(const char *path) {
    char *dup = qs_strdup(path);
    char *last = strrchr(dup, '\\');
    char *fwd = strrchr(dup, '/');
    if (fwd && (!last || fwd > last)) last = fwd;
    if (last) *last = '\0';
    else { free(dup); return qs_strdup("."); }
    return dup;
}
static int qs_dir_exists(const char *path) {
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
}
#elif QS_WASM
/* WebAssembly has no executable path; imports resolve relative to the
 * preopened/working directory. The virtual filesystem still supports stat. */
#include <sys/stat.h>
static char *qs_get_exe_path(void) {
    return NULL;
}
static char *qs_dirname_dup(const char *path) {
    char *dup = qs_strdup(path);
    char *last = strrchr(dup, '/');
    if (last) *last = '\0';
    else { free(dup); return qs_strdup("."); }
    return dup;
}
static int qs_dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}
#else
#if defined(__APPLE__)
#include <mach-o/dyld.h>
static char *qs_get_exe_path(void) {
    uint32_t size = 4096;
    char *buf = malloc(size);
    if (!buf) return NULL;
    if (_NSGetExecutablePath(buf, &size) != 0) {
        char *bigger = realloc(buf, size);
        if (!bigger) { free(buf); return NULL; }
        buf = bigger;
        if (_NSGetExecutablePath(buf, &size) != 0) { free(buf); return NULL; }
    }
    return buf;
}
#else
static char *qs_get_exe_path(void) {
    char *buf = malloc(4096);
    if (!buf) return NULL;
    ssize_t len = readlink("/proc/self/exe", buf, 4095);
    if (len <= 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}
#endif
static char *qs_dirname_dup(const char *path) {
    char *dup = qs_strdup(path);
    char *last = strrchr(dup, '/');
    if (last) *last = '\0';
    else { free(dup); return qs_strdup("."); }
    return dup;
}
static int qs_dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}
#endif

/* Portable "can this file be opened for reading?" probe. Unlike access(2),
 * this works on WASI, where access/stat are unavailable. */
static int qs_file_readable(const char *path) {
#if defined(_WIN32) && !QS_WASM
    return _access(path, 4) == 0;
#elif QS_WASM
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
#else
    return access(path, R_OK) == 0;
#endif
}

#if !QS_WASM
/* Create a directory and any missing parents. Returns 1 on success. */
static int qs_mkdir_p(const char *path) {
    if (!path || !path[0]) return 0;
    char tmp[4096];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return 0;
    memcpy(tmp, path, len + 1);
    while (len > 1 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\')) tmp[--len] = '\0';
    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char sep = tmp[i];
            tmp[i] = '\0';
            if (!qs_dir_exists(tmp)) {
#if defined(_WIN32)
                _mkdir(tmp);
#else
                mkdir(tmp, 0755);
#endif
            }
            tmp[i] = sep;
        }
    }
    if (!qs_dir_exists(tmp)) {
#if defined(_WIN32)
        _mkdir(tmp);
#else
        mkdir(tmp, 0755);
#endif
    }
    return qs_dir_exists(tmp);
}

/* Byte-for-byte copy. Returns 1 on success. */
static int qs_copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return 0;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return 0; }
    char buf[65536];
    size_t n;
    int ok = 1;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
    }
    if (ferror(in)) ok = 0;
    fclose(in);
    if (fclose(out) != 0) ok = 0;
    return ok;
}
#endif

static char *g_quanto_home = NULL;

static const char *get_quanto_home(void) {
    if (g_quanto_home) return g_quanto_home;

    const char *env_home = getenv("QUANTO_HOME");
    if (env_home && env_home[0]) {
        char env_probe[4096];
        snprintf(env_probe, sizeof(env_probe), "%s/stdlib", env_home);
        if (qs_dir_exists(env_probe)) {
            g_quanto_home = qs_strdup(env_home);
            return g_quanto_home;
        }
    }

    char *exe_path = qs_get_exe_path();
    if (exe_path) {
        char *exe_dir = qs_dirname_dup(exe_path);
        free(exe_path);
        char probe[4096];

        snprintf(probe, sizeof(probe), "%s/../share/quantoscript/stdlib", exe_dir);
        if (qs_dir_exists(probe)) {
            snprintf(probe, sizeof(probe), "%s/../share/quantoscript", exe_dir);
            g_quanto_home = qs_strdup(probe);
            free(exe_dir);
            return g_quanto_home;
        }

        snprintf(probe, sizeof(probe), "%s/stdlib", exe_dir);
        if (qs_dir_exists(probe)) {
            g_quanto_home = qs_strdup(exe_dir);
            free(exe_dir);
            return g_quanto_home;
        }

        free(exe_dir);
    }

    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd))) {
        char probe[8192];
        snprintf(probe, sizeof(probe), "%s/stdlib", cwd);
        if (qs_dir_exists(probe)) {
            g_quanto_home = qs_strdup(cwd);
            return g_quanto_home;
        }
    }

    g_quanto_home = qs_strdup("");
    return g_quanto_home;
}

static const char *get_stdlib_dir(void) {
    static char buf[4096];
    const char *home = get_quanto_home();
    if (home[0]) {
        snprintf(buf, sizeof(buf), "%s/stdlib", home);
    } else {
        snprintf(buf, sizeof(buf), "stdlib");
    }
    return buf;
}

/* Locate the prebuilt WebAssembly loader (qs_node.js). It ships under
 * <home>/wasm/ in an installed tree and under build/ or runtime/wasm/ in a
 * source checkout. Returns a static buffer, or an empty string if not found. */
static const char *get_wasm_loader(void) {
    static char buf[4096];
    const char *home = get_quanto_home();
    if (home[0]) {
        snprintf(buf, sizeof(buf), "%s/wasm/qs_node.js", home);
        if (qs_file_readable(buf)) return buf;
    }

    char *exe_path = qs_get_exe_path();
    if (exe_path) {
        char *exe_dir = qs_dirname_dup(exe_path);
        free(exe_path);
        snprintf(buf, sizeof(buf), "%s/wasm/qs_node.js", exe_dir);
        if (qs_file_readable(buf)) { free(exe_dir); return buf; }
        snprintf(buf, sizeof(buf), "%s/qs_node.js", exe_dir);
        if (qs_file_readable(buf)) { free(exe_dir); return buf; }
        /* Installed layout: <prefix>/bin/qs with <prefix>/share/quantoscript/wasm/. */
        snprintf(buf, sizeof(buf), "%s/../share/quantoscript/wasm/qs_node.js", exe_dir);
        if (qs_file_readable(buf)) { free(exe_dir); return buf; }
        free(exe_dir);
    }

    const char *candidates[] = {
        "runtime/wasm/qs_node.js",
        "build/qs_node.js",
        "wasm/qs_node.js",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        if (qs_file_readable(candidates[i])) {
            snprintf(buf, sizeof(buf), "%s", candidates[i]);
            return buf;
        }
    }

    buf[0] = '\0';
    return buf;
}

/* Locate a Node.js executable for `qs wasm`. Honours QS_NODE, then searches
 * PATH (via `where`/`command -v` indirectly: the shell that runs the program
 * resolves bare "node"), and finally a few well-known install locations.
 * Returns a static buffer, or "node" as a last resort. */
static const char *get_node_exe(void) {
    static char buf[4096];
    const char *env_node = getenv("QS_NODE");
    if (env_node && env_node[0]) {
        snprintf(buf, sizeof(buf), "%s", env_node);
        return buf;
    }

#if defined(_WIN32) && !QS_WASM
    const char *home = getenv("USERPROFILE");
    const char *pf = getenv("ProgramFiles");
    const char *localapp = getenv("LOCALAPPDATA");
    const char *candidates[8];
    int n = 0;
    static char c0[2048], c1[2048], c2[2048], c3[2048];
    if (pf) { snprintf(c0, sizeof(c0), "%s\\nodejs\\node.exe", pf); candidates[n++] = c0; }
    if (localapp) { snprintf(c1, sizeof(c1), "%s\\Programs\\nodejs\\node.exe", localapp); candidates[n++] = c1; }
    if (home) { snprintf(c2, sizeof(c2), "%s\\scoop\\apps\\nodejs\\current\\node.exe", home); candidates[n++] = c2; }
    if (pf) { snprintf(c3, sizeof(c3), "%s\\nodejs\\node.exe", pf); candidates[n++] = c3; }
    candidates[n] = NULL;
    for (int i = 0; candidates[i]; i++) {
        if (qs_file_readable(candidates[i])) {
            snprintf(buf, sizeof(buf), "%s", candidates[i]);
            return buf;
        }
    }
#else
    const char *candidates[] = {
        "/usr/local/bin/node",
        "/usr/bin/node",
        "/opt/homebrew/bin/node",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        if (qs_file_readable(candidates[i])) {
            snprintf(buf, sizeof(buf), "%s", candidates[i]);
            return buf;
        }
    }
#endif

    snprintf(buf, sizeof(buf), "node");
    return buf;
}

static char *qs_resolve_import(const char *path, const char *base_dir) {
    if (path[0] == '/' || path[0] == '\\' ||
        (path[0] == '.' && path[1] && (path[1] == '/' || path[1] == '\\'))) {
        return qs_strdup(path);
    }

    char probe[8192];

    if (strncmp(path, "stdlib/", 7) == 0 || strncmp(path, "stdlib\\", 7) == 0) {
        const char *stdlib = get_stdlib_dir();
        snprintf(probe, sizeof(probe), "%s/%s", stdlib, path + 7);
        if (qs_file_readable(probe)) return qs_strdup(probe);
    }

    if (base_dir && base_dir[0]) {
        snprintf(probe, sizeof(probe), "%s/%s", base_dir, path);
        if (qs_file_readable(probe)) return qs_strdup(probe);
    }

    const char *stdlib = get_stdlib_dir();
    if (stdlib[0]) {
        snprintf(probe, sizeof(probe), "%s/%s", stdlib, path);
        if (qs_file_readable(probe)) return qs_strdup(probe);
    }

    const char *home = get_quanto_home();
    if (home[0]) {
        snprintf(probe, sizeof(probe), "%s/%s", home, path);
        if (qs_file_readable(probe)) return qs_strdup(probe);
    }

    return qs_strdup(path);
}
