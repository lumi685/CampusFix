#Requires -Version 5.1
param([Parameter(Mandatory=$true)][string]$Server,
      [Parameter(Mandatory=$true)][string]$Fixture,
      [Parameter(Mandatory=$true)][string]$Migrations,
      [Parameter(Mandatory=$true)][string]$Seed)
$ErrorActionPreference='Stop'
$root=Join-Path ([IO.Path]::GetTempPath()) ('campusfix-order-http-'+[guid]::NewGuid().ToString('N'))
$process=$null
$lockProcess=$null
$oldDb=$env:CAMPUSFIX_DB_PATH
$oldPort=$env:CAMPUSFIX_PORT
$checks=0
function Assert-True($ok,[string]$message) {if(-not $ok) {throw $message}}
function Request([string]$path,[string]$token,$body,[int]$expected,[string]$code='',[string]$method='POST') {
    $headers=@{}
    if($token) {$headers.Authorization='Bearer '+$token}
    $args=@{UseBasicParsing=$true;Uri=$script:base+$path;Method=$method;Headers=$headers;TimeoutSec=10}
    if($null -ne $body) {
        $args.ContentType='application/json'
        $payload=if($body -is [string]) {$body} else {$body|ConvertTo-Json -Depth 5 -Compress}
        $args.Body=[Text.Encoding]::UTF8.GetBytes($payload)
    }
    try {$res=Invoke-WebRequest @args;$status=[int]$res.StatusCode;$json=$res.Content|ConvertFrom-Json;$contentType=[string]$res.Headers['Content-Type']}
    catch {
        if(-not $_.Exception.Response) {throw}
        $status=[int]$_.Exception.Response.StatusCode;$json=$_.ErrorDetails.Message|ConvertFrom-Json
        $contentType=[string]$_.Exception.Response.ContentType
    }
    Assert-True ($status -eq $expected) "Unexpected HTTP $path : $status expected $expected"
    Assert-True ($contentType -match '^application/json') 'Response must be JSON'
    Assert-True ($json.request_id -and $json.message -and $json.PSObject.Properties.Name -contains 'data') 'Invalid response envelope'
    if($code) {Assert-True ($json.code -eq $code) "Wrong error code for $path"}
    if($status -lt 400) {Assert-True ($json.code -eq 0) 'Nonzero success code'}
    $serialized=$json|ConvertTo-Json -Depth 10
    Assert-True ($serialized -notmatch 'password_hash|token_hash|stored_path|SQLite|SQLITE_') 'Sensitive database error/field exposed'
    $script:checks++
    Write-Host "PASS HTTP $method $path => $status $code"
    return $json
}
try {
    New-Item -ItemType Directory -Path $root|Out-Null
    $db=Join-Path $root 'orders.sqlite3';$fixturePath=Join-Path $root 'fixture.json'
    & $Fixture $db $fixturePath $Migrations $Seed
    Assert-True ($LASTEXITCODE -eq 0) 'Fixture initialization failed'
    $data=Get-Content -LiteralPath $fixturePath -Raw|ConvertFrom-Json
    $listener=New-Object Net.Sockets.TcpListener([Net.IPAddress]::Loopback,0)
    $listener.Start();$port=$listener.LocalEndpoint.Port;$listener.Stop()
    $env:CAMPUSFIX_DB_PATH=$db;$env:CAMPUSFIX_PORT=[string]$port;$base="http://127.0.0.1:$port"
    $out=Join-Path $root 'stdout.log';$err=Join-Path $root 'stderr.log'
    $process=Start-Process -FilePath $Server -PassThru -WindowStyle Hidden -RedirectStandardOutput $out -RedirectStandardError $err
    $ready=$false
    for($i=0;$i -lt 60;$i++) {
        if($process.HasExited) {throw 'Production server exited unexpectedly'}
        try {Invoke-WebRequest -UseBasicParsing -Uri "$base/api/health" -TimeoutSec 1|Out-Null;$ready=$true;break} catch {Start-Sleep -Milliseconds 100}
    }
    Assert-True $ready 'Production server not ready'
    # Real production login with a per-run random credential. Never print the token.
    $login=Request '/api/auth/login' '' @{username='demo_reporter01';password=$data.password} 200
    $reporter=$login.data.token;$worker=$data.demo_worker01
    $valid=@{category_id=$data.category_id;location_id=$data.location_id;title='HTTP reporter';description='Fictional description';requested_urgency='NORMAL';contact_name='Fictional';contact_phone='00000000000';expected_visit_at='2026-12-01T08:00:00.000Z'}
    $draft=Request '/api/work-orders' $reporter $valid 201
    Assert-True ($draft.data.status -eq 'DRAFT' -and $draft.data.version -eq 1 -and $null -eq $draft.data.order_no) 'Invalid draft'
    Write-Host ('ACTUAL API-06 response: '+($draft|ConvertTo-Json -Depth 5 -Compress))
    $null=Request '/api/work-orders' '' $valid 401 'AUTH_REQUIRED'
    $null=Request "/api/work-orders/$($draft.data.id)/submit" '' @{expected_version=1} 401 'AUTH_REQUIRED'
    foreach($token in @($data.demo_dispatcher,$data.demo_sysadmin)) {$null=Request '/api/work-orders' $token $valid 403 'ACTION_FORBIDDEN'}
    foreach($field in @('category_id','location_id','title','description','requested_urgency','contact_name','contact_phone')) {
        $bad=$valid.Clone();$bad.Remove($field);$null=Request '/api/work-orders' $reporter $bad 422 'VALIDATION_ERROR'
    }
    foreach($pair in @(@('title',('a'*121)),@('description',('a'*2001)),@('contact_name',('a'*81)),@('contact_phone',('1'*33)),
        @('category_id',999999),@('location_id',999999),@('category_id','1'),@('location_id',$true),
        @('requested_urgency','INVALID'),@('expected_visit_at','2026-02-30T00:00:00Z'))) {
        $bad=$valid.Clone();$bad[$pair[0]]=$pair[1];$null=Request '/api/work-orders' $reporter $bad 422 'VALIDATION_ERROR'
    }
    foreach($field in @('reporter_id','user_id','role','status','order_no')) {
        $bad=$valid.Clone();$bad[$field]='forged';$null=Request '/api/work-orders' $reporter $bad 422 'VALIDATION_ERROR'
    }
    $null=Request '/api/work-orders' $reporter @('not','object') 400 'MALFORMED_REQUEST'
    $null=Request '/api/work-orders' $reporter '{"title":' 400 'MALFORMED_REQUEST'
    $id=$draft.data.id
    foreach($token in @($data.demo_reporter02,$worker)) {$null=Request "/api/work-orders/$id/submit" $token @{expected_version=1} 404 'RESOURCE_NOT_FOUND'}
    $null=Request "/api/work-orders/$id/submit" $data.demo_dispatcher @{expected_version=1} 403 'ACTION_FORBIDDEN'
    $null=Request "/api/work-orders/$id/submit" $reporter @{expected_version=99} 409 'ORDER_VERSION_CONFLICT'
    foreach($version in @(0,-1,'1',$true,1.5)) {$null=Request "/api/work-orders/$id/submit" $reporter @{expected_version=$version} 422 'VALIDATION_ERROR'}
    $null=Request "/api/work-orders/$id/submit" $reporter '{"expected_version":1.0}' 422 'VALIDATION_ERROR'
    $null=Request '/api/work-orders/999999/submit' $reporter @{expected_version=1} 404 'RESOURCE_NOT_FOUND'
    $submit=Request "/api/work-orders/$id/submit" $reporter @{expected_version=1} 200
    Assert-True ($submit.data.status -eq 'PENDING_REVIEW' -and $submit.data.version -eq 2 -and $submit.data.order_no -match '^WX\d{8}\d{4}$' -and $submit.data.submitted_at) 'Invalid submitted order'
    Write-Host ('ACTUAL API-08 response: '+($submit|ConvertTo-Json -Depth 5 -Compress))
    $null=Request "/api/work-orders/$id/submit" $reporter @{expected_version=2} 409 'ORDER_STATUS_CONFLICT'
    $valid.title='HTTP worker';$workerDraft=Request '/api/work-orders' $worker $valid 201
    $workerSubmit=Request "/api/work-orders/$($workerDraft.data.id)/submit" $worker @{expected_version=1} 200
    Assert-True ($workerSubmit.data.order_no -ne $submit.data.order_no) 'Duplicate number'
    # An independent process holds a real SQLite write lock. Guard reads remain valid in WAL.
    $readyFile=Join-Path $root 'write-lock-ready';$releaseFile=Join-Path $root 'write-lock-release'
    $lockArgs='--hold-write "{0}" "{1}" "{2}"' -f $db,$readyFile,$releaseFile
    $lockProcess=Start-Process -FilePath $Fixture -ArgumentList $lockArgs -PassThru -WindowStyle Hidden
    for($i=0;$i -lt 50 -and -not (Test-Path -LiteralPath $readyFile);$i++) {Start-Sleep -Milliseconds 50}
    Assert-True (Test-Path -LiteralPath $readyFile) 'Independent write lock not acquired'
    $timer=[Diagnostics.Stopwatch]::StartNew()
    $null=Request '/api/work-orders' $reporter $valid 503 'DATABASE_BUSY'
    $timer.Stop();Assert-True ($timer.Elapsed.TotalSeconds -lt 9) 'Busy wait was not bounded'
    New-Item -ItemType File -Path $releaseFile|Out-Null
    $lockProcess.WaitForExit();Assert-True ($lockProcess.ExitCode -eq 0) 'Write lock fixture failed';$lockProcess=$null
    # Unimplemented routes use Drogon's default response, not our JSON envelope.
    foreach($probe in @(@('/api/work-orders',405),@("/api/work-orders/$id",404))) {
        try {$probeResponse=Invoke-WebRequest -UseBasicParsing -Uri ($base+$probe[0]);$probeStatus=[int]$probeResponse.StatusCode}
        catch {if(-not $_.Exception.Response) {throw};$probeStatus=[int]$_.Exception.Response.StatusCode}
        Assert-True ($probeStatus -eq $probe[1]) 'Unexpected implementation of out-of-scope query route'
        Write-Host "PASS unimplemented GET $($probe[0]) => $probeStatus"
    }
    $receipts=@{reporter=@{id=$id;create_request_id=$draft.request_id;submit_request_id=$submit.request_id};worker=@{id=$workerDraft.data.id;create_request_id=$workerDraft.request_id;submit_request_id=$workerSubmit.request_id}}
    $receiptPath=Join-Path $root 'receipts.json'
    $receipts|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $receiptPath -Encoding UTF8
    & $Fixture --verify $db $receiptPath
    Assert-True ($LASTEXITCODE -eq 0) 'Post-HTTP database verification failed'
    Stop-Process -Id $process.Id;$process.WaitForExit();$process=$null
    $logs=((Get-Content $out -Raw),(Get-Content $err -Raw)) -join ''
    foreach($secret in @($reporter,$worker,$data.password,$data.demo_sysadmin,$data.demo_dispatcher,$data.demo_reporter02)) {
        Assert-True (-not $logs.Contains($secret)) 'Credential leaked in server logs'
    }
    Write-Host "PASS $checks production HTTP checks; database and log verification passed"
} catch {Write-Error -ErrorAction Continue $_.Exception.Message;exit 1}
finally {
    if($process -and -not $process.HasExited) {Stop-Process -Id $process.Id;$process.WaitForExit()}
    if($lockProcess -and -not $lockProcess.HasExited) {Stop-Process -Id $lockProcess.Id;$lockProcess.WaitForExit()}
    $env:CAMPUSFIX_DB_PATH=$oldDb;$env:CAMPUSFIX_PORT=$oldPort
    $resolved=[IO.Path]::GetFullPath($root);$temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if($resolved.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase) -and ([IO.Path]::GetFileName($resolved)).StartsWith('campusfix-order-http-')) {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
