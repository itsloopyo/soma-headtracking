#!/usr/bin/env pwsh
#Requires -Version 5.1
<#
.SYNOPSIS
    Dev deploy: drop the built .asi and the vendored ASI loader into the local
    SOMA install.
.DESCRIPTION
    Thin wrapper over cameraunlock-core's Invoke-DevDeployASILoader, which owns
    game resolution (env var -> registry -> Steam -> games.json), the
    "game is running" check, and the loader-present check. A caller-supplied
    path wins outright, matching install.cmd's "trust the argument" semantics.

    Non-interactive: every failure exits non-zero with a one-line diagnostic.
.NOTES
    Run via: pixi run install
#>
param(
    [Parameter(Position = 0)]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    # Explicit game directory. Overrides detection, same as install.cmd's %~1.
    [Parameter(Position = 1)]
    [string]$GamePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot

try {
    Import-Module (Join-Path $projectRoot 'cameraunlock-core\powershell\GamePathDetection.psm1')
    Import-Module (Join-Path $projectRoot 'cameraunlock-core\powershell\DevDeploy.psm1')

    # The entry is not only how the install is found: Assert-DevGameNotRunning
    # and Resolve-DevExeDir read Executable out of it on every path, so a
    # -GamePath argument does not stand in for a missing one. Without it the
    # failure is a null Executable inside the orchestrator.
    if (-not (Get-GameConfig -GameId 'soma')) {
        throw "cameraunlock-core/data/games.json has no 'soma' entry. Add it in cameraunlock-core and update the submodule here - passing a game directory does not work around it, because the running-game check and the exe-directory lookup read the entry too."
    }

    $result = Invoke-DevDeployASILoader `
        -GameId 'soma' `
        -GameDisplayName 'SOMA' `
        -BuildOutputPath (Join-Path $projectRoot "bin\$Configuration") `
        -ModDllName 'SomaHeadTracking.asi' `
        -VendorLoaderDll (Join-Path $projectRoot 'vendor\ultimate-asi-loader\dinput8.dll') `
        -AsiLoaderName 'version.dll' `
        -GivenPath $GamePath

    Write-Host ''
    Write-Host "Deployed to $($result.ExeDir). Launch SOMA to use head tracking." -ForegroundColor Green
} catch {
    Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
