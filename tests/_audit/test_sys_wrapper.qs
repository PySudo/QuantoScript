from "stdlib/core.qs" import env
from "stdlib/time.qs" import now

path = env("PATH")
t = now()
print("env works: ", len(path) > 0)
print("time works: ", t > 0)