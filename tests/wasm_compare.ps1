#Requires -Version 5.1
# Compare native qs.exe output against the Emscripten WASM build (node) for
# every tests/*.qs and examples/*.qs file, comparing stdout and stderr
# separately (interleaving of the two is not deterministic).
$ErrorActionPreference = "SilentlyContinue"
$root = Split-Path $PSScriptRoot -Parent
$qs = Join-Path $root "build\qs.exe"
$node = "C:\Users\Lpmarket\emsdk\node\24.19.0_64bit\node.exe"
$js = Join-Path $root "build\qs_node.js"

function RunCapture([string]$exe, [string]$arg, [string]$tag) {
    $outF = Join-Path $env:TEMP "wc_${tag}_out.txt"
    $errF = Join-Path $env:TEMP "wc_${tag}_err.txt"
    $p = New-Object System.Diagnostics.ProcessStartInfo
    $p.FileName = $exe
    $p.Arguments = $arg
    $p.WorkingDirectory = $root
    $p.RedirectStandardOutput = $true
    $p.RedirectStandardError = $true
    $p.UseShellExecute = $false
    $p.CreateNoWindow = $true
    $proc = [System.Diagnostics.Process]::Start($p)
    $o = $proc.StandardOutput.ReadToEndAsync()
    $e = $proc.StandardError.ReadToEndAsync()
    if (-not $proc.WaitForExit(30000)) { try { $proc.Kill() } catch {}; return @{ Exit = -1; Out = "TIMEOUT"; Err = "" } }
    return @{ Exit = $proc.ExitCode; Out = $o.Result; Err = $e.Result }
}

function Normalize([string]$t) {
    if ($null -eq $t) { return "" }
    $t = $t -replace "`r`n", "`n"
    # Wall-clock dependent lines: timestamps, dates, seconds counters.
    $t = [regex]::Replace($t, '(?im)^(timestamp|now|today|tomorrow|custom)\s*:.*$', '$1: <TIME>')
    $t = [regex]::Replace($t, '(?im)^(sec|second|min|minute|hour)\s*:\s*\d+.*$', '$1: <TIME>')
    $t = [regex]::Replace($t, '(?im)^.*\d{4}[-/]\d{1,2}[-/]\d{1,2}.*$', '<TIME>')
    $t = [regex]::Replace($t, '\[\d{2}:\d{2}:\d{2}\]', '[<TIME>]')
    $t = [regex]::Replace($t, '\b\d{9,}\b', '<N>')
    return $t
}

# Files whose behaviour legitimately differs on the WebAssembly target because
# the platform lacks live networking, FFI, POSIX regex, or host process spawn.
$platformLimited = @(
    "test_websocket.qs", "websocket.qs", "websocket_chat.qs",
    "test_regex_builtins.qs",
    "http.qs", "http_methods.qs",
    "ffi.qs",
    "os.qs",
    "test_stdlib_final.qs", "test_vm_imports.qs", "test_vm_import_regression.qs"
)

$files = @()
Get-ChildItem (Join-Path $root "tests\*.qs") | ForEach-Object { $files += $_.FullName }
Get-ChildItem (Join-Path $root "examples\*.qs") | ForEach-Object { $files += $_.FullName }

$pass = 0; $plat = 0; $diff = 0; $mism = @()
foreach ($f in $files) {
    $name = Split-Path $f -Leaf
    $a = RunCapture $qs "`"$f`"" "n"
    $b = RunCapture $node "`"$js`" `"$f`"" "w"
    $ao = Normalize $a.Out; $ae = Normalize $a.Err
    $bo = Normalize $b.Out; $be = Normalize $b.Err
    if ($ao -eq $bo -and $ae -eq $be -and $a.Exit -eq $b.Exit) { $pass++ }
    elseif ($platformLimited -contains $name) { $plat++ }
    else { $diff++; $mism += @{ Name = $name; A = $a; B = $b } }
}
Write-Host "MATCH: $pass / $($files.Count)   platform-limited: $plat   unexpected: $diff"
Write-Host "=== unexpected differences (exit native/wasm) ==="
foreach ($m in $mism) { Write-Host ("  {0}  exit={1}/{2}" -f $m.Name, $m.A.Exit, $m.B.Exit) }
if ($diff -eq 0) { exit 0 } else { exit 1 }
