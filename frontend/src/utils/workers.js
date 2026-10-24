export const availabilityLabels = { AVAILABLE: '可接单', BUSY: '忙碌', OFF_DUTY: '离岗', DISABLED: '不可用' }
const positive = (n) => Number.isSafeInteger(n) && n > 0
export function skillOptions(value) {
  if (!Array.isArray(value)) throw new Error('有效技能选项未成功读取，禁止保存')
  const seen = new Set()
  for (const s of value) {
    if (!positive(s.id) || seen.has(s.id) || typeof s.code !== 'string' || typeof s.name !== 'string' || typeof s.requires_certificate !== 'boolean') throw new Error('技能目录不完整，禁止保存')
    seen.add(s.id)
  }
  return value.map((s) => ({ ...s }))
}
export function workerForm(record, options) {
  skillOptions(options)
  if (!record || !positive(record.id) || typeof record.active !== 'boolean' || typeof record.emergency_duty !== 'boolean' || !Array.isArray(record.skills) || !positive(record.service_area_id) || !Number.isSafeInteger(record.active_tasks) || record.active_tasks < 0) throw new Error('完整真实管理档案未读取，禁止猜测默认值或保存')
  const form = { service_area_id: record.service_area_id, availability: record.availability, max_concurrent_tasks: record.max_concurrent_tasks, active: record.active, emergency_duty: record.emergency_duty, skills: record.skills.map((s) => ({ skill_id: s.skill_id, level: s.level, certified: s.certified })) }
  workerPayload(form, options, 0) // Read may reveal existing overload; do not manufacture a valid capacity.
  return form
}
export function workerPayload(form, options, activeTasks, areas) {
  const choices = skillOptions(options), ids = new Set()
  if (!Number.isSafeInteger(activeTasks) || activeTasks < 0 || !positive(form.service_area_id) || !Object.hasOwn(availabilityLabels, form.availability) || !positive(form.max_concurrent_tasks) || form.max_concurrent_tasks > 100 || form.max_concurrent_tasks < activeTasks || typeof form.active !== 'boolean' || typeof form.emergency_duty !== 'boolean' || !Array.isArray(form.skills)) throw new Error('档案不完整或容量无效（须为1～100整数且不低于当前任务数）')
  if (areas && !areas.some((a) => a.id === form.service_area_id && ['CAMPUS', 'BUILDING'].includes(a.location_type))) throw new Error(`原服务区域 #${form.service_area_id} 不存在、已停用或类型不符；不得静默替换`)
  const skills = form.skills.map((s) => {
    if (!choices.some((o) => o.id === s.skill_id) || ids.has(s.skill_id) || !positive(s.level) || s.level > 5 || typeof s.certified !== 'boolean') throw new Error('请选择有效且不重复的技能、1～5熟练度及真实持证状态')
    ids.add(s.skill_id)
    return { skill_id: s.skill_id, level: s.level, certified: s.certified }
  })
  return { service_area_id: form.service_area_id, availability: form.availability, max_concurrent_tasks: form.max_concurrent_tasks, active: form.active, emergency_duty: form.emergency_duty, skills }
}
export async function serviceAreas(fetchChildren) {
  const roots = (await fetchChildren()).items, result = []
  for (const root of roots) {
    if (root.location_type !== 'CAMPUS') continue
    result.push({ ...root, label: root.name })
    const buildings = (await fetchChildren(root.id)).items
    for (const b of buildings) if (b.location_type === 'BUILDING') result.push({ ...b, label: `${root.name} / ${b.name}` })
  }
  return result
}
// No worker-detail route exists: re-read the authorized, unfiltered management list.
export async function completeWorker(list, id) {
  for (let page = 1; page <= 50; page++) {
    const data = await list({ page, page_size: 100 })
    const options = skillOptions(data.skill_options)
    if (!Array.isArray(data.items) || !Number.isSafeInteger(data.total) || data.total < 0) throw new Error('管理列表格式无效')
    const record = data.items.find((w) => w.id === id)
    if (record) return { record, options, form: workerForm(record, options) }
    if (page * 100 >= data.total) break
  }
  throw Object.assign(new Error('该人员已不在可读取的管理范围；请核查权限或刷新列表'), { status: 404 })
}
export function workerEditor(api, id) {
  return {
    phase: 'loading', snapshot: null, form: null, verification: null,
    async load() {
      this.phase = 'loading'; this.snapshot = null; this.form = null; this.verification = null
      try { this.snapshot = await completeWorker(api.list, id); this.form = this.snapshot.form; this.phase = 'ready' }
      catch (e) { this.phase = 'blocked'; throw e }
    },
    async save(areas) {
      if (this.phase === 'saving') return null
      if (this.phase !== 'ready' || !this.snapshot || !areas) throw new Error('尚未读取全部管理字段、技能与服务区域，禁止保存')
      const body = workerPayload(this.form, this.snapshot.options, this.snapshot.record.active_tasks, areas)
      this.phase = 'saving'
      try {
        await api.update(id, body) // Deliberately ignore the partial API-16 response.
        this.phase = 'verifying'
        const current = await completeWorker(api.list, id)
        const load = await api.workload(id)
        this.snapshot = current; this.form = current.form; this.verification = { current, load }; this.phase = 'confirmed'
        return this.verification
      } catch (e) {
        if (e.status === 422 && this.phase === 'saving') { this.phase = 'ready'; throw e }
        this.phase = 'unknown'
        let permissionLost = [401, 403, 404].includes(e.status)
        try { this.verification = { current: await completeWorker(api.list, id), load: await api.workload(id) } }
        catch (readError) { this.verification = null; permissionLost ||= [401, 403, 404].includes(readError.status) }
        // A matching later snapshot is not proof of which concurrent PUT wrote it.
        if (permissionLost) { this.phase = 'blocked'; this.form = null; this.snapshot = null; this.verification = null }
        throw e
      }
    },
  }
}
