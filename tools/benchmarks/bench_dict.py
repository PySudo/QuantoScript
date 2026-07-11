import time

def bench():
    d = {}
    for i in range(20000):
        d["key"] = 42
    total = 0
    for i in range(20000):
        total = total + d["key"]
    return total

for _ in range(2): bench()
times = []
for _ in range(5):
    start = time.perf_counter()
    result = bench()
    elapsed = (time.perf_counter() - start) * 1000
    times.append(elapsed)
avg = sum(times) / len(times)
print(f"Dict: {avg:.1f}ms")
