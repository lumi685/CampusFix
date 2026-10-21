#Requires -Version 5.1
param([string]$NpmPath)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$frontendDir = Join-Path $repoRoot 'frontend'

$listener = netstat.exe -ano -p tcp | Select-String '^\s*TCP\s+\S+:5173\s+\S+\s+LISTENING\s+\d+\s*$'
if ($listener) {
    throw '127.0.0.1:5173 is already listening. Check the reported PID with netstat -ano; stop only that verified process.'
}

if (-not (Get-Command node.exe -ErrorAction SilentlyContinue)) {
    throw 'Node.js was not found on PATH.'
}
if (-not $NpmPath) {
    $npmCommand = Get-Command npm.cmd -ErrorAction SilentlyContinue
    if ($npmCommand) { $NpmPath = $npmCommand.Source }
}
if (-not $NpmPath -or -not (Test-Path -LiteralPath $NpmPath -PathType Leaf)) {
    throw 'npm.cmd was not found. Pass -NpmPath or put npm on PATH.'
}
if (-not (Test-Path -LiteralPath (Join-Path $frontendDir 'node_modules\vite\package.json'))) {
    throw 'Frontend dependencies are missing. Run npm ci --cache .npm-cache from frontend/ first.'
}

Push-Location $frontendDir
try {
    Write-Host 'Starting http://127.0.0.1:5173 (press Ctrl+C to stop)...'
    & $NpmPath run dev -- --port 5173 --strictPort
    if ($LASTEXITCODE -ne 0) { throw "Vite exited with code $LASTEXITCODE" }
} finally {
    Pop-Location
}
