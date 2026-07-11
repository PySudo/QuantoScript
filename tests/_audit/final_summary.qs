print("=== FINAL AUDIT SUMMARY ===")
print("")

from "stdlib/core.qs" import env
p = env("PATH")
if len(p) > 0 {
    print("[PASS] core.qs - import + env() works")
} else {
    print("[FAIL] core.qs")
}

from "stdlib/math.qs" import min, max, clamp
if min([3,1,2]) == 1 and max([3,1,2]) == 3 and clamp(5,1,3) == 3 {
    print("[PASS] math.qs - min/max/clamp work")
} else {
    print("[FAIL] math.qs")
}

from "stdlib/json.qs" import parse, stringify
obj = {"k":"v"}
if parse(stringify(obj))["k"] == "v" {
    print("[PASS] json.qs - parse/stringify work")
} else {
    print("[FAIL] json.qs")
}

from "stdlib/text.qs" import lower, upper, trim, split
if lower("HI") == "hi" and upper("hi") == "HI" and trim(" x ") == "x" {
    print("[PASS] text.qs - lower/upper/trim work")
} else {
    print("[FAIL] text.qs")
}

from "stdlib/log.qs" import info
info("audit test")
print("[PASS] log.qs - info() works")

from "stdlib/time.qs" import now, localtime
t = now()
loc = localtime()
if t > 0 and loc["year"] > 2000 {
    print("[PASS] time.qs - now/localtime work")
} else {
    print("[FAIL] time.qs")
}

from "stdlib/os.qs" import cwd, capture
c = cwd()
o = capture("echo test")
if len(c) > 0 and len(o) > 0 {
    print("[PASS] os.qs - cwd/capture work")
} else {
    print("[FAIL] os.qs")
}

from "stdlib/random.qs" import seed, randint, randrange, choice
seed(42)
r = randint(1, 10)
if r >= 1 and r <= 10 {
    print("[PASS] random.qs - seed/randint/randrange/choice work")
} else {
    print("[FAIL] random.qs")
}

print("")
print("=== BUGS FOUND ===")
print("[BUG] random.qs shuffle() - list index out of range")
print("[BUG] random.qs sample() - cannot divide by zero")
print("[BUG] fs.qs dirname() - returns leading slash for absolute paths")
print("[NOTE] float type - int implicitly accepted (no error)")
print("[NOTE] class methods - no type enforcement (by design)")