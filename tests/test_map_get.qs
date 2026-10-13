# Map.get(key[, default]): value for key, or default when the key is missing.

from "stdlib/core.qs" import parse_env

m = map("a" = 1, "b" = 2)

print("a:", m.get("a", 0))
print("b:", m.get("b", 0))
print("c:", m.get("c", -1))
print("d:", m.get("d", "fallback"))

# No default -> null.
missing = m.get("nope")
print("missing is null:", missing == null)

# Explicit null default.
print("explicit null:", m.get("nope", null))

# Values of any type come back as-is.
nested = map("list" = [1, 2, 3])
print("nested:", nested.get("list", []))

# get does not mutate the receiver.
m.get("a", 0)
print("keys:", m.keys())

# Works with parse_env, the motivating case.
cfg = parse_env("TOKEN=secret\n")
print("TOKEN:", cfg.get("TOKEN", null))
print("MISSING:", cfg.get("MISSING", "default"))

# A non-map receiver is an error.
try {
    bad = [1, 2, 3].get("x", 0)
    print("unreachable")
} oops e {
    print("error:", e)
}
