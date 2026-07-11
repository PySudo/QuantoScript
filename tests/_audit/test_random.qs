from "stdlib/random.qs" import seed, randint, randrange, choice, shuffle, sample
seed(42)
r1 = randint(1, 10)
seed(42)
r2 = randint(1, 10)
seed(42)
r3 = randrange(0, 10)
items = [10, 20, 30]
r4 = choice(items)
has_valid = false
if r4 == 10 {
    has_valid = true
}
if r4 == 20 {
    has_valid = true
}
if r4 == 30 {
    has_valid = true
}
r5 = shuffle([1, 2, 3])
r6 = sample([1, 2, 3, 4, 5], 3)
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
if not has_valid {
    ok = false
}
if len(r5) != 3 {
    ok = false
}
if len(r6) != 3 {
    ok = false
}
if ok {
    print("PASS: random - seed/randint/randrange/choice/shuffle/sample work")
} else {
    print("FAIL: random")
}