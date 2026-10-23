import { test, after } from 'node:test'
import assert from 'node:assert/strict'
import { randomBytes } from 'node:crypto'
import { AxiosError } from 'axios'
import { apiClient, ApiError } from '../src/api/client.js'
import { credentialSnapshot, replaceCredential } from '../src/api/credentials.js'
import { authState, signIn, signOut, refreshIdentity } from '../src/stores/auth.js'
import { navigation, visibleNavigation, canAccessEntry } from '../src/router/navigation.js'
import { routeAccess } from '../src/router/access.js'

const originalAdapter = apiClient.defaults.adapter
after(() => { apiClient.defaults.adapter = originalAdapter; replaceCredential() })
const secret = () => randomBytes(32).toString('base64url')
const user = (role = 'reporter', capabilities = []) => ({
  id: 1, username: 'unit_user', display_name: '虚构测试用户', role, capabilities, scope_location_ids: [1],
})
function ok(config, data) {
  return { config, status: 200, headers: {}, data: { code: 0, message: 'success', data, request_id: 'req_unit' } }
}
function failed(config, status) {
  return new AxiosError('Request failed', 'ERR_BAD_RESPONSE', config, undefined, {
    config, status, data: { code: status === 401 ? 'AUTH_INVALID' : 'ACTION_FORBIDDEN', data: null, request_id: 'req_unit' },
  })
}
async function loginAs(identity = user()) {
  const token = secret()
  apiClient.defaults.adapter = async (config) => {
    if (config.url === '/auth/login') {
      assert.equal(config.headers.get('Authorization'), undefined)
      return ok(config, { token, expires_at: '2099-01-01T00:00:00.000Z' })
    }
    if (config.url === '/auth/logout') return ok(config, { revoked: true })
    assert.equal(config.headers.get('Authorization'), `Bearer ${token}`)
    return ok(config, identity)
  }
  assert.equal(await signIn('unit_user', secret()), true)
  return token
}

test('anonymous routes, menus and protected routes fail closed', () => {
  assert.deepEqual(routeAccess({ path: '/my-orders', meta: { requiresAuth: true } }), { path: '/login', replace: true })
  assert.equal(routeAccess({ path: '/', meta: {} }), true)
  assert.deepEqual(visibleNavigation(null), [])
})

test('login waits for verified /me identity before exposing protected content', async () => {
  const token = secret()
  let resolveMe
  apiClient.defaults.adapter = (config) => {
    if (config.url === '/auth/login') return Promise.resolve(ok(config, { token, expires_at: '2099-01-01' }))
    return new Promise((resolve) => { resolveMe = () => resolve(ok(config, user('worker'))) })
  }
  const pending = signIn('unit_user', secret())
  await new Promise((resolve) => setImmediate(resolve))
  assert.equal(authState.status, 'checking')
  assert.equal(authState.user, null)
  assert.equal(await signIn('unit_user', secret()), false)
  resolveMe()
  assert.equal(await pending, true)
  assert.equal(authState.user.role, 'worker')
  assert.equal(Object.hasOwn(authState, 'token'), false)
  assert.deepEqual(routeAccess({ path: '/login', meta: {} }), { path: '/home', replace: true })
  assert.deepEqual(routeAccess({ path: '/admin/review', meta: { requiresAuth: true, entryId: 'admin-review' } }), { path: '/forbidden', replace: true })
  apiClient.defaults.adapter = async (config) => ok(config, { revoked: true })
  await signOut()
})

test('four role navigation uses explicit capabilities; worker retains own-report entries', () => {
  const titles = (identity) => visibleNavigation(identity).map((entry) => entry.id)
  assert.deepEqual(titles(user('reporter')), ['home', 'my-orders', 'report-new'])
  assert.ok(titles(user('worker')).includes('worker-tasks'))
  assert.ok(titles(user('worker')).includes('my-orders'))
  assert.ok(titles(user('worker')).includes('report-new'))
  assert.ok(!titles(user('dispatcher')).includes('admin-review'))
  assert.ok(titles(user('dispatcher', ['AREA_SCOPE'])).includes('admin-review'))
  assert.deepEqual(titles(user('sysadmin')), ['home', 'system'])
  const granted = titles(user('sysadmin', ['BUSINESS_READ', 'INTERNAL_NOTE_READ', 'STATS_VIEW']))
  assert.ok(granted.includes('admin-orders') && granted.includes('admin-dashboard'))
  assert.ok(!granted.includes('admin-review') && !granted.includes('admin-dispatch') && !granted.includes('admin-workers'))
  for (const entry of navigation) {
    assert.equal(canAccessEntry(user('sysadmin', ['AREA_SCOPE']), entry), ['home', 'system'].includes(entry.id))
  }
})

