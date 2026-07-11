# Arithmetic loop benchmark - stresses integer ops, env lookup, env store
x = 0
repeat 500000 {
    x = x + 1
}
print("arith done: ", x)
