from "stdlib/os.qs" import run, capture, cwd, chdir, exists
current = cwd()
ok = true
if len(current) == 0 {
    ok = false
}
output = capture("echo hello")
if len(output) == 0 {
    ok = false
}
has_echo = exists("echo")
if not has_echo {
    ok = false
}
if ok {
    print("PASS: os - cwd/capture/exists all work")
} else {
    print("FAIL: os - cwd=", current, " output=", output, " has_echo=", has_echo)
}