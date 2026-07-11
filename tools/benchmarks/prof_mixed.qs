# Mixed workload - combines arithmetic, strings, lists, function calls
func process(n) {
    result = 0
    repeat n {
        result = result + 1
    }
    return result
}

total = 0
repeat 1000 {
    total = total + process(100)
}
print("mixed result: ", total)
