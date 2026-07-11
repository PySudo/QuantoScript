func divide(a, b) {
    if b == 0 {
        oops("division by zero")
    }
    return a / b
}

try {
    print(divide(10, 0))
} oops e {
    print(e)
}