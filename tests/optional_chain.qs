name = null
print("coalesce null: ", name ?? "anonymous")
count = 0
print("coalesce zero: ", count ?? 10)
print("coalesce chain: ", null ?? null ?? 7)
print("precedence: ", null ?? false or true)

user = null
print("dot on null: ", user?.name)
print("index on null: ", user?["id"])

data = {"id": 42}
print("index key hit: ", data?["id"])
print("index key miss: ", data?["missing"])
print("dot key hit: ", data?.id)
print("dot key miss: ", data?.missing)
print("nested chain: ", data?.missing?.deeper)

class C {
    init() {
        self.v = 7
    }
    greet() {
        return "hi"
    }
}

c = C()
print("object field: ", c?.v)
print("object method: ", c?.greet())
print("object field on null: ", user?.v)
print("object method on null: ", user?.greet())
print("fallback combine: ", user?.v ?? 100)
