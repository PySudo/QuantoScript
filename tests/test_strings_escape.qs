print("=== 1. Newline ===")
s = "hello\nworld"
assert_eq(s[1], "h")
assert_eq(s[6], "\n")
assert_eq(s[7], "w")
print("PASS")

print("=== 2. Tab ===")
s = "col1\tcol2"
assert_eq(s[5], "\t")
print("PASS")

print("=== 3. Backslash ===")
s = "back\\slash"
assert_eq(s[5], "\\")
assert_eq(s[6], "s")
print("PASS")

print("=== 4. Double quote ===")
s = "say \"hello\""
assert_eq(s[5], "\"")
assert_eq(s[6], "h")
print("PASS")

print("=== 5. Single quote ===")
s = "it's"
assert_eq(s[3], "\'")
print("PASS")

print("=== 6. Literal backslash-n ===")
s = "\\n"
assert_eq(len(s), 2)
assert_eq(s[1], "\\")
assert_eq(s[2], "n")
print("PASS")

print("=== 7. Invalid escapes ===")
s = "\q"
assert_eq(len(s), 2)
assert_eq(s[1], "\\")
s = "\z"
assert_eq(len(s), 2)
s = "\x"
assert_eq(len(s), 2)
print("PASS")

print("=== 8. Escaped quotes ===")
s = "hello \"world\""
assert_eq(len(s), 13)
assert_eq(s[7], "\"")
assert_eq(s[8], "w")
print("PASS")

print("=== 9. Empty and single ===")
assert_eq(len(""), 0)
s = "\n"
assert_eq(len(s), 1)
s = "\\"
assert_eq(len(s), 1)
print("PASS")

print("=== 10. Long strings ===")
s = ""
repeat 100 {
    s = s + "ab\t"
}
assert_eq(len(s), 300)
s = ""
repeat 100 {
    s = s + "line\n"
}
assert_eq(len(s), 500)
print("PASS")

print("=== 11. Combined ===")
s = "line1\nline2\tcol1\\col2\"done"
assert_eq(s[1], "l")
assert_eq(s[6], "\n")
assert_eq(s[12], "\t")
assert_eq(s[17], "\\")
assert_eq(s[22], "\"")
print("PASS")

print("ALL TESTS PASSED")