func bench() {
    a = 0
    b = 1
    i = 2
    while i <= 100000 {
        t = a + b
        a = b
        b = t
        i = i + 1
    }
    return b
}
print(bench())
