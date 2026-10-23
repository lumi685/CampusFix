#Requires -Version 5.1
param([string]$Server,[string]$Fixture,[string]$Migrations,[string]$Seed)
$ErrorActionPreference='Stop'
$root=Join-Path ([IO.Path]::GetTempPath()) ('campusfix-catalog-http-'+[guid]::NewGuid().ToString('N'))
$process=$null;$oldDb=$env:CAMPUSFIX_DB_PATH;$oldPort=$env:CAMPUSFIX_PORT;$checks=0
function Assert($ok,$message) {if(-not $ok){throw $message}}
function Request($path,$token,$expected=200) {
    $headers=@{};if($token){$headers.Authorization='Bearer '+$token}
    try {$res=Invoke-WebRequest -UseBasicParsing -Uri ($script:base+$path) -Headers $headers -TimeoutSec 8;$status=[int]$res.StatusCode;$body=$res.Content|ConvertFrom-Json;$type=$res.Headers['Content-Type']}
    catch {if(-not $_.Exception.Response){throw};$status=[int]$_.Exception.Response.StatusCode;$body=$_.ErrorDetails.Message|ConvertFrom-Json;$type=$_.Exception.Response.ContentType}
    Assert ($status -eq $expected) "HTTP $path : $status expected $expected"
    Assert ($type -match '^application/json' -and $body.request_id -and $body.message) 'Invalid envelope'
    if($expected -eq 200){Assert ($body.code -eq 0) 'Invalid code'}
    if($expected -eq 422){Assert ($body.code -eq 'VALIDATION_ERROR' -and $body.data.field) 'Invalid validation code or field'}
    Assert (($body|ConvertTo-Json -Depth 8) -notmatch 'required_skill_id|requires_certificate|target_completion_minutes|password_hash|token_hash|stored_path') 'Sensitive fields'
    $script:checks++;Write-Host "PASS HTTP GET $path => $status";return $body
}
try {
    New-Item -ItemType Directory -Path $root|Out-Null
    $db=Join-Path $root 'catalog.sqlite3';$credentials=Join-Path $root 'credentials.json'
    & $Fixture --init $db $credentials $Migrations $Seed
    Assert ($LASTEXITCODE -eq 0) 'Fixture failed'
    $data=Get-Content -LiteralPath $credentials -Raw|ConvertFrom-Json
    $listener=New-Object Net.Sockets.TcpListener([Net.IPAddress]::Loopback,0);$listener.Start();$port=$listener.LocalEndpoint.Port;$listener.Stop()
    $env:CAMPUSFIX_DB_PATH=$db;$env:CAMPUSFIX_PORT=[string]$port;$base="http://127.0.0.1:$port"
    $process=Start-Process -FilePath $Server -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $root 'stdout.log') -RedirectStandardError (Join-Path $root 'stderr.log')
    $ready=$false
    for($i=0;$i -lt 60;$i++){try{Invoke-WebRequest -UseBasicParsing "$base/api/health" -TimeoutSec 1|Out-Null;$ready=$true;break}catch{Start-Sleep -Milliseconds 100}}
    Assert $ready 'Server not ready'
    foreach($path in @('/api/locations','/api/fault-categories')) {
        foreach($role in @('demo_reporter01','demo_worker01','demo_dispatcher','demo_sysadmin')) {
            $result=Request $path $data.$role
            Assert (@($result.data.items).Count -eq $(if($path -eq '/api/locations'){2}else{4})) 'Root mismatch'
            foreach($row in $result.data.items) {
                $keys=(@($row.PSObject.Properties.Name)|Sort-Object) -join ','
                Assert ($keys -eq $(if($path -eq '/api/locations'){'code,id,location_type,name,parent_id'}else{'code,default_priority,id,name'})) 'Whitelist mismatch'
            }
        }
        $null=Request $path '' 401
        foreach($key in @('expired','revoked')){$null=Request $path $data.$key 401}
        foreach($value in @('','0','-1','1.2','abc','9223372036854775808','1%20OR%201=1')){$null=Request ($path+'?parent_id='+$value) $data.demo_reporter01 422}
        $null=Request ($path+'?role=reporter') $data.demo_reporter01 422
        foreach($parent in @(999999,$(if($path -eq '/api/locations'){101}else{104}),$(if($path -eq '/api/locations'){102}else{105}))) {
            $empty=Request ($path+'?parent_id='+$parent) $data.demo_reporter01
            Assert (@($empty.data.items).Count -eq 0) 'Invalid parent expanded query'
        }
    }
    $locations=Request '/api/locations?parent_id=1' $data.demo_reporter01
    Assert ((@($locations.data.items.id) -join ',') -eq '3,4') 'Direct children mismatch'
    $category=Request '/api/fault-categories?parent_id=101' $data.demo_worker01
    Assert ((@($category.data.items.id) -join ',') -eq '102') 'Category children mismatch'
    $leaf=Request '/api/fault-categories?parent_id=1' $data.demo_reporter01
    Assert (@($leaf.data.items).Count -eq 0) 'Leaf mismatch'
    $again=Request '/api/locations?parent_id=1' $data.demo_reporter01
    Assert ($again.request_id -ne $locations.request_id) 'Static request_id'
    Write-Host ('ACTUAL API-04: '+($locations|ConvertTo-Json -Depth 6 -Compress))
    Write-Host ('ACTUAL API-05: '+($category|ConvertTo-Json -Depth 6 -Compress))
    & $Fixture --verify $db $credentials
    Assert ($LASTEXITCODE -eq 0) 'GET changed database'
    # Real API-06 -> 08 -> 09 -> 10 -> 11 chain; IDs are obtained from the catalogs.
    $roots=Request '/api/fault-categories' $data.demo_reporter01
    $rooms=Request '/api/locations?parent_id=7' $data.demo_reporter01
    Assert (@($rooms.data.items).Count -gt 0) 'No room fixture'
    foreach($role in @('demo_reporter01','demo_worker01')) {
        $headers=@{Authorization='Bearer '+$data.$role}
        $payload=@{category_id=$roots.data.items[0].id;location_id=$rooms.data.items[0].id;title='W4-D4 fictional HTTP chain';description='Fictional integration check';requested_urgency='NORMAL';contact_name='Fictional contact';contact_phone='00000000000'}|ConvertTo-Json
        $created=Invoke-WebRequest -UseBasicParsing -Uri "$base/api/work-orders" -Method POST -Headers $headers -ContentType 'application/json' -Body $payload
        Assert ([int]$created.StatusCode -eq 201) 'Create failed';$order=($created.Content|ConvertFrom-Json).data
        $submitted=Invoke-WebRequest -UseBasicParsing -Uri "$base/api/work-orders/$($order.id)/submit" -Method POST -Headers $headers -ContentType 'application/json' -Body (@{expected_version=$order.version}|ConvertTo-Json)
        $receipt=($submitted.Content|ConvertFrom-Json).data
        Assert ($receipt.status -eq 'PENDING_REVIEW' -and $receipt.order_no) 'Submit failed'
        $list=Request '/api/work-orders?view=reported' $data.$role
        Assert (@($list.data.items.id) -contains $order.id) 'Created order missing from reported list'
        $detail=Request "/api/work-orders/$($order.id)" $data.$role
        Assert ($detail.data.order_no -eq $receipt.order_no -and $detail.data.version -eq 2) 'Detail mismatch'
        $timeline=Request "/api/work-orders/$($order.id)/timeline" $data.$role
        Assert ((@($timeline.data.items.action) -join ',') -eq 'ORDER_CREATED,ORDER_SUBMITTED') 'Timeline mismatch'
        $checks+=2
        Write-Host "PASS $role real create-submit-list-detail-timeline chain, order_id=$($order.id) order_no=$($receipt.order_no)"
    }
    Stop-Process -Id $process.Id;$process.WaitForExit();$process=$null
    $logs=((Get-Content (Join-Path $root 'stdout.log') -Raw),(Get-Content (Join-Path $root 'stderr.log') -Raw)) -join ''
    foreach($entry in $data.PSObject.Properties){if($entry.Name -ne 'fingerprint'){Assert (-not $logs.Contains([string]$entry.Value)) 'Credential in logs'}}
    Write-Host "PASS $checks Catalog production HTTP requests"
}catch{Write-Error -ErrorAction Continue $_.Exception.Message;exit 1}
finally {
    if($process -and -not $process.HasExited){Stop-Process -Id $process.Id;$process.WaitForExit()}
    $env:CAMPUSFIX_DB_PATH=$oldDb;$env:CAMPUSFIX_PORT=$oldPort
    $resolved=[IO.Path]::GetFullPath($root);$parent=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if($resolved.StartsWith($parent,[StringComparison]::OrdinalIgnoreCase) -and ([IO.Path]::GetFileName($resolved)).StartsWith('campusfix-catalog-http-')){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
