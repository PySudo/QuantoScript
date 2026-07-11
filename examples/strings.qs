# Strings
# You can use single quotes or double quotes.

single = 'hello'
double = "world"

print(single)
print(double)

# + joins strings.
message = single + " " + double
print(message)

# * repeats strings, like Python.
print("ha" * 3)
print(3 * "yo")

# Quotes can appear inside strings when they are not the closing quote.
funny = 'i use ' inside this string.'
print(funny)

# Indexes start at 1.
word = "Quanto"
print("first letter:", word[1])
print("last letter:", word[-1])

# Slices use start:end. The end is included.
print("slice:", word[2:5])

# len() returns the length of a string.
print("length:", len("Quanto"))

# str() converts a value to a string.
print("number as string:", str(42))

# Short assignment works on strings.
greeting = "hello"
greeting += " world"
print(greeting)

# String with escape sequences.
print("line1\nline2")
print("tab\there")
