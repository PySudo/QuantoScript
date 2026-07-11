func test(x) {
    return isinstance(x, "int")
}
r1 = test(42)
r2 = test("hi")
r3 = test([1])
r4 = test({"a": 1})
r5 = test(null)
ok = true
if not r1 {
    ok = false
}
if r2 {
    ok = false
}
if r3 {
    ok = false
}
if r4 {
    ok = false
}
if r5 {
    ok = false
}
if ok {
    print("PASS: type untyped (any) - accepts all types, isinstance checks work")
} else {
    print("FAIL: type any")
}