import { reactive, readonly } from 'vue'
import { loginRequest, logoutRequest, meRequest } from '../api/auth.js'
import { credentialSnapshot, onCredentialInvalidated, replaceCredential } from '../api/credentials.js'

const state = reactive({ status: 'anonymous', user: null, busy: false, notice: '' })
export const authState = readonly(state)
const roles = ['reporter', 'worker', 'dispatcher', 'sysadmin']
let operation = 0

function clearIdentity(notice = '') {
  state.user = null
  state.status = 'anonymous'
  state.notice = notice
}

onCredentialInvalidated(() => {
  operation += 1
  clearIdentity('登录已失效，请重新登录')
})

function checkedIdentity(value) {
  if (!value || !Number.isSafeInteger(value.id) || value.id <= 0 || !roles.includes(value.role) ||
      typeof value.username !== 'string' || typeof value.display_name !== 'string' ||
      !Array.isArray(value.capabilities) || !Array.isArray(value.scope_location_ids)) {
    throw new Error('后端返回的身份信息不完整')
  }
  return {
    id: value.id, username: value.username, display_name: value.display_name, role: value.role,
    capabilities: [...new Set(value.capabilities.filter((item) => typeof item === 'string'))],
    scope_location_ids: [...new Set(value.scope_location_ids.filter((id) => Number.isSafeInteger(id) && id > 0))],
  }
}

export async function signIn(username, password) {
  if (state.busy) return false
  const current = ++operation
  state.busy = true
  state.status = 'authenticating'
  state.notice = ''
  try {
    const result = await loginRequest(username, password)
    if (current !== operation) return false
    if (typeof result?.token !== 'string' || !result.token || typeof result.expires_at !== 'string') {
      throw new Error('后端返回的登录信息不完整')
    }
    replaceCredential(result.token)
    state.status = 'checking'
    const user = checkedIdentity(await meRequest())
    if (current !== operation) return false
    state.user = user
    state.status = 'authenticated'
    return true
  } catch (error) {
    // A successful login followed by a failed identity query is not an authenticated UI.
    if (credentialSnapshot().token && current === operation) {
      try { await logoutRequest() } catch { /* Offline: cannot claim server revocation. */ }
    }
    if (current === operation) { replaceCredential(); clearIdentity() }
    throw error
  } finally {
    state.busy = false
  }
}

export async function refreshIdentity() {
  if (state.busy || state.status !== 'authenticated') return
  const current = ++operation
  state.busy = true
  state.status = 'checking'
  try {
    const user = checkedIdentity(await meRequest())
    if (current !== operation) return
    state.user = user
    state.status = 'authenticated'
  } catch (error) {
    if (current === operation && error.status === 403) {
      state.status = 'authenticated'
      throw error
    }
    if (current === operation) { replaceCredential(); clearIdentity('身份查询失败，请重新登录') }
    throw error
  } finally { state.busy = false }
}

export async function signOut() {
  if (state.busy) return false
  state.busy = true
  state.status = 'loggingOut'
  operation += 1
  let confirmed = false
  try {
    await logoutRequest()
    confirmed = true
  } catch (error) {
    // 401 already means this session cannot authenticate; network failure does not.
    confirmed = error.status === 401
  } finally {
    replaceCredential()
    clearIdentity(confirmed ? '已安全退出当前会话' : '已清除本机登录状态；网络异常，服务端撤销尚未确认')
    state.busy = false
  }
  return confirmed
}
