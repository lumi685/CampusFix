#Requires -Version 5.1
param([Parameter(Mandatory=$true)][string]$Server,[Parameter(Mandatory=$true)][string]$Fixture,
      [Parameter(Mandatory=$true)][string]$Migrations,[Parameter(Mandatory=$true)][string]$Seed)
$ErrorActionPreference='Stop'
$root=Join-Path ([IO.Path]::GetTempPath()) ('campusfix-read-http-'+[guid]::NewGuid().ToString('N'))
$process=$null;$checks=0;$oldDb=$env:CAMPUSFIX_DB_PATH;$oldPort=$env:CAMPUSFIX_PORT
function Assert-True($ok,[string]$message) {if(-not $ok) {throw $message}}
function Request([string]$path,[string]$token,[int]$expected=200,[string]$code='') {
    $headers=@{};if($token) {$headers.Authorization='Bearer '+$token}
    try {
        $res=Invoke-WebRequest -UseBasicParsing -Uri ($script:base+$path) -Headers $headers -TimeoutSec 8
        $status=[int]$res.StatusCode;$type=[string]$res.Headers['Content-Type'];$body=$res.Content|ConvertFrom-Json
        Assert-True ($res.Headers['Cache-Control'] -eq 'no-store') 'Read response cache policy'
    } catch {
        if(-not $_.Exception.Response) {throw}
        $status=[int]$_.Exception.Response.StatusCode;$type=[string]$_.Exception.Response.ContentType
        $body=$_.ErrorDetails.Message|ConvertFrom-Json
    }
    Assert-True ($status -eq $expected) "Unexpected HTTP $path : got $status expected $expected"
    Assert-True ($type -match '^application/json') 'Not JSON'
    Assert-True ($body.request_id -and $body.message -and $body.PSObject.Properties.Name -contains 'data') 'Invalid response envelope'
    if($code) {Assert-True ($body.code -eq $code) "Wrong error code for $path"}
    if($status -eq 200) {Assert-True ($body.code -eq 0) 'Invalid success code'}
    $encoded=$body|ConvertTo-Json -Depth 12
    Assert-True ($encoded -notmatch 'password_hash|token_hash|stored_path|metadata_json|DO_NOT_LEAK_(INTERNAL|METADATA|STORED_PATH|ASSIGN_REASON)|SQLite|SQLITE_') 'Sensitive nested field/value leaked'
    $script:checks++;Write-Host "PASS HTTP GET $path => $status $code";return $body
}
function Exact-Ids($page,$expected) {
    $actual=(@($page.data.items|ForEach-Object {[long]$_.id})|Sort-Object) -join ','
    $wanted=(@($expected)|Sort-Object) -join ','
    Assert-True ($actual -eq $wanted -and $page.data.total -eq @($expected).Count) 'SQL scope or authorized total mismatch'
}
function WhiteList($row,[bool]$detail,[bool]$contact) {
    $allowed=@('id','order_no','title','category','location','requested_urgency','approved_urgency','status','version','created_at','submitted_at','updated_at')
    if($detail) {$allowed+='attachments'}
    if($contact) {$allowed+=@('description','expected_visit_at','contact_name','contact_phone','current_assignment')}
    foreach($key in $row.PSObject.Properties.Name) {Assert-True ($allowed -contains $key) "Unexpected top-level DTO key $key"}
    foreach($reference in @($row.category,$row.location)) {
        Assert-True ((@($reference.PSObject.Properties.Name|Sort-Object) -join ',') -eq 'code,id,name') 'Unexpected nested reference key'
    }
    if($contact) {
        Assert-True ($row.PSObject.Properties.Name -contains 'contact_phone' -and $row.PSObject.Properties.Name -contains 'description') 'Missing authorized contact fields'
        if($null -ne $row.current_assignment) {
            Assert-True ((@($row.current_assignment.PSObject.Properties.Name|Sort-Object) -join ',') -eq 'worker_id,worker_name') 'Unexpected nested assignment field'
        }
    } else {
        Assert-True ($row.PSObject.Properties.Name -notcontains 'contact_phone' -and $row.PSObject.Properties.Name -notcontains 'description' -and $row.PSObject.Properties.Name -notcontains 'current_assignment') 'Historical/list DTO leaked detail'
    }
    if($detail) {Assert-True (@($row.attachments).Count -eq 0) 'Attachments are explicitly empty in this stage'}
}
function Mode([string]$name) {& $Fixture --grant $script:db $name;Assert-True ($LASTEXITCODE -eq 0) 'Grant fixture update failed'}
try {
    New-Item -ItemType Directory -Path $root|Out-Null
    $db=Join-Path $root 'read.sqlite3';$fixturePath=Join-Path $root 'fixture.json'
    & $Fixture --init $db $fixturePath $Migrations $Seed
    Assert-True ($LASTEXITCODE -eq 0) 'Read fixture initialization failed'
    $f=Get-Content -LiteralPath $fixturePath -Raw|ConvertFrom-Json
    $r=$f.demo_reporter01;$b=$f.demo_reporter02;$w=$f.demo_worker01;$w2=$f.demo_worker02;$d=$f.demo_dispatcher;$a=$f.demo_sysadmin
    $listener=New-Object Net.Sockets.TcpListener([Net.IPAddress]::Loopback,0);$listener.Start();$port=$listener.LocalEndpoint.Port;$listener.Stop()
    $env:CAMPUSFIX_DB_PATH=$db;$env:CAMPUSFIX_PORT=[string]$port;$base="http://127.0.0.1:$port"
    $stdout=Join-Path $root 'stdout.log';$stderr=Join-Path $root 'stderr.log'
    $process=Start-Process -FilePath $Server -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $ready=$false
    for($i=0;$i -lt 60;$i++) {
        if($process.HasExited) {throw 'Production backend exited'}
        try {$null=Invoke-WebRequest -UseBasicParsing -Uri "$base/api/health" -TimeoutSec 1;$ready=$true;break} catch {Start-Sleep -Milliseconds 100}
    }
    Assert-True $ready 'Backend not ready'
    $null=Request '/api/work-orders?view=reported' '' 401 'AUTH_REQUIRED'
    $null=Request '/api/work-orders/1' '' 401 'AUTH_REQUIRED'
    $null=Request '/api/work-orders?view=reported' 'cf_live_invalid_test' 401 'AUTH_INVALID'
    $reported=Request '/api/work-orders?view=reported' $r;Exact-Ids $reported @(1,$f.current,$f.draft)
    Assert-True ($reported.data.page -eq 1 -and $reported.data.page_size -eq 20) 'Default pagination'
    foreach($row in $reported.data.items) {WhiteList $row $false $false}
    Write-Host ('ACTUAL API-09: '+($reported|ConvertTo-Json -Depth 8 -Compress))
    Exact-Ids (Request '/api/work-orders?view=reported' $b) @(2,$f.history,$f.foreign)
    Exact-Ids (Request '/api/work-orders?view=reported' $w) @(4)
    Exact-Ids (Request '/api/work-orders?view=assigned' $w) @($f.current,$f.history)
    Exact-Ids (Request '/api/work-orders?view=assigned' $w2) @($f.history,$f.foreign)
    Exact-Ids (Request '/api/work-orders?view=managed' $d) @(1,3,$f.current,$f.history,$f.draft)
    Exact-Ids (Request '/api/work-orders?view=managed' $a) @(2,4,$f.foreign)
    foreach($pair in @(@($r,'assigned'),@($r,'managed'),@($w,'managed'),@($d,'reported'),@($a,'assigned'))) {
        $null=Request "/api/work-orders?view=$($pair[1])" $pair[0] 403 'ACTION_FORBIDDEN'
    }
    $null=Request '/api/work-orders?view=managed' $f.test_no_grants 403 'ACTION_FORBIDDEN'
    foreach($query in @('','?view=','?view=all','?view=reported&page=0','?view=reported&page=-1','?view=reported&page=1.0',
        '?view=reported&page_size=0','?view=reported&page_size=101','?view=reported&page=9223372036854775807',
        '?view=reported&page_size=abc','?view=reported&category_id=0','?view=reported&category_id=1%20OR%201=1',
        '?view=reported&status=INVALID','?view=reported&role=sysadmin','?view=reported&reporter_id=4')) {
        $null=Request ('/api/work-orders'+$query) $r 422 'VALIDATION_ERROR'
    }
    $seen=@()
    for($page=1;$page -le 3;$page++) {
        $p=Request "/api/work-orders?view=reported&page=$page&page_size=1" $r
        Assert-True ($p.data.total -eq 3 -and @($p.data.items).Count -eq 1) 'Pagination count mismatch'
        $seen+=@($p.data.items)[0].id
    }
    Assert-True (($seen -join ',') -eq "$($f.draft),$($f.current),1") 'Unstable pagination or overlap'
    $past=Request '/api/work-orders?view=reported&page=4&page_size=1' $r
    Assert-True ($past.data.total -eq 3 -and @($past.data.items).Count -eq 0) 'Last page boundary'
    Exact-Ids (Request '/api/work-orders?view=reported&page_size=100' $r) @(1,$f.current,$f.draft)
    Exact-Ids (Request '/api/work-orders?view=reported&status=DRAFT&category_id=2' $r) @($f.draft)
    Exact-Ids (Request '/api/work-orders?view=reported&category_id=1' $r) @(1,$f.current)
    Exact-Ids (Request '/api/work-orders?view=reported&status=DRAFT&category_id=1' $r) @()
    $own=Request "/api/work-orders/$($f.current)" $r;WhiteList $own.data $true $true
    Write-Host ('ACTUAL API-10: '+($own|ConvertTo-Json -Depth 8 -Compress))
    WhiteList (Request '/api/work-orders/4' $w).data $true $true
    WhiteList (Request "/api/work-orders/$($f.current)" $w).data $true $true
    WhiteList (Request "/api/work-orders/$($f.history)" $w).data $true $false
    WhiteList (Request "/api/work-orders/$($f.history)" $w2).data $true $true
    WhiteList (Request "/api/work-orders/$($f.current)" $d).data $true $true
    WhiteList (Request '/api/work-orders/2' $a).data $true $true
    foreach($pair in @(@($r,2),@($w,$f.foreign),@($d,2),@($a,$f.current),@($f.test_no_grants,1))) {
        $null=Request "/api/work-orders/$($pair[1])" $pair[0] 404 'RESOURCE_NOT_FOUND'
    }
    foreach($id in @('0','-1','abc','1.5','9223372036854775808')) {$null=Request "/api/work-orders/$id" $r 422 'VALIDATION_ERROR'}
    $null=Request '/api/work-orders/999999' $r 404 'RESOURCE_NOT_FOUND'
    # Client role/scope hints cannot grant detail access.
    $null=Request '/api/work-orders/2?role=sysadmin&scope_location_id=2' $r 404 'RESOURCE_NOT_FOUND'
    Mode 'admin-building';Exact-Ids (Request '/api/work-orders?view=managed' $a) @(1,$f.current,$f.history,$f.draft)
    WhiteList (Request "/api/work-orders/$($f.current)" $a).data $true $true
    Mode 'admin-room';Exact-Ids (Request '/api/work-orders?view=managed' $a) @(1,$f.current,$f.history,$f.draft)
    $null=Request '/api/work-orders/3' $a 404 'RESOURCE_NOT_FOUND'
    Mode 'admin-global';Assert-True ((Request '/api/work-orders?view=managed' $a).data.total -eq 8) 'Global read grant'
    Mode 'admin-read-only';Exact-Ids (Request '/api/work-orders?view=managed' $a) @(2,4,$f.foreign)
    $readOnlyAdmin=Request '/api/work-orders/2' $a;WhiteList $readOnlyAdmin.data $true $true
    Assert-True ($readOnlyAdmin.data.PSObject.Properties.Name -notcontains 'internal_note') 'Read-only admin got internal notes'
    Mode 'revoke-admin';$null=Request '/api/work-orders?view=managed' $a 403 'ACTION_FORBIDDEN'
    $null=Request '/api/work-orders/2' $a 404 'RESOURCE_NOT_FOUND'
    Mode 'admin-area-only';$null=Request '/api/work-orders?view=managed' $a 403 'ACTION_FORBIDDEN'
    Mode 'revoke-dispatcher';$null=Request '/api/work-orders?view=managed' $d 403 'ACTION_FORBIDDEN'
    $null=Request '/api/work-orders/1' $d 404 'RESOURCE_NOT_FOUND'
    Mode 'disable-reporter';$null=Request '/api/work-orders?view=reported' $r 401 'AUTH_INVALID'
    $null=Request '/api/work-orders/1' $r 401 'AUTH_INVALID'
    & $Fixture --verify $db $fixturePath;Assert-True ($LASTEXITCODE -eq 0) 'Read-only database verification failed'
    Stop-Process -Id $process.Id;$process.WaitForExit();$process=$null
    $logs=((Get-Content $stdout -Raw),(Get-Content $stderr -Raw)) -join ''
    foreach($name in @('demo_reporter01','demo_reporter02','demo_worker01','demo_worker02','demo_dispatcher','demo_sysadmin','test_no_grants')) {
        Assert-True (-not $logs.Contains([string]$f.$name)) 'Token leaked in query logs'
    }
    Write-Host "PASS $checks production read HTTP checks with recursive DTO white-list and read-only verification"
} catch {Write-Error -ErrorAction Continue $_.Exception.Message;exit 1}
finally {
    if($process -and -not $process.HasExited) {Stop-Process -Id $process.Id;$process.WaitForExit()}
    $env:CAMPUSFIX_DB_PATH=$oldDb;$env:CAMPUSFIX_PORT=$oldPort
    $resolved=[IO.Path]::GetFullPath($root);$temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if($resolved.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase) -and ([IO.Path]::GetFileName($resolved)).StartsWith('campusfix-read-http-')) {Remove-Item -LiteralPath $resolved -Recurse -Force}
}
