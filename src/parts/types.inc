#ifndef QS_VERSION
#define QS_VERSION "2.1.0"
#endif

typedef enum {
    VALUE_NULL,
    VALUE_INTEGER,
    VALUE_FLOAT,
    VALUE_STRING,
    VALUE_BOOL,
    VALUE_LIST,
    VALUE_DICT,
    VALUE_LAMBDA,
    VALUE_CLASS,
    VALUE_OBJECT
} ValueType;

typedef struct Value Value;
typedef struct Environment Environment;
typedef struct DictEntry DictEntry;

typedef struct {
    Value *items;
    size_t count;
    size_t capacity;
} ValueList;

typedef struct {
    DictEntry *items;
    size_t count;
    size_t capacity;
} ValueDict;

typedef struct ObjectData ObjectData;

struct ObjectData {
    char *class_name;
    ValueDict fields;
};

struct Value {
    ValueType type;
    union {
        long integer;
        double floating;
        char *string;
        int boolean;
        ValueList list;
        ValueDict dict;
        struct {
            char **param_names;
            char **param_types;
            size_t param_count;
            char *body;
            Environment *closure;
            int is_module;
        } lambda;
        struct {
            char *name;
        } class_def;
        struct {
            ObjectData *data;
        } object;
    } as;
};

struct DictEntry {
    char *key;
    Value value;
};

typedef struct {
    char *name;
    Value value;
} Variable;

struct Environment {
    Variable *items;
    size_t count;
    size_t capacity;
    Value *slots;
    size_t slot_count;
};

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
    char *path;
    char *dir;
} Program;

typedef struct {
    char *name;
    char *type;
    int is_args;
    int is_kwargs;
    int has_default;
    Value default_value;
} Param;

typedef struct {
    char *name;
    Param *params;
    size_t param_count;
    size_t param_capacity;
    size_t body_start;
    size_t body_end;
    Program *program;
    int is_module;
    Environment *module_env;
} Function;

typedef struct {
    Function *items;
    size_t count;
    size_t capacity;
} FunctionTable;

typedef struct {
    Program **items;
    size_t count;
    size_t capacity;
} ProgramTable;

typedef struct {
    int line;
    int column;
    char message[256];
} ParseError;

typedef struct {
    const char *text;
    size_t pos;
    int line;
    Environment *env;
    ParseError *err;
    int var_index;
} Parser;

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_WAITING,
    TASK_DONE,
    TASK_ERROR
} TaskStatus;

typedef struct Task Task;
struct Task {
    int id;
    TaskStatus status;
    Value function;
    Value result;
    int has_result;
    Environment *env;
    char *body;
    size_t body_start;
    size_t body_end;
    Program *program;
    int error;
    char error_msg[256];
};

typedef struct {
    Task *items;
    size_t count;
    size_t capacity;
} TaskQueue;

static Program *g_program = NULL;
static FunctionTable g_functions = {0};
static ProgramTable g_modules = {0};
static int g_function_depth = 0;
static int g_has_return = 0;
static int g_loop_depth = 0;
static int g_has_break = 0;
static int g_has_continue = 0;
static int g_module_mode = 0;
static int g_native_access_depth = 0;
static Environment *g_root_env = NULL;
static Function *g_current_function = NULL;
static int g_module_func_depth = 0;

static int g_var_is_global = 0;
static Value g_return_value;
static char **g_call_stack = NULL;
static size_t g_call_stack_count = 0;
static size_t g_call_stack_capacity = 0;
static int g_profiling = 0;
static int g_sandbox_mode = 0;
static char g_sandbox_path[1024] = {0};

#if defined(__GNUC__)
#define QS_UNUSED_FN __attribute__((unused))
#else
#define QS_UNUSED_FN
#endif
