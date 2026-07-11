func test(x): int {
    return x
}
result = test(42)
if result == 42 {
    print("PASS: return type annotation (: int) is metadata only, accepted int")
} else {
    print("FAIL: return type annotation")
}