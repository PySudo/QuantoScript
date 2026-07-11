func test(x:bool) {
    if x {
        return "yes"
    } else {
        return "no"
    }
}
result = test(true)
if result == "yes" {
    print("PASS: type bool - true accepted, result=", result)
} else {
    print("FAIL: type bool - expected 'yes', got ", result)
}