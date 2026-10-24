import { test, after, beforeEach } from 'node:test'
import assert from 'node:assert/strict'
import { nextTick } from 'vue'
import { apiClient } from '../src/api/client.js'
import { signIn, signOut, authState } from '../src/stores/auth.js'
import { credentialSnapshot, invalidateCredential } from '../src/api/credentials.js'
import { reportDraftHandoff } from '../src/utils/reportDraftHandoff.js'
import { checkedDraft } from '../src/utils/reportAssistant.js'
import { pages, mountPages, hostText } from './report-draft-harness.js'

const original = apiClient.defaults.adapter, fixtures = [], listeners = new Map(), oldWindow = globalThis.window
globalThis.window = { addEventListener: (name, fn) => { if (!listeners.has(name)) listeners.set(name, new Set()); listeners.get(name).add(fn) }, removeEventListener: (name, fn) => listeners.get(name)?.delete(fn) }
let calls, role, failSubmit, ambiguousCreate, confirmations, response, sequence, submitted
const model = () => ({ requires_review: true, draft: { title: 'Model title', description: 'Actual known symptom', category_hint: '给排水维修', location_hint: '宿舍卫生间', missing_information: ['具体位置'], safety_note: '远离危险' } })
function failure(status, code = 'TEST_FAILURE') { return Object.assign(new Error('controlled fixture error'), { status, code }) }
beforeEach(async () => {
  for (const fixture of fixtures.splice(0)) fixture.app.unmount()
  await signOut(); reportDraftHandoff.clear(); calls = []; confirmations = []; sequence = 0; submitted = false; role = 'reporter'; failSubmit = false; ambiguousCreate = false; response = async () => {}
  apiClient.defaults.adapter = async (config) => {
    const body = config.data ? JSON.parse(config.data) : undefined; calls.push({ method: config.method, path: config.url, body })
    let data
    if (config.url === '/auth/login') data = { token: `fictional-session-${++sequence}`, expires_at: '2099-01-01T00:00:00Z' }
    else if (config.url === '/auth/me') data = { id: 3, username: 'fictional_user', display_name: 'Fictional', role, capabilities: [], scope_location_ids: [] }
    else if (config.url === '/auth/logout') data = { revoked: true }
    else if (config.url === '/ai/report-assistant/draft') data = model()
    else if (config.url === '/ai/report-assistant/chat') data = { reply: 'Fictional clarification' }
    else if (config.url === '/fault-categories') data = { items: config.params?.parent_id ? [] : [{ id: 71, code: 'LIVE', name: '实际其他类别', default_priority: 'NORMAL' }] }
    else if (config.url === '/locations') data = { items: config.params?.parent_id ? [{ id: 83, code: 'BUILDING', name: '真实楼栋', parent_id: 81 }] : [{ id: 81, code: 'CAMPUS', name: '真实校区', parent_id: null }] }
    else if (config.method === 'post' && config.url === '/work-orders') { if (ambiguousCreate) throw failure(0); data = { id: 900, status: 'DRAFT', version: 1 } }
    else if (config.url === '/work-orders/900/submit') { if (failSubmit) throw failure(409); submitted = true; data = { id: 900, status: 'PENDING_REVIEW', version: 3, order_no: 'WX203001010001', submitted_at: '2030-01-01T00:00:00Z' } }
    else if (config.url === '/work-orders/900') data = { id: 900, status: submitted ? 'PENDING_REVIEW' : 'DRAFT', version: 2 }
    else throw new Error(`Unexpected test endpoint ${config.url}`)
    return { config, status: 200, data: { code: 0, message: 'success', data, request_id: 'req_component_fixture' }, headers: {} }
  }
  await signIn('fictional_user', 'not-a-real-password')
})
after(async () => { for (const f of fixtures) f.app.unmount(); await signOut(); apiClient.defaults.adapter = original; globalThis.window = oldWindow })
async function mounted(path) {
  const components = pages(async (content, title) => { confirmations.push({ content, title }); return response(content, title) })
  const fixture = await mountPages(components, path); fixtures.push(fixture); return { ...fixture, components }
}
function ready(f) { const s = f.state(f.components.assistant); s.flow.consent = true; s.flow.messages = [{ role: 'user', content: 'Actual symptom' }]; s.flow.draft = checkedDraft(model()); s.flow.draft.title = 'User edited title'; return s }
function fill(s) { Object.assign(s.form, { category_id: 71, location_id: 83, contact_name: 'Fictional person', contact_phone: '00000000000' }) }
const creates = () => calls.filter((c) => c.path === '/work-orders' && c.method === 'post')

