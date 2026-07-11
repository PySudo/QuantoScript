from "stdlib/math.qs" import min, max, clamp
from "stdlib/json.qs" import parse, stringify
from "stdlib/text.qs" import upper, lower, trim, split
from "stdlib/log.qs" import info, warn, error
from "stdlib/random.qs" import seed, randint
from "stdlib/fs.qs" import exists, read, write, remove
from "stdlib/core.qs" import env
from "stdlib/time.qs" import now, year
from "stdlib/os.qs" import cwd

print("=== stdlib ===")
assert_eq(min([3, 7, 1]), 1)
assert_eq(max([3, 7, 1]), 7)
assert_eq(clamp(5, 1, 3), 3)
print("math: PASS")
s = stringify({"k": "v"})
p = parse(s)
assert_eq(p["k"], "v")
print("json: PASS")
assert_eq(upper("hello"), "HELLO")
assert_eq(lower("HELLO"), "hello")
assert_eq(trim("  hi  "), "hi")
print("text: PASS")
info("t")
warn("t")
error("t")
print("log: PASS")
seed(42)
assert_eq(randint(1, 100) >= 1, true)
print("random: PASS")
write("t.txt", "ok")
assert_eq(read("t.txt"), "ok")
remove("t.txt")
print("fs: PASS")
assert_eq(type(env("PATH")), "string")
print("core: PASS")
assert_eq(type(now()), "int")
assert_eq(type(year()), "int")
print("time: PASS")
assert_eq(type(cwd()), "string")
print("os: PASS")

print("=== types ===")
func int_test(x:int) {
    return x
}
func float_test(x:float) {
    return x
}
func string_test(x:string) {
    return x
}
func bool_test(x:bool) {
    return x
}
func list_test(x:list) {
    return x
}
func map_test(x:map) {
    return x
}
func union_test(x:int|string) {
    return x
}
func any_test(x) {
    return x
}
assert_eq(int_test(42), 42)
assert_eq(float_test(3.14), 3.14)
assert_eq(string_test("hi"), "hi")
assert_eq(bool_test(true), true)
assert_eq(list_test([1])[1], 1)
assert_eq(map_test({"k":"v"})["k"], "v")
assert_eq(union_test(42), 42)
assert_eq(union_test("hi"), "hi")
assert_eq(any_test(42), 42)
assert_eq(any_test("hi"), "hi")
print("params: PASS")

func bad(x:int) -> int {
    return "wrong"
}
assert_eq(type(bad(1)), "string")
print("return type: PASS")

class TC {
    inc(x:int) {
        return x + 1
    }
}
tc = TC()
assert_eq(tc.inc(5), 6)
print("class method type: PASS")

print("ALL PASSED")