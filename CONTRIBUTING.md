# Contributing to QuantoScript

## Building

```bash
make        # build
make test   # run regression suite
```

Requires:

- C99 compiler (GCC or Clang)
- OpenSSL development headers
- PowerShell 7 (`pwsh`) on Windows, Linux, or macOS

```bash
# Windows
pwsh tests/run_regression.ps1

# Linux
pwsh tests/run_regression.ps1

# macOS
pwsh tests/run_regression.ps1
```

## Running Tests

The regression suite is the single source of truth for all testing.

```bash
# Via make (recommended — auto-detects PowerShell)
make test

# Direct invocation
pwsh tests/run_regression.ps1            # all paths, all tests
pwsh tests/run_regression.ps1 -Verbose   # show all output
pwsh tests/run_regression.ps1 -StopOnFailure   # stop on first failure
pwsh tests/run_regression.ps1 -InterpreterOnly  # qs only
pwsh tests/run_regression.ps1 -VMOnly            # qs vm only
pwsh tests/run_regression.ps1 -QVMOnly           # qs build+run only
pwsh tests/run_regression.ps1 -Path tests/       # specific folder
pwsh tests/run_regression.ps1 -Path tests/test_final.qs  # single file
```

Every test runs on all three execution paths (qs, qs vm, qvm) and outputs are compared.

## Code Organization

- `src/quanto.c` — amalgamation entry point (don't edit directly)
- `src/parts/` — individual modules (edit these)
- `stdlib/` — standard library modules
- `examples/` — language examples
- `tests/` — regression test suite
- `tests/run_regression.ps1` — canonical test runner

## Adding Features

1. Create a feature branch
2. Write a failing regression test
3. Implement the fix or feature
4. Run `make test` (or `pwsh tests/run_regression.ps1`)
5. Submit a Pull Request

## Code Style

- C99 standard
- 4-space indentation in C source
- No trailing whitespace
- LF line endings

## Git Workflow

1. Checkout main
2. Pull the latest changes
3. Create a feature branch
4. Implement your change
5. Run the regression suite
6. Push the branch
7. Open a Pull Request
8. Merge after review
