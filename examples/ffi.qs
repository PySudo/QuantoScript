# FFI (Foreign Function Interface) demo
# Call C functions from shared libraries

# Windows example: GetTickCount from kernel32.dll
# This returns milliseconds since system start

lib = ffi_open("kernel32.dll")
if lib {
    tick_func = ffi_symbol(lib, "GetTickCount")
    if tick_func {
        ms = ffi_call_i(tick_func)
        print("System uptime:", ms, "ms")
    }
    ffi_close(lib)
}

# Simple memory allocation test
ptr = ffi_alloc(100)
if ptr {
    ffi_write_string(ptr, "hello from C memory!")
    print("Read from C:", ffi_read_string(ptr))
    ffi_free(ptr)
}

print("FFI demo complete")