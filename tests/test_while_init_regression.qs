class Foo1 {
    init() {
        self.x = 1
        i = 1
        while i <= 3 {
            self.x = self.x + 1
            i = i + 1
        }
    }
}
f1 = Foo1()
print(f1.x)
class Foo2 {
    init() {
        i = 1
        while i <= 3 {
            i = i + 1
        }
    }
}
f2 = Foo2()
print("done")
class Foo3 {
    init() {
        self.x = 1
        self.y = 2
        self.z = 3
        i = 1
        while i <= 2 {
            self.x = self.x + 1
            i = i + 1
        }
    }
}
f3 = Foo3()
print(f3.x, f3.y, f3.z)
class Foo4 {
    init() {
        self.count = 0
        i = 1
        while i <= 100 {
            self.count = self.count + 1
            i = i + 1
        }
    }
}
f4 = Foo4()
print(f4.count)