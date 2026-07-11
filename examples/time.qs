# Time library demo
from "../stdlib/time.qs" import now, localtime, format, now_str, today_str
from "../stdlib/time.qs" import year, month, day, hour, minute, second
from "../stdlib/time.qs" import add_days, diff_seconds

# Current timestamp
print("Timestamp:", now())

# Local time as map
t = localtime()
print("Year:", t["year"])
print("Month:", t["month"])
print("Day:", t["day"])
print("Hour:", t["hour"])
print("Min:", t["min"])
print("Sec:", t["sec"])

# Formatted time
print("Now:", now_str())
print("Today:", today_str())
print("Custom:", format(now(), "%Y/%m/%d %H:%M"))

# Convenience functions
print("Year:", year())
print("Month:", month())
print("Day:", day())

# Time arithmetic
tomorrow = add_days(now(), 1)
print("Tomorrow:", format(tomorrow, "%Y-%m-%d"))
