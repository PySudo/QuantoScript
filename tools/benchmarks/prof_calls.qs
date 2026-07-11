# Function call benchmark - stresses call overhead, env cloning
func add(a, b) {
    return a + b
}
total = 0
repeat 100000 {
    total = add(total, 1)
}
print("calls sum: ", total)
