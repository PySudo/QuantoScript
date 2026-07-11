// QuantoScript Runtime Path Discovery
// Finds stdlib, runtime resources, and installation paths.
// Lookup order:
//   1. QUANTO_HOME environment variable
//   2. Executable-relative location
//   3. Current working directory (development mode)

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
#else
#if defined(__APPLE__)
#include <mach-o/dyld.h>
static char *qs_get_exe_path(void) {
    uint32_t size = 4096;
    char *buf = malloc(size);
    if (!buf) return NULL;
    if (_NSGetExecutablePath(buf, &size) != 0) {
        // Buffer too small; `size` now holds the required length.
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

// Find the QuantoScript home directory (cached)
static char *g_quanto_home = NULL;

static const char *get_quanto_home(void) {
    if (g_quanto_home) return g_quanto_home;

    // 1. QUANTO_HOME environment variable
    const char *env_home = getenv("QUANTO_HOME");
    if (env_home && env_home[0]) {
        g_quanto_home = qs_strdup(env_home);
        return g_quanto_home;
    }

    // 2. Executable-relative locations
    char *exe_path = qs_get_exe_path();
    if (exe_path) {
        char *exe_dir = qs_dirname_dup(exe_path);
        free(exe_path);
        char probe[4096];

        // Standard install layout: bin/../share/quantoscript/stdlib/
        snprintf(probe, sizeof(probe), "%s/../share/quantoscript/stdlib", exe_dir);
        if (qs_dir_exists(probe)) {
            snprintf(probe, sizeof(probe), "%s/../share/quantoscript", exe_dir);
            g_quanto_home = qs_strdup(probe);
            free(exe_dir);
            return g_quanto_home;
        }

        // Development layout: exe alongside stdlib/
        snprintf(probe, sizeof(probe), "%s/stdlib", exe_dir);
        if (qs_dir_exists(probe)) {
            g_quanto_home = qs_strdup(exe_dir);
            free(exe_dir);
            return g_quanto_home;
        }

        free(exe_dir);
    }

    // 3. Current working directory (development mode)
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

// Get the stdlib directory path
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

// Resolve an import path.
// Handles: "stdlib/http.qs", relative paths, absolute paths.
static char *qs_resolve_import(const char *path, const char *base_dir) {
    // Absolute or explicit relative — use as-is
    if (path[0] == '/' || path[0] == '\\' ||
        (path[0] == '.' && path[1] && (path[1] == '/' || path[1] == '\\'))) {
        return qs_strdup(path);
    }

    char probe[4096];

    // "stdlib/http.qs" → resolve from QUANTO_HOME/stdlib
    if (strncmp(path, "stdlib/", 7) == 0 || strncmp(path, "stdlib\\", 7) == 0) {
        const char *stdlib = get_stdlib_dir();
        snprintf(probe, sizeof(probe), "%s/%s", stdlib, path + 7);
        if (access(probe, R_OK) == 0) return qs_strdup(probe);
    }

    // Try relative to base directory
    if (base_dir && base_dir[0]) {
        snprintf(probe, sizeof(probe), "%s/%s", base_dir, path);
        if (access(probe, R_OK) == 0) return qs_strdup(probe);
    }

    // Try relative to stdlib
    const char *stdlib = get_stdlib_dir();
    if (stdlib[0]) {
        snprintf(probe, sizeof(probe), "%s/%s", stdlib, path);
        if (access(probe, R_OK) == 0) return qs_strdup(probe);
    }

    // Try relative to QUANTO_HOME
    const char *home = get_quanto_home();
    if (home[0]) {
        snprintf(probe, sizeof(probe), "%s/%s", home, path);
        if (access(probe, R_OK) == 0) return qs_strdup(probe);
    }

    // Return as-is
    return qs_strdup(path);
}
