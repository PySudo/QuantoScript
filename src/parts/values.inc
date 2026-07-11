static void free_environment(Environment *env);
static void free_value(Value value);

// Object registry: tracks all live ObjectData for cleanup at exit
#define MAX_OBJECTS 65536
static ObjectData *g_object_registry[MAX_OBJECTS];
static int g_object_count = 0;

static void object_registry_init(void) { g_object_count = 0; }

static void object_registry_add(ObjectData *data) {
    if (g_object_count < MAX_OBJECTS) {
        g_object_registry[g_object_count++] = data;
    }
}

static void object_registry_free_all(void) {
    for (int i = 0; i < g_object_count; i++) {
        ObjectData *d = g_object_registry[i];
        free(d->class_name);
        for (size_t j = 0; j < d->fields.count; j++) {
            free(d->fields.items[j].key);
            free_value(d->fields.items[j].value);
        }
        free(d->fields.items);
        free(d);
    }
    g_object_count = 0;
}

static void free_value(Value value) {
    AUDIT_FREE_VALUE();
    if (value.type == VALUE_STRING) {
        free(value.as.string);
    } else if (value.type == VALUE_LIST) {
        for (size_t i = 0; i < value.as.list.count; i++) {
            free_value(value.as.list.items[i]);
        }
        free(value.as.list.items);
    } else if (value.type == VALUE_DICT) {
        for (size_t i = 0; i < value.as.dict.count; i++) {
            free(value.as.dict.items[i].key);
            free_value(value.as.dict.items[i].value);
        }
        free(value.as.dict.items);
    } else if (value.type == VALUE_LAMBDA) {
        for (size_t i = 0; i < value.as.lambda.param_count; i++) {
            free(value.as.lambda.param_names[i]);
            if (value.as.lambda.param_types && value.as.lambda.param_types[i]) {
                free(value.as.lambda.param_types[i]);
            }
        }
        free(value.as.lambda.param_names);
        free(value.as.lambda.param_types);
        free(value.as.lambda.body);
        if (value.as.lambda.closure) {
            free_environment(value.as.lambda.closure);
            free(value.as.lambda.closure);
        }
    } else if (value.type == VALUE_CLASS) {
        free(value.as.class_def.name);
    }
    // VALUE_OBJECT: no-op — ObjectData lives until program exit
}

static Value make_integer(long integer) {
    Value value;
    value.type = VALUE_INTEGER;
    value.as.integer = integer;
    return value;
}

static Value make_float(double floating) {
    Value value;
    value.type = VALUE_FLOAT;
    value.as.floating = floating;
    return value;
}

static Value make_null(void) {
    Value value;
    value.type = VALUE_NULL;
    return value;
}

static Value make_bool(int boolean) {
    Value value;
    value.type = VALUE_BOOL;
    value.as.boolean = boolean ? 1 : 0;
    return value;
}

static Value make_string_owned(char *string) {
    Value value;
    value.type = VALUE_STRING;
    value.as.string = string;
    return value;
}

static Value make_list_empty(void) {
    Value value;
    value.type = VALUE_LIST;
    value.as.list.items = NULL;
    value.as.list.count = 0;
    value.as.list.capacity = 0;
    return value;
}

static Value make_dict_empty(void) {
    Value value;
    value.type = VALUE_DICT;
    value.as.dict.items = NULL;
    value.as.dict.count = 0;
    value.as.dict.capacity = 0;
    return value;
}

static Value make_lambda(char **param_names, char **param_types, size_t param_count, char *body, Environment *closure) {
    Value value;
    value.type = VALUE_LAMBDA;
    value.as.lambda.param_names = param_names;
    value.as.lambda.param_types = param_types;
    value.as.lambda.param_count = param_count;
    value.as.lambda.body = body;
    value.as.lambda.closure = closure;
    return value;
}

static Value make_class(const char *name) {
    Value value;
    value.type = VALUE_CLASS;
    value.as.class_def.name = qs_strdup(name);
    return value;
}

static Value make_object(const char *class_name) {
    Value value;
    value.type = VALUE_OBJECT;
    value.as.object.data = malloc(sizeof(ObjectData));
    value.as.object.data->class_name = qs_strdup(class_name);
    value.as.object.data->fields.items = NULL;
    value.as.object.data->fields.count = 0;
    value.as.object.data->fields.capacity = 0;
    object_registry_add(value.as.object.data);
    return value;
}

