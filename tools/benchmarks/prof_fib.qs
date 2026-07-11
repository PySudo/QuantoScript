# Recursive fibonacci - stresses function calls, stack, env cloning
func fib(n) {
    if n <= 1 {
        return n
    }
    return fib(n - 1) + fib(n - 2)
}
result = fib(25)
print("fib(25) = ", result)
