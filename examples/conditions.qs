# Conditions

score = 75

if score >= 90 {
    print("grade: A")
} maybe score >= 70 {
    print("grade: B")
} maybe score >= 50 {
    print("grade: C")
} else {
    print("grade: D")
}

# Conditions use truthy/falsy.
name = ""

if name {
    print("has a name")
} else {
    print("no name yet")
}

# Chained comparisons work like Python.
x = 3
if 0 < x < 5 {
    print("x is between 0 and 5")
}