static Value clone_value(Value value) {
    AUDIT_CLONE();
    Value copy;
    copy.type = value.type;
    if (value.type == VALUE_STRING) {
        copy.as.string = qs_strdup(value.as.string);
    } else if (value.type == VALUE_LIST) {
        copy.as.list.count = value.as.list.count;
        copy.as.list.capacity = value.as.list.count;
        copy.as.list.items = calloc(value.as.list.count, sizeof(Value));
        if (value.as.list.count && !copy.as.list.items) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        for (size_t i = 0; i < value.as.list.count; i++) {
            copy.as.list.items[i] = clone_value(value.as.list.items[i]);
        }
    } else if (value.type == VALUE_DICT) {
        copy.as.dict.count = value.as.dict.count;
        copy.as.dict.capacity = value.as.dict.count;
        copy.as.dict.items = calloc(value.as.dict.count, sizeof(DictEntry));
        if (value.as.dict.count && !copy.as.dict.items) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        for (size_t i = 0; i < value.as.dict.count; i++) {
            copy.as.dict.items[i].key = qs_strdup(value.as.dict.items[i].key);
            copy.as.dict.items[i].value = clone_value(value.as.dict.items[i].value);
        }
    } else if (value.type == VALUE_LAMBDA) {
        copy.as.lambda.param_count = value.as.lambda.param_count;
        copy.as.lambda.param_names = calloc(value.as.lambda.param_count, sizeof(char *));
        copy.as.lambda.param_types = value.as.lambda.param_types ? calloc(value.as.lambda.param_count, sizeof(char *)) : NULL;
        for (size_t i = 0; i < value.as.lambda.param_count; i++) {
            copy.as.lambda.param_names[i] = qs_strdup(value.as.lambda.param_names[i]);
            if (copy.as.lambda.param_types && value.as.lambda.param_types && value.as.lambda.param_types[i]) {
                copy.as.lambda.param_types[i] = qs_strdup(value.as.lambda.param_types[i]);
            }
        }
        copy.as.lambda.body = qs_strdup(value.as.lambda.body);
        copy.as.lambda.closure = NULL;
        if (value.as.lambda.closure) {
            copy.as.lambda.closure = malloc(sizeof(Environment));
            copy.as.lambda.closure->count = value.as.lambda.closure->count;
            copy.as.lambda.closure->capacity = value.as.lambda.closure->count;
            copy.as.lambda.closure->items = calloc(copy.as.lambda.closure->count, sizeof(Variable));
            for (size_t i = 0; i < copy.as.lambda.closure->count; i++) {
                copy.as.lambda.closure->items[i].name = qs_strdup(value.as.lambda.closure->items[i].name);
                copy.as.lambda.closure->items[i].value = clone_value(value.as.lambda.closure->items[i].value);
            }
        }
    } else if (value.type == VALUE_BOOL) {
        copy.as.boolean = value.as.boolean;
    } else if (value.type == VALUE_NULL) {
        (void)copy;
    } else if (value.type == VALUE_FLOAT) {
        copy.as.floating = value.as.floating;
    } else if (value.type == VALUE_CLASS) {
        copy.as.class_def.name = qs_strdup(value.as.class_def.name);
    } else if (value.type == VALUE_OBJECT) {
        // Shared identity: copy the pointer, no allocation
        copy.as.object.data = value.as.object.data;
    } else {
        copy.as.integer = value.as.integer;
    }
    return copy;
}

// Task queue management
static TaskQueue g_task_queue = {0};
static int g_next_task_id = 1;

static void task_queue_init(void) {
    g_task_queue.items = NULL;
    g_task_queue.count = 0;
    g_task_queue.capacity = 0;
    g_next_task_id = 1;
}

static int task_create(Value function) {
    if (g_task_queue.count >= g_task_queue.capacity) {
        size_t new_cap = g_task_queue.capacity ? g_task_queue.capacity * 2 : 8;
        Task *new_items = realloc(g_task_queue.items, new_cap * sizeof(Task));
        if (!new_items) { fprintf(stderr, "out of memory\n"); exit(2); }
        g_task_queue.items = new_items;
        g_task_queue.capacity = new_cap;
    }

    Task task = {0};
    task.id = g_next_task_id++;
    task.status = TASK_READY;
    task.function = clone_value(function);
    task.has_result = 0;
    task.result = make_null();
    task.error = 0;
    task.error_msg[0] = '\0';

    g_task_queue.items[g_task_queue.count++] = task;
    return task.id;
}

static Task *task_find(int id) {
    for (size_t i = 0; i < g_task_queue.count; i++) {
        if (g_task_queue.items[i].id == id) {
            return &g_task_queue.items[i];
        }
    }
    return NULL;
}

static void task_set_result(int id, Value result) {
    Task *task = task_find(id);
    if (task) {
        task->result = result;
        task->has_result = 1;
        task->status = TASK_DONE;
    }
}

