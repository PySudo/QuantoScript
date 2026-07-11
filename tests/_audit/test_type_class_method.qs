class Animal {
    init(name, age) {
        self.name = name
        self.age = age
    }
    get_name() {
        return self.name
    }
    set_age(age) {
        self.age = age
    }
}
a = Animal("Rex", 5)
ok = true
if a.get_name() != "Rex" {
    ok = false
}
a.set_age(6)
if a.age != 6 {
    ok = false
}
if ok {
    print("PASS: class methods - get_name/set_age work correctly")
} else {
    print("FAIL: class methods")
}