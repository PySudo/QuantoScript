# New map methods

person = map("name"="Sara", "age"=20, "city"="Tehran")

# has
print(person.has("name"))                 # true
print(person.has("email"))                # false

# keys
print(person.keys())                      # ['name', 'age', 'city']

# values
print(person.values())                    # ['Sara', 20, 'Tehran']

# items
print(person.items())                     # [['name', 'Sara'], ['age', 20], ['city', 'Tehran']]

# remove
person.remove("city")
print(person.has("city"))                 # false
print(person.keys())                      # ['name', 'age']
