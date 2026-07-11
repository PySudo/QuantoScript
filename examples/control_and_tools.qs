# range creates a list of numbers.
print(range(1, 5))
print(range(5, 1, -2))

items = []

repeat n -> range(1, 6) {
    if n == 2 {
        continue
    }
    if n == 5 {
        break
    }
    items.append(n)
}

print(items)
print([1, [2, 3]] == [1, [2, 3]])
print(map("a"=1, "b"=[2]) == map("b"=[2], "a"=1))

func returns_null() {
}

func never_returns() {
    exit()
}

print(ever_return(returns_null))
print(ever_return(never_returns))
