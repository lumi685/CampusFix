import { apiClient } from './client.js'

export async function loginRequest(username, password) {
  const response = await apiClient.post('/auth/login', { username, password })
  return response.data.data
}

export async function meRequest() {
  const response = await apiClient.get('/auth/me', { authRequired: true })
  return response.data.data
}

export async function logoutRequest() {
  const response = await apiClient.post('/auth/logout', null, { authRequired: true })
  if (response.data.data?.revoked !== true) throw new Error('后端未确认退出，请重试')
}
