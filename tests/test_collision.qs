from "stdlib/json.qs" import parse, stringify
from "stdlib/time.qs" import now, year
s = stringify({"k": "v"})
p = parse(s)
assert_eq(p["k"], "v")
y = year()
assert_eq(type(y), "int")
print("json+time: PASS")