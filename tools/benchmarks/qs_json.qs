func bench() {
    result = "["
    i = 0
    while i < 5000 {
        v = i * 3
        if i > 0 {
            result = result + ","
        }
        result = result + str(i) + ":" + str(v)
        i = i + 1
    }
    result = result + "]"
    return len(result)
}
print(bench())
