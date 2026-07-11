static void print_value(Value value) {
    if (value.type == VALUE_NULL) {
        printf("null");
    } else if (value.type == VALUE_INTEGER) {
        printf("%ld", value.as.integer);
    } else if (value.type == VALUE_FLOAT) {
        printf("%.10g", value.as.floating);
    } else if (value.type == VALUE_BOOL) {
        printf("%s", value.as.boolean ? "true" : "false");
    } else if (value.type == VALUE_STRING) {
        printf("%s", value.as.string);
    } else if (value.type == VALUE_DICT) {
        printf("{");
        for (size_t i = 0; i < value.as.dict.count; i++) {
            if (i) {
                printf(", ");
            }
            printf("'%s': ", value.as.dict.items[i].key);
            print_list_value(value.as.dict.items[i].value);
        }
        printf("}");
    } else if (value.type == VALUE_LAMBDA) {
        printf("<lambda>");
    } else if (value.type == VALUE_CLASS) {
        printf("<class %s>", value.as.class_def.name);
    } else if (value.type == VALUE_OBJECT) {
        printf("<object %s>", value.as.object.data->class_name);
    } else {
        printf("[");
        for (size_t i = 0; i < value.as.list.count; i++) {
            if (i) {
                printf(", ");
            }
            print_list_value(value.as.list.items[i]);
        }
        printf("]");
    }
}

static void print_list_value(Value value) {
    if (value.type == VALUE_STRING) {
        printf("'%s'", value.as.string);
    } else {
        print_value(value);
    }
}

static void free_environment(Environment *env) {
    // Free slot-based locals
    if (env->slots) {
        for (size_t i = 0; i < env->slot_count; i++) {
            free_value(env->slots[i]);
        }
        free(env->slots);
        env->slots = NULL;
        env->slot_count = 0;
    }
    // Free name-based items (globals)
    for (size_t i = 0; i < env->count; i++) {
        free(env->items[i].name);
        free_value(env->items[i].value);
    }
    free(env->items);
    env->items = NULL;
    env->count = 0;
    env->capacity = 0;
}

static void free_program(Program *program) {
    for (size_t i = 0; i < program->count; i++) {
        free(program->items[i]);
    }
    free(program->items);
    free(program->path);
    free(program->dir);
}

static void free_functions(void) {
    for (size_t i = 0; i < g_functions.count; i++) {
        free(g_functions.items[i].name);
        for (size_t p = 0; p < g_functions.items[i].param_count; p++) {
            free(g_functions.items[i].params[p].name);
            free(g_functions.items[i].params[p].type);
            if (g_functions.items[i].params[p].has_default) {
                free_value(g_functions.items[i].params[p].default_value);
            }
        }
        free(g_functions.items[i].params);
    }
    free(g_functions.items);
    g_functions.items = NULL;
    g_functions.count = 0;
    g_functions.capacity = 0;
}

static void free_modules(void) {
    for (size_t i = 0; i < g_modules.count; i++) {
        free_program(g_modules.items[i]);
        free(g_modules.items[i]);
    }
    free(g_modules.items);
    g_modules.items = NULL;
    g_modules.count = 0;
    g_modules.capacity = 0;
}
