#Requires -Version 5.1
param(
    [string]$BackendUrl = 'http://127.0.0.1:8848',
    [string]$FrontendUrl = 'http://127.0.0.1:5173'
)

$ErrorActionPreference = 'Stop'

function Get-Page([string]$url) {
    return Invoke-WebRequest -UseBasicParsing -Uri $url -TimeoutSec 10 -ErrorAction Stop
}

function Assert-Health([string]$label, [string]$url) {
    $response = Get-Page $url
    if ($response.StatusCode -ne 200) { throw "$label returned HTTP $($response.StatusCode), expected 200" }
    $contentType = [string]$response.Headers['Content-Type']
    if ($contentType -notmatch '^application/json(?:\s*;|$)') {
        throw "$label returned '$contentType', expected JSON (not an HTML fallback)"
    }

    try { $body = $response.Content | ConvertFrom-Json -ErrorAction Stop }
    catch { throw "$label returned invalid JSON: $($_.Exception.Message)" }

    if (($body.code -isnot [int] -and $body.code -isnot [long]) -or $body.code -ne 0) {
        throw "$label has invalid code"
    }
    if ($body.data.status -ne 'ok') { throw "$label has invalid data.status" }
    if ($body.data.service -ne 'campusfix-backend') { throw "$label has invalid data.service" }
    if ([string]::IsNullOrWhiteSpace([string]$body.request_id)) { throw "$label has empty request_id" }

    Write-Host "PASS $label HTTP 200 JSON code=0 status=ok service=campusfix-backend request_id=$($body.request_id)"
    return [string]$body.request_id
}

try {
    $directUrl = $BackendUrl.TrimEnd('/') + '/api/health'
    $proxyUrl = $FrontendUrl.TrimEnd('/') + '/api/health'
    $directId1 = Assert-Health 'backend #1' $directUrl
    $directId2 = Assert-Health 'backend #2' $directUrl
    if ($directId1 -eq $directId2) { throw 'Backend repeated request_id' }
    $proxyId1 = Assert-Health 'proxy #1' $proxyUrl
    $proxyId2 = Assert-Health 'proxy #2' $proxyUrl
    if ($proxyId1 -eq $proxyId2) { throw 'Proxy repeated request_id' }

    $homepage = Get-Page ($FrontendUrl.TrimEnd('/') + '/')
    if ($homepage.StatusCode -ne 200) { throw "Frontend homepage returned HTTP $($homepage.StatusCode)" }
    if ([string]$homepage.Headers['Content-Type'] -notmatch '^text/html(?:\s*;|$)') {
        throw 'Frontend homepage is not HTML'
    }
    if ($homepage.Content -notmatch '<title>CampusFix</title>' -or $homepage.Content -notmatch '<div id="app"></div>') {
        throw 'Frontend homepage is missing CampusFix title or Vue mount point'
    }
    Write-Host 'PASS frontend homepage HTTP 200 HTML with CampusFix title and Vue mount point'
    Write-Host 'PASS all W2-D5 smoke checks'
    exit 0
} catch {
    [Console]::Error.WriteLine("FAIL W2-D5 smoke: $($_.Exception.Message)")
    exit 1
}