test('credentials are opt-in and blocked for external URLs and /api traversal', async () => {
  const token = await loginAs()
  let sent = 0
  apiClient.defaults.adapter = async (config) => {
    sent += 1
    assert.equal(config.headers.get('Authorization'), config.authRequired ? `Bearer ${token}` : undefined)
    return ok(config, {})
  }
  await apiClient.get('/health', { headers: { Authorization: 'must-be-removed' } })
  await apiClient.get('/auth/me', { authRequired: true })
  for (const url of ['https://third-party.invalid/collect', '//third-party.invalid', '/../outside']) {
    await assert.rejects(apiClient.get(url, { authRequired: true }), ApiError)
  }
  await assert.rejects(apiClient.get('/auth/me', { baseURL: 'https://third-party.invalid', authRequired: true }), ApiError)
  assert.equal(sent, 2)
})

test('403 keeps identity; 401 clears it and emits no Axios credentials in errors', async () => {
  const before = credentialSnapshot().token
  apiClient.defaults.adapter = async (config) => { throw failed(config, 403) }
  await assert.rejects(apiClient.get('/auth/me', { authRequired: true }), (error) => error.status === 403 && !error.config && !error.request)
  assert.equal(authState.status, 'authenticated')
  assert.equal(credentialSnapshot().token, before)
  await assert.rejects(refreshIdentity(), (error) => error.status === 403)
  assert.equal(authState.status, 'authenticated')
  assert.equal(credentialSnapshot().token, before)
  apiClient.defaults.adapter = async (config) => { throw failed(config, 401) }
  await assert.rejects(apiClient.get('/auth/me', { authRequired: true }), (error) => error.status === 401 && !error.config)
  assert.equal(authState.user, null)
  assert.equal(credentialSnapshot().token, '')
  assert.equal(authState.status, 'anonymous')
})

test('an old in-flight 401 cannot invalidate a newly issued credential', async () => {
  replaceCredential(secret())
  let rejectOld
  apiClient.defaults.adapter = (config) => new Promise((resolve, reject) => {
    rejectOld = () => reject(failed(config, 401))
  })
  const pending = apiClient.get('/auth/me', { authRequired: true })
  await new Promise((resolve) => setImmediate(resolve))
  const fresh = secret()
  replaceCredential(fresh)
  rejectOld()
  await assert.rejects(pending, ApiError)
  assert.equal(credentialSnapshot().token, fresh)
  replaceCredential()
})

test('failed /me after login never authenticates, and attempts to revoke the partial session', async () => {
  let revoked = false
  apiClient.defaults.adapter = async (config) => {
    if (config.url === '/auth/login') return ok(config, { token: secret(), expires_at: '2099-01-01' })
    if (config.url === '/auth/logout') { revoked = true; return ok(config, { revoked: true }) }
    throw failed(config, 503)
  }
  await assert.rejects(signIn('unit_user', secret()), ApiError)
  assert.equal(revoked, true)
  assert.equal(authState.user, null)
  assert.equal(authState.status, 'anonymous')
  assert.equal(credentialSnapshot().token, '')
})

test('offline logout clears local identity without claiming confirmed server revocation', async () => {
  await loginAs()
  apiClient.defaults.adapter = async () => { throw new AxiosError('Network failure', 'ERR_NETWORK') }
  assert.equal(await signOut(), false)
  assert.equal(authState.user, null)
  assert.equal(credentialSnapshot().token, '')
  assert.ok(authState.notice.includes('服务端撤销尚未确认'))
})
