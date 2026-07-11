# Dependencies.cmake — Platform-specific library detection and linking

# ── OpenSSL (required on all platforms) ────────────────────────────
if(NOT OPENSSL_ROOT_DIR AND NOT DEFINED OPENSSL_INCLUDE_DIR)
    if(EXISTS "C:/msys64/ucrt64/include/openssl/ssl.h")
        set(OPENSSL_ROOT_DIR "C:/msys64/ucrt64")
    elseif(EXISTS "C:/msys64/mingw64/include/openssl/ssl.h")
        set(OPENSSL_ROOT_DIR "C:/msys64/mingw64")
    endif()
endif()

find_package(OpenSSL REQUIRED)

if(TARGET OpenSSL::SSL)
    target_link_libraries(qs PRIVATE OpenSSL::SSL OpenSSL::Crypto)
elseif(OPENSSL_FOUND)
    target_include_directories(qs PRIVATE ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qs PRIVATE ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
else()
    message(FATAL_ERROR "OpenSSL not found. Set OPENSSL_ROOT_DIR or install libssl-dev (Linux), openssl (macOS).")
endif()

# ── Platform-specific networking ──────────────────────────────────
if(WIN32)
    target_link_libraries(qs PRIVATE ws2_32 winhttp wininet crypt32)

    if(MSVC)
        target_compile_definitions(qs PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
else()
    find_package(Threads REQUIRED)
    target_link_libraries(qs PRIVATE Threads::Threads)

    if(APPLE)
        if(DEFINED OPENSSL_ROOT_DIR)
            target_link_directories(qs PRIVATE ${OPENSSL_ROOT_DIR}/lib)
        endif()
    else()
        target_link_libraries(qs PRIVATE dl m)
    endif()
endif()
