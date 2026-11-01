import { apiClient } from './client.js'
import { orderId } from './orders.js'
import { workflowText } from './workflow.js'
export async function verifyOrder(id, version, approved, reason = '') {
  if (!Number.isSafeInteger(version) || version < 1 || version >= Number.MAX_SAFE_INTEGER || typeof approved !== 'boolean') throw new Error('验收参数无效，请刷新工单。')
  workflowText(reason,500,approved)
  const r=await apiClient.post(`/work-orders/${orderId(id)}/verify`,{approved,expected_version:version,...(reason?{reason}:{})},{authRequired:true})
  if(r.status!==200||r.data?.code!==0||r.data.message!=='success'||!/^req_[\w-]{1,100}$/.test(r.data.request_id||''))throw new Error('验收回执无效，结果未知。')
  return r.data.data
}
