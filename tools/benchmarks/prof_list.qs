# List operations benchmark - stresses list allocation, clone, method dispatch
items = []
repeat 50000 {
    items.push(42)
}
total = 0
repeat i, item -> items {
    total = total + item
}
print("list sum: ", total)
