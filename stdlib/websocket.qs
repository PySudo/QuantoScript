# WebSocket client library for QuantoScript.
# Provides a clean, stateful WebSocket API with error handling.
#
# Usage:
#   from "stdlib/websocket.qs" import connect, send, recv, close
#
#   ws = connect("ws://echo.websocket.org")
#   send(ws, "hello")
#   msg = recv(ws)
#   close(ws)

# Internal: create a WebSocket connection object
func _new_socket(sock) {
    return {
        "socket": sock,
        "connected": sock != null,
        "timeout_ms": 5000
    }
}

# Connect to a WebSocket server.
# Returns a connection object on success.
# Throws on failure.
func connect(url:string) {
    sock = ws_connect(url)
    if sock == null {
        oops("WebSocket connection failed: " + url)
    }
    return _new_socket(sock)
}

# Try to connect without throwing.
# Returns the connection object or null.
func try_connect(url:string) {
    sock = ws_connect(url)
    if sock == null {
        return null
    }
    return _new_socket(sock)
}

# Set receive timeout in milliseconds (default: 5000).
func set_timeout(ws, ms:int) {
    ws["timeout_ms"] = ms
    ws_set_timeout(ws["socket"], ms)
}

# Send a text message.
# Throws on error.
func send(ws, message:string) {
    if not ws["connected"] {
        oops("WebSocket is not connected")
    }
    result = ws_send(ws["socket"], message)
    if not result {
        ws["connected"] = false
        oops("WebSocket send failed")
    }
}

# Receive a text message.
# Blocks until a message arrives or timeout expires.
# Throws on connection error.
func recv(ws) {
    if not ws["connected"] {
        oops("WebSocket is not connected")
    }
    msg = ws_recv(ws["socket"])
    if msg == null {
        ws["connected"] = false
        oops("WebSocket connection lost or timed out")
    }
    return msg
}

# Send a JSON object as a text message.
func send_json(ws, data:map) {
    send(ws, json.stringify(data))
}

# Receive and parse a JSON message.
func recv_json(ws) {
    msg = recv(ws)
    return json.parse(msg)
}

# Check if the connection is still open.
func is_open(ws) {
    if not ws["connected"] {
        return false
    }
    state = ws_state(ws["socket"])
    if state != "open" {
        ws["connected"] = false
        return false
    }
    return true
}

# Get the raw connection state string: "open", "closed", or "error".
func state(ws) {
    if not ws["connected"] {
        return "closed"
    }
    return ws_state(ws["socket"])
}

# Close the connection gracefully.
func close(ws) {
    if ws["connected"] {
        ws_send(ws["socket"], "")  # ensure socket is valid
        ws_close(ws["socket"])
        ws["connected"] = false
    }
}
