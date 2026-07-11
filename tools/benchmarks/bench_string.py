import time

def bench():
    s = ""
    for i in range(50000):
        s = s + "x"
    return len(s)

for _ in range(2): bench()
times = []
for _ in range(5):
    start = time.perf_counter()
    result = bench()
    elapsed = (time.perf_counter() - start) * 1000
    times.append(elapsed)
avg = sum(times) / len(times)
print(f"String: {avg:.1f}ms")