test('actual mounted page transfers edited snapshot via real memory Router and survives source unmount', async () => {
  const f = await mounted(); const source = ready(f); await source.useDraft(); await nextTick()
  assert.equal(f.router.currentRoute.value.path, '/report/new'); const target = f.state(f.components.form)
  assert.equal(target.form.title, 'User edited title'); assert.equal(target.form.description, 'Actual known symptom')
  assert.equal(source.flow.draft, null); assert.equal(target.form.category_id, undefined); assert.equal(target.form.location_id, undefined)
  assert.equal(target.form.requested_urgency, 'NORMAL'); assert.equal(target.form.contact_name, ''); assert.equal(target.form.contact_phone, ''); assert.equal(target.form.expected_visit_at, null)
  assert.match(hostText(f.root), /给排水维修/); assert.match(hostText(f.root), /宿舍卫生间/); assert.equal(creates().length, 0)
  assert.equal(reportDraftHandoff.consume(), null); assert.deepEqual(f.router.currentRoute.value.query, {}); assert.equal(f.router.options.history.state.title, undefined)
})
test('cancel handoff, then ordinary navigation opens an empty form', async () => {
  const f = await mounted(); const source = ready(f); response = async () => { throw 'cancel' }; await source.useDraft()
  assert.equal(f.router.currentRoute.value.path, '/report/assistant'); assert.equal(reportDraftHandoff.consume(), null)
  await f.router.push('/report/new'); assert.equal(f.state(f.components.form).form.title, ''); assert.equal(creates().length, 0)
})
test('none/stale/busy/failed/error/malformed suggestion cannot invoke confirmation', async () => {
  const f = await mounted(), source = ready(f)
  for (const [key, value] of [['draft', null], ['draftStale', true], ['busy', true], ['draftBusy', true], ['failed', true], ['error', 'unresolved'], ['draftError', 'unresolved'], ['draft', { ...source.flow.draft, title: 'x'.repeat(121) }]]) {
    const old = source.flow[key]; source.flow[key] = value; await source.useDraft(); source.flow[key] = old
  }
  assert.equal(confirmations.length, 0); assert.equal(f.router.currentRoute.value.path, '/report/assistant')
})
test('facts or edits changing while handoff confirmation waits invalidate that confirmation', async () => {
  const f = await mounted(), source = ready(f); let resolve
  response = () => new Promise((r) => { resolve = r }); const pending = source.useDraft(); source.flow.draft.title = 'Changed while dialog open'; resolve(); await pending
  assert.equal(f.router.currentRoute.value.path, '/report/assistant'); assert.equal(reportDraftHandoff.consume(), null)
})
test('aborted or throwing navigation leaves no pending handoff', async () => {
  const f = await mounted(), source = ready(f); const remove = f.router.beforeEach((to) => to.path === '/report/new' ? false : true)
  await source.useDraft(); assert.equal(reportDraftHandoff.consume(), null); remove()
  const originalPush = f.router.push; f.router.push = async () => { throw new Error('navigation failed') }
  await source.useDraft(); assert.equal(reportDraftHandoff.consume(), null); f.router.push = originalPush
})
test('real CatalogPicker lazy functions use catalog APIs, hint IDs never select anything automatically', async () => {
  const f = await mounted(); await ready(f).useDraft(); await nextTick()
  const pickers = f.instances().filter((i) => i.type.__name === 'CatalogPicker'); assert.equal(pickers.length, 2)
  for (const picker of pickers) {
    const items = await new Promise((r) => picker.setupState.options.lazyLoad({ level: 0 }, r)); assert.ok(items.length)
    assert.equal(f.state(f.components.form).form[picker.props.kind === 'categories' ? 'category_id' : 'location_id'], undefined)
    picker.emit('update:modelValue', items[0].id)
  }
  assert.equal(f.state(f.components.form).form.category_id, 71); assert.equal(f.state(f.components.form).form.location_id, 81)
  assert.ok(calls.some((c) => c.path === '/fault-categories')); assert.ok(calls.some((c) => c.path === '/locations'))
})
test('catalog failure resolves no fake options and initial form cannot pass validation', async () => {
  const f = await mounted(); await ready(f).useDraft(); const target = f.state(f.components.form)
  const adapter = apiClient.defaults.adapter; apiClient.defaults.adapter = async (config) => { if (config.url === '/locations') throw failure(503); return adapter(config) }
  const picker = f.instances().find((i) => i.type.__name === 'CatalogPicker' && i.props.kind === 'locations')
  assert.deepEqual(await new Promise((r) => picker.setupState.options.lazyLoad({ level: 0 }, r)), [])
  await target.send(); assert.equal(confirmations.length, 1); assert.equal(creates().length, 0); assert.match(target.error, /联系人|分类/)
})
test('final confirmation uses current real form payload, cancel writes nothing, duplicate clicks blocked', async () => {
  const f = await mounted(); await ready(f).useDraft(); const target = f.state(f.components.form); fill(target); target.form.title = 'Final user correction'
  response = async () => { throw 'cancel' }; await target.send(); assert.equal(creates().length, 0)
  let resolve; response = () => new Promise((r) => { resolve = r }); const pending = target.send(); await target.send()
  const content = confirmations.at(-1).content.children.map((v) => v.children).join(' '); assert.match(content, /Final user correction/); assert.doesNotMatch(content, /Model title/)
  resolve(); await pending; assert.equal(creates().length, 1)
  assert.deepEqual(Object.keys(creates()[0].body).sort(), ['category_id', 'location_id', 'title', 'description', 'requested_urgency', 'contact_name', 'contact_phone'].sort())
  assert.equal(creates()[0].body.title, 'Final user correction'); assert.equal(f.router.currentRoute.value.path, '/orders/900')
})
test('409 submit failure retries same draft with fresh version without second create or create-confirmation', async () => {
  const f = await mounted(); await ready(f).useDraft(); const target = f.state(f.components.form); fill(target); failSubmit = true; await target.send()
  assert.deepEqual({ ...target.flow.draft }, { id: 900, version: 2 }); failSubmit = false; await target.send()
  assert.equal(creates().length, 1); assert.equal(confirmations.length, 2)
  assert.deepEqual(calls.filter((c) => c.path.endsWith('/submit')).map((c) => c.body.expected_version), [1, 2])
})
test('ambiguous creation blocks blind retry even after explicit confirmation', async () => {
  const f = await mounted(); await ready(f).useDraft(); const target = f.state(f.components.form); fill(target); ambiguousCreate = true
  await target.send(); assert.equal(target.flow.uncertain, true); await target.send(); assert.equal(creates().length, 1)
})
test('later manual edits survive rerender and returning to form does not prefill again', async () => {
  const f = await mounted(); await ready(f).useDraft(); const target = f.state(f.components.form); target.form.title = ''; target.form.description = 'Manual correction'; await nextTick()
  assert.equal(target.form.title, ''); assert.equal(target.form.description, 'Manual correction')
  await f.router.push('/my-orders'); await f.router.push('/report/new'); assert.equal(f.state(f.components.form).aiSuggestion, null); assert.equal(f.state(f.components.form).form.title, '')
})
test('signout invalidation immediately clears received hints and edits, relogin cannot restore them', async () => {
  const f = await mounted(); await ready(f).useDraft(); const target = f.state(f.components.form); await signOut(); await nextTick()
  assert.equal(target.aiSuggestion, null); assert.equal(target.form.title, ''); assert.equal(target.allowed, false)
  await signIn('fictional_user', 'not-a-real-password'); await f.router.push('/my-orders'); await f.router.push('/report/new'); assert.equal(f.state(f.components.form).form.title, '')
})
test('expired credentials during final confirmation block writes and clear form', async () => {
  const f = await mounted(); await ready(f).useDraft(); const target = f.state(f.components.form); fill(target); let resolve
  response = () => new Promise((r) => { resolve = r }); const pending = target.send(); invalidateCredential(credentialSnapshot().revision); resolve(); await pending
  assert.equal(creates().length, 0); assert.equal(target.aiSuggestion, null); assert.equal(target.form.title, '')
})
test('pagehide erases consumed AI text; source unmount during confirmation cannot later navigate', async () => {
  const f = await mounted(), source = ready(f); let resolve; response = () => new Promise((r) => { resolve = r }); const pending = source.useDraft()
  await f.router.push('/my-orders'); resolve(); await pending; assert.equal(f.router.currentRoute.value.path, '/my-orders'); assert.equal(reportDraftHandoff.consume(), null)
})
test('pagehide clears received suggestion and cannot restore it after returning', async () => {
  const f = await mounted(); await ready(f).useDraft(); const target = f.state(f.components.form)
  for (const fn of listeners.get('pagehide') || []) fn()
  assert.equal(target.aiSuggestion, null); assert.equal(target.form.title, ''); assert.equal(target.allowed, false)
})
test('same account new login during handoff confirmation invalidates the old operation', async () => {
  const f = await mounted(), source = ready(f); let resolve; response = () => new Promise((r) => { resolve = r }); const pending = source.useDraft()
  await signOut(); await signIn('fictional_user', 'not-a-real-password'); resolve(); await pending
  assert.equal(f.router.currentRoute.value.path, '/report/assistant'); assert.equal(reportDraftHandoff.consume(), null)
})
test('session switching during an in-flight creation cannot submit that draft with a new credential', async () => {
  const f = await mounted(); await ready(f).useDraft(); const target = f.state(f.components.form); fill(target)
  const adapter = apiClient.defaults.adapter; let release, entered
  const inFlight = new Promise((r) => { entered = r })
  apiClient.defaults.adapter = async (config) => {
    if (config.url === '/work-orders' && config.method === 'post') { entered(); await new Promise((r) => { release = r }) }
    return adapter(config)
  }
  const pending = target.send(); await inFlight; await signOut(); await signIn('fictional_user', 'not-a-real-password'); release(); await pending
  assert.equal(calls.filter((c) => c.path.endsWith('/submit')).length, 0); assert.equal(creates().length, 1)
  assert.equal(target.form.title, ''); assert.equal(target.allowed, false)
})
test('blank unknown fields stay unfilled; malicious hint text remains plain rendered text', async () => {
  const f = await mounted(), source = ready(f); source.flow.draft.title = '未知'; source.flow.draft.description = ''; source.flow.draft.location_hint = '<img src=x onerror=alert(1)>'
  await source.useDraft(); const target = f.state(f.components.form)
  assert.equal(target.form.title, ''); assert.equal(target.form.description, ''); assert.match(hostText(f.root), /<img src=x/)
  const tags = []; function walk(n) { tags.push(n.type); n.children.forEach(walk) } walk(f.root)
  assert.equal(tags.includes('img'), false); await target.send(); assert.equal(creates().length, 0)
})
test('manual reporting keeps its previous submit behavior without AI confirmation', async () => {
  const f = await mounted('/report/new'), target = f.state(f.components.form); fill(target); target.form.title = 'Manual'; target.form.description = 'Manual fault'
  await target.send(); assert.equal(confirmations.length, 0); assert.equal(creates().length, 1)
})
test('management and unauthenticated routes reject AI/new-report access through actual Router guards', async () => {
  for (const management of ['dispatcher', 'sysadmin']) {
    await signOut(); role = management; await signIn('fictional_user', 'not-a-real-password')
    const f = await mounted('/report/new'); assert.equal(f.router.currentRoute.value.path, '/forbidden')
  }
  await signOut(); const f = await mounted('/report/assistant'); assert.equal(f.router.currentRoute.value.path, '/login'); assert.equal(authState.status, 'anonymous')
})
