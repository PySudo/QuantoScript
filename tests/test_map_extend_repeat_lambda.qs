person = map("name" = "Sara", "age" = 20)
person.extend(map("country" = "Iran"))
print(person.keys())
print(map("a" = 1).extend(map("b" = 2)))

repeat key, value -> person {
    print(key, value)
}

names = ["Ali", "Sara"]
repeat i, name -> names {
    print(i, name)
}

classify = fn(n) -> {
    if n > 0 {
        return "positive"
    } else {
        return "non-positive"
    }
}
print(classify(5))
print(classify(-3))

double = fn(n) -> n * 2
print(double(7))

blocky = fn() -> {
    x = 10
    y = 20
    return x + y
}
print(blocky())

make_map = fn() -> { "a": 1, "b": 2 }
print(make_map())

func boom(n) {
    if n <= 0 {
        oops("need positive")
    }
    return n
}
try { print(boom(42)) } oops e { print("failed:", e) }
try { print(boom(-1)) } oops e { print("failed:", e) }
print("done")
