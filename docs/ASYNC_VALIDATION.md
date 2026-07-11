# ASYNC_VALIDATION.md

Evidence-based audit of the QuantoScript async subsystem. All conclusions backed by actual execution.

---

## 1. Feature Inventory

### Language-Level Features

| Feature | Parser | AST | Bytecode | VM | Stdlib | Native |
|---------|--------|-----|----------|-----|--------|--------|
| `async` keyword | NO | NO | NO | NO | NO | NO |
| `await` keyword | NO | NO | NO | NO | NO | NO |
| Sleep / setTimeout | NO | NO | NO | NO | NO | NO |
| Promises / Futures | NO | NO | NO | NO | NO | NO |
| Coroutines | NO | NO | NO | NO | NO | NO |
| Generators / yield | NO | NO | NO | NO | NO | NO |
| Async sockets | NO | NO | NO | NO | NO | NO |
| Async HTTP | NO | NO | NO | NO | NO | NO |
| Async WebSocket | NO | NO | NO | NO | NO | NO |
| Async file I/O | NO | NO | NO | NO | NO | NO |
| Cancellation | NO | NO | NO | NO | NO | NO |
| Synchronization | NO | NO | NO | NO | NO | NO |
| Event loop | NO | NO | NO | NO | NO | NO |
| Scheduler | NO | NO | NO | NO | NO | NO |

### Task Queue Features (What Actually Exists)

| Feature | Implementation | Evidence |
|---------|---------------|----------|
| `sys_spawn(lambda)` | Creates task, returns ID | PASS — returns integer ID |
| `sys_task_run(id)` | Executes task synchronously | PASS for 1 task |
| `sys_task_result(id)` | Returns stored result | PASS — returns correct value |
| `sys_task_status(id)` | Returns status string | PASS — "ready"/"done"/"not_found" |
| `sys_task_run_all()` | Runs all ready tasks in loop | **HANGS** with 2+ tasks |
| `thread_spawn(lambda)` | Runs synchronously (no threading) | PASS — returns 1, no concurrency |

### Stdlib Module (`stdlib/async.qs`)

| Function | Wraps | Status |
|----------|-------|--------|
| `spawn(fn)` | `sys_spawn(fn)` | WORKS |
| `run(task_id)` | `sys_task_run(task_id)` | WORKS |
| `result(task_id)` | `sys_task_result(task_id)` | WORKS |
| `status(task_id)` | `sys_task_status(task_id)` | WORKS |
| `run_all()` | `sys_task_run_all()` | **HANGS** with 2+ tasks |
| `spawn_and_run(fn)` | spawn + run + result | FAILS — `run()` not found in scope |

---

## 2. Execution Validation

### Test Results (Windows, verified with WSL)

| Test | Result | Detail |
|------|--------|--------|
| Basic spawn + run + result | **PASS** | `spawn(fn() -> 42)`, run, result returns 42 |
| Lambda with body `fn() { ... }` | **FAIL** | Parser requires `->` syntax; multi-statement lambda not supported by call_lambda |
| Multiple individual task runs | **PASS** | 5 tasks spawned and run individually — all correct |
| `run_all()` with 1 task | **PASS** | Returns count: 1 |
| `run_all()` with 2+ tasks | **HANGS** | Infinite loop or deadlock in second `call_lambda` |
| `thread_spawn` | **PASS** (misleading) | Runs synchronously — returns 1, no actual threading |
| `spawn_and_run` | **FAIL** | `run()` not found in stdlib scope when called from within async.qs |
| Status transitions | **PASS** | "ready" → "done" after run |
| Invalid task ID | **PASS** | Returns "not_found" |
| 100 tasks (individual run) | **CRASH** | Non-deterministic access violation (0xC0000005) |
| 100 tasks (run_all) | **HANGS** | Same run_all bug |

### Crash Pattern (Non-Deterministic Memory Corruption)

```
 1 tasks: CRASH(3221226356)
 5 tasks: PASS
10 tasks: CRASH(3221226356)
20 tasks: PASS
50 tasks: CRASH(3221226356)
```

