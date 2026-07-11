# Numbers and operators

a = 10
b = 3

print("a + b =", a + b)
print("a - b =", a - b)
print("a * b =", a * b)
print("a / b =", a / b)
print("a % b =", a % b)

# Parentheses make the order clear.
answer = (2 + 3) * 4
print("answer:", answer)

# Negative numbers.
x = -5
print("negative:", x)
print("negate:", -x)

# Comparison operators.
print("10 == 10:", 10 == 10)
print("10 != 5:", 10 != 5)
print("3 < 5:", 3 < 5)
print("3 > 5:", 3 > 5)
print("3 <= 3:", 3 <= 3)
print("3 >= 5:", 3 >= 5)

# Chained comparisons.
y = 3
print("0 < 3 < 5:", 0 < y < 5)
print("0 < 3 < 2:", 0 < y < 2)

# Short assignment operators change the old value.
score = 1
score += 4
print("score += 4:", score)

score -= 2
print("score -= 2:", score)

score *= 3
print("score *= 3:", score)

score /= 3
print("score /= 3:", score)

score %= 2
print("score %= 2:", score)
