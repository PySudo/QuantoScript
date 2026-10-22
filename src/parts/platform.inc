#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#endif

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__wasm__) || defined(__wasi__)
#define QS_WASM 1
#else
#define QS_WASM 0
#endif

#if !QS_WASM
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

#if QS_WASM
#define QS_HAVE_POSIX_REGEX 0
#elif defined(_MSC_VER)
#define QS_HAVE_POSIX_REGEX 0
#elif defined(__has_include)
#if __has_include(<regex.h>)
#define QS_HAVE_POSIX_REGEX 1
#else
#define QS_HAVE_POSIX_REGEX 0
#endif
#else
#define QS_HAVE_POSIX_REGEX 1
#endif

#if QS_WASM
/* WebAssembly builds have no sockets, TLS, dynamic loading, or FFI.
 * The core language runs unmodified; the network/ffi builtins report a
 * clear runtime error (see native.inc). The virtual filesystem supports
 * the usual POSIX stat/dirent/unistd calls. */
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
typedef int qs_socket_t;
#define QS_INVALID_SOCKET -1
#define qs_close_socket(fd) ((void)(fd))
#define qs_socket_errno() 0
#define QS_WOULDBLOCK 0
#define QS_IN_PROGRESS 0
#define QS_TIMEDOUT 0
typedef int qs_timeout_t;
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <direct.h>
#if QS_HAVE_POSIX_REGEX
#include <regex.h>
#endif
typedef SOCKET qs_socket_t;
#define QS_INVALID_SOCKET INVALID_SOCKET
#define qs_close_socket closesocket
#define qs_socket_errno() WSAGetLastError()
#define QS_WOULDBLOCK WSAEWOULDBLOCK
#define QS_IN_PROGRESS WSAEINPROGRESS
#define QS_TIMEDOUT WSAETIMEDOUT
typedef DWORD qs_timeout_t;
#define QS_INTERNET_OPEN_TYPE_DIRECT 1
#define QS_INTERNET_FLAG_RELOAD 0x80000000
#define QS_INTERNET_FLAG_NO_CACHE_WRITE 0x04000000
#define QS_INTERNET_FLAG_SECURE 0x00000000
#define QS_INTERNET_FLAG_NO_UI 0x00000200
#define QS_INTERNET_FLAG_IGNORE_CERT_CN_INVALID 0x00001000
#define QS_INTERNET_FLAG_IGNORE_CERT_DATE_INVALID 0x00002000
#define QS_HTTP_QUERY_STATUS_CODE 19
#define QS_HTTP_QUERY_RAW_HEADERS_CRLF 22
#define QS_HTTP_QUERY_FLAG_NUMBER 0x20000000
WINBASEAPI HINTERNET WINAPI InternetOpenA(LPCSTR, DWORD, LPCSTR, LPCSTR, DWORD);
WINBASEAPI HINTERNET WINAPI InternetOpenUrlA(HINTERNET, LPCSTR, LPCSTR, DWORD, DWORD, DWORD_PTR);
WINBASEAPI BOOL WINAPI InternetReadFile(HINTERNET, LPVOID, DWORD, LPDWORD);
WINBASEAPI BOOL WINAPI InternetCloseHandle(HINTERNET);
WINBASEAPI BOOL WINAPI HttpQueryInfoA(HINTERNET, DWORD, LPVOID, LPDWORD, LPDWORD);
#else
#include <fcntl.h>
#include <dirent.h>
#include <dlfcn.h>
#include <netdb.h>
#include <pthread.h>
#include <regex.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
typedef int qs_socket_t;
#define QS_INVALID_SOCKET -1
#define qs_close_socket close

#define qs_socket_errno() errno
#define QS_WOULDBLOCK EWOULDBLOCK
#define QS_IN_PROGRESS EINPROGRESS
#define QS_TIMEDOUT ETIMEDOUT

typedef struct timeval qs_timeout_t;
#endif
