import { apiClient } from './client.js'
import { orderId } from './orders.js'
export function workflowText(value, max = 500, optional = false) {
  if (typeof value !== 'string' || [...value].length > max || /[\uD800-\uDBFF](?![\uDC00-\uDFFF])|(?<![\uD800-\uDBFF])[\uDC00-\uDFFF]|[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]/.test(value) || (!optional && !value.replace(/[\p{White_Space}\uFEFF]/gu, ''))) throw new Error(`请输入合法文字，最多${max}个Unicode字符${optional ? '，可以留空' : '，不能全为空白'}。`)
  return value
}
const version = n => { if (!Number.isSafeInteger(n) || n <= 0 || n >= Number.MAX_SAFE_INTEGER) throw new Error('版本超出安全整数范围或无效。'); return n }
export async function transferRequests({ view, page = 1, page_size = 20, order_id } = {}) {
  if (!['mine','managed'].includes(view) || !Number.isSafeInteger(page) || page < 1 || !Number.isSafeInteger(page_size) || page_size < 1 || page_size > 100 || !Number.isSafeInteger((page - 1) * page_size)) throw new Error('转派查询参数无效。')
  const params = { view, page, page_size, ...(order_id == null ? {} : { order_id: orderId(order_id) }) }
  return (await apiClient.get('/transfer-requests', { authRequired: true, params })).data.data
}
export async function requestTransfer(id, expected, reason) {
  return (await apiClient.post(`/work-orders/${orderId(id)}/transfer-requests`, { expected_version: version(expected), reason: workflowText(reason) }, { authRequired: true })).data.data
}
export async function decideTransfer(id, requestId, expected, decision, reason) {
  if (!['APPROVE','REJECT'].includes(decision)) throw new Error('请选择合法审批决定。')
  return (await apiClient.post(`/work-orders/${orderId(id)}/transfer-requests/${orderId(requestId)}/decision`, { expected_version: version(expected), decision, reason: workflowText(reason) }, { authRequired: true })).data.data
}
export async function progressEvent(id, expected, type, note = '') {
  if (!['ARRIVED','PROGRESS_NOTE'].includes(type)) throw new Error('请选择合法进度事件。')
  workflowText(note, type === 'ARRIVED' ? 500 : 1000, type === 'ARRIVED')
  return (await apiClient.post(`/work-orders/${orderId(id)}/progress-events`, { expected_version: version(expected), type, ...(note ? { note } : {}) }, { authRequired: true })).data.data
}
export async function completeTask(id, expected, repair_summary) {
  const response = await apiClient.post(`/work-orders/${orderId(id)}/complete`, { expected_version: version(expected), repair_summary: workflowText(repair_summary, 2000) }, { authRequired: true })
  const body = response.data
  if (response.status !== 200 || body?.code !== 0 || body.message !== 'success' || typeof body.request_id !== 'string' || !/^req_[\w-]{1,100}$/.test(body.request_id)) throw new Error('维修提交回执不完整，结果未知，请只读核实。')
  return body.data
}
