import { apiClient } from './client.js'
import { dispatchId, validateAssignReason } from '../utils/dispatch.js'

export const getDispatchCandidates = async id => (await apiClient.get(`/work-orders/${dispatchId(id)}/dispatch-candidates`, { authRequired: true })).data.data
export async function assignWorker(id, payload) {
  const order = dispatchId(id)
  if (!Number.isSafeInteger(payload?.worker_id) || payload.worker_id <= 0 || !Number.isSafeInteger(payload?.expected_version) || payload.expected_version <= 0) throw new Error('派单ID或版本超出安全整数范围或无效。')
  if (payload.reason !== undefined) validateAssignReason(payload.reason)
  const body = { worker_id: payload.worker_id, expected_version: payload.expected_version, ...(payload.reason !== undefined ? { reason: payload.reason } : {}) }
  return (await apiClient.post(`/work-orders/${order}/assign`, body, { authRequired: true })).data.data
}
