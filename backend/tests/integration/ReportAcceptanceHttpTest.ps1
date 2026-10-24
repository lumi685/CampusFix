#Requires -Version 5.1
param([Parameter(Mandatory=$true)][string]$Server,[Parameter(Mandatory=$true)][string]$Fixture,
      [Parameter(Mandatory=$true)][string]$Migrations,[Parameter(Mandatory=$true)][string]$Seed,
      [Parameter(Mandatory=$true)][string]$EvidencePath)
$ErrorActionPreference='Stop'
$root=Join-Path ([IO.Path]::GetTempPath()) ('campusfix-report-acceptance-'+[guid]::NewGuid().ToString('N'))
$process=$null;$oldDb=$env:CAMPUSFIX_DB_PATH;$oldPort=$env:CAMPUSFIX_PORT
$checks=0;$assertions=0;$generation=0;$snapshots=0
$evidence=@{kind='production HTTP and independent SQLite, NOT browser E2E';started_at=[DateTime]::UtcNow.ToString('o');requests=@();orders=@();result='running'}
$ids=New-Object 'System.Collections.Generic.HashSet[string]'
$secrets=New-Object 'System.Collections.Generic.List[string]'
function Assert($ok,[string]$message) {$script:assertions++;if(-not $ok){throw $message}}
function Request([string]$method,[string]$path,[string]$token,$body=$null,[int]$expected=200,[string]$code='') {
    $headers=@{};if($token){$headers.Authorization='Bearer '+$token}
    $args=@{UseBasicParsing=$true;Uri=$script:base+$path;Method=$method;Headers=$headers;TimeoutSec=10}
    if($null -ne $body){$args.ContentType='application/json';$args.Body=[Text.Encoding]::UTF8.GetBytes(($body|ConvertTo-Json -Depth 8 -Compress))}
    try {$res=Invoke-WebRequest @args;$status=[int]$res.StatusCode;$json=$res.Content|ConvertFrom-Json;$type=$res.Headers['Content-Type']}
    catch {if(-not $_.Exception.Response){throw 'HTTP transport failed'};$status=[int]$_.Exception.Response.StatusCode;$json=$_.ErrorDetails.Message|ConvertFrom-Json;$type=$_.Exception.Response.ContentType}
    Assert ($status -eq $expected) "HTTP $method $path got $status expected $expected"
    Assert ($type -match '^application/json' -and $json.message -and $json.PSObject.Properties.Name -contains 'data') 'Invalid JSON envelope'
    Assert ($json.request_id -match '^req_[\w-]+$' -and $ids.Add([string]$json.request_id)) 'Missing/repeated request_id'
    if($expected -lt 400){Assert ($json.code -eq 0) 'Invalid success code'}
    if($code){Assert ($json.code -eq $code) 'Wrong error code'}
    $serialized=$json|ConvertTo-Json -Depth 16 -Compress
    Assert ($serialized -notmatch 'password_hash|token_hash|stored_path|metadata_json|SQLITE_|DO_NOT_LEAK_') 'Sensitive fields/error leaked'
    $safe=$serialized|ConvertFrom-Json
    if($path -eq '/api/auth/login' -and $status -eq 200){$safe.data.PSObject.Properties.Remove('token')}
    $script:evidence.requests+=@{method=$method;path=$path;status=$status;response=$safe}
    $script:checks++;Write-Host "PASS HTTP $method $path => $status"
    return $json
}
function Login([string]$name) {
    $response=Request 'POST' '/api/auth/login' '' @{username=$name;password=$script:data.accounts.$name.password}
    Assert ($response.data.token -and $response.data.user.id -eq $script:data.accounts.$name.id) 'Login identity mismatch'
    $script:secrets.Add([string]$response.data.token)
    $me=Request 'GET' '/api/auth/me' $response.data.token
    Assert ($me.data.id -eq $script:data.accounts.$name.id -and $me.data.role -eq $script:data.accounts.$name.role) 'Me identity mismatch'
    return [string]$response.data.token
}
function Snapshot {
    $script:snapshots++;$file=Join-Path $root ('snapshot-'+$snapshots+'.json')
    & $Fixture --snapshot $db $file
    Assert ($LASTEXITCODE -eq 0) 'Independent SQLite query failed'
    $value=Get-Content -LiteralPath $file -Raw|ConvertFrom-Json
    Assert ($value.integrity -eq 'ok' -and $value.foreign_key_violations -eq 0) 'SQLite integrity/FK failure'
    return $value
}
function Fault([string]$mode) {& $Fixture --fault $db $mode;Assert ($LASTEXITCODE -eq 0) 'Fault fixture failed'}
function StartServer {
    $script:generation++
    $script:process=Start-Process -FilePath $Server -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $root "server-$generation.log") -RedirectStandardError (Join-Path $root "server-$generation.err")
    $ready=$false
    for($i=0;$i -lt 60;$i++) {
        if($process.HasExited){throw 'Production server exited'}
        try {Invoke-WebRequest -UseBasicParsing "$base/api/health" -TimeoutSec 1|Out-Null;$ready=$true;break}catch{Start-Sleep -Milliseconds 100}
    }
    Assert $ready 'Production server not ready'
}
function StopServer {if($script:process -and -not $script:process.HasExited){Stop-Process -Id $script:process.Id;$script:process.WaitForExit()};$script:process=$null}
function DatabaseOrder($receipt,$payload,[long]$actor,[bool]$submitted) {
    $s=Snapshot;$rows=@($s.orders|Where-Object {$_.id -eq $receipt.data.id})
    Assert ($rows.Count -eq 1) 'Expected exactly one SQLite row'
    $row=$rows[0];$logs=@($s.logs|Where-Object {$_.order_id -eq $row.id})
    foreach($key in $payload.Keys){Assert ($row.$key -eq $payload[$key]) "SQLite field differs: $key"}
    Assert ($row.reporter_id -eq $actor -and $row.version -eq $(if($submitted){2}else{1})) 'SQLite owner/version mismatch'
    Assert ($row.status -eq $(if($submitted){'PENDING_REVIEW'}else{'DRAFT'})) 'SQLite status mismatch'
    Assert ($logs.Count -eq $(if($submitted){2}else{1})) 'Unexpected event count'
    Assert ($logs[0].action -eq 'ORDER_CREATED' -and $null -eq $logs[0].from_status -and $logs[0].to_status -eq 'DRAFT' -and $logs[0].created_at -eq $row.created_at) 'Creation event mismatch'
    foreach($event in $logs){Assert ($event.actor_id -eq $actor) 'Event actor differs from authenticated user'}
    if($submitted) {
        Assert ($row.order_no -match '^WX\d{12}$' -and $row.order_no -eq $receipt.data.order_no -and $row.submitted_at -match '^\d{4}-\d\d-\d\dT.*Z$') 'Number/UTC mismatch'
        Assert ($logs[1].action -eq 'ORDER_SUBMITTED' -and $logs[1].from_status -eq 'DRAFT' -and $logs[1].to_status -eq 'PENDING_REVIEW' -and $logs[1].created_at -eq $row.submitted_at -and $logs[1].request_id -eq $receipt.request_id) 'Submit event mismatch'
    }else {Assert ($null -eq $row.order_no -and $null -eq $row.submitted_at -and $logs[0].request_id -eq $receipt.request_id) 'Draft allocated a number or wrong request correlation'}
    Write-Host ('PASS SQLite '+($row|Select-Object id,reporter_id,status,version,order_no,created_at,submitted_at|ConvertTo-Json -Compress))
    return @{row=$row;logs=$logs;snapshot=$s}
}
try {
    New-Item -ItemType Directory -Path (Split-Path -Parent $EvidencePath) -Force|Out-Null
    $evidence|ConvertTo-Json -Depth 20|Set-Content -LiteralPath $EvidencePath -Encoding UTF8
    New-Item -ItemType Directory -Path $root|Out-Null
    $db=Join-Path $root 'acceptance.sqlite3';$credentials=Join-Path $root 'credentials.json'
    & $Fixture --init $db $credentials $Migrations $Seed
    Assert ($LASTEXITCODE -eq 0) 'Acceptance fixture failed'
    $data=Get-Content -LiteralPath $credentials -Raw|ConvertFrom-Json
    foreach($account in $data.accounts.PSObject.Properties){$secrets.Add([string]$account.Value.password)}
    $listener=New-Object Net.Sockets.TcpListener([Net.IPAddress]::Loopback,0);$listener.Start();$port=$listener.LocalEndpoint.Port;$listener.Stop()
    $env:CAMPUSFIX_DB_PATH=$db;$env:CAMPUSFIX_PORT=[string]$port;$base="http://127.0.0.1:$port";StartServer
    $tokens=@{}
    foreach($name in @('demo_reporter01','demo_reporter02','demo_reporter03','demo_worker01','demo_dispatcher','demo_sysadmin')){$tokens[$name]=Login $name}
    $reporter=$tokens.demo_reporter01;$worker=$tokens.demo_worker01
    $empty=Request 'GET' '/api/work-orders?view=reported&status=CLOSED' $tokens.demo_reporter03
    Assert ($empty.data.total -eq 0 -and @($empty.data.items).Count -eq 0) 'Expected empty filtered reporter list'
    $roots=Request 'GET' '/api/locations' $reporter
    Assert (@($roots.data.items).Count -eq 2) 'Unexpected root locations'
    $node=$roots.data.items[0];$selected=$null;$depth=0
    while($node) {
        $children=Request 'GET' ("/api/locations?parent_id="+$node.id) $reporter
        foreach($child in $children.data.items){Assert ($child.parent_id -eq $node.id) 'Not a direct child'}
        $depth++;$selected=$node
        if(@($children.data.items).Count -eq 0){break};$node=$children.data.items[0]
    }
    Assert ($depth -eq 4 -and $selected.location_type -eq 'ROOM') 'Did not traverse campus/building/floor/room'
    $catalog=Request 'GET' '/api/fault-categories' $reporter
    Assert (@($catalog.data.items).Count -ge 3) 'Missing active categories'
    $category=$catalog.data.items[0]
    $leaf=Request 'GET' ("/api/fault-categories?parent_id="+$category.id) $reporter
    Assert (@($leaf.data.items).Count -eq 0) 'Expected selectable root leaf category'
    Assert (-not (@($catalog.data.items.id) -contains $data.inactive_category) -and -not (@($roots.data.items.id) -contains $data.inactive_location)) 'Inactive catalog exposed'
    $payload=@{category_id=$category.id;location_id=$selected.id;title='W4-D5 fictional reporter repair';description='Fictional acceptance description';requested_urgency='NORMAL';contact_name='Fictional contact';contact_phone='00000000000';expected_visit_at='2030-01-01T00:30:00.000Z'}
    $records=@()
    foreach($name in @('demo_reporter01','demo_worker01')) {
        $token=$tokens[$name];$body=$payload.Clone();$body.title="W4-D5 fictional $name repair"
        $created=Request 'POST' '/api/work-orders' $token $body 201
        Assert ($created.data.status -eq 'DRAFT' -and $created.data.version -eq 1 -and $null -eq $created.data.order_no) 'Invalid create response'
        $initial=DatabaseOrder $created $body $data.accounts.$name.id $false
        $id=$created.data.id
        $null=Request 'POST' "/api/work-orders/$id/submit" $token @{expected_version=99} 409 'ORDER_VERSION_CONFLICT'
        Assert ((Snapshot).digest -eq $initial.snapshot.digest) 'Version conflict changed DB'
        $submitted=Request 'POST' "/api/work-orders/$id/submit" $token @{expected_version=1}
        $actual=DatabaseOrder $submitted $body $data.accounts.$name.id $true
        Assert ($actual.logs[0].request_id -eq $created.request_id) 'Creation request correlation lost'
        $before=Snapshot
        $page=Request 'GET' '/api/work-orders?view=reported&page_size=1&page=1' $token
        $expectedTotal=@($actual.snapshot.orders|Where-Object {$_.reporter_id -eq $data.accounts.$name.id}).Count
        Assert (@($page.data.items).Count -eq 1 -and $page.data.total -eq $expectedTotal -and $page.data.items[0].id -eq $id) 'Pagination/newest order/total mismatch'
        foreach($key in @('id','order_no','title','status','version','created_at','submitted_at')){Assert ($page.data.items[0].$key -eq $actual.row.$key) "List vs SQLite mismatch: $key"}
        Assert ($page.data.items[0].category.id -eq $category.id -and $page.data.items[0].location.id -eq $selected.id) 'List reference mismatch'
        $second=Request 'GET' '/api/work-orders?view=reported&page_size=1&page=2' $token
        Assert ($second.data.total -eq $expectedTotal -and @($second.data.items).Count -eq 1 -and $second.data.items[0].id -ne $id) 'Second page mismatch'
        $filtered=Request 'GET' ("/api/work-orders?view=reported&status=PENDING_REVIEW&category_id="+$category.id) $token
        Assert (@($filtered.data.items.id) -contains $id) 'Combined filters lost new order'
        $expectedFiltered=@($actual.snapshot.orders|Where-Object {$_.reporter_id -eq $data.accounts.$name.id -and $_.status -eq 'PENDING_REVIEW' -and $_.category_id -eq $category.id}).Count
        Assert ($filtered.data.total -eq $expectedFiltered) 'Filtered total differs from SQLite'
        foreach($entry in $filtered.data.items) {
            Assert ($entry.status -eq 'PENDING_REVIEW' -and $entry.category.id -eq $category.id) 'Filter mismatch'
            Assert ($entry.PSObject.Properties.Name -notcontains 'description' -and $entry.PSObject.Properties.Name -notcontains 'contact_phone' -and $entry.PSObject.Properties.Name -notcontains 'contact_name') 'List exposed detail fields'
        }
        $detail=Request 'GET' "/api/work-orders/$id" $token
        $timeline=Request 'GET' "/api/work-orders/$id/timeline" $token
        foreach($key in @('id','order_no','status','version','title','created_at','submitted_at','description','expected_visit_at')){Assert ($detail.data.$key -eq $actual.row.$key) "HTTP detail vs SQLite mismatch: $key"}
        Assert ($detail.data.category.id -eq $category.id -and $detail.data.location.id -eq $selected.id) 'HTTP reference mismatch'
        Assert ($timeline.data.total -eq 2 -and (@($timeline.data.items.action) -join ',') -eq 'ORDER_CREATED,ORDER_SUBMITTED') 'Timeline events mismatch'
        for($i=0;$i -lt 2;$i++) {
            $event=$timeline.data.items[$i];$stored=$actual.logs[$i]
            foreach($key in @('id','action','from_status','to_status','created_at')){Assert ($event.$key -eq $stored.$key) "Timeline SQL mismatch: $key"}
            Assert ($event.actor.id -eq $actual.row.reporter_id -and $event.PSObject.Properties.Name -notcontains 'internal_note') 'Timeline actor/internal whitelist'
        }
        Assert ((Snapshot).digest -eq $before.digest) 'GET changed whole database'
        $null=Request 'POST' "/api/work-orders/$id/submit" $token @{expected_version=2} 409 'ORDER_STATUS_CONFLICT'
        Assert ((Snapshot).digest -eq $before.digest) 'Repeat submit changed DB'
        $records+=@{name=$name;id=$id;body=$body;row=$actual.row;logs=$actual.logs;create_request_id=$created.request_id;submit_request_id=$submitted.request_id}
        $evidence.orders+=@{name=$name;row=$actual.row;logs=$actual.logs}
    }
    Assert ($records[0].row.order_no -ne $records[1].row.order_no) 'Duplicate order numbers'
    $beforeInvalid=Snapshot
    foreach($path in @('/api/work-orders?view=reported',"/api/work-orders/$($records[0].id)","/api/work-orders/$($records[0].id)/timeline")){$null=Request 'GET' $path '' $null 401}
    foreach($name in @('demo_dispatcher','demo_sysadmin')){$null=Request 'POST' '/api/work-orders' $tokens[$name] $payload 403 'ACTION_FORBIDDEN'}
    $null=Request 'GET' "/api/work-orders/$($records[0].id)" $tokens.demo_reporter02 $null 404
    $null=Request 'GET' "/api/work-orders/$($records[0].id)/timeline" $tokens.demo_reporter02 $null 404
    # A current assignee can see the task, but cannot act as its reporter: visible resource => 403.
    $null=Request 'POST' "/api/work-orders/$($data.current_assignment_order)/submit" $worker @{expected_version=1} 403 'ACTION_FORBIDDEN'
    $assigned=Request 'GET' '/api/work-orders?view=assigned' $worker
    Assert (-not (@($assigned.data.items.id) -contains $records[1].id)) 'Worker reported order mixed into assigned view'
    $managed=Request 'GET' '/api/work-orders?view=managed' $tokens.demo_dispatcher
    Assert (-not (@($managed.data.items.id) -contains $data.other_campus_order)) 'Dispatcher crossed campus scope'
    foreach($field in @('title','description','category_id','location_id','contact_name','contact_phone','requested_urgency')) {
        $bad=$payload.Clone();$bad.Remove($field);$null=Request 'POST' '/api/work-orders' $reporter $bad 422
    }
    foreach($pair in @(@('category_id',$data.inactive_category),@('location_id',$data.inactive_location),@('category_id',999999),@('location_id',999999),@('requested_urgency','INVALID'),@('title',('x'*121)),@('description',('x'*2001)),@('contact_name',('x'*81)),@('contact_phone',('x'*33)),@('reporter_id',999))) {
        $bad=$payload.Clone();$bad[$pair[0]]=$pair[1];$null=Request 'POST' '/api/work-orders' $reporter $bad 422
    }
    foreach($version in @(0,-1,'1',$true)){$null=Request 'POST' "/api/work-orders/$($records[0].id)/submit" $reporter @{expected_version=$version} 422}
    Assert ((Snapshot).digest -eq $beforeInvalid.digest) 'Invalid/unauthorized requests changed DB'
    Fault 'create-log';$before=Snapshot
    $null=Request 'POST' '/api/work-orders' $reporter $payload 500 'INTERNAL_ERROR'
    Assert ((Snapshot).digest -eq $before.digest) 'Creation log failure did not roll back'
    Fault 'off'
    $retry=Request 'POST' '/api/work-orders' $reporter $payload 201
    Fault 'submit-log';$before=Snapshot
    $null=Request 'POST' "/api/work-orders/$($retry.data.id)/submit" $reporter @{expected_version=1} 500 'INTERNAL_ERROR'
    Assert ((Snapshot).digest -eq $before.digest) 'Submit/log/sequence failure did not roll back'
    Fault 'off'
    $retried=Request 'POST' "/api/work-orders/$($retry.data.id)/submit" $reporter @{expected_version=1}
    $retryEvidence=DatabaseOrder $retried $payload $data.accounts.demo_reporter01.id $true
    Assert ($retryEvidence.logs[0].request_id -eq $retry.request_id -and @($retryEvidence.snapshot.orders).Count -eq ($data.baseline_orders+3) -and $retryEvidence.snapshot.sequence_sum -eq 3) 'Retry created duplicate draft/number/event'
    $evidence.orders+=@{name='same-draft-fault-retry';row=$retryEvidence.row;logs=$retryEvidence.logs}
    $null=Request 'POST' "/api/work-orders/$($retry.data.id)/submit" $reporter @{expected_version=2} 409
    foreach($mode in @('admin-global','revoke-admin')) {
        & $Fixture --grant $db $mode;Assert ($LASTEXITCODE -eq 0) 'Grant fixture failed'
        if($mode -eq 'admin-global') {
            $public=Request 'GET' "/api/work-orders/$($data.current_assignment_order)/timeline" $tokens.demo_sysadmin
            foreach($event in $public.data.items){Assert ($event.PSObject.Properties.Name -notcontains 'internal_note') 'INTERNAL_NOTE_READ scope mismatch leaked data'}
        }else {
            $null=Request 'GET' "/api/work-orders/$($data.other_campus_order)" $tokens.demo_sysadmin $null 404
            $null=Request 'GET' '/api/work-orders?view=managed' $tokens.demo_sysadmin $null 403
        }
    }
    foreach($name in @('demo_reporter01','demo_worker01')) {
        $null=Request 'POST' '/api/auth/logout' $tokens[$name]
        $null=Request 'GET' '/api/auth/me' $tokens[$name] $null 401
    }
    StopServer;StartServer
    foreach($record in $records) {
        $token=Login $record.name
        $list=Request 'GET' '/api/work-orders?view=reported' $token
        Assert (@($list.data.items.id) -contains $record.id) 'Order lost after restart/relogin'
        $detail=Request 'GET' "/api/work-orders/$($record.id)" $token
        $timeline=Request 'GET' "/api/work-orders/$($record.id)/timeline" $token
        Assert ($detail.data.order_no -eq $record.row.order_no -and $detail.data.version -eq 2 -and $detail.data.status -eq 'PENDING_REVIEW' -and $timeline.data.total -eq 2) 'Restart lost order state/history'
        for($i=0;$i -lt 2;$i++){Assert ($timeline.data.items[$i].id -eq $record.logs[$i].id -and $timeline.data.items[$i].created_at -eq $record.logs[$i].created_at) 'Restart altered event history'}
    }
    $final=Snapshot;Assert (@($final.orders).Count -eq ($data.baseline_orders+3) -and $final.sequence_sum -eq 3) 'Final write counts differ'
    StopServer
    $logs=(@(Get-ChildItem -LiteralPath $root -File|Where-Object {$_.Extension -in @('.log','.err')}|ForEach-Object {Get-Content -LiteralPath $_.FullName -Raw})) -join ''
    foreach($secret in $secrets){Assert (-not $logs.Contains($secret)) 'Credential leaked to production log'}
    $evidence.result='passed';$evidence.http_requests=$checks;$evidence.assertions=$assertions;$evidence.sqlite_snapshots=$snapshots
    $evidence.integrity_check=$final.integrity;$evidence.foreign_key_violations=$final.foreign_key_violations;$evidence.completed_at=[DateTime]::UtcNow.ToString('o')
    $encoded=$evidence|ConvertTo-Json -Depth 20
    foreach($secret in $secrets){Assert (-not $encoded.Contains($secret)) 'Credential in evidence artifact'}
    New-Item -ItemType Directory -Path (Split-Path -Parent $EvidencePath) -Force|Out-Null
    $encoded|Set-Content -LiteralPath $EvidencePath -Encoding UTF8
    Write-Host "PASS W4-D5 $checks real HTTP requests; $snapshots independent SQLite snapshots; restart, logout, fault rollback, same-draft retry verified"
    Write-Host "Sanitized evidence: $EvidencePath (no credentials; not browser evidence)"
}catch{
    $evidence.result='failed';$evidence.http_requests=$checks
    $evidence|ConvertTo-Json -Depth 20|Set-Content -LiteralPath $EvidencePath -Encoding UTF8
    Write-Error -ErrorAction Continue $_.Exception.Message;exit 1
}
finally {
    StopServer;$env:CAMPUSFIX_DB_PATH=$oldDb;$env:CAMPUSFIX_PORT=$oldPort
    $resolved=[IO.Path]::GetFullPath($root);$temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if($resolved.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase) -and ([IO.Path]::GetFileName($resolved)).StartsWith('campusfix-report-acceptance-')){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
