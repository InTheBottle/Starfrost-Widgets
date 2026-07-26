# Builds release and stages a mod-manager-ready archive in dist\, rooted at Data.

[CmdletBinding()]
param(
    # Defaults to the version in CMakeLists.txt.
    [string] $Version,

    # Skip cmake and package whatever is already built.
    [switch] $NoBuild
)

$ErrorActionPreference = 'Stop'
$repo = $PSScriptRoot

if (-not $Version) {
    $cmake = Get-Content (Join-Path $repo 'CMakeLists.txt') -Raw
    if ($cmake -notmatch 'VERSION\s+(\d+\.\d+\.\d+)') {
        throw 'Could not read VERSION out of CMakeLists.txt; pass -Version instead.'
    }
    $Version = $Matches[1]
}

# CommonLibSSE banners to stderr, which ErrorActionPreference=Stop would treat as fatal.
function Invoke-Native([string] $What, [scriptblock] $Command) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { & $Command } finally { $ErrorActionPreference = $previous }
    if ($LASTEXITCODE -ne 0) { throw "$What failed with exit code $LASTEXITCODE." }
}

if (-not $NoBuild) {
    if (-not $env:VCPKG_ROOT) { throw 'VCPKG_ROOT is not set.' }
    Invoke-Native 'cmake configure' { cmake --preset release }
    Invoke-Native 'cmake build' { cmake --build --preset release }
}

$built = Join-Path $repo 'build\release\Release'
$stage = Join-Path $repo "dist\StarfrostWidgets-$Version"
$plugins = Join-Path $stage 'SKSE\Plugins'

# Start empty so a renamed or dropped file cannot linger in the archive.
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $plugins | Out-Null

Copy-Item (Join-Path $built 'StarfrostWidgets.dll') $plugins
Copy-Item (Join-Path $built 'StarfrostWidgets.pdb') $plugins
Copy-Item (Join-Path $repo 'SKSE\Plugins\StarfrostWidgets.ini') $plugins
Copy-Item (Join-Path $repo 'README.txt') $stage

$zip = Join-Path $repo "dist\StarfrostWidgets-$Version.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal

$size = [math]::Round((Get-Item $zip).Length / 1MB, 2)
Write-Host "Packaged $zip ($size MB)"
