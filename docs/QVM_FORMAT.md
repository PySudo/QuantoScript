# QVM Binary Format Specification

**Version**: 4  
**Status**: Stable  
**Last Updated**: 2026-07-09

---

## Overview

`.qvm` is the compiled bytecode format for QuantoScript. It contains pre-compiled instructions, constant pools, and function metadata that can be loaded and executed by the QVM (QuantoScript Virtual Machine) without recompilation.

## File Structure

```
┌─────────────────────────────────────────┐
│ Header (32 bytes)                       │
├─────────────────────────────────────────┤
│ Constant Pool (variable size)           │
├─────────────────────────────────────────┤
│ Main Program Instructions               │
├─────────────────────────────────────────┤
│ Function Table Entries                  │
├─────────────────────────────────────────┤
│ Function Body Instructions              │
├─────────────────────────────────────────┤
│ Source Path (optional)                  │
└─────────────────────────────────────────┘
```

## Header Format

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | magic | `0x314D5651` — ASCII "QVM1" in little-endian |
| 4 | 4 | version | Format version (currently 4) |
| 8 | 4 | flags | Feature flags (see below) |
| 12 | 4 | crc | CRC32 of original source file |
| 16 | 4 | pool_count | Number of entries in constant pool |
| 20 | 4 | main_count | Number of main program instructions |
| 24 | 4 | func_count | Number of compiled functions |
| 28 | 4 | reserved | Reserved for future use (must be 0) |

**Total header size**: 32 bytes

### Flags

| Flag | Value | Description |
|------|-------|-------------|
| `QVM_FLAG_HAS_SOURCE` | `0x0001` | Source path is present at end of file |
| `QVM_FLAG_HAS_CRC` | `0x0002` | CRC32 field contains valid checksum |

## Constant Pool

The constant pool stores all constant values referenced by instructions. Values are stored sequentially with a type tag byte prefix.

### Value Encoding

| Tag | Type | Data Format |
|-----|------|-------------|
| `0x00` | null | (no data) |
| `0x01` | integer | `int64` (8 bytes, little-endian) |
| `0x02` | float | `float64` (8 bytes, IEEE 754) |
| `0x03` | boolean | `uint8` (0=false, 1=true) |
| `0x04` | string | `uint32` length + raw bytes (no null terminator) |

**Note**: Unknown tags are treated as null for forward compatibility.

## Instructions

Each instruction is 13 bytes:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | op | Opcode (see below) |
| 1 | 4 | arg_i | Integer argument (jump targets, counts, slot indices) |
| 5 | 4 | const_idx | Constant pool index for string argument, or -1 |
| 9 | 4 | line | Source line number for error messages |

### Opcodes

| Code | Name | Description |
|------|------|-------------|
| 0 | LOAD_CONST | Push constant from pool |
| 1 | LOAD_VAR | Push variable from environment |
| 2 | STORE_VAR | Store top-of-stack to variable |
| 3 | ADD | Addition / string concatenation |
| 4 | SUB | Subtraction |
| 5 | MUL | Multiplication / string repeat |
| 6 | DIV | Division |
| 7 | MOD | Modulo |
| 8 | NEG | Negate |
| 9 | EQUAL | Equality comparison |
| 10 | NOT_EQUAL | Inequality comparison |
| 11 | LESS | Less-than comparison |
| 12 | GREATER | Greater-than comparison |
| 13 | LESS_EQUAL | Less-or-equal comparison |
| 14 | GREATER_EQUAL | Greater-or-equal comparison |
| 15 | AND | Logical AND |
| 16 | OR | Logical OR |
| 17 | NOT | Logical NOT |
| 18 | PRINT | Print top-of-stack |
| 19 | JUMP | Unconditional jump |
| 20 | JUMP_IF_FALSE | Jump if top-of-stack is falsy |
| 21 | BUILD_LIST | Build list from N stack values |
| 22 | BUILD_MAP | Build map from N/2 key-value pairs |
| 23 | LOAD_INDEX | Index into list/string/map |
| 24 | STORE_INDEX | Store into list/map |
| 25 | CALL | Call function (reserved) |
| 26 | RETURN | Return from function |
| 27 | POP | Discard top-of-stack |
| 28 | DUP | Duplicate top-of-stack |
| 29 | HALT | Stop execution |
| 30 | CALL_METHOD | Call method on object |
| 31 | CALL_FUNC | Call built-in function |
| 32 | BUILD_STRING | Concatenate N values |
| 33 | PRINT_MULTI | Print N values space-separated |
| 34 | EVAL_STMT | Evaluate source text via tree-walk |
| 35 | SLICE | Slice obj[start:end] |
| 36 | JUMP_IF_TRUE | Jump if top-of-stack is truthy |
| 37 | CALL_USER_FUNC | Call user-defined function |
| 38 | TRY_PUSH | Push exception handler |
| 39 | CATCH_END | Pop exception handler |
| 40 | LOAD_LOCAL | Push local variable by slot index |
| 41 | STORE_LOCAL | Store to local variable by slot index |
| 42 | NEW | Create new object from class |
| 43 | GET_FIELD | Get object field |
| 44 | SET_FIELD | Set object field |

## Function Table

Each function entry contains:

| Field | Type | Description |
|-------|------|-------------|
| name_len | uint32 | Length of function name |
| name | bytes | Function name (UTF-8) |
| param_count | uint32 | Number of parameters |
| body_count | uint32 | Number of instructions in body |
| slot_count | int32 | Number of local variable slots |
| param_idx[i] | int32 | Pool index for each parameter name |

## Function Bodies

All function bodies are serialized sequentially after the function table. Each function's instructions use the same 13-byte format as main program instructions.

**Important**: For `LOAD_CONST` instructions in function bodies, the `arg_i` field contains the global pool index (remapped from local to global during serialization).

## Source Path (Optional)

If `QVM_FLAG_HAS_SOURCE` is set:

| Field | Type | Description |
|-------|------|-------------|
| path_len | uint32 | Length of source path |
| path | bytes | Source file path (UTF-8) |

## Versioning Policy

- **Major version change**: Incompatible format changes
- **Minor version change**: Backward-compatible additions (new opcodes, new value types)
- **Patch version change**: Bug fixes that don't affect format

Current version: **4** (added slot_count to function entries)

### Version History

| Version | Changes |
|---------|---------|
| 1 | Initial format |
| 2 | Added function parameter indices |
| 3 | Added slot_count, source path |
| 4 | Fixed slot_count serialization bug, added reserved header field |

## Example

```qscript
func add(a, b) {
    return a + b
}
print(add(3, 4))
```

Compiled to QVM:

```
Header:
  magic:    0x314D5651
  version:  4
  flags:    0x0003 (HAS_SOURCE | HAS_CRC)
  crc:      <crc32 of source>
  pool:     5 entries (add, a, b, 3, 4)
  main:     6 instructions
  func:     1 function

Constant Pool:
  [0] str "add"
  [1] str "a"
  [2] str "b"
  [3] int 3
  [4] int 4

Main Program:
  [0] LOAD_CONST  [0] = "add"     | L1
  [1] LOAD_CONST  [3] = 3         | L4
  [2] LOAD_CONST  [4] = 4         | L4
  [3] CALL_USER_FUNC "add" argc=2 | L4
  [4] PRINT       -               | L4
  [5] HALT        -               | L0

Function: add
  params: a, b
  slots: 2
  body:
    [0] LOAD_LOCAL slot=0 | L2
    [1] LOAD_LOCAL slot=1 | L2
    [2] ADD              | L2
    [3] RETURN           | L2
    [4] HALT             | L1
```
