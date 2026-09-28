#!/usr/bin/env pwsh
#Requires -Version 5.1
<#
.SYNOPSIS
    Cut a release of SOMA Head Tracking.
.DESCRIPTION
    Validates preconditions, writes the new version into src/version.h and the
    two files derived from it, generates CHANGELOG.md from the commits since the
    last tag, builds, commits, tags and pushes. The pushed tag triggers
    .github/workflows/release.yml.

    Runs unattended end to end. Typing the command is the authorization: there
    is no confirmation prompt and no -Yes switch. Every safety property is a
    deterministic precondition that exits non-zero with a one-line diagnostic -
    on main, clean tree, tag absent, semver valid, notices in sync.

    Never destructive: no force push, no amend, no tag overwrite.
.NOTES
    Run via: pixi run release <major|minor|patch|nightly|X.Y.Z>
#>
param(
    [Parameter(Position = 0)]
    [string]$Version = '',

    # Ship a release even when every commit since the last tag was filtered as
    # noise (writes a maintenance changelog entry instead of aborting).
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot   = Split-Path -Parent $PSScriptRoot
$changelogPath = Join-Path $projectRoot 'CHANGELOG.md'

# Rolling dev pre-release takes a different path entirely.
if ($Version -eq 'nightly') {
    & (Join-Path $PSScriptRoot 'release-nightly.ps1')
    exit $LASTEXITCODE
}

Import-Module (Join-Path $projectRoot 'cameraunlock-core\powershell\ReleaseWorkflow.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'Version.psm1') -Force

# Which files carry the version is Version.psm1's to know; the commit below
# stages exactly what Set-ModVersion writes.
$versionFiles = Get-ModVersionPaths -ProjectRoot $projectRoot

Write-Host '=== SOMA Head Tracking Release ===' -ForegroundColor Cyan
Write-Host ''

$currentVersion = Get-ModVersion -ProjectRoot $projectRoot

if ([string]::IsNullOrWhiteSpace($Version)) {
    Write-Host "Current version: $currentVersion" -ForegroundColor White
    Write-Host 'Usage: pixi run release <major|minor|patch|nightly|X.Y.Z>' -ForegroundColor Yellow
    exit 0
}

try {
    $Version = Resolve-ReleaseVersion -Argument $Version -CurrentVersion $currentVersion
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    Write-Host "Error: '$Version' is not a X.Y.Z release version" -ForegroundColor Red
    exit 1
}

# A version below launcher-manifest.json's canonical_since would ship the canonical
# config under a version that predates it.
Assert-ReleaseNotBelowCanonicalSince -RepoRoot $projectRoot -Version $Version

$tagName = "v$Version"

$currentBranch = git -C $projectRoot rev-parse --abbrev-ref HEAD
if ($currentBranch -ne 'main') {
    Write-Host "Error: releases are cut from 'main' (currently on '$currentBranch')" -ForegroundColor Red
    exit 1
}

if (-not (Test-CleanGitStatus)) {
    Write-Host 'Error: working tree has uncommitted changes' -ForegroundColor Red
    exit 1
}

if (Test-GitTagExists -Tag $tagName) {
    Write-Host "Error: tag '$tagName' already exists" -ForegroundColor Red
    exit 1
}

Write-Host "Running the full test suite..." -ForegroundColor Cyan
Push-Location $projectRoot
try {
    pixi run test
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: pixi run test failed. Nothing was changed." -ForegroundColor Red
        exit 1
    }
} finally {
    Pop-Location
}

# THIRD-PARTY-NOTICES.md names the cameraunlock-core commit compiled into the
# release ZIP, and bumping the submodule does not touch it. Packaging refuses
# to ship that mismatch, so re-sync it here and let this release carry the
# correction rather than failing in CI after the tag is already pushed.
#
# Below the three preconditions, not above them: this is the first thing in the
# script that writes to the repository, and running it first meant a release
# stopped by the on-main, clean-tree or tag-exists gate had already left a
# commit behind on the user's branch. Test-CleanGitStatus has passed by here, so
# any diff this leaves is one sync-core-notices.ps1 just made.
#
# Reset first: a .ps1 that returns without an `exit` leaves $LASTEXITCODE at
# whatever the last NATIVE command set, so the check below would otherwise
# read some earlier git invocation's code rather than this script's.
$global:LASTEXITCODE = 0
& (Join-Path $projectRoot 'cameraunlock-core\scripts\sync-core-notices.ps1') -Repo $projectRoot
if ($LASTEXITCODE -ne 0) { throw "sync-core-notices.ps1 exited $LASTEXITCODE - fix THIRD-PARTY-NOTICES.md before releasing." }
& git -C $projectRoot diff --quiet -- THIRD-PARTY-NOTICES.md
if ($LASTEXITCODE -ne 0) {
    & git -C $projectRoot commit -q -m 'chore: record the cameraunlock-core commit this build compiles' -- THIRD-PARTY-NOTICES.md
    if ($LASTEXITCODE -ne 0) { throw "Could not commit the re-synced THIRD-PARTY-NOTICES.md." }
    Write-Host 'THIRD-PARTY-NOTICES.md re-synced to the pinned cameraunlock-core commit.' -ForegroundColor Yellow
}

Write-Host "Current version: $currentVersion" -ForegroundColor Gray
Write-Host "New version:     $Version" -ForegroundColor Green
Write-Host ''

# CHANGELOG first. This is the gate that aborts when every commit since the last
# tag was noise, so running it before any file is mutated leaves a clean tree on
# abort instead of a half-applied version bump with no tag.
Write-Host 'Generating CHANGELOG from commits...' -ForegroundColor Cyan
try {
    New-ChangelogFromCommits `
        -ChangelogPath $changelogPath `
        -Version $Version `
        -ArtifactPaths @('src/', 'CMakeLists.txt', 'cameraunlock-core/', 'scripts/install.cmd', 'scripts/uninstall.cmd') `
        -Maintenance:$Force
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host 'No user-facing changes to release. Re-run with -Force for a maintenance release.' -ForegroundColor Yellow
    exit 1
}

Write-Host "Updating version to $Version..." -ForegroundColor Cyan
Set-ModVersion -ProjectRoot $projectRoot -Version $Version

Write-Host 'Building Release configuration...' -ForegroundColor Cyan
& pixi run build-release
if ($LASTEXITCODE -ne 0) {
    # Put the four files back. Without this a compile error leaves the version
    # bump and the changelog entry uncommitted, and the next attempt trips
    # Test-CleanGitStatus and refuses to run until someone unpicks by hand which
    # of those edits was this script's.
    git -C $projectRoot checkout -- $versionFiles.VersionHeader $versionFiles.CMakeLists $versionFiles.InstallCmd $changelogPath
    Write-Host 'Error: build-release failed - version bump and CHANGELOG entry reverted' -ForegroundColor Red
    exit 1
}

Write-Host "Committing Release v$Version..." -ForegroundColor Cyan
git -C $projectRoot add -- $versionFiles.VersionHeader $versionFiles.CMakeLists $versionFiles.InstallCmd $changelogPath
if ($LASTEXITCODE -ne 0) { throw 'git add failed' }
git -C $projectRoot commit -m "Release v$Version"
if ($LASTEXITCODE -ne 0) { throw 'git commit failed' }

New-ReleaseTag -Version $Version -Message "SOMA Head Tracking v$Version" -Branch 'main'

Write-Host ''
Write-Host "Release $tagName pushed. CI:" -ForegroundColor Green
Write-Host '  https://github.com/itsloopyo/soma-headtracking/actions' -ForegroundColor Cyan
