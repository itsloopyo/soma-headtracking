#!/usr/bin/env pwsh
#Requires -Version 5.1
<#
.SYNOPSIS
    Package SOMA Head Tracking into its release ZIP.
.DESCRIPTION
    Produces, in release/:
      SomaHeadTracking-v<version>-installer.zip  install.cmd + plugins/ + vendor/ + shared/ + docs

    One ZIP, not two. There is no -nexus.zip stage because there is no Nexus
    route for SOMA: the payload has to sit next to Soma.exe, Vortex has no SOMA
    extension to deploy it with, and a mod manager only deploys into one fixed
    subfolder. The manual route in README.md is "extract the installer ZIP and
    copy two files", so a second archive would be a second thing to keep in step
    with it for no one's benefit. Do not add one back.

    Offline and side-effect free: consumes whatever is committed under vendor/
    and whatever cameraunlock-core commit is checked out. Refreshing either is
    `pixi run update-deps` / `pixi run sync`, both deliberate acts with a commit
    attached.

    Non-interactive: exits 0 on success, non-zero with a one-line diagnostic on
    any failure.
.NOTES
    Run via: pixi run package
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

Add-Type -AssemblyName System.IO.Compression.FileSystem

$projectRoot = Split-Path -Parent $PSScriptRoot
$scriptsDir = Join-Path $projectRoot 'scripts'
$releaseDir = Join-Path $projectRoot 'release'

