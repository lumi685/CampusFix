// Actual frontend API + flow → real Vite proxy → production Drogon → SQLite.
// The two dropped responses below intentionally simulate CLIENT transport loss
// AFTER a real successful server request; they are not mock server responses.
import { test, after } from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync, writeFileSync } from 'node:fs'
import { spawnSync } from 'node:child_process'
import axios from 'axios'
import { apiClient } from '../src/api/client.js'
import { credentialSnapshot, replaceCredential } from '../src/api/credentials.js'
import { signIn, signOut, authState } from '../src/stores/auth.js'
import { meRequest } from '../src/api/auth.js'
import { managedOrders, orderDetail } from '../src/api/orders.js'
import { getDispatchCandidates, assignWorker } from '../src/api/dispatch.js'
import { dispatchFlow, emptyDispatchText } from '../src/utils/dispatch.js'
import { managementAbility } from '../src/utils/adminOrders.js'
const base=process.env.CAMPUSFIX_DISPATCH_TEST_BASE, secrets=process.env.CAMPUSFIX_DISPATCH_TEST_CREDENTIALS, db=process.env.CAMPUSFIX_DISPATCH_TEST_DB, fixture=process.env.CAMPUSFIX_DISPATCH_TEST_FIXTURE, uiFixture=process.env.CAMPUSFIX_DISPATCH_UI_FIXTURE
if(!/^http:\/\/127\.0\.0\.1:\d{1,5}$/.test(base||'')||!secrets||!db||!fixture||!uiFixture)throw new Error('Run npm run test:dispatch:http to provision isolated fixtures first')
const c=JSON.parse(readFileSync(secrets,'utf8')), original=apiClient.defaults.adapter, http=axios.getAdapter('http'), requests=[], verified=[], fingerprints=[], samples={}
const envelope=body=>typeof body==='string'?JSON.parse(body):body
let loseNext=false
apiClient.defaults.adapter=async config=>{
  try{const r=await http({...config,baseURL:`${base}/api`});const j=envelope(r.data);requests.push({method:config.method,path:config.url,status:r.status,code:j?.code,request_id:j?.request_id})
    if(loseNext&&config.url.endsWith('/assign')){loseNext=false;throw Object.assign(new Error('Client-side simulated response loss'),{code:'ECONNABORTED',config})}return r
  }catch(e){if(e.response){const j=envelope(e.response.data);requests.push({method:config.method,path:config.url,status:e.response.status,code:j?.code,request_id:j?.request_id})}throw e}
}
const command=(exe,...args)=>{const p=spawnSync(exe,args.map(String),{encoding:'utf8',windowsHide:true});assert.equal(p.status,0,'independent C++ SQLite fixture');return p.stdout.trim()}
const snap=()=>{const s=command(fixture,'--snapshot',db);fingerprints.push(s);return s}
const mode=m=>command(fixture,'--mode',db,m)
const inspect=id=>JSON.parse(command(fixture,'--inspect',db,id))
const flow=()=>dispatchFlow({candidates:getDispatchCandidates,assign:assignWorker,detail:orderDetail},()=>authState.status==='authenticated'&&managementAbility(authState.user,'BUSINESS_READ'))
function persisted(id,selected,reason){const i=inspect(id),a=i.assignments[0],r=i.dispatch_records[0],l=i.status_logs.at(-1);assert.equal(i.status,'PENDING_ACCEPT');assert.equal(i.version,4);assert.equal(i.assignments.length,1);assert.equal(i.dispatch_records.length,1);assert.equal(a.worker_id,String(selected));assert.equal(r.assignment_id,a.id);assert.equal(l.assignment_id,a.id);assert.equal(l.action,'ORDER_ASSIGNED');assert.equal(r.order_version,'3');assert.equal(JSON.parse(r.candidates_json).length,6);assert.equal(l.request_id,requests.findLast(r=>r.path===`/work-orders/${id}/assign`&&r.status===200).request_id);if(reason)assert.equal(a.assign_reason,reason);verified.push({order_id:id,assignment_id:Number(a.id),status:i.status,version:i.version,worker_id:selected,snapshot_assignment_id:Number(r.assignment_id),audit_assignment_id:Number(l.assignment_id)});return i}
after(async()=>{replaceCredential();apiClient.defaults.adapter=original;writeFileSync(process.env.CAMPUSFIX_DISPATCH_TEST_EVIDENCE,JSON.stringify({requests:requests.length,http:requests,independent_sqlite_snapshots:fingerprints.length,verified,samples,transport_loss_is_client_simulation:true,not_browser_e2e:true},null,2)+'\n');console.log(`ACTUAL frontend proxy requests=${requests.length}, SQLite snapshots=${fingerprints.length}, persisted chains=${verified.length}`)})

