#Requires -Version 5.1
param([Parameter(Mandatory=$true)][string]$Server,[Parameter(Mandatory=$true)][string]$Fixture,
      [Parameter(Mandatory=$true)][string]$Migrations,[Parameter(Mandatory=$true)][string]$Seed)
$ErrorActionPreference='Stop'
$root=Join-Path ([IO.Path]::GetTempPath()) ('campusfix-timeline-http-'+[guid]::NewGuid().ToString('N'))
$process=$null;$checks=0;$oldDb=$env:CAMPUSFIX_DB_PATH;$oldPort=$env:CAMPUSFIX_PORT
function Assert-True($ok,[string]$message) {if(-not $ok) {throw $message}}
function Request([string]$path,[string]$token,[int]$expected=200,[string]$code='') {
    $headers=@{};if($token) {$headers.Authorization='Bearer '+$token}
    try {
        $res=Invoke-WebRequest -UseBasicParsing -Uri ($script:base+$path) -Headers $headers -TimeoutSec 8
        $status=[int]$res.StatusCode;$type=[string]$res.Headers['Content-Type'];$body=$res.Content|ConvertFrom-Json
        Assert-True ($res.Headers['Cache-Control'] -eq 'no-store') 'Timeline response should not be cached'
    }catch {
        if(-not $_.Exception.Response) {throw}
        $status=[int]$_.Exception.Response.StatusCode;$type=[string]$_.Exception.Response.ContentType;$body=$_.ErrorDetails.Message|ConvertFrom-Json
    }
    Assert-True ($status -eq $expected) "HTTP $path : got $status expected $expected"
    Assert-True ($type -match '^application/json' -and $body.request_id -and $body.message -and $body.PSObject.Properties.Name -contains 'data') 'Invalid JSON/envelope'
    if($code) {Assert-True ($body.code -eq $code) "Unexpected error code for $path"}
    if($status -eq 200) {Assert-True ($body.code -eq 0) 'Invalid success code'}
    $encoded=$body|ConvertTo-Json -Depth 12
    Assert-True ($encoded -notmatch 'password_hash|token_hash|stored_path|metadata_json|SECRET_HASH|SECRET_TOKEN_HASH|SECRET_PATH|SECRET_METADATA|SECRET_SCORE|SQLite|SQLITE_') 'Forbidden field/value leaked'
    $script:checks++;Write-Host "PASS HTTP GET $path => $status $code";return $body
}
function Public-Fields($body,[bool]$internal=$false) {
    $keys=@('id','action','from_status','to_status','public_note','actor','created_at')
    foreach($item in $body.data.items) {
        $allowed=$keys;if($internal) {$allowed+=@('internal_note','management_detail')}
        foreach($key in $item.PSObject.Properties.Name) {Assert-True ($allowed -contains $key) "Unexpected timeline field $key"}
        foreach($key in $keys) {Assert-True ($item.PSObject.Properties.Name -contains $key) "Missing public field $key"}
        if($null -ne $item.actor) {Assert-True ((@($item.actor.PSObject.Properties.Name|Sort-Object) -join ',') -eq 'display_name,id') 'Actor leaked nested user fields'}
    }
}
function Management-Fields($body) {
    Public-Fields $body $true
    $counts=@{review=0;assign=0;transfer=0;rework=0}
    foreach($item in $body.data.items) {
        if($item.PSObject.Properties.Name -notcontains 'management_detail') {continue}
        $keys=(@($item.management_detail.PSObject.Properties.Name|Sort-Object) -join ',')
        switch($item.action) {
            'ORDER_REVIEWED' {Assert-True ($keys -eq 'approved_urgency,category_id,review_action') 'Review metadata whitelist';$counts.review++}
            'ORDER_ASSIGNED' {Assert-True ($keys -eq 'assign_reason,assignment_id,candidate_rank,worker_id') 'Assignment metadata whitelist';$counts.assign++}
            'TRANSFER_APPROVED' {Assert-True ($keys -eq 'decision,transfer_request_id') 'Transfer metadata whitelist';$counts.transfer++}
            'ORDER_REWORK_REQUESTED' {Assert-True ($keys -eq 'rework_count') 'Rework metadata whitelist';$counts.rework++}
            default {throw 'Unexpected event exposed management fields'}
        }
    }
    Assert-True ($counts.review -eq 1 -and $counts.assign -eq 1 -and $counts.transfer -eq 1 -and $counts.rework -eq 1) 'Malformed/unknown/cross-type metadata not rejected'
}
function Mode([string]$name) {& $Fixture --grant $script:db $name;Assert-True ($LASTEXITCODE -eq 0) 'Timeline grant fixture update failed'}
try {
    New-Item -ItemType Directory -Path $root|Out-Null
    $db=Join-Path $root 'timeline.sqlite3';$fixturePath=Join-Path $root 'fixture.json'
    & $Fixture --init $db $fixturePath $Migrations $Seed;Assert-True ($LASTEXITCODE -eq 0) 'Timeline fixture failed'
    $f=Get-Content -LiteralPath $fixturePath -Raw|ConvertFrom-Json
    $r=$f.demo_reporter01;$b=$f.demo_reporter02;$w=$f.demo_worker01;$d=$f.demo_dispatcher;$a=$f.demo_sysadmin
    $listener=New-Object Net.Sockets.TcpListener([Net.IPAddress]::Loopback,0);$listener.Start();$port=$listener.LocalEndpoint.Port;$listener.Stop()
    $env:CAMPUSFIX_DB_PATH=$db;$env:CAMPUSFIX_PORT=[string]$port;$base="http://127.0.0.1:$port"
    $stdout=Join-Path $root 'stdout.log';$stderr=Join-Path $root 'stderr.log'
    $process=Start-Process -FilePath $Server -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $ready=$false
    for($i=0;$i -lt 60;$i++) {
        if($process.HasExited) {throw 'Production backend exited'}
        try {$null=Invoke-WebRequest -UseBasicParsing -Uri "$base/api/health" -TimeoutSec 1;$ready=$true;break} catch {Start-Sleep -Milliseconds 100}
    }
    Assert-True $ready 'Backend not ready';$path="/api/work-orders/$($f.current)/timeline"
    $null=Request $path '' 401 'AUTH_REQUIRED'
    $null=Request $path 'cf_live_invalid_test' 401 'AUTH_INVALID'
    $public=Request $path $r;Public-Fields $public
    Assert-True ($public.data.page -eq 1 -and $public.data.page_size -eq 20 -and $public.data.total -eq $f.total) 'Default timeline pagination'
    Assert-True (@($public.data.items).Count -eq @($f.expected_public).Count) 'SQLite vs HTTP event count'
    for($i=0;$i -lt @($f.expected_public).Count;$i++) {
        $got=@($public.data.items)[$i];$want=@($f.expected_public)[$i]
        foreach($key in @('id','action','from_status','to_status','public_note','created_at')) {Assert-True ($got.$key -eq $want.$key) "SQLite vs HTTP mismatch: $key"}
        Assert-True (($null -eq $got.actor) -eq ($null -eq $want.actor)) 'System actor mismatch'
        if($null -ne $want.actor) {Assert-True ($got.actor.id -eq $want.actor.id -and $got.actor.display_name -eq $want.actor.display_name) 'SQLite vs HTTP actor mismatch'}
    }
    $sample=Request '/api/work-orders/1/timeline' $r;Public-Fields $sample
    Write-Host ('ACTUAL API-11 public: '+($sample|ConvertTo-Json -Depth 8 -Compress))
    $again=Request '/api/work-orders/1/timeline' $r;Assert-True ($again.request_id -ne $sample.request_id) 'GET request_id must be generated per request'
    Public-Fields (Request '/api/work-orders/4/timeline' $w)
    Public-Fields (Request $path $w)
    $history=Request "/api/work-orders/$($f.history)/timeline" $w
    Public-Fields $history;Assert-True ($history.data.total -eq 1 -and @($history.data.items).Count -eq 1) 'Historical public timeline'
    $empty=Request "/api/work-orders/$($f.empty)/timeline" $r
    Assert-True ($empty.data.total -eq 0 -and @($empty.data.items).Count -eq 0) 'Authorized empty timeline'
    foreach($pair in @(@($b,$f.current),@($w,$f.foreign),@($d,$f.foreign),@($f.test_no_grants,$f.current),@($a,$f.current))) {
        $null=Request "/api/work-orders/$($pair[1])/timeline" $pair[0] 404 'RESOURCE_NOT_FOUND'
    }
    Management-Fields (Request $path $d)
    $initialAdmin=Request "/api/work-orders/$($f.foreign)/timeline" $a
    Assert-True (@($initialAdmin.data.items)[0].internal_note -eq 'Fixture internal note') 'Matched B grants did not expose permitted note'
    foreach($id in @('0','-1','abc','1.5','9223372036854775808')) {$null=Request "/api/work-orders/$id/timeline" $r 422 'VALIDATION_ERROR'}
    $null=Request '/api/work-orders/999999/timeline' $r 404 'RESOURCE_NOT_FOUND'
    foreach($suffix in @('?page=0','?page=-1','?page=1.5','?page=','?page_size=0','?page_size=101','?page_size=abc',
        '?page=9223372036854775807&page_size=100','?role=sysadmin','?order_id=2')) {$null=Request ($path+$suffix) $r 422 'VALIDATION_ERROR'}
    foreach($size in @(50,100)) {
        $page=Request ($path+"?page_size=$size") $r
        Assert-True ($page.data.page_size -eq $size -and $page.data.total -eq $f.total -and @($page.data.items).Count -eq $f.total) 'Explicit timeline page size boundary'
    }
    $ids=@()
    for($page=1;$page -le $f.total;$page++) {
        $p=Request ($path+"?page=$page&page_size=1") $r
        Assert-True ($p.data.total -eq $f.total -and @($p.data.items).Count -eq 1) 'Paged total mismatch'
        $ids+=@($p.data.items)[0].id
    }
    Assert-True (($ids -join ',') -eq (($f.expected_public|ForEach-Object {$_.id}) -join ',')) 'Timeline unstable sort, overlap or missing events'
    $past=Request ($path+"?page=$($f.total+1)&page_size=1") $r
    Assert-True ($past.data.total -eq $f.total -and @($past.data.items).Count -eq 0) 'Timeline last page boundary'
    # API-10 and API-11 must be distinct real Drogon routes.
    $detail=Request "/api/work-orders/$($f.current)" $r
    Assert-True ($detail.data.id -eq $f.current -and $detail.data.PSObject.Properties.Name -notcontains 'items') 'Detail route swallowed timeline route'
    $null=Request '/api/work-orders?view=reported' $r
    Mode 'admin-mismatch';Public-Fields (Request $path $a)
    Mode 'admin-match';$manager=Request ($path+'?page_size=1&page=4') $a
    Assert-True (@($manager.data.items)[0].internal_note -eq 'Fixture internal note') 'Matched nested grants must expose note'
    Write-Host ('ACTUAL API-11 management: '+($manager|ConvertTo-Json -Depth 8 -Compress))
    Management-Fields (Request $path $a)
    Mode 'revoke-internal';Public-Fields (Request $path $a)
    Mode 'revoke-business';$null=Request $path $a 404 'RESOURCE_NOT_FOUND'
    Mode 'revoke-dispatcher';$null=Request $path $d 404 'RESOURCE_NOT_FOUND'
    & $Fixture --verify $db $fixturePath;Assert-True ($LASTEXITCODE -eq 0) 'Timeline read-only verification failed'
    Stop-Process -Id $process.Id;$process.WaitForExit();$process=$null
    $logs=((Get-Content $stdout -Raw),(Get-Content $stderr -Raw)) -join ''
    foreach($name in @('demo_reporter01','demo_reporter02','demo_worker01','demo_worker02','demo_dispatcher','demo_sysadmin','test_no_grants')) {
        Assert-True (-not $logs.Contains([string]$f.$name)) 'Token leaked in timeline logs'
    }
    Write-Host "PASS $checks production timeline HTTP checks; SQL field comparison, management whitelist and read-only verification passed"
}catch {Write-Error -ErrorAction Continue $_.Exception.Message;exit 1}
finally {
    if($process -and -not $process.HasExited) {Stop-Process -Id $process.Id;$process.WaitForExit()}
    $env:CAMPUSFIX_DB_PATH=$oldDb;$env:CAMPUSFIX_PORT=$oldPort
    $resolved=[IO.Path]::GetFullPath($root);$temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if($resolved.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase) -and ([IO.Path]::GetFileName($resolved)).StartsWith('campusfix-timeline-http-')) {Remove-Item -LiteralPath $resolved -Recurse -Force}
}
