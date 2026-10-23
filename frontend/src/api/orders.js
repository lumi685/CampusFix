import { apiClient } from './client.js'

const authenticated = { authRequired: true }
async function get(path, params) {
  return (await apiClient.get(path, { ...authenticated, params })).data.data
}
export const locations = (parentId) => get('/locations', parentId == null ? {} : { parent_id: parentId })
export const categories = (parentId) => get('/fault-categories', parentId == null ? {} : { parent_id: parentId })
export const reportedOrders = (params = {}) => get('/work-orders', { ...params, view: 'reported' })
export const orderDetail = (id) => get(`/work-orders/${orderId(id)}`)
export const orderTimeline = (id, params) => get(`/work-orders/${orderId(id)}/timeline`, params)
export const createOrder = async (body) => (await apiClient.post('/work-orders', body, authenticated)).data.data
export const submitOrder = async (id, version) => (await apiClient.post(`/work-orders/${orderId(id)}/submit`, { expected_version: version }, authenticated)).data.data
export function orderId(value) {
  const text = String(value)
  if (!/^[1-9]\d*$/.test(text) || !Number.isSafeInteger(Number(text))) throw new Error('资源不存在或无权访问')
  return text
}
