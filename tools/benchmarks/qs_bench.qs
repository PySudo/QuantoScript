# QuantoScript benchmark suite - identical workloads to Python

# Arithmetic loop - 500K iterations
func bench_arith() {
    x = 0
    repeat 500000 {
        x = x + 1
    }
    return x
}

# Recursive fibonacci
func bench_fib(n) {
    if n <= 1 { return n }
    return bench_fib(n - 1) + bench_fib(n - 2)
}

# Function call overhead - 100K calls
func bench_func_calls() {
    func add(a, b) { return a + b }
    total = 0
    repeat 100000 {
        total = add(total, 1)
    }
    return total
}

# Nested recursion
func bench_nested() {
    func process(n) {
        result = 0
        repeat n {
            result = result + 1
        }
        return result
    }
    total = 0
    repeat 1000 {
        total = total + process(100)
    }
    return total
}

# String concatenation - 50K iterations
func bench_string() {
    s = ""
    repeat 50000 {
        s = s + "x"
    }
    return s.len()
}

# Dictionary operations
func bench_dict() {
    d = {}
    repeat 20000 {
        d["key"] = 42
    }
    total = 0
    repeat 20000 {
        total = total + d["key"]
    }
    return total
}

# Mixed workload
func bench_mixed() {
    func process(n) {
        result = 0
        repeat n {
            result = result + 1
        }
        return result
    }
    total = 0
    repeat 1000 {
        total = total + process(100)
    }
    return total
}

# Run all benchmarks
print("=== QuantoScript Benchmarks ===")

t1 = bench_arith()
print("Arithmetic Loop: result=", t1)

t2 = bench_fib(25)
print("Recursive Fibonacci: result=", t2)

t3 = bench_func_calls()
print("Function Calls: result=", t3)

t4 = bench_nested()
print("Nested Recursion: result=", t4)

t5 = bench_string()
print("String Concat: result=", t5)

t6 = bench_dict()
print("Dictionary Ops: result=", t6)

t7 = bench_mixed()
print("Mixed Workload: result=", t7)
