# Minimal HTTP server helpers.

func response(status, body) {
    return {"status": status, "body": body}
}

func route(path, resp) {
    return sys_http_route(path, resp["status"], resp["body"])
}

func serve(host, port) {
    return sys_http_serve(host, port)
}
