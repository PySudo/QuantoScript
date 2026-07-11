from "stdlib/math.qs" import min, max, clamp
r1 = min([3, 1, 2])
r2 = max([3, 1, 2])
r3 = clamp(5, 1, 3)
r4 = clamp(0.5, 0.0, 1.0)
ok = true
if r1 != 1 {
    ok = false
}
if r2 != 3 {
    ok = false
}
if r3 != 3 {
    ok = false
}
if r4 != 0.5 {
    ok = false
}
if ok {
    print("PASS: math - min/max/clamp all correct")
} else {
    print("FAIL: math - got min=", r1, " max=", r2, " clamp(5,1,3)=", r3, " clamp(0.5,0,1)=", r4)
}