func bench() {
    t = 0
    i = 0
    while i < 10000 {
        x = (i * 7 + 13) % 97
        s = str(x)
        t = t + len(s)
        if i % 100 == 0 {
            j = 0
            while j < x and j < 100 {
                t = t + j
                j = j + 1
            }
        }
        i = i + 1
    }
    return t
}
print(bench())
