"""QuantoScript v0.1.0 Benchmark — Python equivalents.

Each function takes no arguments and returns a result.
All input sizes match the QS benchmark files exactly.
"""
import sys, json, random
sys.set_int_max_str_digits(0)

def fib_rec():
    def fib(n):
        if n <= 1: return n
        return fib(n-1) + fib(n-2)
    return fib(30)

def fib_iter():
    a, b = 0, 1
    for _ in range(200000):
        a, b = b, a + b
    return b

def primes():
    c = 0; n = 2
    while n <= 200000:
        isp = True; d = 2
        while d * d <= n:
            if n % d == 0: isp = False; break
            d += 1
        if isp: c += 1
        n += 1
    return c

def str_cat():
    s = ""
    for _ in range(500000): s += "x"
    return len(s)

def sort():
    import random as r
    r.seed(42)
    lst = [r.randint(0, 1000000) for _ in range(10000)]
    lst.sort()
    return lst[-1] + lst[0]

def dict_ops():
    m = {f"key_{i}": i*2 for i in range(100000)}
    return sum(m.values())

def json_ser():
    data = [{"id": i, "value": i*3} for i in range(5000)]
    return len(json.dumps(data))

def func_call():
    def noop(x): return x
    return sum(noop(i % 100) for i in range(1000000))

def recurse():
    def cd(n):
        if n <= 0: return 0
        return 1 + cd(n-1)
    return cd(100)

def matrix():
    n = 50
    A = [[(i*7+j*3)%100 for j in range(n)] for i in range(n)]
    B = [[(i*5+j*11)%100 for j in range(n)] for i in range(n)]
    C = [[sum(A[i][k]*B[k][j] for k in range(n)) for j in range(n)] for i in range(n)]
    return C[0][0] + C[n-1][n-1]

def mixed():
    t = 0
    for i in range(10000):
        x = (i*7+13) % 97
        t += len(str(x))
        if i % 100 == 0:
            t += sum(range(min(x, 100)))
    return t

BENCHMARKS = {
    "fib_rec": fib_rec, "fib_iter": fib_iter, "primes": primes,
    "str_cat": str_cat, "sort": sort, "dict_ops": dict_ops,
    "json_ser": json_ser, "func_call": func_call, "recurse": recurse,
    "matrix": matrix, "mixed": mixed,
}

if __name__ == "__main__":
    name = sys.argv[1]
    print(BENCHMARKS[name]())
