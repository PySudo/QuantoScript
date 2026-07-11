func cd(n) {
    if n <= 0 {
        return 0
    }
    return 1 + cd(n - 1)
}
print(cd(100))
