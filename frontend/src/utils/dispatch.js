import { orderId } from '../api/orders.js'

export const scoreLabels = { skill: '技能分', area: '区域分', load: '负载分', efficiency: '历史效率分', rating: '历史评价分', duty: '紧急值班分' }
export const exclusionLabels = {
  ACCOUNT_INACTIVE: '账号未启用', ACCOUNT_ROLE_MISMATCH: '账号角色不符合维修人员要求', WORKER_INACTIVE: '人员档案已停用',
  UNAVAILABLE: '当前不可接单', MISSING_SKILL: '缺少有效必需技能', MISSING_CERTIFICATE: '缺少必需资质',
  OUTSIDE_SERVICE_AREA: '不在服务区域', AT_CAPACITY: '当前任务量已满', LOCATION_UNVERIFIED: '位置或服务区域尚未核实',
}
const weights = { skill: .35, area: .2, load: .2, efficiency: .1, rating: .1, duty: .05 }
const object = v => v !== null && typeof v === 'object' && !Array.isArray(v)
const integer = (v, min = 1) => Number.isSafeInteger(v) && v >= min
function invalid() { throw new Error('后端调度数据不完整或超出 JavaScript 安全整数范围，已禁止派单；请核对接口数据。') }
export function dispatchId(value) {
  try { return orderId(value) } catch { throw new Error('请输入正整数工单 ID；超出 JavaScript 安全整数范围的 ID 暂不支持，不能截断或取整。') }
}
// Projection/validation only: never compute a score, reorder or decide eligibility.
export function dispatchSnapshot(value, expectedId) {
  if (!object(value) || !integer(value.order_id) || String(value.order_id) !== dispatchId(expectedId) || !integer(value.order_version) ||
      value.strategy !== 'WEIGHTED_SCORE_V1' || !object(value.weights) || !object(value.excluded_summary) ||
      !Array.isArray(value.top_candidates) || value.top_candidates.length > 3 || !Array.isArray(value.other_eligible_candidates) ||
      (value.other_eligible_candidates.length && value.top_candidates.length !== 3)) invalid()
  for (const key of Object.keys(weights)) if (value.weights[key] !== weights[key]) invalid()
  const seen = new Set()
  const rows = [...value.top_candidates, ...value.other_eligible_candidates].map((c, index) => {
    if (!object(c) || !integer(c.worker_id) || seen.has(c.worker_id) || c.rank !== index + 1 || !integer(c.rank) ||
        typeof c.worker_name !== 'string' || !c.worker_name || typeof c.reason !== 'string' ||
        !Array.isArray(c.risks) || c.risks.some(v => typeof v !== 'string') ||
        !integer(c.active_tasks, 0) || !integer(c.max_concurrent_tasks) || !object(c.score_detail) || !object(c.sample_counts) ||
        ![c.total_score, ...Object.keys(weights).map(k => c.score_detail[k])].every(v => typeof v === 'number' && Number.isFinite(v) && v >= 0 && v <= 100) ||
        !['efficiency', 'rating'].every(k => integer(c.sample_counts[k], 0) && c.sample_counts[k] <= 20)) invalid()
    seen.add(c.worker_id)
    return { rank: c.rank, worker_id: c.worker_id, worker_name: c.worker_name, total_score: c.total_score,
      active_tasks: c.active_tasks, max_concurrent_tasks: c.max_concurrent_tasks,
      score_detail: Object.fromEntries(Object.keys(weights).map(k => [k, c.score_detail[k]])),
      sample_counts: { efficiency: c.sample_counts.efficiency, rating: c.sample_counts.rating }, reason: c.reason, risks: [...c.risks] }
  })
  if (!Object.values(value.excluded_summary).every(v => integer(v, 0))) invalid()
  return { order_id: value.order_id, order_version: value.order_version, strategy: value.strategy,
    weights: Object.fromEntries(Object.keys(weights).map(k => [k, value.weights[k]])),
    top_candidates: rows.slice(0, value.top_candidates.length), other_eligible_candidates: rows.slice(value.top_candidates.length),
    excluded_summary: Object.fromEntries(Object.entries(value.excluded_summary)) }
}
export function exclusionRows(summary) {
  return Object.entries(summary || {}).map(([code, count], index) => ({ key: index, label: Object.hasOwn(exclusionLabels, code) ? exclusionLabels[code] : '其他资格条件未满足（具体原因尚未识别）', count }))
}
export function emptyDispatchText(summary) {
  return Object.keys(summary || {}).length ? '覆盖范围内暂无合格候选。请核对以下排除原因；不会放宽资格或容量约束。' :
    '该工单位置当前没有可推荐的覆盖范围内维修人员，请核对人员服务区域或联系有权限的管理员。'
}
export function validateAssignReason(reason, required = false) {
  if (typeof reason !== 'string' || [...reason].length > 500 ||
      /[\uD800-\uDBFF](?![\uDC00-\uDFFF])|(?<![\uD800-\uDBFF])[\uDC00-\uDFFF]|[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]/.test(reason) ||
      (required && !reason.replace(/[\p{White_Space}\uFEFF]/gu, ''))) throw new Error('人工调整原因最多500个Unicode字符，不允许非法控制字符；选择非前三名时不能仅填写空白。')
  return reason
}
export function assignmentPayload(snapshot, workerId, reason = '') {
  const checked = dispatchSnapshot(snapshot, snapshot?.order_id)
  const candidate = [...checked.top_candidates, ...checked.other_eligible_candidates].find(c => c.worker_id === workerId)
  if (!candidate) throw new Error('请选择当前服务器返回的合格候选，不得自行添加人员。')
  validateAssignReason(reason, candidate.rank > 3)
  return { worker_id: candidate.worker_id, expected_version: checked.order_version, ...(reason ? { reason } : {}) }
}
export function dispatchFailure(e) {
  if (e.status === 401) return '登录已失效，请重新登录。'
  if ([403, 404].includes(e.status)) return '无权访问、授权范围已变化或工单不存在；已清除旧候选。'
  if (e.status === 409) return '工单状态、版本或有效派单已变化。旧候选已清除，请重新读取；不会自动重放派单。'
  if (e.status === 422 && e.code === 'WORKER_INELIGIBLE') return `所选人员已不满足实时资格，请重新加载推荐。${Object.hasOwn(exclusionLabels, e.reason) ? `原因：${exclusionLabels[e.reason]}。` : ''}`
  if (e.status === 422) return '字段校验失败，请核对人工原因等输入；服务器可能已重新计算候选排名。'
  return '请求未完成，请核对后端连接后重试读取。'
}
function receipt(value, snapshot, selected) {
  if (!object(value) || value.order_id !== snapshot.order_id || !integer(value.assignment_id) || value.status !== 'PENDING_ACCEPT' ||
      !integer(value.version) || value.version !== snapshot.order_version + 1 || !object(value.worker) ||
      value.worker.id !== selected || !integer(value.worker.id) || typeof value.worker.display_name !== 'string') invalid()
  return { order_id: value.order_id, assignment_id: value.assignment_id, status: value.status, version: value.version,
    worker: { id: value.worker.id, display_name: value.worker.display_name } }
}
// Reactive-friendly local flow; API dependencies also allow deterministic unit
// tests. Generation checks invalidate every late read/write after context loss.
export function dispatchFlow(api, canRead = () => false) {
  let generation = 0
  return {
    phase: 'idle', orderId: null, snapshot: null, selected: null, reason: '', message: '', result: null, verification: null, verifying: false,
    clear() { generation++; this.phase = 'idle'; this.orderId = null; this.snapshot = null; this.selected = null; this.reason = ''; this.message = ''; this.result = null; this.verification = null; this.verifying = false },
    async load(id) {
      if (['submitting', 'unknown_result', 'success'].includes(this.phase) && String(this.orderId) === String(id)) return false
      this.clear(); const current = generation
      try { this.orderId = dispatchId(id) } catch (e) { this.phase = 'error'; this.message = e.message; return false }
      this.phase = 'loading'
      try {
        const data = dispatchSnapshot(await api.candidates(this.orderId), this.orderId)
        if (current !== generation) return false
        this.snapshot = data; this.phase = data.top_candidates.length ? 'ready' : 'empty'; return true
      } catch (e) {
        if (current !== generation) return false
        this.snapshot = null; this.phase = [401, 403, 404].includes(e.status) ? 'forbidden' : e.status === 409 ? 'conflict' : 'error'
        this.message = e.status ? dispatchFailure(e) : e.message; return false
      }
    },
    choose(id) {
      if (this.phase !== 'ready' || !this.snapshot || ![...this.snapshot.top_candidates, ...this.snapshot.other_eligible_candidates].some(c => c.worker_id === id)) return false
      this.selected = id; this.reason = ''; this.message = ''; return true
    },
    cancel() { if (this.phase !== 'submitting') { this.selected = null; this.reason = '' } },
    async verify() {
      if (this.verifying || !this.orderId || !canRead()) return false
      const current = generation, id = this.orderId; this.verifying = true; this.verification = null
      try {
        const d = await api.detail(id)
        if (current !== generation || !canRead()) return false
        if (!d || String(d.id) !== id || !integer(d.id) || !integer(d.version) || typeof d.status !== 'string') invalid()
        this.verification = { id: d.id, status: d.status, version: d.version,
          worker_name: typeof d.current_assignment?.worker_name === 'string' ? d.current_assignment.worker_name : null }
        return true
      } catch (e) { if (current === generation) { this.verification = null; this.message += ' 最新状态未能读取，请人工核实。' } return false }
      finally { if (current === generation) this.verifying = false }
    },
    async send() {
      if (this.phase === 'submitting') return null
      if (this.phase !== 'ready') throw new Error('请读取当前候选后人工确认；未知结果禁止重发。')
      const body = assignmentPayload(this.snapshot, this.selected, this.reason)
      const current = generation, before = this.snapshot, selected = this.selected
      this.phase = 'submitting'; this.message = ''
      try {
        const result = receipt(await api.assign(this.orderId, body), before, selected)
        if (current !== generation) return null
        this.result = result; this.snapshot = null; this.selected = null; this.reason = ''; this.phase = 'success'
        this.message = '服务器已确认派单成功。'; await this.verify(); return result
      } catch (e) {
        if (current !== generation) return null
        if (e.status === 422 && e.code !== 'WORKER_INELIGIBLE') { this.phase = 'ready'; this.message = dispatchFailure(e); return null }
        this.snapshot = null; this.selected = null; this.reason = ''; this.result = null
        if (!e.status || e.status >= 500) {
          this.phase = 'unknown_result'; this.message = '派单结果尚未确认。请求可能已提交；禁止自动重发，请只读核实或联系管理员。'
          await this.verify()
        } else { this.phase = [401, 403, 404].includes(e.status) ? 'forbidden' : e.status === 409 ? 'conflict' : 'error'; this.message = dispatchFailure(e) }
        return null
      }
    },
  }
}
