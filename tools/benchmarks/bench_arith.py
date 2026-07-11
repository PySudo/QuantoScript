import time

def bench():
    x = 0
    for i in range(500000):
        x = x + 1
    return x

for _ in range(2): bench()
times = []
for _ in range(5):
    start = time.perf_counter()
    result = bench()
    elapsed = (time.perf_counter() - start) * 1000
    times.append(elapsed)
avg = sum(times) / len(times)
print(f"Arithmetic: {avg:.1f}ms")
