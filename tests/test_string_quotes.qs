print("=== 1. Opposite quote inside ===")
a = "i use ' inside this string."
assert_eq(a, "i use ' inside this string.")
b = 'i use " inside this string.'
assert_eq(b, 'i use " inside this string.')
print("PASS")

print("=== 2. Same quote inside double-quoted ===")
d = "i use " inside this string."
assert_eq(len(d), 27)
assert_eq(d[7], "\"")
assert_eq(d, "i use \" inside this string.")
print("PASS")

print("=== 3. Same quote inside single-quoted ===")
s = 'i use ' inside this string.'
assert_eq(len(s), 27)
assert_eq(s[7], "'")
assert_eq(s, 'i use \' inside this string.')
print("PASS")

print("=== 4. Multiple same quotes ===")
m = "a "b" c"
assert_eq(len(m), 7)
assert_eq(m, "a \"b\" c")
print("PASS")

print("=== 5. Concatenation still works ===")
c = "hello" + " " + "world"
assert_eq(c, "hello world")
print("PASS")

print("=== 6. String followed by keyword ===")
assert_eq("a" in "cat", true)
assert_eq("z" in "cat", false)
print("PASS")

print("=== 7. Escapes still work ===")
e = "say \"hi\""
assert_eq(e, "say \"hi\"")
print("PASS")

print("ALL TESTS PASSED")
