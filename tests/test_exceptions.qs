# Division by zero
try {
    print(10 / 0)
} oops e {
    print("div0:", e)
}

# Modulo by zero
try {
    print(10 % 0)
} oops e {
    print("mod0:", e)
}

# Undefined variable
try {
    print(undefined_xyz)
} oops e {
    print("undef:", e)
}

# List index out of range
try {
    x = [1, 2]
    print(x[999])
} oops e {
    print("oob:", e)
}

# Normal operations (should NOT throw)
print("ok:", 10 / 2)
print("ok:", 10 % 3)
print("ok:", 42)
