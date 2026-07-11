# Dict operations benchmark - stresses dict allocation, dict_find, dict_set
d = {}
repeat 20000 {
    d["key"] = 42
}
total = 0
repeat 20000 {
    total = total + d["key"]
}
print("dict sum: ", total)
