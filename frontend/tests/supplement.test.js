import { test } from 'node:test'
import assert from 'node:assert/strict'
import { editForm, editPayload, latestReturn, ownsReportedOrder, supplementFlow } from '../src/utils/orderSupplement.js'
import { catalogPath, catalogContains } from '../src/utils/catalogPath.js'
const detail = () => ({ id: 9, status: 'PENDING_SUPPLEMENT', version: 3, category: { id: 1 }, location: { id: 11 }, title: '虚构故障', description: '描述', requested_urgency: 'NORMAL', contact_name: '虚构联系人', contact_phone: '00000000000', expected_visit_at: '2030-01-01T00:30:00.000Z' })
const fail = (status) => Object.assign(new Error('test failure'), { status })
test('edit payload reuses form validation, is full replacement and preserves UTC across local Date', () => {
  const form = editForm(detail())
  assert.equal(form.expected_visit_at.toISOString(), detail().expected_visit_at)
  assert.equal(editPayload(form, 3).expected_visit_at, detail().expected_visit_at)
  assert.equal(editPayload({ ...form, expected_visit_at: null, reporter_id: 88, status: 'CLOSED' }, 3).expected_visit_at, null)
  assert.equal(Object.hasOwn(editPayload(form, 3), 'reporter_id'), false)
  assert.throws(() => editPayload({ ...form, title: '' }, 3))
  assert.throws(() => editPayload(form, 0))
})
test('ownership comes from reported view, including later pages, never worker role or assignment', async () => {
  let calls = 0
  assert.equal(await ownsReportedOrder(9, 'worker', async () => ({ items: [{ id: 99 }], total: 1 })), false)
  assert.equal(await ownsReportedOrder(9, 'sysadmin', async () => { throw new Error('must not query') }), false)
  assert.equal(await ownsReportedOrder(9, 'worker', async (p) => { calls++; return { items: [{ id: p.page === 2 ? 9 : 99 }], total: 101 } }), true)
  assert.equal(calls, 2)
  await assert.rejects(ownsReportedOrder(9, 'reporter', async () => { throw fail(401) }))
})
test('latest return walks backward across pages and skips content edits; never falls back to previous round', async () => {
  const returned = { action: 'ORDER_RETURNED', from_status: 'PENDING_REVIEW', to_status: 'PENDING_SUPPLEMENT', public_note: '最新原因' }
  const calls = []
  assert.equal((await latestReturn(9, async (_, q) => { calls.push(q.page); return { total: 201, items: q.page === 3 ? [{ action: 'ORDER_CONTENT_UPDATED' }] : q.page === 2 ? [returned, { action: 'ORDER_CONTENT_UPDATED' }] : [{ ...returned, public_note: '旧原因' }] } })).public_note, '最新原因')
  assert.deepEqual(calls, [1, 3, 2])
  await assert.rejects(latestReturn(9, async () => ({ total: 2, items: [returned, { action: 'ORDER_SUBMITTED' }] })))
  await assert.rejects(latestReturn(9, async () => { throw fail(503) }))
})
test('catalog hydration uses real tree labels and supports selecting a root category', async () => {
  const data = new Map([[undefined, [{ id: 1, name: '校区' }]], [1, [{ id: 2, name: '建筑' }]], [2, [{ id: 3, name: '楼层' }]], [3, [{ id: 11, name: '房间' }]]])
  const tree = await catalogPath(11, async (id) => ({ items: data.get(id) || [] }))
  assert.equal(tree[0].children[0].children[0].children[0].name, '房间')
  assert.equal(catalogContains(tree, 11), true)
  const root = await catalogPath(1, async () => ({ items: [{ id: 1, name: '根分类' }] }))
  assert.equal(root[0].name, '根分类')
  await assert.rejects(catalogPath(999, async () => ({ items: [] })))
})
test('save uses same id and latest returned version for submit, never creates an order', async () => {
  const calls = []
  const flow = supplementFlow({ edit: async (id, body) => { calls.push(['PUT', id, body.expected_version]); return { id, version: 4, status: 'PENDING_SUPPLEMENT' } }, submit: async (id, version) => { calls.push(['submit', id, version]); return { id, status: 'PENDING_REVIEW', version: 5 } } }, detail())
  await flow.save(editForm(detail())); await flow.submit()
  assert.deepEqual(calls, [['PUT', 9, 3], ['submit', 9, 4]])
})
test('PUT response loss or 409 blocks replay and never continues to submit automatically', async () => {
  for (const status of [0, 409, 500]) {
    let calls = 0
    const flow = supplementFlow({ edit: async () => { calls++; throw fail(status) }, submit: async () => { throw new Error('must not submit') } }, detail())
    await assert.rejects(flow.save(editForm(detail()))); await assert.rejects(flow.save(editForm(detail()))); await assert.rejects(flow.submit())
    assert.equal(calls, 1); assert.equal(flow.id, 9)
  }
})
test('saved supplement survives submit failure; retry only submits same version, no duplicate PUT', async () => {
  let edits = 0, submits = 0
  const flow = supplementFlow({ edit: async () => { edits++; return { id: 9, status: 'PENDING_SUPPLEMENT', version: 4 } }, submit: async (id, version) => { assert.equal(version, 4); if (++submits === 1) throw fail(503); return { id, status: 'PENDING_REVIEW', version: 5 } }, detail: async () => ({ ...detail(), version: 4 }) }, detail())
  await flow.save(editForm(detail())); await assert.rejects(flow.submit()); assert.equal(flow.saved, true)
  await flow.submit(); assert.equal(edits, 1); assert.equal(submits, 2)
})
test('lost successful submit is recovered by GET; changed versions require explicit refresh', async () => {
  const flow = supplementFlow({ submit: async () => { throw fail(503) }, detail: async () => ({ ...detail(), status: 'PENDING_REVIEW', version: 5, order_no: 'WX203001010001', submitted_at: '2030-01-01T00:00:00Z' }) }, detail())
  assert.equal((await flow.submit()).version, 5)
  const conflict = supplementFlow({ submit: async () => { throw fail(409) }, detail: async () => ({ ...detail(), version: 8 }) }, detail())
  await assert.rejects(conflict.submit()); assert.equal(conflict.blocked, true)
})
test('duplicate clicks are ignored; 401/422 never proceed to submit and require no new creation', async () => {
  let release, calls = 0
  const flow = supplementFlow({ edit: () => { calls++; return new Promise((r) => { release = r }) } }, detail())
  const first = flow.save(editForm(detail())); assert.equal(await flow.save(editForm(detail())), null)
  release({ id: 9, status: 'PENDING_SUPPLEMENT', version: 4 }); await first; assert.equal(calls, 1)
  for (const status of [401, 422]) {
    const invalid = supplementFlow({ edit: async () => { throw fail(status) } }, detail())
    await assert.rejects(invalid.save(editForm(detail()))); assert.equal(invalid.saved, false)
  }
})
