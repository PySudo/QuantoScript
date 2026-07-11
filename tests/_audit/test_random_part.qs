from "stdlib/random.qs" import seed, randint, randrange, choice, sample

seed(42)
r1 = randint(1, 10)
print("randint(1,10) = ", r1)

seed(42)
r2 = randint(1, 10)
print("seeded again = ", r2)

seed(42)
r3 = randrange(0, 10)
print("randrange(0,10) = ", r3)

items = [10, 20, 30]
r4 = choice(items)
print("choice = ", r4)

r5 = sample([1, 2, 3, 4, 5], 3)
print("sample len = ", len(r5))

ok = true
if r1 != r2 {
    ok = false
}
if r3 < 0 {
    ok = false
}
if r3 >= 10 {
    ok = false
}
if len(r5) != 3 {
    ok = false
}
if ok {
    print("PASS: random - seed/randint/randrange/choice/sample work")
} else {
    print("FAIL: random")
}