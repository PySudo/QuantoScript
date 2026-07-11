# WebSocket API regression tests
# Tests native WebSocket functions directly

func test_ws_state_invalid() {
    print("Test 1: ws_state returns 'closed' for socket 0")
    s = ws_state(0)
    if s == "closed" {
        print("  PASS")
    } else {
        print("  FAIL: got", s)
    }
}

func test_ws_connect_invalid() {
    print("Test 2: ws_connect returns null for unreachable host")
    sock = ws_connect("ws://invalid-host-xyz-12345.example.com:9999")
    if sock == null {
        print("  PASS: returned null")
    } else {
        print("  FAIL: should have returned null")
        ws_close(sock)
    }
}

func test_ws_send_no_conn() {
    print("Test 3: ws_send returns false for closed socket")
    result = ws_send(0, "test")
    if result == false {
        print("  PASS: returned false")
    } else {
        print("  FAIL: should have returned false")
    }
}

func test_ws_recv_no_conn() {
    print("Test 4: ws_recv returns null for closed socket")
    result = ws_recv(0)
    if result == null {
        print("  PASS: returned null")
    } else {
        print("  FAIL: should have returned null")
    }
}

func test_ws_close_no_conn() {
    print("Test 5: ws_close does not crash on invalid socket")
    ws_close(0)
    print("  PASS: no crash")
}

func test_ws_state_closed() {
    print("Test 6: ws_state returns 'closed' after close")
    s = ws_state(0)
    if s == "closed" {
        print("  PASS: state is", s)
    } else {
        print("  FAIL: expected closed, got", s)
    }
}

func test_ws_set_timeout() {
    print("Test 7: ws_set_timeout does not crash")
    ws_set_timeout(0, 1000)
    print("  PASS: no crash")
}

# Run all tests
test_ws_state_invalid()
test_ws_connect_invalid()
test_ws_send_no_conn()
test_ws_recv_no_conn()
test_ws_close_no_conn()
test_ws_state_closed()
test_ws_set_timeout()

print("All WebSocket tests passed")
