#Requires -Version 5.1
param([string]$Server,[string]$Fixture,[string]$Migrations,[string]$Seed,[string]$EvidencePath)
$ErrorActionPreference='Stop';$root=Join-Path ([IO.Path]::GetTempPath()) ('campusfix-worker-http-'+[guid]::NewGuid().ToString('N'))
$process=$null;$oldDb=$env:CAMPUSFIX_DB_PATH;$oldPort=$env:CAMPUSFIX_PORT;$checks=0;$receipts=@()
function Assert($ok,$message){if(-not $ok){throw $message}}
function SQL($op,$arg=$null){$out=if($null -eq $arg){& $Fixture $op $script:db}else{& $Fixture $op $script:db $arg};Assert ($LASTEXITCODE -eq 0) "Fixture $op failed";return $out}
function Mode($name){$null=SQL '--mode' $name}
function Profile($area=1,$capacity=3){return @{service_area_id=$area;availability='AVAILABLE';max_concurrent_tasks=$capacity;emergency_duty=$true;active=$true;skills=@(@{skill_id=1;level=5;certified=$true},@{skill_id=2;level=3;certified=$false})}}
function Request($method,$path,$token,$body=$null,$expected=200){
 $headers=@{};if($token){$headers.Authorization='Bearer '+$token}
 $args=@{UseBasicParsing=$true;Uri=$script:base+$path;Method=$method;Headers=$headers;TimeoutSec=10}
 if($null -ne $body){$args.ContentType='application/json';$args.Body=[Text.Encoding]::UTF8.GetBytes($body)}
 $before=SQL '--snapshot'
 try{$res=Invoke-WebRequest @args;$status=[int]$res.StatusCode;$json=$res.Content|ConvertFrom-Json;$type=$res.Headers['Content-Type'];$cache=$res.Headers['Cache-Control']}
 catch{if(-not $_.Exception.Response){throw};$res=$_.Exception.Response;$status=[int]$res.StatusCode;$json=$_.ErrorDetails.Message|ConvertFrom-Json;$type=$res.ContentType;$cache=$res.Headers['Cache-Control']}
 Assert ($status -eq $expected) "HTTP $method $path => $status expected $expected code=$($json.code)"
 Assert ($type -match '^application/json' -and $json.request_id -and $cache -eq 'no-store') 'Invalid envelope/cache'
 if($method -eq 'GET' -or $status -ge 400){Assert ((SQL '--snapshot') -eq $before) 'GET/failure changed database'}
 $wire=$json|ConvertTo-Json -Depth 12 -Compress
 Assert ($wire -notmatch '"(?:password_hash|token_hash|work_phone|contact_phone|user_id|username|management_grants|metadata_json|scope_location_id)"\s*:|SELECT |UPDATE |fixture .*failure') 'Sensitive response'
 $script:checks++;Write-Host "PASS Worker production HTTP $method $path => $status $($json.code)";return $json
}
function Put($token,$payload,$expected=200,$id=1){return Request 'PUT' "/api/workers/$id" $token ($payload|ConvertTo-Json -Depth 8 -Compress) $expected}
try {
 New-Item -ItemType Directory -Path $root|Out-Null;$db=Join-Path $root 'workers.sqlite3';$credentials=Join-Path $root 'credentials.json'
 & $Fixture --init $db $credentials $Migrations $Seed;Assert ($LASTEXITCODE -eq 0) 'Fixture init failed';$data=Get-Content -Raw $credentials|ConvertFrom-Json
 $listener=New-Object Net.Sockets.TcpListener([Net.IPAddress]::Loopback,0);$listener.Start();$port=$listener.LocalEndpoint.Port;$listener.Stop()
 $base="http://127.0.0.1:$port";$env:CAMPUSFIX_PORT=[string]$port;$env:CAMPUSFIX_DB_PATH=$db
 $process=Start-Process -FilePath $Server -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $root 'server.log') -RedirectStandardError (Join-Path $root 'error.log')
 $ready=$false;for($i=0;$i -lt 60;$i++){try{$null=Invoke-WebRequest -UseBasicParsing "$base/api/health" -TimeoutSec 1;$ready=$true;break}catch{Start-Sleep -Milliseconds 100}};Assert $ready 'Backend not ready'
 $da=$data.demo_dispatcher;$dbb=$data.test_dispatcher_b;$admin=$data.demo_sysadmin;$worker=$data.demo_worker01;$reporter=$data.demo_reporter01
 foreach($token in @('','invalid-session',$data.expired,$data.revoked)){$null=Request 'GET' '/api/workers' $token $null 401;$null=Put $token (Profile) 401}
 $a=Request 'GET' '/api/workers' $da;$b=Request 'GET' '/api/workers' $dbb
 Assert ($a.data.total -eq 3 -and (($a.data.items.id) -join ',') -eq '1,2,3' -and $b.data.total -eq 3 -and (($b.data.items.id) -join ',') -eq '4,5,6') 'Scoped totals/IDs incorrect'
 foreach($row in @($a.data.items)+@($b.data.items)){Assert (((@($row.PSObject.Properties.Name)|Sort-Object) -join ',') -eq 'active,active_tasks,availability,display_name,emergency_duty,id,max_concurrent_tasks,service_area_id,skills,worker_no') 'Management list whitelist'}
 Assert (@($a.data.skill_options).Count -eq 3) 'Complete active skill options independent of worker page'
 foreach($option in $a.data.skill_options){Assert (((@($option.PSObject.Properties.Name)|Sort-Object) -join ',') -eq 'code,id,name,requires_certificate') 'Skill option whitelist'}
 $mine=Request 'GET' '/api/workers?mine=true' $worker;Assert ($mine.data.total -eq 1 -and $mine.data.items[0].id -eq 1) 'Mine view'
 Assert (-not ($mine.data.PSObject.Properties.Name -contains 'skill_options') -and -not ($mine.data.items[0].PSObject.Properties.Name -contains 'active') -and -not ($mine.data.items[0].PSObject.Properties.Name -contains 'emergency_duty')) 'Worker view must not gain management fields'
 $hydrate=Profile;$hydrate.active=$false;$hydrate.emergency_duty=$true;$hydrate.availability='OFF_DUTY';$hydrate.skills[0].level=4;$hydrate.skills[1].level=2;$null=Put $da $hydrate
 $full=Request 'GET' '/api/workers' $da;$record=@($full.data.items|Where-Object id -eq 1)[0]
 Assert ($record.active -is [bool] -and -not $record.active -and $record.emergency_duty -is [bool] -and $record.emergency_duty -and $record.availability -eq 'OFF_DUTY' -and @($record.skills).Count -eq 2) 'Hydration preserves actual false/true/status/skills'
 $hydrate.max_concurrent_tasks=4;$partial=Put $da $hydrate;Assert (-not ($partial.data.PSObject.Properties.Name -contains 'active')) 'API16 remains partial'
 $reloaded=Request 'GET' '/api/workers' $da;$record=@($reloaded.data.items|Where-Object id -eq 1)[0]
 Assert (-not $record.active -and $record.emergency_duty -and $record.availability -eq 'OFF_DUTY' -and $record.max_concurrent_tasks -eq 4 -and @($record.skills).Count -eq 2 -and $record.skills[0].skill_id -eq 1 -and $record.skills[0].level -eq 4 -and $record.skills[0].certified -and $record.skills[1].skill_id -eq 2 -and $record.skills[1].level -eq 2 -and -not $record.skills[1].certified) 'Capacity-only save preserves other fields'
 $null=Put $da (Profile)
 $null=Request 'GET' '/api/workers' $worker $null 403;$null=Request 'GET' '/api/workers' $reporter $null 403
 $null=Request 'GET' '/api/workers' $admin $null 403;Mode 'admin-area';$null=Request 'GET' '/api/workers' $admin $null 403;Mode 'admin-worker-b'
 $adminList=Request 'GET' '/api/workers' $admin;Assert ((($adminList.data.items.id) -join ',') -eq '4,5,6') 'Admin mixed WORKER_MANAGE and AREA_SCOPE'
 Mode 'overlap-a';$again=Request 'GET' '/api/workers' $da;Assert ($again.data.total -eq 3) 'Overlap duplicates'
 $page=Request 'GET' '/api/workers?page=2&page_size=2' $da;Assert ($page.data.total -eq 3 -and $page.data.items[0].id -eq 3 -and @($page.data.items).Count -eq 1) 'Pagination'
 $filter=Request 'GET' '/api/workers?availability=OFF_DUTY' $da;Assert ($filter.data.items[0].id -eq 3) 'Availability filter'
 $filter=Request 'GET' '/api/workers?skill_id=1' $da;Assert ($filter.data.total -eq 2) 'Skill filter'
 $empty=Request 'GET' '/api/workers?skill_id=3' $da;Assert ($empty.data.total -eq 0 -and @($empty.data.items).Count -eq 0 -and @($empty.data.skill_options).Count -eq 3) 'Empty filter retains independent skill options'
 foreach($query in @('page=0','page=-1','page=1.0','page_size=101','skill_id=0','availability=INVALID','mine=1','worker_id=2','user_id=7','service_area_id=2')){$null=Request 'GET' "/api/workers?$query" $da $null 422}
 $load=Request 'GET' '/api/workers/1/workload' $worker;Assert ($load.data.active_tasks -eq 2 -and $load.data.remaining_capacity -eq 1 -and $load.data.calculated_at -match 'Z$') 'Real workload'
 $load2=Request 'GET' '/api/workers/2/workload' $da;Assert ($load2.data.active_tasks -eq 2) 'Ended assignments counted'
 $load0=Request 'GET' '/api/workers/3/workload' $da;Assert ($load0.data.active_tasks -eq 0) 'Zero load'
 foreach($pair in @(@($worker,2),@($da,4),@($admin,1),@($reporter,1),@($da,999999))){$null=Request 'GET' "/api/workers/$($pair[1])/workload" $pair[0] $null 404}
 Mode 'overload';$overload=Request 'GET' '/api/workers/1/workload' $da;Assert ($overload.data.active_tasks -eq 2 -and $overload.data.max_concurrent_tasks -eq 1 -and $overload.data.remaining_capacity -eq 0) 'Legacy overload hidden';Mode 'restore-capacity'
 $before=SQL '--inspect' 1|ConvertFrom-Json
 foreach($state in @('AVAILABLE','BUSY','OFF_DUTY','DISABLED')){$body=Profile;$body.availability=$state;$body.active=($state -ne 'DISABLED');$body.emergency_duty=($state -eq 'BUSY');$response=Put $da $body;Assert ($response.data.availability -eq $state) 'Availability update'}
 $stored=SQL '--inspect' 1|ConvertFrom-Json;Assert (-not $stored.active -and -not $stored.emergency_duty -and $stored.account_role -eq 'worker' -and $stored.account_status -eq $before.account_status) 'Flags/account isolation'
 $body=Profile;$body.skills=@(@{skill_id=2;level=4;certified=$false});$null=Put $da $body
 $removed=SQL '--inspect' 1|ConvertFrom-Json;Assert (-not $removed.skills[0].active -and $removed.skills[0].effective_at -eq $before.skills[0].effective_at) 'Old skill deleted or left active'
 $body.skills=@(@{skill_id=1;level=3;certified=$false});$null=Put $da $body
 $reactivated=SQL '--inspect' 1|ConvertFrom-Json;Assert ($reactivated.skills[0].active -and -not $reactivated.skills[0].certified -and $reactivated.skills[0].requires_certificate -and $reactivated.skills[0].effective_at -ne $before.skills[0].effective_at) 'Skill reactivation/certificate semantics'
 $body.skills=@();$null=Put $da $body;$without=Request 'GET' '/api/workers?skill_id=1' $da;Assert ($without.data.total -eq 1) 'Removed skill remains effective'
 $null=Put $da (Profile 1 2);$null=Put $da (Profile 1 1) 422
 $null=Put $da (Profile 2) 403;$null=Put $admin (Profile 2) 404;$null=Put $worker (Profile) 403
 Mode 'admin-worker-a';$moved=Put $admin (Profile 2);$null=Put $da (Profile 2) 404;$null=Put $admin (Profile 1)
 Mode 'revoke-admin';$null=Put $admin (Profile) 404;$null=Request 'GET' '/api/workers' $admin $null 403;Mode 'admin-global'
 Mode 'revoke-a';$null=Put $da (Profile) 404;Mode 'restore-a'
 Mode 'disable-user';$null=Put $da (Profile) 401;Mode 'enable-user'
 foreach($raw in @('{','[]','null')){$null=Request 'PUT' '/api/workers/1' $da $raw 400}
 foreach($field in @('service_area_id','availability','max_concurrent_tasks','emergency_duty','active','skills')){$bad=Profile;$bad.Remove($field);$null=Put $da $bad 422}
 foreach($field in @('user_id','worker_no','username','password_hash','role','active_tasks','expected_version')){$bad=Profile;$bad[$field]='forged';$null=Put $da $bad 422}
 foreach($value in @('0','101','1.0','true','"3"','null')){$raw=(Profile|ConvertTo-Json -Depth 8 -Compress) -replace '"max_concurrent_tasks":3',('"max_concurrent_tasks":'+$value);$null=Request 'PUT' '/api/workers/1' $da $raw 422}
 foreach($field in @('active','emergency_duty')){foreach($value in @('0','1','"true"','null')){$raw=(Profile|ConvertTo-Json -Depth 8 -Compress) -replace ('"'+$field+'":true'),('"'+$field+'":'+$value);$null=Request 'PUT' '/api/workers/1' $da $raw 422}}
 foreach($level in @(0,6)){$bad=Profile;$bad.skills[0].level=$level;$null=Put $da $bad 422}
 $bad=Profile;$bad.skills[0].certified=1;$null=Put $da $bad 422
 $bad=Profile;$bad.skills+=@{skill_id=1;level=2;certified=$false};$null=Put $da $bad 422
 $bad=Profile;$bad.skills[0].skill_id=999999;$null=Put $da $bad 422
 $bad=Profile;$bad.skills='not-array';$null=Put $da $bad 422
 $bad=Profile;$bad.availability='INVALID';$null=Put $da $bad 422
 foreach($area in @(999999,7,11)){$null=Put $da (Profile $area) 422}
 Mode 'disable-area';$null=Put $da (Profile 3) 422;Mode 'enable-area'
 Mode 'disable-skill';$null=Put $da (Profile) 422;$activeOptions=Request 'GET' '/api/workers' $da;Assert (-not (@($activeOptions.data.skill_options.id) -contains 1)) 'Inactive skill excluded from options';Mode 'enable-skill'
 foreach($fault in @('fail-profile','fail-skill')){Mode $fault;$bad=Profile;$bad.skills=@(@{skill_id=2;level=5;certified=$true},@{skill_id=3;level=2;certified=$false});$null=Put $da $bad 500;Mode 'clear-failure'}
 $body=Profile 3 2;$body.skills=@(@{skill_id=1;level=4;certified=$false});$updated=Put $da $body
 $after=SQL '--inspect' 1|ConvertFrom-Json;$finalLoad=Request 'GET' '/api/workers/1/workload' $da;$finalList=Request 'GET' '/api/workers?skill_id=1' $da
 Assert ($after.service_area_id -eq 3 -and $after.active -and $after.emergency_duty -and $after.max_concurrent_tasks -eq 2 -and $finalLoad.data.remaining_capacity -eq 0) 'Final persisted profile/workload mismatch'
 $integrity=SQL '--verify' $credentials;Write-Host "PASS $integrity"
 Write-Host ('ACTUAL API-14 '+($finalList|ConvertTo-Json -Depth 9 -Compress));Write-Host ('ACTUAL API-15 '+($finalLoad|ConvertTo-Json -Depth 6 -Compress));Write-Host ('ACTUAL API-16 '+($updated|ConvertTo-Json -Depth 8 -Compress))
 if($EvidencePath){@{http_requests=$checks;list_a=$a;list_b=$b;own=$mine;load_before=$load;load_after=$finalLoad;update_request=$body;update_response=$updated;before=$before;after=$after;integrity=$integrity}|ConvertTo-Json -Depth 20|Set-Content -Encoding UTF8 -LiteralPath $EvidencePath}
 Stop-Process -Id $process.Id;$process.WaitForExit();$process=$null
 $logs=((Get-Content -Raw (Join-Path $root 'server.log')),(Get-Content -Raw (Join-Path $root 'error.log'))) -join ''
 foreach($name in @('demo_reporter01','demo_reporter02','demo_worker01','demo_worker02','demo_dispatcher','demo_sysadmin','test_no_grants','test_dispatcher_b','expired','revoked')){Assert (-not $logs.Contains($data.$name)) 'Raw token logged'}
 Assert ($logs -match 'Worker overload worker_id=1') 'Overload was silently clamped without diagnostic'
 Write-Host "PASS $checks Worker production HTTP requests; GET/failure full database unchanged; business history unchanged; overload diagnosed"
}catch{Write-Error -ErrorAction Continue $_.Exception.Message;exit 1}
finally{
 if($process -and -not $process.HasExited){Stop-Process -Id $process.Id;$process.WaitForExit()}
 $env:CAMPUSFIX_DB_PATH=$oldDb;$env:CAMPUSFIX_PORT=$oldPort
 $resolved=[IO.Path]::GetFullPath($root);$parent=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
 if($resolved.StartsWith($parent,[StringComparison]::OrdinalIgnoreCase) -and ([IO.Path]::GetFileName($resolved)).StartsWith('campusfix-worker-http-')){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
