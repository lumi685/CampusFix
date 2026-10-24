import { test, after } from 'node:test'
import './supplement.test.js'
import assert from 'node:assert/strict'
import { readFileSync, readdirSync } from 'node:fs'
import { join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { apiClient } from '../src/api/client.js'
import { replaceCredential } from '../src/api/credentials.js'
import { reportPayload, statuses, orderError } from '../src/utils/orders.js'
import { reportSubmission } from '../src/utils/reportSubmission.js'
import { reportedOrders, createOrder, submitOrder, orderId } from '../src/api/orders.js'
const originalAdapter = apiClient.defaults.adapter
after(() => { apiClient.defaults.adapter = originalAdapter; replaceCredential() })
const form = () => ({ category_id: 1, location_id: 11, title: '虚构故障', description: '虚构描述', requested_urgency: 'NORMAL', contact_name: '虚构联系人', contact_phone: '00000000000', expected_visit_at: null })
const draft = { id: 9, status: 'DRAFT', version: 1 }
test('all Element Plus template tags are explicitly imported and registered (static regression)', () => {
  const root = fileURLToPath(new URL('../src/', import.meta.url))
  const main = readFileSync(join(root, 'main.js'), 'utf8')
  const imported = new Set((main.match(/import\s*\{([^}]+)\}\s*from ['"]element-plus['"]/)?.[1] || '').split(',').map((name) => name.trim()))
  const registered = new Set([...main.matchAll(/\.use\((El\w+)\)/g)].map((match) => match[1]))
  const missing = []
  function scan(directory) {
    for (const entry of readdirSync(directory, { withFileTypes: true })) {
      const path = join(directory, entry.name)
      if (entry.isDirectory()) scan(path)
      else if (entry.name.endsWith('.vue')) {
        const source = readFileSync(path, 'utf8')
        for (const match of source.matchAll(/<(el-[a-z-]+)\b/g)) {
          const component = match[1].split('-').map((part) => part[0].toUpperCase() + part.slice(1)).join('')
          if (!imported.has(component) || !registered.has(component)) missing.push(component)
        }
      }
    }
  }
  scan(root)
  assert.deepEqual([...new Set(missing)].sort(), [], 'Unregistered template components; build alone cannot detect this')
})
test('form fields, unicode limits, UTC, and forbidden identity fields', () => {
  const input = { ...form(), reporter_id: 99, role: 'sysadmin', order_no: 'forged', status: 'CLOSED' }
  assert.deepEqual(Object.keys(reportPayload(input)).sort(), ['category_id', 'location_id', 'title', 'description', 'requested_urgency', 'contact_name', 'contact_phone'].sort())
  for (const key of ['title', 'description', 'contact_name', 'contact_phone']) assert.throws(() => reportPayload({ ...form(), [key]: ' ' }))
  assert.throws(() => reportPayload({ ...form(), title: '😀'.repeat(121) }))
  assert.equal([...reportPayload({ ...form(), title: '😀'.repeat(120) }).title].length, 120)
  for (const value of [0, -1, '1', null]) assert.throws(() => reportPayload({ ...form(), category_id: value }))
  assert.throws(() => reportPayload({ ...form(), requested_urgency: 'INVALID' }))
  assert.throws(() => reportPayload({ ...form(), expected_visit_at: 'invalid' }))
  assert.equal(reportPayload({ ...form(), expected_visit_at: new Date('2030-01-01T00:00:00Z') }).expected_visit_at, '2030-01-01T00:00:00.000Z')
  assert.equal(reportPayload({ ...form(), expected_visit_at: new Date('2030-01-01T08:30:00+08:00') }).expected_visit_at, '2030-01-01T00:30:00.000Z')
  assert.equal(Object.keys(statuses).length, 10)
})
test('submit failure retains the same draft and never repeats create', async () => {
  let creates = 0, submits = 0
  const flow = reportSubmission({ create: async () => { creates++; return draft }, submit: async (id, version) => {
    assert.equal(id, 9); assert.equal(version, 1)
    if (++submits === 1) throw Object.assign(new Error('offline'), { status: 0 })
    return { id, status: 'PENDING_REVIEW', version: 2 }
  }, detail: async () => { throw new Error('offline') } })
  await assert.rejects(flow.send(reportPayload(form())))
  assert.deepEqual(flow.draft, { id: 9, version: 1 })
  assert.equal((await flow.send()).status, 'PENDING_REVIEW')
  assert.equal(creates, 1); assert.equal(submits, 2)
})
test('409 refreshes the same draft version without auto resubmission', async () => {
  let calls = 0
  const flow = reportSubmission({ create: async () => draft, submit: async (id, v) => {
    calls++; if (calls === 1) throw Object.assign(new Error('conflict'), { status: 409 })
    assert.equal(v, 3); return { id, status: 'PENDING_REVIEW' }
  }, detail: async () => ({ id: 9, status: 'DRAFT', version: 3 }) })
  await assert.rejects(flow.send(form())); assert.equal(flow.draft.version, 3); assert.equal(calls, 1)
  await flow.send(); assert.equal(calls, 2)
})
test('lost submit response recovers a real submitted order', async () => {
  const flow = reportSubmission({ create: async () => draft, submit: async () => { throw Object.assign(new Error('conflict'), { status: 409 }) }, detail: async () => ({ id: 9, status: 'PENDING_REVIEW', order_no: 'WX203001010001', submitted_at: '2030-01-01T00:00:00Z' }) })
  assert.equal((await flow.send(form())).order_no, 'WX203001010001')
})
test('uncertain creation blocks blind duplication and duplicate clicks are ignored', async () => {
  let resolve, calls = 0
  const flow = reportSubmission({ create: () => { calls++; return new Promise((r) => { resolve = r }) }, submit: async () => ({ id: 9 }) })
  const first = flow.send(form()); assert.equal(await flow.send(form()), null); resolve(draft); await first; assert.equal(calls, 1)
  const uncertain = reportSubmission({ create: async () => { throw new Error('network') } })
  await assert.rejects(uncertain.send(form())); assert.equal(uncertain.uncertain, true); await assert.rejects(uncertain.send(form()))
})
test('order API fixes reported view, attaches credentials only through existing client, and sends only version on submit', async () => {
  replaceCredential('unusable-unit-sentinel')
  apiClient.defaults.adapter = async (config) => {
    assert.equal(config.baseURL, '/api'); assert.equal(config.authRequired, true)
    if (config.method === 'get') assert.equal(config.params.view, 'reported')
    if (config.url.endsWith('/submit')) assert.deepEqual(JSON.parse(config.data), { expected_version: 1 })
    return { config, status: 200, data: { code: 0, message: 'success', request_id: 'req_unit', data: draft } }
  }
  await reportedOrders({ view: 'assigned' }); await createOrder(reportPayload(form())); await submitOrder(9, 1)
  for (const id of ['../auth', '0', '-1', '1?x=1', '1.5']) assert.throws(() => orderId(id))
  assert.match(orderError({ status: 404 }), /不存在或无权/)
  assert.match(orderError({ status: 409 }), /不会自动创建/)
})
