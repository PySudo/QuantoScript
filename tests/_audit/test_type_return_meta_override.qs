func test(x): int {
    return "not an int"
}
result = test(1)
if result == "not an int" {
    print("PASS: return type NOT enforced - string returned from :int func")
} else {
    print("FAIL: return type was enforced (should be metadata only)")
}