import { test, after } from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import { randomBytes } from 'node:crypto'
import { AxiosError } from 'axios'
import { apiClient } from '../src/api/client.js'
import { replaceCredential } from '../src/api/credentials.js'
import { getDispatchCandidates, assignWorker } from '../src/api/dispatch.js'
import { assignmentPayload, dispatchSnapshot, dispatchId, dispatchFlow, exclusionRows, emptyDispatchText, validateAssignReason, dispatchFailure } from '../src/utils/dispatch.js'
const adapter = apiClient.defaults.adapter
after(() => { apiClient.defaults.adapter = adapter; replaceCredential() })
export const snapshot = (id = 9) => ({ order_id: id, order_version: 3, strategy: 'WEIGHTED_SCORE_V1', weights: { skill: .35, area: .2, load: .2, efficiency: .1, rating: .1, duty: .05 }, top_candidates: [1, 2, 3].map(candidate), other_eligible_candidates: [4, 5].map(candidate), excluded_summary: {} })
function candidate(rank) { return { rank, worker_id: rank, worker_name: `Fictional ${rank}`, total_score: 81 - rank, active_tasks: 0, max_concurrent_tasks: 3, score_detail: { skill: 100, area: 70, load: 100, efficiency: 60, rating: 60, duty: 0 }, sample_counts: { efficiency: 0, rating: 0 }, reason: 'Server fixture reason', risks: ['Server fixture cold-start risk'] } }
const ack = (worker = 1, id = 9) => ({ order_id: id, assignment_id: 5, worker: { id: worker, display_name: 'Fictional' }, status: 'PENDING_ACCEPT', version: 4 })
const failure = (status, code = '', field = '', reason = '') => Object.assign(new Error('fixture error'), { status, code, field, reason })
const loaded = async (api = {}, read = () => false) => { const f = dispatchFlow({ candidates: async id => snapshot(Number(id)), ...api }, read); await f.load(9); return f }

