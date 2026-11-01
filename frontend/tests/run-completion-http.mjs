// Real Vite -> Drogon production handlers -> isolated SQLite. Not browser E2E.
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
import { completeTask } from '../src/api/workflow.js'
import { workflowFlow } from '../src/utils/workflow.js'
const frontend=resolve(dirname(fileURLToPath(import.meta.url)),'..'),root=resolve(frontend,'..'),bin=join(root,'backend/build/Debug'),fixture=join(bin,'campusfix_workflow_http_fixture.exe')
const dir=mkdtempSync(join(tmpdir(),'campusfix-w7d3-')),db=join(dir,'test.sqlite3'),secrets=join(dir,'sessions.json'),trace=join(dir,'begins.txt'),children=[],ports=[],logs=[],requests=[],snapshots=[],cases=[],samples={}
const originalAdapter=apiClient.defaults.adapter
let vite,base,c,lose=false
const pause=ms=>new Promise(r=>setTimeout(r,ms))
const command=(...args)=>{const r=spawnSync(fixture,args.map(String),{encoding:'utf8',windowsHide:true});assert.equal(r.status,0,`fixture ${args[0]}: ${r.stderr}`);return r.stdout.trim()}
const mode=(m,id=1)=>command('--mode',db,m,id)
const snap=()=>{const s=command('--snapshot',db);snapshots.push(s);return s}
const inspect=id=>JSON.parse(command('--inspect',db,id))
async function freePort(){const s=net.createServer();await new Promise(r=>s.listen(0,'127.0.0.1',r));const p=s.address().port;await new Promise(r=>s.close(r));ports.push(p);return p}
async function stop(p){if(p.exitCode===null&&p.signalCode===null){const done=new Promise(r=>p.once('exit',r));p.kill();await done}}
async function request(path,{token=c.demo_worker01,body,raw,method=body===undefined&&raw===undefined?'GET':'POST'}={}){
  const r=await fetch(`${base}/api${path}`,{method,headers:{'Content-Type':'application/json',...(token?{Authorization:`Bearer ${token}`}:{})},...(body!==undefined||raw!==undefined?{body:raw??JSON.stringify(body)}:{})})
  const j=await r.json();assert.match(r.headers.get('content-type'),/application\/json/);assert.match(j.request_id,/^req_[\w-]+$/);assert.ok(Object.hasOwn(j,'message')&&Object.hasOwn(j,'data')&&Object.hasOwn(j,'code'))
  assert.doesNotMatch(JSON.stringify(j),/password_hash|token_hash|stored_path|metadata_json|internal_note|PRIVATE_WORKFLOW_FAILURE/)
  requests.push({method,path,status:r.status,code:j.code,request_id:j.request_id});return {http:r.status,...j}
}
async function unchanged(path,status,options={}){const before=snap(),r=await request(path,options);assert.equal(r.http,status,`${path}: ${r.code}`);assert.equal(snap(),before,'rejected POST/GET no business mutation');return r}
async function scenario(name,fn){await fn();cases.push(name);console.log(`PASS ${name}`)}
async function assigned({accept=true,worker=1}={}){
  // Positive chains use only actual create -> submit -> review -> assign -> accept APIs.
  const created=await request('/work-orders',{token:c.demo_reporter01,body:{category_id:1,location_id:11,title:'Fictional completion test',description:'Fictional test fault only',requested_urgency:'NORMAL',contact_name:'Fictional user',contact_phone:'00000000000',expected_visit_at:null}});assert.equal(created.http,201);const id=created.data.id;snap()
  assert.equal((await request(`/work-orders/${id}/submit`,{token:c.demo_reporter01,body:{expected_version:1}})).http,200);snap()
  assert.equal((await request(`/work-orders/${id}/review`,{token:c.demo_dispatcher,body:{action:'APPROVE',expected_version:2,category_id:1,approved_urgency:'HIGH',internal_note:'PRIVATE_COMPLETION_REVIEW'}})).http,200);snap()
  assert.equal((await request(`/work-orders/${id}/assign`,{token:c.demo_dispatcher,body:{worker_id:worker,expected_version:3}})).http,200);snap()
  if(accept){assert.equal((await request(`/work-orders/${id}/accept`,{token:worker===1?c.demo_worker01:c.demo_worker02,body:{expected_version:4}})).http,200);snap()}
  return id
}
const complete=(id,summary='Fictional full repair result',version=5)=>request(`/work-orders/${id}/complete`,{body:{repair_summary:summary,expected_version:version}})
const apply=id=>request(`/work-orders/${id}/transfer-requests`,{body:{expected_version:5,reason:'Fictional transfer'}})
const decide=(id,requestId)=>request(`/work-orders/${id}/transfer-requests/${requestId}/decision`,{token:c.demo_dispatcher,body:{expected_version:5,decision:'APPROVE',reason:'Fictional approval'}})
const begins=()=>existsSync(trace)?readFileSync(trace,'utf8').split('\n').filter(Boolean).length:0
async function lock(){const ready=join(dir,`ready-${children.length}`),release=join(dir,`release-${children.length}`);const p=spawn(join(bin,'campusfix_dispatch_http_fixture.exe'),['--hold',db,ready,release,'15'],{windowsHide:true,stdio:'ignore'});children.push(p);for(let i=0;i<300&&!existsSync(ready);i++)await pause(5);assert.ok(existsSync(ready));return async()=>{writeFileSync(release,'release');if(p.exitCode===null)await new Promise(r=>p.once('exit',r));assert.equal(p.exitCode,0)}}
async function run(){
  await scenario('A full HTTP lifecycle to pending verification; assignment/capacity/history retained; public timeline scope',async()=>{
    const id=await assigned();for(const body of [{type:'ARRIVED'},{type:'PROGRESS_NOTE',note:'Fictional repair progress'}]){assert.equal((await request(`/work-orders/${id}/progress-events`,{body:{...body,expected_version:5}})).http,201);snap()}
    const before=inspect(id),summary='已更换虚构损坏部件，测试正常。\n完整维修结果 😀',r=await complete(id,summary);assert.equal(r.http,200);assert.deepEqual(r.data,{order_id:id,status:'PENDING_VERIFY',version:6,repair_summary:summary});snap();const after=inspect(id),a=after.assignments[0],log=after.status_logs.at(-1)
    assert.equal(after.repair_summary,summary);assert.equal(after.rework_count,0);assert.equal(after.completed_at,null);assert.equal(after.status,'PENDING_VERIFY');assert.equal(after.version,6)
    assert.equal(a.ended_at,null);assert.equal(a.end_reason,null);assert.equal(a.accepted_at,before.assignments[0].accepted_at);assert.equal(Number(a.version),Number(before.assignments[0].version)+1);assert.equal(a.result_submitted_at,after.updated_at);assert.deepEqual(after.worker_loads,before.worker_loads);assert.deepEqual(after.dispatch_records,before.dispatch_records);assert.deepEqual(after.status_logs.slice(0,-1),before.status_logs)
    assert.equal(log.action,'ORDER_COMPLETED');assert.equal(log.public_note,summary);assert.equal(log.assignment_id,a.id);assert.equal(log.created_at,a.result_submitted_at);assert.equal(log.request_id,r.request_id);assert.equal(log.from_status,'IN_PROGRESS');assert.equal(log.to_status,'PENDING_VERIFY');assert.equal(after.status_logs.length,before.status_logs.length+1)
    const timeline=await unchanged(`/work-orders/${id}/timeline`,200,{token:c.demo_reporter01});assert.equal(timeline.data.items.at(-1).public_note,summary);assert.equal(timeline.data.items.at(-1).action,'ORDER_COMPLETED');assert.doesNotMatch(JSON.stringify(timeline),/PRIVATE_COMPLETION_REVIEW|management_detail/)
    await unchanged(`/work-orders/${id}/timeline`,404,{token:c.demo_reporter02});await unchanged(`/work-orders/${id}/timeline`,404,{token:c.demo_sysadmin})
    const detail=await unchanged(`/work-orders/${id}`,200,{token:c.demo_reporter01});assert.equal(Object.hasOwn(detail.data,'repair_summary'),false,'API10 unchanged')
    const current=await unchanged('/work-orders?view=assigned&current_only=true&status=PENDING_VERIFY',200);assert.ok(current.data.items.some(x=>x.id===id));await unchanged(`/work-orders/${id}/complete`,409,{body:{expected_version:6,repair_summary:'Duplicate completion'}})
    samples.completed={request:{method:'POST',path:`/api/work-orders/${id}/complete`,body:{expected_version:5,repair_summary:summary}},response:r,assignment:a,load_before:before.worker_loads[0].active_tasks,load_after:after.worker_loads[0].active_tasks,completion_log:log}
  })
  await scenario('B pending transfer superseded atomically and old API22 approval conflicts',async()=>{
    const id=await assigned(),r=await apply(id);assert.equal(r.http,201);snap();const before=inspect(id);assert.equal(before.transfers[0].status,'PENDING');const done=await complete(id);assert.equal(done.http,200);snap();const after=inspect(id),t=after.transfers[0];assert.equal(t.status,'SUPERSEDED');assert.equal(t.reviewed_by,null);assert.ok(t.review_reason);assert.equal(t.resolved_at,after.assignments[0].result_submitted_at);assert.deepEqual(after.worker_loads,before.worker_loads)
    await unchanged(`/work-orders/${id}/transfer-requests/${t.id}/decision`,409,{token:c.demo_dispatcher,body:{expected_version:6,decision:'APPROVE',reason:'Stale page'}})
    const mine=await unchanged(`/transfer-requests?view=mine&order_id=${id}`,200);assert.equal(mine.data.items[0].status,'SUPERSEDED');const managed=await unchanged(`/transfer-requests?view=managed&order_id=${id}`,200,{token:c.demo_dispatcher});assert.deepEqual(managed.data.items,[]);samples.superseded={id,transfer:t,response:done}
  })
  await scenario('C four synchronized HTTP races: complete, approval, progress, application',async()=>{
    for(let m=0;m<4;m++){
      const id=await assigned();let t;if(m===1){t=await apply(id);assert.equal(t.http,201)}const release=await lock(),attempts=begins();const pending=[complete(id),m===0?complete(id,'Second concurrent result'):m===1?decide(id,t.data.transfer_request_id):m===2?request(`/work-orders/${id}/progress-events`,{body:{type:'PROGRESS_NOTE',note:'Concurrent progress',expected_version:5}}):apply(id)]
      let reached=false;for(let i=0;i<400;i++){if(begins()>=attempts+2){reached=true;break}await pause(5)}await release();const responses=await Promise.all(pending);assert.ok(reached,'both requests actually reached BEGIN before unlock');snap();const [a,b]=responses.map(r=>r.http),s=inspect(id),logs=s.status_logs.filter(x=>x.action==='ORDER_COMPLETED')
      if(m===0){assert.deepEqual([a,b].sort(),[200,409]);assert.equal(logs.length,1)}else if(m===1){assert.ok((a===200&&b===409)||(a===403&&b===200));assert.equal(logs.length,a===200?1:0)}else{assert.equal(a,200);assert.ok([201,409].includes(b));assert.equal(logs.length,1)}
      assert.equal(s.assignments.length,1);assert.equal(s.dispatch_records.length,1);samples[`race${m}`]={id,statuses:[a,b],logs:logs.length,status:s.status}
    }
  })
  await scenario('HTTP auth/current owner/state/version/Unicode/payload validation leaves all16 tables unchanged',async()=>{
    const id=await assigned();for(const token of ['', 'invalid',c.expired,c.revoked])await unchanged(`/work-orders/${id}/complete`,401,{token,body:{expected_version:5,repair_summary:'Denied'}})
    for(const [name,status] of [['demo_reporter01',403],['demo_worker02',404],['demo_dispatcher',403],['demo_sysadmin',404]])await unchanged(`/work-orders/${id}/complete`,status,{token:c[name],body:{expected_version:5,repair_summary:'Denied'}})
    mode('disable');await unchanged(`/work-orders/${id}/complete`,401,{body:{expected_version:5,repair_summary:'Disabled'}});mode('enable');await unchanged(`/work-orders/${id}/complete`,409,{body:{expected_version:4,repair_summary:'Stale'}})
    for(const repair_summary of ['', '\u00a0\u3000', '修'.repeat(2001), '\0', '\x01']){const r=await unchanged(`/work-orders/${id}/complete`,422,{body:{expected_version:5,repair_summary}});assert.equal(r.data.field,'repair_summary')}
    for(const body of [{expected_version:5},{expected_version:true,repair_summary:'Bad'},{expected_version:0,repair_summary:'Bad'},{expected_version:'5',repair_summary:'Bad'},{expected_version:5,repair_summary:null},...['actor_id','worker_id','assignment_id','status','end_reason'].map(key=>({expected_version:5,repair_summary:'No client authority',[key]:1}))])await unchanged(`/work-orders/${id}/complete`,422,{body})
    await unchanged(`/work-orders/${id}/complete`,400,{raw:'{"expected_version":5,"repair_summary":"x","repair_summary":"y"}'});await unchanged(`/work-orders/${id}/complete`,400,{body:{expected_version:5,repair_summary:'\ud800'}})
    await unchanged(`/work-orders/${id}/complete`,422,{raw:Buffer.from([123,34,114,101,112,97,105,114,95,115,117,109,109,97,114,121,34,58,34,0xc0,0x80,34,125])})
    const pending=await assigned({accept:false});await unchanged(`/work-orders/${pending}/complete`,409,{body:{expected_version:4,repair_summary:'Not accepted'}})
    const history=await assigned();mode('old-assignment',history);await unchanged(`/work-orders/${history}/complete`,403,{body:{expected_version:5,repair_summary:'Historical worker'}})
    for(const state of ['completed','closed','cancelled']){mode(state,id);await unchanged(`/work-orders/${id}/complete`,409,{body:{expected_version:5,repair_summary:'Terminal state'}})}
    const bound=await assigned();assert.equal((await complete(bound,'修'.repeat(2000))).http,200);snap();assert.equal([...inspect(bound).repair_summary].length,2000)
  })
  await scenario('all four real SQL stages fault injection return500 with complete rollback',async()=>{
    const id=await assigned();assert.equal((await apply(id)).http,201)
    for(const failure of ['fail-order','fail-assignment','fail-update-transfer','fail-log']){mode(failure);await unchanged(`/work-orders/${id}/complete`,500,{body:{expected_version:5,repair_summary:'Fictional injected failure'}});mode('clear-failure')}
  })
  await scenario('explicit isolated rework fixture is not API24, latest final result preserves old snapshot',async()=>{
    const id=await assigned();assert.equal((await complete(id,'First result')).http,200);const first=inspect(id),firstLog=first.status_logs.at(-1);mode('rework-fixture',id);await pause(5);const r=await complete(id,'Second result after fixture',7);assert.equal(r.http,200);snap();const last=inspect(id);assert.equal(last.version,8);assert.equal(last.rework_count,1);assert.deepEqual(last.status_logs[first.status_logs.length-1],firstLog);assert.notEqual(last.assignments[0].result_submitted_at,first.assignments[0].result_submitted_at);assert.deepEqual(last.worker_loads,first.worker_loads)
  })
  await scenario('real committed response loss stays unknown, GET cannot prove POST success or trigger replay',async()=>{
    const id=await assigned();replaceCredential(c.demo_worker01);let writes=0
    const flow=workflowFlow({post:async(ctx,note)=>{writes++;return completeTask(ctx.orderId,ctx.version,note)},verify:async()=>{const r=await request(`/work-orders/${id}/timeline`);return r.data.items.at(-1).action}},()=>true)
    flow.open({kind:'complete',orderId:id,version:5,status:'IN_PROGRESS'});flow.note='Client lost real committed receipt';lose=true;await flow.send();assert.equal(flow.phase,'unknown_result');assert.equal(flow.result,null);assert.equal(flow.verification,'ORDER_COMPLETED');assert.equal(inspect(id).status,'PENDING_VERIFY');assert.equal(writes,1);await assert.rejects(flow.send());assert.equal(writes,1);snap()
  })
  await scenario('actual SQLITE_BUSY503 has no partial write',async()=>{const id=await assigned(),release=await lock();try{await unchanged(`/work-orders/${id}/complete`,503,{body:{expected_version:5,repair_summary:'Busy test'}})}finally{await release()}})
}
try {
  command('--init',db,secrets,join(root,'backend/migrations'),join(root,'backend/seeds/001_demo_data.sql'));c=JSON.parse(readFileSync(secrets,'utf8'));const backendPort=await freePort()
  const p=spawn(fixture,['--serve',db,String(backendPort),trace],{windowsHide:true,stdio:['ignore','pipe','pipe']});children.push(p);p.stdout.on('data',v=>logs.push(v.toString()));p.stderr.on('data',v=>logs.push(v.toString()))
  let ready=false;for(let i=0;i<150;i++){try{if((await fetch(`http://127.0.0.1:${backendPort}/api/health`)).status===200){ready=true;break}}catch{}assert.equal(p.exitCode,null);await pause(20)}assert.ok(ready)
  vite=await createServer({configFile:join(frontend,'vite.config.js'),root:frontend,server:{host:'127.0.0.1',port:0,strictPort:true,proxy:{'/api':{target:`http://127.0.0.1:${backendPort}`}}},logLevel:'warn'});await vite.listen();const port=vite.httpServer.address().port;ports.push(port);base=`http://127.0.0.1:${port}`
  const http=axios.getAdapter('http');apiClient.defaults.adapter=async config=>{try{const r=await http({...config,baseURL:`${base}/api`});const j=typeof r.data==='string'?JSON.parse(r.data):r.data;requests.push({method:config.method,path:config.url,status:r.status,code:j.code,request_id:j.request_id});if(lose){lose=false;throw new Error('Client simulated loss after real commit')}return r}catch(e){if(e.response){const j=typeof e.response.data==='string'?JSON.parse(e.response.data):e.response.data;requests.push({method:config.method,path:config.url,status:e.response.status,code:j.code,request_id:j.request_id})}throw e}}
  await run();assert.equal(command('--integrity',db),'integrity_check=ok; foreign_key_check empty');for(const token of Object.values(c))assert.equal(logs.join('').includes(token),false);assert.equal(logs.join('').includes('Client lost real committed receipt'),false)
  // Execute the existing W2 smoke script, not a new imitation of its assertions.
  const smoke=spawn('powershell.exe',['-NoProfile','-ExecutionPolicy','Bypass','-File',join(root,'scripts/smoke.ps1'),'-BackendUrl',`http://127.0.0.1:${backendPort}`,'-FrontendUrl',base],{windowsHide:true,stdio:['ignore','pipe','pipe']});children.push(smoke);let smokeLog='';smoke.stdout.on('data',v=>{smokeLog+=v.toString()});smoke.stderr.on('data',v=>{smokeLog+=v.toString()});const smokeExit=await new Promise(r=>smoke.once('exit',r));console.log(smokeLog.trim());assert.equal(smokeExit,0)
  writeFileSync(join(root,'backend/build/Testing/w7d3-http-evidence.json'),JSON.stringify({cases,requests,snapshot_count:snapshots.length,samples,integrity:'ok',foreign_key_violations:0,smoke_exit:smokeExit,not_browser:true},null,2)+'\n')
  console.log(`PASS completion HTTP cases=${cases.length}, requests=${requests.length}, independent all16-table snapshots=${snapshots.length}; integrity=ok FK=empty; W2 smoke=0`)
} finally {
  replaceCredential();apiClient.defaults.adapter=originalAdapter;if(vite)await vite.close();for(const p of children)await stop(p)
  const path=resolve(dir);assert.ok(path.startsWith(resolve(tmpdir())+sep)&&path.includes('campusfix-w7d3-'));rmSync(path,{recursive:true,force:true})
}
for(const port of ports){const closed=await new Promise(r=>{const s=net.createConnection({host:'127.0.0.1',port});s.once('error',()=>r(true));s.once('connect',()=>{s.destroy();r(false)})});assert.ok(closed,'owned port released')}
console.log('PASS owned backend/Vite stopped, ports released, temporary DB/random sessions removed')
