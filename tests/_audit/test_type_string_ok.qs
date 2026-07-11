func test(x:string) {
    return "hello " + x
}
result = test("hi")
if result == "hello hi" {
    print("PASS: type string - 'hi' accepted, result=", result)
} else {
    print("FAIL: type string - expected 'hello hi', got ", result)
}