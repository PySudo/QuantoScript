# try / oops
# Use try when a part of the program might fail.
# If it fails, QuantoScript jumps to oops instead of stopping the whole program.

# assert checks a condition and stops the program if it is false.
age = 25
assert(age > 0, "age must be positive")
print("assert passed")

# Use try when you expect an error might happen.
items = ["red", "green", "blue"]

try {
    print("first item:", items[1])
    print("bad item:", items[10])
    print("this line will not run")
} oops {
    print("oops: something went wrong, but the program is still alive")
}

print("after try/oops")

# oops only catches errors inside its try block.
try {
    print("safe work")
} oops {
    print("this will not run")
}

# Multiple operations in try.
try {
    data = [1, 2, 3]
    print("item:", data[2])
    result = 10 / 0
} oops {
    print("caught an error in the try block")
}

print("program continues after error")
