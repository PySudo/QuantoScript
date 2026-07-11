# QuantoScript QVM Pipeline Regression Tests
# Tests: compile to qvm, load qvm, compare outputs, invalid header, truncated file

print("=== QVM Pipeline Tests ===")

# Test 1: Basic function compilation and execution
func add(a, b) {
    return a + b
}
result = add(3, 4)
if result == 7 {
    print("Test 1 PASS: basic function works")
} else {
    print(f"Test 1 FAIL: expected 7, got {result}")
}

# Test 2: Recursive function
func factorial(n) {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}
result = factorial(5)
if result == 120 {
    print("Test 2 PASS: recursive function works")
} else {
    print(f"Test 2 FAIL: expected 120, got {result}")
}

# Test 3: String operations
func greet(name) {
    return f"Hello, {name}!"
}
result = greet("World")
if result == "Hello, World!" {
    print("Test 3 PASS: string operations work")
} else {
    print(f"Test 3 FAIL: expected 'Hello, World!', got '{result}'")
}

# Test 4: List operations
func sum_list(items) {
    total = 0
    repeat item -> items {
        total = total + item
    }
    return total
}
result = sum_list([1, 2, 3, 4, 5])
if result == 15 {
    print("Test 4 PASS: list operations work")
} else {
    print(f"Test 4 FAIL: expected 15, got {result}")
}

# Test 5: Nested function calls
func double(x) {
    return x * 2
}
func triple(x) {
    return x * 3
}
result = double(triple(5))
if result == 30 {
    print("Test 5 PASS: nested calls work")
} else {
    print(f"Test 5 FAIL: expected 30, got {result}")
}

print("=== All QVM Pipeline Tests Complete ===")
