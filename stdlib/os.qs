# stdlib/os.qs — Process execution and environment
#
# SECURITY: run() and capture() execute shell commands directly.
# Do not pass untrusted user input without validation.

func run(command) {
    return sys_run(command)
}

func capture(command) {
    return sys_capture(command)
}

func cwd() {
    return sys_cwd()
}

func chdir(path) {
    return sys_chdir(path)
}

func exists(command) {
    return sys_exec_exists(command)
}
