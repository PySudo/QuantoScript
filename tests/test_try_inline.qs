# Single-line and mixed-layout try/oops must behave identically on all paths.

# Fully single-line, error variable bound
try { oops("one") } oops e { print("inline:", e) }

# Fully single-line, no error variable
try { oops("two") } oops { print("inline-no-var") }

# try inline, oops block multi-line
try { oops("three") } oops e {
    print("inline-try:", e)
}

# try block multi-line, oops inline
try {
    oops("four")
} oops e { print("inline-oops:", e) }

# No error thrown, so oops must not run
try { print("safe") } oops e { print("should not run:", e) }

print("done")
