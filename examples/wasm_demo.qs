# WebAssembly demo.
#
# This program uses only the portable core of the language, so it produces the
# exact same output on every engine: the tree-walk interpreter (qs), the
# bytecode VM (qs vm), the native compiler (qs compile), and the WebAssembly
# build (node build/qs_node.js).
#
# Build and run it in WebAssembly:
#
#     make wasm
#     node build/qs_node.js examples/wasm_demo.qs
#
# The WebAssembly target has no live networking, FFI, POSIX regex, or process
# spawning; those builtins report a clear error if called. Everything below
# runs identically everywhere.

print("=== WebAssembly demo ===")

# --- Numbers and lists ---------------------------------------------------
func fib(n) {
    if n < 2 {
        return n
    }
    return fib(n - 1) + fib(n - 2)
}

print("fib(20) =", fib(20))

nums = []
i = 1
while i <= 10 {
    nums.append(i * i)
    i = i + 1
}
print("squares:", nums)

# --- Maps and optional chaining -----------------------------------------
user = {"name": "Ada", "roles": ["admin", "dev"]}
print("name:", user?.name)
print("missing:", user?.missing)
print("fallback:", user?.nickname ?? "anonymous")

# --- Strings -------------------------------------------------------------
words = ["web", "assembly", "demo"]
repeat w -> words {
    print("word:", w)
}

# --- try / oops across engines ------------------------------------------
try {
    oops("something went wrong")
} oops e {
    print("caught:", e)
}

# --- Higher-order functions ---------------------------------------------
double = fn(x) -> x * 2
print("doubled:", nums.map(double))

print("done")
