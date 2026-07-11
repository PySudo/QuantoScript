cmake_minimum_required(VERSION 3.15)

project(QuantoScript
    VERSION 0.1.0
    DESCRIPTION "A small, readable scripting language with a dual execution engine"
    HOMEPAGE_URL "https://github.com/PySudo/QuantoScript"
    LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(QS_STATIC_OPENSSL "Statically link OpenSSL for a self-contained binary" OFF)
if(QS_STATIC_OPENSSL)
    set(OPENSSL_USE_STATIC_LIBS TRUE)
endif()

# On macOS, Homebrew installs OpenSSL keg-only (off the default search path).
if(APPLE AND NOT OPENSSL_ROOT_DIR)
    execute_process(
        COMMAND brew --prefix openssl@3
        OUTPUT_VARIABLE _brew_openssl
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(_brew_openssl AND EXISTS "${_brew_openssl}")
        set(OPENSSL_ROOT_DIR "${_brew_openssl}")
    endif()
endif()

# ── Target ─────────────────────────────────────────────────────────
add_executable(qs src/quanto.c)
target_include_directories(qs PRIVATE src)
set_target_properties(qs PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(NOT MSVC)
    target_compile_options(qs PRIVATE -O2 -Wall -Wextra)
endif()

# Platform library detection and linking (OpenSSL, threads, networking).
include(${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake/Dependencies.cmake)

# ── Install rules ──────────────────────────────────────────────────
include(GNUInstallDirs)
set(QS_DATADIR "${CMAKE_INSTALL_DATADIR}/quantoscript")

install(TARGETS qs RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY stdlib/   DESTINATION ${QS_DATADIR}/stdlib   FILES_MATCHING PATTERN "*.qs")
install(DIRECTORY examples/ DESTINATION ${QS_DATADIR}/examples FILES_MATCHING PATTERN "*.qs")
install(FILES README.md CHANGELOG.md CONTRIBUTING.md LICENSE
        DESTINATION ${CMAKE_INSTALL_DOCDIR})

# ── Packaging (CPack) ──────────────────────────────────────────────
set(CPACK_PACKAGE_NAME "QuantoScript")
set(CPACK_PACKAGE_VENDOR "QuantoScript Project")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "QuantoScript")

if(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

# Debian package metadata (used when -G DEB is requested)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "QuantoScript Project")
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libssl3 | libssl1.1")

include(CPack)
