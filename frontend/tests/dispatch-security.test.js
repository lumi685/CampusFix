import { test } from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import { parse, compileScript } from '@vue/compiler-sfc'
import { ref, reactive, computed, watch, effectScope } from 'vue'
import { dispatchFlow, dispatchId, dispatchFailure, scoreLabels, exclusionRows, emptyDispatchText } from '../src/utils/dispatch.js'
import { managementAbility, managedQuery } from '../src/utils/adminOrders.js'
import { canAccessEntry, navigation } from '../src/router/navigation.js'
import { routeAccess } from '../src/router/access.js'

const data = (id = 9) => ({ order_id: id, order_version: 3, strategy: 'WEIGHTED_SCORE_V1', weights: { skill:.35,area:.2,load:.2,efficiency:.1,rating:.1,duty:.05 }, top_candidates: [{ worker_id: 1, rank: 1, worker_name: 'Fictional', total_score: 81, active_tasks: 0, max_concurrent_tasks: 3, score_detail: {skill:100,area:70,load:100,efficiency:60,rating:60,duty:0}, sample_counts:{efficiency:0,rating:0}, reason:'Server', risks:[] }], other_eligible_candidates:[], excluded_summary:{} })
const tick = () => new Promise(r => setImmediate(r))
const ack = id => ({ order_id: id, assignment_id: 5, worker:{id:1,display_name:'Fictional'},status:'PENDING_ACCEPT',version:4 })
function actualView({ role = 'sysadmin', capabilities = ['DISPATCH'], id = '9', candidates = async id => data(Number(id)), list = async () => ({ items: [], total: 0 }) } = {}) {
  const { descriptor } = parse(readFileSync(new URL('../src/views/AdminDispatchView.vue', import.meta.url), 'utf8'))
  const code = compileScript(descriptor, { id:'w6d5-security', genDefaultAs:'component' }).content.replace(/^import .*$/gm, '')
  const route = reactive({params:{id}}), auth = reactive({ status:'authenticated',user:{id:8,role,capabilities} }), cleanups = [], queries = [], scope = effectScope()
  const names = ['computed','reactive','ref','watch','onBeforeUnmount','useRoute','useRouter','getDispatchCandidates','assignWorker','managedOrders','orderDetail','authState','managementAbility','managedQuery','dispatchFlow','dispatchId','dispatchFailure','scoreLabels','exclusionRows','emptyDispatchText','DispatchCandidateCard','DispatchAssignDialog','OrderStatusTag']
  const component = new Function(...names, `${code};return component`)(computed,reactive,ref,watch,fn=>cleanups.push(fn),()=>route,()=>({push:async path=>{route.params.id=path.split('/').at(-1)}}),candidates,async()=>ack(Number(route.params.id)),q=>{queries.push(q);return list(q)},async()=>{throw new Error('No detail expected')},auth,managementAbility,managedQuery,dispatchFlow,dispatchId,dispatchFailure,scoreLabels,exclusionRows,emptyDispatchText,{},{},{})
  const view = scope.run(()=>component.setup({}, {expose(){}}))
  return { view, route, auth, queries, dispose(){for(const fn of cleanups)fn();scope.stop()} }
}

