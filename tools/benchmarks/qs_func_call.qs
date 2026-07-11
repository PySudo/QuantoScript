func bench() {
    s = 0
    i = 0
    while i < 100000 {
        s = s + 1
        i = i + 1
    }
    return s
}
print(bench())
