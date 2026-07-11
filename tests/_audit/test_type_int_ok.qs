func test(x:int) {
    return x * 2
}
result = test(42)
if result == 84 {
    print("PASS: type int - 42 accepted, result=", result)
} else {
    print("FAIL: type int - expected 84, got ", result)
}