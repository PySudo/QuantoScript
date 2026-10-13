#Requires -Version 5.1
<#
.SYNOPSIS
    QuantoScript Regression Test Runner
.DESCRIPTION
    Runs every .qs test on all three execution paths: qs, qs vm, qs build+run.
    Compares outputs and reports pass/fail/crash/timeout/mismatch.
.PARAMETER Verbose
    Show output for all tests, not just failures.
.PARAMETER StopOnFailure
    Stop immediately on first failure.
.PARAMETER InterpreterOnly
    Only run the qs interpreter path.
.PARAMETER VMOnly
    Only run the qs vm path.
.PARAMETER QVMOnly
    Only run the qs build+run path.
.PARAMETER Path
    Run a single file or all files in a directory.
#>
param(
    [switch]$Verbose,
    [switch]$StopOnFailure,
    [switch]$InterpreterOnly,
    [switch]$VMOnly,
    [switch]$QVMOnly,
    [string]$Path
)

$ErrorActionPreference = "SilentlyContinue"
$buildDir = Join-Path $PSScriptRoot "..\build"
if ($IsWindows -or $env:OS -eq "Windows_NT") {
    $qsPath = Join-Path $buildDir "qs.exe"
} else {
    $qsPath = Join-Path $buildDir "qs"
}
if (!(Test-Path $qsPath)) { Write-Host "ERROR: $qsPath not found. Build first."; exit 1 }

$timeoutMs = 20000
$results = @()
$totalStart = Get-Date

# Load expected-failure (negative) test names.
# A listed test is expected to exit non-zero (XFAIL). If it exits zero it is
# an XPASS and is treated as a failure.
$expectedFailNames = @{}
$expectedFile = Join-Path $PSScriptRoot "expected_failures.txt"
if (Test-Path $expectedFile) {
    Get-Content $expectedFile | ForEach-Object {
        $n = $_.Trim()
        if ($n -and -not $n.StartsWith("#")) { $expectedFailNames[$n] = $true }
    }
}

# Mask nondeterministic wall-clock values so outputs from independent runs can
# be compared. Timestamps and elapsed/uptime measurements are the only values
# expected to legitimately differ between the qs, vm, and qvm paths.
function Normalize-Output {
    param([string]$Text)
    if ($null -eq $Text) { return "" }
    $t = $Text
    # ISO date/times: 2026-10-13T12:34:56.789Z or 2026-10-13 12:34:56
    $t = [regex]::Replace($t, '\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2}:\d{2}(\.\d+)?Z?', '<TIME>')
    # Slash dates: 2026/10/13 or 2026/10/13 12:34
    $t = [regex]::Replace($t, '\d{4}/\d{1,2}/\d{1,2}([ T]\d{1,2}:\d{2}(:\d{2})?)?', '<TIME>')
    # Bracketed clocks: [12:34:56]
    $t = [regex]::Replace($t, '\[\d{2}:\d{2}:\d{2}\]', '[<TIME>]')
    # "System uptime: 16759078 ms" and similar measurement labels
    $t = [regex]::Replace($t, '(?i)(uptime[^\r\n\d]*)\d+', '${1}<N>')
    # Volatile time fields: the value after a time label is wall-clock dependent
    $t = [regex]::Replace($t, '(?i)\b(timestamp|now|today|custom|tomorrow|year|month|day|hour|min|minute|sec|second|time)\s*:\s*[^\r\n]+', '$1: <TIME>')
    # Bare 9+ digit integers (epoch seconds / large millisecond counters)
    $t = [regex]::Replace($t, '\b\d{9,}\b', '<N>')
    return $t
}

function Run-One {
    param([string]$Exe, [string]$Arguments, [int]$Timeout)
    $result = @{ Exit = -1; Stdout = ""; Stderr = ""; Time = 0; Status = "TIMEOUT" }
    try {
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        $pinfo = New-Object System.Diagnostics.ProcessStartInfo
        $pinfo.FileName = $Exe
        $pinfo.Arguments = $Arguments
        $pinfo.RedirectStandardOutput = $true
        $pinfo.RedirectStandardError = $true
        $pinfo.UseShellExecute = $false
        $pinfo.CreateNoWindow = $true
        $proc = [System.Diagnostics.Process]::Start($pinfo)
        # Read output asynchronously so a full pipe buffer or a hung process
        # can never block the timeout.
        $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
        $stderrTask = $proc.StandardError.ReadToEndAsync()
        $exited = $proc.WaitForExit($Timeout)
        if ($exited -and $proc.HasExited) {
            $stdout = $stdoutTask.Result
            $stderr = $stderrTask.Result
            $sw.Stop()
            $result.Exit = $proc.ExitCode
            $result.Time = $sw.Elapsed.TotalSeconds
            $result.Stdout = $stdout.Trim()
            $result.Stderr = $stderr.Trim()
            $result.Status = if ($proc.ExitCode -eq 0) { "PASS" } else { "FAIL" }
        } else {
            # Kill the whole process tree so spawned children cannot survive.
            & taskkill /PID $proc.Id /T /F 2>&1 | Out-Null
            $proc.WaitForExit(2000) | Out-Null
            $result.Status = "TIMEOUT"
        }
    } catch {
        $result.Status = "CRASH"
    }
    return $result
}

