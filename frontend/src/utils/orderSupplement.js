import { reportPayload } from './orders.js'

export function editPayload(form, version) {
  if (!Number.isSafeInteger(version) || version <= 0) throw new Error('请刷新真实工单版本')
  return { ...reportPayload(form), expected_visit_at: form.expected_visit_at ? new Date(form.expected_visit_at).toISOString() : null, expected_version: version }
}
export function editForm(detail) {
  return { category_id: detail.category?.id, location_id: detail.location?.id,
    title: detail.title ?? '', description: detail.description ?? '', requested_urgency: detail.requested_urgency,
    contact_name: detail.contact_name ?? '', contact_phone: detail.contact_phone ?? '',
    expected_visit_at: detail.expected_visit_at ? new Date(detail.expected_visit_at) : null }
}
// No identity fields are added to DTOs: reported is a server-filtered, authenticated view.
export async function ownsReportedOrder(id, role, fetchPage) {
  if (!['reporter', 'worker'].includes(role)) return false
  for (let page = 1; page <= 50; page++) {
    const result = await fetchPage({ page, page_size: 100 })
    if (result.items.some((item) => String(item.id) === String(id))) return true
    if (page * 100 >= result.total || !result.items.length) return false
  }
  throw new Error('本人报修记录较多，归属尚未确认；请稍后重试，不会开放未确认的编辑权限')
}
export async function latestReturn(id, fetchPage) {
  const first = await fetchPage(id, { page: 1, page_size: 100 })
  const last = Math.ceil(first.total / 100)
  for (let page = last, requests = 0; page >= 1 && requests < 50; page--, requests++) {
    const data = page === 1 ? first : await fetchPage(id, { page, page_size: 100 })
    for (const event of [...data.items].reverse()) {
      if (event.action === 'ORDER_RETURNED' && event.from_status === 'PENDING_REVIEW' && event.to_status === 'PENDING_SUPPLEMENT') return event
      if (event.action === 'ORDER_SUBMITTED') throw new Error('本轮退回原因尚未确认，请刷新工单与时间线')
    }
  }
  throw new Error('未能确认本轮公开退回原因，请重试；不会使用旧一轮原因')
}
export function supplementFlow(api, detail) {
  return {
    id: detail.id, version: detail.version, status: detail.status, busy: false, saved: false, blocked: false,
    async save(form) {
      if (this.busy) return null
      if (this.blocked || this.saved) throw new Error('请先核对最新数据，或只重试提交已保存内容')
      const payload = editPayload(form, this.version); this.busy = true
      try {
        const result = await api.edit(this.id, payload)
        if (result.id !== this.id || !Number.isSafeInteger(result.version) || result.version <= this.version || result.status !== this.status) throw new Error('保存响应不完整，请刷新核对原工单')
        this.version = result.version; this.saved = true
        return result
      } catch (e) {
        if (!e.status || e.status >= 500 || e.status === 409) this.blocked = true
        throw e
      } finally { this.busy = false }
    },
    async submit() {
      if (this.busy) return null
      if (this.blocked) throw new Error('保存结果或版本未确认，请先刷新核对；不会自动覆盖')
      this.busy = true
      try {
        const result = await api.submit(this.id, this.version)
        if (result?.id !== this.id || result.status !== 'PENDING_REVIEW' || !Number.isSafeInteger(result.version) || result.version <= this.version) throw new Error('提交响应未确认，请核对原工单状态')
        this.version = result.version; this.status = result.status; this.saved = false
        return result
      } catch (e) {
        if (!e.status || e.status >= 500 || e.status === 409) {
          try {
            const current = await api.detail(this.id)
            if (current.status === 'PENDING_REVIEW' && current.order_no && current.submitted_at) {
              this.version = current.version; this.status = current.status; this.saved = false; return current
            }
            // A changed version may be another client's edit; never silently submit it.
            if (current.version !== this.version || current.status !== this.status) this.blocked = true
          } catch { /* Preserve the known id/version and saved marker, never POST a new order. */ }
          if (e.status === 409) this.blocked = true
        }
        throw e
      } finally { this.busy = false }
    },
  }
}
