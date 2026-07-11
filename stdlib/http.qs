# HTTP helpers.
# Supports http:// URLs, and https:// on Windows builds.
# Methods: get, post, put, delete, patch
# Body can be a string or a map (auto-converted to JSON)

from "json.qs" import stringify

func get(url:string) {
    return sys_http_get(url)
}

func post(url:string, body:string|map) {
    if isinstance(body, "map") {
        return sys_http_post(url, stringify(body))
    }
    return sys_http_post(url, body)
}

func put(url:string, body:string|map) {
    if isinstance(body, "map") {
        return sys_http_put(url, stringify(body))
    }
    return sys_http_put(url, body)
}

func delete(url:string) {
    return sys_http_delete(url)
}

func patch(url:string, body:string|map) {
    if isinstance(body, "map") {
        return sys_http_patch(url, stringify(body))
    }
    return sys_http_patch(url, body)
}
