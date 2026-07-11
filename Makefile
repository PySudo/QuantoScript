# QuantoScript Makefile — cross-platform build
#
# Common usage:
#   make                       build build/qs (auto-detects OpenSSL)
#   make test                  build and run the regression suite
#   make clean                 remove build artifacts
#
# Overrides:
#   make OPENSSL_DIR=/path      point at a specific OpenSSL prefix
#   make STATIC_SSL=1           link OpenSSL statically (self-contained binary)
#   make CC=clang               choose the compiler

CC       ?= gcc
CFLAGS   ?= -std=c99 -O2 -Wall -Wextra
CPPFLAGS ?=
LDFLAGS  ?=
LIBS     ?=

VERSION  := 1.0.0
SRC       = src/quanto.c
PARTS     = $(wildcard src/parts/*.inc)
EXE      :=

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
ifeq ($(UNAME_S),Windows)
  EXE  := .exe
  LIBS += $(SSL_LIBS) -lws2_32 -lwinhttp -lwininet -lcrypt32 -lbcrypt
else ifeq ($(UNAME_S),Darwin)
  LIBS += $(SSL_LIBS) -lpthread -lm
else
  LIBS += $(SSL_LIBS) -lpthread -ldl -lm
endif

# MinGW on Windows also reports MINGW*/MSYS* from uname
ifneq (,$(findstring MINGW,$(UNAME_S)))
  EXE  := .exe
  LIBS := $(SSL_LIBS) -lws2_32 -lwinhttp -lwininet -lcrypt32 -lbcrypt
endif
ifneq (,$(findstring MSYS,$(UNAME_S)))
  EXE  := .exe
  LIBS := $(SSL_LIBS) -lws2_32 -lwinhttp -lwininet -lcrypt32 -lbcrypt
endif

OUT = build/qs$(EXE)

.PHONY: all clean test verify run

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
