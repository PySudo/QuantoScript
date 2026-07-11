# Full tour

APP = "QuantoScript"
print("welcome to", APP)

names = ["Ali", "Sara", "Mina"]

repeat index -> 3 {
    name = names[index]

    if index == 1 {
        print(index, name, "is first")
    } maybe index == 2 {
        print(index, name, "is second")
    } else {
        print(index, name, "is later")
    }
}

text = "simple"
print("first:", text[1], "last:", text[-1], "middle:", text[2:5])

if "Sara" in names {
    print("Sara exists")
}
