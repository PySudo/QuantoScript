"""QuantoScript vs CPython benchmark suite - identical workloads"""
import time
import sys
import json

def bench_arithmetic():
    """Arithmetic loop - 500K iterations"""
    x = 0
    for i in range(500000):
        x = x + 1
    return x

def bench_fib(n):
    """Recursive fibonacci"""
    if n <= 1:
        return n
    return bench_fib(n - 1) + bench_fib(n - 2)

def bench_func_calls():
    """Function call overhead - 100K calls"""
    def add(a, b):
        return a + b
    total = 0
    for i in range(100000):
        total = add(total, 1)
    return total

def bench_nested_recursion():
    """Nested recursion - compute via nested function calls"""
    def process(n):
        result = 0
        for i in range(n):
            result = result + 1
        return result
    total = 0
    for i in range(1000):
        total = total + process(100)
    return total

def bench_string_concat():
    """String concatenation - 50K iterations"""
    s = ""
    for i in range(50000):
        s = s + "x"
    return len(s)

def bench_dict_ops():
    """Dictionary operations"""
    d = {}
    for i in range(20000):
        d["key"] = 42
    total = 0
    for i in range(20000):
        total = total + d["key"]
    return total

def bench_mixed():
    """Mixed workload"""
    def process(n):
        result = 0
        for i in range(n):
            result = result + 1
        return result
    total = 0
    for i in range(1000):
        total = total + process(100)
    return total

def run_benchmark(name, func, args=None):
    """Run a benchmark and return timing"""
    # Warmup
    for _ in range(2):
        if args:
            func(*args)
        else:
            func()
    
    # Measure
    times = []
    for _ in range(5):
        start = time.perf_counter()
        if args:
            result = func(*args)
        else:
            result = func()
        elapsed = (time.perf_counter() - start) * 1000
        times.append(elapsed)
    
    avg = sum(times) / len(times)
    return avg, result

if __name__ == "__main__":
    print("=== CPython 3.11.0 Benchmarks ===")
    print(f"Python: {sys.version}")
    print()
    
    benchmarks = [
        ("Arithmetic Loop", bench_arithmetic, None),
        ("Recursive Fibonacci", bench_fib, (25,)),
        ("Function Calls", bench_func_calls, None),
        ("Nested Recursion", bench_nested_recursion, None),
        ("String Concat", bench_string_concat, None),
        ("Dictionary Ops", bench_dict_ops, None),
        ("Mixed Workload", bench_mixed, None),
    ]
    
    results = {}
    for name, func, args in benchmarks:
        avg, result = run_benchmark(name, func, args)
        results[name] = avg
        print(f"  {name}: {avg:.1f}ms (result: {result})")
    
    # Save results
    with open("cpython_results.json", "w") as f:
        json.dump(results, f, indent=2)
