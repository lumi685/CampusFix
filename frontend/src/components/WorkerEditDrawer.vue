<script setup>
import { ref, reactive, watch, computed, onBeforeUnmount } from 'vue'
import { ElMessageBox } from 'element-plus'
import { workers, updateWorker, workerWorkload } from '../api/workers.js'
import { locations } from '../api/orders.js'
import { workerEditor, workerPayload, availabilityLabels, serviceAreas } from '../utils/workers.js'
import { adminError } from '../utils/adminOrders.js'
const props = defineProps({ workerId: Number })
const emit = defineEmits(['close', 'saved'])
const editor = ref(null), areas = ref(null), error = ref(''), checking = ref(false)
const inFlight = computed(() => checking.value || ['loading','saving','verifying'].includes(editor.value?.phase))
const ready = computed(() => editor.value?.phase === 'ready' && !!areas.value && !!editor.value.form && areas.value.some((a) => a.id === editor.value.form.service_area_id))
let sequence = 0
async function load() {
  const request = ++sequence; areas.value = null; editor.value = null; error.value = ''
  if (!props.workerId) return
  const operation = reactive(workerEditor({ list: workers, update: updateWorker, workload: workerWorkload }, props.workerId)); editor.value = operation
  try {
    const [, directory] = await Promise.all([operation.load(), serviceAreas(locations)])
    if (request !== sequence) return
    areas.value = directory
    const original = operation.form.service_area_id
    if (!directory.some((a) => a.id === original)) error.value = `原服务区域 #${original} 已停用或无法从真实目录解析；禁止保存，不会静默替换。`
  } catch (e) { if (request === sequence) { error.value = adminError(e); operation.phase = 'blocked' } }
}
watch(() => props.workerId, load, { immediate: true })
async function save() {
  if (!ready.value || inFlight.value) return
  const request = sequence, operation = editor.value; checking.value = true; error.value = ''
  try {
    workerPayload(operation.form, operation.snapshot.options, operation.snapshot.record.active_tasks, areas.value)
    await ElMessageBox.confirm('确认更新该人员的服务区域、启用状态、可用性、紧急值班、容量和完整技能集合？移除技能会停用关联，既有任务不会自动结束。人员接口没有版本锁，并发更新可能最后写入覆盖。', '人员档案更新确认', { type: 'warning', confirmButtonText: '确认保存', cancelButtonText: '返回修改' })
    if (request !== sequence) return
    const verified = await operation.save(areas.value)
    if (request === sequence && verified) emit('saved', verified)
  } catch (e) {
    if (request !== sequence || e === 'cancel' || e === 'close') return
    error.value = adminError(e)
    if (['unknown','blocked'].includes(operation.phase)) error.value += ' 不会重放PUT；请核对服务器档案后重新打开。'
  } finally { if (request === sequence) checking.value = false }
}
function addSkill() { editor.value.form.skills.push({ skill_id: undefined, level: 1, certified: false }) }
onBeforeUnmount(() => { sequence++ })
</script>
<template>
  <el-drawer :model-value="!!workerId" title="维修人员档案" size="min(96vw, 720px)" :show-close="!inFlight" :close-on-click-modal="!inFlight" :close-on-press-escape="!inFlight" @update:model-value="!$event && !inFlight && emit('close')">
    <p v-if="editor?.phase === 'loading'" role="status">正在重新读取完整档案、有效技能与真实服务区域…</p>
    <el-alert v-if="error" :title="error" type="error" :closable="false" />
    <el-button v-if="editor?.phase === 'blocked' && !inFlight" @click="load">重新读取（不提交）</el-button>
    <template v-if="editor?.form">
      <p>{{ editor.snapshot.record.worker_no }} · {{ editor.snapshot.record.display_name }}；原服务区域：{{ areas?.find((a) => a.id === editor.snapshot.record.service_area_id)?.label || `#${editor.snapshot.record.service_area_id}（无法确认有效性）` }}</p>
      <p>当前有效任务：{{ editor.snapshot.record.active_tasks }}。档案启用不同于账号状态，也不同于 availability。</p>
      <el-form label-position="top" :disabled="!ready || inFlight" @submit.prevent="save">
        <el-form-item label="主服务区域（仅校区/楼宇）" required><el-select v-model="editor.form.service_area_id" filterable><el-option v-for="area in areas || []" :key="area.id" :label="area.label" :value="area.id" /></el-select></el-form-item>
        <el-form-item label="可用性" required><el-select v-model="editor.form.availability"><el-option v-for="(label, value) in availabilityLabels" :key="value" :label="label" :value="value" /></el-select></el-form-item>
        <el-form-item label="最大并行任务数（1～100，不低于有效任务数）" required><el-input-number v-model="editor.form.max_concurrent_tasks" :min="1" :max="100" :step="1" step-strictly /></el-form-item>
        <el-form-item label="人员档案启用 active"><el-switch v-model="editor.form.active" active-text="启用" inactive-text="停用" /></el-form-item>
        <el-form-item label="紧急值班 emergency_duty"><el-switch v-model="editor.form.emergency_duty" active-text="是" inactive-text="否" /></el-form-item>
        <h3>有效技能</h3><p>要求证书只是目录说明，不会自动授予持证状态。空集合代表移除全部有效技能。</p>
        <div v-for="(skill, index) in editor.form.skills" :key="index" class="skill-row">
          <el-select v-model="skill.skill_id" placeholder="选择有效技能"><el-option v-for="option in editor.snapshot.options" :key="option.id" :label="`${option.name}${option.requires_certificate ? '（要求证书）' : ''}`" :value="option.id" :disabled="editor.form.skills.some((s, n) => n !== index && s.skill_id === option.id)" /></el-select>
          <el-input-number v-model="skill.level" :min="1" :max="5" step-strictly aria-label="技能等级" />
          <el-switch v-model="skill.certified" active-text="持证" inactive-text="无证" />
          <el-button @click="editor.form.skills.splice(index, 1)">移除</el-button>
        </div>
        <el-button @click="addSkill">添加技能</el-button>
        <el-button type="primary" :disabled="!ready || inFlight" :loading="inFlight" @click="save">核对并保存</el-button>
      </el-form>
    </template>
    <el-alert v-if="editor?.phase === 'confirmed'" title="保存成功，已重新读取完整 API-14 档案及 API-15 负载；不是使用PUT局部响应回填。" type="success" :closable="false" />
    <section v-if="editor?.verification">
      <h3>保存后服务器核对</h3>
      <p>active={{ editor.verification.current.record.active }}；emergency_duty={{ editor.verification.current.record.emergency_duty }}；availability={{ editor.verification.current.record.availability }}</p>
      <p>服务器主服务区域：{{ areas?.find((a) => a.id === editor.verification.current.record.service_area_id)?.label || `#${editor.verification.current.record.service_area_id}` }}；最大并行任务：{{ editor.verification.current.record.max_concurrent_tasks }}</p>
      <ul><li v-for="skill in editor.verification.current.record.skills" :key="skill.skill_id">{{ editor.verification.current.options.find((o) => o.id === skill.skill_id)?.name || `技能 #${skill.skill_id}` }} Lv{{ skill.level }} · {{ skill.certified ? '持证' : '无证' }}</li></ul>
      <p>负载 {{ editor.verification.load.active_tasks }} / {{ editor.verification.load.max_concurrent_tasks }}，剩余 {{ editor.verification.load.remaining_capacity }}</p>
      <p v-if="editor.phase !== 'confirmed'">这是事后读取结果，不证明本次写入一定成功；不得自动重放。</p>
      <el-button v-if="!inFlight" @click="load">读取最新档案后继续编辑</el-button>
    </section>
  </el-drawer>
</template>
<style scoped>.skill-row { display: flex; flex-wrap: wrap; align-items: center; gap: 12px; margin-bottom: 16px; } .skill-row>.el-select { width: 230px; } h3 { margin-top: 24px; }</style>
