#Requires -Version 5.1
param([string]$Server,[string]$Fixture,[string]$Migrations,[string]$Seed,[string]$EvidencePath)
$ErrorActionPreference='Stop';$root=Join-Path ([IO.Path]::GetTempPath()) ('campusfix-edit-http-'+[guid]::NewGuid().ToString('N'))
$process=$null;$oldDb=$env:CAMPUSFIX_DB_PATH;$oldPort=$env:CAMPUSFIX_PORT;$checks=0;$evidence=@()
function Assert($ok,$message){if(-not $ok){throw $message}}
function SQL($op,$arg=$null){$out=if($null -eq $arg){& $Fixture $op $script:db}else{& $Fixture $op $script:db $arg};Assert ($LASTEXITCODE -eq 0) "Fixture $op failed";return $out}
function Request($method,$path,$token,$body=$null,$expected=200){
 $headers=@{};if($token){$headers.Authorization='Bearer '+$token}
 $args=@{UseBasicParsing=$true;Uri=$script:base+$path;Method=$method;Headers=$headers;TimeoutSec=10}
 if($null -ne $body){$args.ContentType='application/json';$args.Body=[Text.Encoding]::UTF8.GetBytes($body)}
 $before=SQL '--snapshot'
 try{$res=Invoke-WebRequest @args;$status=[int]$res.StatusCode;$json=$res.Content|ConvertFrom-Json;$type=$res.Headers['Content-Type'];$cache=$res.Headers['Cache-Control']}
 catch{if(-not $_.Exception.Response){throw};$res=$_.Exception.Response;$status=[int]$res.StatusCode;$json=$_.ErrorDetails.Message|ConvertFrom-Json;$type=$res.ContentType;$cache=$res.Headers['Cache-Control']}
 Assert ($status -eq $expected) "HTTP $method $path => $status expected $expected code=$($json.code)"
 Assert ($type -match '^application/json' -and $json.request_id -and $cache -eq 'no-store') 'Invalid envelope/cache'
 if($status -ge 400 -or $method -eq 'GET'){Assert ((SQL '--snapshot') -eq $before) 'Failure/read changed business rows'}
 $wire=$json|ConvertTo-Json -Depth 10 -Compress
 Assert ($wire -notmatch '"(?:password_hash|token_hash|stored_path|metadata_json|internal_note)"\s*:|PRIVATE_REVIEW_NOTE') 'Sensitive response fields'
 $script:checks++;Write-Host "PASS production HTTP $method $path => $status $($json.code)";return $json
}
function Form($version){return @{category_id=1;location_id=11;title='Fictional edit HTTP';description='Fictional initial description';requested_urgency='NORMAL';contact_name='Fictional contact';contact_phone='00000000000';expected_visit_at=$null;expected_version=$version}}
function NewOrder($token){$form=Form 1;$form.Remove('expected_version');return (Request 'POST' '/api/work-orders' $token ($form|ConvertTo-Json -Compress) 201).data}
try {
 New-Item -ItemType Directory -Path $root|Out-Null;$db=Join-Path $root 'edit.sqlite3';$credentials=Join-Path $root 'credentials.json'
 & $Fixture --init $db $credentials $Migrations $Seed;Assert ($LASTEXITCODE -eq 0) 'Fixture init failed';$data=Get-Content -Raw $credentials|ConvertFrom-Json
 $listener=New-Object Net.Sockets.TcpListener([Net.IPAddress]::Loopback,0);$listener.Start();$port=$listener.LocalEndpoint.Port;$listener.Stop()
 $base="http://127.0.0.1:$port";$env:CAMPUSFIX_PORT=[string]$port;$env:CAMPUSFIX_DB_PATH=$db
 $process=Start-Process -FilePath $Server -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $root 'server.log') -RedirectStandardError (Join-Path $root 'error.log')
 $ready=$false;for($i=0;$i -lt 60;$i++){try{$null=Invoke-WebRequest -UseBasicParsing "$base/api/health" -TimeoutSec 1;$ready=$true;break}catch{Start-Sleep -Milliseconds 100}};Assert $ready 'Backend not ready'
 foreach($name in @('demo_reporter01','demo_worker01')) {
  $token=$data.$name;$draft=NewOrder $token;$id=$draft.id
  $first=(Request 'POST' "/api/work-orders/$id/submit" $token '{"expected_version":1}').data
  $return=Request 'POST' "/api/work-orders/$id/review" $data.demo_dispatcher '{"action":"RETURN","expected_version":2,"reason":"Fictional public return reason","internal_note":"PRIVATE_REVIEW_NOTE"}'
  $timeline=Request 'GET' "/api/work-orders/$id/timeline" $token;Assert ($timeline.data.items[-1].public_note -eq 'Fictional public return reason') 'Public return reason missing'
  $before=SQL '--inspect' $id|ConvertFrom-Json
  $payload=Form 3;$payload.description='PRIVATE_CHANGED_DESCRIPTION';$payload.contact_phone='00012345678';$payload.requested_urgency='HIGH';$payload.expected_visit_at='2030-01-01T00:30:00.000Z'
  $edit=Request 'PUT' "/api/work-orders/$id" $token ($payload|ConvertTo-Json -Compress)
  Assert ($edit.data.id -eq $id -and $edit.data.version -eq 4 -and $edit.data.status -eq 'PENDING_SUPPLEMENT' -and $edit.data.updated_at -match 'Z$') 'Edit response mismatch'
  Assert (((@($edit.data.PSObject.Properties.Name)|Sort-Object) -join ',') -eq 'id,status,updated_at,version') 'API-07 response whitelist'
  $saved=SQL '--inspect' $id|ConvertFrom-Json
  foreach($field in @('order_no','submitted_at','created_at','approved_urgency','cancelled_at','reporter_id')){Assert ($saved.$field -eq $before.$field) "Immutable field $field changed"}
  $log=$saved.logs[-1];Assert ($log.request_id -eq $edit.request_id -and $log.action -eq 'ORDER_CONTENT_UPDATED' -and $log.from_status -eq $log.to_status) 'Edit audit mismatch'
  Assert ($log.public_note -eq 'Updated fields: description, requested_urgency, contact_phone, expected_visit_at' -and $null -eq $log.metadata_json) 'Unsafe/inaccurate edit summary'
  $null=Request 'PUT' "/api/work-orders/$id" $token ($payload|ConvertTo-Json -Compress) 409
  $null=SQL '--mode' 'submit-failure';$null=Request 'POST' "/api/work-orders/$id/submit" $token '{"expected_version":4}' 500;$null=SQL '--mode' 'clear'
  $persisted=Request 'GET' "/api/work-orders/$id" $token;Assert ($persisted.data.description -eq 'PRIVATE_CHANGED_DESCRIPTION' -and $persisted.data.version -eq 4) 'Saved supplement lost after failed submit'
  $resubmitted=Request 'POST' "/api/work-orders/$id/submit" $token '{"expected_version":4}'
  Assert ($resubmitted.data.version -eq 5 -and $resubmitted.data.order_no -eq $first.order_no -and $resubmitted.data.submitted_at -eq $first.submitted_at) 'Original number/time/version chain changed'
  $final=SQL '--inspect' $id|ConvertFrom-Json
  Assert ($final.orders -eq $before.orders -and $final.sequence -eq $before.sequence -and (($final.logs.action) -join ',') -eq 'ORDER_CREATED,ORDER_SUBMITTED,ORDER_RETURNED,ORDER_CONTENT_UPDATED,ORDER_SUBMITTED') 'Extra order/sequence/history loss'
  Assert ((@($final.logs[0..2])|ConvertTo-Json -Depth 8 -Compress) -eq (@($before.logs)|ConvertTo-Json -Depth 8 -Compress)) 'Existing review history changed'
  $detail=Request 'GET' "/api/work-orders/$id" $token;$time=Request 'GET' "/api/work-orders/$id/timeline" $token
  Assert ($detail.data.description -eq $payload.description -and $detail.data.version -eq 5 -and $time.data.items[-1].from_status -eq 'PENDING_SUPPLEMENT') 'Final read mismatch'
  $null=Request 'POST' "/api/work-orders/$id/submit" $token '{"expected_version":5}' 409
  $evidence+=@{role=$name;first=$first;returned=$return;edit_request=$payload;edit_response=$edit;final=$final}
  Write-Host ('ACTUAL API-07 '+($edit|ConvertTo-Json -Depth 6 -Compress))
  if($name -eq 'demo_reporter01') {
   $null=Request 'POST' "/api/work-orders/$id/review" $data.demo_dispatcher '{"action":"RETURN","expected_version":5,"reason":"Second-round public reason"}'
   $payload.expected_version=6;$payload.description='Second-round fictional description'
   $null=Request 'PUT' "/api/work-orders/$id" $token ($payload|ConvertTo-Json -Compress)
   $again=Request 'POST' "/api/work-orders/$id/submit" $token '{"expected_version":7}'
   Assert ($again.data.version -eq 8 -and $again.data.order_no -eq $first.order_no -and $again.data.submitted_at -eq $first.submitted_at) 'Second round used fixed starting version or changed original numbering'
   $rounds=Request 'GET' "/api/work-orders/$id/timeline?page=2&page_size=3" $token
   Assert ($rounds.data.total -eq 8 -and ($rounds.data.items.action -contains 'ORDER_RETURNED')) 'Multi-round paginated timeline mismatch'
  }
 }
 $draft=NewOrder $data.demo_reporter01;$id=$draft.id;$valid=Form 1
 foreach($token in @('','invalid-session',$data.expired,$data.revoked)){$null=Request 'PUT' "/api/work-orders/$id" $token ($valid|ConvertTo-Json -Compress) 401}
 foreach($name in @('demo_reporter02','demo_worker01','demo_dispatcher','demo_sysadmin')){$expected=if($name -eq 'demo_dispatcher'){403}else{404};$null=Request 'PUT' "/api/work-orders/$id" $data.$name ($valid|ConvertTo-Json -Compress) $expected}
 $null=Request 'PUT' "/api/work-orders/$($data.assigned)" $data.demo_worker01 ($valid|ConvertTo-Json -Compress) 403
 $null=Request 'PUT' '/api/work-orders/999999' $data.demo_reporter01 ($valid|ConvertTo-Json -Compress) 404
 foreach($raw in @('{','[]','null')){$null=Request 'PUT' "/api/work-orders/$id" $data.demo_reporter01 $raw 400}
 foreach($field in @('reporter_id','status','order_no','approved_urgency','actor_id')){$bad=Form 1;$bad[$field]='forged';$null=Request 'PUT' "/api/work-orders/$id" $data.demo_reporter01 ($bad|ConvertTo-Json -Compress) 422}
 foreach($field in @('title','description','category_id','location_id','requested_urgency','contact_name','contact_phone','expected_visit_at','expected_version')){$bad=Form 1;$bad.Remove($field);$null=Request 'PUT' "/api/work-orders/$id" $data.demo_reporter01 ($bad|ConvertTo-Json -Compress) 422}
 foreach($value in @('0','-1','true','1.0','"1"','null','9223372036854775808')){$raw=($valid|ConvertTo-Json -Compress) -replace '"expected_version":1',('"expected_version":'+$value);$null=Request 'PUT' "/api/work-orders/$id" $data.demo_reporter01 $raw 422}
 foreach($field in @('category_id','location_id','requested_urgency','expected_visit_at','title')){$bad=Form 1;$bad[$field]=switch($field){category_id{999999}location_id{999999}requested_urgency{'CRITICAL'}expected_visit_at{'2030-02-30T00:00:00Z'}title{"bad`0title"}};$null=Request 'PUT' "/api/work-orders/$id" $data.demo_reporter01 ($bad|ConvertTo-Json -Compress) 422}
 foreach($kind in @('location','category')){$null=SQL '--mode' "disable-$kind";$null=Request 'PUT' "/api/work-orders/$id" $data.demo_reporter01 ($valid|ConvertTo-Json -Compress) 422;$null=SQL '--mode' "enable-$kind"}
 $null=SQL '--mode' 'disable-ancestor';$null=Request 'PUT' "/api/work-orders/$id" $data.demo_reporter01 ($valid|ConvertTo-Json -Compress) 422;$null=SQL '--mode' 'enable-ancestor'
 foreach($fault in @('log-failure','update-failure')){$null=SQL '--mode' $fault;$null=Request 'PUT' "/api/work-orders/$id" $data.demo_reporter01 ($valid|ConvertTo-Json -Compress) 500;$null=SQL '--mode' 'clear'}
 $null=Request 'PUT' "/api/work-orders/$id" $data.demo_reporter01 ($valid|ConvertTo-Json -Compress)
 $check=SQL '--inspect' $id|ConvertFrom-Json;Assert ($check.status -eq 'DRAFT' -and $check.version -eq 2 -and $null -eq $check.expected_visit_at -and $null -eq $check.order_no) 'Draft/null semantics'
 $integrity=SQL '--verify';Write-Host "PASS $integrity"
 if($EvidencePath){@{http_requests=$checks;chains=$evidence;integrity=$integrity}|ConvertTo-Json -Depth 20|Set-Content -Encoding UTF8 -LiteralPath $EvidencePath}
 Stop-Process -Id $process.Id;$process.WaitForExit();$process=$null
 $logs=((Get-Content -Raw (Join-Path $root 'server.log')),(Get-Content -Raw (Join-Path $root 'error.log'))) -join ''
 foreach($name in @('demo_reporter01','demo_reporter02','demo_worker01','demo_worker02','demo_dispatcher','demo_sysadmin','test_no_grants')){Assert (-not $logs.Contains($data.$name)) 'Token logged'}
 Assert (-not $logs.Contains('PRIVATE_CHANGED_DESCRIPTION') -and -not $logs.Contains('00012345678')) 'Private body logged'
 Write-Host "PASS $checks edit production HTTP requests; same-order chain and rollback; no sensitive logs"
}catch{Write-Error -ErrorAction Continue $_.Exception.Message;exit 1}
finally{
 if($process -and -not $process.HasExited){Stop-Process -Id $process.Id;$process.WaitForExit()}
 $env:CAMPUSFIX_DB_PATH=$oldDb;$env:CAMPUSFIX_PORT=$oldPort
 $resolved=[IO.Path]::GetFullPath($root);$parent=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
 if($resolved.StartsWith($parent,[StringComparison]::OrdinalIgnoreCase) -and ([IO.Path]::GetFileName($resolved)).StartsWith('campusfix-edit-http-')){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
