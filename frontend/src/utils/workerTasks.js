const integer = v => Number.isSafeInteger(v) && v > 0
const restricted = e => [401, 403, 404].includes(e.status)
function invalid() { throw new Error('服务器任务数据无效或超出安全整数范围，已禁止操作。') }
function row(o, status) {
  if (!o || !integer(o.id) || !integer(o.version) || o.version >= Number.MAX_SAFE_INTEGER || o.status !== status || typeof o.title !== 'string' || typeof o.order_no !== 'string' || typeof o.category?.name !== 'string' || typeof o.location?.name !== 'string' || !['LOW', 'NORMAL', 'HIGH', 'URGENT'].includes(o.approved_urgency)) invalid()
  return { id: o.id, version: o.version, status: o.status, title: o.title, order_no: o.order_no, category: o.category.name, location: o.location.name, urgency: o.approved_urgency }
}
// Unknown writes stay locked. Generations invalidate late reads AND writes.
export function workerTaskFlow(api, authorized = () => false) {
  let generation = 0
  return {
    phase: 'idle', items: [], total: 0, page: 1, status: 'PENDING_ACCEPT', selected: null, reason: '', message: '', verification: null, verifying: false,
    clear() { generation++; this.phase = 'idle'; this.items = []; this.total = 0; this.selected = null; this.reason = ''; this.message = ''; this.verification = null; this.verifying = false },
    async load(page = 1, status = this.status) {
      if (['submitting', 'unknown_result'].includes(this.phase)) return false
      this.clear(); const current = generation; this.page = page; this.status = status
      if (!authorized()) return false
      this.phase = 'loading'
      try {
        const data = await api.list({ page, status })
        if (current !== generation || !authorized()) return false
        if (!Array.isArray(data?.items) || !Number.isSafeInteger(data.total) || data.total < 0 || data.page !== page || data.page_size !== 20) invalid()
        const items = data.items.map(o => row(o, status)); if (new Set(items.map(o => o.id)).size !== items.length) invalid()
        this.items = items; this.total = data.total; this.phase = items.length ? 'ready' : 'empty'; return true
      } catch (e) {
        if (current !== generation) return false
        this.items = []; this.total = 0; this.phase = restricted(e) ? 'forbidden' : 'error'
        this.message = restricted(e) ? '身份、权限或资源已变化，旧任务已清除。' : '任务读取失败，请检查连接后重试。'; return false
      }
    },
    choose(id, kind) {
      if (this.phase !== 'ready' || this.status !== 'PENDING_ACCEPT' || !['accept', 'refuse'].includes(kind)) return false
      const task = this.items.find(o => o.id === id)
      if (!task) return false
      this.selected = { ...task, kind }; this.reason = ''; this.message = ''; return true
    },
    cancel() { if (this.phase === 'ready') { this.selected = null; this.reason = '' } },
    async verify() {
      if (this.phase !== 'unknown_result' || this.verifying || !this.selected || !authorized()) return false
      const current = generation, id = this.selected.id; this.verifying = true
      try {
        const d = await api.detail(id)
        if (current !== generation || !authorized()) return false
        if (d?.id !== id || !integer(d.version) || typeof d.status !== 'string') invalid()
        this.verification = { id, version: d.version, status: d.status }; return true
      } catch (e) {
        if (current !== generation) return false
        if (restricted(e)) { this.clear(); this.phase = 'forbidden'; this.message = '无权核实或登录已失效，旧任务数据已清除。' }
        else this.message = '操作结果仍未确认，只读查询也失败；禁止重发，请稍后核实或联系管理员。'
        return false
      } finally { if (current === generation) this.verifying = false }
    },
    async send() {
      if (this.phase === 'submitting') return null
      if (this.phase !== 'ready' || !this.selected || !authorized()) throw new Error('请刷新任务后重新人工确认；未知结果不得重发。')
      const current = generation, task = { ...this.selected }, reason = this.reason
      this.phase = 'submitting'; this.message = ''
      try {
        const d = await api.action(task.id, task.version, task.kind === 'refuse' ? reason : undefined)
        if (current !== generation || !authorized()) return null
        if (d?.order_id !== task.id || d.version !== task.version + 1 || d.status !== (task.kind === 'accept' ? 'IN_PROGRESS' : 'PENDING_DISPATCH') || (task.kind === 'accept' ? typeof d.accepted_at !== 'string' || !d.accepted_at : d.assignment_ended !== true)) invalid()
        this.phase = 'success'; this.selected = null; this.reason = ''; this.items = []; this.total = 0
        const loaded = await this.load(1, task.kind === 'accept' ? 'IN_PROGRESS' : 'PENDING_ACCEPT')
        if (current + 1 === generation && authorized()) this.message = loaded ? '服务器已确认操作成功；任务列表已重新读取。' : '服务器已确认操作成功，但列表重新读取失败；请只读刷新任务。'
        return d
      } catch (e) {
        if (current !== generation) return null
        this.items = []; this.total = 0; this.reason = ''
        if (!e.status || e.status >= 500) {
          this.phase = 'unknown_result'; this.message = '操作结果尚未确认，可能已提交；禁止自动重发。只读查询不能证明本次请求成功。'; await this.verify()
        } else if (restricted(e)) { this.clear(); this.phase = 'forbidden'; this.message = '身份、权限或资源已变化，旧任务已清除。' }
        else { this.selected = null; this.phase = e.status === 409 ? 'conflict' : 'error'; this.message = e.status === 409 ? '工单状态或版本已变化。已清除旧操作，请刷新任务；不会自动重发。' : '操作未被接受，请检查拒单原因并重新读取任务。' }
        return null
      }
    },
  }
}
