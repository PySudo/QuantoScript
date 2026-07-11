from "stdlib/time.qs" import now, localtime, format, year, month, day, hour, diff_seconds, add_seconds, add_days
t = now()
local = localtime()
y = year()
m = month()
d = day()
h = hour()
ts1 = now()
ts2 = add_seconds(ts1, 10)
diff = diff_seconds(ts1, ts2)
ts3 = add_days(ts1, 1)
ok = true
if t == 0 {
    ok = false
}
if local["year"] == 0 {
    ok = false
}
if y == 0 {
    ok = false
}
if m == 0 {
    ok = false
}
if d == 0 {
    ok = false
}
if diff != 10 {
    ok = false
}
if ts3 <= ts1 {
    ok = false
}
fmt = format(t, "%Y-%m-%d")
if len(fmt) < 8 {
    ok = false
}
if ok {
    print("PASS: time - all functions work")
} else {
    print("FAIL: time - t=", t, " y=", y, " m=", m, " diff=", diff)
}