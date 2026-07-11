from "stdlib/random.qs" import seed, sample
seed(42)
r = sample([1, 2], 1)
print("sample([1,2], 1) = ", r)