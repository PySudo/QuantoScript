func test(x:list) {
    return len(x)
}
result = test([1, 2, 3])
if result == 3 {
    print("PASS: type list - [1,2,3] accepted, len=", result)
} else {
    print("FAIL: type list - expected 3, got ", result)
}