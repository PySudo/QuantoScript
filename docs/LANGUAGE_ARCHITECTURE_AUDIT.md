# LANGUAGE_ARCHITECTURE_AUDIT.md

Architecture audit for QuantoScript v0.1.0 — evidence-based findings.

---

## 1. Type System Status

### Function Parameters (WORKS)
```qscript
func add(a:int, b:int) {
    return a + b
}
print(add(3, 4))  # 7
add("hello", 4)   # SyntaxError: argument 'a' has the wrong type
```

### Union Types (WORKS)
```qscript
func concat(x:int|string, y:int|string) {
    return str(x) + str(y)
}
print(concat(1, 2))      # 12
print(concat("a", "b"))  # ab
```

### Nullable Types (WORKS)
```qscript
func test(x:int|null) {
    return x
}
print(test(42))    # 42
print(test(null))  # null
```

### Class Method Parameters (WORKS — Same Parser)
```qscript
class Foo {
    init(x:int) {
        self.x = x
    }
}
f = Foo(42)      # Works
Foo("hello")     # SyntaxError: argument 'x' has the wrong type
```

### Supported Type Annotations

| Annotation | Matches |
|------------|---------|
| `int`, `integer` | VALUE_INTEGER |
| `float`, `double`, `number` | VALUE_FLOAT, VALUE_INTEGER |
| `string`, `str` | VALUE_STRING |
| `bool`, `boolean` | VALUE_BOOL |
| `list` | VALUE_LIST |
| `map`, `dict` | VALUE_DICT |
| `null` | VALUE_NULL |
| `any` (default) | Everything |
| `type1\|type2` | Union of types |

### Class Parity Status

| Feature | Functions | Methods | Parity? |
|---------|-----------|---------|---------|
| Parameter types | Yes | Yes | EQUAL |
| Union types | Yes | Yes | EQUAL |
| Nullable types | Yes | Yes | EQUAL |
| Type error messages | Yes | Yes | EQUAL |
| Return type annotations | Not implemented | Not implemented | EQUAL (both missing) |
| Field declarations (var x:int) | N/A | Not implemented | N/A |
| Assignment type validation | Not implemented | Not implemented | EQUAL (both missing) |

**Conclusion:** Functions and methods share the exact same type infrastructure. The only missing features (return types, field declarations, assignment validation) are new language features that affect both equally.

---

## 2. Stdlib Isolation Status

### Architecture
```
User Code
    ↓ (can call)
Public Stdlib API (run, capture, cwd, chdir, exists)
    ↓ (can call)
Internal sys_* Functions (sys_run, sys_capture, sys_cwd, ...)
    ↓ (can call)
C Runtime (system(), popen(), getcwd(), chdir())
```

### Security Boundary

| Test | Result |
|------|--------|
| `sys_http_get("url")` from user code | BLOCKED (SyntaxError: unknown function) |
| `sys_cwd()` from user code | BLOCKED |
| `sys_run("cmd")` from user code | BLOCKED |
| `sys_capture("cmd")` from user code | BLOCKED |
| `from "stdlib/os.qs" import run` | WORKS |
| `run("echo hi")` via stdlib | WORKS |
| `from "stdlib/time.qs" import now` | WORKS |
| `now()` via stdlib | WORKS |

### Implementation

- `g_module_mode` flag: Set to 1 during `import_from_module()`, cleared after
- `g_native_access_depth`: Set to 1 when calling functions from stdlib-defined code
- `call_builtin()` gate: `sys_*` names require `g_native_access_depth > 0 OR g_module_mode > 0`
- User code gets `SyntaxError: unknown function 'sys_xxx'` for internal functions

---

## 3. Dependency Status

| Dependency | Build | Runtime | Status |
|------------|-------|---------|--------|
| OpenSSL | Required | Required | Always linked |
| Python | Optional | Requires `-DQS_PYTHON` | Not functional by default |
| pthread | Linux only | Required | Linked on Linux |
| WinSock2 | Windows only | Required | Linked on Windows |

---

## 4. Build Status

| Platform | Compiler | Status |
|----------|----------|--------|
| Windows (MSYS2) | GCC 13.2.0 | PASS |
| Linux (WSL Ubuntu 24.04) | GCC 13.3.0 | PASS |

---

## 5. Known Limitations

1. **Closures** with captured variables — inner functions return null
2. **Class method bodies** — must use multi-line syntax
3. **Return type annotations** — not implemented (use comments for documentation)
4. **Field type declarations** — fields are created dynamically via `self.field = value`
5. **Assignment type validation** — types checked only at function call boundaries
6. **Python embedding** — not functional by default
8. **Async/await** — task queue exists but broken (hangs, crashes)
9. **Native compiler** — only integer arithmetic workloads