# Determine which paths to run
$runQS = -not $VMOnly -and -not $QVMOnly
$runVM = -not $InterpreterOnly -and -not $QVMOnly
$runQVM = -not $InterpreterOnly -and -not $VMOnly

# Discover test files
$testFiles = @()
if ($Path) {
    $target = Get-Item $Path -ErrorAction SilentlyContinue
    if ($target -is [System.IO.DirectoryInfo]) {
        Get-ChildItem $target.FullName -Filter "*.qs" -Recurse | Where-Object {
            $rel = $_.FullName.Substring($target.FullName.Length + 1)
            $rel -notlike "build\*" -and $rel -notlike "out\*" -and $rel -notlike "bin\*" -and
            $_.Name -notlike "*.qvm" -and $_.Name -notlike "run_regression*"
        } | ForEach-Object { $testFiles += @{ Path = $_.FullName; Name = $_.Name; Dir = $_.DirectoryName } }
    } elseif ($target -is [System.IO.FileInfo]) {
        $testFiles += @{ Path = $target.FullName; Name = $target.Name; Dir = $target.DirectoryName }
    }
} else {
    Get-ChildItem tests/*.qs -ErrorAction SilentlyContinue | Where-Object { $_.Name -notlike "*.qvm" -and $_.Name -notlike "run_regression*" } | ForEach-Object { $testFiles += @{ Path = $_.FullName; Name = $_.Name; Dir = "tests" } }
    Get-ChildItem examples/*.qs -ErrorAction SilentlyContinue | Where-Object { $_.Name -notlike "*.qvm" } | ForEach-Object { $testFiles += @{ Path = $_.FullName; Name = $_.Name; Dir = "examples" } }
    # Recursive discovery from tests subdirectories
    Get-ChildItem tests -Recurse -Filter "*.qs" -ErrorAction SilentlyContinue | Where-Object { $_.DirectoryName -ne (Resolve-Path tests).Path -and $_.Name -notlike "*.qvm" } | ForEach-Object { $testFiles += @{ Path = $_.FullName; Name = $_.Name; Dir = $_.DirectoryName } }
}

# Sort alphabetically
$testFiles = @($testFiles | Sort-Object { $_.Name })

Write-Host ""
Write-Host "======================================="
Write-Host " QuantoScript Regression Suite"
Write-Host "======================================="
Write-Host ""
$paths = @()
if ($runQS) { $paths += "qs" }
if ($runVM) { $paths += "vm" }
if ($runQVM) { $paths += "qvm" }
Write-Host "Paths:   $($paths -join ', ')"
Write-Host "Tests:   $($testFiles.Count)"
Write-Host "Running..."
Write-Host ""

$idx = 0
foreach ($tf in $testFiles) {
    $idx++
    $pct = [math]::Round(($idx / $testFiles.Count) * 100)
    Write-Host -NoNewline "`r[$idx/$($testFiles.Count)] $($tf.Name) ($pct%)..."

    $r = @{
        Name     = $tf.Name
        Dir      = $tf.Dir
        QS       = $null
        VM       = $null
        QVM      = $null
        Mismatch = $false
    }

    if ($runQS) {
        $r.QS = Run-One -Exe $qsPath -Arguments "`"$($tf.Path)`"" -Timeout $timeoutMs
        $r.QS.Stdout = Normalize-Output $r.QS.Stdout
    }
    if ($runVM) {
        $r.VM = Run-One -Exe $qsPath -Arguments "vm `"$($tf.Path)`"" -Timeout $timeoutMs
        $r.VM.Stdout = Normalize-Output $r.VM.Stdout
    }
    if ($runQVM) {
        $qvmFile = [System.IO.Path]::ChangeExtension($tf.Path, ".qvm")
        $buildResult = Run-One -Exe $qsPath -Arguments "build `"$($tf.Path)`" -o `"$qvmFile`"" -Timeout $timeoutMs
        if ($buildResult.Exit -eq 0) {
            $r.QVM = Run-One -Exe $qsPath -Arguments "run `"$qvmFile`"" -Timeout $timeoutMs
            $r.QVM.Stdout = Normalize-Output $r.QVM.Stdout
        } else {
            $r.QVM = @{ Exit = $buildResult.Exit; Stdout = ""; Stderr = "Build failed"; Time = 0; Status = "FAIL" }
        }
        Remove-Item $qvmFile -ErrorAction SilentlyContinue
    }

    # Apply expected-failure expectation for this test.
    $expectFail = $expectedFailNames.ContainsKey($tf.Name)
    $r.ExpectFail = $expectFail
    if ($expectFail) {
        foreach ($p in @($r.QS, $r.VM, $r.QVM)) {
            if (-not $p) { continue }
            if ($p.Status -eq "FAIL") { $p.Status = "XFAIL" }
            elseif ($p.Status -eq "PASS") { $p.Status = "XPASS" }
            # TIMEOUT/CRASH stay as-is (always failures)
        }
    }

    # Compare outputs across active paths
    if ($expectFail) {
        # For negative tests, flag inconsistency only when some path
        # unexpectedly succeeded while others failed.
        $statuses = @()
        if ($r.QS) { $statuses += $r.QS.Status }
        if ($r.VM) { $statuses += $r.VM.Status }
        if ($r.QVM) { $statuses += $r.QVM.Status }
        if (($statuses | Where-Object { $_ -ne "XFAIL" }).Count -gt 0) { $r.Mismatch = $true }
    } else {
        $outputs = @()
        if ($r.QS) { $outputs += $r.QS.Stdout }
        if ($r.VM) { $outputs += $r.VM.Stdout }
        if ($r.QVM) { $outputs += $r.QVM.Stdout }
        if ($outputs.Count -gt 1 -and ($outputs | Select-Object -Unique).Count -gt 1) {
            $r.Mismatch = $true
        }
    }

    $results += $r

    if ($StopOnFailure) {
        $failed = $false
        if ($r.QS -and $r.QS.Status -ne "PASS" -and $r.QS.Status -ne "XFAIL") { $failed = $true }
        if ($r.VM -and $r.VM.Status -ne "PASS" -and $r.VM.Status -ne "XFAIL") { $failed = $true }
        if ($r.QVM -and $r.QVM.Status -ne "PASS" -and $r.QVM.Status -ne "XFAIL") { $failed = $true }
        if ($r.Mismatch) { $failed = $true }
        if ($failed) {
            Write-Host ""
            Write-Host "STOPPED ON FAILURE: $($tf.Name)"
            break
        }
    }
}

$totalEnd = Get-Date
$totalRuntime = ($totalEnd - $totalStart).TotalSeconds

$qsPass = @($results | Where-Object { $_.QS -and ($_.QS.Status -eq "PASS" -or $_.QS.Status -eq "XFAIL") }).Count
$qsFail = @($results | Where-Object { $_.QS -and $_.QS.Status -ne "PASS" -and $_.QS.Status -ne "XFAIL" }).Count
$vmPass = @($results | Where-Object { $_.VM -and ($_.VM.Status -eq "PASS" -or $_.VM.Status -eq "XFAIL") }).Count
$vmFail = @($results | Where-Object { $_.VM -and $_.VM.Status -ne "PASS" -and $_.VM.Status -ne "XFAIL" }).Count
$qvmPass = @($results | Where-Object { $_.QVM -and ($_.QVM.Status -eq "PASS" -or $_.QVM.Status -eq "XFAIL") }).Count
$qvmFail = @($results | Where-Object { $_.QVM -and $_.QVM.Status -ne "PASS" -and $_.QVM.Status -ne "XFAIL" }).Count
$mismatches = @($results | Where-Object { $_.Mismatch }).Count

$crashes = 0
$timeouts = 0
foreach ($r in $results) {
    if ($r.QS -and $r.QS.Status -eq "CRASH") { $crashes++ }
    if ($r.VM -and $r.VM.Status -eq "CRASH") { $crashes++ }
    if ($r.QVM -and $r.QVM.Status -eq "CRASH") { $crashes++ }
    if ($r.QS -and $r.QS.Status -eq "TIMEOUT") { $timeouts++ }
    if ($r.VM -and $r.VM.Status -eq "TIMEOUT") { $timeouts++ }
    if ($r.QVM -and $r.QVM.Status -eq "TIMEOUT") { $timeouts++ }
}

$qsTotal = $qsPass + $qsFail
$vmTotal = $vmPass + $vmFail
$qvmTotal = $qvmPass + $qvmFail

Write-Host ""
Write-Host "======================================="
Write-Host " QuantoScript Regression Suite"
Write-Host "======================================="
Write-Host ""
if ($runQS) {
    $qsPct = if ($qsTotal -gt 0) { [math]::Round(($qsPass / $qsTotal) * 100) } else { 0 }
    Write-Host "Interpreter (qs):  $qsPass/$qsTotal ($qsPct%)"
}
if ($runVM) {
    $vmPct = if ($vmTotal -gt 0) { [math]::Round(($vmPass / $vmTotal) * 100) } else { 0 }
    Write-Host "VM (qs vm):        $vmPass/$vmTotal ($vmPct%)"
}
if ($runQVM) {
    $qvmPct = if ($qvmTotal -gt 0) { [math]::Round(($qvmPass / $qvmTotal) * 100) } else { 0 }
    Write-Host "QVM (build+run):   $qvmPass/$qvmTotal ($qvmPct%)"
}
Write-Host ""
Write-Host "Mismatches: $mismatches"
Write-Host "Crashes:    $crashes"
Write-Host "Timeouts:   $timeouts"
Write-Host ""
$totalExecs = $qsTotal + $vmTotal + $qvmTotal
Write-Host "Tests:      $($results.Count)"
Write-Host "Executions: $totalExecs"
Write-Host "Runtime:    $([math]::Round($totalRuntime, 1))s"
Write-Host "======================================="

# Show failed/mismatched tests
$hasIssues = ($qsFail -gt 0 -or $vmFail -gt 0 -or $qvmFail -gt 0 -or $mismatches -gt 0)
if ($Verbose -or $hasIssues) {
    Write-Host ""
    Write-Host "--- Failed/Mismatched Tests ---"
    foreach ($r in $results) {
        $show = $false
        if ($r.QS -and $r.QS.Status -ne "PASS" -and $r.QS.Status -ne "XFAIL") { $show = $true }
        if ($r.VM -and $r.VM.Status -ne "PASS" -and $r.VM.Status -ne "XFAIL") { $show = $true }
        if ($r.QVM -and $r.QVM.Status -ne "PASS" -and $r.QVM.Status -ne "XFAIL") { $show = $true }
        if ($r.Mismatch) { $show = $true }
        if ($show) {
            Write-Host ""
            Write-Host "  $($r.Dir)/$($r.Name)"
            if ($r.QS) {
                $qsDetail = if ($r.QS.Status -eq "CRASH") { "CRASH" } elseif ($r.QS.Status -eq "TIMEOUT") { "TIMEOUT" } else { "exit=$($r.QS.Exit)" }
                Write-Host "    qs:  $($r.QS.Status) ($qsDetail) [$([math]::Round($r.QS.Time, 3))s]"
            }
            if ($r.VM) {
                $vmDetail = if ($r.VM.Status -eq "CRASH") { "CRASH" } elseif ($r.VM.Status -eq "TIMEOUT") { "TIMEOUT" } else { "exit=$($r.VM.Exit)" }
                Write-Host "    vm:  $($r.VM.Status) ($vmDetail) [$([math]::Round($r.VM.Time, 3))s]"
            }
            if ($r.QVM) {
                $qvmDetail = if ($r.QVM.Status -eq "CRASH") { "CRASH" } elseif ($r.QVM.Status -eq "TIMEOUT") { "TIMEOUT" } else { "exit=$($r.QVM.Exit)" }
                Write-Host "    qvm: $($r.QVM.Status) ($qvmDetail) [$([math]::Round($r.QVM.Time, 3))s]"
            }
            if ($r.Mismatch) {
                Write-Host "    *** OUTPUT MISMATCH ***"
                # Show unified diff of first divergence
                $qsLines = if ($r.QS) { $r.QS.Stdout -split "`n" } else { @() }
                $vmLines = if ($r.VM) { $r.VM.Stdout -split "`n" } else { @() }
                $qvmLines = if ($r.QVM) { $r.QVM.Stdout -split "`n" } else { @() }
                $maxLines = [math]::Max([math]::Max($qsLines.Count, $vmLines.Count), $qvmLines.Count)
                $shown = 0
                for ($li = 0; $li -lt $maxLines -and $shown -lt 5; $li++) {
                    $ql = if ($li -lt $qsLines.Count) { $qsLines[$li] } else { "<missing>" }
                    $vl = if ($li -lt $vmLines.Count) { $vmLines[$li] } else { "<missing>" }
                    $ql2 = if ($li -lt $qvmLines.Count) { $qvmLines[$li] } else { "<missing>" }
                    if ($ql -ne $vl -or $vl -ne $ql2) {
                        Write-Host "      L$($li+1): qs=[$ql] vm=[$vl] qvm=[$ql2]"
                        $shown++
                    }
                }
            }
        }
    }
}

if ($hasIssues) { exit 1 }
exit 0
