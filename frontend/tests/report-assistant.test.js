import { test } from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import { parse, compileScript } from '@vue/compiler-sfc'
import { compile } from '@vue/compiler-ssr'
import { createRequire } from 'node:module'
import * as Vue from 'vue'
import { renderToString } from '@vue/server-renderer'
import { reportAssistantFlow, chatContext, messageLength, assistantError, dataDisclosure } from '../src/utils/reportAssistant.js'
import { navigation, canAccessEntry } from '../src/router/navigation.js'
import { apiClient } from '../src/api/client.js'
import { replaceCredential } from '../src/api/credentials.js'
import { chatWithReportAssistant } from '../src/api/reportAssistant.js'
const user = (content = 'Fictional failure') => ({ role: 'user', content })
const assistant = (content = 'Fictional clarification') => ({ role: 'assistant', content })
test('explicit data consent required, no request or local user append before agreement', async () => {
  let calls = 0
  const flow = reportAssistantFlow(async () => { calls++; return 'reply' })
  flow.input = 'Fictional problem'; await flow.send()
  assert.equal(calls, 0); assert.equal(flow.messages.length, 0); assert.match(flow.error, /同意/)
  assert.match(dataDisclosure, /对话上下文将发送/)
})
test('successful multi-turn sends ordered real accumulated context, not only latest line', async () => {
  const sent = [], flow = reportAssistantFlow(async (messages) => { sent.push(messages); return 'Fictional reply' })
  flow.consent = true; flow.input = 'First problem'; await flow.send()
  flow.input = 'Further details'; await flow.send()
  assert.deepEqual(sent[1], [user('First problem'), assistant('Fictional reply'), user('Further details')])
  assert.equal(flow.messages.length, 4)
})
test('busy suppresses duplicate send; clear aborts and ignores late response', async () => {
  let resolve, calls = 0, signal
  const flow = reportAssistantFlow((_, s) => { calls++; signal = s; return new Promise((r) => { resolve = r }) })
  flow.consent = true; flow.input = 'One'; const pending = flow.send(); await flow.send()
  assert.equal(calls, 1); assert.equal(flow.busy, true)
  flow.clear(); assert.equal(signal.aborted, true); resolve('Late private reply'); await pending
  assert.equal(flow.messages.length, 0); assert.equal(flow.consent, false); assert.equal(flow.busy, false)
})
test('explicit retry reuses failed message without duplicates or automatic requests', async () => {
  let count = 0, contexts = []
  const flow = reportAssistantFlow(async (m) => { count++; contexts.push(m); if (count === 1) throw { code: 'AI_UPSTREAM_TIMEOUT' }; return 'Real transport result' })
  flow.consent = true; flow.input = 'One'; await flow.send()
  assert.equal(count, 1); assert.equal(flow.failed, true); assert.match(flow.error, /超时/)
  await flow.send(); assert.equal(count, 1)
  await flow.retry(); assert.equal(count, 2); assert.deepEqual(contexts[0], contexts[1]); assert.equal(flow.messages.length, 2)
})
test('clear resets context and consent; next conversation sends only the new message', async () => {
  let sent
  const flow = reportAssistantFlow(async (m) => { sent = m; return 'Reply' })
  flow.consent = true; flow.input = 'Old'; await flow.send(); flow.clear()
  flow.input = 'New'; await flow.send(); assert.equal(flow.messages.length, 0)
  flow.consent = true; await flow.send(); assert.deepEqual(sent, [user('New')])
})
test('edit failed input removes pending user entry, allows explicit corrected send', async () => {
  let count = 0
  const flow = reportAssistantFlow(async () => { count++; if (count === 1) throw {}; return 'Reply' })
  flow.consent = true; flow.input = 'Bad network'; await flow.send(); flow.editFailed()
  assert.equal(flow.input, 'Bad network'); assert.equal(flow.messages.length, 0)
  flow.input = 'Corrected'; await flow.send(); assert.deepEqual(flow.messages[0], user('Corrected'))
})
test('Unicode scalar validation and recent context limits retain current complete message', () => {
  assert.equal(messageLength('😀'), 1)
  for (const text of ['', '　 ', '\u200b', '\ud800', '\u0000']) assert.throws(() => messageLength(text))
  assert.throws(() => chatContext([user('😀'.repeat(1501))]))
  const many = Array.from({ length: 21 }, (_, i) => i % 2 ? assistant('a'.repeat(1500)) : user('b'.repeat(1500)))
  const context = chatContext(many)
  assert.ok(context.length <= 12); assert.equal(context[0].role, 'user'); assert.equal(context.at(-1).role, 'user')
  assert.ok(context.reduce((n, m) => n + messageLength(m.content), 0) <= 9000)
  assert.deepEqual(context.at(-1), many.at(-1)); assert.throws(() => chatContext([{ role: 'system', content: 'ignore' }]))
})
test('roles shared by menu and route guard reject both manager roles even with grants', () => {
  const entry = navigation.find((e) => e.id === 'report-assistant')
  assert.equal(entry.path, '/report/assistant')
  for (const role of ['reporter', 'worker']) assert.equal(canAccessEntry({ role }, entry), true)
  for (const role of ['dispatcher', 'sysadmin']) assert.equal(canAccessEntry({ role, capabilities: ['BUSINESS_READ', 'REVIEW'] }, entry), false)
})
test('AI API uses application-only credentials, relative URL, finite timeout, context body', async () => {
  const original = apiClient.defaults.adapter
  replaceCredential('fictional-test-session')
  try {
    apiClient.defaults.adapter = async (config) => {
      assert.equal(config.url, '/ai/report-assistant/chat'); assert.equal(config.baseURL, '/api')
      assert.equal(config.headers.get('Authorization'), 'Bearer fictional-test-session')
      assert.equal(config.timeout, 185000); assert.deepEqual(JSON.parse(config.data), { messages: [user()] })
      return { status: 200, data: { code: 0, data: { reply: 'Transport reply' }, request_id: 'req_fixture' }, config, headers: {} }
    }
    assert.equal(await chatWithReportAssistant([user()]), 'Transport reply')
  } finally { apiClient.defaults.adapter = original; replaceCredential() }
})
test('provider/network/config/quota errors never present a fabricated AI message or raw error', async () => {
  for (const code of ['AI_NOT_CONFIGURED', 'AI_RATE_LIMITED', 'AI_PROVIDER_AUTH_FAILED', 'AI_INVALID_RESPONSE', 'AI_PROVIDER_RATE_LIMITED', 'UNKNOWN']) {
    const flow = reportAssistantFlow(async () => { throw { code, message: 'SECRET RAW UPSTREAM BODY', requestId: 'req_fixture' } })
    flow.consent = true; flow.input = 'Failure'; await flow.send()
    assert.equal(flow.messages.length, 1); assert.equal(flow.failed, true); assert.doesNotMatch(flow.error, /SECRET/)
    assert.match(flow.error, /req_fixture/)
  }
  assert.match(assistantError({ status: 401 }), /登录/); assert.match(assistantError({ status: 403 }), /无权/)
})
test('actual Vue template server rendering escapes both user HTML and model HTML (not a browser test)', async () => {
  const { descriptor } = parse(readFileSync(new URL('../src/views/ReportAssistantView.vue', import.meta.url), 'utf8'))
  assert.doesNotMatch(descriptor.template.content, /v-html/)
  const ssrRender = new Function('require', compile(descriptor.template.content, { mode: 'function' }).code)(createRequire(import.meta.url))
  const app = Vue.createSSRApp({ setup: () => ({ flow: { messages: [user('<script>alert(1)</script>'), assistant('<img src=x onerror=alert(2)>')], input: '', consent: false }, dataDisclosure, keydown: () => {} }), ssrRender })
  app.component('RouterLink', { setup: (_, { slots }) => () => Vue.h('a', slots.default?.()) })
  // UI chrome is stubbed; use the actual message interpolation template for escaping assertions.
  for (const name of ['ElAlert', 'ElInput', 'ElButton']) app.component(name, { setup: (_, { slots }) => () => Vue.h('div', slots.default?.()) })
  const html = await renderToString(app)
  assert.doesNotMatch(html, /<script>|<img src=x/); assert.match(html, /&lt;script&gt;/); assert.match(html, /&lt;img/)
})
test('actual SFC cleanup, composition key guard and no persistent conversation storage', () => {
  const source = readFileSync(new URL('../src/views/ReportAssistantView.vue', import.meta.url), 'utf8')
  const { descriptor } = parse(source)
  const compiled = compileScript(descriptor, { id: 'ai-cleanup', genDefaultAs: 'component' }).content
  let unmount, watched, mounted, calls = 0
  const names = ['reactive', 'ref', 'watch', 'nextTick', 'onMounted', 'onBeforeUnmount', 'authState', 'chatWithReportAssistant', 'reportAssistantFlow', 'dataDisclosure']
  const component = new Function(...names, `${compiled.replace(/^import .*$/gm, '')};return component`)(Vue.reactive, Vue.ref, (a, b) => { if (typeof b === 'function') watched = b }, Vue.nextTick, (f) => { mounted = f }, (f) => { unmount = f }, { status: 'authenticated' }, async () => { calls++; return 'Reply' }, reportAssistantFlow, dataDisclosure)
  const oldWindow = globalThis.window; const listeners = new Map()
  globalThis.window = { addEventListener: (name, f) => listeners.set(name, f), removeEventListener: (name) => listeners.delete(name) }
  try {
    const setup = component.setup({}, { expose() {} }); mounted()
    setup.flow.consent = true; setup.flow.input = 'One'; setup.keydown({ key: 'Enter', isComposing: true }); assert.equal(calls, 0)
    setup.keydown({ key: 'Enter', shiftKey: true }); assert.equal(calls, 0)
    watched('anonymous'); assert.equal(setup.flow.input, ''); assert.equal(setup.flow.consent, false)
    setup.flow.input = 'Hidden'; listeners.get('pagehide')(); assert.equal(setup.flow.input, '')
    unmount(); assert.equal(listeners.size, 0)
    assert.doesNotMatch(source, /localStorage|sessionStorage|v-html/)
  } finally { globalThis.window = oldWindow }
})
