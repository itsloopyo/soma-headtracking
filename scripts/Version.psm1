#Requires -Version 5.1
<#
.SYNOPSIS
    The mod version: where it is read from, where it is written to, and what
    has to agree with it.
.DESCRIPTION
    src/version.h is canonical. Three other files carry the same number and are
    derived from it:

      src/version.h      VERSION_STRING   - what the built .asi reports
      CMakeLists.txt     project(VERSION) - what the release tag is checked against
      scripts/install.cmd MOD_VERSION     - what the installer writes to the state file

    Before this module each of release.ps1, package-release.ps1 and
    release-nightly.ps1 carried its own copy of the regexes, so the set of files
    that carry the version was written down three times and read back
    differently in each. It is written down once, here.

    .github/workflows/release.yml passes the same version.h pattern to
    cameraunlock-core's release-mod.yml as a workflow input; that one cannot
    import a module and is the only other copy.
#>

Set-StrictMode -Version Latest

<#
.SYNOPSIS
    The files that carry the mod version, so no caller has to name them again.
#>
function Get-ModVersionPaths {
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)

    return [pscustomobject]@{
        VersionHeader = Join-Path $ProjectRoot 'src\version.h'
        CMakeLists    = Join-Path $ProjectRoot 'CMakeLists.txt'
        InstallCmd    = Join-Path $ProjectRoot 'scripts\install.cmd'
    }
}

<#
.SYNOPSIS
    Reads X.Y.Z from src/version.h.
#>
function Get-ModVersion {
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)

    $path = (Get-ModVersionPaths -ProjectRoot $ProjectRoot).VersionHeader
    if (-not (Test-Path -LiteralPath $path)) { throw "Version header not found: $path" }

    $content = Get-Content -LiteralPath $path -Raw
    $major = [regex]::Match($content, 'VERSION_MAJOR\s*=\s*(\d+)').Groups[1].Value
    $minor = [regex]::Match($content, 'VERSION_MINOR\s*=\s*(\d+)').Groups[1].Value
    $patch = [regex]::Match($content, 'VERSION_PATCH\s*=\s*(\d+)').Groups[1].Value
    if (-not $major -or -not $minor -or -not $patch) {
        throw "Could not parse VERSION_MAJOR/MINOR/PATCH from $path"
    }
    return "$major.$minor.$patch"
}

# Windows PowerShell 5.1 defaults BOTH halves of a read-modify-write to the
# system ANSI codepage - Get-Content on a BOM-less file and Set-Content on the
# way back - so the pair is lossless only by accident, and only while every byte
# stays under 0x80. Core's own changelog writer already reads and writes UTF-8
# (ReleaseWorkflow.psm1), so the two disagree about the same file the moment a
# commit subject carries a curly quote or an accented name.
#
# Both halves go through these two functions. Moving only the write is worse
# than moving neither: it decodes as ANSI and re-encodes as UTF-8, which bakes
# the mojibake in permanently. BOM-free, because cmd.exe mis-parses a batch file
# that starts with one and MSVC does not want one either.
function Read-TextFileUtf8 {
    param([Parameter(Mandatory = $true)][string]$Path)
    return [System.IO.File]::ReadAllText($Path, (New-Object System.Text.UTF8Encoding($false)))
}

function Write-TextFileUtf8 {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Text
    )
    [System.IO.File]::WriteAllText($Path, $Text, (New-Object System.Text.UTF8Encoding($false)))
}

<#
.SYNOPSIS
    Writes a new version into src/version.h and the two files derived from it.
#>
function Set-ModVersion {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$Version
    )

    # Checked here rather than trusted from the caller: this function rewrites
    # three tracked files, and a version that is not X.Y.Z either indexes past
    # $parts or writes a number the release tag will not match.
    if ($Version -notmatch '^\d+\.\d+\.\d+$') {
        throw "Set-ModVersion expects a X.Y.Z version, got '$Version'."
    }

    $paths = Get-ModVersionPaths -ProjectRoot $ProjectRoot
    $parts = $Version.Split('.')

    $header = Read-TextFileUtf8 -Path $paths.VersionHeader
    $header = $header -replace 'VERSION_MAJOR\s*=\s*\d+', "VERSION_MAJOR = $($parts[0])"
    $header = $header -replace 'VERSION_MINOR\s*=\s*\d+', "VERSION_MINOR = $($parts[1])"
    $header = $header -replace 'VERSION_PATCH\s*=\s*\d+', "VERSION_PATCH = $($parts[2])"
    $header = $header -replace 'VERSION_STRING\s*=\s*"[^"]*"', "VERSION_STRING = `"$Version`""
    Write-TextFileUtf8 -Path $paths.VersionHeader -Text $header

    $cmake = Read-TextFileUtf8 -Path $paths.CMakeLists
    $cmake = $cmake -replace '(project\(SomaHeadTracking\s+VERSION\s+)\d+\.\d+\.\d+', "`${1}$Version"
    Write-TextFileUtf8 -Path $paths.CMakeLists -Text $cmake

    $install = Read-TextFileUtf8 -Path $paths.InstallCmd
    $install = $install -replace 'set "MOD_VERSION=[^"]*"', "set `"MOD_VERSION=$Version`""
    Write-TextFileUtf8 -Path $paths.InstallCmd -Text $install
}

<#
.SYNOPSIS
    Throws unless every derived copy of the version matches src/version.h.
.DESCRIPTION
    This is what makes "derived from version.h" true rather than intended.
    Set-ModVersion writes all four in one pass, so a mismatch means one of them
    was hand-edited; shipping it would tag a release against one number and
    install a different one.
#>
function Assert-ModVersionsInSync {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$Version
    )

    $paths = Get-ModVersionPaths -ProjectRoot $ProjectRoot

    $headerString = [regex]::Match(
        (Get-Content -LiteralPath $paths.VersionHeader -Raw),
        'VERSION_STRING\s*=\s*"([^"]*)"').Groups[1].Value
    if ($headerString -ne $Version) {
        throw "src/version.h sets VERSION_STRING=$headerString but its VERSION_MAJOR/MINOR/PATCH say $Version. The built .asi would report a version nothing else agrees with."
    }

    $cmakeVersion = [regex]::Match(
        (Get-Content -LiteralPath $paths.CMakeLists -Raw),
        'project\(SomaHeadTracking\s+VERSION\s+(\d+\.\d+\.\d+)').Groups[1].Value
    if ($cmakeVersion -ne $Version) {
        throw "CMakeLists.txt declares project version $cmakeVersion but src/version.h says $Version."
    }

    $installCmdVersion = [regex]::Match(
        (Get-Content -LiteralPath $paths.InstallCmd -Raw),
        'set "MOD_VERSION=([^"]*)"').Groups[1].Value
    if ($installCmdVersion -ne $Version) {
        throw "install.cmd records MOD_VERSION=$installCmdVersion but src/version.h says $Version. The installer would write a state file the launcher then reads as the wrong version."
    }
}

Export-ModuleMember -Function @(
    'Get-ModVersionPaths',
    'Get-ModVersion',
    'Set-ModVersion',
    'Assert-ModVersionsInSync',
    'Read-TextFileUtf8',
    'Write-TextFileUtf8'
)
