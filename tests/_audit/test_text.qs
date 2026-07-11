from "stdlib/text.qs" import lower, upper, trim, split
r1 = lower("HELLO")
r2 = upper("hello")
r3 = trim("  spaces  ")
r4 = split("a,b,c", ",")
ok = true
if r1 != "hello" {
    ok = false
}
if r2 != "HELLO" {
    ok = false
}
if r3 != "spaces" {
    ok = false
}
if r4[1] != "a" {
    ok = false
}
if r4[2] != "b" {
    ok = false
}
if r4[3] != "c" {
    ok = false
}
if ok {
    print("PASS: text - lower/upper/trim/split all correct")
} else {
    print("FAIL: text - got lower=", r1, " upper=", r2, " trim=", r3)
}