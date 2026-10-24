import { test } from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import { createRequire } from 'node:module'
import { parse, compileScript } from '@vue/compiler-sfc'
import { compile } from '@vue/compiler-ssr'
import { createSSRApp, h, reactive, ref, nextTick } from 'vue'
import { renderToString } from '@vue/server-renderer'
import { reportAssistantFlow, chatContext, draftContext, checkedDraft, draftFields, draftText } from '../src/utils/reportAssistant.js'
import { generateReportDraft } from '../src/api/reportAssistant.js'
import { apiClient } from '../src/api/client.js'
import { replaceCredential } from '../src/api/credentials.js'
const user = (content = 'Fictional fault') => ({ role: 'user', content })
const assistant = () => ({ role: 'assistant', content: 'Fictional reply' })
const result = () => ({ requires_review: true, draft: { title: 'Fictional leak', description: 'Known symptoms only', category_hint: null, location_hint: 'Location unknown', missing_information: ['Building and room'], safety_note: 'Avoid wet floor' } })
function ready(request = async () => result(), chat = async () => 'Chat reply') {
  const f = reportAssistantFlow(chat, request); f.consent = true; f.messages = [user(), assistant()]; return f
}
test('no transcript, no consent, busy chat or unresolved chat cannot generate', async () => {
  for (const change of [(f) => { f.messages = [] }, (f) => { f.consent = false }, (f) => { f.busy = true }, (f) => { f.failed = true }]) {
    let calls = 0; const f = ready(async () => { calls++; return result() }); change(f); await f.generateDraft(); assert.equal(calls, 0); assert.equal(f.draft, null)
  }
})
test('draft context allows assistant ending without weakening chat context', () => {
  const messages = [user(), assistant()]
  assert.deepEqual(draftContext(messages), messages); assert.throws(() => chatContext(messages))
  assert.deepEqual(draftContext([user()]), [user()])
  assert.throws(() => draftContext([assistant()]))
  assert.throws(() => draftContext([user('x'.repeat(1500)), ...Array.from({ length: 11 }, () => ({ role: 'assistant', content: 'x'.repeat(1500) }))]))
})
test('successful result editable as text and server review flag is mandatory', async () => {
  let sent; const f = ready(async (m) => { sent = m; return result() }); await f.generateDraft()
  assert.deepEqual(sent, [user(), assistant()]); assert.equal(f.draft.title, 'Fictional leak'); assert.equal(f.draft.category_hint, '')
  assert.equal('category_id' in f.draft, false); assert.equal('location_id' in f.draft, false)
  assert.throws(() => checkedDraft({ ...result(), requires_review: false }))
})
test('chat and draft mutually block duplicates while generating', async () => {
  let resolve, calls = 0, chats = 0
  const f = ready(() => { calls++; return new Promise((r) => { resolve = r }) }, async () => { chats++; return 'Reply' })
  const pending = f.generateDraft(); await f.generateDraft(); f.input = 'Next'; await f.send()
  assert.equal(calls, 1); assert.equal(chats, 0); assert.equal(f.draftBusy, true)
  resolve(result()); await pending; assert.equal(f.draftBusy, false)
})
test('draft model error preserves transcript and does not synthesize result or retry', async () => {
  let calls = 0; const f = ready(async () => { calls++; throw { code: 'AI_DRAFT_INVALID_RESPONSE', message: 'RAW_SECRET' } })
  await f.generateDraft(); assert.equal(calls, 1); assert.equal(f.messages.length, 2); assert.equal(f.draft, null)
  assert.match(f.draftError, /有效的报修建议/); assert.doesNotMatch(f.draftError, /RAW_SECRET/)
})
test('continue conversation immediately invalidates the old suggestion', async () => {
  const f = ready(); await f.generateDraft(); f.input = 'New actual fact'; await f.send()
  assert.equal(f.draftStale, true); assert.ok(f.draft); let copies = 0; await f.copyDraft(async () => { copies++ }); assert.equal(copies, 0)
  await f.generateDraft(); assert.equal(f.draftStale, false)
})
test('clear / page cleanup aborts draft and ignores its delayed result', async () => {
  let resolve, signal; const f = ready((_, s) => { signal = s; return new Promise((r) => { resolve = r }) })
  const pending = f.generateDraft(); f.clear(); assert.equal(signal.aborted, true)
  resolve(result()); await pending; assert.equal(f.draft, null); assert.equal(f.messages.length, 0); assert.equal(f.draftError, ''); assert.equal(f.draftBusy, false)
})
test('cancel pending suggestion preserves chat, late completion cannot reopen preview', async () => {
  let resolve; const f = ready(() => new Promise((r) => { resolve = r })); const pending = f.generateDraft()
  f.cancelDraft(); resolve(result()); await pending
  assert.equal(f.draft, null); assert.equal(f.messages.length, 2); assert.equal(f.consent, true)
})
test('copy includes current edits, success notice clears on further edits', async () => {
  const f = ready(); await f.generateDraft(); f.editDraft({ ...f.draft, title: 'Edited title', description: 'Edited description', category_hint: '', missing_information: [] })
  let copied; await f.copyDraft(async (text) => { copied = text })
  assert.match(copied, /Edited title/); assert.match(copied, /Edited description/); assert.doesNotMatch(copied, /Known symptoms only/)
  assert.match(f.copyNotice, /已复制/); f.editDraft({ ...f.draft, title: 'Further edit' }); assert.equal(f.copyNotice, '')
})
test('clipboard failure keeps edits and never claims successful transfer', async () => {
  const f = ready(); await f.generateDraft(); await f.copyDraft(async () => { throw new Error('denied') })
  assert.match(f.copyNotice, /复制失败/); assert.equal(f.draft.title, 'Fictional leak')
})
test('clearing while copy awaits cannot restore old success notice', async () => {
  const f = ready(); await f.generateDraft(); let resolve; const pending = f.copyDraft(() => new Promise((r) => { resolve = r }))
  f.clear(); resolve(); await pending; assert.equal(f.copyNotice, ''); assert.equal(f.draft, null)
})
test('explicit regenerate replaces previous edited suggestion, never auto-retries', async () => {
  let calls = 0; const f = ready(async () => { calls++; const r = result(); r.draft.title = `generation ${calls}`; return r })
  await f.generateDraft(); f.draft.title = 'edited'; await f.generateDraft(); assert.equal(calls, 2); assert.equal(f.draft.title, 'generation 2')
})
test('draft context uses bounded retained messages and never invents deleted history', () => {
  const m = Array.from({ length: 20 }, (_, i) => ({ role: i % 2 ? 'assistant' : 'user', content: `Fact ${i}` }))
  const context = draftContext(m); assert.ok(context.length <= 12); assert.equal(context[0].role, 'user'); assert.equal(context.at(-1).content, 'Fact 19')
  assert.equal(context.some((x) => x.content === 'Fact 0'), false)
})
test('draft adapter calls only AI-02 through authenticated application API', async () => {
  replaceCredential('fictional-test-session'); const original = apiClient.defaults.adapter
  try {
    apiClient.defaults.adapter = async (config) => {
      assert.equal(config.url, '/ai/report-assistant/draft'); assert.equal(config.baseURL, '/api'); assert.equal(config.authRequired, true)
      assert.equal(config.timeout, 185000); assert.deepEqual(JSON.parse(config.data), { messages: [user(), assistant()] })
      return { status: 200, data: { code: 0, data: result(), request_id: 'req_fixture' }, headers: {}, config }
    }
    assert.deepEqual(await generateReportDraft([user(), assistant()]), result())
  } finally { apiClient.defaults.adapter = original; replaceCredential() }
})
test('output checks reject unknown IDs, types, missing/extra fields and invalid Unicode', () => {
  for (const mutate of [(r) => { r.draft.category_id = 999 }, (r) => { delete r.draft.title }, (r) => { r.draft.title = 'x'.repeat(121) }, (r) => { r.draft.description = 2 }, (r) => { r.draft.missing_information = Array(6).fill('a') }, (r) => { r.draft.title = '\ud800' }]) { const r = result(); mutate(r); assert.throws(() => checkedDraft(r)) }
  const d = checkedDraft(result()); d.title = 'x'.repeat(121); assert.throws(() => draftText(d))
})
test('actual preview setup emits edits for all fields and missing-information entries', () => {
  const { descriptor } = parse(readFileSync(new URL('../src/components/AiReportDraftPreview.vue', import.meta.url), 'utf8'))
  const code = compileScript(descriptor, { id: 'ai-draft-edit', genDefaultAs: 'component' }).content.replace(/^import .*$/gm, '')
  const component = new Function('draftFields', `${code};return component`)(draftFields)
  const props = { modelValue: checkedDraft(result()) }, events = []
  const setup = component.setup(props, { expose() {}, emit: (name, value) => { events.push(name); props.modelValue = value } })
  for (const [field] of draftFields) setup.edit(field, `edited ${field}`)
  setup.editMissing(0, 'Edited missing'); assert.equal(props.modelValue.missing_information[0], 'Edited missing')
  setup.removeMissing(0); assert.equal(props.modelValue.missing_information.length, 0)
  assert.equal(events.length, 7); assert.ok(events.every((e) => e === 'update:modelValue'))
})
test('actual preview SSR escapes model and edited text, no executable markup (not browser)', async () => {
  const { descriptor } = parse(readFileSync(new URL('../src/components/AiReportDraftPreview.vue', import.meta.url), 'utf8'))
  const render = new Function('require', compile(descriptor.template.content, { mode: 'function' }).code)(createRequire(import.meta.url))
  const d = checkedDraft(result()); d.title = '<script>alert(1)</script>'; d.missing_information = ['<img src=x onerror=alert(1)>']
  const app = createSSRApp({ setup: () => ({ modelValue: d, stale: false, busy: false, canGenerate: true, copyNotice: '', draftFields, edit() {}, editMissing() {}, removeMissing() {}, emit() {} }), ssrRender: render })
  for (const name of ['ElForm', 'ElFormItem', 'ElButton', 'ElAlert', 'RouterLink']) app.component(name, { setup: (_, { slots }) => () => h('div', slots.default?.()) })
  app.component('ElInput', { props: ['modelValue'], setup: (props) => () => h('textarea', props.modelValue) })
  const html = await renderToString(app); assert.doesNotMatch(html, /<script>|<img src=x/); assert.match(html, /&lt;script&gt;/); assert.match(html, /&lt;img/)
})
test('actual page auth loss and unmount clear pending suggestions and edited state', async () => {
  const { descriptor } = parse(readFileSync(new URL('../src/views/ReportAssistantView.vue', import.meta.url), 'utf8'))
  const code = compileScript(descriptor, { id: 'draft-cleanup', genDefaultAs: 'component' }).content.replace(/^import .*$/gm, '')
  for (const kind of ['authentication', 'unmount']) {
    let authChanged, unmount, resolve, signal, mounted
    const names = ['reactive', 'ref', 'watch', 'nextTick', 'onMounted', 'onBeforeUnmount', 'authState', 'chatWithReportAssistant', 'generateReportDraft', 'AiReportDraftPreview', 'reportAssistantFlow', 'dataDisclosure']
    const component = new Function(...names, `${code};return component`)(reactive, ref, (_, cb) => { authChanged = cb }, nextTick, (f) => { mounted = f }, (f) => { unmount = f }, { status: 'authenticated' }, async () => 'Reply', (_, s) => { signal = s; return new Promise((r) => { resolve = r }) }, {}, reportAssistantFlow, 'Disclosure')
    const previous = globalThis.window; globalThis.window = { addEventListener() {}, removeEventListener() {} }
    try {
      const setup = component.setup({}, { expose() {} }); mounted()
      setup.flow.messages = [user(), assistant()]; setup.flow.consent = true
      setup.flow.draft = checkedDraft(result()); setup.flow.draft.title = 'Unconfirmed edit'
      const pending = setup.flow.generateDraft()
      if (kind === 'authentication') authChanged('anonymous'); else unmount()
      assert.equal(signal.aborted, true); assert.equal(setup.flow.draft, null); assert.equal(setup.flow.messages.length, 0)
      resolve(result()); await pending; assert.equal(setup.flow.draft, null); assert.equal(setup.flow.draftError, '')
    } finally { globalThis.window = previous }
  }
})
test('no order writes, prefill, persistence or HTML execution in AI-D2 UI modules', () => {
  for (const path of ['../src/components/AiReportDraftPreview.vue', '../src/views/ReportAssistantView.vue', '../src/api/reportAssistant.js', '../src/utils/reportAssistant.js']) {
    const source = readFileSync(new URL(path, import.meta.url), 'utf8')
    assert.doesNotMatch(source, /createOrder|submitOrder|editOrder|\/api\/work-orders|localStorage|sessionStorage|v-html/)
  }
  const source = readFileSync(new URL('../src/components/AiReportDraftPreview.vue', import.meta.url), 'utf8')
  assert.match(source, /to="\/report\/new"/); assert.match(source, /max-width: 480px/)
  // Static responsiveness wiring only; actual 390px browser inspection remains manual.
})
