from "stdlib/math.qs" import min, max, clamp
assert_eq(min([3, 7, 1]), 1)
assert_eq(max([3, 7, 1]), 7)
assert_eq(clamp(5, 1, 3), 3)
print("math: PASS")

from "stdlib/json.qs" import parse, stringify
data = {"k":"v"}
s = stringify(data)
p = parse(s)
assert_eq(p["k"], "v")
print("json: PASS")

from "stdlib/text.qs" import upper, lower, trim, split
assert_eq(upper("hello"), "HELLO")
assert_eq(lower("HELLO"), "hello")
assert_eq(trim("  hi  "), "hi")
parts = split("a,b,c", ",")
assert_eq(len(parts), 3)
print("text: PASS")

from "stdlib/log.qs" import info, warn, error
info("test")
warn("test")
error("test")
print("log: PASS")

from "stdlib/random.qs" import seed, randint, choice
seed(42)
x = randint(1, 10)
assert_eq(x >= 1 and x <= 10, true)
print("random: PASS")

from "stdlib/fs.qs" import exists, read, write, remove
write("vm_test_temp.txt", "hello")
content = read("vm_test_temp.txt")
assert_eq(content, "hello")
remove("vm_test_temp.txt")
print("fs: PASS")

from "stdlib/core.qs" import env
path = env("PATH")
assert_eq(type(path), "string")
print("core: PASS")

print("ALL VM IMPORT TESTS PASSED")