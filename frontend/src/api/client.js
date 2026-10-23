import axios from 'axios'
import { credentialSnapshot, invalidateCredential } from './credentials.js'

export class ApiError extends Error {
  constructor(message, status = 0, code = '', requestId = '') {
    super(message)
    this.name = 'ApiError'
    this.status = status
    this.code = code
    this.requestId = requestId
  }
}

export const apiClient = axios.create({
  baseURL: '/api',
  timeout: 5000,
  withCredentials: false,
})

apiClient.interceptors.request.use((config) => {
  // Credentials must never follow an absolute/external URL or a path escaping /api.
  const url = config.url || ''
  if (config.baseURL !== '/api' || !url.startsWith('/') || url.startsWith('//') || url.includes('\\')) {
    throw new ApiError('请求地址不属于本应用 API')
  }
  const resolved = new URL(`/api${url}`, 'http://campusfix.local')
  if (!resolved.pathname.startsWith('/api/')) throw new ApiError('请求地址不属于本应用 API')
  config.headers.delete('Authorization')
  if (config.authRequired === true) {
    const snapshot = credentialSnapshot()
    if (!snapshot.token) throw new ApiError('请先登录', 401, 'AUTH_REQUIRED')
    config.headers.set('Authorization', `Bearer ${snapshot.token}`)
    config.authRevision = snapshot.revision
  }
  return config
})

function safeError(status, code, requestId, timeout = false) {
  let message = '请求未完成，请稍后重试'
  if (timeout) message = '连接后端超时，请稍后重试'
  else if (status === 0 || status >= 500) message = '无法连接后端或服务暂不可用，请确认后端已启动'
  else if (status === 401) message = '用户名或密码错误，或当前登录已失效'
  else if (status === 403) message = '当前账号无权访问此功能'
  else if (status === 404) message = '资源不存在或无权访问'
  else if (status === 409) message = '工单状态或版本已变化，请核对最新数据'
  else if (status === 400 || status === 422) message = '请求字段格式不正确，请检查输入'
  return new ApiError(message, status, code, requestId)
}

apiClient.interceptors.response.use(
  (response) => {
    const body = response.data
    if (body?.code !== 0 || typeof body?.request_id !== 'string' || !body.request_id || !('data' in body)) {
      throw new ApiError('后端响应格式不正确')
    }
    return response
  },
  (error) => {
    if (error instanceof ApiError) return Promise.reject(error)
    const status = error.response?.status || 0
    const body = error.response?.data
    if (status === 401 && error.config?.authRequired === true) {
      invalidateCredential(error.config.authRevision)
    }
    // Never propagate the Axios config/request: those can contain credentials.
    return Promise.reject(safeError(status, typeof body?.code === 'string' ? body.code : '',
      typeof body?.request_id === 'string' && /^req_[\w-]{1,100}$/.test(body.request_id) ? body.request_id : '',
      error.code === 'ECONNABORTED'))
  },
)
