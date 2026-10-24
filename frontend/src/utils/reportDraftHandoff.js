import { credentialSnapshot, onCredentialChanged } from '../api/credentials.js'
import { authState } from '../stores/auth.js'
import { watch } from 'vue'
import { draftFields, draftText } from './reportAssistant.js'

// Copy only the six editable text fields. Never carry credentials or database IDs.
export function draftSnapshot(value) {
  const keys = [...draftFields.map(([field]) => field), 'missing_information']
  if (!value || Array.isArray(value) || Object.keys(value).length !== keys.length ||
      keys.some((key) => !Object.hasOwn(value, key))) throw new Error('请核对有效建议后重试')
  draftText(value)
  const copy = Object.fromEntries(draftFields.map(([field]) => [field, value[field]]))
  copy.missing_information = [...value.missing_information].filter((text) => text !== '')
  return copy
}

export function prefillText(value) {
  // Standalone unknown markers are not actual fault facts. Never invent a fallback.
  return !value.trim() || /^(?:[（(]?\s*(?:未知|待补充|不详|未提供|未填写|null|undefined)\s*[）)]?)$/iu.test(value.trim()) ? '' : value
}

export function createReportDraftHandoff(session, { now = Date.now, ttl = 300000, schedule = setTimeout, cancel = clearTimeout } = {}) {
  let pending = null, timer
  function clear() { pending = null; cancel(timer); timer = undefined }
  function identity() {
    const s = session()
    return s?.authenticated && ['reporter', 'worker'].includes(s.role) && Number.isSafeInteger(s.id) && s.id > 0 && Number.isSafeInteger(s.revision) ? s : null
  }
  return {
    clear,
    offer(value) {
      clear()
      const s = identity()
      if (!s) throw new Error('当前会话不能使用报修建议，请重新登录后重试')
      const draft = draftSnapshot(value), ticket = Symbol('one-use-report-draft')
      pending = { draft, id: s.id, role: s.role, revision: s.revision, expires: now() + ttl, ticket }
      timer = schedule(clear, ttl); timer?.unref?.()
      return ticket
    },
    consume() {
      const p = pending, s = identity()
      clear()
      if (!p || !s || p.id !== s.id || p.role !== s.role || p.revision !== s.revision || now() >= p.expires) return null
      return draftSnapshot(p.draft)
    },
    discard(ticket) { if (pending?.ticket === ticket) clear() },
  }
}

export const reportDraftHandoff = createReportDraftHandoff(() => {
  const credential = credentialSnapshot()
  return { authenticated: authState.status === 'authenticated' && !!credential.token,
    id: authState.user?.id, role: authState.user?.role, revision: credential.revision }
})
onCredentialChanged(() => reportDraftHandoff.clear())
watch(() => authState.status, (status) => { if (!['authenticated', 'checking'].includes(status)) reportDraftHandoff.clear() }, { flush: 'sync' })
if (typeof window !== 'undefined') window.addEventListener('pagehide', () => reportDraftHandoff.clear())
