import { test } from 'node:test'
import assert from 'node:assert/strict'
import { createReportDraftHandoff, draftSnapshot, prefillText } from '../src/utils/reportDraftHandoff.js'
import { reportAssistantFlow } from '../src/utils/reportAssistant.js'

const draft = () => ({ title: 'Edited fault', description: 'Edited actual symptoms', category_hint: 'Plumbing text', location_hint: 'Dormitory text', missing_information: ['Exact location'], safety_note: 'Keep away' })
function fixture() {
  let current = { authenticated: true, id: 3, role: 'reporter', revision: 1 }, time = 10, scheduled
  const handoff = createReportDraftHandoff(() => current, { now: () => time, schedule: (f) => { scheduled = f }, cancel: () => { scheduled = undefined } })
  return { handoff, set: (value) => { current = { ...current, ...value } }, advance: () => { time += 300000 }, expire: () => scheduled?.() }
}
test('independent snapshot carries exactly six fields, once only', () => {
  const { handoff } = fixture(), source = draft(); handoff.offer(source)
  source.title = 'Later mutation'; source.missing_information[0] = 'Later mutation'
  assert.deepEqual(handoff.consume(), draft()); assert.equal(handoff.consume(), null)
})
test('no suggestion and a fresh application instance cannot restore any previous handoff', () => {
  const first = fixture(), second = fixture(); first.handoff.offer(draft()); assert.equal(second.handoff.consume(), null)
})
test('same account but different credential revision cannot consume old session data', () => {
  const f = fixture(); f.handoff.offer(draft()); f.set({ revision: 2 }); assert.equal(f.handoff.consume(), null)
  f.set({ revision: 1 }); assert.equal(f.handoff.consume(), null)
})
test('different account, role, logout or management role fail closed', () => {
  for (const change of [{ id: 9 }, { role: 'worker' }, { authenticated: false }, { role: 'dispatcher' }, { role: 'sysadmin' }]) {
    const f = fixture(); f.handoff.offer(draft()); f.set(change); assert.equal(f.handoff.consume(), null)
  }
  for (const role of ['dispatcher', 'sysadmin']) { const f = fixture(); f.set({ role }); assert.throws(() => f.handoff.offer(draft())) }
})
test('worker own reporting session is eligible, without assigning database IDs', () => {
  const f = fixture(); f.set({ role: 'worker' }); f.handoff.offer(draft()); assert.deepEqual(f.handoff.consume(), draft())
})
test('deadline and active expiry both remove unconsumed suggestions', () => {
  const f = fixture(); f.handoff.offer(draft()); f.advance(); assert.equal(f.handoff.consume(), null)
  f.handoff.offer(draft()); f.expire(); assert.equal(f.handoff.consume(), null)
})
test('failed navigation discards only its own offer and clear erases pending content', () => {
  const f = fixture(), first = f.handoff.offer(draft()), second = f.handoff.offer(draft())
  f.handoff.discard(first); assert.deepEqual(f.handoff.consume(), draft())
  f.handoff.offer(draft()); f.handoff.discard(second); f.handoff.clear(); assert.equal(f.handoff.consume(), null)
})
test('unknown fields including credentials, IDs and malformed data are rejected', () => {
  for (const key of ['token', 'api_key', 'category_id', 'location_id', 'reporter_id', '__extra']) assert.throws(() => draftSnapshot({ ...draft(), [key]: 'forged' }))
  for (const value of [null, [], {}, { ...draft(), title: '\ud800' }, { ...draft(), description: 'x'.repeat(2001) }, { ...draft(), missing_information: [7] }]) assert.throws(() => draftSnapshot(value))
})
test('unknown markers remain blank, no invented fallback; ordinary text remains unchanged', () => {
  for (const text of ['', ' ', '未知', '（待补充）', 'null', '(NULL)', '未提供']) assert.equal(prefillText(text), '')
  assert.equal(prefillText('实际故障尚待补充细节'), '实际故障尚待补充细节')
  assert.equal(prefillText(' <script>test</script> '), ' <script>test</script> ')
})
test('use eligibility requires a valid current result without requests or unresolved errors', () => {
  const f = reportAssistantFlow(); assert.equal(f.canUseDraft, false); f.draft = draft(); assert.equal(f.canUseDraft, true)
  for (const [key, value] of [['busy', true], ['draftBusy', true], ['draftStale', true], ['failed', true], ['error', 'error'], ['draftError', 'error']]) {
    const old = f[key]; f[key] = value; assert.equal(f.canUseDraft, false); f[key] = old
  }
  f.draft.title = 'x'.repeat(121); assert.equal(f.canUseDraft, false)
})
