from "stdlib/fs.qs" import basename, dirname, extension, join_path

b1 = basename("/foo/bar/test.txt")
b2 = basename("test.txt")
b3 = basename("/single")
print("basename: ", b1, " | ", b2, " | ", b3)

d1 = dirname("/foo/bar/test.txt")
d2 = dirname("test.txt")
d3 = dirname("/foo")
print("dirname: ", d1, " | ", d2, " | ", d3)

e1 = extension("/foo/bar/test.txt")
e2 = extension("test")
print("extension: ", e1, " | ", e2)

j1 = join_path("a", "b")
j2 = join_path("a/", "b")
j3 = join_path("", "b")
print("join: ", j1, " | ", j2, " | ", j3)