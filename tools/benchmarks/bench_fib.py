import time

def fib(n):
    if n <= 1: return n
    return fib(n-1) + fib(n-2)

def bench():
    return fib(25)

for _ in range(2): bench()
times = []
for _ in range(5):
    start = time.perf_counter()
    result = bench()
    elapsed = (time.perf_counter() - start) * 1000
    times.append(elapsed)
avg = sum(times) / len(times)
print(f"Fibonacci: {avg:.1f}ms")
