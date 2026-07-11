# Regression tests for single-line function bodies

# 1. Basic single-line func
func a() { return 1 }
print(a())

# 2. Empty single-line func
func b() { }

# 3. Multi-line func (should still work)
func c() {
    return 2
}
print(c())

# 4. Single-line func with params
func add(a, b) { return a + b }
print(add(3, 4))

# 5. Single-line func calling another func
func double(x) { return x * 2 }
func triple(x) { return double(x) + x }
print(triple(5))

# 6. Single-line func with if inside
func nested() {
    if true {
        return 5
    }
}
print(nested())

# 7. Multiple single-line funcs
func f1() { return 10 }
func f2() { return 20 }
func f3() { return 30 }
print(f1())
print(f2())
print(f3())

# 8. Single-line func followed by class
func helper() { return 42 }
class Foo {
    test() { return helper() }
}
print(Foo().test())

# 9. Class followed by single-line func
class Bar {
    test() { return 99 }
}
func after_class() { return Bar().test() }
print(after_class())

# 10. Single-line func with string body
func greet(name) { return "Hello " + name }
print(greet("World"))

# 11. Single-line func with arithmetic (simple)
func simple_calc(a) { return a + 1 }
print(simple_calc(6))

# 12. Single-line func with bool
func check(x) { return x > 5 }
print(check(10))

# 13. Single-line func with list
func make_list() { return [1, 2, 3] }
print(make_list())

# 14. Single-line func with map
func make_map() { return {"key": "value"} }
print(make_map())

# 15. Single-line func with comparison (using !=)
func is_diff(a, b) { return a != b }
print(is_diff(5, 5))
