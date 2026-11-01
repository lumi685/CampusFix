import { test, after } from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import { parse, compileScript, compileTemplate } from '@vue/compiler-sfc'
import { reactive, computed, watch, effectScope } from 'vue'
import { AxiosError } from 'axios'
import { apiClient } from '../src/api/client.js'
import { replaceCredential } from '../src/api/credentials.js'
import { workerTasks, taskAction, refusalReason } from '../src/api/workerTasks.js'
import { workerTaskFlow } from '../src/utils/workerTasks.js'
import { navigation, canAccessEntry } from '../src/router/navigation.js'
const original=apiClient.defaults.adapter
after(()=>{apiClient.defaults.adapter=original;replaceCredential()})
const item=(id=9,status='PENDING_ACCEPT')=>({id,order_no:`WX-FICTIONAL-${id}`,title:'Fictional task',category:{name:'Electrical'},location:{name:'Fictional room'},approved_urgency:'HIGH',status,version:status==='PENDING_ACCEPT'?4:5})
const page=(status='PENDING_ACCEPT')=>({items:[item(9,status)],page:1,page_size:20,total:1})
const receipt=(kind='accept')=>({order_id:9,status:kind==='accept'?'IN_PROGRESS':'PENDING_DISPATCH',version:5,...(kind==='accept'?{accepted_at:'2026-10-31T00:00:00.000Z'}:{assignment_ended:true})})
const fail=status=>Object.assign(new Error('Fictional failure'),{status})
const loaded=async(api={},can=()=>true)=>{const f=workerTaskFlow({list:async({status})=>page(status),...api},can);await f.load();return f}
const tick=()=>new Promise(r=>setImmediate(r))

