[« Previous: Imports](13-imports.md) · [Guide Index](README.md) · [Next: Built-in Functions »](15-built-in-functions.md)

# 14. Standard Library

QuantoScript ships with a set of modules under `stdlib/`. Import only what you
need:

```qscript
from "stdlib/http.qs" import get, post
from "stdlib/text.qs" import lower, split
```

Or bring in a whole module with `import *`:

```qscript
from "stdlib/fs.qs" import *
```

> Modules marked **experimental** are not yet production-ready. See
> [Style and Limits](20-style-and-limits.md#known-limitations).

---

## Module overview

| Module | Purpose |
|--------|---------|
| [`math.qs`](#math) | Numeric helpers |
| [`text.qs`](#text) | String helpers |
| [`fs.qs`](#fs) | File system and path helpers |
| [`os.qs`](#os) | Process execution and working directory |
| [`json.qs`](#json) | JSON encode/decode |
| [`time.qs`](#time) | Timestamps and formatting |
| [`log.qs`](#log) | Timestamped logging |
| [`random.qs`](#random) | Reproducible pseudo-random numbers |
| [`core.qs`](#core) | Environment access |
| [`http.qs`](#http) | HTTP client |
| [`url.qs`](#url) | URL encoding helpers |
| [`net.qs`](#net) | Raw TCP networking |
| [`websocket.qs`](#websocket) | WebSocket client |
| [`async.qs`](#async) | Cooperative tasks *(experimental)* |

---

## math

Numeric helpers.

| Function | Description |
|----------|-------------|
| `min(list)` | smallest element of a list |
| `max(list)` | largest element of a list |
| `clamp(value, low, high)` | keep `value` within `[low, high]` |

```qscript
from "stdlib/math.qs" import min, max, clamp

print(min([3, 1, 9]))     # 1
print(max([3, 1, 9]))     # 9
print(clamp(12, 1, 10))   # 10
```

---

## text

String helpers.

| Function | Description |
|----------|-------------|
| `lower(text)` | lowercase copy |
| `upper(text)` | uppercase copy |
| `trim(text)` | remove surrounding whitespace |
| `split(text, separator)` | split into a list |

```qscript
from "stdlib/text.qs" import lower, upper, trim, split

print(lower("HELLO"))                 # hello
print(upper("hello"))                 # HELLO
print(trim("  QuantoScript  "))       # QuantoScript
print(split("red,green,blue", ","))   # ['red', 'green', 'blue']
```

---

## fs

File system and path helpers.

| Function | Description |
|----------|-------------|
| `exists(path)` | does the path exist? |
| `read(path)` | read a file as text |
| `write(path, text)` | write text to a file |
| `mkdir(path)` | create a directory |
| `remove(path)` | delete a file or directory |
| `rename(old, new)` | rename or move |
| `is_dir(path)` | is it a directory? |
| `list_dir(path)` | list directory entries |
| `basename(path)` | final path component |
| `dirname(path)` | path without the final component |
| `extension(path)` | file extension |
| `join_path(a, b)` | join two path parts |

```qscript
from "stdlib/fs.qs" import exists, read, write, list_dir

path = "notes.txt"
write(path, "hello stdlib")
print(exists(path))    # true
print(read(path))      # hello stdlib
print(list_dir("."))
```

---

## os

Process execution and working-directory helpers.

| Function | Description |
|----------|-------------|
| `run(command)` | run a shell command; returns its exit code |
| `capture(command)` | run a command; returns its stdout |
| `cwd()` | current working directory |
| `chdir(path)` | change the working directory |
| `exists(command)` | is an executable available? |

```qscript
from "stdlib/os.qs" import run, capture, cwd, exists

print(run("echo Hello"))           # 0
print(capture("git --version"))
print(cwd())
print(exists("gcc"))               # true / false
```

> `run` and `capture` execute shell commands directly. Never pass untrusted
> input to them without validation.

---

## json

Encode and decode JSON.

| Function | Description |
|----------|-------------|
| `parse(text)` | JSON string → list/map/value |
| `stringify(value)` | value → JSON string |

```qscript
from "stdlib/json.qs" import parse, stringify

data = parse('{"name": "Sara", "age": 20, "active": true}')
print(data["name"])       # Sara

print(stringify(map("name" = "Ali", "age" = 25)))
```

---

## time

Timestamps and formatting.

| Function | Description |
|----------|-------------|
| `now()` | current timestamp (seconds since epoch) |
| `sleep(milliseconds)` | pause execution for the given number of milliseconds |
| `localtime()` | local time as a map (`year`, `month`, `day`, `hour`, `min`, `sec`, `wday`, `yday`) |
| `format(timestamp, fmt)` | format with a `strftime` pattern |
| `parse(text, fmt)` | parse a time string |
| `year()`, `month()`, `day()` | current date parts |
| `hour()`, `minute()`, `second()` | current time parts |
| `weekday()`, `yearday()` | day of week / day of year |
| `now_str()` | formatted current date and time |
| `today_str()` | formatted current date |
| `diff_seconds(a, b)` | absolute difference in seconds |
| `add_seconds/minutes/hours/days(t, n)` | shift a timestamp |

```qscript
from "stdlib/time.qs" import now, localtime, format, now_str, today_str, add_days

print(now_str())
print(today_str())

t = localtime()
print(t["year"], t["month"], t["day"])

tomorrow = add_days(now(), 1)
print(format(tomorrow, "%Y-%m-%d"))
```

---

## log

Timestamped logging to standard output.

| Function | Description |
|----------|-------------|
| `debug(msg)` | `[DEBUG]` line |
| `info(msg)` | `[INFO]` line |
| `warn(msg)` | `[WARN]` line |
| `error(msg)` | `[ERROR]` line |

```qscript
from "stdlib/log.qs" import info, warn, error

info("Application started")
warn("Low disk space")
error("Something failed")
```

---

## random

Reproducible pseudo-random numbers (PCG32). Seeding with the same value
produces the same sequence across platforms.

| Function | Description |
|----------|-------------|
| `seed(value)` | set the random seed |
| `randint(min, max)` | random integer, inclusive on both ends |
| `randrange(start, stop)` | random integer in `[start, stop)` |
| `choice(list)` | random element |
| `shuffle(list)` | new shuffled list |
| `sample(list, k)` | `k` unique random elements |

> The built-ins `random()` (float in `[0, 1)`) and `random_seed(value)` are also
> available without an import — see
> [Built-in Functions](15-built-in-functions.md).

```qscript
from "stdlib/random.qs" import seed, randint, choice, shuffle, sample

seed(42)
print(randint(1, 100))         # reproducible with seed(42)
print(choice(["red", "green", "blue"]))
print(shuffle([1, 2, 3, 4, 5]))
print(sample([10, 20, 30, 40, 50], 3))
```

---

## core

| Function | Description |
|----------|-------------|
| `env(name)` | value of an environment variable |

```qscript
from "stdlib/core.qs" import env
print(env("PATH"))
```

---

## http

HTTP client. `get`, `put`, `delete`, and `patch` accept a URL; `post` and `patch`
additionally accept a body, which may be a string or a map (auto-converted to
JSON).

| Function | Description |
|----------|-------------|
| `get(url)` | HTTP GET |
| `post(url, body)` | HTTP POST |
| `put(url, body)` | HTTP PUT |
| `delete(url)` | HTTP DELETE |
| `patch(url, body)` | HTTP PATCH |

Responses are maps with `ok`, `status`, `error`, and `body` keys. When a
request cannot be completed (DNS failure, timeout, TLS error) `ok` is `false`,
`status` is `0` and `body` is empty; `error` then carries the underlying
transport error code — check `len(response["body"])` before parsing.

```qscript
from "stdlib/http.qs" import get, post

response = get("https://example.com/")
print(response["status"])
print(response["body"][:40])

response = post("https://httpbin.org/post", {"name": "QuantoScript"})
print(response["status"])
```

> `http://` works everywhere; `https://` requires the networking stack included
> in the prebuilt binaries.

---

## url

Percent-encoding helpers for building URLs and query strings.

| Function | Description |
|----------|-------------|
| `url_encode(text)` | percent-encode a value for use in a URL |
| `url_decode(text)` | reverse of `url_encode` (`+` becomes a space) |

`url_encode` escapes everything except the unreserved characters
`A–Z`, `a–z`, `0–9`, `-`, `_`, `.`, and `~`, so the result is always safe to
place in a query parameter.

```qscript
from "stdlib/url.qs" import url_encode, url_decode

print(url_encode("654صثگ \\/,m.:;"))   # 654%D8%B5%D8%AB%DA%AF%20%5C%2F%2Cm.%3A%3B
print(url_decode("a%20b%26c%3Dd"))     # a b&c=d
```

> The HTTP client already percent-encodes characters that are invalid in a URL
> (spaces, non-ASCII bytes, backslashes), so simple URLs just work. When a value
> may contain reserved characters such as `&`, `=`, or `+`, encode it explicitly
> with `url_encode` before adding it to the URL.

---

## net

Raw TCP networking.

| Function | Description |
|----------|-------------|
| `send(host, port, message)` | send a string, return the response |
| `send_json(host, port, data)` | send a map as JSON, return the parsed response |
| `test(host, port)` | is the host reachable? |

```qscript
from "stdlib/net.qs" import send, send_json, test

print(test("example.com", 80))
```

---

## websocket

A stateful WebSocket client.

| Function | Description |
|----------|-------------|
| `connect(url)` | connect; raises on failure |
| `try_connect(url)` | connect; returns `null` on failure |
| `send(ws, message)` | send text |
| `recv(ws)` | receive text (blocks until a message or timeout) |
| `send_json(ws, data)` | send a map as JSON |
| `recv_json(ws)` | receive and parse JSON |
| `set_timeout(ws, ms)` | set the receive timeout |
| `is_open(ws)` | is the connection open? |
| `state(ws)` | `"open"`, `"closed"`, or `"error"` |
| `close(ws)` | close gracefully |

```qscript
from "stdlib/websocket.qs" import try_connect, send, recv, close, set_timeout

ws = try_connect("wss://echo.websocket.org")
if ws == null {
    print("could not connect")
} else {
    set_timeout(ws, 3000)
    send(ws, "Hello from QuantoScript!")
    print(recv(ws))
    close(ws)
}
```

---

## async

Cooperative task helpers. **Experimental** — single-task use is reliable, but the
queue can misbehave with multiple tasks. See
[Style and Limits](20-style-and-limits.md#known-limitations).

| Function | Description |
|----------|-------------|
| `spawn(fn)` | create a task from a lambda; returns a task id |
| `run(task_id)` | run a task |
| `result(task_id)` | result of a finished task |
| `status(task_id)` | task status |
| `run_all()` | run all ready tasks cooperatively |
| `spawn_and_run(fn)` | spawn, run, and return the result |

```qscript
from "stdlib/async.qs" import spawn_and_run, run, result

task = spawn(fn() -> "done")
run(task)
print(result(task))
```

---

## Next steps

[Built-in Functions](15-built-in-functions.md) lists everything available without
an import.

---

[« Previous: Imports](13-imports.md) · [Guide Index](README.md) · [Next: Built-in Functions »](15-built-in-functions.md)
