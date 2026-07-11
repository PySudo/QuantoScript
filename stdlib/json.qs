# JSON helpers.

func parse(text:string) {
    return sys_json_parse(text)
}

func stringify(value) {
    return sys_json_stringify(value)
}
