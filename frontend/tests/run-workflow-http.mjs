// Isolated real Vite -> production Drogon handlers -> SQLite; NOT browser E2E.
import assert from 'node:assert/strict'
import { spawn, spawnSync } from 'node:child_process'
import { mkdtempSync, readFileSync, writeFileSync, existsSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, resolve, dirname, sep } from 'node:path'
import { fileURLToPath } from 'node:url'
import net from 'node:net'
import { createServer } from 'vite'
import axios from 'axios'
import { apiClient } from '../src/api/client.js'
import { replaceCredential } from '../src/api/credentials.js'
import { transferRequests, requestTransfer, decideTransfer, progressEvent } from '../src/api/workflow.js'
import { workflowFlow, transferPage } from '../src/utils/workflow.js'
const frontend=resolve(dirname(fileURLToPath(import.meta.url)),'..'),root=resolve(frontend,'..'),bin=join(root,'backend/build/Debug'),fixture=join(bin,'campusfix_workflow_http_fixture.exe')
const dir=mkdtempSync(join(tmpdir(),'campusfix-w7d2-')),db=join(dir,'test.sqlite3'),secrets=join(dir,'sessions.json'),trace=join(dir,'begins.txt'),children=[],logs=[],requests=[],fingerprints=[],cases=[],ports=[]
let vite,base,c,lose=false
const originalAdapter=apiClient.defaults.adapter,samples={}
const pause=ms=>new Promise(r=>setTimeout(r,ms))
const command=(...args)=>{const p=spawnSync(fixture,args.map(String),{encoding:'utf8',windowsHide:true});assert.equal(p.status,0,`fixture ${args[0]}: ${p.stderr}`);return p.stdout.trim()}
const mode=(m,id=1)=>command('--mode',db,m,id)
const snap=()=>{const s=command('--snapshot',db);fingerprints.push(s);return s}
const inspect=id=>JSON.parse(command('--inspect',db,id))
async function freePort(){const s=net.createServer();await new Promise(r=>s.listen(0,'127.0.0.1',r));const p=s.address().port;await new Promise(r=>s.close(r));ports.push(p);return p}
async function stop(p){if(p.exitCode===null&&p.signalCode===null){const done=new Promise(r=>p.once('exit',r));p.kill();await done}}
async function request(path,{token=c.demo_worker01,body,raw,method=body===undefined&&raw===undefined?'GET':'POST'}={}){
  const r=await fetch(`${base}/api${path}`,{method,headers:{'Content-Type':'application/json',...(token?{Authorization:`Bearer ${token}`}:{})},...(body!==undefined||raw!==undefined?{body:raw??JSON.stringify(body)}:{})})
  const j=await r.json();assert.match(r.headers.get('content-type'),/json/);assert.ok(j.request_id&&typeof j.request_id==='string');assert.ok(Object.hasOwn(j,'code')&&Object.hasOwn(j,'message')&&Object.hasOwn(j,'data'));assert.doesNotMatch(JSON.stringify(j),/password_hash|token_hash|stored_path|metadata_json|contact_phone|contact_name|internal_note|PRIVATE_WORKFLOW_FAILURE/)
  requests.push({method,path,status:r.status,code:j.code,request_id:j.request_id});return {http:r.status,...j}
}
async function unchanged(path,status,options){const before=snap(),r=await request(path,options);assert.equal(r.http,status,path);assert.equal(snap(),before,'success/failure GET or rejected POST leaves all16 tables unchanged');return r}
async function assigned(worker=1){const id=Number(command('--new',db,secrets));const r=await request(`/work-orders/${id}/assign`,{token:c.demo_dispatcher,body:{worker_id:worker,expected_version:3}});assert.equal(r.http,200);return id}
async function scenario(name,fn){await fn();cases.push(name);console.log(`PASS ${name}`)}
async function queryTests(){
  const one=await assigned(),two=await assigned(2),foreign=await assigned();for(const id of [one,two,foreign])mode('pending',id);mode('foreign',foreign)
  await scenario('API34 mine own-only, real current version, public historical decision',async()=>{
    let r=await unchanged('/transfer-requests?view=mine',200,{});assert.equal(r.data.total,2);assert.ok(r.data.items.every(t=>t.order_id!==two));assert.equal(r.data.items.find(t=>t.order_id===one).order_version,4)
    const accepted=await request(`/work-orders/${one}/accept`,{body:{expected_version:4}});assert.equal(accepted.http,200);r=await unchanged(`/transfer-requests?view=mine&order_id=${one}`,200,{});assert.equal(r.data.items[0].order_version,5);assert.equal(inspect(one).transfers[0].order_version,4)
    mode('rejected',one);r=await unchanged(`/transfer-requests?view=mine&order_id=${one}`,200,{});assert.equal(r.data.items[0].status,'REJECTED');assert.equal(r.data.items[0].review_reason,'Fictional public decision')
  })
  await scenario('API34 managed scoped, independent DISPATCH, no internal/full-order permission gained',async()=>{
    const path='/transfer-requests?view=managed';await unchanged(path,403,{token:c.demo_sysadmin});mode('admin-read');await unchanged(path,403,{token:c.demo_sysadmin});mode('admin-dispatch');mode('remove-admin-read')
    let r=await unchanged(path,200,{token:c.demo_sysadmin});assert.equal(r.data.total,1);assert.equal(r.data.items[0].order_id,two);await unchanged(path,200,{token:c.demo_dispatcher})
    await unchanged(`/work-orders/${two}/timeline`,404,{token:c.demo_sysadmin});mode('overlap');r=await unchanged(path,200,{token:c.demo_sysadmin});assert.equal(r.data.total,1)
    for(const id of [foreign,999999]){r=await unchanged(`${path}&order_id=${id}`,200,{token:c.demo_sysadmin});assert.deepEqual(r.data.items,[]);assert.equal(r.data.total,0)}
    mode('ended',two);r=await unchanged(path,200,{token:c.demo_sysadmin});assert.equal(r.data.total,0);mode('revoke');await unchanged(path,403,{token:c.demo_sysadmin})
  })
  await scenario('API34 auth, role, parameter, stable pagination and exact response white list',async()=>{
    const path='/transfer-requests?view=mine';for(const token of ['', 'bad',c.expired,c.revoked])await unchanged(path,401,{token});await unchanged(path,403,{token:c.demo_reporter01});await unchanged(path,403,{token:c.demo_dispatcher});mode('disable');await unchanged(path,401,{});mode('enable')
    for(const query of ['','view=all','view=mine&page=0','view=mine&page_size=101','view=mine&order_id=-1','view=mine&page=9223372036854775808','view=mine&page=9223372036854775807&page_size=100','view=mine&status=PENDING'])await unchanged(`/transfer-requests?${query}`,422,{})
    const a=await unchanged(`${path}&page=1&page_size=1`,200,{}),b=await unchanged(`${path}&page=2&page_size=1`,200,{});assert.equal(a.data.total,b.data.total);assert.notEqual(a.data.items[0].transfer_request_id,b.data.items[0].transfer_request_id)
    const x=a.data.items[0],y=b.data.items[0];assert.ok(x.created_at>y.created_at||(x.created_at===y.created_at&&x.transfer_request_id>y.transfer_request_id))
    assert.deepEqual(Object.keys(x).sort(),['transfer_request_id','order_id','order_no','assignment_id','reason','status','created_at','order_status','order_version','review_reason','resolved_at'].sort());for(const k of ['transfer_request_id','order_id','assignment_id','order_version'])assert.ok(Number.isSafeInteger(x[k])&&x[k]>0)
  })
}
const application=(id,v=4)=>request(`/work-orders/${id}/transfer-requests`,{body:{expected_version:v,reason:'Fictional specialist needed'}})
const decision=(id,r,v=4,d='APPROVE',token=c.demo_dispatcher)=>request(`/work-orders/${id}/transfer-requests/${r}/decision`,{token,body:{expected_version:v,decision:d,reason:'Fictional public management reason'}})
const progress=(id,type='ARRIVED',note,version=5)=>request(`/work-orders/${id}/progress-events`,{body:{expected_version:version,type,...(note===undefined?{}:{note})}})
const nonlog=()=>command('--nonlog-snapshot',db)
const begins=()=>existsSync(trace)?readFileSync(trace,'utf8').split('\n').filter(Boolean).length:0
async function lock(){const ready=join(dir,`ready-${children.length}`),release=join(dir,`release-${children.length}`),holder=join(bin,'campusfix_dispatch_http_fixture.exe');const p=spawn(holder,['--hold',db,ready,release,'15'],{windowsHide:true,stdio:'ignore'});children.push(p);for(let i=0;i<300&&!existsSync(ready);i++)await pause(5);assert.ok(existsSync(ready));return async()=>{writeFileSync(release,'release');if(p.exitCode===null)await new Promise(r=>p.once('exit',r));assert.equal(p.exitCode,0)}}
async function workflowTests(){
  mode('admin-dispatch')
  await scenario('chain A real assignment/accept/arrival/multiple progress leaves all15 non-log tables unchanged',async()=>{
    const id=await assigned();assert.equal((await request(`/work-orders/${id}/accept`,{body:{expected_version:4}})).http,200);const before=nonlog(),initial=inspect(id)
    const arrival=await progress(id);assert.equal(arrival.http,201);assert.equal(arrival.data.order_version,5);assert.equal(arrival.data.assignment_id,Number(initial.assignments[0].id));await unchanged(`/work-orders/${id}/progress-events`,409,{body:{expected_version:5,type:'ARRIVED'}})
    const first=await progress(id,'PROGRESS_NOTE','Fictional public progress'),second=await progress(id,'PROGRESS_NOTE','😀'.repeat(1000));assert.equal(first.http,201);assert.equal(second.http,201);assert.ok(arrival.data.id<first.data.id&&first.data.id<second.data.id);assert.equal(nonlog(),before)
    const t=await request(`/work-orders/${id}/timeline`);assert.equal(t.http,200);assert.deepEqual(t.data.items.slice(-3).map(x=>x.action),['WORKER_ARRIVED','WORKER_PROGRESS_NOTE','WORKER_PROGRESS_NOTE']);const state=inspect(id);assert.equal(state.status,'IN_PROGRESS');assert.equal(state.version,5);assert.deepEqual(state.assignments,initial.assignments);assert.deepEqual(state.worker_loads,initial.worker_loads);samples.progress={id,response:arrival.data,request_id:arrival.request_id}
  })
  await scenario('chain B actual API21 -> API34 selected request -> API22 REJECT retains owner/version/capacity',async()=>{
    const id=await assigned(),initial=inspect(id),a=await application(id);assert.equal(a.http,201);const req=a.data.transfer_request_id;let s=inspect(id);assert.equal(s.version,4);assert.equal(s.status,'PENDING_ACCEPT');assert.deepEqual(s.assignments,initial.assignments);assert.deepEqual(s.worker_loads,initial.worker_loads)
    const duplicate=await unchanged(`/work-orders/${id}/transfer-requests`,409,{body:{expected_version:4,reason:'Duplicate pending'}});assert.equal(duplicate.code,'DUPLICATE_OPERATION');assert.equal(duplicate.data.transfer_request_id,req)
    replaceCredential(c.demo_sysadmin);const queue=transferPage(await transferRequests({view:'managed',order_id:id}),true),selected=queue.items[0];assert.equal(selected.transfer_request_id,req)
    const rejected=await decideTransfer(selected.order_id,selected.transfer_request_id,selected.order_version,'REJECT','Frontend actual public decision');assert.equal(rejected.order_status,'PENDING_ACCEPT');assert.equal(rejected.version,4);assert.equal(rejected.assignment_ended,false)
    s=inspect(id);assert.deepEqual(s.assignments,initial.assignments);assert.deepEqual(s.worker_loads,initial.worker_loads);assert.equal(s.transfers[0].status,'REJECTED');assert.ok(s.transfers[0].resolved_at);assert.equal(s.transfers[0].review_reason,'Frontend actual public decision');await unchanged(`/work-orders/${id}/transfer-requests/${req}/decision`,409,{token:c.demo_sysadmin,body:{expected_version:4,decision:'APPROVE',reason:'Stale page'}});samples.rejected={id,response:rejected}
    // No BUSINESS_READ or INTERNAL_NOTE_READ was granted to this administrator.
    await unchanged(`/work-orders/${id}/timeline`,404,{token:c.demo_sysadmin})
  })
  await scenario('chain C application-before-accept current version -> DISPATCH-only queue/APPROVE -> ended history/capacity/recommend',async()=>{
    const id=await assigned(),a=await application(id);assert.equal(a.http,201);const req=a.data.transfer_request_id
    assert.equal((await request(`/work-orders/${id}/accept`,{body:{expected_version:4}})).http,200);const initial=inspect(id),load=initial.worker_loads[0].active_tasks
    replaceCredential(c.demo_sysadmin);const selected=transferPage(await transferRequests({view:'managed',order_id:id}),true).items[0];assert.equal(selected.order_version,5);assert.equal(initial.transfers[0].order_version,4)
    const approved=await decideTransfer(id,selected.transfer_request_id,selected.order_version,'APPROVE','Fictional safe reassignment');assert.equal(approved.order_status,'PENDING_DISPATCH');assert.equal(approved.version,6);assert.equal(approved.assignment_ended,true)
    const s=inspect(id),entry=s.status_logs.at(-1);assert.equal(s.transfers[0].status,'APPROVED');assert.ok(s.transfers[0].reviewed_by);assert.equal(s.assignments.length,1);assert.equal(s.assignments[0].end_reason,'TRANSFERRED');assert.equal(s.assignments[0].version,'3');assert.equal(s.worker_loads[0].active_tasks,load-1);assert.deepEqual(s.dispatch_records,initial.dispatch_records);assert.equal(entry.assignment_id,s.assignments[0].id);assert.equal(JSON.parse(entry.metadata_json).transfer_request_id,req);assert.equal(JSON.parse(entry.metadata_json).decision,'APPROVE');assert.equal(entry.action,'TRANSFER_APPROVED')
    await unchanged(`/work-orders/${id}/progress-events`,403,{body:{expected_version:6,type:'ARRIVED'}});const recommendation=await unchanged(`/work-orders/${id}/dispatch-candidates`,200,{token:c.demo_dispatcher});assert.ok(recommendation.data.top_candidates.length>0);samples.approved={id,response:approved,assignment_id:Number(s.assignments[0].id),load_before:load,load_after:s.worker_loads[0].active_tasks}
  })
  await scenario('actual HTTP negative identity, scope, stale/foreign/resolved applications, UTF8/limits and JSON',async()=>{
    const id=await assigned(),a=await application(id),req=a.data.transfer_request_id,other=await assigned()
    for(const token of ['',c.expired,c.revoked,c.demo_reporter01,c.demo_worker02])await unchanged(`/work-orders/${id}/transfer-requests`,token===c.demo_reporter01?403:token===c.demo_worker02?404:401,{token,body:{expected_version:4,reason:'No ownership'}})
    await unchanged(`/work-orders/${id}/transfer-requests`,403,{token:c.demo_dispatcher,body:{expected_version:4,reason:'Cannot impersonate'}})
    await unchanged(`/work-orders/${other}/transfer-requests/${req}/decision`,404,{token:c.demo_dispatcher,body:{expected_version:4,decision:'APPROVE',reason:'Other order'}})
    await unchanged(`/work-orders/${id}/transfer-requests/999999/decision`,404,{token:c.demo_dispatcher,body:{expected_version:4,decision:'APPROVE',reason:'No request'}})
    await unchanged(`/work-orders/${id}/transfer-requests/${req}/decision`,409,{token:c.demo_dispatcher,body:{expected_version:3,decision:'APPROVE',reason:'Stale'}})
    const fresh=await assigned();for(const reason of ['', '\u00a0\u3000', 'x'.repeat(501), '\u0000'])await unchanged(`/work-orders/${fresh}/transfer-requests`,422,{body:{expected_version:4,reason}})
    for(const body of [{expected_version:4},{expected_version:true,reason:'Bad'},{expected_version:4,reason:'Bad',worker_id:1}])await unchanged(`/work-orders/${fresh}/transfer-requests`,422,{body})
    await unchanged(`/work-orders/${fresh}/transfer-requests`,400,{raw:'{"expected_version":4,"reason":"a","reason":"b"}'});await unchanged(`/work-orders/${fresh}/transfer-requests`,400,{body:{expected_version:4,reason:'\ud800'}})
    assert.equal((await request(`/work-orders/${fresh}/transfer-requests`,{body:{expected_version:4,reason:'😀'.repeat(500)}})).http,201)
    for(const body of [{expected_version:4,type:'ARRIVED'},{expected_version:4,type:'OTHER',note:'Bad'}])await unchanged(`/work-orders/${other}/progress-events`,body.type==='OTHER'?422:409,{body})
    await request(`/work-orders/${other}/accept`,{body:{expected_version:4}})
    for(const body of [{expected_version:5,type:'PROGRESS_NOTE'},{expected_version:5,type:'PROGRESS_NOTE',note:'x'.repeat(1001)},{expected_version:5,type:'ARRIVED',note:'x'.repeat(501)},{expected_version:5,type:'PROGRESS_NOTE',note:'　'},{expected_version:5,type:'ARRIVED',note:null}])await unchanged(`/work-orders/${other}/progress-events`,422,{body})
    const foreign=await assigned(),ft=await application(foreign);mode('foreign',foreign);await unchanged(`/work-orders/${foreign}/transfer-requests/${ft.data.transfer_request_id}/decision`,404,{token:c.demo_dispatcher,body:{expected_version:4,decision:'APPROVE',reason:'Outside scope'}})
    const ended=await assigned(),et=await application(ended);await request(`/work-orders/${ended}/refuse`,{body:{expected_version:4,reason:'Refused supersedes'}});assert.equal(inspect(ended).transfers[0].status,'SUPERSEDED');await unchanged(`/work-orders/${ended}/transfer-requests/${et.data.transfer_request_id}/decision`,409,{token:c.demo_dispatcher,body:{expected_version:4,decision:'APPROVE',reason:'Superseded'}})
    const wrong=await assigned(),wt=await application(wrong);mode('pending-verify',wrong);await unchanged(`/work-orders/${wrong}/transfer-requests/${wt.data.transfer_request_id}/decision`,409,{token:c.demo_dispatcher,body:{expected_version:4,decision:'APPROVE',reason:'Wrong state'}})
    mode('completed',wrong);await unchanged(`/work-orders/${wrong}/transfer-requests/${wt.data.transfer_request_id}/decision`,409,{token:c.demo_dispatcher,body:{expected_version:4,decision:'APPROVE',reason:'Completed not transferable'}})
    const old=await assigned(),ot=await application(old);mode('old-assignment',old);await unchanged(`/work-orders/${old}/transfer-requests/${ot.data.transfer_request_id}/decision`,409,{token:c.demo_dispatcher,body:{expected_version:4,decision:'APPROVE',reason:'Old assignment'}});await unchanged(`/work-orders/${old}/transfer-requests`,403,{body:{expected_version:4,reason:'Historical owner cannot request'}})
  })
  await scenario('all actual HTTP workflow write stages roll back under injected SQL failures',async()=>{
    for(const op of ['create','approve','reject','arrived','progress']){
      const id=await assigned();let req=0;if(['approve','reject'].includes(op))req=(await application(id)).data.transfer_request_id;if(['arrived','progress'].includes(op))await request(`/work-orders/${id}/accept`,{body:{expected_version:4}})
      const failures=op==='create'?['fail-insert-transfer','fail-log']:op==='approve'?['fail-update-transfer','fail-assignment','fail-order','fail-log']:op==='reject'?['fail-update-transfer','fail-log']:['fail-log']
      for(const failure of failures){mode(failure);const path=op==='create'?`/work-orders/${id}/transfer-requests`:['approve','reject'].includes(op)?`/work-orders/${id}/transfer-requests/${req}/decision`:`/work-orders/${id}/progress-events`;const body=op==='create'?{expected_version:4,reason:'Fault injection'}:['approve','reject'].includes(op)?{expected_version:4,decision:op==='approve'?'APPROVE':'REJECT',reason:'Fault injection'}:{expected_version:5,type:op==='arrived'?'ARRIVED':'PROGRESS_NOTE',note:'Fault injection'};await unchanged(path,500,{token:['approve','reject'].includes(op)?c.demo_dispatcher:c.demo_worker01,body});mode('clear-failure')}
    }
  })
  await scenario('eight synchronized HTTP races with third-connection lock, no mutex substitute',async()=>{
    for(let m=0;m<8;m++){
      const id=await assigned();if(m>=6)await request(`/work-orders/${id}/accept`,{body:{expected_version:4}});let req=0;if(m!==0&&m!==6)req=(await application(id,m===7?5:4)).data.transfer_request_id
      const release=await lock(),before=begins();const pending=[false,true].map(second=>m===0?application(id):m===6?progress(id):second&&m===4?request(`/work-orders/${id}/refuse`,{body:{expected_version:4,reason:'Concurrent refusal'}}):second&&m===5?request(`/work-orders/${id}/accept`,{body:{expected_version:4}}):second&&m===7?progress(id,'PROGRESS_NOTE','Concurrent progress'):decision(id,req,m===7?5:4,m===3||(m===2&&second)?'REJECT':'APPROVE',second?c.demo_sysadmin:c.demo_dispatcher))
      let reached=false;for(let i=0;i<400;i++){if(begins()>=before+2){reached=true;break}await pause(5)}await release();const results=await Promise.all(pending),statuses=results.map(r=>r.http);assert.ok(reached,'two real BEGIN attempts blocked before release')
      if(m===0||m===6)assert.deepEqual(statuses.sort(),[201,409]);else if(m===7){assert.equal(statuses[0],200);assert.ok([201,403].includes(statuses[1]))}else{assert.equal(statuses.filter(s=>s===200).length,1);assert.ok(statuses.every(s=>[200,409,403].includes(s)))}
      const s=inspect(id);assert.equal(s.assignments.length,1);assert.equal(s.dispatch_records.length,1);assert.ok(s.status_logs.filter(l=>['TRANSFER_APPROVED','TRANSFER_REJECTED'].includes(l.action)).length<=1);if(m===6)assert.equal(s.status_logs.filter(l=>l.action==='WORKER_ARRIVED').length,1)
    }
  })
  await scenario('real SQLITE_BUSY503 and committed client-response loss never replays POST',async()=>{
    const id=await assigned(),release=await lock(),before=snap();const r=await application(id);await release();assert.equal(r.http,503);assert.equal(r.code,'DATABASE_BUSY');assert.equal(snap(),before)
    replaceCredential(c.demo_worker01);const f=workflowFlow({post:(ctx,note)=>requestTransfer(ctx.orderId,ctx.version,note),verify:async()=>transferPage(await transferRequests({view:'mine',order_id:id})).items.map(x=>x.status).join(',')},()=>true);f.open({kind:'transfer',orderId:id,version:4,status:'PENDING_ACCEPT'});f.note='Frontend actual application before lost acknowledgement';lose=true;await f.send();assert.equal(f.phase,'unknown_result');assert.equal(f.verification,'PENDING');assert.equal(inspect(id).transfers.length,1);const count=requests.length;await assert.rejects(f.send());assert.equal(f.open({kind:'transfer',orderId:id,version:4,status:'PENDING_ACCEPT'}),false);assert.equal(requests.length,count)
  })
}
try {
  command('--init',db,secrets,join(root,'backend/migrations'),join(root,'backend/seeds/001_demo_data.sql'));c=JSON.parse(readFileSync(secrets,'utf8'));const backendPort=await freePort()
  const server=spawn(fixture,['--serve',db,String(backendPort),trace],{windowsHide:true,stdio:['ignore','pipe','pipe']});children.push(server);server.stdout.on('data',v=>logs.push(v.toString()));server.stderr.on('data',v=>logs.push(v.toString()))
  let ready=false;for(let i=0;i<150;i++){try{if((await fetch(`http://127.0.0.1:${backendPort}/api/health`)).status===200){ready=true;break}}catch{}assert.equal(server.exitCode,null);await pause(20)}assert.ok(ready)
  vite=await createServer({configFile:join(frontend,'vite.config.js'),root:frontend,server:{host:'127.0.0.1',port:0,strictPort:true,proxy:{'/api':{target:`http://127.0.0.1:${backendPort}`}}},logLevel:'warn'});await vite.listen();const port=vite.httpServer.address().port;ports.push(port);base=`http://127.0.0.1:${port}`
  await queryTests()
  if(!process.argv.includes('--query-only')){
    const http=axios.getAdapter('http');apiClient.defaults.adapter=async config=>{try{const r=await http({...config,baseURL:`${base}/api`});const j=typeof r.data==='string'?JSON.parse(r.data):r.data;requests.push({method:config.method,path:config.url,status:r.status,code:j.code,request_id:j.request_id});if(lose&&config.method==='post'){lose=false;throw new Error('Client injected response loss after real server commit')}return r}catch(e){if(e.response){const j=typeof e.response.data==='string'?JSON.parse(e.response.data):e.response.data;requests.push({method:config.method,path:config.url,status:e.response.status,code:j.code,request_id:j.request_id})}throw e}}
    await workflowTests()
  }
  assert.equal(command('--integrity',db),'integrity_check=ok; foreign_key_check empty');for(const token of Object.values(c))assert.equal(logs.join('').includes(token),false,'no complete sessions logged')
  const scope=process.argv.includes('--query-only')?'query':'workflow';writeFileSync(join(root,`backend/build/Testing/w7d2-${scope}-http-evidence.json`),JSON.stringify({cases,requests,fingerprints:fingerprints.length,samples,not_browser:true},null,2)+'\n')
  console.log(`PASS ${scope} HTTP cases=${cases.length}, requests=${requests.length}, independent SQLite fingerprints=${fingerprints.length}`)
} finally {
  replaceCredential();apiClient.defaults.adapter=originalAdapter
  if(vite)await vite.close();for(const p of children)await stop(p);const path=resolve(dir);assert.ok(path.startsWith(resolve(tmpdir())+sep)&&path.includes('campusfix-w7d2-'));rmSync(path,{recursive:true,force:true})
}
for(const port of ports){const closed=await new Promise(r=>{const s=net.createConnection({host:'127.0.0.1',port});s.once('error',()=>r(true));s.once('connect',()=>{s.destroy();r(false)})});assert.ok(closed,'owned port released')}
console.log('PASS owned servers/ports stopped; temporary database and random credentials cleaned')
