import time

def process(n):
    result = 0
    for i in range(n):
        result = result + 1
    return result

def bench():
    total = 0
    for i in range(1000):
        total = total + process(100)
    return total

for _ in range(2): bench()
times = []
for _ in range(5):
    start = time.perf_counter()
    result = bench()
    elapsed = (time.perf_counter() - start) * 1000
    times.append(elapsed)
avg = sum(times) / len(times)
print(f"Nested: {avg:.1f}ms")