Import-Module (Join-Path $projectRoot 'cameraunlock-core\powershell\ReleaseWorkflow.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'Version.psm1') -Force

$version = Get-ModVersion -ProjectRoot $projectRoot

Write-Host '=== SOMA Head Tracking - Package Release ===' -ForegroundColor Magenta
Write-Host "Version: $version" -ForegroundColor Cyan
Write-Host ''

$asiPath = Join-Path $projectRoot 'bin\Release\SomaHeadTracking.asi'
if (-not (Test-Path $asiPath)) { throw "Build output not found: $asiPath (run 'pixi run build-release')" }

$vendorDir = Join-Path $projectRoot 'vendor\ultimate-asi-loader'
$vendorLoaderDll = Join-Path $vendorDir 'dinput8.dll'
if (-not (Test-Path $vendorLoaderDll)) {
    throw "Vendored ASI loader missing: $vendorLoaderDll (run 'pixi run update-deps')"
}

# The vendored loader is the one binary in the release ZIPs this repo did not
# compile, and it ships into the game's exe directory where Windows loads it
# ahead of the real version.dll (this comment's "release ZIPs" was two, once). update-deps.ps1 records its SHA-256 in the
# sidecar README precisely so the committed artifact is verifiable; until this
# check existed nothing ever read that line back, so a DLL swapped in the
# working tree - or a bad partial write during the unwrap - was packaged and
# published without a word. Offline: it compares what is on disk against what
# is committed beside it, and refuses to build a ZIP when they disagree.
$vendorReadme = Join-Path $vendorDir 'README.md'
if (-not (Test-Path $vendorReadme)) {
    throw "Vendored ASI loader metadata missing: $vendorReadme (run 'pixi run update-deps')"
}
$recordedHash = [regex]::Match(
    (Get-Content -LiteralPath $vendorReadme -Raw),
    '(?m)^-\s*dinput8\.dll SHA-256:\s*`?([0-9a-fA-F]{64})`?').Groups[1].Value
if (-not $recordedHash) {
    throw "vendor/ultimate-asi-loader/README.md records no 'dinput8.dll SHA-256' line, so the shipped loader cannot be verified. Run 'pixi run update-deps' and commit the result."
}
$actualHash = (Get-FileHash -LiteralPath $vendorLoaderDll -Algorithm SHA256).Hash
if ($actualHash -ne $recordedHash.ToUpperInvariant()) {
    throw "vendor/ultimate-asi-loader/dinput8.dll does not match the SHA-256 recorded beside it (recorded $($recordedHash.ToLowerInvariant()), found $($actualHash.ToLowerInvariant())). Refusing to package a loader nothing in the repo vouches for."
}
Write-Host "  vendored loader verified against its recorded SHA-256" -ForegroundColor Green

# install.cmd carries the loader version a second time, in ASI_LOADER_VERSION,
# and writes it into the state file the launcher reads to tell which loader
# build it is looking at. update-deps.ps1 rewrites this README and nothing else,
# so a bump that was not hand-mirrored into install.cmd ships a state file
# naming the previous release, with nothing anywhere failing. Same read, one
# more comparison.
$recordedTag = [regex]::Match(
    (Get-Content -LiteralPath $vendorReadme -Raw), '(?m)^-\s*Tag:\s*`?v(\d+\.\d+\.\d+)`?').Groups[1].Value
if (-not $recordedTag) {
    throw "vendor/ultimate-asi-loader/README.md records no 'Tag:' line, so the shipped loader's version cannot be verified. Run 'pixi run update-deps' and commit the result."
}
$installCmdText = Get-Content -LiteralPath (Join-Path $scriptsDir 'install.cmd') -Raw
$declaredLoader = [regex]::Match($installCmdText, 'set "ASI_LOADER_VERSION=([^"]*)"').Groups[1].Value
if ($declaredLoader -ne $recordedTag) {
    throw "scripts/install.cmd sets ASI_LOADER_VERSION=$declaredLoader but vendor/ultimate-asi-loader/README.md records v$recordedTag. Update install.cmd to match the vendored loader."
}
Write-Host "  install.cmd ASI_LOADER_VERSION matches the vendored loader ($recordedTag)" -ForegroundColor Green

foreach ($s in @('install.cmd', 'uninstall.cmd')) {
    if (-not (Test-Path (Join-Path $scriptsDir $s))) { throw "Required script not found: scripts/$s" }
}

# src/version.h is canonical and release.ps1 writes every derived copy in one
# pass; this is what makes that true rather than intended.
Assert-ModVersionsInSync -ProjectRoot $projectRoot -Version $version

# MinHook and the Hacker Disassembler Engine (both BSD-2-Clause) are compiled
# into the .asi out of cameraunlock-core's vendored copy, so their notice has to
# travel with every ZIP that carries the binary.
$minhookLicence = Join-Path $projectRoot 'cameraunlock-core\vendor\minhook\LICENSE.txt'
if (-not (Test-Path $minhookLicence)) {
    throw "MinHook LICENSE.txt not found at $minhookLicence. It is compiled into the .asi and its notice must ship with it."
}

# The vendored dinput8.dll is a static binary and is not one component. Ultimate
# ASI Loader's x64 premake target compiles miniz (MIT) and ThirteenAG/injector
# (zlib) - which carries its own MinHook - into it, so shipping that DLL is a
# binary distribution of all three and each notice has to travel with it. Their
# texts sit beside the DLL they are inside rather than in a repo-level folder,
# so a loader bump and its licence set move together. MemoryModule, d3d8to9 and
# minidx9 are 32-bit only and are absent from this binary; re-check that in
# premake5.lua on any bump, because the set is a property of the release, not of
# the upstream repository.
$bundledLicences = [ordered]@{
    'miniz-LICENSE.txt'    = 'miniz'
    'injector-LICENSE.txt' = 'injector'
}
foreach ($lf in $bundledLicences.Keys) {
    if (-not (Test-Path (Join-Path $vendorDir $lf))) {
        throw "vendor/ultimate-asi-loader/$lf not found. $($bundledLicences[$lf]) is compiled into the vendored dinput8.dll and its notice must ship with it."
    }
}

if (-not (Test-Path $releaseDir)) { New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null }

function New-StagingDir {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (Test-Path $Path) { Remove-Item -Recurse -Force $Path }
    New-Item -ItemType Directory -Path $Path -Force | Out-Null
    return $Path
}

function Compress-Staging {
    param(
        [Parameter(Mandatory = $true)][string]$StagingDir,
        [Parameter(Mandatory = $true)][string]$ZipPath
    )
    if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
    # Not Compress-Archive: on Windows PowerShell 5.1 it writes the OS separator
    # into the central directory, so entries come out as `plugins\Soma...asi`.
    # ZIP requires `/`, and an extractor that treats `\` as an ordinary filename
    # character - which several non-Windows ones do - unpacks the installer as a
    # flat pile of files whose names contain backslashes, at which point
    # install.cmd cannot find its own plugins folder.
    [System.IO.Compression.ZipFile]::CreateFromDirectory(
        $StagingDir, $ZipPath, [System.IO.Compression.CompressionLevel]::Optimal, $false)
    Remove-Item -Recurse -Force $StagingDir
    Write-Host ("  $ZipPath ({0:N1} KB)" -f ((Get-Item $ZipPath).Length / 1KB)) -ForegroundColor Green
}

# --- Installer ZIP (GitHub Releases / the launcher) ---
Write-Host '--- Installer ZIP ---' -ForegroundColor Yellow
$ghStaging = New-StagingDir (Join-Path $releaseDir 'staging-installer')

foreach ($s in @('install.cmd', 'uninstall.cmd')) {
    Copy-Item (Join-Path $scriptsDir $s) -Destination $ghStaging -Force
    Write-Host "  $s" -ForegroundColor Green
}

$pluginsDir = Join-Path $ghStaging 'plugins'
New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null
Copy-Item $asiPath -Destination $pluginsDir -Force
Write-Host '  plugins/SomaHeadTracking.asi' -ForegroundColor Green

$ghVendorDir = Join-Path $ghStaging 'vendor\ultimate-asi-loader'
New-Item -ItemType Directory -Path $ghVendorDir -Force | Out-Null
foreach ($vf in @('dinput8.dll', 'LICENSE', 'README.md') + @($bundledLicences.Keys)) {
    $src = Join-Path $vendorDir $vf
    if (-not (Test-Path $src)) { throw "Vendored ASI loader file missing: vendor/ultimate-asi-loader/$vf" }
    Copy-Item $src -Destination $ghVendorDir -Force
    Write-Host "  vendor/ultimate-asi-loader/$vf" -ForegroundColor Green
}

Copy-LicenceNotices -StagingDir $ghStaging -ProjectRoot $projectRoot -Additional @('README.md', 'CHANGELOG.md')
# Also under licenses/, not only under vendor/, so "what is in this thing and
# under what terms" is answered from one folder rather than two places.
Copy-Item (Join-Path $vendorDir 'LICENSE') `
          -Destination (Join-Path $ghStaging 'licenses\ultimate-asi-loader-LICENSE.txt') -Force
Write-Host '  licenses/ultimate-asi-loader-LICENSE.txt' -ForegroundColor Green
Copy-Item $minhookLicence -Destination (Join-Path $ghStaging 'licenses\minhook-LICENSE.txt') -Force
Write-Host '  licenses/minhook-LICENSE.txt' -ForegroundColor Green
foreach ($lf in $bundledLicences.Keys) {
    Copy-Item (Join-Path $vendorDir $lf) -Destination (Join-Path $ghStaging "licenses\$lf") -Force
    Write-Host "  licenses/$lf" -ForegroundColor Green
}

# shared/ carries install-body-asi.cmd and find-game.ps1, which scripts/install.cmd
# is a thin wrapper over. Without it the installer cannot run.
Copy-SharedBundle -StagingDir $ghStaging -CoreRoot (Join-Path $projectRoot 'cameraunlock-core')

Compress-Staging -StagingDir $ghStaging -ZipPath (Join-Path $releaseDir "SomaHeadTracking-v$version-installer.zip")

Write-Host ''
Write-Host '=== Package Complete ===' -ForegroundColor Magenta
