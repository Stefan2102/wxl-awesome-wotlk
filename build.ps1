#requires -Version 5.1
<#
.SYNOPSIS
    Build (and optionally deploy) wxl-awesome-wotlk.dll, a WarcraftXL extension.

.PARAMETER Config
    Build configuration. Default: Release.

.PARAMETER CorePath
    WarcraftXL checkout to build against. Optional once it has been cached by a first run, or when
    the checkout sits beside this one as ..\wxl-core. Also read from $env:WXL_CORE_DIR.

.PARAMETER ClientPath
    Client directory to deploy into. Optional; without it the DLL is only built.

.PARAMETER Clean
    Delete the build directory before configuring, forcing a from-scratch build.

.EXAMPLE
    .\build.ps1
.EXAMPLE
    .\build.ps1 -CorePath ..\wxl-core -ClientPath D:\Path\To\Client
#>
param(
    [string]$Config = "Release",
    [string]$CorePath,
    [string]$ClientPath,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$root     = $PSScriptRoot
$buildDir = Join-Path $root "build"

if (-not (Test-Path (Join-Path $root "CMakeLists.txt"))) {
    throw "CMakeLists.txt is missing from '$root'. Keep build.ps1 in the project root directory."
}

function Get-CachedValue([string]$cacheDir, [string]$key) {
    $cache = Join-Path $cacheDir "CMakeCache.txt"
    if (Test-Path $cache) {
        $hit = Select-String -Path $cache -Pattern "^$key`:PATH=(.+)$" | Select-Object -First 1
        if ($hit) {
            $value = $hit.Matches[0].Groups[1].Value.Trim()
            if ($value) { return $value }
        }
    }
    return $null
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "cmake was not found on PATH. Install CMake 3.20 or later, or run this from a Visual Studio developer prompt."
}

function Invoke-Native([string]$exe, [string[]]$cmdArgs) {
    Write-Host ">> $exe $($cmdArgs -join ' ')" -ForegroundColor Cyan
    $prev = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try { & $exe @cmdArgs } finally { $ErrorActionPreference = $prev }
    if ($LASTEXITCODE -ne 0) { throw "Failure ($LASTEXITCODE): $exe $($cmdArgs -join ' ')" }
}

# WarcraftXL: the switch wins, then the environment, then whatever the cache already configured
# with, and finally the sibling checkout. CMake applies the same fallback, so an unresolved value
# here is not an error yet.
if (-not $CorePath) { $CorePath = $env:WXL_CORE_DIR }
if (-not $CorePath) { $CorePath = Get-CachedValue $buildDir "WXL_CORE_DIR" }
if ($CorePath -and -not (Test-Path $CorePath)) {
    throw "WarcraftXL path not found: $CorePath"
}

if (-not $ClientPath) { $ClientPath = Get-CachedValue $buildDir "CLIENT_PATH" }
if ($ClientPath -and -not (Test-Path $ClientPath)) {
    throw "Client path not found: $ClientPath"
}

Write-Host "Project : $root"
if ($CorePath)   { Write-Host "Core    : $CorePath" }
if ($ClientPath) { Write-Host "Client  : $ClientPath" } else { Write-Host "Client  : (not deploying)" }
Write-Host "Config  : $Config"
Write-Host ""

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Clean $buildDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

$needConfigure = $Clean `
    -or $PSBoundParameters.ContainsKey('CorePath') `
    -or $PSBoundParameters.ContainsKey('ClientPath') `
    -or (-not (Test-Path (Join-Path $buildDir "CMakeCache.txt")))

if ($needConfigure) {
    $configureArgs = @("-S", $root, "-B", $buildDir, "-A", "Win32")
    if ($CorePath)   { $configureArgs += "-DWXL_CORE_DIR=$CorePath" }
    if ($ClientPath) { $configureArgs += "-DCLIENT_PATH=$ClientPath" }
    Invoke-Native "cmake" $configureArgs
}

$sw = [System.Diagnostics.Stopwatch]::StartNew()
Invoke-Native "cmake" @("--build", $buildDir, "--config", $Config, "--parallel")
$sw.Stop()

$output = Join-Path $buildDir "$Config\wxl-awesome-wotlk.dll"
Write-Host ""
Write-Host "OK - built in $([int]$sw.Elapsed.TotalSeconds)s -> $output" -ForegroundColor Green
if ($ClientPath) {
    Write-Host "Deployed to $ClientPath\Extensions\wxl-awesome-wotlk" -ForegroundColor Green
}
