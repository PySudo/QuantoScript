# File system helpers.

func exists(path:string) {
    return sys_exists(path)
}

func read(path:string) {
    return sys_read(path)
}

func write(path:string, text:string) {
    return sys_write(path, text)
}

func mkdir(path:string) {
    return sys_mkdir(path)
}

func remove(path:string) {
    return sys_remove(path)
}

func rename(old_path:string, new_path:string) {
    return sys_rename(old_path, new_path)
}

func is_dir(path:string) {
    return sys_is_dir(path)
}

func list_dir(path:string) {
    return sys_list_dir(path)
}

# Path helpers (pure QuantoScript)

func basename(path:string) {
    parts = path.split("/")
    if len(parts) == 0 {
        return path
    }
    return parts[len(parts)]
}

func dirname(path:string) {
    parts = path.split("/")
    if len(parts) <= 1 {
        return "."
    }
    result = ""
    i = 1
    while i < len(parts) {
        if i > 1 {
            result += "/"
        }
        result += parts[i]
        i = i + 1
    }
    return result
}

func extension(path:string) {
    name = basename(path)
    parts = name.split(".")
    if len(parts) <= 1 {
        return ""
    }
    return parts[len(parts)]
}

func join_path(a:string, b:string) {
    if a == "" {
        return b
    }
    last = a[len(a)]
    if last == "/" || last == "\\" {
        return a + b
    }
    return a + "/" + b
}
