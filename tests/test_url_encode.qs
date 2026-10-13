from "stdlib/url.qs" import url_encode, url_decode

assert_eq(url_encode("a b"), "a%20b")
assert_eq(url_encode("a&b=c"), "a%26b%3Dc")
assert_eq(url_encode("hello"), "hello")
assert_eq(url_encode("~-_. "), "~-_.%20")
assert_eq(url_decode("a%20b%26c%3Dd"), "a b&c=d")
assert_eq(url_decode("a+b"), "a b")
assert_eq(url_encode("سلام"), "%D8%B3%D9%84%D8%A7%D9%85")
assert_eq(url_decode(url_encode("سلام a")), "سلام a")

print(url_encode("654صثگ \\/,m.:;"))
print("url_encode: PASS")
