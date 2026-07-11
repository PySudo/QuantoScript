# Step 1: Literals, variables, arithmetic
print(42)
print(3.14)
print("hello")
x = 10
print(x)
print(1 + 2)
print(2 * 3 + 1)
print(10 - 3)
print(10 / 2)
print(10 % 3)

# Step 2: Booleans, comparisons, logic
print(true)
print(false)
print(null)
print(not false)
print(3 > 2)
print(3 == 3)
print(3 != 2)
print(true and true)
print(true and false)
print(false or true)
print(1 + 2 > 2 and 3 < 5)

# Step 3: If/else/maybe
x = 10
if x > 5 {
    print("big")
}
if x > 100 {
    print("huge")
} else {
    print("not huge")
}
score = 85
if score >= 90 {
    print("A")
} maybe score >= 80 {
    print("B")
} maybe score >= 70 {
    print("C")
} else {
    print("F")
}

# Step 4: While loops
i = 0
while i < 5 {
    print(i)
    i += 1
}

# Step 5: Functions
func add(a, b) {
    return a + b
}
print(add(3, 4))

func factorial(n) {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}
print(factorial(5))

func classify(score) {
    if score >= 90 {
        return "A"
    } maybe score >= 80 {
        return "B"
    } else {
        return "C"
    }
}
print(classify(85))
print(classify(95))
print(classify(70))
