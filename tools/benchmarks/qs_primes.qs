func bench() {
    c = 0
    n = 2
    while n <= 10000 {
        isp = true
        d = 2
        while d * d <= n {
            if n % d == 0 {
                isp = false
            }
            if n % d == 0 {
                break
            }
            d = d + 1
        }
        if isp {
            c = c + 1
        }
        n = n + 1
    }
    return c
}
print(bench())
