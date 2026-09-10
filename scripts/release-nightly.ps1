# Thin shim. Determine version, delegate to the shared publisher.
# See cameraunlock-core/powershell/NightlyRelease.psm1 for what it does.

[CmdletBinding()]
param(
    [switch]$AllowDirty
)

$ErrorActionPreference = 'Stop'

$projectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')

Import-Module (Join-Path $projectRoot 'cameraunlock-core\powershell\NightlyRelease.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'Version.psm1') -Force

# -NoNexusZip: there is no Nexus route for SOMA, so package-release.ps1 builds
# the installer ZIP alone. Without the switch Publish-NightlyBuild treats the
# missing Nexus ZIP as fatal and every nightly fails.
Publish-NightlyBuild `
    -ModId 'soma' `
    -ModName 'SomaHeadTracking' `
    -Version (Get-ModVersion -ProjectRoot $projectRoot) `
    -ProjectRoot $projectRoot `
    -NoNexusZip `
    -AllowDirty:$AllowDirty
