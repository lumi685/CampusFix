import { test, after } from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import { randomBytes } from 'node:crypto'
import { parse, compileScript, compileTemplate } from '@vue/compiler-sfc'
import { reactive, ref, watch, computed, effectScope, h, createSSRApp } from 'vue'
import * as Vue from 'vue'
import { renderToString } from '@vue/server-renderer'
import { actions, statuses, dateText } from '../src/utils/orders.js'
import { completeTask, workflowText } from '../src/api/workflow.js'
import { workerTasks } from '../src/api/workerTasks.js'
import { workflowFlow } from '../src/utils/workflow.js'
import { apiClient } from '../src/api/client.js'
import { replaceCredential } from '../src/api/credentials.js'
const original=apiClient.defaults.adapter
after(()=>{apiClient.defaults.adapter=original;replaceCredential()})
const summary='Fictional repair result',result=()=>({order_id:9,status:'PENDING_VERIFY',version:6,repair_summary:summary})
const context=()=>({kind:'complete',orderId:9,status:'IN_PROGRESS',version:5})
const fail=status=>Object.assign(new Error('Fictional error'),{status})
const tick=()=>new Promise(r=>setImmediate(r))
function flow(api={}){const f=workflowFlow({post:async()=>result(),verify:async()=>'Read-only current state',...api},()=>true);f.open(context());f.note=summary;return f}
test('complete exact URL, payload, authorized relative address and true envelope',async()=>{
  replaceCredential(randomBytes(32).toString('hex'));let config
  apiClient.defaults.adapter=async c=>{config=c;return {status:200,headers:{},config:c,data:{code:0,message:'success',data:result(),request_id:'req_fixture'}}}
  assert.deepEqual(await completeTask(9,5,summary),result());assert.equal(config.url,'/work-orders/9/complete');assert.equal(config.authRequired,true);assert.deepEqual(JSON.parse(config.data),{expected_version:5,repair_summary:summary})
})
test('complete envelope missing message/request ID, malformed200/201 are uncertain',async()=>{
  for(const changed of [{message:undefined},{request_id:' '},{request_id:'other'},{code:'error'}]){apiClient.defaults.adapter=async c=>({status:200,headers:{},config:c,data:{code:0,message:'success',request_id:'req_fixture',data:result(),...changed}});await assert.rejects(completeTask(9,5,summary))}
  apiClient.defaults.adapter=async c=>({status:201,headers:{},config:c,data:{code:0,message:'success',request_id:'req_fixture',data:result()}});await assert.rejects(completeTask(9,5,summary))
})
test('2000 Unicode boundary, nonblank, malformed surrogates and controls',()=>{
  workflowText('修'.repeat(2000),2000);workflowText('😀'.repeat(2000),2000)
  for(const value of ['', '\u3000\u00a0','修'.repeat(2001),'\ud800','\udfff','\0','\x01'])assert.throws(()=>workflowText(value,2000))
})
test('pending verify query keeps current_only true, no client-side authority',async()=>{
  let config;apiClient.defaults.adapter=async c=>{config=c;return {status:200,headers:{},config:c,data:{code:0,message:'success',request_id:'req_fixture',data:{}}}}
  await workerTasks({status:'PENDING_VERIFY'});assert.deepEqual(config.params,{view:'assigned',current_only:true,status:'PENDING_VERIFY',page:1,page_size:20})
})
test('complete confirmation is explicit, cancellation sends no POST',async()=>{let writes=0;const f=flow({post:async()=>{writes++;return result()}});assert.equal(writes,0);f.cancel();assert.equal(writes,0);assert.equal(f.phase,'idle')})
test('valid complete receipt succeeds without guessing server state',async()=>{const f=flow();assert.deepEqual(await f.send(),result());assert.equal(f.phase,'success')})
for(const changed of [{order_id:10},{status:'COMPLETED'},{version:7},{repair_summary:'Different'}, {repair_summary:undefined}])test(`complete invalid receipt ${Object.keys(changed)[0]} cannot claim success`,async()=>{const f=flow({post:async()=>({...result(),...changed})});await f.send();assert.equal(f.phase,'unknown_result');assert.equal(f.result,null)})
test('blank input prevents write before entering submitting',async()=>{let writes=0;const f=flow({post:async()=>{writes++}});f.note='　';await assert.rejects(f.send());assert.equal(writes,0);assert.equal(f.phase,'ready')})
test('422 retains repair_summary draft, 409 destroys stale confirmation',async()=>{const f=flow({post:async()=>{throw fail(422)}});await f.send();assert.equal(f.note,summary);assert.equal(f.phase,'ready');assert.match(f.message,/repair_summary/);const g=flow({post:async()=>{throw fail(409)}});await g.send();assert.equal(g.context,null);assert.equal(g.phase,'conflict');await assert.rejects(g.send())})
for(const status of [401,403,404])test(`complete HTTP${status} clears protected state`,async()=>{const f=flow({post:async()=>{throw fail(status)}});await f.send();assert.equal(f.phase,'forbidden');assert.equal(f.context,null);assert.equal(f.note,'')})
test('double click, unknown committed result and account change never replay',async()=>{let release,writes=0;const f=flow({post:()=>{writes++;return new Promise(r=>{release=r})}});const p=f.send();await f.send();assert.equal(writes,1);f.clear();release(result());await p;assert.equal(f.result,null);const g=flow({post:async()=>{writes++;throw fail(0)}});await g.send();assert.equal(g.phase,'unknown_result');assert.equal(g.open(context()),false);await assert.rejects(g.send());assert.equal(writes,2)})
function actual({post=async()=>result(),gate=reactive({owner:null,busy:false,unknown:false}),status='IN_PROGRESS'}={}){
  const {descriptor}=parse(readFileSync(new URL('../src/components/WorkerWorkflowPanel.vue',import.meta.url),'utf8'))
  assert.deepEqual(compileTemplate({source:descriptor.template.content,filename:'WorkerWorkflowPanel.vue',id:'completion'}).errors,[])
  const code=compileScript(descriptor,{id:'completion',genDefaultAs:'component'}).content.replace(/^import .*$/gm,'')
  const auth=reactive({status:'authenticated',user:{id:8,role:'worker'}}),props=reactive({task:{id:9,order_no:'WX-FICTIONAL',status,version:5},gate,blocked:false}),scope=effectScope(),cleanups=[],emitted=[]
  const names=['computed','reactive','ref','watch','onBeforeUnmount','ElDialog','authState','requestTransfer','transferRequests','progressEvent','completeTask','orderTimeline','workflowFlow','transferPage','transferStatuses','OrderTimeline']
  const component=new Function(...names,`${code};return component`)(computed,reactive,ref,watch,fn=>cleanups.push(fn),{},auth,post,async()=>({items:[],page:1,page_size:20,total:0}),post,post,async()=>({items:[{id:1,action:'ORDER_COMPLETED',public_note:summary}],total:1}),workflowFlow,x=>x,{}, {})
  const view=scope.run(()=>component.setup(props,{expose(){},emit:(...e)=>emitted.push(e)}))
  return {view,props,auth,emitted,dispose(){for(const fn of cleanups)fn();scope.stop()}}
}
test('actual SFC complete reads public result and asks parent to reload server list',async()=>{const a=actual();try{a.view.open('complete');a.view.flow.note=summary;await a.view.confirm();assert.equal(a.view.flow.phase,'success');assert.equal(a.view.timeline.value[0].public_note,summary);assert.ok(a.emitted.some(e=>e[0]==='changed'));assert.equal(a.props.gate.busy,false)}finally{a.dispose()}})
test('actual shared gate prevents cross-task transfer/progress/complete while POST pending',async()=>{
  const gate=reactive({owner:null,busy:false,unknown:false});let release,writes=0
  const a=actual({gate,post:()=>{writes++;return new Promise(r=>{release=r})}}),b=actual({gate,post:async()=>{writes++;return result()}})
  try{a.view.open('complete');a.view.flow.note=summary;const p=a.view.confirm();for(const kind of ['complete','transfer','ARRIVED','PROGRESS_NOTE']){b.view.open(kind);assert.equal(b.view.flow.context,null)}await a.view.confirm();assert.equal(writes,1);release(result());await p;assert.equal(gate.busy,false)}finally{a.dispose();b.dispose()}
})
test('actual SFC unknown result holds shared gate through read-only reconciliation',async()=>{const a=actual({post:async()=>{throw fail(0)}});try{a.view.open('complete');a.view.flow.note=summary;await a.view.confirm();assert.equal(a.view.flow.phase,'unknown_result');assert.equal(a.props.gate.unknown,true);assert.equal(a.view.flow.result,null);a.view.open('transfer');assert.equal(a.view.flow.context.kind,'complete')}finally{a.dispose()}})
test('actual SFC logout/unmount discards delayed completion and cannot unlock new owner',async()=>{
  for(const unmount of [false,true]){let release;const a=actual({post:()=>new Promise(r=>{release=r})});a.view.open('complete');a.view.flow.note=summary;const p=a.view.confirm();if(unmount)a.dispose();else a.auth.status='anonymous';const newer=Symbol('new-owner');a.props.gate.owner=newer;a.props.gate.busy=true;release(result());await p;assert.equal(a.view.flow.result,null);assert.equal(a.props.gate.owner,newer);assert.equal(a.props.gate.busy,true);if(!unmount)a.dispose()}
})
test('actual SFC409 invalidates parent task;422 retains draft;403 purges records',async()=>{
  for(const status of [409,422,403]){const a=actual({post:async()=>{throw fail(status)}});try{a.view.open('complete');a.view.flow.note=summary;await a.view.confirm();if(status===422){assert.equal(a.view.flow.note,summary);assert.equal(a.view.flow.context.version,5)}else{assert.equal(a.view.flow.context,null);assert.equal(a.view.timeline.value.length,0);assert.ok(a.emitted.some(e=>e[0]===(status===409?'stale':'forbidden')))}}finally{a.dispose()}}
})
test('pending verify never offers completion, including programmatic opening',()=>{const a=actual({status:'PENDING_VERIFY'});try{assert.throws(()=>a.view.open('complete'));assert.equal(a.view.flow.context,null)}finally{a.dispose()}})
test('actual OrderTimeline render shows full public completion text with safe escaping',async()=>{
  const source=readFileSync(new URL('../src/components/OrderTimeline.vue',import.meta.url),'utf8');const {descriptor}=parse(source);const rendered=compileTemplate({source:descriptor.template.content,filename:'OrderTimeline.vue',id:'timeline'});assert.deepEqual(rendered.errors,[]);assert.match(rendered.code,/public_note/);assert.doesNotMatch(source,/v-html/)
  const labels=readFileSync(new URL('../src/utils/orders.js',import.meta.url),'utf8');assert.match(labels,/ORDER_COMPLETED/)
  const code=rendered.code.replace(/^import \{([^}]+)\} from "vue"/gm,(_,names)=>`const {${names.replace(/ as /g,':')}}=Vue`).replace('export function render','function render')
  const render=new Function('Vue',`${code};return render`)(Vue)
  const note='Fictional full result <script>not executable</script> 😀'
  const component={props:['items'],setup(){return {actions,statuses,dateText}},render}
  const app=createSSRApp(component,{items:[{id:1,action:'ORDER_COMPLETED',from_status:'IN_PROGRESS',to_status:'PENDING_VERIFY',public_note:note,created_at:'2026-11-01T00:00:00.000Z',actor:{display_name:'Fictional worker'}}]})
  for(const name of ['el-timeline','el-timeline-item'])app.component(name,{setup(_,ctx){return()=>h('div',ctx.slots.default?.())}})
  const html=await renderToString(app);assert.match(html,/提交维修结果|维修完成|维修结果/);assert.match(html,/Fictional full result &lt;script&gt;not executable&lt;\/script&gt; 😀/);assert.match(html,/待验收/)
})
