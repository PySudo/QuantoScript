# Functions

# A simple function.
func greet(name) {
    print("hello", name)
}

greet("Ali")

# A function can return a value.
func add(a:int, b:int) {
    return a + b
}

print("sum:", add(2, 3))

# Parameters can have default values.
func say(text:string, ending:string=" world") {
    return text + ending
}

print(say("hello"))
print(say("hello", " Quanto"))

# If you do not write a type, the parameter accepts any value.
func show(value) {
    print("value:", value)
}

show("text")
show(42)

# Union types let a parameter accept multiple types.
func greet_any(name:string|int) {
    if isinstance(name, "int") {
        print("hello number", name)
    } else {
        print("hello", name)
    }
}

greet_any("Sara")
greet_any(42)

# *args catches extra positional values as a list.
func collect(first, *rest) {
    print("first:", first)
    print("rest:", rest)
}

collect("a", "b", "c", "d")

# **kwargs catches keyword values as a dict.
func profile(name:string, **info) {
    print("name:", name)
    print("age:", info["age"])
    print("city:", info["city"])
}

profile("Sara", age=20, city="Tehran")

# ever_return checks if a function can return a value.
func gives_number() {
    return 2
}

func gives_null() {
}

func stops() {
    exit()
}

print("gives_number ever returns:", ever_return(gives_number))
print("gives_null ever returns:", ever_return(gives_null))
print("stops ever returns:", ever_return(stops))
