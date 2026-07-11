<#
.SYNOPSIS
    QuantoScript installer for Windows.

.DESCRIPTION
    Downloads a prebuilt, self-contained QuantoScript release and installs it
    per-user (no administrator rights required). Adds the binary to your PATH
    and sets QUANTO_HOME.

    Install:
        irm https://raw.githubusercontent.com/PySudo/QuantoScript/main/install.ps1 | iex

    Or with options:
        & ([scriptblock]::Create((irm https://raw.githubusercontent.com/PySudo/QuantoScript/main/install.ps1))) -Version 1.0.0

.PARAMETER Version
    Version to install (e.g. 1.0.0). Defaults to the latest release.

.PARAMETER InstallDir
    Install prefix. Defaults to $env:USERPROFILE\.quanto

.PARAMETER Archive
    Install from a local .zip instead of downloading (offline install).

.PARAMETER Uninstall
    Remove a previous installation.

.PARAMETER NoModifyPath
    Do not modify the user PATH / environment variables.
#>

[CmdletBinding()]
param(
    [string]$Version = $env:QS_VERSION,
    [string]$InstallDir = $(if ($env:QUANTO_INSTALL) { $env:QUANTO_INSTALL } else { Join-Path $env:USERPROFILE ".quanto" }),
    [string]$Repo = $(if ($env:QS_REPO) { $env:QS_REPO } else { "PySudo/QuantoScript" }),
    [string]$Archive = $env:QS_ARCHIVE,
    [switch]$Uninstall,
    [switch]$NoModifyPath
)

$ErrorActionPreference = "Stop"
$BinName    = "qs.exe"
$DataSubdir = "share\quantoscript"

function Say  ($m) { Write-Host $m -ForegroundColor Cyan }
function Ok   ($m) { Write-Host $m -ForegroundColor Green }
function Fail ($m) { Write-Host "error: $m" -ForegroundColor Red; exit 1 }

# ── Uninstall ──────────────────────────────────────────────────────
function Invoke-Uninstall {
    Say "Removing QuantoScript from $InstallDir"
    if (Test-Path $InstallDir) {
        Remove-Item -Recurse -Force $InstallDir -ErrorAction SilentlyContinue
    }
    if (-not $NoModifyPath) {
        [Environment]::SetEnvironmentVariable("QUANTO_HOME", $null, "User")
        $binPath = Join-Path $InstallDir "bin"
        $path = [Environment]::GetEnvironmentVariable("PATH", "User")
        if ($path) {
            $new = ($path -split ';' | Where-Object { $_ -and $_ -ne $binPath }) -join ';'
            [Environment]::SetEnvironmentVariable("PATH", $new, "User")
        }
    }
    Ok "Uninstalled. Open a new terminal for the PATH change to take effect."
    exit 0
}

# ── Detect target ──────────────────────────────────────────────────
function Get-Target {
    $arch = $env:PROCESSOR_ARCHITECTURE
    switch ($arch) {
        "AMD64" { return "x86_64-pc-windows-gnu" }
        "ARM64" {
            # No native arm64 build yet; the x64 build runs under emulation.
            Say "  note: no native ARM64 build; using the x64 binary (runs under emulation)"
            return "x86_64-pc-windows-gnu"
        }
        default { return "x86_64-pc-windows-gnu" }
    }
}

function Get-LatestVersion {
    $api = "https://api.github.com/repos/$Repo/releases/latest"
    try {
        $rel = Invoke-RestMethod -Uri $api -Headers @{ "User-Agent" = "quantoscript-installer" }
        return ($rel.tag_name -replace '^v', '')
    } catch {
        Fail "could not determine the latest release. Set -Version or check that $Repo has published releases."
    }
}

# ── Install ────────────────────────────────────────────────────────
function Invoke-Install {
    $target = Get-Target
    Say "Installing QuantoScript"
    Say "  target:  $target"
    Say "  prefix:  $InstallDir"

    $tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("quanto-" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $tmp | Out-Null
    try {
        $zip = Join-Path $tmp "pkg.zip"

        if ($Archive) {
            if (-not (Test-Path $Archive)) { Fail "Archive not found: $Archive" }
            Say "  source:  $Archive (local)"
            Copy-Item $Archive $zip -Force
        } else {
            if (-not $Version) {
                Say "  resolving latest release..."
                $Version = Get-LatestVersion
            }
            Say "  version: $Version"
            $asset = "quantoscript-$Version-$target.zip"
            $base  = "https://github.com/$Repo/releases/download/v$Version"
            Say "  downloading $asset ..."
            try {
                Invoke-WebRequest -Uri "$base/$asset" -OutFile $zip -UseBasicParsing
            } catch {
                Fail "no prebuilt binary for $target (v$Version). Build from source: https://github.com/$Repo#building-from-source"
            }
            # Verify checksum if available
            $shaFile = Join-Path $tmp "pkg.sha256"
            try {
                Invoke-WebRequest -Uri "$base/$asset.sha256" -OutFile $shaFile -UseBasicParsing
                $expected = (Get-Content $shaFile -Raw).Trim().Split()[0].ToLower()
                $actual   = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
                if ($expected -ne $actual) { Fail "checksum mismatch for $asset" }
                Ok "  checksum verified"
            } catch {
                Write-Host "  (no checksum available; skipping verification)" -ForegroundColor DarkYellow
            }
        }

        Say "  extracting..."
        $ex = Join-Path $tmp "extract"
        Expand-Archive -Path $zip -DestinationPath $ex -Force
        $pkgdir = Get-ChildItem -Path $ex -Directory | Where-Object { $_.Name -like "quantoscript-*" } | Select-Object -First 1
        if (-not $pkgdir) { $pkgdir = Get-Item $ex }
        $srcBin = Join-Path $pkgdir.FullName "bin\$BinName"
        if (-not (Test-Path $srcBin)) { Fail "binary not found in archive" }

        # Layout: InstallDir\bin\qs.exe  and  InstallDir\share\quantoscript\{stdlib,examples}
        $binDir  = Join-Path $InstallDir "bin"
        $dataDir = Join-Path $InstallDir $DataSubdir
        New-Item -ItemType Directory -Force -Path $binDir, $dataDir | Out-Null
        Copy-Item $srcBin (Join-Path $binDir $BinName) -Force
        foreach ($sub in "stdlib", "examples") {
            $s = Join-Path $pkgdir.FullName $sub
            $d = Join-Path $dataDir $sub
            if (Test-Path $d) { Remove-Item -Recurse -Force $d }
            if (Test-Path $s) { Copy-Item $s $d -Recurse -Force }
        }
        Ok "  installed $BinName to $binDir\$BinName"

        if (-not $NoModifyPath) {
            [Environment]::SetEnvironmentVariable("QUANTO_HOME", $dataDir, "User")
            $userPath = [Environment]::GetEnvironmentVariable("PATH", "User")
            if (-not $userPath) { $userPath = "" }
            if (($userPath -split ';') -notcontains $binDir) {
                $newPath = if ($userPath) { "$binDir;$userPath" } else { $binDir }
                [Environment]::SetEnvironmentVariable("PATH", $newPath, "User")
                Say "  added $binDir to your PATH"
            }
            # Make it available in the current session too.
            $env:QUANTO_HOME = $dataDir
            $env:PATH = "$binDir;$env:PATH"
        }

        # Verify
        $exe = Join-Path $binDir $BinName
        $out = & $exe version 2>$null
        if ($LASTEXITCODE -eq 0) {
            Write-Host ""
            Ok "$out installed successfully!"
        } else {
            Fail "installation completed but '$BinName version' failed to run"
        }

        Write-Host ""
        Say "Next steps:"
        Write-Host "  Open a new terminal, then run:  qs --help"
        Write-Host "                                  qs $dataDir\examples\full_tour.qs"
    }
    finally {
        Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
    }
}

# ── Entry point ────────────────────────────────────────────────────
if ($Uninstall) { Invoke-Uninstall } else { Invoke-Install }
