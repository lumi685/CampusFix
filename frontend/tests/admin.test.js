import { test, after } from 'node:test'
import assert from 'node:assert/strict'
import { randomBytes } from 'node:crypto'
import { readFileSync } from 'node:fs'
import { AxiosError } from 'axios'
import { apiClient } from '../src/api/client.js'
import { replaceCredential } from '../src/api/credentials.js'
import { managedOrders, reviewOrder } from '../src/api/orders.js'
import { workers, workerWorkload, updateWorker } from '../src/api/workers.js'
import { managementAbility, managedQuery, reviewPayload, reviewFlow } from '../src/utils/adminOrders.js'
import { workerForm, workerPayload, skillOptions, completeWorker, workerEditor, serviceAreas } from '../src/utils/workers.js'
import { navigation, canAccessEntry } from '../src/router/navigation.js'
const options = () => [{ id: 1, code: 'A', name: '真实读取技能A', requires_certificate: true }, { id: 2, code: 'B', name: '真实读取技能B', requires_certificate: false }, { id: 3, code: 'C', name: '尚未分配技能', requires_certificate: false }]
const record = (id = 1) => ({ id, worker_no: `test-${id}`, display_name: '虚构测试', active: false, emergency_duty: true, service_area_id: 1, availability: 'OFF_DUTY', max_concurrent_tasks: 3, active_tasks: 2, skills: [{ skill_id: 1, level: 4, certified: true }, { skill_id: 2, level: 2, certified: false }] })
const area = [{ id: 1, location_type: 'CAMPUS', label: '测试校区' }]
const data = (r = record()) => ({ items: [r], page: 1, page_size: 100, total: 1, skill_options: options() })
const order = () => ({ id: 9, version: 2, status: 'PENDING_REVIEW' })
const failure = (status, field = '') => Object.assign(new Error('test failure'), { status, field })
const user = (role, capabilities = []) => ({ role, capabilities, scope_location_ids: [1] })
const originalAdapter = apiClient.defaults.adapter
after(() => { apiClient.defaults.adapter = originalAdapter; replaceCredential() })

