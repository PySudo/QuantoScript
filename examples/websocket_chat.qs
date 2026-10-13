# WebSocket chat client example
# Demonstrates error handling with try/oops and connection lifecycle

from "../stdlib/websocket.qs" import connect, try_connect, send, recv, close, is_open, set_timeout, state

# Helper: safe send with error reporting
func safe_send(ws, message) {
    try {
        send(ws, message)
        return true
    } oops e {
        print("Send error:", e)
        return false
    }
}

# Helper: safe receive with error reporting
func safe_recv(ws) {
    try {
        return recv(ws)
    } oops e {
        print("Receive error:", e)
        return null
    }
}

# Main chat loop
func chat(server_url:string) {
    ws = try_connect(server_url)

    if ws == null {
        print("Failed to connect to", server_url)
        return null
    }

    set_timeout(ws, 2000)
    print("Connected to", server_url)

    # Some echo servers (echo.websocket.org) send a greeting on connect;
    # consume it so the replies below line up with the messages we send.
    safe_recv(ws)

    # Send a few messages
    messages = ["hello", "how are you?", "testing 1 2 3", "goodbye"]

    repeat msg -> messages {
        print("Sending:", msg)

        ok = safe_send(ws, msg)
        if not ok {
            print("Connection lost during send")
            break
        }

        # Try to receive a response
        response = safe_recv(ws)
        if response == null {
            print("No response or timeout")
        } else {
            print("Got:", response)
        }
    }

    # Check final state
    print("Final state:", state(ws))

    close(ws)
    print("Disconnected")
}

# Run
chat("wss://echo.websocket.org")