test('API current-only assigned query cannot be overridden to reported/history/other user',async()=>{
  let config;apiClient.defaults.adapter=async c=>{config=c;return {data:{code:0,message:'success',request_id:'req_fixture',data:page()},status:200,headers:{},config:c}};replaceCredential('fictional-test-only')
  await workerTasks({status:'PENDING_ACCEPT',view:'reported',current_only:false,user_id:3});assert.deepEqual(config.params,{view:'assigned',current_only:true,status:'PENDING_ACCEPT',page:1,page_size:20});assert.equal(config.authRequired,true)
  await assert.rejects(workerTasks({status:'CLOSED'}));await assert.rejects(workerTasks({page:0}))
})
test('API exact accept/refuse payload, safe integer and no identity fields',async()=>{
  const requests=[];apiClient.defaults.adapter=async c=>{requests.push(c);return {data:{code:0,message:'success',request_id:'req_fixture',data:{}},status:200,headers:{},config:c}}
  await taskAction(9,4);await taskAction(9,4,'Fictional reason');assert.equal(requests[0].url,'/work-orders/9/accept');assert.deepEqual(JSON.parse(requests[0].data),{expected_version:4});assert.equal(requests[1].url,'/work-orders/9/refuse');assert.deepEqual(JSON.parse(requests[1].data),{expected_version:4,reason:'Fictional reason'})
  for(const id of ['9007199254740993',0,-1,'1.1','01'])await assert.rejects(taskAction(id,4))
  for(const version of [true,'4',0,-1,1.2,Number.MAX_SAFE_INTEGER])await assert.rejects(taskAction(9,version))
})
test('reason Unicode scalars, Unicode whitespace, malformed surrogate and control boundaries',()=>{
  assert.equal(refusalReason('😀'.repeat(500)).length,1000)
  for(const v of ['', ' \n\u00a0　\uFEFF', '😀'.repeat(501),'\ud800','\udc00','x\u0000',undefined])assert.throws(()=>refusalReason(v))
})
test('front API propagates HTTP409 with no automatic retry',async()=>{
  let calls=0;apiClient.defaults.adapter=async c=>{calls++;throw new AxiosError('Conflict','ERR_BAD_REQUEST',c,null,{status:409,data:{code:'ORDER_VERSION_CONFLICT',message:'Conflict',data:{},request_id:'fixture'},headers:{},config:c})}
  await assert.rejects(taskAction(9,4),{status:409});assert.equal(calls,1)
})
test('server-only current list projection drops private fields and keeps reviewed urgency',async()=>{
  const d=page();d.items[0].internal_note='PRIVATE';d.items[0].metadata_json='PRIVATE';d.items[0].category.private='PRIVATE'
  const f=await loaded({list:async()=>d});assert.equal(f.items[0].urgency,'HIGH');assert.doesNotMatch(JSON.stringify(f.items),/PRIVATE|metadata_json|internal_note/)
})
test('empty server task list has honest empty state',async()=>{const f=await loaded({list:async()=>({...page(),items:[],total:0})});assert.equal(f.phase,'empty');assert.equal(f.choose(9,'accept'),false)})
test('malformed list or unsafe ID/version never enables operations',async()=>{
  for(const mutate of [d=>{d.items[0].id=Number.MAX_SAFE_INTEGER+1},d=>{d.items[0].version=0},d=>{d.items[0].status='IN_PROGRESS'},d=>{d.items[0].approved_urgency=null},d=>{d.items.push(item())},d=>{d.total=-1}]){const d=page();mutate(d);const f=await loaded({list:async()=>d});assert.equal(f.phase,'error');assert.deepEqual(f.items,[]);assert.equal(f.choose(9,'accept'),false)}
})
test('cancel is read-only and does not send accept/refuse',async()=>{let writes=0;const f=await loaded({action:async()=>{writes++}});f.choose(9,'refuse');f.reason='Reason';f.cancel();assert.equal(f.selected,null);assert.equal(f.reason,'');assert.equal(writes,0)})
test('real server receipt required and accepted list is re-read rather than locally forged',async()=>{
  const reads=[];const f=await loaded({list:async q=>{reads.push(q);return page(q.status)},action:async()=>receipt()});f.choose(9,'accept');await f.send();assert.equal(f.status,'IN_PROGRESS');assert.equal(f.items[0].status,'IN_PROGRESS');assert.equal(reads.length,2);assert.equal(f.choose(9,'refuse'),false)
})
test('refuse passes real version/reason and removes task only on server list response',async()=>{
  let called,reads=0;const f=await loaded({list:async()=>++reads===1?page():({...page(),items:[],total:0}),action:async(...a)=>{called=a;return receipt('refuse')}});f.choose(9,'refuse');f.reason='Specific reason';await f.send();assert.deepEqual(called,[9,4,'Specific reason']);assert.equal(f.phase,'empty');assert.deepEqual(f.items,[])
})
test('double confirm while POST pending sends exactly once and cannot change selection',async()=>{
  let release,writes=0;const f=await loaded({action:()=>{writes++;return new Promise(r=>{release=r})}});f.choose(9,'accept');const p=f.send();await f.send();assert.equal(writes,1);assert.equal(f.choose(9,'refuse'),false);f.cancel();assert.ok(f.selected);release(receipt());await p;assert.equal(writes,1)
})
test('409 invalidates stale version/controls and requires explicit fresh read',async()=>{
  let writes=0;const f=await loaded({action:async()=>{writes++;throw fail(409)}});f.choose(9,'accept');await f.send();assert.equal(f.phase,'conflict');assert.equal(f.selected,null);assert.deepEqual(f.items,[]);await assert.rejects(f.send());assert.equal(writes,1);await f.load();assert.equal(f.phase,'ready')
})
for(const status of [401,403,404])test(`HTTP${status} clears list, selection, reason and verification`,async()=>{
  const f=await loaded({action:async()=>{throw fail(status)}});f.choose(9,'refuse');f.reason='Sensitive reason';await f.send();assert.equal(f.phase,'forbidden');assert.deepEqual(f.items,[]);assert.equal(f.selected,null);assert.equal(f.reason,'');assert.equal(f.verification,null)
})
for(const status of [0,500,503])test(`transport/HTTP${status} remains unknown with authorized GET, never replays`,async()=>{
  let writes=0,reads=0;const f=await loaded({action:async()=>{writes++;throw fail(status)},detail:async()=>{reads++;return {id:9,status:'IN_PROGRESS',version:5}}});f.choose(9,'accept');await f.send();assert.equal(f.phase,'unknown_result');assert.equal(f.verification.status,'IN_PROGRESS');assert.deepEqual(f.items,[]);f.cancel();assert.ok(f.selected);await assert.rejects(f.send());await f.load();assert.equal(writes,1);assert.equal(reads,1)
})
test('unknown result GET403 clears all unauthorized data instead of retaining old snapshot',async()=>{const f=await loaded({action:async()=>{throw fail(0)},detail:async()=>{throw fail(403)}});f.choose(9,'accept');await f.send();assert.equal(f.phase,'forbidden');assert.equal(f.selected,null);assert.equal(f.verification,null);assert.deepEqual(f.items,[])})
test('late list responses after newer load or clear cannot restore old tasks',async()=>{let release;const f=workerTaskFlow({list:()=>new Promise(r=>{release=r})},()=>true);const p=f.load();f.clear();release(page());await p;assert.equal(f.phase,'idle');assert.deepEqual(f.items,[])})
test('late POST receipt after identity/navigation clear cannot start follow-up GET or restore data',async()=>{
  let release,reads=0;const f=await loaded({list:async()=>{reads++;return page()},action:()=>new Promise(r=>{release=r})});f.choose(9,'accept');const p=f.send();f.clear();release(receipt());await p;assert.equal(reads,1);assert.equal(f.phase,'idle');assert.deepEqual(f.items,[])
})
test('late verification after identity loss cannot restore previous task status',async()=>{
  let release;const f=await loaded({action:async()=>{throw fail(0)},detail:()=>new Promise(r=>{release=r})});f.choose(9,'accept');const p=f.send();await tick();f.clear();release({id:9,status:'IN_PROGRESS',version:5});await p;assert.equal(f.verification,null);assert.equal(f.phase,'idle')
})
test('malformed success receipt cannot fabricate a successful task action',async()=>{const f=await loaded({action:async()=>({order_id:999,status:'IN_PROGRESS',version:5}),detail:async()=>({id:9,status:'PENDING_ACCEPT',version:4})});f.choose(9,'accept');await f.send();assert.equal(f.phase,'unknown_result');assert.equal(f.verification.status,'PENDING_ACCEPT')})
function actualView(role='worker',list=async()=>page()){
  const {descriptor}=parse(readFileSync(new URL('../src/views/WorkerTasksView.vue',import.meta.url),'utf8'))
  assert.deepEqual(compileTemplate({source:descriptor.template.content,filename:'WorkerTasksView.vue',id:'w7d1'}).errors,[])
  const code=compileScript(descriptor,{id:'w7d1',genDefaultAs:'component'}).content.replace(/^import .*$/gm,'')
  const auth=reactive({status:'authenticated',user:{id:7,role}}),cleanups=[],scope=effectScope()
  const component=new Function('computed','reactive','watch','onBeforeUnmount','ElDialog','authState','workerTasks','taskAction','refusalReason','orderDetail','workerTaskFlow','OrderStatusTag','WorkerWorkflowPanel','WorkerTransferHistory',`${code};return component`)(computed,reactive,watch,fn=>cleanups.push(fn),{},auth,list,async()=>receipt(),refusalReason,async()=>({id:9,status:'IN_PROGRESS',version:5}),workerTaskFlow,{},{},{})
  const view=scope.run(()=>component.setup({},{expose(){}}));return {view,auth,dispose(){for(const c of cleanups)c();scope.stop()}}
}
test('actual compiled SFC registers dialog and worker-only route navigation remains separated',async()=>{
  const a=actualView();try{await tick();assert.equal(a.view.allowed.value,true);assert.equal(a.view.flow.phase,'ready');const entry=navigation.find(n=>n.id==='worker-tasks');for(const role of ['reporter','dispatcher','sysadmin'])assert.equal(canAccessEntry({role,capabilities:['DISPATCH']},entry),false);assert.equal(canAccessEntry({role:'worker'},entry),true)}finally{a.dispose()}
})
test('actual SFC wrong role makes no task query',async()=>{let reads=0;const a=actualView('reporter',async()=>{reads++;return page()});try{await tick();assert.equal(reads,0);assert.equal(a.view.allowed.value,false)}finally{a.dispose()}})
test('actual SFC logout clears pending old response synchronously',async()=>{let release;const a=actualView('worker',()=>new Promise(r=>{release=r}));try{a.auth.status='loggingOut';release(page());await tick();assert.deepEqual(a.view.flow.items,[]);assert.equal(a.view.flow.phase,'idle')}finally{a.dispose()}})
test('actual SFC account switch clears previous reason/selection and starts fresh query',async()=>{let calls=0;const a=actualView('worker',async()=>{calls++;return page()});try{await tick();a.view.flow.choose(9,'refuse');a.view.flow.reason='Old user input';a.auth.user.id=8;assert.equal(a.view.flow.reason,'');assert.equal(a.view.flow.selected,null);await tick();assert.equal(calls,2)}finally{a.dispose()}})
test('actual SFC unmount discards late task list',async()=>{let release;const a=actualView('worker',()=>new Promise(r=>{release=r}));a.dispose();release(page());await tick();assert.deepEqual(a.view.flow.items,[])})
