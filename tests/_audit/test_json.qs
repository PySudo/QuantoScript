from "stdlib/json.qs" import parse, stringify
obj = {"key": "value", "num": 42}
text = stringify(obj)
roundtrip = parse(text)
ok = true
if roundtrip["key"] != "value" {
    ok = false
}
if roundtrip["num"] != 42 {
    ok = false
}
parsed = parse("[1, 2, 3]")
if parsed[1] != 1 {
    ok = false
}
if parsed[2] != 2 {
    ok = false
}
if parsed[3] != 3 {
    ok = false
}
if ok {
    print("PASS: json - parse/stringify roundtrip works")
} else {
    print("FAIL: json - roundtrip mismatch")
}