test('checked server DTO preserves all actual ranks and exact scores without sorting/calculation', () => {
  const data = snapshot(); data.top_candidates[0].total_score = 12.3 // Deliberately not recomputed by frontend.
  const dto = dispatchSnapshot(data, '9'); assert.deepEqual(dto, data)
  assert.deepEqual([...dto.top_candidates, ...dto.other_eligible_candidates].map(c => c.rank), [1,2,3,4,5])
  assert.equal(dto.top_candidates[0].total_score, 12.3); assert.deepEqual(dto.top_candidates[0].score_detail, data.top_candidates[0].score_detail)
})
test('snapshot projection drops all private raw fields and unknown weight fields', () => {
  const data = snapshot(); data.internal_note = 'PRIVATE'; data.weights.private = 'PRIVATE'; data.top_candidates[0].token_hash = 'PRIVATE'
  const dto = dispatchSnapshot(data, 9); assert.equal(JSON.stringify(dto).includes('PRIVATE'), false)
})
test('missing or malformed DTO cannot become actionable', async () => {
  for (const mutate of [d => { delete d.order_version }, d => { d.order_id = 8 }, d => { d.order_version = '3' }, d => { d.weights.skill = .5 }, d => { delete d.top_candidates[0].score_detail.skill }, d => { d.top_candidates[0].total_score = NaN }, d => { d.other_eligible_candidates[0].rank = 1 }, d => { d.top_candidates[1].worker_id = 1 }, d => { d.top_candidates[0].sample_counts.efficiency = 21 }, d => { d.excluded_summary.INVALID = -1 }, d => { d.top_candidates.pop() }]) {
    const d = snapshot(); mutate(d); assert.throws(() => dispatchSnapshot(d, 9))
    let writes = 0; const f = await loaded({ candidates: async () => d, assign: async () => { writes++ } }); await assert.rejects(f.send()); assert.equal(writes, 0); assert.equal(f.snapshot, null)
  }
})
test('safe integer limits: reject unsafe ID/version, never round/truncate/stringify to another ID', () => {
  assert.equal(dispatchId('9007199254740991'), '9007199254740991')
  for (const id of ['9007199254740992', '9007199254740993', Number.MAX_SAFE_INTEGER + 1, '1.1', 0, -1, true, '01']) assert.throws(() => dispatchId(id))
  for (const mutate of [d => { d.order_id = Number.MAX_SAFE_INTEGER + 1 }, d => { d.order_version = Number.MAX_SAFE_INTEGER + 1 }, d => { d.top_candidates[0].worker_id = Number.MAX_SAFE_INTEGER + 1 }]) { const d = snapshot(); mutate(d); assert.throws(() => dispatchSnapshot(d, 9)) }
})
test('all nine exclusion codes map safely; unknown reasons not fabricated; no summed people count', () => {
  const codes = ['ACCOUNT_INACTIVE','ACCOUNT_ROLE_MISMATCH','WORKER_INACTIVE','UNAVAILABLE','MISSING_SKILL','MISSING_CERTIFICATE','OUTSIDE_SERVICE_AREA','AT_CAPACITY','LOCATION_UNVERIFIED']
  assert.equal(new Set(exclusionRows(Object.fromEntries(codes.map(k => [k, 1]))).map(x => x.label)).size, 9)
  const rows = exclusionRows({ PRIVATE_SQL_ERROR: 2 }); assert.equal(rows[0].count, 2); assert.doesNotMatch(rows[0].label, /PRIVATE_SQL/)
  assert.match(exclusionRows(JSON.parse('{"__proto__":1,"toString":2}'))[0].label, /尚未识别/)
  assert.match(exclusionRows({ toString: 1 })[0].label, /尚未识别/)
  assert.match(emptyDispatchText({}), /该工单位置当前没有可推荐的覆盖范围内维修人员/); assert.doesNotMatch(emptyDispatchText({}), /全校/)
  assert.match(emptyDispatchText({ AT_CAPACITY: 2 }), /排除原因/)
})
test('zero candidates empty pool or excluded pool stay empty, never synthesize eligible workers', async () => {
  for (const excluded_summary of [{}, { AT_CAPACITY: 6 }]) {
    const d = { ...snapshot(), top_candidates: [], other_eligible_candidates: [], excluded_summary }
    const f = await loaded({ candidates: async () => d }); assert.equal(f.phase, 'empty'); assert.equal(f.choose(1), false); await assert.rejects(f.send())
  }
})
test('server sample counts and risks preserved; no fabricated historical insufficiency when absent', () => {
  const d = snapshot(); d.top_candidates[0].sample_counts = { efficiency: 3, rating: 20 }; d.top_candidates[0].risks = []
  assert.deepEqual(dispatchSnapshot(d, 9).top_candidates[0].risks, []); assert.equal(dispatchSnapshot(d, 9).top_candidates[0].sample_counts.rating, 20)
})
test('payload obtains current version/rank from loaded snapshot and only whitelists fields', () => {
  assert.deepEqual(assignmentPayload(snapshot(), 1), { worker_id: 1, expected_version: 3 })
  assert.deepEqual(assignmentPayload(snapshot(), 4, 'Manual reason'), { worker_id: 4, expected_version: 3, reason: 'Manual reason' })
  assert.throws(() => assignmentPayload(snapshot(), 99, 'Not in candidates'))
  assert.throws(() => assignmentPayload(snapshot(), 4))
})
test('reason Unicode scalar limits and whitespace/control validation', () => {
  validateAssignReason('😀'.repeat(500), true); assert.throws(() => validateAssignReason('😀'.repeat(501), true))
  for (const value of [' \t\n\u3000', '\u00a0\u2009\u0085', '\uFEFF', '', '\ud800', '\udc00', '\0', '\x7f']) assert.throws(() => validateAssignReason(value, true))
  validateAssignReason('', false); validateAssignReason('\tLegal\nReason', true)
})
test('choosing/cancelling never POSTs; top3 reason optional, other chosen candidates require reason', async () => {
  let writes = 0; const f = await loaded({ assign: async () => { writes++; return ack(4) } })
  assert.equal(f.choose(99), false); f.choose(4); await assert.rejects(f.send()); assert.equal(writes, 0)
  f.cancel(); assert.equal(f.selected, null); assert.equal(f.reason, '')
  f.choose(4); f.reason = 'Manual adjustment'; await f.send(); assert.equal(writes, 1); assert.equal(f.phase, 'success')
})
test('duplicate confirm clicks only generate one POST; no selection changes during submitting', async () => {
  let writes = 0, release; const f = await loaded({ assign: async () => { writes++; await new Promise(r => { release = r }); return ack() } })
  f.choose(1); const first = f.send(); assert.equal(f.phase, 'submitting'); assert.equal(f.choose(2), false)
  assert.equal(await f.send(), null); f.cancel(); assert.equal(f.selected, 1); release(); await first
  assert.equal(writes, 1); assert.equal(f.phase, 'success'); assert.equal(f.snapshot, null); assert.equal(f.selected, null)
  await assert.rejects(f.send()); assert.equal(await f.load(9), false)
})
test('success receipt validated; authorized detail refreshed and no local fake state update', async () => {
  let reads = 0; const f = await loaded({ assign: async () => ack(), detail: async () => { reads++; return { id: 9, status: 'PENDING_ACCEPT', version: 4, current_assignment: { worker_name: 'Fictional' }, contact_phone: 'PRIVATE' } } }, () => true)
  f.choose(1); await f.send(); assert.equal(f.result.assignment_id, 5); assert.equal(reads, 1); assert.equal(f.verification.status, 'PENDING_ACCEPT'); assert.equal(Object.hasOwn(f.verification, 'contact_phone'), false)
})
test('malformed or unsafe success DTO enters unknown result, not false failure/success or replay', async () => {
  for (const bad of [null, {}, { ...ack(), version: Number.MAX_SAFE_INTEGER + 1 }, { ...ack(), assignment_id: Number.MAX_SAFE_INTEGER + 1 }, { ...ack(), order_id: 10 }, { ...ack(), worker: { id: 2, display_name: 'Wrong' } }]) {
    let writes = 0; const f = await loaded({ assign: async () => { writes++; return bad } }); f.choose(1); await f.send(); assert.equal(f.phase, 'unknown_result'); assert.equal(f.result, null); await assert.rejects(f.send()); assert.equal(writes, 1)
  }
})
test('409 clears stale version/candidate/reason for all frozen conflict codes and never auto-replays', async () => {
  for (const code of ['ORDER_VERSION_CONFLICT','ORDER_STATUS_CONFLICT','ACTIVE_ASSIGNMENT_CONFLICT']) {
    let writes = 0; const f = await loaded({ assign: async () => { writes++; throw failure(409, code) } }); f.choose(1); f.reason = 'Old reason'; await f.send()
    assert.equal(f.phase, 'conflict'); assert.equal(f.snapshot, null); assert.equal(f.selected, null); assert.equal(f.reason, ''); await assert.rejects(f.send()); assert.equal(writes, 1)
    await f.load(9); assert.equal(f.phase, 'ready')
  }
})
test('422 ineligible clears unsafe snapshot; field validation retains editable reason without rewriting version', async () => {
  const f = await loaded({ assign: async () => { throw failure(422, 'WORKER_INELIGIBLE', '', 'AT_CAPACITY') } }); f.choose(1); await f.send(); assert.equal(f.snapshot, null); assert.match(f.message, /任务量已满/)
  const editable = await loaded({ assign: async () => { throw failure(422, 'VALIDATION_ERROR', 'reason') } }); editable.choose(4); editable.reason = 'Retain'; await editable.send()
  assert.equal(editable.phase, 'ready'); assert.equal(editable.selected, 4); assert.equal(editable.reason, 'Retain'); assert.equal(editable.snapshot.order_version, 3)
})
test('network, timeout and every 5xx remain unknown even if read confirms an assignment', async () => {
  for (const status of [0, 500, 502, 503, 504]) {
    let writes = 0, reads = 0; const f = await loaded({ assign: async () => { writes++; throw failure(status) }, detail: async () => { reads++; return { id: 9, status: 'PENDING_ACCEPT', version: 4 } } }, () => true)
    f.choose(1); await f.send(); assert.equal(f.phase, 'unknown_result'); assert.equal(f.result, null); assert.equal(f.verification.status, 'PENDING_ACCEPT')
    await assert.rejects(f.send()); assert.equal(await f.load(9), false); assert.equal(writes, 1); assert.equal(reads, 1)
  }
})
test('DISPATCH without BUSINESS_READ never queries details, including unknown-result reconciliation', async () => {
  let reads = 0; const f = await loaded({ assign: async () => { throw failure(0) }, detail: async () => { reads++; return {} } })
  f.choose(1); await f.send(); await f.verify(); assert.equal(reads, 0); assert.equal(f.phase, 'unknown_result')
})
test('401/403/404 remove all previous candidate data without further lookup', async () => {
  for (const status of [401,403,404]) {
    let reads = 0; const f = await loaded({ assign: async () => { throw failure(status) }, detail: async () => { reads++ } }, () => true)
    f.choose(1); await f.send(); assert.equal(f.phase, 'forbidden'); assert.equal(f.snapshot, null); assert.equal(f.selected, null); assert.equal(reads, 0)
  }
})
test('API wrapper uses common authenticated client and strips forbidden write fields', async () => {
  replaceCredential(randomBytes(32).toString('hex')); const calls = []
  apiClient.defaults.adapter = async config => { calls.push(config); return { config, status: 200, headers: {}, data: { code: 0, data: {}, request_id: 'req_unit' } } }
  await getDispatchCandidates(9); await assignWorker(9, { worker_id: 4, expected_version: 3, reason: 'Manual', rank: 1, active_tasks: 0, total_score: 100, status: 'PENDING_ACCEPT', assigned_by: 1, strategy: 'FAKE' })
  assert.deepEqual(calls.map(c => c.url), ['/work-orders/9/dispatch-candidates','/work-orders/9/assign'])
  assert.ok(calls.every(c => c.authRequired === true && c.headers.get('Authorization')))
  assert.deepEqual(JSON.parse(calls[1].data), { worker_id: 4, expected_version: 3, reason: 'Manual' })
  await assert.rejects(assignWorker('9007199254740993', { worker_id: 1, expected_version: 3 })); assert.equal(calls.length, 2)
})
test('safe error includes only known public worker-ineligible reason, never raw SQL/config', async () => {
  replaceCredential(randomBytes(32).toString('hex'))
  for (const reason of ['AT_CAPACITY', 'PRIVATE_SQL_FAILURE_MARKER']) {
    apiClient.defaults.adapter = async config => { throw new AxiosError('unsafe', 'ERR_BAD_RESPONSE', config, null, { status: 422, data: { code: 'WORKER_INELIGIBLE', data: { reason, password_hash: 'PRIVATE' }, request_id: 'req_unit' } }) }
    await assert.rejects(assignWorker(9, { worker_id: 1, expected_version: 3 }), e => {
      assert.equal(e.reason, reason === 'AT_CAPACITY' ? 'AT_CAPACITY' : undefined); assert.equal('config' in e, false); assert.equal('password_hash' in e, false); return true
    })
  }
})
test('real component wiring: route/auth meta, explicit dialog registration, responsive server-score display', () => {
  const src = name => readFileSync(new URL(`../src/${name}`, import.meta.url), 'utf8')
  assert.match(src('router/index.js'), /'admin-dispatch': AdminDispatchView/); assert.match(src('router/index.js'), /path: 'admin\/dispatch\/:id'.*requiresAuth: true, entryId: 'admin-dispatch'/)
  assert.match(src('components/DispatchAssignDialog.vue'), /import \{ ElDialog \}/); assert.match(src('components/DispatchAssignDialog.vue'), /@click="\$emit\('confirm'\)"/)
  assert.match(src('components/DispatchScoreDetail.vue'), /candidate.score_detail\[key\]/); assert.match(src('components/DispatchScoreDetail.vue'), /sample_counts/)
  assert.match(src('views/AdminDispatchView.vue'), /@choose="flow.choose\(\$event\)"/)
  assert.match(src('views/AdminDispatchView.vue'), /max-width:600px/); assert.doesNotMatch(src('views/AdminDispatchView.vue'), /v-html|localStorage|sessionStorage/)
})
