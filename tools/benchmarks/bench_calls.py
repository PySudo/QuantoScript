import time

def add(a, b):
    return a + b

def bench():
    total = 0
    for i in range(100000):
        total = add(total, 1)
    return total

for _ in range(2): bench()
times = []
for _ in range(5):
    start = time.perf_counter()
    result = bench()
    elapsed = (time.perf_counter() - start) * 1000
    times.append(elapsed)
avg = sum(times) / len(times)
print(f"Calls: {avg:.1f}ms")
