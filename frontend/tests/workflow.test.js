import { test, after } from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import { randomBytes } from 'node:crypto'
import { parse, compileScript, compileTemplate } from '@vue/compiler-sfc'
import { computed, reactive, ref, watch, effectScope } from 'vue'
import { apiClient } from '../src/api/client.js'
import { replaceCredential } from '../src/api/credentials.js'
import { workflowText, transferRequests, requestTransfer, decideTransfer, progressEvent } from '../src/api/workflow.js'
import { workflowFlow, transferPage } from '../src/utils/workflow.js'
import { managementAbility } from '../src/utils/adminOrders.js'
const original=apiClient.defaults.adapter
after(()=>{apiClient.defaults.adapter=original;replaceCredential()})
const row=(overrides={})=>({transfer_request_id:7,order_id:9,order_no:'WX-FICTIONAL',assignment_id:5,reason:'Fictional specialist',status:'PENDING',created_at:'2026-11-01T00:00:00.000Z',order_status:'IN_PROGRESS',order_version:5,review_reason:null,resolved_at:null,...overrides})
const page=()=>({items:[row()],page:1,page_size:20,total:1})
const context=(kind='transfer',decision='APPROVE')=>({kind,orderId:9,requestId:7,version:5,status:'IN_PROGRESS',decision})
const receipt=(kind='transfer',decision='APPROVE')=>kind==='transfer'?{transfer_request_id:7,order_id:9,status:'PENDING',order_status:'IN_PROGRESS'}:kind==='decision'?{transfer_request_id:7,decision,order_status:decision==='APPROVE'?'PENDING_DISPATCH':'IN_PROGRESS',version:decision==='APPROVE'?6:5,assignment_ended:decision==='APPROVE'}:{id:88,order_id:9,assignment_id:5,type:kind,order_status:'IN_PROGRESS',order_version:5,created_at:'2026-11-01T00:00:00.000Z'}
const fail=status=>Object.assign(new Error('Fictional failure'),{status})
const tick=()=>new Promise(r=>setImmediate(r))
const ready=(api={},kind='transfer',decision='APPROVE')=>{const f=workflowFlow({post:async()=>receipt(kind,decision),verify:async()=>'Only current state, not proof',...api},()=>true);f.open(context(kind,decision));f.note='Fictional public note';return f}

