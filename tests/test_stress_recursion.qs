# Stress Test: Deep Recursion (smaller depths)
# Goal: Verify VM handles recursive calls correctly

func factorial(n) {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}

func fibonacci(n) {
    if n <= 1 {
        return n
    }
    return fibonacci(n - 1) + fibonacci(n - 2)
}

func sum_range(n, acc) {
    if n <= 0 {
        return acc
    }
    return sum_range(n - 1, acc + n)
}

print("fact 5:", factorial(5))
print("fact 10:", factorial(10))
print("fact 12:", factorial(12))

print("fib 10:", fibonacci(10))
print("fib 15:", fibonacci(15))
print("fib 20:", fibonacci(20))

print("sum 10:", sum_range(10, 0))
print("sum 50:", sum_range(50, 0))
print("sum 100:", sum_range(100, 0))

# Mutually recursive
func is_even(n) {
    if n == 0 {
        return true
    }
    return is_odd(n - 1)
}

func is_odd(n) {
    if n == 0 {
        return false
    }
    return is_even(n - 1)
}

print("is_even 4:", is_even(4))
print("is_odd 7:", is_odd(7))
print("is_even 0:", is_even(0))
print("is_odd 1:", is_odd(1))
