# package_release.ps1 — Generate release archives for QuantoScript.
#
# Usage (from project root):
#   pwsh -File scripts/package_release.ps1 -Version 1.0.0
#
# Produces (inside dist/):
#   quantoscript-1.0.0-x86_64-pc-windows-gnu.zip        + .sha256
#   quantoscript-1.0.0-x86_64-unknown-linux-gnu.tar.gz   + .sha256
#   quantoscript-1.0.0-x86_64-apple-darwin.tar.gz        + .sha256
#
# Each archive contains exactly one top-level directory:
#   quantoscript-<ver>-<target>/{bin/qs[.exe], stdlib/, examples/, LICENSE, README.md}

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Version
)

$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot -Parent   # project root
$Dist = Join-Path $Root "dist"
if (Test-Path $Dist) { Remove-Item $Dist -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Dist | Out-Null

$targets = @(
    @{ Name = "x86_64-pc-windows-gnu";    Bin = "qs.exe"; Ext = "zip"  }
    @{ Name = "x86_64-unknown-linux-gnu";  Bin = "qs";     Ext = "tar.gz" }
    @{ Name = "x86_64-apple-darwin";       Bin = "qs";     Ext = "tar.gz" }
)

function Make-Checksum {
    param([string]$FilePath)
    $hash = (Get-FileHash $FilePath -Algorithm SHA256).Hash.ToLower()
    $line = "$hash  $(Split-Path $FilePath -Leaf)"
    Set-Content -Path "$FilePath.sha256" -Value $line -NoNewline
    Write-Host "  checksum: $(Split-Path $FilePath -Leaf).sha256"
}

foreach ($t in $targets) {
    $pkgName = "quantoscript-$Version-$($t.Name)"
    $staging = Join-Path $Dist "staging_$($t.Name)"
    if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }

    $pkgDir = Join-Path $staging $pkgName
    $binDir = Join-Path $pkgDir "bin"
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null

    # Copy binary
    $srcBin = Join-Path $Root "build\$($t.Bin)"
    if (-not (Test-Path $srcBin)) {
        Write-Warning "Binary not found: $srcBin, skipping for $($t.Name)"
    } else {
        Copy-Item $srcBin (Join-Path $binDir $t.Bin) -Force
    }

    # Copy stdlib, examples, LICENSE, README.md
    Copy-Item (Join-Path $Root "stdlib")   (Join-Path $pkgDir "stdlib")   -Recurse -Force
    Copy-Item (Join-Path $Root "examples") (Join-Path $pkgDir "examples") -Recurse -Force
    Copy-Item (Join-Path $Root "LICENSE")  (Join-Path $pkgDir "LICENSE")  -Force
    Copy-Item (Join-Path $Root "README.md")(Join-Path $pkgDir "README.md") -Force

    # Create archive
    if ($t.Ext -eq "zip") {
        $archive = Join-Path $Dist "$pkgName.zip"
        Compress-Archive -Path $pkgDir -DestinationPath $archive -Force
        Write-Host "Created: $archive"
        Make-Checksum $archive
    } else {
        $archive = Join-Path $Dist "$pkgName.tar.gz"
        # tar is available on Windows 10+ and all Linux/macOS
        tar -czf $archive -C $staging $pkgName
        if ($LASTEXITCODE -ne 0) { throw "tar failed" }
        Write-Host "Created: $archive"
        Make-Checksum $archive
    }

    # Cleanup staging
    Remove-Item $staging -Recurse -Force
}

Write-Host ""
Write-Host "All archives written to $Dist"
Get-ChildItem $Dist | ForEach-Object { Write-Host "  $($_.Name)" }
