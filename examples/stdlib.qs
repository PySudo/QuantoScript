# Standard library tour.

from "../stdlib/time.qs" import now
from "../stdlib/text.qs" import lower, upper, trim, split
from "../stdlib/fs.qs" import exists, write, read
from "../stdlib/math.qs" import min, max, clamp

print("time:", now())
print("string:", str([1, 2, 3]))

print(lower("HELLO"))
print(upper("hello"))
print(trim("  QuantoScript  "))
print(split("red,green,blue", ","))

print("min:", min([3, 1, 9]))
print("max:", max([3, 1, 9]))
print("clamp:", clamp(12, 1, 10))

path = "tmp_qs_test.txt"
print("written:", write(path, "hello stdlib"))
print("exists:", exists(path))
print("read:", read(path))

