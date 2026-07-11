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

$timeoutMs = 10000
$results = @()
$totalStart = Get-Date

function Run-One {
    param([string]$Exe, [string]$Args, [int]$Timeout)
    $result = @{ Exit = -1; Stdout = ""; Stderr = ""; Time = 0; Status = "TIMEOUT" }
    try {
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        $pinfo = New-Object System.Diagnostics.ProcessStartInfo
        $pinfo.FileName = $Exe
        $pinfo.Arguments = $Args
        $pinfo.RedirectStandardOutput = $true
        $pinfo.RedirectStandardError = $true
        $pinfo.UseShellExecute = $false
        $pinfo.CreateNoWindow = $true
        $proc = [System.Diagnostics.Process]::Start($pinfo)
        $stdout = $proc.StandardOutput.ReadToEnd()
        $stderr = $proc.StandardError.ReadToEnd()
        $proc.WaitForExit($Timeout)
        if ($proc.HasExited) {
            $sw.Stop()
            $result.Exit = $proc.ExitCode
            $result.Time = $sw.Elapsed.TotalSeconds
            $result.Stdout = $stdout.Trim()
            $result.Stderr = $stderr.Trim()
            $result.Status = if ($proc.ExitCode -eq 0) { "PASS" } else { "FAIL" }
        } else {
            $proc.Kill()
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
$testFiles = $testFiles | Sort-Object { $_.Name }

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
        $r.QS = Run-One -Exe $qsPath -Args "`"$($tf.Path)`"" -Timeout $timeoutMs
    }
    if ($runVM) {
        $r.VM = Run-One -Exe $qsPath -Args "vm `"$($tf.Path)`"" -Timeout $timeoutMs
    }
    if ($runQVM) {
        $qvmFile = [System.IO.Path]::ChangeExtension($tf.Path, ".qvm")
        $buildResult = Run-One -Exe $qsPath -Args "build `"$($tf.Path)`" -o `"$qvmFile`"" -Timeout $timeoutMs
        if ($buildResult.Exit -eq 0) {
            $r.QVM = Run-One -Exe $qsPath -Args "run `"$qvmFile`"" -Timeout $timeoutMs
        } else {
            $r.QVM = @{ Exit = $buildResult.Exit; Stdout = ""; Stderr = "Build failed"; Time = 0; Status = "FAIL" }
        }
        Remove-Item $qvmFile -ErrorAction SilentlyContinue
    }

    # Compare outputs across active paths
    $outputs = @()
    if ($r.QS) { $outputs += $r.QS.Stdout }
    if ($r.VM) { $outputs += $r.VM.Stdout }
    if ($r.QVM) { $outputs += $r.QVM.Stdout }
    if ($outputs.Count -gt 1 -and ($outputs | Select-Object -Unique).Count -gt 1) {
        $r.Mismatch = $true
    }

    $results += $r

    if ($StopOnFailure) {
        $failed = $false
        if ($r.QS -and $r.QS.Status -ne "PASS") { $failed = $true }
        if ($r.VM -and $r.VM.Status -ne "PASS") { $failed = $true }
        if ($r.QVM -and $r.QVM.Status -ne "PASS") { $failed = $true }
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

$qsPass = ($results | Where-Object { $_.QS -and $_.QS.Status -eq "PASS" }).Count
$qsFail = ($results | Where-Object { $_.QS -and $_.QS.Status -ne "PASS" }).Count
$vmPass = ($results | Where-Object { $_.VM -and $_.VM.Status -eq "PASS" }).Count
$vmFail = ($results | Where-Object { $_.VM -and $_.VM.Status -ne "PASS" }).Count
$qvmPass = ($results | Where-Object { $_.QVM -and $_.QVM.Status -eq "PASS" }).Count
$qvmFail = ($results | Where-Object { $_.QVM -and $_.QVM.Status -ne "PASS" }).Count
$mismatches = ($results | Where-Object { $_.Mismatch }).Count

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
        if ($r.QS -and $r.QS.Status -ne "PASS") { $show = $true }
        if ($r.VM -and $r.VM.Status -ne "PASS") { $show = $true }
        if ($r.QVM -and $r.QVM.Status -ne "PASS") { $show = $true }
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
