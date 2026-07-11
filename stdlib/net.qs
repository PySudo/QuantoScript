# TCP networking helpers.
# Send data over TCP and receive responses.

from "json.qs" import stringify, parse

# Send a string message and receive response
func send(host:string, port:int, message:string) {
    return sys_tcp_send(host, port, message)
}

# Send a map as JSON and receive parsed response
func send_json(host:string, port:int, data:map) {
    raw = sys_tcp_send(host, port, stringify(data))
    if raw == "" {
        return raw
    }
    result = parse(raw)
    return result
}

# Test if a host:port is reachable (returns true/false)
func test(host:string, port:int) {
    result = sys_tcp_send(host, port, "")
    return result != ""
}
