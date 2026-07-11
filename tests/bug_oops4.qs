func bad() {
    oops("from func")
}
try {
    bad()
} oops e {
    print(e)
}