func bench() {
    n = 50
    A = []
    B = []
    i = 0
    while i < n {
        ra = []
        rb = []
        j = 0
        while j < n {
            ra = ra + [((i * 7 + j * 3) % 100)]
            rb = rb + [((i * 5 + j * 11) % 100)]
            j = j + 1
        }
        A = A + [ra]
        B = B + [rb]
        i = i + 1
    }
    C = []
    i = 0
    while i < n {
        row = []
        j = 0
        while j < n {
            s = 0
            k = 0
            while k < n {
                s = s + A[i][k] * B[k][j]
                k = k + 1
            }
            row = row + [s]
            j = j + 1
        }
        C = C + [row]
        i = i + 1
    }
    return C[1][1] + C[n][n]
}
print(bench())
