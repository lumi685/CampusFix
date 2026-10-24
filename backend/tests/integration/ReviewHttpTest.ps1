#Requires -Version 5.1
param([string]$Server,[string]$Fixture,[string]$Migrations,[string]$Seed,[string]$EvidencePath)
$ErrorActionPreference='Stop'
$root=Join-Path ([IO.Path]::GetTempPath()) ('campusfix-review-http-'+[guid]::NewGuid().ToString('N'))
$process=$null;$oldDb=$env:CAMPUSFIX_DB_PATH;$oldPort=$env:CAMPUSFIX_PORT;$checks=0;$receipts=@()
function Assert($ok,$message) {if(-not $ok){throw $message}}
function FixtureCall($operation,$argument) {
    $result=if($null -eq $argument){& $Fixture $operation $script:db}else{& $Fixture $operation $script:db $argument}
    Assert ($LASTEXITCODE -eq 0) "Fixture $operation failed";return $result
}
function Snapshot {return (FixtureCall '--snapshot' $null)}
function Mode($name) {$null=FixtureCall '--mode' $name}
function Request($path,$token,$expected,$payload=$null,$method='POST') {
    $headers=@{};if($token){$headers.Authorization='Bearer '+$token}
    $args=@{UseBasicParsing=$true;Uri=$script:base+$path;Headers=$headers;TimeoutSec=10;Method=$method}
    if($null -ne $payload) {$args.ContentType='application/json';$args.Body=[Text.Encoding]::UTF8.GetBytes($payload)}
    try {$res=Invoke-WebRequest @args;$status=[int]$res.StatusCode;$body=$res.Content|ConvertFrom-Json;$type=$res.Headers['Content-Type'];$cache=$res.Headers['Cache-Control']}
    catch {if(-not $_.Exception.Response){throw};$res=$_.Exception.Response;$status=[int]$res.StatusCode;$body=$_.ErrorDetails.Message|ConvertFrom-Json;$type=$res.ContentType;$cache=$res.Headers['Cache-Control']}
    Assert ($status -eq $expected) "HTTP $method $path => $status expected $expected code=$($body.code)"
    Assert ($type -match '^application/json' -and $body.request_id -and $body.message) 'Invalid envelope'
    Assert ($cache -eq 'no-store') 'Missing no-store'
    if($status -eq 200 -or $status -eq 201){Assert ($body.code -eq 0) 'Invalid success code'}
    $wire=$body|ConvertTo-Json -Depth 12 -Compress
    # A validation response may name a rejected field; that is not a serialized secret column.
    Assert ($wire -notmatch '"(?:password_hash|token_hash|stored_path|metadata_json)"\s*:|SELECT |UPDATE |fixture failure') 'Sensitive response'
    $script:checks++;Write-Host "PASS production HTTP $method $path => $status $($body.code)"
    return $body
}
function Review($id,$token,$payload,$expected=200) {
    $before=Snapshot
    $r=Request "/api/work-orders/$id/review" $token $expected $payload
    if($expected -ne 200){Assert ((Snapshot) -eq $before) 'Failure mutated business data/logs'}
    return $r
}
function Payload($action='APPROVE',$version=2) {
    if($action -eq 'APPROVE'){return (@{action=$action;expected_version=$version;category_id=2;approved_urgency='HIGH';internal_note='REVIEW_PRIVATE_MARKER'}|ConvertTo-Json -Compress)}
    return (@{action=$action;expected_version=$version;reason='Fictional public reason';internal_note='REVIEW_PRIVATE_MARKER'}|ConvertTo-Json -Compress)
}
function NewPending {
    $payload=@{category_id=1;location_id=11;title='Fictional note boundary test';description='Temporary HTTP test only';requested_urgency='NORMAL';contact_name='Fictional contact';contact_phone='00000000000'}|ConvertTo-Json -Compress
    $created=Request '/api/work-orders' $script:reporter 201 $payload
    $submitted=Request "/api/work-orders/$($created.data.id)/submit" $script:reporter 200 (@{expected_version=$created.data.version}|ConvertTo-Json -Compress)
    Assert ($submitted.data.status -eq 'PENDING_REVIEW' -and $submitted.data.version -eq 2) 'Boundary fixture submit failed'
    return $created.data.id
}
try {
    New-Item -ItemType Directory -Path $root|Out-Null
    $db=Join-Path $root 'review.sqlite3';$credentials=Join-Path $root 'credentials.json'
    & $Fixture --init $db $credentials $Migrations $Seed;Assert ($LASTEXITCODE -eq 0) 'Init failed'
    $data=Get-Content -LiteralPath $credentials -Raw|ConvertFrom-Json
    $listener=New-Object Net.Sockets.TcpListener([Net.IPAddress]::Loopback,0);$listener.Start();$port=$listener.LocalEndpoint.Port;$listener.Stop()
    $env:CAMPUSFIX_DB_PATH=$db;$env:CAMPUSFIX_PORT=[string]$port;$base="http://127.0.0.1:$port"
    $process=Start-Process -FilePath $Server -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $root 'stdout.log') -RedirectStandardError (Join-Path $root 'stderr.log')
    $ready=$false
    for($i=0;$i -lt 60;$i++){try{Invoke-WebRequest -UseBasicParsing "$base/api/health" -TimeoutSec 1|Out-Null;$ready=$true;break}catch{Start-Sleep -Milliseconds 100}}
    Assert $ready 'Server not ready'
    $dispatcher=$data.demo_dispatcher;$admin=$data.demo_sysadmin;$reporter=$data.demo_reporter01;$worker=$data.demo_worker01
    $valid=Payload
    foreach($token in @('','invalid-session',$data.expired,$data.revoked)) {$r=Review $data.validation $token $valid 401;Assert ($r.code -match '^AUTH_') 'Wrong 401 code'}
    $r=Review $data.validation $reporter $valid 403;Assert ($r.code -eq 'ACTION_FORBIDDEN') 'Reporter forbidden'
    $null=Review $data.worker $worker $valid 403
    $null=Review $data.current $worker $valid 403
    $r=Review $data.foreign $dispatcher $valid 404;Assert ($r.code -eq 'RESOURCE_NOT_FOUND' -and -not $r.data.current_status) 'Scope existence leak'
    $null=Review 999999 $dispatcher $valid 404
    $null=Review $data.validation $admin $valid 404
    Mode 'admin-read';$null=Review $data.validation $admin $valid 403
    Mode 'admin-area';$null=Review $data.validation $admin $valid 403
    Mode 'admin-wrong-review';$null=Review $data.validation $admin $valid 403
    Mode 'admin-review'
    foreach($action in @('APPROVE','RETURN','REJECT')) {
        $key=$action.ToLower();$id=$data.$key;$payload=Payload $action
        $before=FixtureCall '--inspect' $id|ConvertFrom-Json
        $r=Review $id $dispatcher $payload
        $after=FixtureCall '--inspect' $id|ConvertFrom-Json
        $target=if($action -eq 'APPROVE'){'PENDING_DISPATCH'}elseif($action -eq 'RETURN'){'PENDING_SUPPLEMENT'}else{'CANCELLED'}
        Assert ($r.data.id -eq $id -and $r.data.status -eq $target -and $r.data.version -eq 3 -and $r.data.review_action -eq $action) 'Outcome mismatch'
        Assert (((@($r.data.PSObject.Properties.Name)|Sort-Object) -join ',') -eq 'id,review_action,status,version') 'Nonminimal review response'
        Assert ($after.status -eq $target -and $after.version -eq 3 -and $after.requested_urgency -eq 'NORMAL') 'Persisted mismatch'
        Assert ($after.order_no -eq $before.order_no -and $after.submitted_at -eq $before.submitted_at -and @($after.logs).Count -eq 3) 'History changed'
        Assert ((@($after.logs[0..1])|ConvertTo-Json -Depth 8 -Compress) -eq (@($before.logs)|ConvertTo-Json -Depth 8 -Compress)) 'Previously persisted events changed'
        $event=$after.logs[-1]
        Assert ($event.request_id -eq $r.request_id -and $event.from_status -eq 'PENDING_REVIEW' -and $event.to_status -eq $target -and $event.internal_note -eq 'REVIEW_PRIVATE_MARKER') 'Audit mismatch'
        if($action -eq 'APPROVE'){Assert ($after.category_id -eq 2 -and $after.approved_urgency -eq 'HIGH' -and $null -eq $event.public_note -and $event.action -eq 'ORDER_REVIEWED') 'Approve fields'}
        else {Assert ($event.public_note -eq 'Fictional public reason' -and $after.category_id -eq 1 -and $null -eq $after.approved_urgency) 'Return/reject fields'}
        if($action -eq 'REJECT'){Assert ($after.cancelled_at -match '^\d{4}-\d\d-\d\dT.*Z$' -and $event.action -eq 'ORDER_REJECTED') 'Reject UTC/action'}
        $public=Request "/api/work-orders/$id/timeline" $reporter 200 $null 'GET'
        Assert ((($public|ConvertTo-Json -Depth 10) -notmatch 'internal_note|management_detail|REVIEW_PRIVATE_MARKER') -and $public.data.items[-1].public_note -eq $event.public_note) 'Public timeline leakage'
        $management=Request "/api/work-orders/$id/timeline" $dispatcher 200 $null 'GET'
        Assert ($management.data.items[-1].internal_note -eq 'REVIEW_PRIVATE_MARKER') 'Management note missing'
        $managementKeys=@($management.data.items[-1].management_detail.PSObject.Properties.Name)|Sort-Object
        Assert (($managementKeys -join ',') -eq $(if($action -eq 'APPROVE'){'approved_urgency,category_id,review_action'}else{'review_action'})) 'Review metadata whitelist'
        $restricted=Request "/api/work-orders/$id/timeline" $admin 200 $null 'GET'
        Assert (($restricted|ConvertTo-Json -Depth 10) -notmatch 'internal_note|management_detail|REVIEW_PRIVATE_MARKER') 'REVIEW incorrectly grants internal read'
        $detail=Request "/api/work-orders/$id" $reporter 200 $null 'GET'
        $list=Request "/api/work-orders?view=reported&status=$target" $reporter 200 $null 'GET'
        Assert ($detail.data.status -eq $target -and $detail.data.version -eq 3 -and (@($list.data.items.id) -contains $id)) 'List/detail mismatch'
        $rConflict=Review $id $dispatcher $payload 409;Assert ($rConflict.code -eq 'ORDER_STATUS_CONFLICT') 'Wrong status conflict'
        Write-Host ('ACTUAL API-13 '+$payload+' => '+($r|ConvertTo-Json -Depth 6 -Compress))
        Write-Host ('ACTUAL SQLite '+($after|ConvertTo-Json -Depth 8 -Compress))
        $receipts+=@{request=($payload|ConvertFrom-Json);response=$r;sqlite=$after}
    }
    $r=Review $data.validation $dispatcher (Payload 'APPROVE' 1) 409;Assert ($r.code -eq 'ORDER_VERSION_CONFLICT' -and $r.data.current_version -eq 2) 'Version semantics'
    $null=Review $data.draft $dispatcher (Payload 'APPROVE' 1) 409
    $null=Review $data.current $dispatcher (Payload 'APPROVE' 1) 409
    foreach($id in @('0','-1','1.0','abc','9223372036854775808')) {$null=Review $id $dispatcher $valid 422}
    foreach($raw in @('{','[]','null','"text"')) {$r=Review $data.validation $dispatcher $raw 400;Assert ($r.code -eq 'MALFORMED_REQUEST') 'Malformed semantics'}
    foreach($value in @('0','-1','1.5','2.0','true','"2"','null','9223372036854775808')) {
        $raw='{"action":"APPROVE","expected_version":'+$value+',"category_id":2,"approved_urgency":"HIGH"}'
        $r=Review $data.validation $dispatcher $raw 422;Assert ($r.code -eq 'VALIDATION_ERROR') 'Validation code'
    }
    foreach($field in @('reporter_id','reviewer_id','operator_id','role','status','to_status','order_no','location_id','metadata_json')) {
        $obj=$valid|ConvertFrom-Json;$obj|Add-Member NoteProperty $field 'forged';$null=Review $data.validation $dispatcher ($obj|ConvertTo-Json -Compress) 422
    }
    foreach($field in @('category_id','approved_urgency','action')) {
        $obj=$valid|ConvertFrom-Json;$obj.PSObject.Properties.Remove($field);$null=Review $data.validation $dispatcher ($obj|ConvertTo-Json -Compress) 422
    }
    foreach($category in @(0,-1,999999)) {$obj=$valid|ConvertFrom-Json;$obj.category_id=$category;$null=Review $data.validation $dispatcher ($obj|ConvertTo-Json -Compress) 422}
    foreach($category in @('true','"2"','2.0','null','9223372036854775808')) {
        $raw='{"action":"APPROVE","expected_version":2,"category_id":'+$category+',"approved_urgency":"HIGH"}'
        $null=Review $data.validation $dispatcher $raw 422
    }
    Mode 'disable-category';$null=Review $data.validation $dispatcher $valid 422;Mode 'enable-category'
    foreach($urgency in @('urgent','','CRITICAL')) {$obj=$valid|ConvertFrom-Json;$obj.approved_urgency=$urgency;$null=Review $data.validation $dispatcher ($obj|ConvertTo-Json -Compress) 422}
    foreach($value in @('[]','{}','null','123')) {$raw='{"action":"RETURN","expected_version":2,"reason":"reason","internal_note":'+$value+'}';$null=Review $data.validation $dispatcher $raw 422}
    foreach($action in @('RETURN','REJECT')) {
        foreach($reason in @(''," `t`r`n",('x'*501))) {$obj=Payload $action|ConvertFrom-Json;$obj.reason=$reason;$null=Review $data.validation $dispatcher ($obj|ConvertTo-Json -Compress) 422}
        $obj=Payload $action|ConvertFrom-Json;$obj.PSObject.Properties.Remove('reason');$null=Review $data.validation $dispatcher ($obj|ConvertTo-Json -Compress) 422
        $obj=Payload $action|ConvertFrom-Json;$obj|Add-Member NoteProperty category_id 2;$null=Review $data.validation $dispatcher ($obj|ConvertTo-Json -Compress) 422
    }
    $obj=$valid|ConvertFrom-Json;$obj|Add-Member NoteProperty reason 'not accepted on APPROVE';$null=Review $data.validation $dispatcher ($obj|ConvertTo-Json -Compress) 422
    $obj=$valid|ConvertFrom-Json;$obj.action='CANCEL';$null=Review $data.validation $dispatcher ($obj|ConvertTo-Json -Compress) 422
    foreach($action in @('APPROVE','RETURN','REJECT')) {
        $obj=Payload $action|ConvertFrom-Json;$obj.internal_note=([string][char]0x4fee)*2000
        $id=NewPending;$null=Review $id $dispatcher ($obj|ConvertTo-Json -Compress)
        $stored=FixtureCall '--inspect' $id|ConvertFrom-Json
        Assert ($stored.logs[-1].internal_note -ceq $obj.internal_note) '2000 Unicode note not preserved'
        $obj.internal_note+=([string][char]0x4fee)
        $r=Review $data.validation $dispatcher ($obj|ConvertTo-Json -Compress) 422
        Assert ($r.code -eq 'VALIDATION_ERROR' -and $r.data.field -eq 'internal_note') '2001-character note did not return field validation'
        foreach($kind in @('omitted','empty','blank')) {
            $obj=Payload $action|ConvertFrom-Json
            if($kind -eq 'omitted'){$obj.PSObject.Properties.Remove('internal_note')}
            elseif($kind -eq 'empty'){$obj.internal_note=''}
            else{$obj.internal_note=" `t`r`n"+([string][char]0x3000)}
            $id=NewPending;$null=Review $id $dispatcher ($obj|ConvertTo-Json -Compress)
            $stored=FixtureCall '--inspect' $id|ConvertFrom-Json
            Assert ($null -eq $stored.logs[-1].internal_note) 'Blank/omitted note was not stored as NULL'
            if($action -ne 'APPROVE'){Assert ($stored.logs[-1].public_note -eq 'Fictional public reason') 'Public reason changed by note normalization'}
        }
    }
    $obj=Payload|ConvertFrom-Json;$obj.internal_note=([char]::ConvertFromUtf32(0x1F527))*2000
    $id=NewPending;$null=Review $id $dispatcher ($obj|ConvertTo-Json -Compress)
    $stored=FixtureCall '--inspect' $id|ConvertFrom-Json
    Assert ($stored.logs[-1].internal_note -ceq $obj.internal_note) 'Supplementary Unicode boundary not preserved'
    $obj.internal_note+=[char]::ConvertFromUtf32(0x1F527)
    $r=Review $data.validation $dispatcher ($obj|ConvertTo-Json -Compress) 422;Assert ($r.data.field -eq 'internal_note') 'Supplementary Unicode over-limit accepted'
    Mode 'disable-dispatcher';$null=Review $data.validation $dispatcher $valid 401;Mode 'enable-dispatcher'
    Mode 'revoke-dispatcher';$null=Review $data.validation $dispatcher $valid 404;Mode 'restore-dispatcher'
    Mode 'revoke-review';$null=Review $data.validation $admin $valid 403;Mode 'admin-review'
    $null=Review $data.admin $admin $valid
    Mode 'admin-internal'
    $expanded=Request "/api/work-orders/$($data.admin)/timeline" $admin 200 $null 'GET';Assert ($expanded.data.items[-1].internal_note -eq 'REVIEW_PRIVATE_MARKER') 'Explicit internal read failed'
    $null=Review $data.worker $dispatcher (Payload 'RETURN')
    $workerTime=Request "/api/work-orders/$($data.worker)/timeline" $worker 200 $null 'GET'
    Assert ($workerTime.data.items[-1].public_note -eq 'Fictional public reason' -and (($workerTime|ConvertTo-Json -Depth 10) -notmatch 'internal_note|REVIEW_PRIVATE_MARKER')) 'Worker owner note leakage'
    foreach($failure in @(@('log-failure','log_failure','APPROVE'),@('update-failure','update_failure','APPROVE'),@('reject-failure','reject_failure','REJECT'))) {
        Mode $failure[0];$r=Review $data.($failure[1]) $dispatcher (Payload $failure[2]) 500;Assert ($r.code -eq 'INTERNAL_ERROR') 'Fault response';Mode 'clear-failure'
    }
    # Actual concurrent requests to the production route, in addition to two-connection C++ tests.
    Add-Type -AssemblyName System.Net.Http
    $client=New-Object Net.Http.HttpClient;$client.Timeout=[TimeSpan]::FromSeconds(15)
    try {
        $messages=@();$tasks=@()
        foreach($action in @('APPROVE','RETURN')) {
            $message=New-Object Net.Http.HttpRequestMessage([Net.Http.HttpMethod]::Post,"$base/api/work-orders/$($data.race)/review")
            $actorToken=if($action -eq 'APPROVE'){$dispatcher}else{$admin}
            $message.Headers.Authorization=New-Object Net.Http.Headers.AuthenticationHeaderValue('Bearer',$actorToken)
            $message.Content=New-Object Net.Http.StringContent((Payload $action),[Text.Encoding]::UTF8,'application/json')
            $messages+=,$message;$tasks+=,$client.SendAsync($message)
        }
        $statuses=@();foreach($task in $tasks){$response=$task.GetAwaiter().GetResult();$statuses+=[int]$response.StatusCode;$response.Dispose()}
        Assert ((($statuses|Sort-Object) -join ',') -eq '200,409') 'HTTP race did not yield one winner and one conflict'
        $final=FixtureCall '--inspect' $data.race|ConvertFrom-Json
        Assert ($final.version -eq 3 -and @($final.logs).Count -eq 3) 'HTTP race repeated version/log'
        $checks+=2;Write-Host 'PASS production HTTP concurrent APPROVE/RETURN => 200,409; version=3; logs=3'
    }finally{foreach($message in $messages){$message.Dispose()};$client.Dispose()}
    $integrity=FixtureCall '--integrity' $null;Write-Host "PASS $integrity"
    if($EvidencePath) {
        $parent=Split-Path -Parent $EvidencePath;if(-not(Test-Path -LiteralPath $parent)){New-Item -ItemType Directory -Path $parent|Out-Null}
        @{http_requests=$checks;receipts=$receipts;integrity=$integrity;concurrency='one 200 and one 409'}|ConvertTo-Json -Depth 16|Set-Content -LiteralPath $EvidencePath -Encoding UTF8
    }
    Stop-Process -Id $process.Id;$process.WaitForExit();$process=$null
    $logs=((Get-Content (Join-Path $root 'stdout.log') -Raw),(Get-Content (Join-Path $root 'stderr.log') -Raw)) -join ''
    foreach($name in @('demo_reporter01','demo_reporter02','demo_worker01','demo_worker02','demo_dispatcher','demo_sysadmin','test_no_grants','expired','revoked')){Assert (-not $logs.Contains([string]$data.$name)) 'Raw session token logged'}
    Assert (-not $logs.Contains('REVIEW_PRIVATE_MARKER')) 'Internal note logged'
    Write-Host "PASS $checks production HTTP requests; business rollback fingerprints; no token/note in logs; 0 new pure Policy tests"
}catch{Write-Error -ErrorAction Continue $_.Exception.Message;exit 1}
finally {
    if($process -and -not $process.HasExited){Stop-Process -Id $process.Id;$process.WaitForExit()}
    $env:CAMPUSFIX_DB_PATH=$oldDb;$env:CAMPUSFIX_PORT=$oldPort
    $resolved=[IO.Path]::GetFullPath($root);$parent=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if($resolved.StartsWith($parent,[StringComparison]::OrdinalIgnoreCase) -and ([IO.Path]::GetFileName($resolved)).StartsWith('campusfix-review-http-')){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
