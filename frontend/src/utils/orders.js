export const statuses = {
  DRAFT: '草稿', PENDING_REVIEW: '待审核', PENDING_SUPPLEMENT: '待补充',
  PENDING_DISPATCH: '待派单', PENDING_ACCEPT: '待接单', IN_PROGRESS: '维修中',
  PENDING_VERIFY: '待验收', COMPLETED: '已完成', CLOSED: '已关闭', CANCELLED: '已取消',
}
export const urgencies = { LOW: '低', NORMAL: '普通', HIGH: '高', URGENT: '紧急' }
export const actions = {
  ORDER_CREATED: '创建草稿', ORDER_SUBMITTED: '提交报修', ORDER_REVIEWED: '审核通过',
  ORDER_RETURNED: '退回补充', ORDER_REJECTED: '审核拒绝', ORDER_ASSIGNED: '派单',
  ORDER_ACCEPTED: '维修人员接单', ORDER_REFUSED: '拒单', TRANSFER_REQUESTED: '申请转派',
  TRANSFER_APPROVED: '转派获批', TRANSFER_REJECTED: '转派未通过', ORDER_COMPLETED: '提交维修结果',
  ORDER_VERIFIED: '验收通过', ORDER_REWORK_REQUESTED: '要求返工', ORDER_CANCELLED: '工单取消',
  ORDER_CLOSED: '工单关闭', ORDER_EVALUATED: '评价', WORKER_ARRIVED: '维修人员到场', WORKER_PROGRESS_NOTE: '维修进度',
}
export function dateText(value) {
  if (!value) return '—'
  const date = new Date(value)
  return Number.isNaN(date.getTime()) ? '—' : date.toLocaleString('zh-CN', { hour12: false })
}
export function orderError(error) {
  if (error.status === 404) return '资源不存在或无权访问'
  if (error.status === 409) return '工单状态或版本已变化，请核对最新详情后重试；不会自动创建另一张草稿。'
  if (error.status === 422 || error.status === 400) return '字段、选项或日期不符合要求，请检查输入；停用的分类和位置不能用于提交。'
  return error.message || '请求失败，请稍后重试'
}
export function reportPayload(form) {
  const body = {}
  for (const [key, max] of Object.entries({ title: 120, description: 2000, contact_name: 80, contact_phone: 32 })) {
    const value = form[key]
    if (typeof value !== 'string' || !value.trim() || [...value].length > max || /[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]/.test(value)) throw new Error(`请检查${{ title: '标题', description: '描述', contact_name: '联系人', contact_phone: '联系电话' }[key]}：必填且不能超过 ${max} 个字符`)
    body[key] = value
  }
  for (const key of ['category_id', 'location_id']) {
    if (!Number.isSafeInteger(form[key]) || form[key] <= 0) throw new Error('请选择真实的故障分类和位置')
    body[key] = form[key]
  }
  if (!Object.hasOwn(urgencies, form.requested_urgency)) throw new Error('请选择有效的紧急程度')
  body.requested_urgency = form.requested_urgency
  if (form.expected_visit_at) {
    const date = new Date(form.expected_visit_at)
    if (Number.isNaN(date.getTime()) || date.getUTCFullYear() < 1 || date.getUTCFullYear() > 9999) throw new Error('请选择有效的期望上门时间')
    body.expected_visit_at = date.toISOString()
  }
  return body
}
