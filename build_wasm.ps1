#Requires -Version 5.1
<#
.SYNOPSIS
    Build QuantoScript for WebAssembly with Emscripten.
.DESCRIPTION
    Produces build/qs.js + build/qs.wasm. The interpreter, the bytecode VM and
    the native compiler all run unmodified; only the platform layer differs:

      * no live networking (http/websocket), no FFI, no POSIX regex
      * no process spawning (system/popen)
      * the virtual filesystem is backed by NODERAWFS under Node

    Output is written to build/qs_node.js. Run it with:

        node build/qs_node.js path/to/program.qs
        node build/qs_node.js vm path/to/program.qs
        node build/qs_node.js build path/to/program.qs -o out.qvm
        node build/qs_node.js run out.qvm

    Requires the Emscripten SDK on PATH (emcc). Install with:

        git clone https://github.com/emscripten-core/emsdk
        emsdk install latest
        emsdk activate latest
#>
param(
    [string]$Out = "build/qs_node.js",
    [string]$Emcc = "emcc",
    [switch]$Debug
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

$opt = if ($Debug) { "-O0" } else { "-O2" }
$assert = if ($Debug) { "-sASSERTIONS=2" } else { "" }

Write-Host "Building QuantoScript for WebAssembly ($opt)..."
& $Emcc `
    -std=c99 $opt -Isrc "$root\src\quanto.c" -o "$root\$Out" `
    -sEXIT_RUNTIME=1 `
    -sALLOW_MEMORY_GROWTH=1 `
    -sNODERAWFS=1 `
    -sENVIRONMENT=node `
    -sNODE_HOST_ENV=1 `
    -sSTACK_SIZE=8388608 `
    -sERROR_ON_UNDEFINED_SYMBOLS=0 `
    $assert

if ($LASTEXITCODE -ne 0) {
    Write-Error "emcc failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}
Write-Host "Done: $Out"
