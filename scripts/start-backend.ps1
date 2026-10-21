#Requires -Version 5.1
param(
    [string]$CMakePath,
    [string]$VcpkgRoot,
    [string]$Generator = 'Visual Studio 18 2026',
    [string]$Triplet = 'x64-windows'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$backendDir = Join-Path $repoRoot 'backend'
$buildDir = Join-Path $backendDir 'build'
$cachePath = Join-Path $buildDir 'CMakeCache.txt'

function Get-CacheValue([string]$key) {
    if (-not (Test-Path -LiteralPath $cachePath)) { return $null }
    $line = Get-Content -LiteralPath $cachePath | Where-Object { $_.StartsWith("${key}:") } | Select-Object -First 1
    if ($line) { return ($line -split '=', 2)[1] }
    return $null
}

function Assert-PortFree([int]$port) {
    $listener = netstat.exe -ano -p tcp | Select-String "^\s*TCP\s+\S+:$port\s+\S+\s+LISTENING\s+(\d+)\s*$"
    if ($listener) {
        throw "127.0.0.1:$port is already listening. Check the reported PID with netstat -ano; stop only that verified process."
    }
}

if ($env:CAMPUSFIX_PORT -and $env:CAMPUSFIX_PORT -ne '8848') {
    throw 'CAMPUSFIX_PORT must be unset or 8848 for the current Vite development proxy.'
}
Assert-PortFree 8848

if (-not $CMakePath) {
    $CMakePath = Get-CacheValue 'CMAKE_COMMAND'
}
if (-not $CMakePath) {
    $cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($cmakeCommand) { $CMakePath = $cmakeCommand.Source }
}
if (-not $CMakePath -or -not (Test-Path -LiteralPath $CMakePath -PathType Leaf)) {
    throw 'CMake not found. Pass -CMakePath with the full path to cmake.exe or put CMake on PATH.'
}
$CMakePath = (Resolve-Path -LiteralPath $CMakePath).Path

if (Test-Path -LiteralPath $cachePath) {
    $cachedToolchain = Get-CacheValue 'CMAKE_TOOLCHAIN_FILE'
    if ($cachedToolchain -and -not (Test-Path -LiteralPath $cachedToolchain -PathType Leaf)) {
        throw 'Existing CMake cache points to a missing vcpkg toolchain. Use a fresh checkout/build directory; do not reuse another machine cache.'
    }
}

if (-not (Test-Path -LiteralPath $cachePath)) {
    if ($VcpkgRoot) {
        $toolchain = Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
    } elseif ($env:VCPKG_ROOT) {
        $toolchain = Join-Path $env:VCPKG_ROOT 'scripts\buildsystems\vcpkg.cmake'
    } else {
        $toolchain = Get-CacheValue 'CMAKE_TOOLCHAIN_FILE'
    }
    if (-not $toolchain -or -not (Test-Path -LiteralPath $toolchain -PathType Leaf)) {
        throw 'vcpkg toolchain not found. Pass -VcpkgRoot or set VCPKG_ROOT before the first configure.'
    }
    $toolchain = (Resolve-Path -LiteralPath $toolchain).Path
    Write-Host "Configuring backend: generator=$Generator triplet=$Triplet"
    & $CMakePath -S $backendDir -B $buildDir -G $Generator -A x64 "-DCMAKE_TOOLCHAIN_FILE=$toolchain" "-DVCPKG_TARGET_TRIPLET=$Triplet"
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE" }
}

Write-Host 'Building CampusFix backend (Debug)...'
& $CMakePath --build $buildDir --config Debug --target campusfix_backend --parallel 4
if ($LASTEXITCODE -ne 0) { throw "CMake build failed with exit code $LASTEXITCODE" }

$exePath = Join-Path $buildDir 'Debug\campusfix_backend.exe'
if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
    throw "Backend executable not found: $exePath"
}

Assert-PortFree 8848
Write-Host 'Starting http://127.0.0.1:8848 (press Ctrl+C to stop)...'
& $exePath
if ($LASTEXITCODE -ne 0) { throw "Backend exited with code $LASTEXITCODE" }