test('manager reads actual false/true/status and every skill without mutation or defaults', () => {
  const original = record(), form = workerForm(original, options())
  assert.equal(form.active, false); assert.equal(form.emergency_duty, true); assert.equal(form.availability, 'OFF_DUTY'); assert.deepEqual(form.skills, original.skills)
  form.max_concurrent_tasks = 4
  assert.deepEqual(workerPayload(form, options(), 2, area), { service_area_id: 1, active: false, emergency_duty: true, availability: 'OFF_DUTY', max_concurrent_tasks: 4, skills: original.skills })
  form.skills[0].level = 1; assert.equal(original.skills[0].level, 4)
})
test('missing or invalid management boolean/skills never gains guessed defaults', () => {
  for (const key of ['active', 'emergency_duty', 'skills', 'service_area_id', 'active_tasks']) { const r = record(); delete r[key]; assert.throws(() => workerForm(r, options())) }
  for (const key of ['active', 'emergency_duty']) for (const v of [null, 0, 'true']) assert.throws(() => workerForm({ ...record(), [key]: v }, options()))
  assert.throws(() => workerForm(record(), undefined)); assert.throws(() => skillOptions([{ ...options()[0], requires_certificate: 1 }]))
})
test('capacity/skill integer limits, duplicates and actual options are enforced', () => {
  const form = workerForm(record(), options())
  for (const n of [0, 101, 1.5, '4', 1]) assert.throws(() => workerPayload({ ...form, max_concurrent_tasks: n }, options(), 2, area))
  for (const level of [0, 6, 2.1, '3']) assert.throws(() => workerPayload({ ...form, skills: [{ skill_id: 1, level, certified: true }] }, options(), 2, area))
  assert.throws(() => workerPayload({ ...form, skills: [form.skills[0], form.skills[0]] }, options(), 2, area))
  assert.throws(() => workerPayload({ ...form, skills: [{ skill_id: 99, level: 3, certified: false }] }, options(), 2, area))
  assert.equal(workerPayload({ ...form, skills: [] }, options(), 2, area).skills.length, 0)
  assert.equal(workerPayload({ ...form, skills: [{ skill_id: 1, level: 3, certified: false }] }, options(), 2, area).skills[0].certified, false)
})
test('campus/building service options come from real catalog; floor/room never selectable', async () => {
  const calls = []
  const result = await serviceAreas(async (id) => { calls.push(id); return { items: id == null ? [{ id: 1, name: '校区', location_type: 'CAMPUS' }] : [{ id: 2, name: '楼宇', location_type: 'BUILDING' }, { id: 3, name: '楼层', location_type: 'FLOOR' }] } })
  assert.deepEqual(calls, [undefined, 1]); assert.deepEqual(result.map((a) => a.id), [1, 2]); assert.equal(result[1].label, '校区 / 楼宇')
  assert.throws(() => workerPayload({ ...workerForm(record(), options()), service_area_id: 3 }, options(), 2, result))
  assert.throws(() => workerPayload(workerForm(record(), options()), options(), 2, []))
})
test('complete profile re-read traverses authorized unfiltered pages, not current table subset', async () => {
  const queries = []
  const result = await completeWorker(async (q) => { queries.push(q); return { ...data(q.page === 1 ? record(2) : record(1)), total: 101 } }, 1)
  assert.equal(result.record.id, 1); assert.deepEqual(queries, [{ page: 1, page_size: 100 }, { page: 2, page_size: 100 }])
  await assert.rejects(completeWorker(async () => ({ ...data(), items: [], total: 0 }), 1), { status: 404 })
})
test('edit only capacity: PUT full fields then GET full profile and GET workload; ignore partial response', async () => {
  const calls = []; let saved
  const api = { list: async () => { calls.push('list'); return data(saved ? { ...record(), ...saved } : record()) }, update: async (_, payload) => { calls.push('put'); saved = payload; return { id: 1, max_concurrent_tasks: 4 } }, workload: async () => { calls.push('load'); return { active_tasks: 2, max_concurrent_tasks: 4 } } }
  const flow = workerEditor(api, 1); await flow.load(); flow.form.max_concurrent_tasks = 4; await flow.save(area)
  assert.deepEqual(calls, ['list', 'put', 'list', 'load']); assert.equal(saved.active, false); assert.equal(saved.emergency_duty, true); assert.deepEqual(saved.skills, record().skills); assert.equal(flow.phase, 'confirmed'); assert.equal(flow.form.active, false)
})
test('unread fields/options or network loading failure disable any PUT', async () => {
  let writes = 0
  for (const bad of [{ ...data(), skill_options: undefined }, { ...data(), items: [{ ...record(), active: undefined }] }]) {
    const flow = workerEditor({ list: async () => bad, update: async () => { writes++ } }, 1)
    await assert.rejects(flow.load()); await assert.rejects(flow.save(area)); assert.equal(flow.phase, 'blocked')
  }
  const offline = workerEditor({ list: async () => { throw failure(0) }, update: async () => { writes++ } }, 1)
  await assert.rejects(offline.load()); await assert.rejects(offline.save(area)); assert.equal(offline.form, null); assert.equal(writes, 0)
})
test('422 retains edits; 403/404 clears unauthorized snapshot without retry', async () => {
  for (const status of [422, 403, 404]) {
    const flow = workerEditor({ list: async () => data(), update: async () => { throw failure(status) }, workload: async () => ({ active_tasks: 2 }) }, 1)
    await flow.load(); flow.form.max_concurrent_tasks = 4; await assert.rejects(flow.save(area))
    if (status === 422) { assert.equal(flow.phase, 'ready'); assert.equal(flow.form.max_concurrent_tasks, 4) }
    else { assert.equal(flow.phase, 'blocked'); assert.equal(flow.form, null); assert.equal(flow.verification, null) }
  }
})
test('network/5xx never retries PUT and independently re-reads actual profile/load', async () => {
  for (const status of [0, 500, 503]) {
    let writes = 0, reads = 0, loads = 0
    const flow = workerEditor({ list: async () => { reads++; return data() }, update: async () => { writes++; throw failure(status) }, workload: async () => { loads++; return {} } }, 1)
    await flow.load(); await assert.rejects(flow.save(area)); await assert.rejects(flow.save(area)); assert.equal(writes, 1); assert.equal(reads, 2); assert.equal(loads, 1); assert.equal(flow.phase, 'unknown')
  }
})
test('success with failed full re-read remains unknown, never merges partial DTO or replays', async () => {
  let reads = 0, writes = 0
  const flow = workerEditor({ list: async () => { if (++reads > 1) throw failure(503); return data() }, update: async () => { writes++; return { id: 1, max_concurrent_tasks: 4 } } }, 1)
  await flow.load(); await assert.rejects(flow.save(area)); await assert.rejects(flow.save(area)); assert.equal(flow.phase, 'unknown'); assert.equal(writes, 1); assert.equal(flow.form.active, false); assert.equal(flow.verification, null)
})
test('permission loss during uncertain-write reconciliation removes stale management data', async () => {
  let reads = 0
  const flow = workerEditor({ list: async () => { if (++reads > 1) throw failure(404); return data() }, update: async () => { throw failure(503) } }, 1)
  await flow.load(); await assert.rejects(flow.save(area)); assert.equal(flow.phase, 'blocked'); assert.equal(flow.form, null); assert.equal(flow.snapshot, null)
})
test('different drawer instances re-read actual data; no previous flags/skills persist', async () => {
  const a = workerEditor({ list: async () => ({ ...data(), items: [record(1), { ...record(2), active: true, emergency_duty: false, skills: [] }], total: 2 }) }, 1)
  await a.load(); a.form.skills = []
  const b = workerEditor({ list: aApi }, 2)
  async function aApi() { return { ...data(), items: [{ ...record(2), active: true, emergency_duty: false, skills: [] }] } }
  await b.load(); assert.equal(b.form.active, true); assert.equal(b.form.emergency_duty, false); assert.deepEqual(b.form.skills, [])
})
test('worker duplicate save clicks produce one PUT only', async () => {
  let release, writes = 0
  const flow = workerEditor({ list: async () => data(), update: async () => { writes++; await new Promise((r) => { release = r }) }, workload: async () => ({}) }, 1)
  await flow.load(); const first = flow.save(area); assert.equal(await flow.save(area), null); release(); await first; assert.equal(writes, 1)
})
test('BUSINESS_READ, REVIEW and WORKER_MANAGE remain independent for sysadmin', () => {
  for (const cap of ['BUSINESS_READ', 'REVIEW', 'WORKER_MANAGE']) {
    const u = user('sysadmin', [cap])
    for (const action of ['BUSINESS_READ', 'REVIEW', 'WORKER_MANAGE']) assert.equal(managementAbility(u, action), action === cap)
  }
  assert.equal(managementAbility(user('sysadmin', ['AREA_SCOPE']), 'REVIEW'), false)
  assert.equal(managementAbility(user('dispatcher', ['AREA_SCOPE']), 'REVIEW'), true)
  for (const role of ['reporter', 'worker']) for (const id of ['admin-orders', 'admin-review', 'admin-workers']) assert.equal(canAccessEntry(user(role), navigation.find((n) => n.id === id)), false)
})
test('managed queries whitelist status/category/page, never location_id or reported view', () => {
  assert.deepEqual(managedQuery({ page: 2, status: 'PENDING_REVIEW', category: 1, location_id: 7, view: 'reported' }), { view: 'managed', page: 2, page_size: 20, status: 'PENDING_REVIEW', category_id: 1 })
  assert.throws(() => managedQuery({ status: 'INVALID' })); assert.throws(() => managedQuery({ page: 0 }))
})
test('review action fields are mutually exclusive, version from actual detail', () => {
  const common = { category_id: 1, approved_urgency: 'HIGH', reason: '公开原因', internal_note: '内部备注', status: 'CLOSED', expected_version: 999 }
  assert.deepEqual(reviewPayload(order(), { ...common, action: 'APPROVE' }), { action: 'APPROVE', expected_version: 2, category_id: 1, approved_urgency: 'HIGH', internal_note: '内部备注' })
  for (const action of ['RETURN', 'REJECT']) { const p = reviewPayload(order(), { ...common, action }); assert.equal(p.reason, '公开原因'); assert.equal(Object.hasOwn(p, 'category_id'), false); assert.throws(() => reviewPayload(order(), { ...common, action, reason: ' \u3000' })) }
  assert.throws(() => reviewPayload({ ...order(), status: 'CANCELLED' }, { ...common, action: 'RETURN' }))
})
test('review Unicode scalar boundaries and invalid surrogate/control validation match service', () => {
  assert.equal([...reviewPayload(order(), { action: 'RETURN', reason: '😀'.repeat(500), internal_note: '😀'.repeat(2000) }).reason].length, 500)
  assert.throws(() => reviewPayload(order(), { action: 'RETURN', reason: '😀'.repeat(501) }))
  assert.throws(() => reviewPayload(order(), { action: 'RETURN', reason: '原因', internal_note: '😀'.repeat(2001) }))
  assert.throws(() => reviewPayload(order(), { action: 'RETURN', reason: '\ud800' }))
  assert.throws(() => reviewPayload(order(), { action: 'RETURN', reason: '原因', internal_note: '\0' }))
})
test('three review actions refresh real detail and lock after success', async () => {
  for (const action of ['APPROVE','RETURN','REJECT']) {
    let reads = 0
    const target = { APPROVE: 'PENDING_DISPATCH', RETURN: 'PENDING_SUPPLEMENT', REJECT: 'CANCELLED' }[action]
    const flow = reviewFlow({ review: async () => ({ ...order(), status: target, version: 3 }), detail: async () => { reads++; return { ...order(), status: target, version: 3 } } }, order())
    await flow.send({ action, category_id: 1, approved_urgency: 'NORMAL', reason: '原因' }); assert.equal(reads, 1); assert.equal(flow.current.status, target); assert.equal(flow.blocked, true)
  }
})
test('review 409/5xx/network errors re-read, preserve separated text and never replay', async () => {
  for (const status of [409, 500, 503, 0]) {
    let writes = 0, reads = 0
    const flow = reviewFlow({ review: async () => { writes++; throw failure(status) }, detail: async () => { reads++; return order() } }, order())
    const form = { action: 'RETURN', reason: '公开原因', internal_note: '内部备注' }
    await assert.rejects(flow.send(form)); await assert.rejects(flow.send(form)); assert.equal(writes, 1); assert.equal(reads, 1); assert.equal(form.reason, '公开原因'); assert.equal(form.internal_note, '内部备注')
  }
})
test('review missing read permission/failure does not fabricate verification', async () => {
  const flow = reviewFlow({ review: async () => { throw failure(503) }, detail: async () => { throw failure(404) } }, order())
  await assert.rejects(flow.send({ action: 'RETURN', reason: '原因' })); assert.equal(flow.verification, null); assert.equal(flow.blocked, true); assert.equal(flow.denied, true)
})
test('review duplicate clicks do not produce extra POST', async () => {
  let release, writes = 0
  const flow = reviewFlow({ review: async () => { writes++; await new Promise((r) => { release = r }); return {} }, detail: async () => order() }, order())
  const first = flow.send({ action: 'RETURN', reason: '原因' }); assert.equal(await flow.send({ action: 'RETURN', reason: '第二次' }), null); release(); await first; assert.equal(writes, 1)
})
test('API adapters use authenticated real paths and safe field hints only', async () => {
  replaceCredential(randomBytes(32).toString('hex')); const calls = []
  apiClient.defaults.adapter = async (config) => { calls.push(config); return { config, status: 200, headers: {}, data: { code: 0, request_id: 'req_unit', data: {} } } }
  await managedOrders({ view: 'reported', page: 2 }); await workers({ page: 1 }); await workerWorkload(1); await updateWorker(1, workerPayload(workerForm(record(), options()), options(), 2, area)); await reviewOrder(9, reviewPayload(order(), { action: 'RETURN', reason: '原因' }))
  assert.deepEqual(calls.map((c) => c.url), ['/work-orders', '/workers', '/workers/1/workload', '/workers/1', '/work-orders/9/review'])
  assert.equal(calls[0].params.view, 'managed'); assert.ok(calls.every((c) => c.authRequired && c.headers.get('Authorization')))
  apiClient.defaults.adapter = async (config) => { throw new AxiosError('fail', 'ERR_BAD_RESPONSE', config, null, { status: 422, data: { code: 'VALIDATION_ERROR', data: { field: 'skills[0].level', token_hash: 'never copy' }, request_id: 'req_unit' } }) }
  await assert.rejects(workers(), (e) => e.field === 'skills[0].level' && !Object.hasOwn(e, 'config') && !Object.hasOwn(e, 'token_hash'))
})
test('static UI wiring: separate admin details, confirmations and no note carryover', () => {
  const source = (name) => readFileSync(new URL(`../src/${name}`, import.meta.url), 'utf8')
  const routes = source('router/index.js')
  for (const key of ["'admin-orders': AdminOrdersView", "'admin-review': AdminReviewView", "'admin-workers': AdminWorkersView"]) assert.ok(routes.includes(key))
  assert.ok(source('views/AdminOrdersView.vue').includes('AdminOrderDetailDrawer')); assert.ok(!source('views/AdminOrdersView.vue').includes('`/orders/'))
  const review = source('components/OrderReviewDrawer.vue')
  assert.ok(review.includes('ElMessageBox.confirm')); assert.ok(review.includes("reason: '', internal_note: ''")); assert.ok(review.includes('operation.send'))
  assert.ok(source('components/WorkerEditDrawer.vue').includes('ElMessageBox.confirm'))
  assert.ok(!source('components/AdminOrderDetailDrawer.vue').includes('JSON.stringify'))
})
