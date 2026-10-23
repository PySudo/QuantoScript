# QuantoScript Makefile — cross-platform build
#
# Common usage:
#   make                       build build/qs (auto-detects OpenSSL)
#   make test                  build and run the regression suite
#   make wasm                  build the WebAssembly target (needs emcc)
#   make wasm-test             compare the wasm build against the native binary
#   make clean                 remove build artifacts
#
# Overrides:
#   make OPENSSL_DIR=/path      point at a specific OpenSSL prefix
#   make STATIC_SSL=1           link OpenSSL statically (self-contained binary)
#   make CC=clang               choose the compiler
#   make EMCC=emcc              choose the Emscripten front-end for `make wasm`

CC       ?= gcc
CFLAGS   ?= -std=c99 -O2 -Wall -Wextra
CPPFLAGS ?=
LDFLAGS  ?=
LIBS     ?=
EMCC     ?= emcc

VERSION  := 2.5.0
SRC       = src/quanto.c
PARTS     = $(wildcard src/parts/*.inc)
EXE      :=

# Expose the version to the C sources (src/parts/types.inc provides a fallback).
CPPFLAGS += -DQS_VERSION=\"$(VERSION)\"

UNAME_S  := $(shell uname -s 2>/dev/null || echo Windows)
PKG_CONFIG ?= pkg-config

# ── OpenSSL discovery ──────────────────────────────────────────────
# Priority: explicit OPENSSL_DIR > Homebrew (macOS) > pkg-config > default -lssl
ifeq ($(OPENSSL_DIR),)
  ifeq ($(UNAME_S),Darwin)
    OPENSSL_DIR := $(shell brew --prefix openssl@3 2>/dev/null || brew --prefix openssl 2>/dev/null)
  endif
endif

ifneq ($(OPENSSL_DIR),)
  CPPFLAGS += -I$(OPENSSL_DIR)/include
  LDFLAGS  += -L$(OPENSSL_DIR)/lib
  ifeq ($(STATIC_SSL),1)
    SSL_LIBS := $(OPENSSL_DIR)/lib/libssl.a $(OPENSSL_DIR)/lib/libcrypto.a
  else
    SSL_LIBS := -lssl -lcrypto
  endif
else ifneq ($(shell $(PKG_CONFIG) --exists openssl 2>/dev/null && echo yes),)
  CPPFLAGS += $(shell $(PKG_CONFIG) --cflags openssl)
  SSL_LIBS := $(shell $(PKG_CONFIG) --libs openssl)
else
  SSL_LIBS := -lssl -lcrypto
endif

# ── Platform-specific settings ─────────────────────────────────────
# POSIX <regex.h> is provided by libc on Linux/macOS, and by libsystre on MinGW.
# REGEX_LIB is only used by the MinGW branches below; there it links the
# systre -> tre -> gettext -> iconv archives statically so the Windows binary
# has no extra DLL dependencies (matching the self-contained release build).
REGEX_H_AVAILABLE := $(shell echo '\#include <regex.h>' | $(CC) -E -x c - >/dev/null 2>&1 && echo yes)
ifeq ($(REGEX_H_AVAILABLE),yes)
REGEX_LIB := -l:libregex.a -l:libtre.a -l:libintl.a -l:libiconv.a
endif

ifeq ($(UNAME_S),Windows)
  EXE  := .exe
  LIBS += $(SSL_LIBS) -lws2_32 -lwinhttp -lwininet -lcrypt32 -lbcrypt $(REGEX_LIB)
else ifeq ($(UNAME_S),Darwin)
  LIBS += $(SSL_LIBS) -lpthread -lm
else
  LIBS += $(SSL_LIBS) -lpthread -ldl -lm
endif

# MinGW on Windows also reports MINGW*/MSYS* from uname
ifneq (,$(findstring MINGW,$(UNAME_S)))
  EXE  := .exe
  LIBS := $(SSL_LIBS) -lws2_32 -lwinhttp -lwininet -lcrypt32 -lbcrypt $(REGEX_LIB)
endif
ifneq (,$(findstring MSYS,$(UNAME_S)))
  EXE  := .exe
  LIBS := $(SSL_LIBS) -lws2_32 -lwinhttp -lwininet -lcrypt32 -lbcrypt $(REGEX_LIB)
endif

OUT = build/qs$(EXE)

# The WebAssembly build: emcc compiles the whole program to build/qs_node.js +
# build/qs_node.wasm. The engine is unchanged; the platform layer reports a
# clear error for networking, FFI, POSIX regex, and process spawning.
WASM_OUT = build/qs_node.js
EMCC_BASE_FLAGS = -std=c99 -O2 -Isrc \
	-sEXIT_RUNTIME=1 \
	-sALLOW_MEMORY_GROWTH=1 \
	-sNODERAWFS=1 \
	-sENVIRONMENT=node \
	-sNODE_HOST_ENV=1 \
	-sSTACK_SIZE=8388608 \
	-sERROR_ON_UNDEFINED_SYMBOLS=0

.PHONY: all clean test verify run wasm wasm-debug wasm-test

all: $(OUT)

$(OUT): $(SRC) $(PARTS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(CPPFLAGS) -Isrc $(SRC) -o $(OUT) $(LDFLAGS) $(LIBS)
	@echo "built $(OUT)"

clean:
	rm -rf build/

run: $(OUT)
	$(OUT)

test: $(OUT)
	@echo "=== Regression Suite ==="
	@if command -v pwsh >/dev/null 2>&1; then \
		pwsh -ExecutionPolicy Bypass -File tests/run_regression.ps1; \
	elif command -v powershell >/dev/null 2>&1; then \
		powershell -ExecutionPolicy Bypass -File tests/run_regression.ps1; \
	else \
		echo "ERROR: PowerShell (pwsh or powershell) is required to run the regression suite."; \
		echo "Install PowerShell Core: https://aka.ms/pscore"; \
		exit 1; \
	fi

verify: test
	@echo "=== Verify complete ==="

wasm:
	@mkdir -p build
	$(EMCC) $(EMCC_BASE_FLAGS) $(SRC) -o $(WASM_OUT)
	@echo "built $(WASM_OUT)"

wasm-debug:
	@mkdir -p build
	$(EMCC) $(filter-out -O2,$(EMCC_BASE_FLAGS)) -O0 -sASSERTIONS=2 $(SRC) -o $(WASM_OUT)
	@echo "built $(WASM_OUT) (debug)"

wasm-test: $(OUT) wasm
	@echo "=== WebAssembly parity check ==="
	@if command -v pwsh >/dev/null 2>&1; then \
		QS_SOURCE_DIR="$(CURDIR)/src" pwsh -ExecutionPolicy Bypass -File tests/wasm_compare.ps1; \
	else \
		QS_SOURCE_DIR="$(CURDIR)/src" powershell -ExecutionPolicy Bypass -File tests/wasm_compare.ps1; \
	fi
