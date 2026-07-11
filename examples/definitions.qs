# Definitions
# If a name is all uppercase, QuantoScript treats it as a definition.
# A definition is meant to stay the same.

VERSION = "1.0"
APP_NAME = "QuantoScript"

print(APP_NAME, VERSION)

# This would be an error because VERSION already exists:
# VERSION = "2.0"

# Normal (lowercase) variables can be changed freely.
counter = 0
print("before:", counter)
counter = 10
print("after:", counter)
