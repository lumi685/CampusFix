// Real production routes, isolated SQLite, independent snapshots, synchronized
// multi-thread HTTP races. No production/user database or upstream network.
import assert from 'node:assert/strict'
import net from 'node:net'
import { spawn, spawnSync } from 'node:child_process'
import { mkdtempSync, readFileSync, writeFileSync, rmSync, existsSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, resolve, sep } from 'node:path'

const [server, fixture, migrations, seed, evidence] = process.argv.slice(2)
const dir = mkdtempSync(join(tmpdir(), 'campusfix-dispatch-http-'))
const db = join(dir, 'test.sqlite3'), credentials = join(dir, 'credentials.json'), trace = join(dir, 'begins.txt')
const children = [], logs = [], results = [], races = [], samples = {}, ports = []
let accounts, base, snapshots = 0, evidenceData
const pause = ms => new Promise(r => setTimeout(r, ms))
function command(...args) {
  const p = spawnSync(fixture, args.map(String), { encoding: 'utf8', windowsHide: true })
  assert.equal(p.status, 0, 'isolated fixture command must succeed')
  return p.stdout.trim()
}
const mode = name => command('--mode', db, name)
const snapshot = () => { snapshots++; return command('--snapshot', db) }
const inspect = id => JSON.parse(command('--inspect', db, id))
const freshOrder = () => Number(command('--new', db, credentials))
const dto = (worker = 1, version = 3, extra = {}) => ({ worker_id: worker, expected_version: version, ...extra })
async function freePort() {
  const s = net.createServer(); await new Promise(r => s.listen(0, '127.0.0.1', r))
  const p = s.address().port; await new Promise(r => s.close(r)); return p
}
function child(exe, args, env = process.env) {
  const p = spawn(exe, args, { env, windowsHide: true, stdio: ['ignore', 'pipe', 'pipe'] })
  children.push(p); p.stdout.on('data', c => logs.push(c.toString())); p.stderr.on('data', c => logs.push(c.toString()))
  return p
}
async function waitFor(test, label, timeout = 5000) {
  const limit = Date.now() + timeout
  while (Date.now() < limit) { if (await test()) return; await pause(10) }
  throw new Error(label)
}
async function stop(p) {
  if (p.exitCode === null && p.signalCode === null) { const exit = new Promise(r => p.once('exit', r)); p.kill(); await exit }
}
async function start(isolated = false) {
  const port = await freePort()
  ports.push(port)
  const p = isolated ? child(fixture, ['--serve', db, String(port), trace]) : child(server, [], { ...process.env, CAMPUSFIX_DB_PATH: db, CAMPUSFIX_PORT: String(port) })
  base = `http://127.0.0.1:${port}`
  await waitFor(async () => { assert.equal(p.exitCode, null, 'Drogon must stay running'); try { return (await fetch(`${base}/api/health`)).status === 200 } catch { return false } }, 'server startup')
  return p
}
async function request(label, status, id, { post = false, body = dto(), raw, token = accounts.demo_dispatcher, code, unchanged = true, query = '', route } = {}) {
  const before = unchanged ? snapshot() : null
  const response = await fetch(`${base}${route || `/api/work-orders/${id}/${post ? 'assign' : 'dispatch-candidates'}`}${query}`, {
    method: post ? 'POST' : 'GET', headers: { ...(token ? { Authorization: `Bearer ${token}` } : {}), ...(post ? { 'Content-Type': 'application/json' } : {}) },
    ...(post ? { body: raw === undefined ? JSON.stringify(body) : raw } : {})
  })
  const text = await response.text(), j = JSON.parse(text)
  assert.equal(response.status, status, `${label}: ${j.code}`)
  assert.match(response.headers.get('content-type'), /application\/json/)
  assert.equal(response.headers.get('cache-control'), 'no-store')
  assert.deepEqual(Object.keys(j).sort(), ['code', 'data', 'message', 'request_id'])
  assert.equal(typeof j.request_id, 'string'); assert.ok(j.request_id.length > 8)
  if (code !== undefined) assert.equal(j.code, code, label)
  if (status === 200) assert.equal(j.code, 0)
  for (const secret of [...Object.values(accounts).filter(x => typeof x === 'string'), 'PRIVATE_SQL_FAILURE_MARKER', 'REVIEW_PRIVATE_MARKER', 'password_hash', 'token_hash', 'metadata_json', 'stored_path', 'contact_phone', 'internal_note']) assert.equal(text.includes(secret), false, `${label}: no secret/raw SQL in response`)
  if (unchanged) assert.equal(snapshot(), before, `${label}: every column in all 16 business tables unchanged`)
  results.push({ label, method: post ? 'POST' : 'GET', status, code: j.code, request_id: j.request_id })
  return j
}
function checkCandidates(j) {
  const d = j.data
  assert.deepEqual(Object.keys(d).sort(), ['excluded_summary', 'order_id', 'order_version', 'other_eligible_candidates', 'strategy', 'top_candidates', 'weights'])
  assert.equal(d.strategy, 'WEIGHTED_SCORE_V1'); assert.deepEqual(d.weights, { skill: .35, area: .2, load: .2, efficiency: .1, rating: .1, duty: .05 })
  const rows = [...d.top_candidates, ...d.other_eligible_candidates]
  assert.ok(d.top_candidates.length <= 3)
  rows.forEach((c, i) => {
    assert.equal(c.rank, i + 1)
    assert.deepEqual(Object.keys(c).sort(), ['active_tasks', 'max_concurrent_tasks', 'rank', 'reason', 'risks', 'sample_counts', 'score_detail', 'total_score', 'worker_id', 'worker_name'])
    assert.deepEqual(Object.keys(c.score_detail).sort(), ['area', 'duty', 'efficiency', 'load', 'rating', 'skill'])
    const sum = Object.entries(d.weights).reduce((n, [k, w]) => n + c.score_detail[k] * w, 0)
    assert.equal(c.total_score, Math.round(sum * 10) / 10)
    assert.ok(c.reason.length > 0); assert.ok(Array.isArray(c.risks))
  })
  return rows
}
function checkAssignment(id, response, expectedWorker = 1) {
  assert.deepEqual(Object.keys(response.data).sort(), ['assignment_id', 'order_id', 'status', 'version', 'worker'])
  assert.deepEqual(Object.keys(response.data.worker).sort(), ['display_name', 'id'])
  const i = inspect(id), a = i.assignments[0], r = i.dispatch_records[0], l = i.status_logs.at(-1)
  assert.equal(i.status, 'PENDING_ACCEPT'); assert.equal(i.version, 4)
  assert.equal(i.assignments.length, 1); assert.equal(i.dispatch_records.length, 1)
  assert.equal(Number(a.id), response.data.assignment_id); assert.equal(Number(a.worker_id), expectedWorker)
  assert.equal(Number(a.order_id), id); assert.equal(a.ended_at, null)
  assert.equal(r.assignment_id, a.id); assert.equal(r.selected_worker_id, a.worker_id); assert.equal(r.order_version, '3')
  assert.equal(r.requested_by, a.assigned_by); assert.equal(l.actor_id, a.assigned_by)
  assert.equal(l.assignment_id, a.id); assert.equal(l.action, 'ORDER_ASSIGNED'); assert.equal(l.request_id, response.request_id)
  assert.equal(l.from_status, 'PENDING_DISPATCH'); assert.equal(l.to_status, 'PENDING_ACCEPT')
  assert.equal(l.public_note, null); assert.equal(l.internal_note, null)
  const weights = JSON.parse(r.weights_json), candidates = JSON.parse(r.candidates_json), metadata = JSON.parse(l.metadata_json)
  assert.equal(weights.skill, .35); assert.ok(candidates.length >= 3)
  assert.equal(metadata.assignment_id, Number(a.id)); assert.equal(metadata.worker_id, expectedWorker)
  assert.equal(metadata.candidate_rank, candidates.find(c => c.worker_id === expectedWorker).rank)
  assert.equal(i.worker_loads.find(w => w.worker_id === expectedWorker).active_tasks, 1)
  return { order_id: id, assignment_id: Number(a.id), snapshot_assignment_id: Number(r.assignment_id), audit_assignment_id: Number(l.assignment_id), candidate_count: candidates.length, status: i.status, version: i.version }
}
async function holder(seconds = 15) {
  const ready = join(dir, 'lock-ready'), release = join(dir, 'lock-release')
  rmSync(ready, { force: true }); rmSync(release, { force: true })
  const p = child(fixture, ['--hold', db, ready, release, String(seconds)])
  await waitFor(() => existsSync(ready), 'independent writer acquired lock')
  return async () => { writeFileSync(release, 'release'); await new Promise(r => p.exitCode === null ? p.once('exit', r) : r()); assert.equal(p.exitCode, 0) }
}
try {
  command('--init', db, credentials, migrations, seed); accounts = JSON.parse(readFileSync(credentials, 'utf8'))
  const production = await start()
  let id = freshOrder()
  for (const [name, token] of [['anonymous', ''], ['invalid', 'not-a-session'], ['expired', accounts.expired], ['revoked', accounts.revoked]]) {
    await request(`${name} preview`, 401, id, { token }); await request(`${name} assign`, 401, id, { post: true, token })
  }
  mode('disable'); await request('disabled old token', 401, id); mode('enable')
  for (const [name, status] of [['demo_reporter01', 403], ['demo_worker01', 404], ['demo_sysadmin', 404]]) {
    await request(`${name} preview denied`, status, id, { token: accounts[name] })
    await request(`${name} assign denied`, status, id, { token: accounts[name], post: true })
  }
  await request('outside dispatcher area preview', 404, accounts.foreign)
  await request('outside dispatcher area assign', 404, accounts.foreign, { post: true })
  for (const m of ['admin-read', 'admin-area', 'admin-wrong']) {
    mode(m); await request(`${m} insufficient capability/scope`, 403, id, { token: accounts.demo_sysadmin })
    await request(`${m} cannot assign`, 403, id, { token: accounts.demo_sysadmin, post: true })
  }
  mode('admin-dispatch'); checkCandidates(await request('explicit DISPATCH building scope', 200, id, { token: accounts.demo_sysadmin }))
  mode('admin-revoke'); await request('revoked DISPATCH immediately lost', 403, id, { token: accounts.demo_sysadmin })
  const preview = await request('production true candidates', 200, id), rows = checkCandidates(preview)
  assert.equal(rows.length, 6); assert.equal(preview.data.top_candidates.length, 3); assert.equal(preview.data.other_eligible_candidates.length, 3)
  assert.equal(rows[0].worker_id, 1); assert.equal(rows[0].total_score, 81)
  assert.deepEqual(rows[0].sample_counts, { efficiency: 0, rating: 0 }); samples.preview = preview
  mode('none'); const empty = await request('zero eligible with independent multiple reasons', 200, id)
  assert.deepEqual(empty.data.top_candidates, []); assert.deepEqual(empty.data.other_eligible_candidates, [])
  assert.equal(empty.data.excluded_summary.UNAVAILABLE, 6); assert.equal(empty.data.excluded_summary.WORKER_INACTIVE, 6); mode('reset')
  await request('missing order', 404, 999999999); await request('illegal path id', 422, 'abc')
  await request('query override rejected', 422, id, { query: '?rank=1' })
  for (const body of [dto(0), dto(-1), dto(1, 0), dto(1.1), dto(true), dto('1'), dto(1, true), dto(1, 3.5), { worker_id: 1 }, dto(1, 3, { reason: null }), dto(1, 3, { reason: '😀'.repeat(501) }), dto(1, 3, { rank: 1 }), dto(1, 3, { total_score: 100 }), dto(1, 3, { active_tasks: 0 }), dto(1, 3, { status: 'IN_PROGRESS' }), dto(1, 3, { assigned_by: 1 }), dto(1, 3, { strategy: 'override' })]) await request('strict assignment input', 422, id, { post: true, body })
  for (const raw of ['{broken', '[]', '{"worker_id":1,"worker_id":2,"expected_version":3}', '{"worker_id":1,"expected_version":3} trailing', '{"worker_id":1,"expected_version":9223372036854775808}', '{"worker_id":9223372036854775808,"expected_version":3}']) await request('malformed/overflow JSON', raw.includes('9223') ? 422 : 400, id, { post: true, raw })
  await request('bad UTF8', 422, id, { post: true, raw: Buffer.from([0xff]) })
  await request('stale version', 409, id, { post: true, body: dto(1, 2), code: 'ORDER_VERSION_CONFLICT' })
  const fourth = rows[3].worker_id
  for (const reason of [undefined, '', ' \t\u3000', '\u00a0\u2009']) await request('current rank four reason required', 422, id, { post: true, body: dto(fourth, 3, reason === undefined ? {} : { reason }) })
  for (const m of ['off-duty', 'uncertified', 'skill-revoked']) { mode(m); await request(`fresh eligibility ${m}`, 422, id, { post: true, code: 'WORKER_INELIGIBLE' }); mode('reset') }
  mode('lower-rank'); await request('preview rank becomes stale', 422, id, { post: true })
  const moved = await request('recomputed current low rank with reason', 200, id, { post: true, body: dto(1, 3, { reason: 'Fictional current lower rank decision' }), unchanged: false })
  samples.recomputed = checkAssignment(id, moved)
  assert.ok(JSON.parse(inspect(id).dispatch_records[0].candidates_json).find(c => c.worker_id === 1).rank > 3)
  mode('restore-rank'); mode('reset'); id = freshOrder()
  const selectedFourth = await request('current rank four Unicode limit accepted', 200, id, { post: true, body: dto(fourth, 3, { reason: '修'.repeat(500) }), unchanged: false })
  samples.fourth = checkAssignment(id, selectedFourth, fourth)
  mode('reset'); id = freshOrder()
  const assigned = await request('production Top1 assignment', 200, id, { post: true, unchanged: false })
  samples.assignment = assigned; samples.links = checkAssignment(id, assigned)
  await request('duplicate assignment', 409, id, { post: true, code: 'ACTIVE_ASSIGNMENT_CONFLICT' })
  await request('assigned order cannot preview', 409, id, { code: 'ORDER_STATUS_CONFLICT' })
  const next = freshOrder(); const later = await request('other order sees immediate real load', 200, next)
  assert.equal(checkCandidates(later).find(c => c.worker_id === 1).active_tasks, 1)
  mode('capacity-one'); await request('fresh last capacity exhausted', 422, next, { post: true, code: 'WORKER_INELIGIBLE' }); mode('reset')
  for (const m of ['fail-assignment', 'fail-snapshot', 'fail-update', 'fail-log']) {
    const target = freshOrder(); mode(m)
    await request(`${m} returns safe 500 and all-table rollback`, 500, target, { post: true, code: 'INTERNAL_ERROR' }); mode('clear-failure')
    const i = inspect(target); assert.equal(i.status, 'PENDING_DISPATCH'); assert.equal(i.version, 3); assert.equal(i.assignments.length, 0); assert.equal(i.dispatch_records.length, 0)
  }
  const historyBefore = snapshot(); mode('history-failure')
  await request('history read failure not cold start', 500, next, { unchanged: false })
  await request('history read failure assignment rollback', 500, next, { post: true, unchanged: false })
  mode('history-restore'); assert.equal(snapshot(), historyBefore)
  mode('big'); const big = await request('64bit production ids', 200, 5000000000)
  assert.equal(big.data.order_id, 5000000000); assert.ok(checkCandidates(big).some(c => c.worker_id === 5000000000))
  const bigAssign = await request('64bit selected worker and order', 200, 5000000000, { post: true, body: dto(5000000000, 3, { reason: 'Fictional large ID selection' }), unchanged: false })
  checkAssignment(5000000000, bigAssign, 5000000000)
  await stop(production)

  // Isolated four-thread Drogon process, unchanged production handlers. A third
  // connection holds the write lock until BOTH HTTP writers trace BEGIN IMMEDIATE.
  const threaded = await start(true)
  for (const race of ['same order different workers', 'same order same worker', 'two orders last capacity']) {
    mode('reset'); const first = freshOrder(), second = race === 'two orders last capacity' ? freshOrder() : first
    if (race === 'two orders last capacity') mode('capacity-one')
    writeFileSync(trace, ''); const release = await holder()
    // Do not assert a preselected winner; collect either success/business conflict.
    const raceSend = async (id, worker) => {
      const r = await fetch(`${base}/api/work-orders/${id}/assign`, { method: 'POST', headers: { Authorization: `Bearer ${accounts.demo_dispatcher}`, 'Content-Type': 'application/json' }, body: JSON.stringify(dto(worker)) })
      const j = await r.json(); assert.ok([200, 409, 422].includes(r.status)); assert.ok(j.request_id)
      results.push({ label: race, method: 'POST', status: r.status, code: j.code, request_id: j.request_id }); return { status: r.status, data: j }
    }
    const one = raceSend(first, 1), two = raceSend(second, race === 'same order different workers' ? 2 : 1)
    let attempts = 0
    try { await waitFor(() => { attempts = existsSync(trace) ? readFileSync(trace, 'utf8').trim().split('\n').filter(Boolean).length : 0; return attempts >= 2 }, 'both HTTP handlers must attempt SQLite write before lock release') } finally { await release() }
    const responses = await Promise.all([one, two]); assert.equal(responses.filter(x => x.status === 200).length, 1)
    const affected = [...new Set([first, second])].map(inspect)
    assert.equal(affected.reduce((n, i) => n + i.assignments.length, 0), 1)
    assert.equal(affected.reduce((n, i) => n + i.dispatch_records.length, 0), 1)
    assert.equal(affected.reduce((n, i) => n + i.status_logs.filter(l => l.action === 'ORDER_ASSIGNED').length, 0), 1)
    assert.equal(affected.filter(i => i.status === 'PENDING_ACCEPT' && i.version === 4).length, 1)
    if (second !== first) assert.equal(affected.filter(i => i.status === 'PENDING_DISPATCH' && i.version === 3).length, 1)
    races.push({ race, sqlite_begin_attempts_before_release: attempts, statuses: responses.map(x => x.status), wins: 1, fingerprint: snapshot() })
  }
  mode('reset'); const busyOrder = freshOrder(), releaseBusy = await holder()
  try { await request('real SQLite busy is retryable 503', 503, busyOrder, { post: true, code: 'DATABASE_BUSY' }) } finally { await releaseBusy() }
  await request('busy failure can retry successfully', 200, busyOrder, { post: true, unchanged: false })
  assert.equal(command('--integrity', db), 'integrity_check=ok; foreign_key_check empty')
  await stop(threaded)
  const allLogs = logs.join('')
  for (const secret of [...Object.values(accounts).filter(x => typeof x === 'string'), 'PRIVATE_SQL_FAILURE_MARKER', 'Authorization: Bearer']) assert.equal(allLogs.includes(secret), false, 'server logs do not leak credentials or injected SQL errors')
  evidenceData = { requests: results.length, all_table_snapshots: snapshots, results, samples, races, integrity_check: 'ok', foreign_key_check: 'empty', production_and_isolated_four_thread_routes: true, logs_sensitive_scan: 'passed' }
} finally {
  for (const p of children) await stop(p)
  const target = resolve(dir), parent = resolve(tmpdir()) + sep
  assert.ok(target.startsWith(parent) && target.includes('campusfix-dispatch-http-'))
  rmSync(target, { recursive: true, force: true })
}
for (const port of ports) {
  const released = await new Promise(r => { const s = net.createConnection({ host: '127.0.0.1', port }); s.once('error', () => r(true)); s.once('connect', () => { s.destroy(); r(false) }) })
  assert.equal(released, true, 'owned test listener must be released')
}
evidenceData.ports_released = true
evidenceData.temporary_database_removed = !existsSync(dir)
writeFileSync(evidence, JSON.stringify(evidenceData, null, 2) + '\n')
console.log(`PASS dispatch real HTTP requests=${results.length} all16table_snapshots=${snapshots} synchronized_http_races=${races.length}; integrity_check=ok; foreign_key_check empty; ports and temporary database cleaned`)
for (const race of races) console.log(JSON.stringify(race))
