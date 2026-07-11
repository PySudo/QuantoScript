# Logging framework.
# Usage:
#   from "stdlib/log.qs" import debug, info, warn, error

func debug(msg) {
    timestamp = sys_format_time(sys_time(), "%H:%M:%S")
    print("[" + timestamp + "] [DEBUG]", msg)
}

func info(msg) {
    timestamp = sys_format_time(sys_time(), "%H:%M:%S")
    print("[" + timestamp + "] [INFO]", msg)
}

func warn(msg) {
    timestamp = sys_format_time(sys_time(), "%H:%M:%S")
    print("[" + timestamp + "] [WARN]", msg)
}

func error(msg) {
    timestamp = sys_format_time(sys_time(), "%H:%M:%S")
    print("[" + timestamp + "] [ERROR]", msg)
}
