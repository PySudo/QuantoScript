try {
    x = 10 / 0
} oops e {
    assert_eq(e, "cannot divide by zero")
}
print("try/oops: PASS")