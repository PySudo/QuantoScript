func test(x:map) {
    keys = keys(x)
    return len(keys)
}
result = test({"a": 1, "b": 2})
if result == 2 {
    print("PASS: type map - accepted, keys len=", result)
} else {
    print("FAIL: type map - expected 2, got ", result)
}