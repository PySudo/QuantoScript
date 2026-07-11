import time
import sys

def run_all():
    results = {}
    
    # Arithmetic
    def bench_arith():
        x = 0
        for i in range(500000):
            x = x + 1
        return x
    for _ in range(2): bench_arith()
    times = []
    for _ in range(5):
        start = time.perf_counter()
        bench_arith()
        times.append((time.perf_counter() - start) * 1000)
    results["arith"] = sum(times) / len(times)
    
    # Fibonacci
    def fib(n):
        if n <= 1: return n
        return fib(n-1) + fib(n-2)
    for _ in range(2): fib(25)
    times = []
    for _ in range(5):
        start = time.perf_counter()
        fib(25)
        times.append((time.perf_counter() - start) * 1000)
    results["fib"] = sum(times) / len(times)
    
    # Function calls
    def add(a, b): return a + b
    def bench_calls():
        total = 0
        for i in range(100000):
            total = add(total, 1)
        return total
    for _ in range(2): bench_calls()
    times = []
    for _ in range(5):
        start = time.perf_counter()
        bench_calls()
        times.append((time.perf_counter() - start) * 1000)
    results["calls"] = sum(times) / len(times)
    
    # String concat
    def bench_string():
        s = ""
        for i in range(50000):
            s = s + "x"
        return len(s)
    for _ in range(2): bench_string()
    times = []
    for _ in range(5):
        start = time.perf_counter()
        bench_string()
        times.append((time.perf_counter() - start) * 1000)
    results["string"] = sum(times) / len(times)
    
    # Dict
    def bench_dict():
        d = {}
        for i in range(20000):
            d["key"] = 42
        total = 0
        for i in range(20000):
            total = total + d["key"]
        return total
    for _ in range(2): bench_dict()
    times = []
    for _ in range(5):
        start = time.perf_counter()
        bench_dict()
        times.append((time.perf_counter() - start) * 1000)
    results["dict"] = sum(times) / len(times)
    
    # Mixed
    def process(n):
        result = 0
        for i in range(n):
            result = result + 1
        return result
    def bench_mixed():
        total = 0
        for i in range(1000):
            total = total + process(100)
        return total
    for _ in range(2): bench_mixed()
    times = []
    for _ in range(5):
        start = time.perf_counter()
        bench_mixed()
        times.append((time.perf_counter() - start) * 1000)
    results["mixed"] = sum(times) / len(times)
    
    return results

results = run_all()
for name, time_ms in results.items():
    print(f"{name}: {time_ms:.1f}ms")