static void task_set_error(int id, const char *msg) {
    Task *task = task_find(id);
    if (task) {
        task->error = 1;
        snprintf(task->error_msg, sizeof(task->error_msg), "%s", msg);
        task->status = TASK_ERROR;
    }
}

static void task_cleanup(void) {
    for (size_t i = 0; i < g_task_queue.count; i++) {
        free_value(g_task_queue.items[i].function);
        if (g_task_queue.items[i].has_result) {
            free_value(g_task_queue.items[i].result);
        }
    }
    free(g_task_queue.items);
    g_task_queue.items = NULL;
    g_task_queue.count = 0;
    g_task_queue.capacity = 0;
}

static int value_truthy(Value value) {
    if (value.type == VALUE_NULL) {
        return 0;
    }
    if (value.type == VALUE_BOOL) {
        return value.as.boolean;
    }
    if (value.type == VALUE_INTEGER) {
        return value.as.integer != 0;
    }
    if (value.type == VALUE_FLOAT) {
        return value.as.floating != 0.0;
    }
    if (value.type == VALUE_STRING) {
        return value.as.string[0] != '\0';
    }
    if (value.type == VALUE_DICT) {
        return value.as.dict.count > 0;
    }
    if (value.type == VALUE_LAMBDA) {
        return 1;
    }
    if (value.type == VALUE_CLASS) {
        return value.as.class_def.name != NULL;
    }
    if (value.type == VALUE_OBJECT) {
        return value.as.object.data != NULL;
    }
    return value.as.list.count > 0;
}

static double value_to_double(Value value) {
    if (value.type == VALUE_INTEGER) return (double)value.as.integer;
    if (value.type == VALUE_FLOAT) return value.as.floating;
    return 0.0;
}

static int value_compare(Value left, Value right) {
    if (left.type != right.type) {
        return (int)left.type - (int)right.type;
    }
    if (left.type == VALUE_INTEGER) {
        return (left.as.integer > right.as.integer) - (left.as.integer < right.as.integer);
    }
    if (left.type == VALUE_FLOAT) {
        return (left.as.floating > right.as.floating) - (left.as.floating < right.as.floating);
    }
    if (left.type == VALUE_STRING) {
        return strcmp(left.as.string, right.as.string);
    }
    if (left.type == VALUE_BOOL) {
        return left.as.boolean - right.as.boolean;
    }
    if (left.type == VALUE_LIST) {
        return (left.as.list.count > right.as.list.count) - (left.as.list.count < right.as.list.count);
    }
    if (left.type == VALUE_DICT) {
        return (left.as.dict.count > right.as.dict.count) - (left.as.dict.count < right.as.dict.count);
    }
    return 0;
}

