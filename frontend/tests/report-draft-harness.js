// Actual Vue runtime, SFC setup/template and memory Router; host nodes are not a browser DOM.
import { readFileSync } from 'node:fs'
import { parse, compileScript } from '@vue/compiler-sfc'
import { compile } from '@vue/compiler-dom'
import * as Vue from 'vue'
import * as Router from 'vue-router'
import * as credentials from '../src/api/credentials.js'
import * as auth from '../src/stores/auth.js'
import * as assistant from '../src/utils/reportAssistant.js'
import * as aiApi from '../src/api/reportAssistant.js'
import * as handoff from '../src/utils/reportDraftHandoff.js'
import * as orders from '../src/utils/orders.js'
import * as orderApi from '../src/api/orders.js'
import * as submission from '../src/utils/reportSubmission.js'
import * as catalog from '../src/utils/catalogPath.js'
import { routeAccess } from '../src/router/access.js'

export function pages(confirm) {
  const imports = { vue: Vue, 'vue-router': Router, 'element-plus': { ElMessageBox: { confirm } },
    '../stores/auth.js': auth, '../api/credentials.js': credentials, '../utils/reportDraftHandoff.js': handoff,
    '../utils/reportAssistant.js': assistant, '../api/reportAssistant.js': aiApi,
    '../utils/orders.js': orders, '../api/orders.js': orderApi, '../utils/reportSubmission.js': submission, '../utils/catalogPath.js': catalog }
  function load(path) {
    const { descriptor } = parse(readFileSync(new URL(path, import.meta.url), 'utf8'), { filename: path })
    const compiled = compileScript(descriptor, { id: path, genDefaultAs: 'component' }), names = [], values = []
    const source = compiled.content.replace(/^import\s+(.+?)\s+from\s+['"](.+?)['"];?\s*$/gm, (_, spec, target) => {
      const module = imports[target]
      if (!module) throw new Error(`Unmapped SFC dependency: ${target}`)
      if (spec.startsWith('{')) for (const entry of spec.slice(1, -1).split(',').map((s) => s.trim()).filter(Boolean)) {
        const [original, alias = original] = entry.split(/\s+as\s+/); names.push(alias); values.push(module[original])
      } else { names.push(spec); values.push(module.default) }
      return ''
    })
    const component = new Function(...names, `${source};return component`)(...values)
    component.render = new Function('Vue', compile(descriptor.template.content, { mode: 'function', prefixIdentifiers: true, bindingMetadata: compiled.bindings }).code)(Vue)
    return component
  }
  const picker = load('../src/components/CatalogPicker.vue'); imports['../components/CatalogPicker.vue'] = { default: picker }
  const preview = load('../src/components/AiReportDraftPreview.vue'); imports['../components/AiReportDraftPreview.vue'] = { default: preview }
  return { assistant: load('../src/views/ReportAssistantView.vue'), form: load('../src/views/ReportNewView.vue') }
}
function node(type, text = '') { return { type, tagName: type.toUpperCase(), text, children: [], parent: null, props: {}, addEventListener() {}, removeEventListener() {} } }
function remove(n) { if (n.parent) { const i = n.parent.children.indexOf(n); if (i >= 0) n.parent.children.splice(i, 1); n.parent = null } }
const renderer = Vue.createRenderer({
  createElement: (tag) => node(tag), createText: (s) => node('text', s), createComment: (s) => node('comment', s),
  setText: (n, s) => { n.text = s }, setElementText: (n, s) => { n.children = []; n.text = s },
  parentNode: (n) => n.parent, nextSibling: (n) => n.parent?.children[n.parent.children.indexOf(n) + 1] || null,
  insert(n, parent, anchor) { remove(n); n.parent = parent; const i = anchor ? parent.children.indexOf(anchor) : -1; if (i < 0) parent.children.push(n); else parent.children.splice(i, 0, n) },
  remove, patchProp: (n, key, _, value) => { n.props[key] = value },
})
export async function mountPages(components, path = '/report/assistant') {
  const router = Router.createRouter({ history: Router.createMemoryHistory(), routes: [
    { path: '/report/assistant', component: components.assistant, meta: { requiresAuth: true, entryId: 'report-assistant' } },
    { path: '/report/new', component: components.form, meta: { requiresAuth: true, entryId: 'report-new' } },
    { path: '/orders/:id', component: { render: () => Vue.h('p', 'actual detail route') } },
    ...['/login', '/forbidden', '/my-orders'].map((path) => ({ path, component: { render: () => Vue.h('p', path) } })),
  ] })
  router.beforeEach(routeAccess)
  const app = renderer.createApp({ render: () => Vue.h(Router.RouterView) })
  app.use(router)
  for (const name of ['ElAlert', 'ElForm', 'ElFormItem', 'ElButton', 'ElInput', 'ElSelect', 'ElOption', 'ElDatePicker', 'ElCascader']) app.component(name, {
    props: ['modelValue', 'disabled'], setup: (props, { slots, attrs }) => () => Vue.h('div', { ...attrs, disabled: props.disabled }, [props.modelValue == null ? '' : String(props.modelValue), slots.default?.()]),
  })
  await router.push(path); await router.isReady(); const root = node('root'); app.mount(root); await Vue.nextTick()
  function instances() {
    const list = []
    function visit(v) { if (!v) return; if (v.component) { list.push(v.component); visit(v.component.subTree) } else if (Array.isArray(v.children)) v.children.forEach(visit) }
    visit(app._instance.subTree); return list
  }
  return { app, router, root, instances, state: (component) => instances().find((i) => i.type === component)?.setupState }
}
export function hostText(root) { return root.text + root.children.map(hostText).join('') }
