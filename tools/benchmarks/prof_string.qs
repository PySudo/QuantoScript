# String concatenation benchmark - stresses string allocation, value_to_string
s = ""
repeat 50000 {
    s = s + "x"
}
print("string len: ", s.len())
