# Test: oops directly in try
try {
    oops("direct oops")
} oops e {
    print("direct: ", e)
}

# Test: oops inside a called function
func bad() {
    oops("from func")
}

try {
    bad()
} oops e {
    print("func: ", e)
}