test('real login, identity, proxy JSON, authorized pending list and Top1 assignment',async()=>{
  const health=await fetch(`${base}/api/health`);assert.equal(health.status,200);assert.match(health.headers.get('content-type'),/json/);assert.equal((await health.json()).data.service,'campusfix-backend')
  assert.equal(await signIn('demo_dispatcher',c.passwords.demo_dispatcher),true);assert.equal(authState.user.role,'dispatcher');assert.equal((await meRequest()).role,'dispatcher')
  const page=await managedOrders({status:'PENDING_DISPATCH',page:1,page_size:100});assert.ok(page.items.some(o=>o.id===c.top));assert.ok(page.items.every(o=>o.location.id!==13))
  const f=flow(), before=snap();await f.load(c.top);assert.equal(snap(),before);assert.equal(f.phase,'ready');assert.equal(f.snapshot.top_candidates.length,3);assert.equal(f.snapshot.other_eligible_candidates.length,3)
  assert.equal(f.snapshot.top_candidates[0].total_score,81);const selected=f.snapshot.top_candidates[0].worker_id;f.choose(selected);await f.send();assert.equal(f.phase,'success');assert.equal(f.result.status,'PENDING_ACCEPT');assert.equal(f.verification.status,'PENDING_ACCEPT');persisted(c.top,selected)
  samples.top_receipt=structuredClone(f.result)
  assert.equal((await orderDetail(c.top)).current_assignment.worker_name,f.result.worker.display_name)
})
test('real non-Top3 reason, final snapshot linkage and safe conflict reload',async()=>{
  mode('reset');const f=flow();await f.load(c.fourth);const fourth=f.snapshot.other_eligible_candidates[0].worker_id;f.choose(fourth)
  const writes=requests.filter(r=>r.method==='post'&&r.path.endsWith('/assign')).length;await assert.rejects(f.send());assert.equal(requests.filter(r=>r.method==='post'&&r.path.endsWith('/assign')).length,writes)
  samples.non_top_preview=structuredClone(f.snapshot);f.reason='Fictional manual adjustment through frontend';await f.send();assert.equal(f.phase,'success');persisted(c.fourth,fourth,'Fictional manual adjustment through frontend');samples.non_top_receipt=structuredClone(f.result)
  const stale=flow();await stale.load(c.conflict);stale.choose(1);await assignWorker(c.conflict,{worker_id:2,expected_version:3});const before=snap();await stale.send();assert.equal(stale.phase,'conflict');assert.equal(stale.snapshot,null);assert.equal(snap(),before);await assert.rejects(stale.send())
})
test('real late eligibility change, stale version, multi-reason empty and empty covering pool',async()=>{
  mode('reset');const f=flow();await f.load(c.ineligible);f.choose(1);mode('off-duty');const before=snap();await f.send();assert.equal(f.phase,'error');assert.match(f.message,/不可接单/);assert.equal(snap(),before)
  mode('reset');await assert.rejects(assignWorker(c.ineligible,{worker_id:1,expected_version:2}),{status:409})
  mode('none');const empty=flow();await empty.load(c.empty);assert.equal(empty.phase,'empty');assert.equal(empty.snapshot.excluded_summary.UNAVAILABLE,6);assert.equal(empty.snapshot.excluded_summary.WORKER_INACTIVE,6)
  mode('reset');command(uiFixture,'--mode',db,'no-coverage');await empty.load(c.empty);assert.equal(empty.phase,'empty');assert.deepEqual(empty.snapshot.excluded_summary,{});assert.match(emptyDispatchText(empty.snapshot.excluded_summary),/覆盖范围内/)
  command(uiFixture,'--mode',db,'coverage')
})
test('actual committed POST with lost client response remains unknown and never replays',async()=>{
  mode('reset');const f=flow();await f.load(c.unknown);f.choose(1);loseNext=true;await f.send();assert.equal(f.phase,'unknown_result');assert.equal(f.result,null);assert.equal(f.verification.status,'PENDING_ACCEPT');persisted(c.unknown,1)
  const count=requests.filter(r=>r.path.endsWith('/assign')).length;await assert.rejects(f.send());await f.load(c.unknown);assert.equal(requests.filter(r=>r.path.endsWith('/assign')).length,count)
})
test('real DISPATCH-only sysadmin logs in, performs no list/detail lookup and stays unknown on lost ack',async()=>{
  await signOut();assert.equal(await signIn('demo_sysadmin',c.passwords.demo_sysadmin),true);assert.deepEqual(authState.user.capabilities.includes('BUSINESS_READ'),false);assert.ok(authState.user.capabilities.includes('DISPATCH'))
  mode('reset');const start=requests.length,id=Number(command(fixture,'--new',db,secrets)),f=flow();await f.load(id);f.choose(1);loseNext=true;await f.send();assert.equal(f.phase,'unknown_result');assert.equal(f.verification,null)
  assert.ok(requests.slice(start).every(r=>/dispatch-candidates|\/assign$/.test(r.path)));persisted(id,1)
  const before=snap();await assert.rejects(getDispatchCandidates(c.foreign),{status:404});assert.equal(snap(),before)
})
test('real unauthorized role and revoked/anonymous sessions cannot read candidates or assign',async()=>{
  await signOut();assert.equal(await signIn('demo_reporter01',c.passwords.demo_reporter01),true)
  const before=snap();await assert.rejects(getDispatchCandidates(c.empty),{status:403});await assert.rejects(assignWorker(c.empty,{worker_id:1,expected_version:3}),{status:403});assert.equal(snap(),before)
  const oldToken=credentialSnapshot().token;await signOut();assert.equal(authState.status,'anonymous');await assert.rejects(getDispatchCandidates(c.empty),{status:401})
  replaceCredential(oldToken);await assert.rejects(getDispatchCandidates(c.empty),{status:401});assert.equal(command(fixture,'--integrity',db),'integrity_check=ok; foreign_key_check empty')
})
