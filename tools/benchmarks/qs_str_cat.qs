func bench() {
    s = ""
    i = 0
    while i < 100000 {
        s = s + "x"
        i = i + 1
    }
    return len(s)
}
print(bench())
