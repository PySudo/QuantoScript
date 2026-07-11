from "stdlib/time.qs" import year, month, day, today_str, now_str
from "stdlib/math.qs" import min, max, clamp
from "stdlib/json.qs" import stringify, parse
from "stdlib/log.qs" import info
from "stdlib/text.qs" import upper, lower
from "stdlib/random.qs" import seed, randint

func add(x:int, y:int) {
    return x + y
}
assert_eq(add(3, 4), 7)
try {
    add("hello", 4)
} oops e {
    print("type: PASS")
}

class Counter {
    init() {
        self.count = 0
    }
    inc(x:int) {
        self.count = self.count + x
    }
    get() {
        return self.count
    }
}
c = Counter()
c.inc(5)
assert_eq(c.get(), 5)
try {
    c.inc("bad")
} oops e {
    print("class: PASS")
}

assert_eq(isinstance(42, "int"), true)
print("isinstance: PASS")

y = year()
assert_eq(type(y), "int")
assert_eq(min([3, 7, 1]), 1)
data = stringify({"k":"v"})
info("test")
assert_eq(upper("hi"), "HI")
seed(42)
today = today_str()
assert_eq(type(today), "string")

print("ALL REGRESSION TESTS PASSED")