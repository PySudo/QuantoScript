func bench() {
    n = 1000
    m = {"a": 1, "b": 2, "c": 3, "d": 4, "e": 5}
    t = 0
    i = 0
    while i < n {
        t = t + m["a"]
        t = t + m["b"]
        t = t + m["c"]
        i = i + 1
    }
    return t
}
print(bench())
