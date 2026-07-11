# QuantoScript Benchmark Suite v0.1.0
# Canonical implementations matching benchmark.py exactly.
# Each function returns a result. Runner measures wall-clock time.

func bench_fib_rec() {
    def fib(n) {
        if n <= 1 { return n }
        return fib(n - 1) + fib(n - 2)
    }
    return fib(30)
}

func bench_fib_iter() {
    n = 200000
    a = 0
    b = 1
    i = 2
    while i <= n {
        tmp = a + b
        a = b
        b = tmp
        i = i + 1
    }
    return b
}

func bench_primes() {
    limit = 200000
    count = 0
    n = 2
    while n <= limit {
        isp = true
        d = 2
        while d * d <= n {
            if n % d == 0 {
                isp = false
                break
            }
            d = d + 1
        }
        if isp { count = count + 1 }
        n = n + 1
    }
    return count
}

func bench_string_concat() {
    n = 500000
    s = ""
    i = 0
    while i < n {
        s = s + "x"
        i = i + 1
    }
    return len(s)
}

func bench_string_search() {
    haystack = ("abcdefghij" * 100000)
    count = 0
    i = 0
    while i < len(haystack) {
        idx = haystack.find("xyz", i)
        if idx == -1 { break }
        count = count + 1
        i = idx + 1
    }
    present = 0
    i = 0
    while i < len(haystack) {
        idx = haystack.find("def", i)
        if idx == -1 { break }
        present = present + 1
        i = idx + 1
    }
    return count + present
}

func bench_list_ops() {
    n = 100000
    lst = []
    i = 0
    while i < n {
        lst = lst + [i]
        i = i + 1
    }
    total = 0
    i = 0
    while i < 10000 {
        idx = (i % len(lst)) + 1
        total = total + lst[idx]
        i = i + 1
    }
    s = 0
    i = 0
    while i < len(lst) {
        s = s + lst[i]
        i = i + 1
    }
    return len(lst) + total + (s % 1000)
}

func bench_sort() {
    n = 10000
    lst = []
    i = 0
    while i < n {
        lst = lst + [((i * 7 + 13) % 1000000)]
        i = i + 1
    }
    lst.sort()
    return lst[len(lst)] + lst[1]
}

func bench_dict_ops() {
    n = 100000
    m = {}
    i = 0
    while i < n {
        m["key_" + str(i)] = i * 2
        i = i + 1
    }
    total = 0
    i = 0
    while i < n {
        total = total + m["key_" + str(i)]
        i = i + 1
    }
    return total
}

func bench_json_ser() {
    parts = []
    i = 0
    while i < 5000 {
        parts = parts + ["{\"id\":" + str(i) + ",\"name\":\"item_" + str(i) + "\",\"value\":" + str(i * 3) + "}"  ]
        i = i + 1
    }
    s = "[" + ",".join(parts) + "]"
    return len(s)
}

func bench_json_de() {
    parts = []
    i = 0
    while i < 5000 {
        parts = parts + ["{\"id\":" + str(i) + ",\"value\":" + str(i * 3) + "}"]
        i = i + 1
    }
    s = "[" + ",".join(parts) + "]"
    return len(s)
}

func bench_func_calls() {
    def noop(x) { return x }
    n = 1000000
    s = 0
    i = 0
    while i < n {
        s = s + noop(i % 100)
        i = i + 1
    }
    return s
}

func bench_deep_recursion() {
    def countdown(n) {
        if n <= 0 { return 0 }
        return 1 + countdown(n - 1)
    }
    return countdown(5000)
}

func bench_matrix() {
    n = 50
    A = []
    i = 0
    while i < n {
        row = []
        j = 0
        while j < n {
            row = row + [((i * 7 + j * 3) % 100)]
            j = j + 1
        }
        A = A + [row]
        i = i + 1
    }
    B = []
    i = 0
    while i < n {
        row = []
        j = 0
        while j < n {
            row = row + [((i * 5 + j * 11) % 100)]
            j = j + 1
        }
        B = B + [row]
        i = i + 1
    }
    C = []
    i = 0
    while i < n {
        row = []
        j = 0
        while j < n {
            s = 0
            k = 0
            while k < n {
                s = s + A[i][k] * B[k][j]
                k = k + 1
            }
            row = row + [s]
            j = j + 1
        }
        C = C + [row]
        i = i + 1
    }
    return C[1][1] + C[n][n]
}

func bench_file_io() {
    n = 100
    path = "bench_io_tmp.txt"
    i = 0
    f = ""
    while i < n {
        f = f + "line " + str(i) + ": " + ("x" * 200) + "\n"
        i = i + 1
    }
    write(path, f)
    content = read(path)
    remove(path)
    return len(content)
}

func bench_regex() {
    text = ("hello world 123 test456 foo789 " * 10000)
    count = 0
    i = 0
    while i < len(text) {
        idx = text.find("123", i)
        if idx == -1 { break }
        count = count + 1
        i = idx + 1
    }
    count2 = 0
    i = 0
    while i < len(text) {
        idx = text.find("456", i)
        if idx == -1 { break }
        count2 = count2 + 1
        i = idx + 1
    }
    return count + count2
}

func bench_mixed() {
    total = 0
    i = 0
    while i < 10000 {
        x = (i * 7 + 13) % 97
        s = str(x)
        total = total + len(s)
        if i % 100 == 0 {
            j = 0
            while j < x and j < 100 {
                total = total + j
                j = j + 1
            }
        }
        i = i + 1
    }
    return total
}

print("fib_rec|" + str(bench_fib_rec()))
print("fib_iter|" + str(bench_fib_iter()))
print("primes|" + str(bench_primes()))
print("string_concat|" + str(bench_string_concat()))
print("string_search|" + str(bench_string_search()))
print("list_ops|" + str(bench_list_ops()))
print("sort|" + str(bench_sort()))
print("dict_ops|" + str(bench_dict_ops()))
print("json_ser|" + str(bench_json_ser()))
print("json_de|" + str(bench_json_de()))
print("func_calls|" + str(bench_func_calls()))
print("deep_recursion|" + str(bench_deep_recursion()))
print("matrix|" + str(bench_matrix()))
print("file_io|" + str(bench_file_io()))
print("regex|" + str(bench_regex()))
print("mixed|" + str(bench_mixed()))