test('API34 whitelist, required view, pagination bounds and relative authenticated path',async()=>{
  const requests=[];replaceCredential(randomBytes(32).toString('hex'));apiClient.defaults.adapter=async c=>{requests.push(c);return {status:200,data:{code:0,message:'success',data:page(),request_id:'req_fixture'},headers:{},config:c}}
  await transferRequests({view:'managed',order_id:9,role:'sysadmin',status:'APPROVED',user_id:3});assert.equal(requests[0].url,'/transfer-requests');assert.deepEqual(requests[0].params,{view:'managed',page:1,page_size:20,order_id:'9'});assert.equal(requests[0].authRequired,true)
  for(const q of [{},{view:'other'},{view:'mine',page:0},{view:'mine',page_size:101},{view:'mine',order_id:'9007199254740993'}])await assert.rejects(transferRequests(q))
})
test('API21/22/33 exact paths and current versions, no client identity or target state',async()=>{
  const calls=[];apiClient.defaults.adapter=async c=>{calls.push(c);return {status:201,data:{code:0,data:{},request_id:'req_fixture'},headers:{},config:c}}
  await requestTransfer(9,5,'Public reason');await decideTransfer(9,7,5,'APPROVE','Public decision');await progressEvent(9,5,'ARRIVED');await progressEvent(9,5,'PROGRESS_NOTE','Public progress')
  assert.deepEqual(calls.map(c=>c.url),['/work-orders/9/transfer-requests','/work-orders/9/transfer-requests/7/decision','/work-orders/9/progress-events','/work-orders/9/progress-events']);assert.deepEqual(calls.map(c=>JSON.parse(c.data)),[{expected_version:5,reason:'Public reason'},{expected_version:5,decision:'APPROVE',reason:'Public decision'},{expected_version:5,type:'ARRIVED'},{expected_version:5,type:'PROGRESS_NOTE',note:'Public progress'}])
  await assert.rejects(decideTransfer(9,7,5,'CANCEL','Bad'));await assert.rejects(progressEvent(9,5,'COMPLETE','Bad'));await assert.rejects(requestTransfer('9007199254740993',5,'Bad'));await assert.rejects(requestTransfer(9,Number.MAX_SAFE_INTEGER,'Bad'))
})
test('Unicode reason/progress boundaries, optional arrival, malformed/control and whitespace',()=>{
  workflowText('😀'.repeat(500));workflowText('😀'.repeat(1000),1000);workflowText('',500,true)
  for(const value of ['', '\u00a0\u3000\uFEFF', 'x'.repeat(501), '\ud800', '\u0000'])assert.throws(()=>workflowText(value))
  assert.throws(()=>workflowText('😀'.repeat(1001),1000))
})
test('API34 projection has fixed whitelist, authentic current version and allows mine history',()=>{
  const d=page();d.items[0].internal_note='PRIVATE';d.items[0].contact_phone='PRIVATE';d.items[0].order_version=6;const result=transferPage(d,true);assert.equal(result.items[0].order_version,6);assert.doesNotMatch(JSON.stringify(result),/PRIVATE|internal_note|contact_phone/)
  for(const status of ['APPROVED','REJECTED','SUPERSEDED'])assert.equal(transferPage({...page(),items:[row({status,review_reason:'Public result'})]}).items[0].status,status)
})
test('malformed/unsafe or nonactionable managed rows cannot become an approval selection',()=>{
  for(const change of [{status:'APPROVED'},{order_status:'PENDING_DISPATCH'},{order_version:0},{transfer_request_id:Number.MAX_SAFE_INTEGER+1},{assignment_id:'5'},{review_reason:{private:'Bad'}}])assert.throws(()=>transferPage({...page(),items:[row(change)]},true))
  assert.throws(()=>transferPage({...page(),items:[row(),row()]},true))
})
for(const [kind,decision] of [['transfer','APPROVE'],['ARRIVED','APPROVE'],['PROGRESS_NOTE','APPROVE'],['decision','APPROVE'],['decision','REJECT']])test(`receipt ${kind}/${decision} reflects correct state/version without fabricated transition`,async()=>{
  const f=ready({},kind,decision);const result=await f.send();assert.deepEqual(result,receipt(kind,decision));assert.equal(f.phase,'success')
})
test('invalid successful receipt stays unknown, never treated as a committed success',async()=>{const f=ready({post:async()=>({transfer_request_id:999,order_id:100,status:'PENDING',order_status:'IN_PROGRESS'})});await f.send();assert.equal(f.phase,'unknown_result');assert.equal(f.result,null)})
test('duplicate confirm sends one POST, cancellation never writes',async()=>{
  let release,writes=0;const f=ready({post:()=>{writes++;return new Promise(r=>{release=r})}});const p=f.send();await f.send();f.cancel();assert.equal(f.phase,'submitting');release(receipt());await p;assert.equal(writes,1);const g=ready({post:async()=>{writes++}});g.cancel();assert.equal(writes,1)
})
test('422 preserves repairable note/context, no automatic resend',async()=>{let writes=0;const f=ready({post:async()=>{writes++;throw fail(422)}});await f.send();assert.equal(f.phase,'ready');assert.equal(f.note,'Fictional public note');assert.equal(f.context.version,5);assert.equal(writes,1)})
test('409 clears stale version/selection/note and blocks immediate replay',async()=>{const f=ready({post:async()=>{throw fail(409)}});await f.send();assert.equal(f.phase,'conflict');assert.equal(f.context,null);assert.equal(f.note,'');await assert.rejects(f.send())})
for(const status of [401,403,404])test(`HTTP${status} destroys unauthorized operation state`,async()=>{const f=ready({post:async()=>{throw fail(status)}});await f.send();assert.equal(f.phase,'forbidden');assert.equal(f.context,null);assert.equal(f.note,'');assert.equal(f.result,null);assert.equal(f.verification,null)})
for(const status of [0,500,503])test(`network/HTTP${status} read-only reconciliation cannot authorize write replay`,async()=>{
  let writes=0,reads=0;const f=ready({post:async()=>{writes++;throw fail(status)},verify:async()=>{reads++;return 'PENDING_DISPATCH'}});await f.send();assert.equal(f.phase,'unknown_result');assert.equal(f.result,null);assert.equal(f.verification,'PENDING_DISPATCH');assert.equal(f.open(context()),false);await assert.rejects(f.send());assert.equal(writes,1);assert.equal(reads,1)
})
test('unknown-result verification403 clears sensitive state',async()=>{const f=ready({post:async()=>{throw fail(0)},verify:async()=>{throw fail(403)}});await f.send();assert.equal(f.phase,'forbidden');assert.equal(f.context,null);assert.equal(f.verification,null)})
test('late POST and late verification after clear never restore data',async()=>{
  let release,reads=0;const f=ready({post:()=>new Promise(r=>{release=r}),verify:async()=>{reads++}});const p=f.send();f.clear();release(receipt());await p;assert.equal(f.phase,'idle');assert.equal(f.result,null);assert.equal(reads,0)
  let verifyRelease;const g=ready({post:async()=>{throw fail(0)},verify:()=>new Promise(r=>{verifyRelease=r})});const q=g.send();await tick();g.clear();verifyRelease('Late data');await q;assert.equal(g.verification,null);assert.equal(g.phase,'idle')
})
function actual(name,{role='sysadmin',capabilities=['DISPATCH'],list=async()=>page(),post=async()=>receipt('decision'),timeline=async()=>({items:[],total:0})}={}){
  const {descriptor}=parse(readFileSync(new URL(`../src/components/${name}.vue`,import.meta.url),'utf8'));assert.deepEqual(compileTemplate({source:descriptor.template.content,filename:`${name}.vue`,id:'workflow'}).errors,[])
  const code=compileScript(descriptor,{id:'workflow',genDefaultAs:'component'}).content.replace(/^import .*$/gm,'')
  const auth=reactive({status:'authenticated',user:{id:8,role,capabilities,scope_location_ids:[1]}}),props=reactive(name==='TransferQueue'?{orderId:null,blocked:false}:{task:{id:9,order_no:'WX-FICTIONAL',status:'IN_PROGRESS',version:5},blocked:false,gate:{owner:null,busy:false,unknown:false}}),cleanups=[],scope=effectScope(),queries=[],emitted=[]
  const names=['computed','reactive','ref','watch','onBeforeUnmount','ElDialog','authState','managementAbility','transferRequests','decideTransfer','requestTransfer','progressEvent','orderTimeline','workflowFlow','transferPage','transferStatuses','OrderStatusTag','OrderTimeline','dateText']
  const component=new Function(...names,`${code};return component`)(computed,reactive,ref,watch,fn=>cleanups.push(fn),{},auth,managementAbility,q=>{queries.push(q);return list(q)},post,post,post,timeline,workflowFlow,transferPage,{}, {},{},v=>v)
  const view=scope.run(()=>component.setup(props,{expose(){},emit:(...args)=>emitted.push(args)}));return {view,props,auth,queries,emitted,dispose(){for(const fn of cleanups)fn();scope.stop()}}
}
test('actual queue DISPATCH-only identity uses API34 only and selects real returned request/current version',async()=>{const a=actual('TransferQueue');try{await tick();assert.equal(a.view.allowed.value,true);assert.deepEqual(a.queries,[{view:'managed',page:1}]);const t=a.view.items.value[0];a.view.choose(t,'APPROVE');assert.equal(a.view.flow.context.requestId,7);assert.equal(a.view.flow.context.version,5);a.view.choose(row({transfer_request_id:99}),'APPROVE');assert.equal(a.view.flow.context.requestId,7)}finally{a.dispose()}})
test('actual queue BUSINESS_READ alone or worker never queries managed data',async()=>{for(const config of [{role:'sysadmin',capabilities:['BUSINESS_READ']},{role:'worker',capabilities:['DISPATCH']}]){const a=actual('TransferQueue',config);try{await tick();assert.equal(a.queries.length,0);assert.equal(a.view.allowed.value,false)}finally{a.dispose()}}})
test('actual queue revoke/identity change invalidates delayed list and old selection',async()=>{let release;const a=actual('TransferQueue',{list:()=>new Promise(r=>{release=r})});try{a.auth.user.capabilities=[];release(page());await tick();assert.deepEqual(a.view.items.value,[]);assert.equal(a.view.flow.context,null)}finally{a.dispose()}})
test('actual queue 409 clears stale rows and selection; refresh required',async()=>{const a=actual('TransferQueue',{post:async()=>{throw fail(409)}});try{await tick();a.view.choose(a.view.items.value[0],'APPROVE');a.view.flow.note='Public decision';await a.view.confirm();assert.equal(a.view.flow.phase,'conflict');assert.deepEqual(a.view.items.value,[]);assert.equal(a.view.flow.context,null)}finally{a.dispose()}})
test('actual queue unmount and late POST cannot overwrite new route',async()=>{let release;const a=actual('TransferQueue',{post:()=>new Promise(r=>{release=r})});await tick();a.view.choose(a.view.items.value[0],'APPROVE');a.view.flow.note='Public decision';const p=a.view.confirm();a.dispose();release(receipt('decision'));await p;assert.equal(a.view.lastReceipt.value,null);assert.deepEqual(a.view.items.value,[])})
test('actual worker IN_PROGRESS submits arrival, then reads real mine/timeline and retains arrival confirmation',async()=>{const a=actual('WorkerWorkflowPanel',{role:'worker',capabilities:[],post:async()=>receipt('ARRIVED')});try{a.view.open('ARRIVED');await a.view.confirm();assert.equal(a.view.arrived.value,true);assert.equal(a.view.flow.phase,'success');assert.equal(a.queries[0].view,'mine');assert.equal(a.queries[0].order_id,9);assert.equal(a.props.gate.busy,false)}finally{a.dispose()}})
test('actual worker 422 keeps editable note; parent write gate released',async()=>{const a=actual('WorkerWorkflowPanel',{role:'worker',capabilities:[],post:async()=>{throw fail(422)}});try{a.view.open('transfer');a.view.flow.note='Public reason';await a.view.confirm();assert.equal(a.view.flow.note,'Public reason');assert.equal(a.view.flow.phase,'ready');assert.equal(a.props.gate.busy,false)}finally{a.dispose()}})
test('actual worker timeout gates all new writes and cannot claim receipt from read',async()=>{const a=actual('WorkerWorkflowPanel',{role:'worker',capabilities:[],post:async()=>{throw fail(0)}});try{a.view.open('transfer');a.view.flow.note='Public reason';await a.view.confirm();assert.equal(a.view.flow.phase,'unknown_result');assert.equal(a.props.gate.unknown,true);assert.equal(a.view.flow.result,null)}finally{a.dispose()}})
test('actual worker account switch drops late write and old gate cannot unlock a newer operation',async()=>{
  const releases=[];const a=actual('WorkerWorkflowPanel',{role:'worker',capabilities:[],post:()=>new Promise(r=>releases.push(r))});try{a.view.open('transfer');a.view.flow.note='First reason';const p=a.view.confirm();a.auth.user.id=9;a.view.open('transfer');a.view.flow.note='New user reason';const q=a.view.confirm();releases[0](receipt());await p;assert.equal(a.props.gate.busy,true);assert.equal(a.view.flow.phase,'submitting');releases[1](receipt());await q;assert.equal(a.props.gate.busy,false)}finally{a.dispose()}
})
test('actual mine history remains available when original assignment ended; never queries full details',async()=>{const a=actual('WorkerTransferHistory',{role:'worker',capabilities:[],list:async()=>({...page(),items:[row({status:'APPROVED',order_status:'PENDING_DISPATCH',review_reason:'Public approval'})]})});try{await tick();assert.deepEqual(a.queries,[{view:'mine',page:1}]);assert.equal(a.view.items.value[0].status,'APPROVED');assert.equal(a.view.items.value[0].review_reason,'Public approval')}finally{a.dispose()}})
test('actual mine history clears late old-user response after logout',async()=>{let release;const a=actual('WorkerTransferHistory',{role:'worker',capabilities:[],list:()=>new Promise(r=>{release=r})});try{a.auth.status='anonymous';release(page());await tick();assert.deepEqual(a.view.items.value,[]);assert.equal(a.view.total.value,0)}finally{a.dispose()}})
test('actual queue query403 signals parent to clear stale dispatch candidates',async()=>{const a=actual('TransferQueue',{list:async()=>{throw fail(403)}});try{await tick();assert.deepEqual(a.view.items.value,[]);assert.ok(a.emitted.some(e=>e[0]==='forbidden'))}finally{a.dispose()}})
test('actual mine history403 signals parent to clear old task data',async()=>{const a=actual('WorkerTransferHistory',{role:'worker',capabilities:[],list:async()=>{throw fail(403)}});try{await tick();assert.deepEqual(a.view.items.value,[]);assert.ok(a.emitted.some(e=>e[0]==='forbidden'))}finally{a.dispose()}})
