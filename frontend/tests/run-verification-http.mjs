// Real production handlers and Vite, independent temporary SQLite; not browser E2E.
import assert from 'node:assert/strict'
import { spawn,spawnSync } from 'node:child_process'
import { mkdtempSync,readFileSync,writeFileSync,existsSync,rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join,resolve,dirname,sep } from 'node:path'
import { fileURLToPath } from 'node:url'
import net from 'node:net'
import { createServer } from 'vite'
import axios from 'axios'
import { apiClient } from '../src/api/client.js'
import { replaceCredential } from '../src/api/credentials.js'
import { verifyOrder } from '../src/api/verification.js'
import { verificationContext,verifyFlow } from '../src/utils/verification.js'
const frontend=resolve(dirname(fileURLToPath(import.meta.url)),'..'),root=resolve(frontend,'..'),bin=join(root,'backend/build/Debug'),fixture=join(bin,'campusfix_workflow_http_fixture.exe')
const dir=mkdtempSync(join(tmpdir(),'campusfix-w7d4-')),db=join(dir,'test.sqlite3'),secrets=join(dir,'sessions.json'),trace=join(dir,'begins.txt'),children=[],ports=[],logs=[],requests=[],snapshots=[],cases=[],samples={}
const originalAdapter=apiClient.defaults.adapter
let vite,base,c,lose=false
const pause=ms=>new Promise(r=>setTimeout(r,ms))
const command=(...args)=>{const r=spawnSync(fixture,args.map(String),{encoding:'utf8',windowsHide:true});assert.equal(r.status,0,`fixture ${args[0]}: ${r.stderr}`);return r.stdout.trim()}
const mode=(m,id=1)=>command('--mode',db,m,id),inspect=id=>JSON.parse(command('--inspect',db,id))
const snap=()=>{const s=command('--snapshot',db);snapshots.push(s);return s}
async function freePort(){const s=net.createServer();await new Promise(r=>s.listen(0,'127.0.0.1',r));const p=s.address().port;await new Promise(r=>s.close(r));ports.push(p);return p}
async function stop(p){if(p.exitCode===null&&p.signalCode===null){const done=new Promise(r=>p.once('exit',r));p.kill();await done}}
async function request(path,{token=c.demo_reporter01,body,raw,method=body===undefined&&raw===undefined?'GET':'POST'}={}){
  const r=await fetch(`${base}/api${path}`,{method,headers:{'Content-Type':'application/json',...(token?{Authorization:`Bearer ${token}`}:{})},...(body!==undefined||raw!==undefined?{body:raw??JSON.stringify(body)}:{})});const j=await r.json()
  assert.match(r.headers.get('content-type'),/application\/json/);assert.match(j.request_id,/^req_[\w-]+$/);assert.ok(Object.hasOwn(j,'message')&&Object.hasOwn(j,'data')&&Object.hasOwn(j,'code'));assert.doesNotMatch(JSON.stringify(j),/password_hash|token_hash|stored_path|metadata_json|internal_note|PRIVATE_WORKFLOW_FAILURE/)
  requests.push({method,path,status:r.status,code:j.code,request_id:j.request_id});return {http:r.status,...j}
}
async function unchanged(path,status,options={}){const before=snap(),r=await request(path,options);assert.equal(r.http,status,`${path} ${r.code}`);assert.equal(snap(),before,'GET/rejection leaves16table contents unchanged');return r}
async function scenario(name,fn){await fn();cases.push(name);console.log(`PASS ${name}`)}
async function ready({owner='demo_reporter01',extraProgress=0}={}){
  const r=await request('/work-orders',{token:c[owner],body:{category_id:1,location_id:11,title:'Fictional verify test',description:'Fictional fault',requested_urgency:'NORMAL',contact_name:'Fictional user',contact_phone:'00000000000',expected_visit_at:null}});assert.equal(r.http,201);const id=r.data.id;snap()
  for(const [path,token,body] of [[`/${id}/submit`,c[owner],{expected_version:1}],[`/${id}/review`,c.demo_dispatcher,{action:'APPROVE',expected_version:2,category_id:1,approved_urgency:'HIGH'}],[`/${id}/assign`,c.demo_dispatcher,{worker_id:1,expected_version:3}],[`/${id}/accept`,c.demo_worker01,{expected_version:4}]]){assert.equal((await request(`/work-orders${path}`,{token,body})).http,200);snap()}
  for(let i=0;i<extraProgress;i++)assert.equal((await request(`/work-orders/${id}/progress-events`,{token:c.demo_worker01,body:{type:'PROGRESS_NOTE',note:`Fictional progress ${i}`,expected_version:5}})).http,201)
  assert.equal((await complete(id,'Result A')).http,200);snap();return id
}
const complete=(id,summary,version=5)=>request(`/work-orders/${id}/complete`,{token:c.demo_worker01,body:{expected_version:version,repair_summary:summary}})
const verify=(id,approved=true,version=6,reason=approved?undefined:'Fictional concurrent rework reason',token=c.demo_reporter01)=>request(`/work-orders/${id}/verify`,{token,body:{approved,expected_version:version,...(reason===undefined?{}:{reason})}})
const begins=()=>existsSync(trace)?readFileSync(trace,'utf8').split('\n').filter(Boolean).length:0
async function lock(){const readyFile=join(dir,`ready-${children.length}`),release=join(dir,`release-${children.length}`),p=spawn(join(bin,'campusfix_dispatch_http_fixture.exe'),['--hold',db,readyFile,release,'15'],{windowsHide:true,stdio:'ignore'});children.push(p);for(let i=0;i<300&&!existsSync(readyFile);i++)await pause(5);assert.ok(existsSync(readyFile));return async()=>{writeFileSync(release,'release');if(p.exitCode===null)await new Promise(r=>p.once('exit',r));assert.equal(p.exitCode,0)}}
async function run(){
  await scenario('A complete -> verify pass atomically ends original assignment and releases capacity',async()=>{
    const id=await ready(),before=inspect(id),r=await verify(id);assert.equal(r.http,200);assert.deepEqual(r.data,{order_id:id,approved:true,status:'COMPLETED',version:7,rework_count:0});snap();const after=inspect(id),a=after.assignments[0],log=after.status_logs.at(-1)
    assert.equal(after.status,'COMPLETED');assert.equal(after.completed_at,a.ended_at);assert.equal(after.updated_at,a.ended_at);assert.equal(a.end_reason,'COMPLETED');assert.equal(Number(a.version),Number(before.assignments[0].version)+1);assert.equal(a.accepted_at,before.assignments[0].accepted_at);assert.equal(a.result_submitted_at,before.assignments[0].result_submitted_at);assert.equal(after.worker_loads[0].active_tasks,before.worker_loads[0].active_tasks-1);assert.equal(after.assignments.filter(x=>x.ended_at===null).length,0);assert.equal(after.repair_summary,'Result A');assert.deepEqual(after.dispatch_records,before.dispatch_records);assert.deepEqual(after.status_logs.slice(0,-1),before.status_logs)
    assert.equal(log.action,'ORDER_VERIFIED');assert.equal(log.assignment_id,a.id);assert.equal(log.request_id,r.request_id);assert.equal(log.created_at,after.completed_at);assert.equal(log.from_status,'PENDING_VERIFY');assert.equal(log.to_status,'COMPLETED')
    await unchanged(`/work-orders/${id}/verify`,409,{body:{approved:true,expected_version:7}});const metrics=JSON.parse(command('--metrics',db,id))[0];assert.equal(metrics.active_tasks,after.worker_loads[0].active_tasks);assert.ok(metrics.samples.some(s=>s.assignment_id===Number(a.id)));samples.pass={id,response:r,before,after,metrics}
  })
  await scenario('B real two-round repair -> rework -> resubmit -> pass, no SQL state shortcuts',async()=>{
    const id=await ready({extraProgress:101}),before=inspect(id),r=await verify(id,false,6,'Fictional socket still broken');assert.equal(r.http,200);snap();const rework=inspect(id);assert.equal(rework.status,'IN_PROGRESS');assert.equal(rework.rework_count,1);assert.equal(rework.version,7);assert.equal(rework.completed_at,null);assert.deepEqual(rework.assignments,before.assignments);assert.deepEqual(rework.worker_loads,before.worker_loads);assert.equal(rework.repair_summary,'Result A');assert.equal(rework.status_logs.at(-1).public_note,'Fictional socket still broken');assert.equal(rework.status_logs.at(-1).action,'ORDER_REWORK_REQUESTED')
    const assigned=await unchanged('/work-orders?view=assigned&current_only=true&status=IN_PROGRESS',200,{token:c.demo_worker01});assert.ok(assigned.data.items.some(x=>x.id===id));await pause(5);assert.equal((await complete(id,'Result B',7)).http,200);snap();const second=inspect(id);assert.equal(second.repair_summary,'Result B');assert.notEqual(second.assignments[0].result_submitted_at,before.assignments[0].result_submitted_at);assert.equal(second.rework_count,1);assert.deepEqual(second.worker_loads,before.worker_loads)
    const readonly=apiFor();const context=await verificationContext(id,'reporter',readonly);assert.equal(context.result.public_note,'Result B');assert.equal(context.reworks,1);assert.equal(context.version,8);assert.ok(context.items.length>100,'actual timeline traverses page boundary')
    const done=await verify(id,true,8);assert.equal(done.http,200);snap();const final=inspect(id);assert.equal(final.version,9);assert.equal(final.rework_count,1);assert.equal(final.status,'COMPLETED');assert.deepEqual(final.dispatch_records,before.dispatch_records);assert.deepEqual(final.status_logs.filter(x=>x.action==='ORDER_COMPLETED').map(x=>x.public_note),['Result A','Result B']);assert.equal(final.assignments[0].result_submitted_at,second.assignments[0].result_submitted_at)
    const metrics=JSON.parse(command('--metrics',db,id))[0],sample=metrics.samples.find(s=>s.assignment_id===Number(final.assignments[0].id));assert.ok(sample);assert.ok(Math.abs(sample.actual_minutes-(Date.parse(second.assignments[0].result_submitted_at)-Date.parse(second.assignments[0].accepted_at))/60000)<0.0001);samples.rework={id,response:r,before,rework,second,final,metrics}
  })
  await scenario('C D017 worker original reporter accepts independent worker repair; repair identity alone denied',async()=>{
    const id=await ready({owner:'demo_worker02'});await unchanged(`/work-orders/${id}/verify`,403,{token:c.demo_worker01,body:{approved:true,expected_version:6}});const r=await verify(id,false,6,'Worker own report still faulty',c.demo_worker02);assert.equal(r.http,200);assert.equal((await complete(id,'Worker-owned result B',7)).http,200);const done=await verify(id,true,8,undefined,c.demo_worker02);assert.equal(done.http,200);snap();samples.worker_owned={id,response:done}
  })
  await scenario('D real session/role/scope/payload/Unicode/state/version boundaries fail closed',async()=>{
    const id=await ready();for(const token of ['', 'invalid',c.expired,c.revoked])await unchanged(`/work-orders/${id}/verify`,401,{token,body:{approved:true,expected_version:6}})
    mode('admin-read');mode('admin-dispatch');for(const [name,status] of [['demo_worker01',403],['demo_worker02',404],['demo_reporter02',404],['demo_dispatcher',403],['demo_sysadmin',403]])await unchanged(`/work-orders/${id}/verify`,status,{token:c[name],body:{approved:true,expected_version:6}})
    mode('disable-reporter');await unchanged(`/work-orders/${id}/verify`,401,{body:{approved:true,expected_version:6}});mode('enable-reporter');await unchanged(`/work-orders/${id}/verify`,409,{body:{approved:true,expected_version:5}})
    for(const body of [{expected_version:6},{approved:1,expected_version:6},{approved:'true',expected_version:6},{approved:false,expected_version:6},{approved:true,expected_version:true},{approved:true,expected_version:'6'},...['reporter_id','worker_id','assignment_id','completed_at','status'].map(k=>({approved:true,expected_version:6,[k]:1}))])await unchanged(`/work-orders/${id}/verify`,422,{body})
    for(const reason of ['', '\u00a0\u3000','修'.repeat(501),'\0','\x01']){const r=await unchanged(`/work-orders/${id}/verify`,422,{body:{approved:false,expected_version:6,reason}});assert.equal(r.data.field,'reason')}
    await unchanged(`/work-orders/${id}/verify`,400,{raw:'{"approved":true,"approved":false,"expected_version":6}'});await unchanged(`/work-orders/${id}/verify`,400,{body:{approved:false,expected_version:6,reason:'\ud800'}});await unchanged(`/work-orders/${id}/verify`,422,{raw:Buffer.from([123,34,114,101,97,115,111,110,34,58,34,0xc0,0x80,34,125])})
    const other=await ready();mode('foreign',other);await unchanged(`/work-orders/${other}/verify`,404,{token:c.demo_dispatcher,body:{approved:true,expected_version:6}});const historic=await ready();mode('old-assignment',historic);await unchanged(`/work-orders/${historic}/verify`,403,{token:c.demo_worker01,body:{approved:true,expected_version:6}})
    for(const m of ['version-limit','rework-limit','assignment-limit']){const x=await ready();mode(m,x);await unchanged(`/work-orders/${x}/verify`,409,{raw:`{"approved":${m!=='rework-limit'},"reason":"Overflow","expected_version":${m==='version-limit'?'9223372036854775807':'6'}}`})}
    const border=await ready();assert.equal((await verify(border,false,6,'修'.repeat(500))).http,200);snap();await unchanged(`/work-orders/${border}/verify`,409,{body:{approved:true,expected_version:7}})
  })
  await scenario('five real synchronized HTTP competitions preserve one verification log',async()=>{
    for(let m=0;m<5;m++){const id=await ready(),release=await lock(),attempts=begins(),pending=[verify(id,m!==2),m===3?complete(id,'Duplicate complete',6):m===4?request(`/work-orders/${id}/progress-events`,{token:c.demo_worker01,body:{type:'PROGRESS_NOTE',expected_version:6,note:'Late progress'}}):verify(id,m===0)]
      let reached=false;for(let i=0;i<400;i++){if(begins()>=attempts+2){reached=true;break}await pause(5)}await release();const r=await Promise.all(pending);assert.ok(reached);const statuses=r.map(x=>x.http);if(m<3)assert.deepEqual(statuses.slice().sort(),[200,409]);else{assert.equal(statuses[0],200);assert.ok([403,409].includes(statuses[1]))}snap();const s=inspect(id);assert.equal(s.status_logs.filter(x=>['ORDER_VERIFIED','ORDER_REWORK_REQUESTED'].includes(x.action)).length,1);assert.equal(s.assignments.length,1);assert.equal(s.dispatch_records.length,1);samples[`race${m}`]={id,statuses,status:s.status,reworks:s.rework_count}}
  })
  await scenario('work-order/assignment/audit write failures roll back all16 tables',async()=>{
    for(const approved of [true,false]){const id=await ready();for(const failure of approved?['fail-order','fail-assignment','fail-log']:['fail-order','fail-log']){mode(failure);await unchanged(`/work-orders/${id}/verify`,500,{body:{approved,expected_version:6,reason:'Fault injection'}});mode('clear-failure')}}
  })
  await scenario('lost real committed receipt remains unknown and authorized GET cannot prove POST success',async()=>{
    const id=await ready();replaceCredential(c.demo_reporter01);let writes=0;const flow=verifyFlow({...apiFor(),post:async(...args)=>{writes++;return verifyOrder(...args)}},()=>true);assert.equal(await flow.load(id,'reporter'),true);flow.choose(true);lose=true;await flow.send();assert.equal(flow.phase,'unknown_result');assert.equal(flow.receipt,null);assert.equal(flow.verification.status,'COMPLETED');assert.equal(inspect(id).status,'COMPLETED');await assert.rejects(flow.send());assert.equal(writes,1);snap()
  })
  await scenario('actual SQLITE_BUSY503 has no partial verification',async()=>{const id=await ready(),release=await lock();try{await unchanged(`/work-orders/${id}/verify`,503,{body:{approved:true,expected_version:6}})}finally{await release()}})
}
function apiFor(){return {reported:async q=>(await request(`/work-orders?view=reported&page=${q.page}&page_size=${q.page_size}`)).data,detail:async id=>(await request(`/work-orders/${id}`)).data,timeline:async(id,q)=>(await request(`/work-orders/${id}/timeline?page=${q.page}&page_size=${q.page_size}`)).data}}
try{
  command('--init',db,secrets,join(root,'backend/migrations'),join(root,'backend/seeds/001_demo_data.sql'));c=JSON.parse(readFileSync(secrets,'utf8'));const backendPort=await freePort();const p=spawn(fixture,['--serve',db,String(backendPort),trace],{windowsHide:true,stdio:['ignore','pipe','pipe']});children.push(p);p.stdout.on('data',v=>logs.push(v.toString()));p.stderr.on('data',v=>logs.push(v.toString()));let readyServer=false;for(let i=0;i<150;i++){try{if((await fetch(`http://127.0.0.1:${backendPort}/api/health`)).status===200){readyServer=true;break}}catch{}assert.equal(p.exitCode,null);await pause(20)}assert.ok(readyServer)
  vite=await createServer({configFile:join(frontend,'vite.config.js'),root:frontend,server:{host:'127.0.0.1',port:0,strictPort:true,proxy:{'/api':{target:`http://127.0.0.1:${backendPort}`}}},logLevel:'warn'});await vite.listen();const port=vite.httpServer.address().port;ports.push(port);base=`http://127.0.0.1:${port}`
  const http=axios.getAdapter('http');apiClient.defaults.adapter=async config=>{const r=await http({...config,baseURL:`${base}/api`});const j=typeof r.data==='string'?JSON.parse(r.data):r.data;requests.push({method:config.method,path:config.url,status:r.status,code:j.code,request_id:j.request_id});if(lose){lose=false;throw new Error('Client response loss after real database commit')}return r}
  await run();assert.equal(command('--integrity',db),'integrity_check=ok; foreign_key_check empty');for(const token of Object.values(c))assert.equal(logs.join('').includes(token),false)
  const smoke=spawn('powershell.exe',['-NoProfile','-ExecutionPolicy','Bypass','-File',join(root,'scripts/smoke.ps1'),'-BackendUrl',`http://127.0.0.1:${backendPort}`,'-FrontendUrl',base],{windowsHide:true,stdio:['ignore','pipe','pipe']});children.push(smoke);let smokeLog='';smoke.stdout.on('data',v=>{smokeLog+=v});smoke.stderr.on('data',v=>{smokeLog+=v});const exit=await new Promise(r=>smoke.once('exit',r));console.log(smokeLog.trim());assert.equal(exit,0)
  writeFileSync(join(root,'backend/build/Testing/w7d4-http-evidence.json'),JSON.stringify({cases,requests,snapshot_count:snapshots.length,samples,integrity:'ok',foreign_key_violations:0,smoke_exit:exit,not_browser:true},null,2)+'\n');console.log(`PASS verification cases=${cases.length} HTTP=${requests.length} independent16tableSnapshots=${snapshots.length}; integrity=ok FK=empty W2 smoke=0`)
}finally{replaceCredential();apiClient.defaults.adapter=originalAdapter;if(vite)await vite.close();for(const p of children)await stop(p);const path=resolve(dir);assert.ok(path.startsWith(resolve(tmpdir())+sep)&&path.includes('campusfix-w7d4-'));rmSync(path,{recursive:true,force:true})}
for(const port of ports){const closed=await new Promise(r=>{const s=net.createConnection({host:'127.0.0.1',port});s.once('error',()=>r(true));s.once('connect',()=>{s.destroy();r(false)})});assert.ok(closed)}console.log('PASS owned servers/ports and temporary DB/random sessions cleaned')
