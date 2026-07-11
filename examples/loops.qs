# Loops

# repeat N runs the block N times.
counter = 0
repeat 3 {
    counter += 1
    print("counter:", counter)
}

# repeat item -> number counts from 1 to that number.
repeat index -> 5 {
    print("index:", index)
}

# repeat item -> string gives one letter each time.
repeat letter -> "abc" {
    print("letter:", letter)
}

# repeat item -> list gives one item each time.
repeat color -> ["red", "green", "blue"] {
    print("color:", color)
}

# repeat item -> map gives one key each time.
person = map("name"="Sara", "age"=20)
repeat key -> person {
    print("key:", key, "value:", person[key])
}

# repeat i, item -> collection gives index and value.
names = ["Ali", "Sara", "Mina"]
repeat i, name -> names {
    print(i, name)
}

# range creates a list of numbers for looping.
print(range(5))         # [1, 2, 3, 4, 5]
print(range(2, 5))      # [2, 3, 4, 5]
print(range(5, 1, -2))  # [5, 3, 1]

# break exits the loop early.
repeat n -> range(1, 10) {
    if n == 4 {
        break
    }
    print("n:", n)
}

# continue skips to the next iteration.
repeat n -> range(1, 6) {
    if n == 3 {
        continue
    }
    print("skip:", n)
}

# while runs while a condition is true.
x = 0
while x < 3 {
    x += 1
    print("while x:", x)
}
