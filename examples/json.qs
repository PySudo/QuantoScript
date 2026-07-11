# JSON support

from "../stdlib/json.qs" import parse, stringify

# Parse JSON string
data = parse('{"name": "Sara", "age": 20, "active": true}')
print(data["name"])
print(data["age"])
print(data["active"])

# Parse JSON array
items = parse('[1, 2, 3, "hello"]')
print(items)

# Stringify to JSON
person = map("name"="Ali", "age"=25)
print(stringify(person))

# Round-trip
original = parse('{"x": 1, "y": [2, 3]}')
text = stringify(original)
print(text)
restored = parse(text)
print(restored["x"])
print(restored["y"])
