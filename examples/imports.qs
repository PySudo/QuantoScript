# Lazy imports only bring the names you ask for.

from "tools.qs" import APP_NAME, greet, double

print(APP_NAME)
print(greet("Sara"))
print(double(7))

# Use * when you want everything exported by the module.
from "tools.qs" import *

print(VERSION)
print(hidden_helper())
