import { apiClient } from './client.js'
import { orderId } from './orders.js'

export async function workerTasks({ page = 1, status = 'PENDING_ACCEPT' } = {}) {
  if (!Number.isSafeInteger(page) || page < 1 || !['PENDING_ACCEPT', 'IN_PROGRESS'].includes(status)) throw new Error('任务查询参数无效。')
  return (await apiClient.get('/work-orders', { authRequired: true, params: { view: 'assigned', current_only: true, status, page, page_size: 20 } })).data.data
}
export function refusalReason(reason) {
  if (typeof reason !== 'string' || [...reason].length > 500 || !reason.replace(/[\p{White_Space}\uFEFF]/gu, '') || /[\uD800-\uDBFF](?![\uDC00-\uDFFF])|(?<![\uD800-\uDBFF])[\uDC00-\uDFFF]|[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]/.test(reason)) throw new Error('拒单原因需为1～500个Unicode字符，不可全为空白或含非法控制字符。')
  return reason
}
export async function taskAction(id, version, reason) {
  const resource = orderId(id)
  if (!Number.isSafeInteger(version) || version < 1 || version >= Number.MAX_SAFE_INTEGER) throw new Error('工单版本无效或超出安全整数范围。')
  const body = { expected_version: version }
  if (reason !== undefined) body.reason = refusalReason(reason)
  return (await apiClient.post(`/work-orders/${resource}/${reason === undefined ? 'accept' : 'refuse'}`, body, { authRequired: true })).data.data
}
