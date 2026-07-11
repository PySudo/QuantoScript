# Comments

# This is a one-line comment.
print("before")

/*
This is a multi-line comment.
QuantoScript ignores all of these lines.
*/

print("after")

# Block comments can sit inside a line.
answer = 1 /* ignored */ + 2
print(answer)

# A free text line without "=" is also treated like a comment.
This line is a comment too.

