# Core helpers.
# Environment variable access.

from "fs.qs" import exists, read
from "text.qs" import split, trim

func env(name:string) {
    return sys_env(name)
}

# Parse ".env"-style text into a map of key -> value.
# Blank lines and lines starting with '#' are skipped. Spaces around the key
# and value are trimmed, and any '=' inside a value is preserved.
func parse_env(text) {
    values = map()
    repeat line -> split(text, "\n") {
        line = trim(line)
        if line == "" {
            continue
        }
        if line[1] == "#" {
            continue
        }
        parts = split(line, "=")
        if len(parts) < 2 {
            continue
        }
        key = trim(parts[1])
        value = parts[2]
        i = 3
        while i <= len(parts) {
            value = value + "=" + parts[i]
            i = i + 1
        }
        values[key] = trim(value)
    }
    return values
}

# Load a ".env" file into a map of key -> value.
# Returns an empty map when the file does not exist.
func load_env(path) {
    if not exists(path) {
        return map()
    }
    return parse_env(read(path))
}
