import { statuses, urgencies } from './orders.js'

// Menu hints only. Each HTTP call is independently authorized by the backend.
export function managementAbility(user, capability) {
  return !!user && ((user.role === 'dispatcher' && user.capabilities.includes('AREA_SCOPE')) ||
    (user.role === 'sysadmin' && user.capabilities.includes(capability)))
}
export function managedQuery({ page = 1, status = '', category } = {}) {
  if (!Number.isSafeInteger(page) || page < 1 || (status && !Object.hasOwn(statuses, status))) throw new Error('筛选参数无效')
  if (category != null && (!Number.isSafeInteger(category) || category <= 0)) throw new Error('请选择真实分类')
  return { view: 'managed', page, page_size: 20, ...(status ? { status } : {}), ...(category ? { category_id: category } : {}) }
}
export function adminError(error) {
  if (error.status === 409) return '状态或版本已变化；请重新读取后确认，不会自动重放审核。'
  if (error.status === 422) return `字段不符合要求${error.field ? `（${error.field}）` : ''}；容量不能低于当前有效任务数，请核对输入。`
  if ([403, 404].includes(error.status)) return '权限、资源或授权范围已变化；不会请求其他区域的数据。'
  return error.message || '请求失败，请稍后重试'
}
export function reviewPayload(order, form) {
  if (order?.status !== 'PENDING_REVIEW' || !Number.isSafeInteger(order.version) || order.version <= 0) throw new Error('请读取当前待审核工单及真实版本')
  const body = { action: form.action, expected_version: order.version }
  if (form.action === 'APPROVE') {
    if (!Number.isSafeInteger(form.category_id) || form.category_id <= 0 || !Object.hasOwn(urgencies, form.approved_urgency)) throw new Error('请选择真实分类与确认紧急度')
    body.category_id = form.category_id; body.approved_urgency = form.approved_urgency
  } else if (['RETURN', 'REJECT'].includes(form.action)) {
    validateText(form.reason, 500, true); body.reason = form.reason
  } else throw new Error('请选择合法审核动作')
  if (form.internal_note != null) { validateText(form.internal_note, 2000, false); body.internal_note = form.internal_note }
  return body
}
function validateText(value, max, required) {
  if (typeof value !== 'string' || (required && !value.trim()) || [...value].length > max || /[\uD800-\uDBFF](?![\uDC00-\uDFFF])|(?<![\uD800-\uDBFF])[\uDC00-\uDFFF]|[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]/.test(value)) throw new Error(`请检查${required ? '公开原因（必填）' : '内部备注'}：最多${max}个Unicode字符，不允许非法控制字符`)
}
export function reviewFlow(api, order) {
  return {
    busy: false, blocked: false, current: order, verification: null, denied: false,
    async send(form) {
      if (this.busy) return null
      if (this.blocked) throw new Error('请关闭抽屉并重新读取工单后确认；禁止重放未知结果的请求')
      const body = reviewPayload(this.current, form); this.busy = true
      try {
        const result = await api.review(this.current.id, body)
        this.blocked = true
        this.current = await api.detail(this.current.id)
        this.verification = this.current
        return result
      } catch (e) {
        if (!e.status || e.status >= 500 || e.status === 409 || this.blocked) {
          this.blocked = true
          try { this.verification = await api.detail(order.id) }
          catch (readError) { this.verification = null; this.denied = [401, 403, 404].includes(readError.status) }
        }
        if ([401, 403, 404].includes(e.status)) { this.blocked = true; this.denied = true }
        throw e
      } finally { this.busy = false }
    },
  }
}
