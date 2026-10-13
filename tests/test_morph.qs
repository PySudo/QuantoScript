# morph(value, type) converts a value to another type.

assert_eq(morph("5", int), 5)
assert_eq(morph("5", "int"), 5)
assert_eq(morph(5, string), "5")
assert_eq(morph(10.9, int), 10)
assert_eq(morph("3.14", float), 3.14)
assert_eq(morph(1, bool), true)
assert_eq(morph(0, bool), false)
assert_eq(morph("abc", list), ["a", "b", "c"])
assert_eq(morph("42", int) + 1, 43)
assert_eq(isinstance(morph("5", int), int), true)

kind = "float"
assert_eq(morph("2.5", kind), 2.5)

try {
    morph("nope", int)
} oops e {
    print(e)
}

print("morph: PASS")
