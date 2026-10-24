// Real production HTTP through Vite, not a browser/DOM test. Controlled random
// sessions come only from the existing temporary Worker HTTP fixture.
import { test, after } from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import axios from 'axios'
import { apiClient } from '../src/api/client.js'
import { replaceCredential } from '../src/api/credentials.js'
import { managedOrders, orderDetail, orderTimeline, reviewOrder, createOrder, submitOrder, editOrder, locations, categories } from '../src/api/orders.js'
import { workers, updateWorker, workerWorkload } from '../src/api/workers.js'
import { workerEditor, serviceAreas } from '../src/utils/workers.js'
import { reviewFlow } from '../src/utils/adminOrders.js'
const base = process.env.CAMPUSFIX_ADMIN_TEST_BASE
if (!/^http:\/\/127\.0\.0\.1:\d{1,5}$/.test(base || '') || !process.env.CAMPUSFIX_ADMIN_TEST_CREDENTIALS) throw new Error('Requires isolated local test server and temporary fixture credential file; see W5-D4 report')
const credentials = JSON.parse(readFileSync(process.env.CAMPUSFIX_ADMIN_TEST_CREDENTIALS, 'utf8'))
const http = axios.getAdapter('http'), adapter = apiClient.defaults.adapter
let requests = 0
apiClient.defaults.adapter = (config) => { requests++; return http({ ...config, baseURL: `${base}/api` }) }
const as = (name) => replaceCredential(credentials[name])
after(() => { replaceCredential(); apiClient.defaults.adapter = adapter; console.log(`PASS frontend modules made ${requests} real proxied HTTP requests (not browser E2E)`) })
const prepared = {}
test('real options and authorized managed list; reporter/worker denied; sysadmin read distinct from review/manage', async () => {
  as('demo_dispatcher')
  const roots = await locations(), cats = await categories()
  assert.ok(roots.items.length && cats.items.length)
  prepared.areas = await serviceAreas(locations); prepared.category = cats.items[0].id
  const page = await managedOrders({ page: 1, page_size: 100 }); assert.ok(Array.isArray(page.items))
  for (const name of ['demo_reporter01', 'demo_worker01']) { as(name); await assert.rejects(managedOrders(), { status: 403 }); await assert.rejects(workers(), { status: 403 }) }
  as('demo_sysadmin'); const visible = await managedOrders(); await assert.rejects(workers(), { status: 403 })
  assert.ok(visible.items.length)
  await assert.rejects(reviewOrder(visible.items[0].id, { action: 'RETURN', expected_version: visible.items[0].version, reason: '无审核权限的可见工单' }), { status: 403 })
})
test('real RETURN → reporter supplement → resubmit → APPROVE; same ID/version/history', async () => {
  as('demo_reporter01')
  const body = { category_id: prepared.category, location_id: prepared.areas[0].id, title: 'W5-D4 虚构管理联调样本', description: '仅临时测试数据', requested_urgency: 'NORMAL', contact_name: '虚构联系人', contact_phone: '00000000000' }
  const draft = await createOrder(body); const pending = await submitOrder(draft.id, draft.version)
  as('demo_sysadmin'); await assert.rejects(reviewOrder(draft.id, { action: 'RETURN', expected_version: pending.version, reason: '测试' }), { status: 404 })
  as('demo_dispatcher')
  const flow = reviewFlow({ review: reviewOrder, detail: orderDetail }, await orderDetail(draft.id))
  await flow.send({ action: 'RETURN', reason: '请补充虚构故障描述', internal_note: '仅管理人员可读的临时测试备注' })
  assert.equal(flow.current.status, 'PENDING_SUPPLEMENT')
  as('demo_reporter01'); const returned = await orderDetail(draft.id)
  const timeline = await orderTimeline(draft.id, { page: 1, page_size: 100 })
  assert.ok(timeline.items.some((e) => e.public_note === '请补充虚构故障描述')); assert.ok(timeline.items.every((e) => !Object.hasOwn(e, 'internal_note')))
  const edited = await editOrder(draft.id, { ...body, expected_visit_at: null, description: '虚构描述已补充', expected_version: returned.version })
  const resubmitted = await submitOrder(draft.id, edited.version)
  as('demo_dispatcher'); const approved = await reviewOrder(draft.id, { action: 'APPROVE', expected_version: resubmitted.version, category_id: body.category_id, approved_urgency: 'HIGH' })
  assert.equal(approved.status, 'PENDING_DISPATCH'); assert.equal((await orderDetail(draft.id)).id, draft.id)
  await assert.rejects(reviewOrder(draft.id, { action: 'RETURN', expected_version: pending.version, reason: '旧版本' }), { status: 409 })
  const managedTimeline = await orderTimeline(draft.id, { page: 1, page_size: 100 })
  assert.ok(managedTimeline.items.some((e) => e.internal_note === '仅管理人员可读的临时测试备注'))
  console.log(`ACTUAL review chain temporary order id=${draft.id} final_status=${approved.status} version=${approved.version}`)
})
test('real REJECT and mandatory public reason validation', async () => {
  as('demo_reporter01')
  const draft = await createOrder({ category_id: prepared.category, location_id: prepared.areas[0].id, title: '虚构拒绝样本', description: '临时数据', requested_urgency: 'NORMAL', contact_name: '虚构联系人', contact_phone: '00000000000' })
  const pending = await submitOrder(draft.id, draft.version)
  as('demo_dispatcher')
  await assert.rejects(reviewOrder(draft.id, { action: 'REJECT', expected_version: pending.version, reason: '' }), (e) => e.status === 422 && e.field === 'reason')
  const result = await reviewOrder(draft.id, { action: 'REJECT', expected_version: pending.version, reason: '虚构测试无效报修' })
  assert.equal(result.status, 'CANCELLED')
})
test('real complete-worker hydration, capacity-only save and skill removal preserve unrelated fields', async () => {
  as('demo_dispatcher')
  const api = { list: workers, update: updateWorker, workload: workerWorkload }
  const flow = workerEditor(api, 1); await flow.load()
  const original = flow.form
  await updateWorker(1, { ...original, active: false, emergency_duty: true, availability: 'OFF_DUTY', max_concurrent_tasks: 3, skills: [{ skill_id: 1, level: 4, certified: true }, { skill_id: 2, level: 2, certified: false }] })
  await flow.load(); const before = structuredClone(flow.form)
  assert.equal(before.active, false); assert.equal(before.emergency_duty, true)
  flow.form.max_concurrent_tasks = 4; await flow.save(prepared.areas)
  assert.deepEqual(flow.form, { ...before, max_concurrent_tasks: 4 }); assert.equal(flow.verification.load.max_concurrent_tasks, 4)
  await flow.load(); flow.form.skills = [flow.form.skills[0]]; await flow.save(prepared.areas)
  assert.equal(flow.form.skills.length, 1); assert.equal(flow.form.skills[0].level, 4)
  await flow.load(); flow.form.max_concurrent_tasks = 1
  await assert.rejects(flow.save(prepared.areas)) // client blocks known capacity lower than load
  await assert.rejects(updateWorker(1, { ...flow.form }), (e) => e.status === 422 && e.field === 'max_concurrent_tasks')
  const foreign = prepared.areas.find((a) => a.id === 2)
  assert.ok(foreign); await assert.rejects(updateWorker(1, { ...flow.form, service_area_id: foreign.id, max_concurrent_tasks: 4 }), { status: 403 })
  console.log('ACTUAL false/true/OFF_DUTY and 2 skills preserved by capacity-only update; skill removal re-read; 422/403 verified')
})
