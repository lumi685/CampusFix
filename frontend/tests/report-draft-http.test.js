// Real SFC lifecycle/memory Router + production Drogon + independent SQLite fixture.
// The rendering host/confirmation UI and model upstream are test doubles, NOT a browser or DeepSeek.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { spawn, spawnSync } from 'node:child_process'
import { mkdtempSync, readFileSync, writeFileSync, rmSync } from 'node:fs'
import { resolve, join, sep } from 'node:path'
import { tmpdir } from 'node:os'
import { randomBytes } from 'node:crypto'
import http from 'node:http'
import net from 'node:net'
import axios from 'axios'
import { nextTick } from 'vue'
import { apiClient } from '../src/api/client.js'
import { signIn, signOut } from '../src/stores/auth.js'
import { reportDraftHandoff } from '../src/utils/reportDraftHandoff.js'
import { categories, locations, createOrder, orderDetail, orderTimeline } from '../src/api/orders.js'
import { pages, mountPages } from './report-draft-harness.js'

const root = resolve('..'), build = join(root, 'backend', 'build'), server = join(build, 'Debug', 'campusfix_backend.exe'), fixture = join(build, 'Debug', 'campusfix_report_acceptance_fixture.exe')
const delay = (ms) => new Promise((r) => setTimeout(r, ms))
async function freePort() { const s = net.createServer(); await new Promise((r) => s.listen(0, '127.0.0.1', r)); const port = s.address().port; await new Promise((r) => s.close(r)); return port }
test('AI-D3 confirmed handoff → real catalog → explicit creation/submission; failure/permission/database safety', { timeout: 60000 }, async () => {
  const temporary = mkdtempSync(join(tmpdir(), 'campusfix-ai-d3-')), db = join(temporary, 'test.sqlite3'), credentialFile = join(temporary, 'credentials.json')
  const oldAdapter = apiClient.defaults.adapter, oldWindow = globalThis.window, running = [], logs = [], evidence = [], mounts = [], orders = [], digests = [], tokens = []
  let process, calls = 0, snapshots = 0, modelCalls = 0, failCreationReply = false, conflict = false, confirmFinal = true
  const key = randomBytes(32).toString('hex')
  const upstream = http.createServer(async (req, res) => {
    const chunks = []; for await (const chunk of req) chunks.push(chunk)
    const body = JSON.parse(Buffer.concat(chunks)); modelCalls++
    assert.equal(req.headers.authorization, `Bearer ${key}`)
    const draft = { title: 'Fictional drain leak', description: 'Known fictional leak symptom', category_hint: 'Plumbing clue only', location_hint: 'Dormitory clue only', missing_information: ['Exact location'], safety_note: 'Avoid hazard' }
    res.setHeader('Content-Type', 'application/json'); res.end(JSON.stringify({ choices: [{ finish_reason: 'stop', message: { role: 'assistant', content: body.response_format ? JSON.stringify(draft) : 'Fictional clarification' } }] }))
  })
  function command(...args) { const p = spawnSync(fixture, args, { encoding: 'utf8', windowsHide: true }); assert.equal(p.status, 0, 'Isolated fixture command failed'); return p.stdout }
  function snapshot() { const file = join(temporary, `snapshot-${++snapshots}.json`); command('--snapshot', db, file); const s = JSON.parse(readFileSync(file)); assert.equal(s.integrity, 'ok'); assert.equal(s.foreign_key_violations, 0); digests.push({ sequence: snapshots, digest: s.digest, orders: s.orders.length }); return s }
  function count() { return snapshot().orders.length }
  function writes() { return evidence.filter((r) => r.method === 'post' && r.path === '/work-orders').length }
  let accounts
  try {
    command('--init', db, credentialFile, join(root, 'backend/migrations'), join(root, 'backend/seeds/001_demo_data.sql'))
    accounts = JSON.parse(readFileSync(credentialFile, 'utf8'))
    globalThis.window = { addEventListener() {}, removeEventListener() {} }
    await new Promise((r) => upstream.listen(0, '127.0.0.1', r))
    const port = await freePort(), base = `http://127.0.0.1:${port}`
    process = spawn(server, [], { windowsHide: true, stdio: ['ignore', 'pipe', 'pipe'], env: { ...globalThis.process.env,
      CAMPUSFIX_DB_PATH: db, CAMPUSFIX_PORT: String(port), CAMPUSFIX_LLM_BASE_URL: `http://127.0.0.1:${upstream.address().port}`,
      CAMPUSFIX_LLM_API_KEY: key, CAMPUSFIX_LLM_MODEL: 'fixture-model', CAMPUSFIX_LLM_TIMEOUT_MS: '1000' } })
    process.stdout.on('data', (c) => logs.push(c.toString())); process.stderr.on('data', (c) => logs.push(c.toString())); running.push(process)
    let ready = false
    for (let n = 0; n < 100; n++) { try { if ((await fetch(`${base}/api/health`)).status === 200) { ready = true; break } } catch {} await delay(50) }
    assert.ok(ready, 'Production backend must start')
    const transport = axios.getAdapter('http')
    apiClient.defaults.adapter = async (config) => {
      let sent = config
      if (conflict && config.url.endsWith('/submit')) { conflict = false; sent = { ...config, data: JSON.stringify({ expected_version: 999 }) } }
      try {
        const result = await transport({ ...sent, baseURL: `${base}/api` }); calls++
        const body = typeof result.data === 'string' ? JSON.parse(result.data) : result.data
        if (config.url === '/auth/login') tokens.push(body.data.token)
        evidence.push({ method: config.method, path: config.url, status: result.status, code: body.code, request_id: body.request_id })
        if (failCreationReply && config.method === 'post' && config.url === '/work-orders') { failCreationReply = false; throw new Error('Controlled lost creation response after commit') }
        return result
      } catch (error) {
        if (error.response) { calls++; const body = typeof error.response.data === 'string' ? JSON.parse(error.response.data) : error.response.data; evidence.push({ method: config.method, path: config.url, status: error.response.status, code: body.code, request_id: body.request_id }) }
        throw error
      }
    }
    for (const user of ['demo_reporter01', 'demo_worker01']) {
      await signOut(); await signIn(user, accounts.accounts[user].password)
      const components = pages(async (_, title) => { if (title === '确认创建并提交报修' && !confirmFinal) throw 'cancel' }), f = await mountPages(components); mounts.push(f)
      const source = f.state(components.assistant); source.flow.consent = true; source.flow.input = 'Fictional drain leak'; const beforeAi = snapshot()
      await source.flow.send(); await source.flow.generateDraft(); assert.equal(source.flow.draft?.title, 'Fictional drain leak')
      assert.equal(snapshot().digest, beforeAi.digest, 'Chat and suggestion must not write SQLite')
      source.flow.draft.title = `User edited ${user}`; const beforeHandoff = snapshot(), beforeWrites = writes()
      await source.useDraft(); await nextTick(); const target = f.state(components.form)
      assert.equal(target.form.title, `User edited ${user}`); assert.equal(target.form.category_id, undefined); assert.equal(target.form.location_id, undefined)
      assert.equal(snapshot().digest, beforeHandoff.digest); assert.equal(writes(), beforeWrites, 'Receiving suggestion must not create order')
      const cat = (await categories()).items[0], campus = (await locations()).items[0]
      const building = (await locations(campus.id)).items[0]
      Object.assign(target.form, { category_id: cat.id, location_id: building.id, contact_name: 'Fictional person', contact_phone: '00000000000', title: `Final edited ${user}` })
      confirmFinal = false; const beforeCancel = snapshot(); await target.send(); assert.equal(snapshot().digest, beforeCancel.digest); assert.equal(writes(), beforeWrites)
      confirmFinal = true; const beforeCount = count(); await target.send(); assert.equal(count(), beforeCount + 1)
      const id = Number(f.router.currentRoute.value.params.id); assert.ok(id > 0)
      const detail = await orderDetail(id), timeline = await orderTimeline(id)
      assert.equal(detail.title, `Final edited ${user}`); assert.equal(detail.description, 'Known fictional leak symptom')
      assert.equal(detail.category.id, cat.id); assert.equal(detail.location.id, building.id); assert.equal(detail.status, 'PENDING_REVIEW'); assert.equal(detail.version, 2); assert.match(detail.order_no, /^WX\d{12}$/)
      assert.deepEqual(timeline.items.map((e) => [e.action, e.from_status, e.to_status]), [['ORDER_CREATED', null, 'DRAFT'], ['ORDER_SUBMITTED', 'DRAFT', 'PENDING_REVIEW']])
      orders.push({ role: user === 'demo_reporter01' ? 'reporter' : 'worker', id, order_no: detail.order_no, status: detail.status, version: detail.version,
        title: detail.title, description: detail.description, category_id: detail.category.id, location_id: detail.location.id, actions: timeline.items.map((e) => e.action) })
      f.app.unmount(); mounts.pop()
    }
    await signOut(); await signIn('demo_reporter01', accounts.accounts.demo_reporter01.password)
    const category = (await categories()).items[0], location = (await locations()).items[0]
    const body = { category_id: category.id, location_id: location.id, title: 'Fictional final fault', description: 'Fictional known symptom', requested_urgency: 'NORMAL', contact_name: 'Fictional', contact_phone: '00000000000' }
    const suggestion = { title: body.title, description: body.description, category_hint: 'Untrusted', location_hint: 'Untrusted', missing_information: [], safety_note: '' }
    for (const mode of ['submit-failure', 'conflict', 'creation-unknown']) {
      reportDraftHandoff.offer(suggestion); const components = pages(async () => {}), f = await mountPages(components, '/report/new'); mounts.push(f); const target = f.state(components.form)
      Object.assign(target.form, body); const baseline = count(), posted = writes()
      if (mode === 'submit-failure') command('--fault', db, 'submit-log')
      if (mode === 'conflict') conflict = true
      if (mode === 'creation-unknown') failCreationReply = true
      await target.send(); assert.equal(count(), baseline + 1); assert.equal(writes(), posted + 1)
      if (mode === 'creation-unknown') { assert.equal(target.flow.uncertain, true); await target.send(); assert.equal(writes(), posted + 1) }
      else {
        const draft = { ...target.flow.draft }; assert.ok(draft.id > 0); assert.equal((await orderDetail(draft.id)).status, 'DRAFT')
        if (mode === 'submit-failure') command('--fault', db, 'off')
        await target.send(); assert.equal(count(), baseline + 1); assert.equal(writes(), posted + 1); assert.equal((await orderDetail(draft.id)).status, 'PENDING_REVIEW')
      }
      f.app.unmount(); mounts.pop()
    }
    for (const changed of [{ category_id: accounts.inactive_category }, { location_id: accounts.inactive_location }, { category_id: 987654321 }, { location_id: 987654321 }]) {
      const before = snapshot(); await assert.rejects(createOrder({ ...body, ...changed }), { status: 422 }); assert.equal(snapshot().digest, before.digest)
    }
    for (const user of ['demo_dispatcher', 'demo_sysadmin']) {
      await signOut(); await signIn(user, accounts.accounts[user].password); const before = snapshot()
      await assert.rejects(createOrder(body), { status: 403 }); assert.equal(snapshot().digest, before.digest)
      assert.throws(() => reportDraftHandoff.offer(suggestion))
    }
    assert.ok(evidence.every((r) => /^req_[\w-]+$/.test(r.request_id))); assert.ok(modelCalls === 4)
    await signOut()
    const secrets = [key, ...tokens, ...Object.values(accounts.accounts).map((a) => a.password)]
    assert.ok(secrets.every((secret) => !logs.join('').includes(secret)), 'Server logs must not contain test secrets')
    const final = snapshot()
    writeFileSync(join(build, 'Testing', 'v1.2-ai-d3-evidence.json'), JSON.stringify({ kind: 'Vue custom renderer/memory router, real production HTTP and SQLite; NOT browser/DeepSeek', requests: evidence, calls, snapshots, digests, orders, modelCalls, finalOrderCount: final.orders.length, integrity: final.integrity, foreignKeyViolations: final.foreign_key_violations }, null, 2))
    console.log(`PASS AI-D3: ${calls} real HTTP requests, ${snapshots} independent SQLite snapshots, ${modelCalls} fake-upstream calls; integrity=ok foreign_key_check=0`)
  } finally {
    for (const f of mounts) f.app.unmount()
    try { await signOut() } catch {} apiClient.defaults.adapter = oldAdapter; reportDraftHandoff.clear(); globalThis.window = oldWindow
    for (const p of running) { if (p.exitCode === null) { p.kill(); await new Promise((r) => p.once('exit', r)) } }
    if (upstream.listening) { upstream.closeAllConnections(); await new Promise((r) => upstream.close(r)) }
    const actual = resolve(temporary); assert.ok(actual.startsWith(resolve(tmpdir()) + sep) && actual.includes('campusfix-ai-d3-'))
    rmSync(actual, { recursive: true, force: true })
  }
})
