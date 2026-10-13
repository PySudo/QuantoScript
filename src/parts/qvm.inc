#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define QVM_MAGIC          0x314D5651
#define QVM_VERSION        4
#define QVM_NO_CONST       (-1)
#define QVM_FLAG_HAS_SOURCE  0x0001
#define QVM_FLAG_HAS_CRC     0x0002
#define QVM_MAX_POOL       65536
#define QVM_MAX_INSTR      65536
#define QVM_MAX_FUNCS      256
#define QVM_MAX_FUNC_BODY  65536
#define QVM_MAX_NAME_LEN   4096
#define QVM_MAX_STRING_LEN (1 << 20)
#define QVM_HEADER_SIZE    32

static uint32_t qvm_crc32(const unsigned char *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

static int qr_u8_f(FILE *f, uint8_t *out) {
    return fread(out, 1, 1, f) == 1;
}

static int qr_u32_f(FILE *f, uint32_t *out) {
    return fread(out, 4, 1, f) == 1;
}

static int qr_i32_f(FILE *f, int32_t *out) {
    return fread(out, 4, 1, f) == 1;
}

static int qr_i64_f(FILE *f, int64_t *out) {
    return fread(out, 8, 1, f) == 1;
}

static int qr_f64_f(FILE *f, double *out) {
    return fread(out, 8, 1, f) == 1;
}

static QS_UNUSED_FN uint8_t  qr_u8(FILE *f)  { uint8_t v = 0;  qr_u8_f(f, &v);  return v; }
static QS_UNUSED_FN uint32_t qr_u32(FILE *f) { uint32_t v = 0; qr_u32_f(f, &v); return v; }
static QS_UNUSED_FN int32_t  qr_i32(FILE *f) { int32_t v = 0;  qr_i32_f(f, &v); return v; }

static void qw_u8(FILE *f, uint8_t v)   { fwrite(&v, 1, 1, f); }
static void qw_u32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void qw_i32(FILE *f, int32_t v)  { fwrite(&v, 4, 1, f); }
static void qw_i64(FILE *f, int64_t v)  { fwrite(&v, 8, 1, f); }
static void qw_f64(FILE *f, double v)   { fwrite(&v, 8, 1, f); }

static void qvm_write_str(FILE *f, const char *s) {
    uint32_t len = s ? (uint32_t)strlen(s) : 0;
    qw_u32(f, len);
    if (len) fwrite(s, 1, len, f);
}

static int qvm_read_str(FILE *f, char **out) {
    uint32_t len = 0;
    if (!qr_u32_f(f, &len)) return 0;
    if (len == 0) { *out = NULL; return 1; }
    if (len > (1u << 24)) return 0;
    char *s = malloc(len + 1);
    if (!s) return 0;
    if (fread(s, 1, len, f) != len) { free(s); return 0; }
    s[len] = '\0';
    *out = s;
    return 1;
}

static void qvm_write_value(FILE *f, Value v) {
    switch (v.type) {
        case VALUE_NULL:    qw_u8(f, 0x00); break;
        case VALUE_INTEGER: qw_u8(f, 0x01); qw_i64(f, v.as.integer); break;
        case VALUE_FLOAT:   qw_u8(f, 0x02); qw_f64(f, v.as.floating); break;
        case VALUE_BOOL:    qw_u8(f, 0x03); qw_u8(f, v.as.boolean ? 1 : 0); break;
        case VALUE_STRING: {
            qw_u8(f, 0x04);
            uint32_t len = v.as.string ? (uint32_t)strlen(v.as.string) : 0;
            qw_u32(f, len);
            if (len > 0) fwrite(v.as.string, 1, len, f);
            break;
        }
        case VALUE_LAMBDA: {
            qw_u8(f, 0x05);
            qvm_write_str(f, v.as.lambda.body);
            qw_u32(f, (uint32_t)v.as.lambda.param_count);
            for (size_t i = 0; i < v.as.lambda.param_count; i++) {
                qvm_write_str(f, (v.as.lambda.param_names) ? v.as.lambda.param_names[i] : NULL);
                qvm_write_str(f, (v.as.lambda.param_types && v.as.lambda.param_types[i]) ? v.as.lambda.param_types[i] : NULL);
            }
            qw_u8(f, v.as.lambda.is_module ? 1 : 0);
            break;
        }
        case VALUE_LIST: {
            qw_u8(f, 0x06);
            qw_u32(f, (uint32_t)v.as.list.count);
            for (size_t i = 0; i < v.as.list.count; i++) qvm_write_value(f, v.as.list.items[i]);
            break;
        }
        case VALUE_DICT: {
            qw_u8(f, 0x07);
            qw_u32(f, (uint32_t)v.as.dict.count);
            for (size_t i = 0; i < v.as.dict.count; i++) {
                qvm_write_str(f, v.as.dict.items[i].key);
                qvm_write_value(f, v.as.dict.items[i].value);
            }
            break;
        }
        default: qw_u8(f, 0x00); break;
    }
}

static int qvm_read_value_f(FILE *f, Value *out) {
    uint8_t tag;
    if (!qr_u8_f(f, &tag)) return 0;
    switch (tag) {
        case 0x00: *out = make_null(); return 1;
        case 0x01: { int64_t v; if (!qr_i64_f(f, &v)) return 0; *out = make_integer(v); return 1; }
        case 0x02: { double v; if (!qr_f64_f(f, &v)) return 0; *out = make_float(v); return 1; }
        case 0x03: { uint8_t v; if (!qr_u8_f(f, &v)) return 0; *out = make_bool(v != 0); return 1; }
        case 0x04: {
            uint32_t len;
            if (!qr_u32_f(f, &len)) return 0;
            if (len > QVM_MAX_STRING_LEN) return 0;
            if (len == 0) { *out = make_string_owned(qs_strdup("")); return 1; }
            char *buf = malloc(len + 1);
            if (!buf) return 0;
            if (fread(buf, 1, len, f) != len) { free(buf); return 0; }
            buf[len] = '\0';
            *out = make_string_owned(buf);
            return 1;
        }
        case 0x05: {
            Value lam; memset(&lam, 0, sizeof(lam));
            lam.type = VALUE_LAMBDA;
            char *body = NULL;
            if (!qvm_read_str(f, &body)) return 0;
            lam.as.lambda.body = body;
            uint32_t pc = 0;
            if (!qr_u32_f(f, &pc)) { free(body); return 0; }
            lam.as.lambda.param_count = pc;
            lam.as.lambda.param_names = pc ? calloc(pc, sizeof(char *)) : NULL;
            lam.as.lambda.param_types = pc ? calloc(pc, sizeof(char *)) : NULL;
            for (uint32_t i = 0; i < pc; i++) {
                if (!qvm_read_str(f, &lam.as.lambda.param_names[i])) return 0;
                if (!qvm_read_str(f, &lam.as.lambda.param_types[i])) return 0;
            }
            uint8_t im = 0;
            if (!qr_u8_f(f, &im)) return 0;
            lam.as.lambda.is_module = im;
            *out = lam;
            return 1;
        }
        case 0x06: {
            uint32_t n = 0;
            if (!qr_u32_f(f, &n)) return 0;
            Value lv = make_list_empty();
            for (uint32_t i = 0; i < n; i++) {
                Value item;
                if (!qvm_read_value_f(f, &item)) return 0;
                list_push(&lv.as.list, item);
            }
            *out = lv;
            return 1;
        }
        case 0x07: {
            uint32_t n = 0;
            if (!qr_u32_f(f, &n)) return 0;
            Value dv = make_dict_empty();
            for (uint32_t i = 0; i < n; i++) {
                char *k = NULL;
                if (!qvm_read_str(f, &k)) return 0;
                Value item;
                if (!qvm_read_value_f(f, &item)) { free(k); return 0; }
                dict_set(&dv.as.dict, k ? k : "", item);
                free(k);
            }
            *out = dv;
            return 1;
        }
        default: *out = make_null(); return 1;
    }
}

static QS_UNUSED_FN Value qvm_read_value(FILE *f) {
    Value v = {0};
    qvm_read_value_f(f, &v);
    return v;
}

static int qvm_valid_opcode(uint8_t op) {
    return op <= OP_CALL_KW;
}

static QS_UNUSED_FN int qvm_serialize(BytecodeProgram *prog, CompiledFunc *funcs, int func_count,
                         const char *source_path, const char *output_path) {
    #define QVM_MAX_GLOBAL QVM_MAX_POOL
    Value *pool = calloc(QVM_MAX_GLOBAL, sizeof(Value));
    if (!pool) return 0;
    int pool_count = 0;

    for (size_t i = 0; i < prog->const_count && pool_count < QVM_MAX_GLOBAL; i++)
        pool[pool_count++] = clone_value(prog->constants[i]);

    int *main_arg_indices = calloc(prog->count, sizeof(int));
    for (size_t i = 0; i < prog->count; i++) {
        Value arg = prog->instructions[i].arg;
        if (arg.type == VALUE_STRING && arg.as.string) {
            int found = -1;
            for (int ci = 0; ci < pool_count; ci++) {
                if (pool[ci].type == VALUE_STRING && pool[ci].as.string &&
                    strcmp(pool[ci].as.string, arg.as.string) == 0) {
                    found = ci; break;
                }
            }
            if (found >= 0) {
                main_arg_indices[i] = found;
            } else if (pool_count < QVM_MAX_GLOBAL) {
                main_arg_indices[i] = pool_count;
                pool[pool_count++] = clone_value(arg);
            } else {
                main_arg_indices[i] = QVM_NO_CONST;
            }
        } else {
            main_arg_indices[i] = QVM_NO_CONST;
        }
    }

    int *func_const_start = calloc(func_count, sizeof(int));
    int **func_local_to_global = calloc(func_count, sizeof(int*));
    for (int fi = 0; fi < func_count; fi++) {
        func_local_to_global[fi] = calloc(funcs[fi].body.const_count, sizeof(int));
        func_const_start[fi] = pool_count;
        for (size_t ci = 0; ci < funcs[fi].body.const_count && pool_count < QVM_MAX_GLOBAL; ci++) {
            func_local_to_global[fi][ci] = pool_count;
            pool[pool_count++] = clone_value(funcs[fi].body.constants[ci]);
        }
    }

    int *func_arg_start = calloc(func_count, sizeof(int));
    int **func_arg_indices = calloc(func_count, sizeof(int*));
    for (int fi = 0; fi < func_count; fi++) {
        func_arg_indices[fi] = calloc(funcs[fi].body.count, sizeof(int));
        func_arg_start[fi] = pool_count;
        for (size_t i = 0; i < funcs[fi].body.count && pool_count < QVM_MAX_GLOBAL; i++) {
            Value arg = funcs[fi].body.instructions[i].arg;
            if (arg.type == VALUE_STRING && arg.as.string) {
                int found = -1;

                for (int ci = func_const_start[fi]; ci < pool_count; ci++) {
                    if (pool[ci].type == VALUE_STRING && pool[ci].as.string &&
                        strcmp(pool[ci].as.string, arg.as.string) == 0) {
                        found = ci; break;
                    }
                }

                if (found < 0) {
                    for (int ci = 0; ci < func_const_start[fi]; ci++) {
                        if (pool[ci].type == VALUE_STRING && pool[ci].as.string &&
                            strcmp(pool[ci].as.string, arg.as.string) == 0) {
                            found = ci; break;
                        }
                    }
                }
                if (found >= 0) {
                    func_arg_indices[fi][i] = found;
                } else if (pool_count < QVM_MAX_GLOBAL) {
                    func_arg_indices[fi][i] = pool_count;
                    pool[pool_count++] = clone_value(arg);
                } else {
                    func_arg_indices[fi][i] = QVM_NO_CONST;
                }
            } else {
                func_arg_indices[fi][i] = QVM_NO_CONST;
            }
        }
    }

    int *param_indices_flat = calloc(func_count * 32, sizeof(int));
    for (int fi = 0; fi < func_count; fi++) {
        for (int pi = 0; pi < funcs[fi].param_count && pi < 32; pi++) {
            param_indices_flat[fi * 32 + pi] = QVM_NO_CONST;
            for (int ci = 0; ci < pool_count; ci++) {
                if (pool[ci].type == VALUE_STRING && pool[ci].as.string &&
                    strcmp(pool[ci].as.string, funcs[fi].param_names[pi]) == 0) {
                    param_indices_flat[fi * 32 + pi] = ci; break;
                }
            }
        }
    }

    uint32_t crc = 0;
    int has_crc = 0;
    if (source_path) {
        FILE *sf = fopen(source_path, "rb");
        if (sf) {
            fseek(sf, 0, SEEK_END);
            long sz = ftell(sf);
            fseek(sf, 0, SEEK_SET);
            if (sz > 0) {
                unsigned char *buf = malloc(sz);
                if (buf) {
                    if (fread(buf, 1, sz, sf) == (size_t)sz) {
                        crc = qvm_crc32(buf, sz);
                        has_crc = 1;
                    }
                    free(buf);
                }
            }
            fclose(sf);
        }
    }

    FILE *f = fopen(output_path, "wb");
    if (!f) goto cleanup;

    uint32_t flags = 0;
    if (has_crc) flags |= QVM_FLAG_HAS_CRC;
    if (source_path) flags |= QVM_FLAG_HAS_SOURCE;
    qw_u32(f, QVM_MAGIC);
    qw_u32(f, QVM_VERSION);
    qw_u32(f, flags);
    qw_u32(f, crc);
    qw_u32(f, (uint32_t)pool_count);
    qw_u32(f, (uint32_t)prog->count);
    qw_u32(f, (uint32_t)func_count);
    qw_u32(f, 0);

    for (int i = 0; i < pool_count; i++)
        qvm_write_value(f, pool[i]);

    for (size_t i = 0; i < prog->count; i++) {
        qw_u8(f, (uint8_t)prog->instructions[i].op);
        qw_i32(f, prog->instructions[i].arg_i);
        qw_i32(f, main_arg_indices[i]);
        qw_i32(f, prog->instructions[i].line);
    }

    for (int fi = 0; fi < func_count; fi++) {
        qw_u32(f, (uint32_t)strlen(funcs[fi].name));
        fwrite(funcs[fi].name, 1, strlen(funcs[fi].name), f);
        qw_u32(f, (uint32_t)funcs[fi].param_count);
        qw_u32(f, (uint32_t)funcs[fi].body.count);
        qw_i32(f, (int32_t)funcs[fi].slot_count);
        for (int pi = 0; pi < funcs[fi].param_count; pi++)
            qw_i32(f, param_indices_flat[fi * 32 + pi]);
    }

    for (int fi = 0; fi < func_count; fi++) {
        for (size_t i = 0; i < funcs[fi].body.count; i++) {
            Instruction *ins = &funcs[fi].body.instructions[i];
            int32_t w_arg_i = ins->arg_i;
            if (ins->op == OP_LOAD_CONST && ins->arg_i >= 0 && ins->arg_i < (int32_t)funcs[fi].body.const_count)
                w_arg_i = func_local_to_global[fi][ins->arg_i];
            qw_u8(f, (uint8_t)ins->op);
            qw_i32(f, w_arg_i);
            qw_i32(f, func_arg_indices[fi][i]);
            qw_i32(f, ins->line);
        }
    }

    if (source_path) {
        uint32_t plen = (uint32_t)strlen(source_path);
        qw_u32(f, plen);
        fwrite(source_path, 1, plen, f);
    }

    fclose(f);

cleanup:
    for (int i = 0; i < pool_count; i++) free_value(pool[i]);
    free(pool);
    free(main_arg_indices);
    free(param_indices_flat);
    for (int fi = 0; fi < func_count; fi++) {
        free(func_local_to_global[fi]);
        free(func_arg_indices[fi]);
    }
    free(func_local_to_global);
    free(func_arg_indices);
    free(func_const_start);
    free(func_arg_start);
    return (f != NULL);
}

typedef struct {
    BytecodeProgram main_prog;
    Value *pool;
    int pool_count;
    CompiledFunc *funcs;
    int func_count;
    char *source_path;
} QVMFile;

static void qvm_free(QVMFile *qvm) {
    if (!qvm) return;
    free(qvm->main_prog.instructions);
    for (size_t i = 0; i < qvm->main_prog.const_count; i++)
        free_value(qvm->main_prog.constants[i]);
    free(qvm->main_prog.constants);
    for (int i = 0; i < qvm->pool_count; i++) free_value(qvm->pool[i]);
    free(qvm->pool);
    for (int i = 0; i < qvm->func_count; i++) {
        free(qvm->funcs[i].name);
        for (int p = 0; p < qvm->funcs[i].param_count; p++)
            free(qvm->funcs[i].param_names[p]);
        free(qvm->funcs[i].param_names);
    }
    free(qvm->funcs);
    free(qvm->source_path);
    memset(qvm, 0, sizeof(*qvm));
}

static int qvm_load(const char *path, QVMFile *qvm) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open '%s'\n", path);
        return 0;
    }

    memset(qvm, 0, sizeof(*qvm));

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (file_size < QVM_HEADER_SIZE) {
        fprintf(stderr, "qvm: file too small for header (%ld bytes)\n", file_size);
        fclose(f); return 0;
    }

    uint32_t magic, version, flags, crc;
    if (!qr_u32_f(f, &magic) || !qr_u32_f(f, &version) ||
        !qr_u32_f(f, &flags) || !qr_u32_f(f, &crc)) {
        fprintf(stderr, "qvm: failed to read header\n");
        fclose(f); return 0;
    }

    if (magic != QVM_MAGIC) {
        fprintf(stderr, "qvm: not a valid .qvm file (bad magic 0x%08X)\n", magic);
        fclose(f); return 0;
    }
    if (version > QVM_VERSION) {
        fprintf(stderr, "qvm: unsupported version %u (max: %d)\n", version, QVM_VERSION);
        fclose(f); return 0;
    }

    uint32_t pool_count, inst_count, func_count;
    if (!qr_u32_f(f, &pool_count) || !qr_u32_f(f, &inst_count) || !qr_u32_f(f, &func_count)) {
        fprintf(stderr, "qvm: failed to read counts\n");
        fclose(f); return 0;
    }
    qr_u32(f);

    if (pool_count > QVM_MAX_POOL || inst_count > QVM_MAX_INSTR || func_count > QVM_MAX_FUNCS) {
        fprintf(stderr, "qvm: counts out of range (pool=%u inst=%u func=%u)\n", pool_count, inst_count, func_count);
        fclose(f); return 0;
    }

    long min_size = QVM_HEADER_SIZE;

    min_size += (long)pool_count;

    min_size += (long)inst_count * 13;

    min_size += (long)func_count * 12;
    if (file_size < min_size) {
        fprintf(stderr, "qvm: truncated file (%ld < %ld)\n", file_size, min_size);
        fclose(f); return 0;
    }

    Value *pool = calloc(pool_count, sizeof(Value));
    if (!pool) { fprintf(stderr, "qvm: out of memory\n"); fclose(f); return 0; }
    for (uint32_t i = 0; i < pool_count; i++) {
        if (!qvm_read_value_f(f, &pool[i])) {
            fprintf(stderr, "qvm: failed to read pool entry %u\n", i);
            for (uint32_t j = 0; j < i; j++) free_value(pool[j]);
            free(pool); fclose(f); return 0;
        }
    }
    qvm->pool = pool;
    qvm->pool_count = (int)pool_count;

    qvm->main_prog = (BytecodeProgram){0};
    qvm->main_prog.instructions = calloc(inst_count, sizeof(Instruction));
    if (!qvm->main_prog.instructions && inst_count > 0) {
        fprintf(stderr, "qvm: out of memory\n");
        for (uint32_t i = 0; i < pool_count; i++) free_value(pool[i]);
        free(pool); fclose(f); return 0;
    }
    qvm->main_prog.count = inst_count;
    qvm->main_prog.const_count = (int)pool_count;
    qvm->main_prog.constants = calloc(pool_count, sizeof(Value));
    for (uint32_t i = 0; i < pool_count; i++)
        qvm->main_prog.constants[i] = clone_value(pool[i]);

    for (uint32_t i = 0; i < inst_count; i++) {
        uint8_t op; int32_t arg_i, ci, line;
        if (!qr_u8_f(f, &op) || !qr_i32_f(f, &arg_i) || !qr_i32_f(f, &ci) || !qr_i32_f(f, &line)) {
            fprintf(stderr, "qvm: truncated instruction stream at %u\n", i);
            qvm_free(qvm); fclose(f); return 0;
        }
        if (!qvm_valid_opcode(op)) {
            fprintf(stderr, "qvm: invalid opcode 0x%02X at instruction %u\n", op, i);
            qvm_free(qvm); fclose(f); return 0;
        }
        qvm->main_prog.instructions[i].op = (Opcode)op;
        qvm->main_prog.instructions[i].arg_i = arg_i;
        qvm->main_prog.instructions[i].line = line;
        if (ci >= 0 && (uint32_t)ci < pool_count)
            qvm->main_prog.instructions[i].arg = clone_value(pool[ci]);
        else
            qvm->main_prog.instructions[i].arg = make_null();
    }

    for (uint32_t i = 0; i < inst_count; i++) {
        Instruction *ins = &qvm->main_prog.instructions[i];
        switch (ins->op) {
            case OP_JUMP: case OP_JUMP_IF_FALSE: case OP_JUMP_IF_TRUE: case OP_TRY_PUSH:
                if (ins->arg_i < 0 || (uint32_t)ins->arg_i >= inst_count) {
                    fprintf(stderr, "qvm: jump target %d out of range at instruction %u\n", ins->arg_i, i);
                    qvm_free(qvm); fclose(f); return 0;
                }
                break;
            case OP_LOAD_CONST:
                if (ins->arg_i < 0 || (uint32_t)ins->arg_i >= pool_count) {
                    fprintf(stderr, "qvm: LOAD_CONST index %d out of range at instruction %u\n", ins->arg_i, i);
                    qvm_free(qvm); fclose(f); return 0;
                }
                break;
            case OP_LOAD_VAR: case OP_STORE_VAR:
                if (ins->arg.type != VALUE_STRING || !ins->arg.as.string) {
                    fprintf(stderr, "qvm: LOAD_VAR/STORE_VAR missing variable name at instruction %u\n", i);
                    qvm_free(qvm); fclose(f); return 0;
                }
                break;
            case OP_CALL_USER_FUNC: case OP_CALL_FUNC: case OP_CALL_METHOD: case OP_NEW:
            case OP_GET_FIELD: case OP_SET_FIELD:
                if (ins->arg.type != VALUE_STRING || !ins->arg.as.string) {
                    fprintf(stderr, "qvm: call/field instruction missing name at instruction %u\n", i);
                    qvm_free(qvm); fclose(f); return 0;
                }
                break;
            default:
                break;
        }
    }

    qvm->func_count = (int)func_count;
    qvm->funcs = calloc(func_count, sizeof(CompiledFunc));
    uint32_t *func_bcounts = calloc(func_count, sizeof(uint32_t));
    if ((!qvm->funcs && func_count > 0) || (!func_bcounts && func_count > 0)) {
        fprintf(stderr, "qvm: out of memory\n");
        qvm_free(qvm); fclose(f); return 0;
    }

    for (uint32_t fi = 0; fi < func_count; fi++) {
        uint32_t nlen;
        if (!qr_u32_f(f, &nlen)) {
            fprintf(stderr, "qvm: truncated function name length at func %u\n", fi);
            qvm_free(qvm); fclose(f); return 0;
        }
        if (nlen > QVM_MAX_NAME_LEN) {
            fprintf(stderr, "qvm: function name too long (%u bytes) at func %u\n", nlen, fi);
            qvm_free(qvm); fclose(f); return 0;
        }
        char *name = malloc(nlen + 1);
        if (!name) { fprintf(stderr, "qvm: out of memory\n"); qvm_free(qvm); fclose(f); return 0; }
        if (nlen > 0 && fread(name, 1, nlen, f) != nlen) {
            fprintf(stderr, "qvm: truncated function name at func %u\n", fi);
            free(name); qvm_free(qvm); fclose(f); return 0;
        }
        name[nlen] = '\0';

        uint32_t pcount, bcount;
        int32_t slot_count;
        if (!qr_u32_f(f, &pcount) || !qr_u32_f(f, &bcount) || !qr_i32_f(f, &slot_count)) {
            fprintf(stderr, "qvm: truncated function entry at func %u\n", fi);
            free(name); qvm_free(qvm); fclose(f); return 0;
        }
        if (pcount > 32) {
            fprintf(stderr, "qvm: function '%s' has too many parameters (%u)\n", name, pcount);
            free(name); qvm_free(qvm); fclose(f); return 0;
        }
        if (bcount > QVM_MAX_FUNC_BODY) {
            fprintf(stderr, "qvm: function '%s' body too large (%u instructions)\n", name, bcount);
            free(name); qvm_free(qvm); fclose(f); return 0;
        }
        if (slot_count < 0 || slot_count > 256) {
            fprintf(stderr, "qvm: function '%s' invalid slot_count %d\n", name, slot_count);
            free(name); qvm_free(qvm); fclose(f); return 0;
        }

        func_bcounts[fi] = bcount;
        qvm->funcs[fi].name = name;
        qvm->funcs[fi].param_count = (int)pcount;
        qvm->funcs[fi].slot_count = (int)slot_count;
        qvm->funcs[fi].param_names = calloc(pcount, sizeof(char*));
        if (!qvm->funcs[fi].param_names && pcount > 0) {
            fprintf(stderr, "qvm: out of memory\n");
            qvm_free(qvm); fclose(f); return 0;
        }
        for (uint32_t pi = 0; pi < pcount; pi++) {
            int32_t pidx;
            if (!qr_i32_f(f, &pidx)) {
                fprintf(stderr, "qvm: truncated parameter index at func %u param %u\n", fi, pi);
                qvm_free(qvm); fclose(f); return 0;
            }
            if (pidx >= 0 && (uint32_t)pidx < pool_count && pool[pidx].type == VALUE_STRING)
                qvm->funcs[fi].param_names[pi] = qs_strdup(pool[pidx].as.string);
            else
                qvm->funcs[fi].param_names[pi] = qs_strdup("?");
        }
    }

    for (uint32_t fi = 0; fi < func_count; fi++) {
        uint32_t bcount = func_bcounts[fi];
        qvm->funcs[fi].body = (BytecodeProgram){0};
        if (bcount > 0) {
            qvm->funcs[fi].body.instructions = calloc(bcount, sizeof(Instruction));
            if (!qvm->funcs[fi].body.instructions) {
                fprintf(stderr, "qvm: out of memory\n");
                qvm_free(qvm); fclose(f); return 0;
            }
        }
        qvm->funcs[fi].body.count = bcount;
        qvm->funcs[fi].body.const_count = (int)pool_count;
        qvm->funcs[fi].body.constants = calloc(pool_count, sizeof(Value));
        for (uint32_t i = 0; i < pool_count; i++)
            qvm->funcs[fi].body.constants[i] = clone_value(pool[i]);

        for (uint32_t bi = 0; bi < bcount; bi++) {
            uint8_t op; int32_t arg_i, ci, line;
            if (!qr_u8_f(f, &op) || !qr_i32_f(f, &arg_i) || !qr_i32_f(f, &ci) || !qr_i32_f(f, &line)) {
                fprintf(stderr, "qvm: truncated function body at func %u instr %u\n", fi, bi);
                qvm_free(qvm); fclose(f); return 0;
            }
            if (!qvm_valid_opcode(op)) {
                fprintf(stderr, "qvm: invalid opcode 0x%02X in function '%s' at %u\n", op, qvm->funcs[fi].name, bi);
                qvm_free(qvm); fclose(f); return 0;
            }
            qvm->funcs[fi].body.instructions[bi].op = (Opcode)op;
            qvm->funcs[fi].body.instructions[bi].arg_i = arg_i;
            qvm->funcs[fi].body.instructions[bi].line = line;
            if (ci >= 0 && (uint32_t)ci < pool_count)
                qvm->funcs[fi].body.instructions[bi].arg = clone_value(pool[ci]);
            else
                qvm->funcs[fi].body.instructions[bi].arg = make_null();
        }

        for (uint32_t bi = 0; bi < bcount; bi++) {
            Instruction *ins = &qvm->funcs[fi].body.instructions[bi];
            switch (ins->op) {
                case OP_JUMP: case OP_JUMP_IF_FALSE: case OP_JUMP_IF_TRUE: case OP_TRY_PUSH:
                    if (ins->arg_i < 0 || (uint32_t)ins->arg_i >= bcount) {
                        fprintf(stderr, "qvm: jump target %d out of range in '%s' at %u\n", ins->arg_i, qvm->funcs[fi].name, bi);
                        qvm_free(qvm); fclose(f); return 0;
                    }
                    break;
                case OP_LOAD_CONST:
                    if (ins->arg_i < 0 || (uint32_t)ins->arg_i >= pool_count) {
                        fprintf(stderr, "qvm: LOAD_CONST index %d out of range in '%s' at %u\n", ins->arg_i, qvm->funcs[fi].name, bi);
                        qvm_free(qvm); fclose(f); return 0;
                    }
                    break;
                case OP_LOAD_LOCAL: case OP_STORE_LOCAL:
                    if (ins->arg_i < 0 || ins->arg_i >= MAX_INLINE_SLOTS) {
                        fprintf(stderr, "qvm: local slot %d out of range in '%s' at %u\n", ins->arg_i, qvm->funcs[fi].name, bi);
                        qvm_free(qvm); fclose(f); return 0;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    free(func_bcounts);

    long pos = ftell(f);
    if (pos < file_size) {
        uint32_t plen;
        if (qr_u32_f(f, &plen)) {
            if (plen > 0 && plen < QVM_MAX_NAME_LEN) {
                char *sp = malloc(plen + 1);
                if (sp && fread(sp, 1, plen, f) == plen) {
                    sp[plen] = '\0';
                    qvm->source_path = sp;
                } else {
                    free(sp);
                }
            }
        }
    }

    fclose(f);
    return 1;
}

static QS_UNUSED_FN int run_qvm_file(const char *path) {
    QVMFile qvm = {0};
    if (!qvm_load(path, &qvm)) return 1;

    compiled_funcs_init();
    for (int i = 0; i < qvm.func_count; i++) {
        if (g_compiled_func_count >= MAX_COMPILED_FUNCS) break;
        CompiledFunc *dst = &g_compiled_funcs[g_compiled_func_count];
        dst->name = qs_strdup(qvm.funcs[i].name);
        dst->body = qvm.funcs[i].body;
        qvm.funcs[i].body = (BytecodeProgram){0};
        dst->param_count = qvm.funcs[i].param_count;
        dst->slot_count = qvm.funcs[i].slot_count;
        dst->param_names = calloc(dst->param_count, sizeof(char*));
        for (int p = 0; p < dst->param_count; p++)
            dst->param_names[p] = qs_strdup(qvm.funcs[i].param_names[p]);
        g_compiled_func_count++;
    }

    Environment env = {0};
    VM vm;
    vm_init(&vm, &qvm.main_prog, &env);
    g_vm_trace = g_trace_vm;
    if (g_vm_source_dir) { free(g_vm_source_dir); g_vm_source_dir = NULL; }
    if (qvm.source_path) g_vm_source_dir = path_dirname(qvm.source_path);

    Program src_prog = {0};
    int have_src = 0;
    Program *saved_g_program = g_program;
    if (qvm.source_path && load_program_file(qvm.source_path, &src_prog)) {
        g_program = &src_prog;
        strip_block_comments(&src_prog);
        ParseError ferr = {0};
        collect_functions(&src_prog, &ferr);
        have_src = 1;
    }
    int result = vm_execute(&vm);
    g_program = saved_g_program;
    if (have_src) free_program(&src_prog);
    if (vm.pending_exception) {
        fprintf(stderr, "%s\n", vm.exception_msg ? vm.exception_msg : "unhandled exception");
        result = 1;
    }
    free(vm.exception_msg);
    vm.exception_msg = NULL;
    g_vm_trace = 0;
    free_environment(&env);
    qvm_free(&qvm);
    return result;
}

static const char *qvm_value_repr(Value v) {
    static char buf[256];
    switch (v.type) {
        case VALUE_NULL: return "null";
        case VALUE_BOOL: return v.as.boolean ? "true" : "false";
        case VALUE_INTEGER: snprintf(buf, sizeof(buf), "%ld", v.as.integer); return buf;
        case VALUE_FLOAT: snprintf(buf, sizeof(buf), "%g", v.as.floating); return buf;
        case VALUE_STRING:
            if (v.as.string) {
                snprintf(buf, sizeof(buf), "\"%.100s%s\"", v.as.string,
                         strlen(v.as.string) > 100 ? "..." : "");
                return buf;
            }
            return "\"\"";
        default: return "<value>";
    }
}

static void qvm_disassemble_pool(Value *pool, int pool_count) {
    fprintf(stderr, "\nConstant Pool (%d entries):\n", pool_count);
    for (int i = 0; i < pool_count; i++) {
        fprintf(stderr, "  [%3d] %-6s %s\n", i,
                pool[i].type == VALUE_NULL ? "null" :
                pool[i].type == VALUE_INTEGER ? "int" :
                pool[i].type == VALUE_FLOAT ? "float" :
                pool[i].type == VALUE_BOOL ? "bool" :
                pool[i].type == VALUE_STRING ? "str" : "?",
                qvm_value_repr(pool[i]));
    }
}

static QS_UNUSED_FN void qvm_disassemble_func(CompiledFunc *func) {
    fprintf(stderr, "\nFunction: %s (params=%d, slots=%d, %zu instructions)\n",
            func->name, func->param_count, func->slot_count, func->body.count);

    if (func->param_count > 0) {
        fprintf(stderr, "  Parameters: ");
        for (int i = 0; i < func->param_count; i++) {
            if (i > 0) fprintf(stderr, ", ");
            fprintf(stderr, "%s", func->param_names[i]);
        }
        fprintf(stderr, "\n");
    }

    int *labels = calloc(func->body.count, sizeof(int));
    for (size_t i = 0; i < func->body.count; i++) {
        Instruction *ins = &func->body.instructions[i];
        if ((ins->op == OP_JUMP || ins->op == OP_JUMP_IF_FALSE || ins->op == OP_JUMP_IF_TRUE ||
             ins->op == OP_TRY_PUSH) && ins->arg_i >= 0 && (size_t)ins->arg_i < func->body.count) {
            labels[ins->arg_i] = 1;
        }
    }

    fprintf(stderr, "  ----+------------------+--------+------+----------------------------------------\n");
    fprintf(stderr, "  Addr| Instruction      |  Arg   | Line | Notes\n");
    fprintf(stderr, "  ----+------------------+--------+------+----------------------------------------\n");

    for (size_t i = 0; i < func->body.count; i++) {
        Instruction *ins = &func->body.instructions[i];

        if (labels[i]) {
            fprintf(stderr, "  L%zu:\n", i);
        }

        fprintf(stderr, "  %4zu| %-16s", i, opcode_name(ins->op));

        switch (ins->op) {
            case OP_LOAD_CONST:
                fprintf(stderr, "  [%3d]", ins->arg_i);
                if (ins->arg.type == VALUE_STRING && ins->arg.as.string)
                    fprintf(stderr, " = %s", qvm_value_repr(ins->arg));
                else if (ins->arg.type != VALUE_NULL)
                    fprintf(stderr, " = %s", qvm_value_repr(ins->arg));
                break;
            case OP_LOAD_VAR: case OP_STORE_VAR:
                if (ins->arg.type == VALUE_STRING && ins->arg.as.string)
                    fprintf(stderr, "  '%s'", ins->arg.as.string);
                else
                    fprintf(stderr, "  -");
                break;
            case OP_LOAD_LOCAL: case OP_STORE_LOCAL:
                fprintf(stderr, "  slot=%d", ins->arg_i);
                break;
            case OP_JUMP: case OP_JUMP_IF_FALSE: case OP_JUMP_IF_TRUE:
                fprintf(stderr, "  -> L%d", ins->arg_i);
                break;
            case OP_TRY_PUSH:
                fprintf(stderr, "  -> L%d", ins->arg_i);
                break;
            case OP_CALL_USER_FUNC: case OP_CALL_FUNC: case OP_CALL_METHOD:
                if (ins->arg.type == VALUE_STRING && ins->arg.as.string)
                    fprintf(stderr, "  '%s' argc=%d", ins->arg.as.string, ins->arg_i);
                else
                    fprintf(stderr, "  argc=%d", ins->arg_i);
                break;
            case OP_NEW:
                if (ins->arg.type == VALUE_STRING && ins->arg.as.string)
                    fprintf(stderr, "  '%s' argc=%d", ins->arg.as.string, ins->arg_i);
                else
                    fprintf(stderr, "  argc=%d", ins->arg_i);
                break;
            case OP_GET_FIELD: case OP_SET_FIELD:
                if (ins->arg.type == VALUE_STRING && ins->arg.as.string)
                    fprintf(stderr, "  '%s'", ins->arg.as.string);
                else
                    fprintf(stderr, "  -");
                break;
            case OP_BUILD_LIST: case OP_PRINT_MULTI: case OP_BUILD_STRING:
                fprintf(stderr, "  count=%d", ins->arg_i);
                break;
            case OP_BUILD_MAP:
                fprintf(stderr, "  pairs=%d", ins->arg_i / 2);
                break;
            default:
                if (ins->arg_i != 0)
                    fprintf(stderr, "  %d", ins->arg_i);
                else
                    fprintf(stderr, "  -");
                break;
        }

        fprintf(stderr, " | L%-4d", ins->line);

        fprintf(stderr, "\n");
    }

    fprintf(stderr, "  ----+------------------+--------+------+----------------------------------------\n");
    free(labels);
}

static void qvm_disassemble(QVMFile *qvm, const char *label) {
    fprintf(stderr, "\n");
    fprintf(stderr, "========================================================\n");
    fprintf(stderr, "  QVM Disassembly: %s\n", label);
    fprintf(stderr, "  Version: %d | Pool: %d | Instructions: %zu | Functions: %d\n",
            QVM_VERSION, qvm->pool_count, qvm->main_prog.count, qvm->func_count);
    if (qvm->source_path)
        fprintf(stderr, "  Source: %s\n", qvm->source_path);
    fprintf(stderr, "========================================================\n");

    if (qvm->pool_count > 0)
        qvm_disassemble_pool(qvm->pool, qvm->pool_count);

    fprintf(stderr, "\nMain Program (%zu instructions):\n", qvm->main_prog.count);

    int *labels = calloc(qvm->main_prog.count, sizeof(int));
    for (size_t i = 0; i < qvm->main_prog.count; i++) {
        Instruction *ins = &qvm->main_prog.instructions[i];
        if ((ins->op == OP_JUMP || ins->op == OP_JUMP_IF_FALSE || ins->op == OP_JUMP_IF_TRUE ||
             ins->op == OP_TRY_PUSH) && ins->arg_i >= 0 && (size_t)ins->arg_i < qvm->main_prog.count) {
            labels[ins->arg_i] = 1;
        }
    }

    fprintf(stderr, "  ----+------------------+--------+------+----------------------------------------\n");
    fprintf(stderr, "  Addr| Instruction      |  Arg   | Line | Notes\n");
    fprintf(stderr, "  ----+------------------+--------+------+----------------------------------------\n");

    for (size_t i = 0; i < qvm->main_prog.count; i++) {
        Instruction *ins = &qvm->main_prog.instructions[i];

        if (labels[i]) {
            fprintf(stderr, "  L%zu:\n", i);
        }

        fprintf(stderr, "  %4zu| %-16s", i, opcode_name(ins->op));

        switch (ins->op) {
            case OP_LOAD_CONST:
                fprintf(stderr, "  [%3d]", ins->arg_i);
                if (ins->arg.type != VALUE_NULL)
                    fprintf(stderr, " = %s", qvm_value_repr(ins->arg));
                break;
            case OP_LOAD_VAR: case OP_STORE_VAR:
                if (ins->arg.type == VALUE_STRING && ins->arg.as.string)
                    fprintf(stderr, "  '%s'", ins->arg.as.string);
                else
                    fprintf(stderr, "  -");
                break;
            case OP_LOAD_LOCAL: case OP_STORE_LOCAL:
                fprintf(stderr, "  slot=%d", ins->arg_i);
                break;
            case OP_JUMP: case OP_JUMP_IF_FALSE: case OP_JUMP_IF_TRUE:
                fprintf(stderr, "  -> L%d", ins->arg_i);
                break;
            case OP_TRY_PUSH:
                fprintf(stderr, "  -> L%d", ins->arg_i);
                break;
            case OP_CALL_USER_FUNC: case OP_CALL_FUNC: case OP_CALL_METHOD:
                if (ins->arg.type == VALUE_STRING && ins->arg.as.string)
                    fprintf(stderr, "  '%s' argc=%d", ins->arg.as.string, ins->arg_i);
                else
                    fprintf(stderr, "  argc=%d", ins->arg_i);
                break;
            case OP_NEW:
                if (ins->arg.type == VALUE_STRING && ins->arg.as.string)
                    fprintf(stderr, "  '%s' argc=%d", ins->arg.as.string, ins->arg_i);
                else
                    fprintf(stderr, "  argc=%d", ins->arg_i);
                break;
            case OP_GET_FIELD: case OP_SET_FIELD:
                if (ins->arg.type == VALUE_STRING && ins->arg.as.string)
                    fprintf(stderr, "  '%s'", ins->arg.as.string);
                else
                    fprintf(stderr, "  -");
                break;
            case OP_BUILD_LIST: case OP_PRINT_MULTI: case OP_BUILD_STRING:
                fprintf(stderr, "  count=%d", ins->arg_i);
                break;
            case OP_BUILD_MAP:
                fprintf(stderr, "  pairs=%d", ins->arg_i / 2);
                break;
            default:
                if (ins->arg_i != 0)
                    fprintf(stderr, "  %d", ins->arg_i);
                else
                    fprintf(stderr, "  -");
                break;
        }

        fprintf(stderr, " | L%-4d", ins->line);
        fprintf(stderr, "\n");
    }

    fprintf(stderr, "  ----+------------------+--------+------+----------------------------------------\n");
    free(labels);

    if (qvm->func_count > 0) {
        fprintf(stderr, "\nFunction Table (%d functions):\n", qvm->func_count);
        fprintf(stderr, "  ----+------------------+--------+------+--------+-----------------------------------\n");
        fprintf(stderr, "  Idx | Name             | Params | Slots | Body   | Parameters\n");
        fprintf(stderr, "  ----+------------------+--------+------+--------+-----------------------------------\n");
        for (int i = 0; i < qvm->func_count; i++) {
            CompiledFunc *func = &qvm->funcs[i];
            fprintf(stderr, "  %3d | %-16s | %6d | %4d | %6zu | ",
                    i, func->name, func->param_count, func->slot_count, func->body.count);
            for (int p = 0; p < func->param_count; p++) {
                if (p > 0) fprintf(stderr, ", ");
                fprintf(stderr, "%s", func->param_names[p]);
            }
            fprintf(stderr, "\n");
        }
        fprintf(stderr, "  ----+------------------+--------+------+--------+-----------------------------------\n");
    }

    for (int fi = 0; fi < qvm->func_count; fi++) {
        CompiledFunc *func = &qvm->funcs[fi];
        if (func->body.count == 0) continue;

        fprintf(stderr, "\nFunction: %s (params=%d, slots=%d, %zu instructions)\n",
                func->name, func->param_count, func->slot_count, func->body.count);

        if (func->param_count > 0) {
            fprintf(stderr, "  Parameters: ");
            for (int p = 0; p < func->param_count; p++) {
                if (p > 0) fprintf(stderr, ", ");
                fprintf(stderr, "%s", func->param_names[p]);
            }
            fprintf(stderr, "\n");
        }

        int *flabels = calloc(func->body.count, sizeof(int));
        for (size_t i = 0; i < func->body.count; i++) {
            Instruction *ins = &func->body.instructions[i];
            if ((ins->op == OP_JUMP || ins->op == OP_JUMP_IF_FALSE || ins->op == OP_JUMP_IF_TRUE ||
                 ins->op == OP_TRY_PUSH) && ins->arg_i >= 0 && (size_t)ins->arg_i < func->body.count) {
                flabels[ins->arg_i] = 1;
            }
        }

        fprintf(stderr, "  ----+------------------+--------+------+----------------------------------------\n");
        fprintf(stderr, "  Addr| Instruction      |  Arg   | Line | Notes\n");
        fprintf(stderr, "  ----+------------------+--------+------+----------------------------------------\n");

        for (size_t i = 0; i < func->body.count; i++) {
            Instruction *ins = &func->body.instructions[i];

            if (flabels[i]) {
                fprintf(stderr, "  L%zu:\n", i);
            }

            fprintf(stderr, "  %4zu| %-16s", i, opcode_name(ins->op));

            switch (ins->op) {
                case OP_LOAD_CONST:
                    fprintf(stderr, "  [%3d]", ins->arg_i);
                    if (ins->arg.type != VALUE_NULL)
                        fprintf(stderr, " = %s", qvm_value_repr(ins->arg));
                    break;
                case OP_LOAD_VAR: case OP_STORE_VAR:
                    if (ins->arg.type == VALUE_STRING && ins->arg.as.string)
                        fprintf(stderr, "  '%s'", ins->arg.as.string);
                    else
                        fprintf(stderr, "  -");
                    break;
                case OP_LOAD_LOCAL: case OP_STORE_LOCAL:
                    fprintf(stderr, "  slot=%d", ins->arg_i);
                    break;
                case OP_JUMP: case OP_JUMP_IF_FALSE: case OP_JUMP_IF_TRUE:
                    fprintf(stderr, "  -> L%d", ins->arg_i);
                    break;
                case OP_TRY_PUSH:
                    fprintf(stderr, "  -> L%d", ins->arg_i);
                    break;
                case OP_CALL_USER_FUNC: case OP_CALL_FUNC: case OP_CALL_METHOD:
                    if (ins->arg.type == VALUE_STRING && ins->arg.as.string)
                        fprintf(stderr, "  '%s' argc=%d", ins->arg.as.string, ins->arg_i);
                    else
                        fprintf(stderr, "  argc=%d", ins->arg_i);
                    break;
                case OP_NEW:
                    if (ins->arg.type == VALUE_STRING && ins->arg.as.string)
                        fprintf(stderr, "  '%s' argc=%d", ins->arg.as.string, ins->arg_i);
                    else
                        fprintf(stderr, "  argc=%d", ins->arg_i);
                    break;
                case OP_GET_FIELD: case OP_SET_FIELD:
                    if (ins->arg.type == VALUE_STRING && ins->arg.as.string)
                        fprintf(stderr, "  '%s'", ins->arg.as.string);
                    else
                        fprintf(stderr, "  -");
                    break;
                case OP_BUILD_LIST: case OP_PRINT_MULTI: case OP_BUILD_STRING:
                    fprintf(stderr, "  count=%d", ins->arg_i);
                    break;
                case OP_BUILD_MAP:
                    fprintf(stderr, "  pairs=%d", ins->arg_i / 2);
                    break;
                default:
                    if (ins->arg_i != 0)
                        fprintf(stderr, "  %d", ins->arg_i);
                    else
                        fprintf(stderr, "  -");
                    break;
            }

            fprintf(stderr, " | L%-4d", ins->line);
            fprintf(stderr, "\n");
        }

        fprintf(stderr, "  ----+------------------+--------+------+----------------------------------------\n");
        free(flabels);
    }

    fprintf(stderr, "\n========================================================\n");
    fprintf(stderr, "  End of disassembly\n");
    fprintf(stderr, "========================================================\n\n");
}
