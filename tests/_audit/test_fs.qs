from "stdlib/fs.qs" import exists, read, write, remove, basename, dirname, extension, join_path
test_file = "_audit_temp.txt"
write(test_file, "hello qs")
content = read(test_file)
exists_before = exists(test_file)
remove(test_file)
exists_after = exists(test_file)
ok = true
if content != "hello qs" {
    ok = false
}
if not exists_before {
    ok = false
}
if exists_after {
    ok = false
}
b = basename("/foo/bar/test.txt")
d2 = dirname("/foo/bar/test.txt")
e = extension("/foo/bar/test.txt")
j = join_path("a", "b")
if b != "test.txt" {
    ok = false
}
if d2 != "foo/bar" {
    ok = false
}
if e != "txt" {
    ok = false
}
if j != "a/b" {
    ok = false
}
if ok {
    print("PASS: fs - exists/read/write/remove/basename/dirname/extension/join_path all work")
} else {
    print("FAIL: fs - content=", content, " b=", b, " d=", d2, " e=", e, " j=", j)
}