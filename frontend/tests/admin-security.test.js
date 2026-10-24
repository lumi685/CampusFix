import { test } from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import { parse, compileScript } from '@vue/compiler-sfc'
import { ref, computed } from 'vue'
import { adminError, managementAbility, managedQuery, reviewFlow } from '../src/utils/adminOrders.js'
import { workerEditor, workerForm } from '../src/utils/workers.js'
const failure = (status, field = '') => Object.assign(new Error('test failure'), { status, field })
const options = [{ id: 1, code: 'TEST', name: 'Fictional', requires_certificate: true }]
const record = (id = 1) => ({ id, active: false, emergency_duty: true, service_area_id: 1, availability: 'OFF_DUTY', max_concurrent_tasks: 3, active_tasks: 1, skills: [{ skill_id: 1, level: 4, certified: false }] })
const page = () => ({ items: [record()], total: 1, skill_options: options })

// Execute the actual SFC setup/load/checkLoad functions, without a DOM or browser.
// compiler-sfc is already part of Vue/Vite; no new dependency or UI framework.
function workerView(api) {
  const file = new URL('../src/views/AdminWorkersView.vue', import.meta.url)
  const { descriptor } = parse(readFileSync(file, 'utf8'))
  const compiled = compileScript(descriptor, { id: 'w5d5-security', genDefaultAs: 'component' }).content
  const names = ['ref','onMounted','onBeforeUnmount','workers','workerWorkload','locations','availabilityLabels','skillOptions','workerForm','serviceAreas','adminError','WorkerEditDrawer']
  const code = compiled.replace(/^import .*$/gm, '')
  const component = new Function(...names, `${code}; return component`)(ref, () => {}, () => {}, api.list, api.load, () => {}, {}, (v) => v, workerForm, () => [], adminError, {})
  return component.setup({}, { expose() {} })
}
function orderView(list) {
  const { descriptor } = parse(readFileSync(new URL('../src/views/AdminOrdersView.vue', import.meta.url), 'utf8'))
  const code = compileScript(descriptor, { id: 'w5d5-order-security', genDefaultAs: 'component' }).content.replace(/^import .*$/gm, '')
  const names = ['ref','computed','watch','onBeforeUnmount','managedOrders','authState','managementAbility','managedQuery','adminError','statuses','dateText','CatalogPicker','OrderStatusTag','OrderReviewDrawer','AdminOrderDetailDrawer']
  const component = new Function(...names, `${code}; return component`)(ref, computed, () => {}, () => {}, list, { user: { role: 'dispatcher', capabilities: ['AREA_SCOPE'] } }, managementAbility, managedQuery, adminError, {}, () => '', {}, {}, {}, {})
  return component.setup({ reviewOnly: false }, { expose() {} })
}
test('422 for review/category never incorrectly gives worker capacity advice', () => {
  assert.doesNotMatch(adminError(failure(422, 'category_id')), /容量|任务数/)
  assert.doesNotMatch(adminError(failure(422, 'reason')), /容量|任务数/)
  assert.match(adminError(failure(422, 'max_concurrent_tasks')), /容量|任务数/)
})
test('actual AdminWorkersView list refresh invalidates old workload and pending response', async () => {
  let resolve
  const view = workerView({ list: async () => page(), load: () => new Promise((r) => { resolve = r }) })
  view.workload.value = { worker_id: 99, active_tasks: 20 }
  const pending = view.checkLoad(1)
  await view.load()
  assert.equal(view.workload.value, null)
  resolve({ worker_id: 1, active_tasks: 1 }); await pending
  assert.equal(view.workload.value, null, 'Late load from pre-refresh generation must not reappear')
  assert.equal(view.checkingId.value, undefined)
})
test('actual AdminWorkersView failed refresh clears previously displayed workload', async () => {
  const view = workerView({ list: async () => { throw failure(403) }, load: async () => ({}) })
  view.workload.value = { worker_id: 1, active_tasks: 1 }
  await view.load(); assert.equal(view.workload.value, null); assert.deepEqual(view.items.value, [])
})
test('forbidden personnel refresh closes the editor holding stale authorized data', async () => {
  const view = workerView({ list: async () => { throw failure(403) }, load: async () => ({}) })
  view.editingId.value = 1; await view.load(); assert.equal(view.editingId.value, undefined)
})
test('forbidden order refresh closes stale detail and review drawers', async () => {
  for (const status of [403, 404]) {
    const view = orderView(async () => { throw failure(status) })
    view.detailId.value = 1; view.reviewId.value = 1; await view.load()
    assert.equal(view.detailId.value, undefined); assert.equal(view.reviewId.value, undefined); assert.deepEqual(view.items.value, [])
  }
})
test('mixed summarized capabilities do not turn scope IDs into resource permissions', () => {
  const u = { role: 'sysadmin', capabilities: ['REVIEW'], scope_location_ids: [1, 2] }
  assert.equal(managementAbility(u, 'BUSINESS_READ'), false); assert.equal(managementAbility(u, 'WORKER_MANAGE'), false)
  assert.equal(managementAbility({ ...u, capabilities: ['AREA_SCOPE'] }, 'REVIEW'), false)
})
test('review 403/404 clears display permission and blocks replay', async () => {
  for (const status of [403, 404]) {
    let writes = 0
    const flow = reviewFlow({ review: async () => { writes++; throw failure(status) } }, { id: 1, version: 2, status: 'PENDING_REVIEW' })
    await assert.rejects(flow.send({ action: 'RETURN', reason: 'Fictional reason' })); await assert.rejects(flow.send({ action: 'RETURN', reason: 'Fictional reason' }))
    assert.equal(writes, 1); assert.equal(flow.denied, true)
  }
})
test('review conflict and unknown network results never replay POST', async () => {
  for (const status of [409, 0, 500, 503]) {
    let writes = 0, reads = 0
    const flow = reviewFlow({ review: async () => { writes++; throw failure(status) }, detail: async () => { reads++; return { status: 'PENDING_SUPPLEMENT', version: 3 } } }, { id: 1, version: 2, status: 'PENDING_REVIEW' })
    await assert.rejects(flow.send({ action: 'RETURN', reason: 'test' })); await assert.rejects(flow.send({ action: 'RETURN', reason: 'test' }))
    assert.equal(writes, 1); assert.equal(reads, 1)
  }
})
test('missing management fields/options forbid worker save; no defaults', async () => {
  for (const field of ['active','emergency_duty','skill_options']) {
    let writes = 0
    const result = page(); if (field === 'skill_options') delete result.skill_options; else delete result.items[0][field]
    const editor = workerEditor({ list: async () => result, update: async () => { writes++ } }, 1)
    await assert.rejects(editor.load()); await assert.rejects(editor.save([])); assert.equal(writes, 0); assert.equal(editor.form, null)
  }
})
test('new personnel editor never inherits old edits and complete DTO is re-read after PUT', async () => {
  let reads = 0
  const api = { list: async () => { reads++; return page() }, update: async () => ({ id: 1 }), workload: async () => ({ active_tasks: 1 }) }
  const first = workerEditor(api, 1); await first.load(); first.form.active = true
  const second = workerEditor(api, 1); await second.load(); assert.equal(second.form.active, false)
  await second.save([{ id: 1, location_type: 'CAMPUS' }]); assert.equal(reads, 3); assert.equal(second.form.emergency_duty, true)
})
