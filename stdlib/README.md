# QuantoScript Standard Library

Import only what you need:

```qs
from "stdlib/http.qs" import get, post
from "stdlib/text.qs" import lower, split
from "stdlib/net.qs" import send, send_json
```

Use `import *` when you want the whole file:

```qs
from "stdlib/fs.qs" import *
```

## Modules

| Module | Functions | Description |
|--------|-----------|-------------|
| `core.qs` | `env` | Environment variable access |
| `fs.qs` | `exists`, `read`, `write`, `mkdir`, `remove`, `rename`, `is_dir`, `list_dir`, `basename`, `dirname`, `extension`, `join_path` | File system operations and path helpers |
| `http.qs` | `get`, `post`, `put`, `delete`, `patch` | HTTP client (auto-JSON for map bodies) |
| `json.qs` | `parse`, `stringify` | JSON encoding/decoding |
| `log.qs` | `debug`, `info`, `warn`, `error` | Timestamped logging |
| `math.qs` | `min`, `max`, `clamp` | Numeric utilities |
| `net.qs` | `send`, `send_json`, `test` | Raw TCP networking |
| `text.qs` | `lower`, `upper`, `trim`, `split` | String manipulation |
| `time.qs` | `now`, `localtime`, `format`, `parse`, `year`, `month`, `day`, `hour`, `minute`, `second`, `weekday`, `yearday`, `now_str`, `today_str`, `diff_seconds`, `add_seconds`, `add_minutes`, `add_hours`, `add_days` | Date/time operations |
| `websocket.qs` | `connect`, `try_connect`, `send`, `recv`, `send_json`, `recv_json`, `close`, `set_timeout`, `is_open`, `state` | WebSocket client |
| `async.qs` | `spawn`, `run`, `result`, `status`, `run_all` | Cooperative multitasking |
| `os.qs` | `run`, `capture`, `cwd`, `chdir`, `exists` | Process execution and environment |

## Quick Examples

### HTTP

```qs
from "stdlib/http.qs" import get, post

response = get("https://example.com")
print(response["status"], response["body"])

response = post("https://httpbin.org/post", {"key": "val"})
```

### WebSocket

```qs
from "stdlib/websocket.qs" import connect, send, recv, close

ws = connect("ws://echo.websocket.org")
send(ws, "Hello!")
msg = recv(ws)
print(msg)
close(ws)
```

### File System

```qs
from "stdlib/fs.qs" import read, write, exists

if exists("data.txt") {
    content = read("data.txt")
    print(content)
}
write("output.txt", "Hello, file!")
```

### JSON

```qs
from "stdlib/json.qs" import parse, stringify

data = parse('{"name": "Quanto"}')
text = stringify({"name": "Quanto"})
```

### Time

```qs
from "stdlib/time.qs" import now_str, year, month, day

print("Now:", now_str())
print("Date:", year(), "-", month(), "-", day())
```

### Logging

```qs
from "stdlib/log.qs" import info, warn

info("Application started")
warn("Low memory")
```

### Process Execution

```qs
from "stdlib/os.qs" import run, capture, cwd, exists

run("echo Hello")                       # returns exit code
text = capture("git --version")         # returns stdout
print(cwd())                            # current directory
print(exists("gcc"))                    # check if command exists
```

Network note: `http.get` supports `http://` and `https://` URLs on Windows builds.
