from "stdlib/core.qs" import env
result = env("PATH")
if result != "" {
    print("PASS: core - env() returns non-empty PATH")
} else {
    print("FAIL: core - env() returned empty")
}