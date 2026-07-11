# Union types for function parameters

# Function that accepts string or int
func greet(name:string|int) {
    if isinstance(name, "int") {
        print("Hello number", name)
    } else {
        print("Hello", name)
    }
}

greet("Sara")
greet(42)

# Function with multiple union types
func show(value:string|int|bool) {
    print("value:", value)
}

show("hello")
show(123)
show(true)

# HTTP with map body (auto-converted to JSON)
from "../stdlib/http.qs" import post

# This would send a POST with JSON body automatically:
# response = post("https://httpbin.org/post", {"name": "QuantoScript", "version": 1})
# print(response["status"])
print("Union types working!")
