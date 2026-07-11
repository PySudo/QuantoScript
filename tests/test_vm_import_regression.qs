# QuantoScript v1.0 VM Import Regression Test
# Tests all stdlib modules across interpreter, VM, and QVM
# Expected: identical output on all 3 paths

from "stdlib/time.qs" import now, year, month, day, today_str, now_str
from "stdlib/math.qs" import min, max, clamp
from "stdlib/json.qs" import parse, stringify
from "stdlib/text.qs" import upper, lower, trim, split
from "stdlib/log.qs" import info, warn, error
from "stdlib/random.qs" import seed, randint, choice
from "stdlib/fs.qs" import exists, read, write, remove
from "stdlib/core.qs" import env

print("=== time ===")
t = now()
assert_eq(type(t), "int")
y = year()
assert_eq(type(y), "int")
m = month()
assert_eq(type(m), "int")
d = day()
assert_eq(type(d), "int")
today = today_str()
assert_eq(type(today), "string")
nows = now_str()
assert_eq(type(nows), "string")

print("=== math ===")
assert_eq(min([3, 7, 1]), 1)
assert_eq(max([3, 7, 1]), 7)
assert_eq(clamp(5, 1, 3), 3)

print("=== json ===")
data = {"k": "v", "n": 42}
s = stringify(data)
p = parse(s)
assert_eq(p["k"], "v")
assert_eq(p["n"], 42)

print("=== text ===")
assert_eq(upper("hello"), "HELLO")
assert_eq(lower("HELLO"), "hello")
assert_eq(trim("  hi  "), "hi")
parts = split("a,b,c", ",")
assert_eq(len(parts), 3)

print("=== log ===")
info("test")
warn("test")
error("test")

print("=== random ===")
seed(42)
x = randint(1, 100)
assert_eq(x >= 1 and x <= 100, true)

print("=== fs ===")
write("vm_reg_test.txt", "ok")
assert_eq(read("vm_reg_test.txt"), "ok")
remove("vm_reg_test.txt")
assert_eq(exists("vm_reg_test.txt"), false)

print("=== core ===")
p = env("PATH")
assert_eq(type(p), "string")

print("ALL VM IMPORT REGRESSION TESTS PASSED")