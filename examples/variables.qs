# Variables
# A variable is a name for a value.

text = "Quanto"
number = 3
flag = true
nothing = null
items = ["red", "green"]
person = {
    "name": "Sara",
    "age": 20
}

print("text:", text)
print("number:", number)
print("flag:", flag)
print("nothing:", nothing)
print("items:", items)
print("person:", person)
print("person name:", person["name"])

# type(value) shows the data type name.
print("type text:", type(text))
print("type number:", type(number))
print("type flag:", type(flag))
print("type nothing:", type(nothing))
print("type items:", type(items))
print("type person:", type(person))

# isinstance(value, "type") checks the type.
print(isinstance(text, "string"))
print(isinstance(number, "int"))
print(isinstance(flag, "bool"))
print(isinstance(nothing, "null"))
print(isinstance(items, "list"))
print(isinstance(person, "map"))

# Names can start with a letter or "_".
_secret = "starts with underscore"
print(_secret)

# You can change a normal variable.
number = 4
print("new number:", number)