test('dispatch menu and route prechecks preserve independent capabilities', () => {
  const entry = navigation.find(n => n.id === 'admin-dispatch')
  for (const role of ['reporter','worker']) assert.equal(canAccessEntry({role,capabilities:['DISPATCH']},entry),false)
  assert.equal(canAccessEntry({role:'dispatcher',capabilities:['AREA_SCOPE']},entry),true)
  assert.equal(canAccessEntry({role:'dispatcher',capabilities:[]},entry),false)
  assert.equal(canAccessEntry({role:'sysadmin',capabilities:['DISPATCH']},entry),true)
  assert.equal(managementAbility({role:'sysadmin',capabilities:['DISPATCH']},'BUSINESS_READ'),false)
  for(const capabilities of [[],['AREA_SCOPE'],['BUSINESS_READ']])assert.equal(canAccessEntry({role:'sysadmin',capabilities},entry),false)
  assert.deepEqual(routeAccess({path:'/admin/dispatch/9',meta:{requiresAuth:true,entryId:'admin-dispatch'}}),{path:'/login',replace:true})
})
test('actual SFC: DISPATCH-only sysadmin reads candidates but never management list', async () => {
  const a=actualView();try{await tick();assert.equal(a.view.flow.phase,'ready');assert.equal(a.queries.length,0);assert.equal(a.view.canRead.value,false)}finally{a.dispose()}
})
test('actual SFC: dispatcher AREA_SCOPE fetches only pending managed query', async () => {
  const a=actualView({role:'dispatcher',capabilities:['AREA_SCOPE']});try{await tick();assert.deepEqual(a.queries,[{view:'managed',page:1,page_size:20,status:'PENDING_DISPATCH'}]);assert.equal(a.view.canDispatch.value,true)}finally{a.dispose()}
})
test('actual SFC: missing permission never issues GETs even on manually typed dispatch deep link', async () => {
  let reads=0;const a=actualView({capabilities:['BUSINESS_READ'],candidates:async()=>{reads++;return data()}})
  try{await tick();assert.equal(reads,0);assert.equal(a.queries.length,0);assert.equal(a.view.flow.snapshot,null)}finally{a.dispose()}
})
test('actual SFC: invalid unsafe route never sends recommendation HTTP', async () => {
  let reads=0;const a=actualView({id:'9007199254740993',candidates:async()=>{reads++;return data()}})
  try{await tick();assert.equal(reads,0);assert.equal(a.view.flow.phase,'error');assert.match(a.view.flow.message,/安全整数/)}finally{a.dispose()}
})
test('actual SFC: page switch late candidate response cannot overwrite current order', async () => {
  let release;const a=actualView({candidates:id=>id==='9'?new Promise(r=>{release=r}):Promise.resolve(data(Number(id)))})
  try{a.route.params.id='10';await tick();release(data(9));await tick();assert.equal(a.view.flow.snapshot.order_id,10);assert.equal(a.view.flow.reason,'');assert.equal(a.view.flow.selected,null)}finally{a.dispose()}
})
test('actual SFC: logout invalidates pending response and destroys previous candidates immediately', async () => {
  let release;const a=actualView({candidates:()=>new Promise(r=>{release=r})})
  try{a.auth.status='loggingOut';assert.equal(a.view.flow.snapshot,null);release(data());await tick();assert.equal(a.view.flow.snapshot,null);assert.equal(a.view.flow.phase,'idle');assert.equal(a.view.canDispatch.value,false)}finally{a.dispose()}
})
test('actual SFC: capability removal clears selection, reason, version and list', async () => {
  const a=actualView({capabilities:['DISPATCH','BUSINESS_READ']});try{await tick();a.view.flow.choose(1);a.view.flow.reason='Old';a.auth.user.capabilities=[];assert.equal(a.view.flow.snapshot,null);assert.equal(a.view.flow.selected,null);assert.equal(a.view.flow.reason,'');assert.deepEqual(a.view.list.value,[])}finally{a.dispose()}
})
test('actual SFC: destroyed route cannot receive late candidates or list', async () => {
  let release, listRelease;const a=actualView({capabilities:['DISPATCH','BUSINESS_READ'],candidates:()=>new Promise(r=>{release=r}),list:()=>new Promise(r=>{listRelease=r})})
  a.dispose();release(data());listRelease({items:[{id:9,status:'PENDING_DISPATCH'}],total:1});await tick();assert.equal(a.view.flow.snapshot,null);assert.deepEqual(a.view.list.value,[])
})
test('actual SFC: list permission changes cancel old in-flight list, not just hide menu', async () => {
  let release;const a=actualView({capabilities:['DISPATCH','BUSINESS_READ'],list:()=>new Promise(r=>{release=r})})
  try{a.auth.user.capabilities=['DISPATCH'];release({items:[{id:9,status:'PENDING_DISPATCH',title:'Old'}],total:1});await tick();assert.equal(a.view.list.value.length,0);assert.equal(a.view.canRead.value,false)}finally{a.dispose()}
})
test('flow clear discards late successful POST receipt, verification and sensitive state', async () => {
  let release, reads=0;const f=dispatchFlow({candidates:async()=>data(),assign:()=>new Promise(r=>{release=r}),detail:async()=>{reads++}},()=>true)
  await f.load(9);f.choose(1);const pending=f.send();f.clear();release(ack(9));await pending
  assert.equal(f.snapshot,null);assert.equal(f.result,null);assert.equal(f.verification,null);assert.equal(reads,0);assert.equal(f.phase,'idle')
})
test('late verification response cannot repopulate another order after a successful old write', async () => {
  let release;const f=dispatchFlow({candidates:async id=>data(Number(id)),assign:async()=>ack(9),detail:()=>new Promise(r=>{release=r})},()=>true)
  await f.load(9);f.choose(1);const pending=f.send();await tick();await f.load(10);release({id:9,status:'PENDING_ACCEPT',version:4});await pending
  assert.equal(f.snapshot.order_id,10);assert.equal(f.verification,null);assert.equal(f.result,null)
})
test('unknown result with failed or forbidden read stays unknown and cannot replay', async () => {
  for(const status of [403,404,503]){let writes=0;const f=dispatchFlow({candidates:async()=>data(),assign:async()=>{writes++;throw Object.assign(new Error(),{status:0})},detail:async()=>{throw Object.assign(new Error(),{status})}},()=>true)
    await f.load(9);f.choose(1);await f.send();assert.equal(f.phase,'unknown_result');assert.equal(f.verification,null);assert.equal(f.snapshot,null);await assert.rejects(f.send());assert.equal(writes,1)}
})
test('GET 409 during reconciliation cannot be promoted to this request success', async () => {
  let writes=0;const f=dispatchFlow({candidates:async()=>data(),assign:async()=>{writes++;throw Object.assign(new Error(),{status:0})},detail:async()=>{throw Object.assign(new Error(),{status:409})}},()=>true)
  await f.load(9);f.choose(1);await f.send();assert.equal(f.phase,'unknown_result');assert.equal(f.result,null);assert.equal(writes,1)
})
