# QVM Validation and Error Handling

**Status**: Production-ready  
**Last Updated**: 2026-07-09

---

## Overview

The QVM loader performs comprehensive validation to ensure safe execution. All malformed, truncated, or corrupted files are rejected with clear error messages. The loader **never crashes** — it always returns a clean error.

## Validation Layers

### 1. File-Level Checks

| Check | Error Message | Condition |
|-------|---------------|-----------|
| File exists | `Cannot open '<path>'` | File not found |
| File size | `file too small for header (<n> bytes)` | Size < 32 bytes |
| Magic number | `not a valid .qvm file (bad magic 0x<hex>)` | First 4 bytes ≠ `0x314D5651` |
| Version | `unsupported version <n> (max: 4)` | Version > 4 |
| Counts | `counts out of range (pool=<n> inst=<n> func=<n>)` | Any count exceeds limit |
| Truncation | `truncated file (<n> < <n>)` | File smaller than minimum size |

### 2. Constant Pool Validation

| Check | Error Message | Condition |
|-------|---------------|-----------|
| Read failure | `failed to read pool entry <n>` | fread returns < expected bytes |
| String length | `string too long (<n> bytes)` | String > 1 MB |
| Memory | `out of memory` | calloc/malloc fails |

### 3. Instruction Validation

| Check | Error Message | Condition |
|-------|---------------|-----------|
| Read failure | `truncated instruction stream at <n>` | fread returns < expected bytes |
| Opcode | `invalid opcode 0x<hex> at instruction <n>` | Opcode > 44 (OP_SET_FIELD) |
| Jump target | `jump target <n> out of range at instruction <n>` | target < 0 or ≥ instruction count |
| LOAD_CONST | `LOAD_CONST index <n> out of range at instruction <n>` | index < 0 or ≥ pool count |
| Variable name | `LOAD_VAR/STORE_VAR missing variable name at instruction <n>` | arg is not a string |
| Call name | `call/field instruction missing name at instruction <n>` | arg is not a string |

### 4. Function Table Validation

| Check | Error Message | Condition |
|-------|---------------|-----------|
| Name length | `function name too long (<n> bytes) at func <n>` | name_len > 4096 |
| Name read | `truncated function name at func <n>` | fread returns < name_len |
| Param count | `function '<name>' has too many parameters (<n>)` | param_count > 32 |
| Body size | `function '<name>' body too large (<n> instructions)` | body_count > 65536 |
| Slot count | `function '<name>' invalid slot_count <n>` | slot_count < 0 or > 256 |
| Param index | `truncated parameter index at func <n> param <n>` | fread returns < 4 bytes |

### 5. Function Body Validation

| Check | Error Message | Condition |
|-------|---------------|-----------|
| Read failure | `truncated function body at func <n> instr <n>` | fread returns < expected bytes |
| Opcode | `invalid opcode 0x<hex> in function '<name>' at <n>` | Opcode > 44 |
| Jump target | `jump target <n> out of range in '<name>' at <n>` | target < 0 or ≥ body count |
| LOAD_CONST | `LOAD_CONST index <n> out of range in '<name>' at <n>` | index < 0 or ≥ pool count |
| Local slot | `local slot <n> out of range in '<name>' at <n>` | slot < 0 or ≥ 8 (MAX_INLINE_SLOTS) |

## Error Output Format

All errors are written to stderr with the prefix `qvm:`:

```
qvm: not a valid .qvm file (bad magic 0x00000000)
qvm: unsupported version 99 (max: 4)
qvm: truncated file (21 < 32)
qvm: invalid opcode 0xFF at instruction 5
qvm: jump target 999 out of range at instruction 10
```

## Memory Safety

### Allocation Failure

All `malloc`/`calloc` calls are checked. On failure:
- Error message: `out of memory`
- All previously allocated memory is freed
- Function returns 0 (failure)

### Double-Free Prevention

The `qvm_free()` function uses a zeroing pattern:
```c
static void qvm_free(QVMFile *qvm) {
    // ... free all resources ...
    memset(qvm, 0, sizeof(*qvm));
}
```

### Resource Cleanup

On any validation failure during loading:
1. All partially-loaded resources are freed
2. The QVMFile struct is zeroed
3. The function returns 0

## Bounds Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| `QVM_MAX_POOL` | 65536 | Maximum constant pool entries |
| `QVM_MAX_INSTR` | 65536 | Maximum instructions per program |
| `QVM_MAX_FUNCS` | 256 | Maximum compiled functions |
| `QVM_MAX_FUNC_BODY` | 65536 | Maximum instructions per function |
| `QVM_MAX_NAME_LEN` | 4096 | Maximum function name length |
| `QVM_MAX_STRING_LEN` | 1048576 (1 MB) | Maximum string constant length |
| `MAX_INLINE_SLOTS` | 8 | Maximum local variable slots per frame |

## Testing

### Malformed File Test Suite

| Test | Input | Expected |
|------|-------|----------|
| Invalid magic | File with wrong first 4 bytes | Error: bad magic |
| Wrong version | Version field = 99 | Error: unsupported version |
| Truncated file | Header only (32 bytes) | Error: truncated file |
| Corrupted pool | Random bytes in constant pool | Error or graceful null |
| Empty file | 0 bytes | Error: file too small |
| Missing file | Non-existent path | Error: cannot open |
| Non-QVM file | Random data (≥32 bytes) | Error: bad magic |

### Verification Commands

```bash
# Test valid .qvm execution
qs build test.qs -o test.qvm
qs vm test.qvm

# Test malformed file handling
echo "random data" > bad.qvm
qs vm bad.qvm  # Should print error and exit 1

# Test truncated file
dd if=test.qvm of=trunc.qvm bs=1 count=16
qs vm trunc.qvm  # Should print error and exit 1
```

## Future Considerations

1. **CRC32 Validation**: Currently computed but not validated on load. Could be added as an optional strict mode.

2. **Opcode Extension**: New opcodes can be added by extending the enum. Old loaders will reject new opcodes with "invalid opcode" error.

3. **Pool Extension**: New value types can be added by extending the tag byte. Old loaders treat unknown tags as null.

4. **Header Extension**: The `reserved` field (4 bytes) is available for future header extensions without breaking backward compatibility.
