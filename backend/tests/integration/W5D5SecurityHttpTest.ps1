#Requires -Version 5.1
param([string]$Server,[string]$Fixture,[string]$Migrations,[string]$Seed,[string]$EvidencePath)
$ErrorActionPreference='Stop'
$root=Join-Path ([IO.Path]::GetTempPath()) ('campusfix-w5d5-'+[guid]::NewGuid().ToString('N'))
$process=$null;$gate=$null;$oldDb=$env:CAMPUSFIX_DB_PATH;$oldPort=$env:CAMPUSFIX_PORT
$checks=0;$snapshots=0;$receipts=@();$races=@();$transitions=@();$workerEvidence=$null
function Assert($ok,$message){if(-not $ok){throw $message}}
function SQL($op,$arg=$null){$out=if($null -eq $arg){& $Fixture $op $script:db}else{& $Fixture $op $script:db $arg};Assert ($LASTEXITCODE -eq 0) "Fixture $op failed";return $out}
function Snapshot{$script:snapshots++;return (SQL '--snapshot'|ConvertFrom-Json)}
function Mode($name){$before=Snapshot;$null=SQL '--mode' $name;$after=Snapshot;Assert ($before.business -eq $after.business) "Setup $name changed business history"}
function Wire($value){return ($value|ConvertTo-Json -Depth 16 -Compress)}
function Request($path,$token,$expected=200,$body=$null,$method='GET'){
 $before=Snapshot;$headers=@{};if($token){$headers.Authorization='Bearer '+$token}
 $args=@{UseBasicParsing=$true;Uri=$script:base+$path;Headers=$headers;TimeoutSec=15;Method=$method}
 if($null -ne $body){$args.ContentType='application/json';$args.Body=[Text.Encoding]::UTF8.GetBytes($body)}
 try{$res=Invoke-WebRequest @args;$status=[int]$res.StatusCode;$j=$res.Content|ConvertFrom-Json;$type=$res.Headers['Content-Type'];$cache=$res.Headers['Cache-Control']}
 catch{if(-not $_.Exception.Response){throw};$res=$_.Exception.Response;$status=[int]$res.StatusCode;$j=$_.ErrorDetails.Message|ConvertFrom-Json;$type=$res.ContentType;$cache=$res.Headers['Cache-Control']}
 Assert ($status -eq $expected) "HTTP $method $path returned $status expected $expected ($($j.code))"
 Assert ($type -match '^application/json' -and $j.request_id -match '^req_[\w-]+$' -and $cache -eq 'no-store') 'Response envelope/type/cache'
 if($expected -lt 400){Assert ($j.code -eq 0) 'Success code'}
 else{
  $codes=@{400=@('MALFORMED_REQUEST');401=@('AUTH_REQUIRED','AUTH_INVALID','AUTH_EXPIRED');403=@('ACTION_FORBIDDEN');404=@('RESOURCE_NOT_FOUND');409=@('ORDER_STATUS_CONFLICT','ORDER_VERSION_CONFLICT');422=@('VALIDATION_ERROR');500=@('INTERNAL_ERROR')}
  Assert ($codes[$expected] -contains $j.code) "Unexpected error code $($j.code)"
 }
 $wire=Wire $j;Assert ($wire -notmatch '"(?:password_hash|token_hash|stored_path|metadata_json|management_grants)"\s*:|SELECT |INSERT INTO |test .*failure') 'Sensitive/error internals leaked'
 $after=Snapshot;$unchanged=$before.all -eq $after.all
 if($method -eq 'GET' -or $expected -ge 400){Assert $unchanged "GET/failure changed whole SQLite database: $method $path"}
 $script:checks++;$script:receipts+=@{method=$method;path=$path;status=$status;code=$j.code;request_id=$j.request_id;unchanged=$unchanged;before=$before;after=$after}
 Write-Host "PASS W5D5 HTTP $method $path => $status $($j.code); unchanged=$unchanged"
 return $j
}
function Payload($action='APPROVE',$version=2){if($action -eq 'APPROVE'){return @{action=$action;expected_version=$version;category_id=2;approved_urgency='HIGH';internal_note='W5D5_PRIVATE_NOTE'}};return @{action=$action;expected_version=$version;reason='W5D5 public reason';internal_note='W5D5_PRIVATE_NOTE'}}
function Review($id,$token,$payload,$expected=200){return Request "/api/work-orders/$id/review" $token $expected (Wire $payload) 'POST'}
function Profile($area=1,$capacity=3){return @{service_area_id=$area;availability='OFF_DUTY';max_concurrent_tasks=$capacity;active=$false;emergency_duty=$true;skills=@(@{skill_id=1;level=4;certified=$true},@{skill_id=2;level=2;certified=$false})}}
function Put($id,$token,$payload,$expected=200){return Request "/api/workers/$id" $token $expected (Wire $payload) 'PUT'}
function NewPending($owner=$script:ra){
 $body=@{category_id=1;location_id=11;title='W5D5 fictional HTTP order';description='Temporary fixture only';requested_urgency='NORMAL';contact_name='Fictional contact';contact_phone='00000000000';expected_visit_at=$null}
 $draft=Request '/api/work-orders' $owner 201 (Wire $body) 'POST'
 return (Request "/api/work-orders/$($draft.data.id)/submit" $owner 200 (Wire @{expected_version=$draft.data.version}) 'POST').data
}
function LegalReview($id,$name,$action){
 $before=SQL '--order' $id|ConvertFrom-Json;$world=Snapshot;$r=Review $id $script:t.$name (Payload $action $before.row.version)
 $after=SQL '--order' $id|ConvertFrom-Json;$end=@{APPROVE='PENDING_DISPATCH';RETURN='PENDING_SUPPLEMENT';REJECT='CANCELLED'}[$action]
 Assert ($after.row.status -eq $end -and $after.row.version -eq $before.row.version+1 -and @($after.logs).Count -eq @($before.logs).Count+1) 'Review state/version/log mismatch'
 $last=$after.logs[-1];Assert ($last.actor_id -eq $script:data.user_ids.$name -and $last.from_status -eq 'PENDING_REVIEW' -and $last.to_status -eq $end -and $last.request_id -eq $r.request_id -and $last.internal_note -eq 'W5D5_PRIVATE_NOTE') 'Review audit attribution mismatch'
 if($action -eq 'APPROVE'){Assert ($after.row.category_id -eq 2 -and $after.row.approved_urgency -eq 'HIGH' -and $null -eq $last.public_note) 'Approval field/public note mismatch'}else{Assert ($last.public_note -eq 'W5D5 public reason') 'Public reason not separate'}
 Assert ($world.workers -eq (Snapshot).workers) 'Review changed personnel'
 $script:transitions+=@{action=$action;request_id=$r.request_id;before=$before;after=$after};return $r
}
try{
 New-Item -ItemType Directory -Path $root|Out-Null;$db=Join-Path $root 'w5d5.sqlite3';$credentials=Join-Path $root 'credentials.json'
 & $Fixture --init $db $credentials $Migrations $Seed;Assert ($LASTEXITCODE -eq 0) 'Fixture init failed'
 $data=Get-Content -Raw -Encoding UTF8 -LiteralPath $credentials|ConvertFrom-Json;$t=$data.tokens;$o=$data.orders;$da=$t.demo_dispatcher;$dbb=$t.test_dispatcher_b;$ra=$t.demo_reporter01;$rb=$t.demo_reporter02
 $listener=New-Object Net.Sockets.TcpListener([Net.IPAddress]::Loopback,0);$listener.Start();$port=$listener.LocalEndpoint.Port;$listener.Stop()
 $base="http://127.0.0.1:$port";$env:CAMPUSFIX_DB_PATH=$db;$env:CAMPUSFIX_PORT=[string]$port
 $process=Start-Process -FilePath $Server -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $root 'server.log') -RedirectStandardError (Join-Path $root 'error.log')
 $ready=$false;for($i=0;$i -lt 80;$i++){try{$null=Invoke-WebRequest -UseBasicParsing "$base/api/health" -TimeoutSec 1;$ready=$true;break}catch{Start-Sleep -Milliseconds 100}};Assert $ready 'Production backend not ready'
 # Action/scope matrix across distinct principals in the same real database.
 $a=Request '/api/work-orders?view=managed&page_size=100' $da;$b=Request '/api/work-orders?view=managed&page_size=100' $dbb
 Assert (@($a.data.items.id) -contains $o.validation -and -not (@($a.data.items.id) -contains $o.b) -and @($b.data.items.id) -contains $o.b -and -not (@($b.data.items.id) -contains $o.validation)) 'Dispatcher A/B scope leak'
 $small=Request '/api/work-orders?view=managed&page=1&page_size=1' $da;Assert ($small.data.total -eq $a.data.total -and @($small.data.items).Count -eq 1) 'Scoped pagination total'
 foreach($pair in @(@($da,$o.b),@($dbb,$o.validation))){$null=Request "/api/work-orders/$($pair[1])" $pair[0] 404;$null=Request "/api/work-orders/$($pair[1])/timeline" $pair[0] 404}
 foreach($name in @('demo_reporter01','demo_worker01','test_no_grants','test_admin_r','test_admin_rb','test_admin_w','test_admin_area')){$null=Request '/api/work-orders?view=managed' $t.$name 403}
 $bs=Request '/api/work-orders?view=managed&page_size=100' $t.test_admin_b;Assert (((@($bs.data.items.id)|Sort-Object)-join ',') -eq ((@($b.data.items.id)|Sort-Object)-join ',')) 'BUSINESS_READ B scope differs'
 $global=Request '/api/work-orders?view=managed&page_size=100' $t.test_admin_global
 Mode 'overlap';$overlap=Request '/api/work-orders?view=managed&page_size=100' $t.test_admin_global;$a2=Request '/api/work-orders?view=managed&page_size=100' $da
 Assert ($overlap.data.total -eq $global.data.total -and @($overlap.data.items.id|Select-Object -Unique).Count -eq $overlap.data.items.Count -and $a2.data.total -eq $a.data.total) 'Global/overlap duplicate or missing total'
 foreach($token in @('',$t.expired,$t.revoked)){$null=Review $o.validation $token (Payload) 401}
 foreach($case in @(@($da,$o.b,404),@($dbb,$o.validation,404),@($t.test_no_grants,$o.validation,404),@($t.test_admin_area,$o.validation,404),@($t.test_admin_b,$o.b,403),@($t.test_admin_r,$o.b,404),@($t.test_admin_rb,$o.validation,404),@($t.test_admin_mix,$o.b,403))){$null=Review $case[1] $case[0] (Payload) $case[2]}
 $null=LegalReview $o.review_r 'test_admin_r' 'APPROVE';$null=LegalReview $o.review_mix 'test_admin_mix' 'RETURN'
 Mode 'revoke-r';$null=Review $o.revoke $t.test_admin_r (Payload) 404;Mode 'restore-r'
 Mode 'disable-da';$null=Review $o.validation $da (Payload) 401;Mode 'enable-da'
 # Personnel capabilities cannot borrow AREA_SCOPE or BUSINESS_READ scopes.
 $wa=Request '/api/workers' $da;$wb=Request '/api/workers' $dbb;$wm=Request '/api/workers' $t.test_admin_w
 Assert ((@($wa.data.items.id)-join ',') -eq '1,2,3' -and (@($wb.data.items.id)-join ',') -eq '4,5,6' -and (@($wm.data.items.id)-join ',') -eq '4,5,6') 'Worker scope leak'
 foreach($name in @('test_no_grants','test_admin_b','test_admin_rb','test_admin_area')){$null=Request '/api/workers' $t.$name 403;$null=Put 4 $t.$name (Profile 2) 404}
 $mine=Request '/api/workers?mine=true' $t.demo_worker01
 Assert ($mine.data.total -eq 1 -and $mine.data.items[0].id -eq 1 -and -not ($mine.data.PSObject.Properties.Name -contains 'skill_options') -and -not ($mine.data.items[0].PSObject.Properties.Name -contains 'active')) 'Worker own view leaked management fields'
 $null=Put 1 $t.demo_worker01 (Profile) 403;$null=Request '/api/workers/4/workload' $t.demo_worker01 404
 $null=Put 4 $da (Profile 2) 404;$null=Put 1 $dbb (Profile) 404
 $null=Put 4 $t.test_admin_mix (Profile 1) 403
 Mode 'mix-wm-a';$null=Put 4 $t.test_admin_mix (Profile 1)
 Mode 'mix-revoke-b';$null=Put 4 $t.test_admin_mix (Profile 2) 403
 Mode 'mix-wm-b';Mode 'mix-revoke-a';$null=Put 4 $t.test_admin_mix (Profile 2) 404
 Mode 'mix-wm-a';$null=Put 4 $t.test_admin_mix (Profile 2)
 # Every malformed/state-invalid review compares all tables before and after.
 $invalid=@(@{action='INVALID';expected_version=2},@{expected_version=2},@{action='APPROVE'},@{action='APPROVE';expected_version=0})
 foreach($v in @(1.5,$true,'2')){$bad=Payload;$bad.expected_version=$v;$invalid+=,$bad}
 foreach($field in @('category_id','approved_urgency')){$bad=Payload;$bad.Remove($field);$invalid+=,$bad}
 foreach($v in @(999999,0)){$bad=Payload;$bad.category_id=$v;$invalid+=,$bad}
 $bad=Payload;$bad.approved_urgency='INVALID';$invalid+=,$bad
 $bad=Payload;$bad.reason='unexpected';$invalid+=,$bad
 foreach($action in @('RETURN','REJECT')){
  $bad=Payload $action;$bad.Remove('reason');$invalid+=,$bad
  foreach($v in @('',' ',([string][char]0x3000),('x'*501))){$bad=Payload $action;$bad.reason=$v;$invalid+=,$bad}
  foreach($field in @('category_id','approved_urgency')){$bad=Payload $action;$bad[$field]=if($field -eq 'category_id'){1}else{'NORMAL'};$invalid+=,$bad}
 }
 $bad=Payload;$bad.internal_note=([char]::ConvertFromUtf32(0x1F527))*2001;$invalid+=,$bad
 $bad=Payload;$bad.extra='forged';$invalid+=,$bad
 foreach($bad in $invalid){$null=Review $o.validation $da $bad 422}
 foreach($raw in @('{','[]','null','"string"')){$null=Request "/api/work-orders/$($o.validation)/review" $da 400 $raw 'POST'}
 Mode 'off-category';$null=Review $o.validation $da (Payload) 422;Mode 'on-category'
 foreach($key in @('draft','supplement','dispatch','cancelled')){$id=$o.$key;$current=SQL '--order' $id|ConvertFrom-Json;$null=Review $id $da (Payload 'APPROVE' $current.row.version) 409}
 $null=Review $o.validation $da (Payload 'APPROVE' 1) 409
 $null=LegalReview $o.approve 'demo_dispatcher' 'APPROVE';$null=Review $o.approve $da (Payload) 409
 $null=LegalReview $o.returned 'demo_dispatcher' 'RETURN';$null=LegalReview $o.reject 'demo_dispatcher' 'REJECT'
 # Public/internal audiences, plus same-order resubmission/version invariants.
 foreach($pair in @(@($o.returned,$ra),@($o.review_mix,$ra))){$public=Request "/api/work-orders/$($pair[0])/timeline" $pair[1];Assert ((Wire $public) -notmatch 'internal_note|W5D5_PRIVATE_NOTE') 'Reporter internal note leak'}
 $workerPending=NewPending $t.demo_worker01
 $null=LegalReview $workerPending.id 'demo_dispatcher' 'RETURN'
 $workerPublic=Request "/api/work-orders/$($workerPending.id)/timeline" $t.demo_worker01
 Assert ((Wire $workerPublic) -notmatch 'internal_note|W5D5_PRIVATE_NOTE') 'Worker original reporter internal note leak'
 $null=Request "/api/work-orders/$($workerPending.id)/timeline" $t.demo_worker04 404
 $manager=Request "/api/work-orders/$($o.returned)/timeline" $da;Assert ($manager.data.items[-1].internal_note -eq 'W5D5_PRIVATE_NOTE') 'Authorized note missing'
 $before=SQL '--order' $o.returned|ConvertFrom-Json;$countBefore=(Snapshot).orders
 $edit=@{category_id=1;location_id=11;title='Fictional supplement';description='Fictional updated description';requested_urgency='NORMAL';contact_name='Fictional contact';contact_phone='00000000000';expected_visit_at=$null;expected_version=3}
 $null=Request "/api/work-orders/$($o.returned)" $da 403 (Wire $edit) 'PUT'
 $null=Review $o.returned $ra (Payload 'RETURN' 3) 403
 $edited=Request "/api/work-orders/$($o.returned)" $ra 200 (Wire $edit) 'PUT'
 $null=Review $o.returned $da (Payload) 409
 $again=Request "/api/work-orders/$($o.returned)/submit" $ra 200 (Wire @{expected_version=$edited.data.version}) 'POST'
 $null=Review $o.returned $da (Payload) 409;$null=LegalReview $o.returned 'demo_dispatcher' 'APPROVE'
 $after=SQL '--order' $o.returned|ConvertFrom-Json
 Assert ($after.row.id -eq $before.row.id -and $after.row.order_no -eq $before.row.order_no -and $after.row.submitted_at -eq $before.row.submitted_at -and $after.row.version -eq 6 -and (Snapshot).orders -eq $countBefore -and @($after.logs|Where-Object action -eq 'ORDER_RETURNED').Count -eq 1) 'Supplement chain changed identity/history/versions'
 # Exact full fields, other workers, account state and assignment history.
 $beforeDisable=Snapshot;$null=Put 1 $da (Profile);Assert ($beforeDisable.business -eq (Snapshot).business -and $beforeDisable.accounts -eq (Snapshot).accounts) 'Disabling personnel changed existing assignments/history/account'
 $beforeWorker=SQL '--worker' 1|ConvertFrom-Json;$world=Snapshot
 $null=Put 1 $da (Profile 1 4);$afterWorker=SQL '--worker' 1|ConvertFrom-Json
 Assert ($afterWorker.row.active -eq 0 -and $afterWorker.row.emergency_duty -eq 1 -and $afterWorker.row.availability -eq 'OFF_DUTY' -and $afterWorker.row.max_concurrent_tasks -eq 4 -and $afterWorker.others -eq $beforeWorker.others) 'Capacity-only profile corrupted fields/other workers'
 Assert ((@($afterWorker.skills|ForEach-Object{"$($_.skill_id):$($_.level):$($_.certified):$($_.active):$($_.effective_at)"})-join '|') -eq (@($beforeWorker.skills|ForEach-Object{"$($_.skill_id):$($_.level):$($_.certified):$($_.active):$($_.effective_at)"})-join '|')) 'Capacity-only save changed skills/history'
 Assert ($world.business -eq (Snapshot).business -and $world.accounts -eq (Snapshot).accounts) 'Personnel update changed assignments/history/accounts'
 $workerEvidence=@{before=$beforeWorker;after=$afterWorker}
 $list=Request '/api/workers' $da;$load=Request '/api/workers/1/workload' $da;$row=@($list.data.items|Where-Object id -eq 1)[0]
 Assert ($load.data.active_tasks -eq 2 -and $row.active_tasks -eq 2 -and $load.data.remaining_capacity -eq 2) 'Real load/history/ended_at mismatch'
 $badProfiles=@()
 foreach($v in @(0,101,1)){$bad=Profile;$bad.max_concurrent_tasks=$v;$badProfiles+=,$bad}
 foreach($level in @(0,6)){$bad=Profile;$bad.skills[0].level=$level;$badProfiles+=,$bad}
 $bad=Profile;$bad.skills[0].skill_id=999999;$badProfiles+=,$bad
 $bad=Profile;$bad.skills+=$bad.skills[0];$badProfiles+=,$bad
 $bad=Profile;$bad.skills[0].certified=1;$badProfiles+=,$bad
 foreach($field in @('active','emergency_duty')){$bad=Profile;$bad[$field]='false';$badProfiles+=,$bad}
 foreach($field in @('active','emergency_duty','skills','service_area_id','availability','max_concurrent_tasks')){$bad=Profile;$bad.Remove($field);$badProfiles+=,$bad}
 foreach($field in @('user_id','worker_no','users.role')){$bad=Profile;$bad[$field]='forged';$badProfiles+=,$bad}
 foreach($id in @(999999,7,11)){$badProfiles+=,(Profile $id)}
 foreach($bad in $badProfiles){$null=Put 1 $da $bad 422}
 Mode 'off-skill';$null=Put 1 $da (Profile) 422;Mode 'on-skill';Mode 'off-area';$null=Put 1 $da (Profile 3) 422;Mode 'on-area'
 Mode 'fail-skill';$bad=Profile;$bad.skills=@(@{skill_id=2;level=5;certified=$false},@{skill_id=3;level=1;certified=$true});$null=Put 1 $da $bad 500;Mode 'clear-fault'
 Mode 'fail-log';$null=Review $o.validation $da (Payload) 500;Mode 'clear-fault'
 $null=LegalReview $o.b 'test_dispatcher_b' 'RETURN'
 $bPublic=Request "/api/work-orders/$($o.b)/timeline" $rb;Assert ((Wire $bPublic) -notmatch 'internal_note|W5D5_PRIVATE_NOTE') 'B reporter internal note leak'
 # Truly overlapping requests on independent HTTP connections, held behind a real SQLite writer.
 Add-Type -AssemblyName System.Net.Http
 foreach($opponent in @('APPROVE','RETURN','REJECT')){for($round=1;$round -le 3;$round++){
  $pending=NewPending;$prior=SQL '--order' $pending.id|ConvertFrom-Json;$world=Snapshot
  $gateReady=Join-Path $root ('ready-'+[guid]::NewGuid().ToString('N'));$release=$gateReady+'-release'
  $gate=Start-Process -FilePath $Fixture -ArgumentList @('--hold',$db,$gateReady,$release) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $root 'gate.log') -RedirectStandardError (Join-Path $root 'gate-error.log')
  $null=$gate.Handle # PS5.1: retain the process handle before exit so ExitCode remains observable.
  for($i=0;$i -lt 100 -and -not (Test-Path -LiteralPath $gateReady);$i++){Start-Sleep -Milliseconds 20};Assert (Test-Path -LiteralPath $gateReady) 'Concurrency writer not ready'
  $clients=@();$messages=@();$tasks=@();$responses=@()
  try{
   $actorNames=if($round % 2 -eq 0){@('test_admin_r','demo_dispatcher')}else{@('demo_dispatcher','test_admin_r')};$actorIndex=0
   $plannedActions=if($round % 2 -eq 0){@($opponent,'APPROVE')}else{@('APPROVE',$opponent)}
   foreach($action in $plannedActions){
    $client=New-Object Net.Http.HttpClient;$client.Timeout=[TimeSpan]::FromSeconds(15);$clients+=,$client
    $message=New-Object Net.Http.HttpRequestMessage([Net.Http.HttpMethod]::Post,"$base/api/work-orders/$($pending.id)/review")
    $message.Headers.Authorization=New-Object Net.Http.Headers.AuthenticationHeaderValue('Bearer',$t.($actorNames[$actorIndex]));$actorIndex++
    $message.Content=New-Object Net.Http.StringContent((Wire (Payload $action $pending.version)),[Text.Encoding]::UTF8,'application/json');$messages+=,$message;$tasks+=,$client.SendAsync($message)
   }
   Start-Sleep -Milliseconds 150;Assert (-not $tasks[0].IsCompleted -and -not $tasks[1].IsCompleted) 'Requests did not overlap at transaction gate'
   New-Item -ItemType File -Path $release|Out-Null
   $statuses=@();$bodies=@()
   foreach($task in $tasks){$response=$task.GetAwaiter().GetResult();$responses+=,$response;$statuses+=[int]$response.StatusCode;$body=$response.Content.ReadAsStringAsync().GetAwaiter().GetResult()|ConvertFrom-Json;$bodies+=,$body;Assert ($body.request_id -and $response.Content.Headers.ContentType.MediaType -eq 'application/json') 'Race envelope';Assert (($response.StatusCode -eq 200 -and $body.code -eq 0) -or ($response.StatusCode -eq 409 -and $body.code -in @('ORDER_STATUS_CONFLICT','ORDER_VERSION_CONFLICT'))) 'Race business code'}
   Assert ((($statuses|Sort-Object)-join ',') -eq '200,409') 'Race requires exactly one success and one conflict'
   $final=SQL '--order' $pending.id|ConvertFrom-Json;$winnerIndex=if($statuses[0]-eq 200){0}else{1};$winner=$plannedActions[$winnerIndex];$target=@{APPROVE='PENDING_DISPATCH';RETURN='PENDING_SUPPLEMENT';REJECT='CANCELLED'}[$winner]
   $winnerActor=$data.user_ids.($actorNames[$winnerIndex])
   Assert ($final.row.version -eq $pending.version+1 -and $final.row.status -eq $target -and @($final.logs).Count -eq @($prior.logs).Count+1 -and $final.logs[-1].to_status -eq $target -and $final.logs[-1].actor_id -eq $winnerActor) 'Race state/version/log inconsistent'
   $winBody=if($statuses[0]-eq 200){$bodies[0]}else{$bodies[1]};Assert ($final.logs[-1].request_id -eq $winBody.request_id) 'Race request attribution'
   Assert ($bodies[0].request_id -ne $bodies[1].request_id -and $final.logs[-1].action -eq @{APPROVE='ORDER_REVIEWED';RETURN='ORDER_RETURNED';REJECT='ORDER_REJECTED'}[$winner]) 'Race event identity'
   if($winner -eq 'APPROVE'){Assert ($final.row.category_id -eq 2 -and $final.row.approved_urgency -eq 'HIGH') 'Winning approval fields missing'}else{Assert ($final.row.category_id -eq $prior.row.category_id -and $final.row.approved_urgency -eq $prior.row.approved_urgency) 'Losing approval mutated category/urgency'}
   Assert ($world.workers -eq (Snapshot).workers) 'Review race changed workers'
   $races+=@{opponent=$opponent;round=$round;overlap_confirmed=$true;statuses=$statuses;winner=$winner;request_ids=@($bodies.request_id);before=$prior;after=$final};$checks+=2
   Write-Host "PASS W5D5 overlapping APPROVE/$opponent round=$round => 200,409; one version/log"
  }finally{if(-not(Test-Path -LiteralPath $release)){New-Item -ItemType File -Path $release|Out-Null};foreach($response in $responses){$response.Dispose()};foreach($message in $messages){$message.Dispose()};foreach($client in $clients){$client.Dispose()};$gate.WaitForExit();Assert ($gate.ExitCode -eq 0) 'Concurrency gate failed';$gate=$null}
 }}
 $integrity=SQL '--integrity';Write-Host "PASS $integrity"
 if($EvidencePath){@{http_requests=$checks;sqlite_snapshots=$snapshots;new_service_assertions=0;race_rounds=$races.Count;receipts=$receipts;transitions=$transitions;worker=$workerEvidence;races=$races;integrity=$integrity}|ConvertTo-Json -Depth 22|Set-Content -Encoding UTF8 -LiteralPath $EvidencePath}
 Stop-Process -Id $process.Id;$process.WaitForExit();$process=$null
 $logs=((Get-Content -Raw (Join-Path $root 'server.log')),(Get-Content -Raw (Join-Path $root 'error.log'))) -join ''
 foreach($name in $t.PSObject.Properties.Name){Assert (-not $logs.Contains([string]$t.$name)) 'Session token leaked in logs'}
 Assert (-not $logs.Contains('W5D5_PRIVATE_NOTE')) 'Private note logged'
 Write-Host "PASS W5D5 $checks production HTTP requests; $snapshots SQLite snapshots; $($races.Count) overlapping race rounds; 0 new C++ Service assertions"
}catch{Write-Error -ErrorAction Continue $_.Exception.Message;exit 1}
finally{
 if($gate -and -not $gate.HasExited){Stop-Process -Id $gate.Id;$gate.WaitForExit()}
 if($process -and -not $process.HasExited){Stop-Process -Id $process.Id;$process.WaitForExit()}
 $env:CAMPUSFIX_DB_PATH=$oldDb;$env:CAMPUSFIX_PORT=$oldPort
 $resolved=[IO.Path]::GetFullPath($root);$parent=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
 if($resolved.StartsWith($parent,[StringComparison]::OrdinalIgnoreCase) -and ([IO.Path]::GetFileName($resolved)).StartsWith('campusfix-w5d5-')){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
