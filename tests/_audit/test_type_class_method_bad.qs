class Animal {
    init(name) {
        self.name = name
    }
    set_name(name) {
        self.name = name
    }
}
a = Animal("Rex")
a.set_name(42)
print("set_name(42) accepted - type not enforced on class methods")