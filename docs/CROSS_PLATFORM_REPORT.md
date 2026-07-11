# CROSS_PLATFORM_REPORT.md

## Build Results

| Platform | Compiler | Status | Warnings |
|----------|----------|--------|----------|
| Windows (MSYS2/MinGW) | GCC 13.2.0 | PASS | fread unused-result (harmless) |
| Linux (WSL Ubuntu 24.04) | GCC 13.3.0 | PASS | fread unused-result, format-truncation (harmless) |

## Test Results

### Windows
```
test_final.qs              PASS
test_exceptions.qs         PASS
test_index.qs              PASS
test_stress_recursion.qs   PASS
test_websocket.qs          PASS
classes.qs (VM)            PASS
```
**6/6 tests pass**

### Linux (WSL)
```
test_final.qs              PASS
test_exceptions.qs         PASS
test_index.qs              PASS
test_stress_recursion.qs   PASS
test_websocket.qs          PASS
classes.qs (VM)            PASS
```
**6/6 tests pass**

## Feature Matrix by Platform

| Feature | Windows | Linux | Notes |
|---------|---------|-------|-------|
| Parser | PASS | PASS | |
| Interpreter | PASS | PASS | |
| Bytecode VM | PASS | PASS | |
| Native Compiler | N/A | N/A | Not tested (requires gcc in PATH) |
| REPL | PASS | PASS | Interactive testing |
| Functions | PASS | PASS | |
| Classes (VM only) | PASS | PASS | |
| Exceptions | PASS | PASS | |
| HTTP GET | PASS | PASS | Windows: WinHTTP+OpenSSL, Linux: raw TCP+OpenSSL |
| HTTP POST | PASS | PASS | |
| HTTPS GET | PASS | PASS | Both use OpenSSL |
| WebSocket | PASS | PASS | Fixed: base64 key, frame masking |
| File I/O | PASS | PASS | |
| Imports | PASS | PASS | |
| Logging | PASS | PASS | |

## Bugs Fixed During Cross-Platform Audit

1. **Missing `-lm` on Linux** — math library not linked. Added to Makefile and platform.inc.
2. **Windows-specific socket error codes** — `WSAGetLastError`, `WSAEWOULDBLOCK`, `DWORD` used directly. Added cross-platform abstractions: `qs_socket_errno()`, `QS_WOULDBLOCK`, `QS_IN_PROGRESS`, `QS_TIMEDOUT`, `qs_timeout_t`.
3. **ws_set_timeout used DWORD on Linux** — `setsockopt` expects `struct timeval` on POSIX. Added `#ifdef _WIN32` guard.
4. **WebSocket key not base64-encoded** — Generated random ASCII chars instead of proper base64. Fixed to generate 16 random bytes and base64-encode per RFC 6455.
5. **Client WebSocket frames not masked** — RFC 6455 requires all client-to-server frames to be masked. Added 4-byte random mask to ws_send and ws_close.
6. **ws_close sent unmasked close frame** — Same masking fix applied to close frame.
7. **ws_state returned "error" for closed connections** — After socket close, recv returns EBADF, not WOULDBLOCK. Changed to return "closed" for any unexpected error.

## Build Instructions

### Windows
```bash
gcc -std=c99 -O2 -I src src/quanto.c -o qs.exe -lssl -lcrypto -lws2_32 -lwinhttp -lwininet
```

### Linux
```bash
gcc -std=c99 -O2 -I src src/quanto.c -o qs -lssl -lcrypto -lpthread -ldl -lm
```

### Cross-platform (Make)
```bash
make          # auto-detects platform
make test     # runs regression suite
```

## Platform-Specific Code Locations

All platform-specific code is guarded by `#ifdef _WIN32` in:
- `platform.inc` — types, includes, socket abstractions
- `native.inc` — HTTP (WinHTTP vs OpenSSL), WebSocket, filesystem, Python embedding
- `cli.inc` — REPL, CLI subcommands (some Win32-specific path handling)

## Known Platform Differences

1. **Python embedding** — Only on Windows (requires `-DQS_PYTHON` and Python dev headers). Linux support is stubbed.
2. **Installer** — NSIS (Windows), shell script (Linux).
