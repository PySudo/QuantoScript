func test(x:float) {
    return x + 1.0
}
result = test(3.14)
if result >= 4.13 {
    if result <= 4.15 {
        print("PASS: type float - 3.14 accepted, result=", result)
    } else {
        print("FAIL: type float - too large ", result)
    }
} else {
    print("FAIL: type float - too small ", result)
}