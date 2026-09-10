#!/usr/bin/env pwsh
#Requires -Version 5.1
# ============================================================================
# <mod>/scripts/update-deps.ps1
# ============================================================================
# Bumps the vendored mod-loader copy under vendor/ultimate-asi-loader/ to the
# latest upstream release within the pinned version range, and writes refreshed
# LICENSE + README.md sidecar metadata.
#
# Usage:    pixi run update-deps
# Frequency: manual. The vendored copy is the install-time source of truth, so
# the dev runs this when they want a fresh upstream bump, reviews the diff, and
# commits the updated vendor/ tree. No build task depends on this, and CI never
# refreshes - it consumes whatever is committed under vendor/.
# ============================================================================

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'

$projectRoot = Split-Path -Parent $PSScriptRoot

# The submodule, and only the submodule: a sibling checkout sits at whatever
# commit its own working tree is on, and vendoring from it would put a loader
# in the ZIP that nothing in this repo pins.
$modulePath = Join-Path $projectRoot 'cameraunlock-core/powershell/ModLoaderSetup.psm1'
if (-not (Test-Path $modulePath)) {
    throw "ModLoaderSetup.psm1 not found. Run 'pixi run sync' to update the cameraunlock-core submodule."
}
Import-Module $modulePath -Force

# --- CALL BLOCK ----------------------------------------------------------
# Soma.exe is 64-bit (HPL3) -> Ultimate ASI Loader x64. The x64 release asset
# (Ultimate-ASI-Loader_x64.zip) is a wrapper zip containing a single x64
# dinput8.dll. Update-VendoredLoader cannot unwrap it, so it lands as
# vendor/ultimate-asi-loader/dinput8.dll *as a zip*; we extract the real DLL
# below. install.cmd copies the bundled dinput8.dll to the exe directory as
# version.dll - Soma.exe imports no proxy directly, but SDL2.dll, which it
# loads out of the exe directory, imports VERSION.dll.
$vendorDir = Join-Path $projectRoot 'vendor/ultimate-asi-loader'
Update-VendoredLoader `
    -Name 'ultimate-asi-loader' `
    -OutputDir $vendorDir `
    -OutputFileName 'dinput8.dll' `
    -Owner 'ThirteenAG' -Repo 'Ultimate-ASI-Loader' `
    -VersionPrefix 'v9.' `
    -AssetPattern '^Ultimate-ASI-Loader_x64\.zip$' `
    -LicenseUrl 'https://raw.githubusercontent.com/ThirteenAG/Ultimate-ASI-Loader/master/license' | Out-Null

# Unwrap: the saved dinput8.dll is the wrapper zip. Replace it with the x64 DLL
# it contains.
#
# One named entry, never Expand-Archive over the whole thing. This archive is
# third-party input off a GitHub release, and Expand-Archive does not validate
# entry paths - an entry called '..\..\something' is written outside the
# destination, which here is inside the repo. Matching on the entry's leaf Name
# and extracting to a path we chose leaves nothing for a crafted entry name to
# steer.
$saved = Join-Path $vendorDir 'dinput8.dll'
$bytes = [System.IO.File]::ReadAllBytes($saved)
if ($bytes.Length -ge 2 -and $bytes[0] -eq 0x50 -and $bytes[1] -eq 0x4B) {
    $zipCopy = Join-Path $vendorDir '_loader.zip'
    Copy-Item $saved $zipCopy -Force
    try {
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $zip = [System.IO.Compression.ZipFile]::OpenRead($zipCopy)
        try {
            $entry = $zip.Entries | Where-Object { $_.Name -eq 'dinput8.dll' } | Select-Object -First 1
            if (-not $entry) { throw "x64 dinput8.dll not found inside Ultimate-ASI-Loader_x64.zip" }
            [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $saved, $true)
        } finally { $zip.Dispose() }
    } finally { Remove-Item $zipCopy -Force }
    Write-Host "Unwrapped x64 dinput8.dll from the release zip." -ForegroundColor Green
}

# The module's SHA-256 line covers the wrapper zip, which is not what we commit.
# Record the unwrapped DLL's hash too, so the committed artifact is verifiable.
$readme  = Join-Path $vendorDir 'README.md'
$dllHash = (Get-FileHash -Path $saved -Algorithm SHA256).Hash.ToLowerInvariant()
$lines   = Get-Content $readme
$anchor  = ($lines | Select-String -SimpleMatch '- SHA-256:' | Select-Object -First 1).LineNumber
if (-not $anchor) { throw "vendor README.md has no '- SHA-256:' line to anchor the DLL hash after." }
# $anchor is 1-based, so it is also the index of the first line AFTER the
# anchor. Built as an explicit empty tail when there is none: PowerShell ranges
# count DOWN when the start is past the end, so `$lines[$n..($n-1)]` on an
# anchor that is the last line yields $null plus the last line over again, and
# the rewritten README ends with a duplicated line and a blank.
$tail = if ($anchor -lt $lines.Count) { @($lines[$anchor..($lines.Count - 1)]) } else { @() }
$lines = @($lines[0..($anchor - 1)]) + "- dinput8.dll SHA-256: ``$dllHash``" + $tail
Set-Content -Path $readme -Value $lines -Encoding utf8

# --- END CALL BLOCK ------------------------------------------------------

Write-Host ""
Write-Host "Vendored dependencies refreshed. Review and commit the changes under vendor/." -ForegroundColor Green
Write-Host ""
# The set of components inside dinput8.dll is a property of the release being
# vendored, not of the upstream repository, and a bump can add one silently. At
# v9.7.4 the x64 target compiled MinHook, ThirteenAG/injector and miniz, and
# left MemoryModule (MPL-2.0), d3d8to9 and minidx9 to the 32-bit build. A bump
# that pulled MemoryModule into x64 would put an MPL component in the release
# ZIP with no source offer, which is the one licence in that set with an
# obligation the existing notices do not already discharge.
Write-Host "Before committing: re-read the Ultimate-ASI-Loader-x64 block in the new tag's" -ForegroundColor Yellow
Write-Host "premake5.lua. Every third-party source it compiles needs a section in" -ForegroundColor Yellow
Write-Host "THIRD-PARTY-NOTICES.md and a <name>-LICENSE.txt beside the DLL, or packaging fails." -ForegroundColor Yellow
