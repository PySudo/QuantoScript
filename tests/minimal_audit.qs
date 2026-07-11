from "stdlib/math.qs" import min, max, clamp
assert_eq(min([3, 7, 1]), 1)
assert_eq(max([3, 7, 1]), 7)
assert_eq(clamp(5, 1, 3), 3)
print("math: PASS")
