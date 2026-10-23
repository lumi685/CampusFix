#Requires -Version 5.1
param([Parameter(Mandatory=$true)][string]$Server,
      [Parameter(Mandatory=$true)][string]$Migrations,
      [Parameter(Mandatory=$true)][string]$Seed)
$ErrorActionPreference='Stop'
$root=Join-Path ([IO.Path]::GetTempPath()) ('campusfix-guard-'+[guid]::NewGuid().ToString('N'))
$process=$null
$checks=0
function Assert-True($condition,[string]$message) {
    if(-not $condition) {throw $message}
}
function Request([string]$path,[string]$token,[int]$expected,[string]$code='', [string]$method='GET') {
    $headers=@{}
    if($token) {$headers.Authorization='Bearer '+$token}
    try {
        $response=Invoke-WebRequest -UseBasicParsing -Uri ($script:base+$path) -Method $method -Headers $headers -TimeoutSec 5
        $status=[int]$response.StatusCode
        $json=$response.Content|ConvertFrom-Json
    } catch {
        if(-not $_.Exception.Response) {throw}
        $status=[int]$_.Exception.Response.StatusCode
        $json=$_.ErrorDetails.Message|ConvertFrom-Json
    }
    Assert-True ($status -eq $expected) "HTTP $path expected $expected got $status"
    Assert-True ($null -ne $json.code -and $null -ne $json.message -and $json.request_id) 'Invalid response envelope'
    Assert-True ($json.PSObject.Properties.Name -contains 'data') 'Missing data field'
    if($code) {Assert-True ($json.code -eq $code) "Unexpected error code for $path"}
    if($expected -ne 200) {Assert-True ($null -eq $json.data) 'Error response leaked resource data'}
    Assert-True (-not ($json|ConvertTo-Json -Depth 10).Contains('password_hash')) 'Password hash leaked'
    Assert-True (-not ($json|ConvertTo-Json -Depth 10).Contains('token_hash')) 'Token hash leaked'
    $script:checks++
    Write-Output "PASS HTTP $method $path => $status"
    return $json
}
try {
    New-Item -ItemType Directory -Path $root|Out-Null
    $db=Join-Path $root 'guard.sqlite3'
    $fixturePath=Join-Path $root 'fixture.json'
    $stdout=Join-Path $root 'stdout.log'
    $stderr=Join-Path $root 'stderr.log'
    $listener=New-Object Net.Sockets.TcpListener([Net.IPAddress]::Loopback,0)
    $listener.Start();$port=$listener.LocalEndpoint.Port;$listener.Stop()
    $base="http://127.0.0.1:$port"
    $arguments='"{0}" "{1}" "{2}" "{3}" {4}' -f $db,$fixturePath,$Migrations,$Seed,$port
    $process=Start-Process -FilePath $Server -ArgumentList $arguments -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $ready=$false
    for($i=0;$i -lt 50;$i++) {
        if($process.HasExited) {throw ('Isolated server failed: '+(Get-Content $stderr -Raw))}
        try {Invoke-WebRequest -UseBasicParsing -Uri "$base/api/health" -TimeoutSec 1|Out-Null;$ready=$true;break} catch {Start-Sleep -Milliseconds 100}
    }
    Assert-True $ready 'Isolated server not ready'
    $fixture=Get-Content -LiteralPath $fixturePath -Raw|ConvertFrom-Json
    # Real production login route, using a per-run random password in this temporary fixture.
    $loginResponse=Invoke-WebRequest -UseBasicParsing -Method POST -Uri "$base/api/auth/login" -ContentType 'application/json' -Body (@{
        username='demo_reporter01';password=$fixture.password}|ConvertTo-Json -Compress)
    $login=$loginResponse.Content|ConvertFrom-Json
    Assert-True ($loginResponse.StatusCode -eq 200 -and $login.code -eq 0 -and $login.request_id -and $login.data.token) 'HTTP login regression'
    Assert-True ($loginResponse.Headers['Cache-Control'] -eq 'no-store') 'Login must not be cached'
    $null=Request '/api/auth/me' $login.data.token 200
    $null=Request '/api/health' '' 200
    $null=Request '/test/identity' '' 401 'AUTH_REQUIRED'
    $null=Request '/test/identity' 'cf_live_invalid_token' 401 'AUTH_INVALID'
    $null=Request '/test/identity' $fixture.expired 401 'AUTH_EXPIRED'
    $null=Request '/test/identity' $fixture.revoked 401 'AUTH_INVALID'
    $null=Request '/test/identity' $fixture.disabled 401 'AUTH_INVALID'
    $null=Request '/test/unconfigured' $fixture.reporter 401 'AUTH_INVALID'
    $null=Request '/api/auth/me' '' 401 'AUTH_REQUIRED'
    $null=Request '/api/auth/me' $fixture.reporter 200
    # Alternating identities + forged client identity fields must not mutate request context.
    $reporter=Request '/test/identity?user_id=999&role=sysadmin&capabilities=REVIEW' $fixture.reporter 200
    Assert-True ($reporter[-1].data.id -eq $fixture.reporter_id -and $reporter[-1].data.role -eq 'reporter') 'Client identity trusted'
    $worker=Request '/test/identity' $fixture.worker 200
    Assert-True ($worker[-1].data.id -eq $fixture.worker_id -and $worker[-1].data.role -eq 'worker') 'Identity crossed requests'
    $again=Request '/test/identity' $fixture.reporter 200
    Assert-True ($again[-1].data.id -eq $fixture.reporter_id) 'Request identity overwritten'
    $null=Request ("/test/order-access?id=$($fixture.order_a)&action=read") $fixture.reporter 200
    $null=Request ("/test/order-access?id=$($fixture.order_b)&action=read") $fixture.reporter 404 'RESOURCE_NOT_FOUND'
    $null=Request ("/test/order-access?id=$($fixture.order_a)&action=review") $fixture.reporter 403 'ACTION_FORBIDDEN'
    $null=Request ("/test/order-access?id=$($fixture.order_a)&action=review") $fixture.dispatcher 200
    $null=Request ("/test/order-access?id=$($fixture.order_b)&action=read") $fixture.dispatcher 404 'RESOURCE_NOT_FOUND'
    $null=Request '/test/order-access?id=999999&action=read' $fixture.reporter 404 'RESOURCE_NOT_FOUND'
    $null=Request '/api/auth/logout' $fixture.reporter 200 '' 'POST'
    $null=Request '/api/auth/me' $fixture.reporter 401 'AUTH_INVALID'
    $null=Request '/api/auth/me' $login.data.token 200
    $null=Request '/api/auth/me' $fixture.worker 200
    Stop-Process -Id $process.Id -ErrorAction Stop;$process.WaitForExit();$process=$null
    $logs=((Get-Content $stdout -Raw),(Get-Content $stderr -Raw)) -join ''
    foreach($name in 'reporter','worker','admin','dispatcher','expired','revoked','disabled') {
        Assert-True (-not $logs.Contains($fixture.$name)) 'Token in isolated server log'
    }
    Assert-True (-not $logs.Contains($fixture.password)) 'Password in isolated server log'
    Assert-True (-not $logs.Contains($login.data.token)) 'Login token in isolated server log'
    Write-Output "PASS $checks isolated HTTP Guard/auth checks; no W4 business API tested"
} catch {
    Write-Error -ErrorAction Continue $_.Exception.Message
    exit 1
} finally {
    if($process -and -not $process.HasExited) {Stop-Process -Id $process.Id;$process.WaitForExit()}
    # This generated temporary directory is the only recursive cleanup target.
    $resolved=[IO.Path]::GetFullPath($root)
    $temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if($resolved.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase) -and
       ([IO.Path]::GetFileName($resolved)).StartsWith('campusfix-guard-')) {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
