func test(x:null) {
    return "was null"
}
result = test(null)
if result == "was null" {
    print("PASS: type null - null accepted, result=", result)
} else {
    print("FAIL: type null - expected 'was null', got ", result)
}