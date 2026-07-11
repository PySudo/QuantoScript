func test(x:int|string) {
    if isinstance(x, "int") {
        return x * 2
    } else {
        return x + x
    }
}
r1 = test(21)
r2 = test("ab")
ok = true
if r1 != 42 {
    ok = false
}
if r2 != "abab" {
    ok = false
}
if ok {
    print("PASS: type int|string union - int=", r1, " string=", r2)
} else {
    print("FAIL: type int|string - r1=", r1, " r2=", r2)
}