func test(x:map) {
    return "got map"
}
result = test({"a": 1})
if result == "got map" {
    print("PASS: type map - {} accepted")
} else {
    print("FAIL: type map")
}