static int find_variable(Environment *env, const char *name) {
    AUDIT_FIND_VAR();
    for (size_t i = 0; i < env->count; i++) {
        if (env->items[i].name == name || strcmp(env->items[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

// Fast parameter initialization: directly append without lookup
// Only safe when environment is known to be empty or parameter doesn't exist
static QS_UNUSED_FN void env_init_param(Environment *env, const char *name, Value value) {
    AUDIT_ENV_SET();
    if (env->count == env->capacity) {
        size_t next_capacity = env->capacity ? env->capacity * 2 : 8;
        Variable *next = realloc(env->items, next_capacity * sizeof(Variable));
        if (!next) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        env->items = next;
        env->capacity = next_capacity;
    }
    env->items[env->count].name = qs_strdup(name);
    env->items[env->count].value = value;
    env->count++;
}

static void env_set(Environment *env, const char *name, Value value) {
    AUDIT_ENV_SET();
    int index = find_variable(env, name);
    if (index >= 0) {
        free_value(env->items[index].value);
        env->items[index].value = value;
        return;
    }

    if (env->count == env->capacity) {
        size_t next_capacity = env->capacity ? env->capacity * 2 : 8;
        Variable *next = realloc(env->items, next_capacity * sizeof(Variable));
        if (!next) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        env->items = next;
        env->capacity = next_capacity;
    }

    env->items[env->count].name = qs_strdup(name);
    env->items[env->count].value = value;
    env->count++;
}

static QS_UNUSED_FN Variable *env_get_slot(Environment *env, const char *name) {
    int index = find_variable(env, name);
    if (index < 0) {
        env_set(env, name, make_null());
        index = find_variable(env, name);
    }
    return index >= 0 ? &env->items[index] : NULL;
}

// ---- Slot-based local variable access ----
// These functions provide O(1) access to local variables without string comparison
// Slots are stored inline in CallFrame (no heap allocation)

static inline Value env_load_slot(Environment *env, int slot) {
    // Slots are now in CallFrame.local_slots, accessed via frame pointer
    // This function is kept for compatibility but actual access goes through frame
    return env->slots[slot];
}

static inline void env_store_slot(Environment *env, int slot, Value value) {
    free_value(env->slots[slot]);
    env->slots[slot] = value;
}

static void list_push(ValueList *list, Value value) {
    if (list->count == list->capacity) {
        size_t next_capacity = list->capacity ? list->capacity * 2 : 4;
        Value *next = realloc(list->items, next_capacity * sizeof(Value));
        if (!next) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        list->items = next;
        list->capacity = next_capacity;
    }
    list->items[list->count++] = value;
}

static int dict_find(ValueDict *dict, const char *key) {
    for (size_t i = 0; i < dict->count; i++) {
        if (strcmp(dict->items[i].key, key) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static void dict_set(ValueDict *dict, const char *key, Value value) {
    int index = dict_find(dict, key);
    if (index >= 0) {
        free_value(dict->items[index].value);
        dict->items[index].value = value;
        return;
    }
    if (dict->count == dict->capacity) {
        size_t next_capacity = dict->capacity ? dict->capacity * 2 : 4;
        DictEntry *next = realloc(dict->items, next_capacity * sizeof(DictEntry));
        if (!next) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        dict->items = next;
        dict->capacity = next_capacity;
    }
    dict->items[dict->count].key = qs_strdup(key);
    dict->items[dict->count].value = value;
    dict->count++;
}

static void param_push(Function *function, Param param) {
    if (function->param_count == function->param_capacity) {
        size_t next_capacity = function->param_capacity ? function->param_capacity * 2 : 4;
        Param *next = realloc(function->params, next_capacity * sizeof(Param));
        if (!next) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        function->params = next;
        function->param_capacity = next_capacity;
    }
    function->params[function->param_count++] = param;
}

static void function_push(Function function) {
    for (size_t i = 0; i < g_functions.count; i++) {
        if (strcmp(g_functions.items[i].name, function.name) == 0) {
            free(g_functions.items[i].name);
            for (size_t p = 0; p < g_functions.items[i].param_count; p++) {
                free(g_functions.items[i].params[p].name);
                free(g_functions.items[i].params[p].type);
                if (g_functions.items[i].params[p].has_default) {
                    free_value(g_functions.items[i].params[p].default_value);
                }
            }
            free(g_functions.items[i].params);
            g_functions.items[i] = function;
            return;
        }
    }

    if (g_functions.count == g_functions.capacity) {
        size_t next_capacity = g_functions.capacity ? g_functions.capacity * 2 : 8;
        Function *next = realloc(g_functions.items, next_capacity * sizeof(Function));
        if (!next) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        g_functions.items = next;
        g_functions.capacity = next_capacity;
    }
    g_functions.items[g_functions.count++] = function;
}

// Push function only if no function with the same name exists.
// Returns 1 if pushed, 0 if skipped (name already taken).
static int function_push_if_new(Function function) {
    for (size_t i = 0; i < g_functions.count; i++) {
        if (strcmp(g_functions.items[i].name, function.name) == 0) {
            return 0;  // already exists — don't overwrite
        }
    }
    if (g_functions.count == g_functions.capacity) {
        size_t next_capacity = g_functions.capacity ? g_functions.capacity * 2 : 8;
        Function *next = realloc(g_functions.items, next_capacity * sizeof(Function));
        if (!next) { fprintf(stderr, "out of memory\n"); exit(2); }
        g_functions.items = next;
        g_functions.capacity = next_capacity;
    }
    g_functions.items[g_functions.count++] = function;
    return 1;
}

static Program *module_push(Program program) {
    if (g_modules.count == g_modules.capacity) {
        size_t next_capacity = g_modules.capacity ? g_modules.capacity * 2 : 4;
        Program **next = realloc(g_modules.items, next_capacity * sizeof(Program *));
        if (!next) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        g_modules.items = next;
        g_modules.capacity = next_capacity;
    }
    Program *stored = malloc(sizeof(Program));
    if (!stored) {
        fprintf(stderr, "out of memory\n");
        exit(2);
    }
    *stored = program;
    g_modules.items[g_modules.count++] = stored;
    return stored;
}

static Function *find_function(const char *name) {
    for (size_t i = 0; i < g_functions.count; i++) {
        if (strcmp(g_functions.items[i].name, name) == 0) {
            return &g_functions.items[i];
        }
    }
    return NULL;
}

static void list_sort(ValueList *list) {
    for (size_t i = 0; i < list->count; i++) {
        for (size_t j = i + 1; j < list->count; j++) {
            if (value_compare(list->items[i], list->items[j]) > 0) {
                Value tmp = list->items[i];
                list->items[i] = list->items[j];
                list->items[j] = tmp;
            }
        }
    }
}
