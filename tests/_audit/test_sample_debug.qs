from "stdlib/random.qs" import seed, sample
seed(42)
r = sample([1, 2, 3], 2)
print("sample([1,2,3], 2) = ", r)