# WebSocket echo demo
# Demonstrates the WebSocket client API

from "../stdlib/websocket.qs" import connect, send, recv, close, try_connect, set_timeout, is_open

# Try to connect to an echo server
print("Connecting to echo server...")
ws = try_connect("ws://echo.websocket.org")

if ws == null {
    print("Could not connect to echo server")
    print("This example requires a reachable WebSocket echo server")
} else {
    print("Connected!")

    # Set a 3-second timeout for receives
    set_timeout(ws, 3000)

    # Send a text message
    send(ws, "Hello from QuantoScript!")
    print("Sent: Hello from QuantoScript!")

    # Receive the echo response
    msg = recv(ws)
    print("Received:", msg)

    # Send JSON
    send(ws, '{"action": "echo", "data": [1, 2, 3]}')
    msg = recv(ws)
    print("JSON response:", msg)

    # Check connection state
    print("Connection open:", is_open(ws))

    # Close gracefully
    close(ws)
    print("Connection closed")
}

print("WebSocket demo complete")
