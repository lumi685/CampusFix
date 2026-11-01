// Real frontend API/flow -> Vite proxy -> production Drogon handlers -> SQLite.
// This is HTTP integration, not a browser visual/DOM test.
import assert from 'node:assert/strict'
import { spawn, spawnSync } from 'node:child_process'
import { mkdtempSync, readFileSync, writeFileSync, existsSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, resolve, dirname, sep } from 'node:path'
import { fileURLToPath } from 'node:url'
import net from 'node:net'
import axios from 'axios'
import { createServer } from 'vite'
import { apiClient } from '../src/api/client.js'
import { replaceCredential } from '../src/api/credentials.js'
import { workerTasks, taskAction } from '../src/api/workerTasks.js'
import { orderDetail } from '../src/api/orders.js'
import { workerTaskFlow } from '../src/utils/workerTasks.js'

const frontend=resolve(dirname(fileURLToPath(import.meta.url)),'..'),root=resolve(frontend,'..'),bin=join(root,'backend/build/Debug')
const fixture=join(bin,'campusfix_worker_task_http_fixture.exe'),holder=join(bin,'campusfix_dispatch_http_fixture.exe')
const dir=mkdtempSync(join(tmpdir(),'campusfix-w7d1-')),db=join(dir,'test.sqlite3'),secrets=join(dir,'sessions.json'),trace=join(dir,'begins.txt'),children=[],logs=[],requests=[],snapshots=[],cases=[],chains=[],ports=[]
const original=apiClient.defaults.adapter,http=axios.getAdapter('http')
let vite,base,c,lose=false
const pause=ms=>new Promise(r=>setTimeout(r,ms))
const command=(...args)=>{const p=spawnSync(fixture,args.map(String),{encoding:'utf8',windowsHide:true});assert.equal(p.status,0,'independent C++ fixture');return p.stdout.trim()}
const mode=(m,id=1)=>command('--mode',db,m,id)
const inspect=id=>JSON.parse(command('--inspect',db,id))
const snap=()=>{const s=command('--snapshot',db);snapshots.push(s);return s}
async function freePort(){const s=net.createServer();await new Promise(r=>s.listen(0,'127.0.0.1',r));const p=s.address().port;await new Promise(r=>s.close(r));ports.push(p);return p}
async function stop(p){if(p.exitCode===null&&p.signalCode===null){const done=new Promise(r=>p.once('exit',r));p.kill();await done}}
async function request(path,{token=c.demo_worker01,body,raw,method=body===undefined&&raw===undefined?'GET':'POST'}={}){
  const r=await fetch(`${base}/api${path}`,{method,headers:{'Content-Type':'application/json',...(token?{Authorization:`Bearer ${token}`}:{})},...(body!==undefined||raw!==undefined?{body:raw??JSON.stringify(body)}:{})})
  const j=await r.json();assert.match(r.headers.get('content-type'),/json/);assert.equal(typeof j.request_id,'string');assert.ok(j.request_id);assert.ok(Object.hasOwn(j,'code')&&Object.hasOwn(j,'message')&&Object.hasOwn(j,'data'))
  assert.doesNotMatch(JSON.stringify(j),/password_hash|token_hash|stored_path|metadata_json|PRIVATE_TASK_STORAGE_FAILURE/)
  requests.push({method,path,status:r.status,code:j.code,request_id:j.request_id});return {status:r.status,...j}
}
async function denied(path,status,options){const before=snap();const r=await request(path,options);assert.equal(r.status,status);assert.equal(snap(),before,'rejected HTTP leaves all16 tables unchanged');return r}
async function assigned(){const id=Number(command('--new',db,secrets));const r=await request(`/work-orders/${id}/assign`,{token:c.demo_dispatcher,body:{worker_id:1,expected_version:3}});assert.equal(r.status,200);assert.equal(r.data.status,'PENDING_ACCEPT');assert.equal(r.data.version,4);return id}
const flow=()=>workerTaskFlow({list:workerTasks,action:taskAction,detail:orderDetail},()=>true)
async function scenario(name,fn){await fn();cases.push(name);console.log(`PASS ${name}`)}
const begins=()=>existsSync(trace)?readFileSync(trace,'utf8').split('\n').filter(Boolean).length:0
async function lock(){const ready=join(dir,`ready-${children.length}`),release=join(dir,`release-${children.length}`);const p=spawn(holder,['--hold',db,ready,release,'15'],{windowsHide:true,stdio:'ignore'});children.push(p);for(let i=0;i<200&&!existsSync(ready);i++)await pause(5);assert.ok(existsSync(ready),'independent SQLite write lock ready');return async()=>{writeFileSync(release,'release');if(p.exitCode===null)await new Promise(r=>p.once('exit',r));assert.equal(p.exitCode,0)}}
try {
  command('--init',db,secrets,join(root,'backend/migrations'),join(root,'backend/seeds/001_demo_data.sql'));c=JSON.parse(readFileSync(secrets,'utf8'))
  const backendPort=await freePort();const server=spawn(fixture,['--serve',db,String(backendPort),trace],{windowsHide:true,stdio:['ignore','pipe','pipe']});children.push(server);server.stdout.on('data',v=>logs.push(v.toString()));server.stderr.on('data',v=>logs.push(v.toString()))
  let ready=false;for(let i=0;i<150;i++){try{if((await fetch(`http://127.0.0.1:${backendPort}/api/health`)).status===200){ready=true;break}}catch{}assert.equal(server.exitCode,null);await pause(20)}assert.ok(ready,'four-thread production-handler server starts')
  vite=await createServer({configFile:join(frontend,'vite.config.js'),root:frontend,server:{host:'127.0.0.1',port:0,strictPort:true,proxy:{'/api':{target:`http://127.0.0.1:${backendPort}`}}},logLevel:'warn'});await vite.listen();const port=vite.httpServer.address().port;ports.push(port);base=`http://127.0.0.1:${port}`
  apiClient.defaults.adapter=async config=>{try{const r=await http({...config,baseURL:`${base}/api`});const j=typeof r.data==='string'?JSON.parse(r.data):r.data;requests.push({method:config.method,path:config.url,status:r.status,code:j.code,request_id:j.request_id});if(lose&&/\/(accept|refuse)$/.test(config.url)){lose=false;throw new Error('Simulated client response loss after real commit')}return r}catch(e){if(e.response){const j=typeof e.response.data==='string'?JSON.parse(e.response.data):e.response.data;requests.push({method:config.method,path:config.url,status:e.response.status,code:j.code,request_id:j.request_id})}throw e}}
  replaceCredential(c.demo_worker01)
  await scenario('HTTP chain B: real assign -> refuse, load 1 to 0, audit/history/transfer retained',async()=>{
    const id=await assigned();assert.equal(inspect(id).worker_loads[0].active_tasks,1);mode('pending-transfer',id)
    const r=await request(`/work-orders/${id}/refuse`,{body:{expected_version:4,reason:'Fictional HTTP refusal'}});assert.equal(r.status,200);assert.deepEqual(r.data,{order_id:id,status:'PENDING_DISPATCH',version:5,assignment_ended:true})
    const s=inspect(id),a=s.assignments[0],l=s.status_logs.at(-1);assert.equal(s.status,'PENDING_DISPATCH');assert.equal(s.version,5);assert.equal(a.version,'2');assert.equal(a.end_reason,'REFUSED');assert.ok(a.ended_at.endsWith('Z'));assert.equal(a.accepted_at,null);assert.equal(s.worker_loads[0].active_tasks,0);assert.equal(s.assignments.length,1);assert.equal(s.dispatch_records.length,1);assert.equal(l.assignment_id,a.id);assert.equal(l.action,'ORDER_REFUSED');assert.equal(l.public_note,'Fictional HTTP refusal');assert.equal(l.request_id,r.request_id);assert.equal(s.transfers[0].status,'SUPERSEDED');assert.equal(s.transfers[0].reviewed_by_null,true);assert.ok(s.transfers[0].review_reason&&s.transfers[0].resolved_at)
    await denied(`/work-orders/${id}/refuse`,409,{body:{expected_version:4,reason:'Replay'}})
    const preview=await request(`/work-orders/${id}/dispatch-candidates`,{token:c.demo_dispatcher});assert.equal(preview.status,200);assert.equal(preview.data.top_candidates[0].worker_id,1);assert.equal(preview.data.top_candidates[0].active_tasks,0);chains.push({id,response:r.data,assignment_id:Number(a.id),load_before:1,load_after:0})
  })
  await scenario('HTTP chain A: real assign -> frontend accept, current list/readback IN_PROGRESS',async()=>{
    const id=await assigned(),f=flow();await f.load();assert.ok(f.items.some(t=>t.id===id));assert.equal(f.choose(id,'accept'),true);const result=await f.send();assert.equal(result.status,'IN_PROGRESS');assert.equal(result.version,5);assert.equal(f.status,'IN_PROGRESS');assert.ok(f.items.some(t=>t.id===id&&t.status==='IN_PROGRESS'))
    const s=inspect(id),a=s.assignments[0],l=s.status_logs.at(-1);assert.equal(a.accepted_at,result.accepted_at);assert.equal(a.ended_at,null);assert.equal(a.version,'2');assert.equal(s.assignments.length,1);assert.equal(s.worker_loads[0].active_tasks,1);assert.equal(l.assignment_id,a.id);assert.equal(l.action,'ORDER_ACCEPTED');assert.equal(l.request_id,requests.findLast(r=>r.path===`/work-orders/${id}/accept`).request_id);await denied(`/work-orders/${id}/accept`,409,{body:{expected_version:4}});chains.push({id,response:result,assignment_id:Number(a.id),load_before:1,load_after:1})
  })
  await scenario('HTTP strict schema, int64/Unicode bounds and safe 400/422 failures',async()=>{
    const id=await assigned(),path=`/work-orders/${id}`
    for(const body of [{},{expected_version:0},{expected_version:-1},{expected_version:1.2},{expected_version:'4'},{expected_version:true},{expected_version:4,worker_id:1},{expected_version:4,reason:'not permitted'}])await denied(`${path}/accept`,422,{body})
    for(const raw of ['[]','null','{"expected_version":4,"expected_version":4}','{"expected_version":4,}','{"expected_version":4} true'])await denied(`${path}/accept`,400,{raw})
    await denied(`${path}/accept`,422,{raw:'{"expected_version":9223372036854775808}'})
    for(const reason of ['', '　\u00a0\n', 'x'.repeat(501), '\u0000'])await denied(`${path}/refuse`,422,{body:{expected_version:4,reason}})
    // JsonCpp rejects an unpaired surrogate during strict JSON parsing (400),
    // before service-level UTF-8/field validation (422).
    await denied(`${path}/refuse`,400,{body:{expected_version:4,reason:'\ud800'}})
    await denied(`${path}/accept`,409,{body:{expected_version:3}})
    const r=await request(`${path}/refuse`,{body:{expected_version:4,reason:'😀'.repeat(500)}});assert.equal(r.status,200);assert.equal([...inspect(id).status_logs.at(-1).public_note].length,500)
  })
  await scenario('HTTP authentication/current-owner/historical-owner/manager boundary and current-only pagination',async()=>{
    const id=await assigned(),path=`/work-orders/${id}/accept`
    for(const token of ['', 'invalid',c.expired,c.revoked])await denied(path,401,{token,body:{expected_version:4}})
    mode('disable');await denied(path,401,{body:{expected_version:4}});mode('enable')
    await denied(path,404,{token:c.demo_worker02,body:{expected_version:4}})
    for(const token of [c.demo_reporter01,c.demo_dispatcher,c.demo_sysadmin])await denied(path,403,{token,body:{expected_version:4}})
    mode('history',id);await denied(path,403,{token:c.demo_worker02,body:{expected_version:4}});await denied(`/work-orders/${id}/refuse`,403,{token:c.demo_worker02,body:{expected_version:4,reason:'Historical cannot act'}})
    const before=snap(),history=await request('/work-orders?view=assigned&status=PENDING_ACCEPT',{token:c.demo_worker02}),current=await request('/work-orders?view=assigned&current_only=true&status=PENDING_ACCEPT',{token:c.demo_worker02});assert.ok(history.data.items.some(o=>o.id===id));assert.equal(current.data.total,0);assert.deepEqual(current.data.items,[]);assert.equal(snap(),before)
    for(const path of ['/work-orders?view=reported&current_only=true','/work-orders?view=assigned&current_only=1'])await denied(path,422,{})
  })
  await scenario('HTTP storage fault injection rolls back accept/refuse across all16 tables',async()=>{
    for(const action of ['accept','refuse'])for(const failure of ['fail-assignment','fail-order','fail-log',...(action==='refuse'?['fail-transfer']:[])]){
      const id=await assigned();if(action==='refuse')mode('pending-transfer',id);mode(failure,id)
      await denied(`/work-orders/${id}/${action}`,500,{body:{expected_version:4,...(action==='refuse'?{reason:'Fictional rollback'}:{})}});mode('clear-failure')
    }
  })
  await scenario('HTTP real concurrent accept/refuse with two BEGIN attempts blocked behind third connection',async()=>{
    for(const actions of [['accept','accept'],['refuse','refuse'],['accept','refuse']]){
      const id=await assigned(),release=await lock(),before=begins();const pending=actions.map(action=>request(`/work-orders/${id}/${action}`,{body:{expected_version:4,...(action==='refuse'?{reason:'Fictional race'}:{})}}))
      let reached=false;for(let i=0;i<300;i++){if(begins()>=before+2){reached=true;break}await pause(5)}await release();const results=await Promise.all(pending);assert.ok(reached,'both independent HTTP transactions attempted before lock release');assert.deepEqual(results.map(r=>r.status).sort(),[200,409]);const s=inspect(id);assert.equal(s.version,5);assert.equal(s.assignments.length,1);assert.equal(s.assignments[0].version,'2');assert.equal(s.status_logs.filter(l=>['ORDER_ACCEPTED','ORDER_REFUSED'].includes(l.action)).length,1)
    }
  })
  await scenario('HTTP genuine SQLITE_BUSY returns safe503 and no writes',async()=>{
    const id=await assigned(),release=await lock(),before=snap();const r=await request(`/work-orders/${id}/accept`,{body:{expected_version:4}});await release();assert.equal(r.status,503);assert.equal(r.code,'DATABASE_BUSY');assert.equal(snap(),before)
  })
  await scenario('frontend real refusal disappears; committed response-loss uses GET and never replays',async()=>{
    const id=await assigned(),f=flow();await f.load();f.choose(id,'refuse');f.reason='Frontend actual refusal';await f.send();assert.equal(f.status,'PENDING_ACCEPT');assert.ok(!f.items.some(o=>o.id===id));assert.equal(inspect(id).status,'PENDING_DISPATCH')
    const lost=await assigned(),u=flow();await u.load();u.choose(lost,'accept');lose=true;await u.send();assert.equal(u.phase,'unknown_result');assert.equal(u.verification.status,'IN_PROGRESS');assert.equal(inspect(lost).status,'IN_PROGRESS');const count=requests.length;await assert.rejects(u.send());await u.load();assert.equal(requests.length,count)
  })
  assert.equal(command('--integrity',db),'integrity_check=ok; foreign_key_check empty')
  for(const token of Object.values(c))assert.equal(logs.join('').includes(token),false,'no fixture sessions in application logs')
  writeFileSync(join(root,'backend/build/Testing/w7d1-http-evidence.json'),JSON.stringify({cases,requests,sqlite_snapshots:snapshots.length,chains,not_browser_e2e:true,transport_loss:'Client simulated after real server commit'},null,2)+'\n')
  console.log(`PASS worker HTTP cases=${cases.length} requests=${requests.length} independent SQLite fingerprints=${snapshots.length}; integrity_check=ok, foreign_key_check empty`)
} finally {
  replaceCredential();apiClient.defaults.adapter=original;if(vite)await vite.close();for(const p of children)await stop(p)
  const path=resolve(dir);assert.ok(path.startsWith(resolve(tmpdir())+sep)&&path.includes('campusfix-w7d1-'));rmSync(path,{recursive:true,force:true})
}
for(const port of ports){const closed=await new Promise(r=>{const s=net.createConnection({host:'127.0.0.1',port});s.once('error',()=>r(true));s.once('connect',()=>{s.destroy();r(false)})});assert.ok(closed,'owned listener released')}
console.log('PASS temporary SQLite/credentials removed, owned server/Vite stopped and listeners released')
