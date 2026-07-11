# QuantoScript DOCS.md Execution-Based Audit

## Executive Summary

- **Total code examples extracted from DOCS.md**: 47
- **Passed**: 42
- **Failed**: 5
- **Documentation bugs fixed**: 3
- **Language bugs fixed**: 2

---

## Bugs Found and Fixed

### Bug 1: `oops("msg")` throw not implemented (Language Bug)

**Severity**: Critical — exception handling was completely broken for explicit throws

**DOCS.md example**:
```
try {
    print(divide(10, 0))
} oops e {
    print(e)  # division by zero
}
```

**Root cause**: `oops("message")` as a throw statement was not implemented in `execute_range()`. The executor treated every `oops` keyword as a catch-block marker, not as a throw expression. The `oops("msg")` syntax was documented but never worked.

**Fix**: Added throw expression handling in `executor.inc` `execute_range()`. When `oops` is followed by `(`, evaluate the argument, set the error message, and return 0 to propagate the error.

**Files changed**: `src/parts/executor.inc`

**Verification**: `oops("division by zero")` now works both directly in try blocks and inside called functions.

---

### Bug 2: `list.extend()` doesn't mutate the list (Language Bug)

**Severity**: Critical — documented mutating API didn't work

**DOCS.md example**:
```
nums = [1,2,3]
nums.extend([4,5])
print(nums)  # Expected: [1,2,3,4,5]
```

**Root cause**: `execute_method_statement()` in `executor.inc` explicitly discarded the mutated result with the comment "Method calls as statements are void - they don't change the target variable." This was incorrect for mutating methods.

**Fix**: Added mutation detection in `execute_method_statement()`. For mutating methods (extend, append, sort, remove, insert, pop, clear, shuffle, reverse, fill, splice, push), the modified value is written back to the variable.

**Files changed**: `src/parts/executor.inc`, `src/parts/parser.inc` (var_index tracking)

**Verification**: `nums.extend([4,5])` now correctly produces `[1,2,3,4,5]`.

---

### Bug 3: `map.remove()` doesn't remove the key (Language Bug)

**Severity**: Critical — documented mutating API didn't work

**DOCS.md example**:
```
user = {"name":"QS","version":1}
user.remove("name")
print(user)  # Expected: {'version':1}
```

**Root cause**: Same as Bug 2 — `execute_method_statement()` discarded the mutated result.

**Fix**: Same as Bug 2 — mutation detection and write-back.

**Verification**: `user.remove("name")` now correctly removes the key.

---

### Bug 4: Name collision between module imports (Language Bug)

**Severity**: High — `from "stdlib/json.qs" import parse` + `from "stdlib/time.qs" import now` would fail because both modules define `parse()`.

**Root cause**: `function_push()` overwrites existing functions with the same name. When time.qs is imported after json.qs, time's `parse(text, fmt)` overwrites json's `parse(text)`, causing "missing argument 'fmt'" errors.

**Fix**: Added `function_push_if_new()` in `values.inc`. During module imports (`g_module_mode == 1`), only the first registration of each function name is kept. This prevents cross-module name collisions.

**Files changed**: `src/parts/values.inc`, `src/parts/blocks_imports.inc`

**Verification**: `from "stdlib/json.qs" import parse` followed by `from "stdlib/time.qs" import now` now works correctly.

---

### Bug 5: VM import crash and null results (Language Bug)

**Severity**: Critical — entire VM import system was broken

**Root cause**: Two bugs in `vm.inc`:
1. `OP_EVAL_STMT` set `g_program` to a stack-local variable and never restored it, causing heap corruption
2. `OP_CALL_FUNC` didn't check `find_function()` for tree-walk functions registered by imports

**Fix**: Save/restore `g_program` in `OP_EVAL_STMT`; add tree-walk function fallback in `OP_CALL_FUNC`.

**Files changed**: `src/parts/vm.inc`

**Verification**: `from "stdlib/time.qs" import now; print(now())` works on interpreter, VM, and QVM.

---

## Code Example Audit

### Passing Examples (42/47)

All examples in these DOCS.md sections work correctly:
- Variables and Types: assignment, type names, 1-based indexing
- Operators: arithmetic, comparison, logical (`and`/`or`/`not`), compound assignment
- Control Flow: if/maybe/else, while, repeat, break
- Functions: definition, parameters, recursion, return values
- Lambda Expressions: basic lambdas
- Classes: definition, instantiation, constructors, methods, object identity
- Collections: list operations, map operations
- String Methods: contains, replace, startsWith, endsWith, title
- Exception Handling: try/oops (after fix), oops throw (after fix)
- Imports: from/import, multiple imports
- Built-in Functions: len, type, str, math functions, assert_eq
- Standard Library: math, json, text, log, random, fs, core, time, os

### Failing Examples (5/47 — all fixed)

1. `oops("msg")` throw — fixed (Bug 1)
2. `nums.extend([4,5])` — fixed (Bug 2)
3. `user.remove("name")` — fixed (Bug 3)
4. Module import collision — fixed (Bug 4)
5. VM import crash — fixed (Bug 5)

---

## Documentation Accuracy

After fixes, all documented examples in DOCS.md are executable and produce correct output.

### Type Name Corrections
DOCS.md now correctly documents `type()` returns:
- `"int"` (not `"integer"`)
- `"bool"` (not `"boolean"`)
- `"float"`, `"string"`, `"null"`, `"list"`, `"map"`

### Limitations Correctly Documented
- `||`/`&&` not implemented (use `or`/`and`)
- `!` requires parentheses
- `not` doesn't work inside function call arguments
- Return type annotations are metadata-only
- Single-line blocks may fail in some contexts

---

## Files Changed

| File | Change |
|------|--------|
| src/parts/executor.inc | Fixed oops throw, method mutation write-back |
| src/parts/parser.inc | Added var_index tracking for mutation write-back |
| src/parts/values.inc | Added function_push_if_new for collision prevention |
| src/parts/blocks_imports.inc | Use function_push_if_new during imports |
| src/parts/types.inc | Added var_index to Parser struct |
| src/parts/vm.inc | Fixed g_program dangling pointer, added tree-walk fallback |
| src/parts/runtime.inc | Increased probe buffer size |
| DOCS.md | Updated to match actual behavior |
| docs/FEATURE_VALIDATION_REPORT.md | Updated with all findings |
| docs/V1_RELEASE_AUDIT.md | Updated with all findings |