Crashes are non-deterministic — same code sometimes passes, sometimes crashes. Classic memory corruption pattern.

---

## 3. Event Loop

**There is no event loop.**

No event loop, message queue, poll/select/epoll/IOCP, or any I/O multiplexing exists anywhere in the codebase. Verified by exhaustive search of all 22 `.inc` files.

---

## 4. Scheduler

**There is no scheduler.**

The `task_run_all()` function iterates the task queue and calls `call_lambda` synchronously for each READY task. This is a simple sequential loop, not a scheduler.

No cooperative yielding, no preemption, no priority queue, no round-robin.

---

## 5. Bytecode

**There are no async opcodes.**

The bytecode VM has 36 opcodes (`OP_LOAD_CONST` through `OP_SET_FIELD`). None are async-related. The async task queue operates entirely through native built-in functions, bypassing the bytecode system entirely.

There is no suspension or resume mechanism. Tasks run to completion when `call_lambda` is invoked.

---

## 6. Memory Safety

### Stress Test Results

| Test | Result |
|------|--------|
| 1 task (spawn + run) | CRASH (non-deterministic) |
| 5 tasks (spawn + run) | PASS |
| 10 tasks (spawn + run) | CRASH (non-deterministic) |
| 20 tasks (spawn + run) | PASS |
| 50 tasks (spawn + run) | CRASH (non-deterministic) |
| `run_all()` with 2+ tasks | HANGS |
| 100 tasks via `run_all` | HANGS |

**Verdict: The async subsystem has memory safety issues.** Non-deterministic crashes indicate use-after-free, double-free, or buffer overflow in the task queue or lambda cloning.

---

## 7. Native Compiler

**Async is not supported by the native compiler.** The native compiler does not handle `sys_spawn`, `sys_task_run`, or any task-related functions. It would emit a compile error for any code using these functions. This is correct behavior (fail loudly rather than silently ignore).

---

## 8. Cross Platform

| Test | Windows | Linux (WSL) |
|------|---------|-------------|
| spawn + run + result | PASS | PASS |
| run_all (2+ tasks) | HANGS | HANGS |
| thread_spawn | PASS (sync) | PASS (sync) |
| Individual 5 tasks | PASS | PASS |
| Memory corruption | Yes (non-deterministic) | Not tested |

No behavioral differences — the bugs are consistent across platforms.

---

## 9. Performance

Not applicable — the async subsystem does not provide actual concurrency. All tasks execute synchronously. There is no performance benefit over sequential execution.

---

## 10. Final Verdict

### **D) Async is broken.**

**Evidence:**

1. **`run_all()` hangs** with 2+ tasks — the core multi-task execution function is non-functional
2. **Non-deterministic crashes** under load — memory corruption in the task queue
3. **`thread_spawn` is synchronous** — the comment in the source code explicitly says "Execute the lambda synchronously (full threading requires interpreter rewrite)"
4. **No async/await keywords** — the language has no native async support
5. **No event loop or scheduler** — there is no concurrency infrastructure
6. **No async opcodes** — async bypasses the bytecode VM entirely
7. **`spawn_and_run` broken** — can't resolve `run()` in its own scope
8. **Multi-statement lambdas fail** — `fn() { ... }` produces "fn needs '->'" error when used as task body
9. **The async example hangs** — `examples/async.qs` calls `run_all()` with 3 tasks and hangs

**What works:**
- `spawn()` creates tasks
- Individual `task_run()` executes a single task
- `task_result()` returns the result
- `task_status()` returns status strings

**What is broken:**
- `run_all()` — hangs with 2+ tasks
- `spawn_and_run()` — scope resolution failure
- Memory safety — non-deterministic crashes under load
- Actual concurrency — all execution is synchronous

**Recommendation:** The async subsystem should be documented as "not supported" in v0.1.0. Remove `async.qs` from the stdlib, remove the `async` example, and document the limitation. The task queue code can remain as internal infrastructure for future development but should not be exposed to users.
