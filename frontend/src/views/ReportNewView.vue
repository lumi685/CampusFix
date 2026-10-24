<script setup>
import { reactive, ref, computed, h, watch, onMounted, onBeforeUnmount } from 'vue'
import { useRouter, onBeforeRouteLeave } from 'vue-router'
import { ElMessageBox } from 'element-plus'
import CatalogPicker from '../components/CatalogPicker.vue'
import { createOrder, submitOrder, orderDetail } from '../api/orders.js'
import { urgencies, reportPayload, orderError } from '../utils/orders.js'
import { reportSubmission } from '../utils/reportSubmission.js'
import { authState } from '../stores/auth.js'
import { credentialSnapshot, onCredentialChanged } from '../api/credentials.js'
import { reportDraftHandoff, prefillText } from '../utils/reportDraftHandoff.js'
const router = useRouter(), error = ref(''), completed = ref(false)
const aiSuggestion = ref(reportDraftHandoff.consume()), confirming = ref(false), sessionEnded = ref(false)
const sessionRevision = credentialSnapshot().revision
let active = true
const allowed = computed(() => !sessionEnded.value && authState.status === 'authenticated' && ['reporter', 'worker'].includes(authState.user?.role))
const form = reactive({ category_id: undefined, location_id: undefined, title: '', description: '', requested_urgency: 'NORMAL', contact_name: '', contact_phone: '', expected_visit_at: null })
if (aiSuggestion.value) { form.title = prefillText(aiSuggestion.value.title); form.description = prefillText(aiSuggestion.value.description) }
function inCurrentSession(request) {
  return (...args) => {
    if (!active || !allowed.value || sessionRevision !== credentialSnapshot().revision) throw Object.assign(new Error('登录状态已变化，请重新登录后核查原工单。'), { status: 401 })
    return request(...args)
  }
}
const flow = reactive(reportSubmission({ create: inCurrentSession(createOrder), submit: inCurrentSession(submitOrder), detail: inCurrentSession(orderDetail) }))
function clearSensitive() {
  aiSuggestion.value = null; error.value = ''; sessionEnded.value = true
  for (const key of ['title', 'description', 'contact_name', 'contact_phone']) form[key] = ''
  form.category_id = undefined; form.location_id = undefined; form.expected_visit_at = null
}
const unsubscribe = onCredentialChanged(clearSensitive)
watch(() => authState.status, (status) => { if (!['authenticated', 'checking'].includes(status)) clearSensitive() }, { flush: 'sync' })
onMounted(() => window.addEventListener('pagehide', clearSensitive))
onBeforeUnmount(() => { active = false; unsubscribe(); window.removeEventListener('pagehide', clearSensitive); clearSensitive() })
async function send() {
  if (flow.busy || confirming.value || flow.uncertain || !allowed.value || sessionRevision !== credentialSnapshot().revision) return
  error.value = ''
  try {
    const payload = flow.draft ? undefined : reportPayload(form)
    if (aiSuggestion.value && !flow.draft) {
      confirming.value = true
      const labels = { category_id: '真实分类 ID', location_id: '真实位置 ID', title: '标题', description: '描述', requested_urgency: '紧急程度', contact_name: '联系人', contact_phone: '联系电话', expected_visit_at: '期望上门时间（UTC）' }
      const content = h('div', { style: 'max-height:60vh;overflow:auto;overflow-wrap:anywhere;white-space:pre-wrap' }, [
        h('p', '请再次核对故障分类、位置、标题、描述、紧急程度和联系方式。确认后系统将创建工单并提交审核。AI 不会代替你执行本操作。'),
        ...Object.entries(payload).map(([key, value]) => h('p', `${labels[key]}：${key === 'requested_urgency' ? urgencies[value] : value}`)),
      ])
      try { await ElMessageBox.confirm(content, '确认创建并提交报修', { type: 'warning' }) }
      catch (e) { if (e === 'cancel' || e === 'close') return; throw e }
      if (!active || !allowed.value || sessionRevision !== credentialSnapshot().revision || JSON.stringify(payload) !== JSON.stringify(reportPayload(form))) return
    }
    confirming.value = false
    const result = await flow.send(payload)
    if (result && active && allowed.value && sessionRevision === credentialSnapshot().revision) { completed.value = true; await router.push(`/orders/${result.id}`) }
  } catch (e) { if (active && allowed.value) error.value = orderError(e) }
  finally { confirming.value = false }
}
onBeforeRouteLeave(async () => {
  if (flow.busy || confirming.value) return false
  if (!completed.value && (flow.draft || flow.uncertain)) {
    try { await ElMessageBox.confirm('离开后当前页面的草稿重试信息将丢失。可在本人报修列表打开原工单详情，继续编辑或提交。确定离开？', '报修尚未确认完成', { type: 'warning' }); return true } catch { return false }
  }
})
</script>
<template>
  <main class="order-page">
    <h1>提交报修</h1><p>仅为当前登录账户本人报修。图片上传为后续功能，本页不接收图片。</p>
    <el-alert v-if="!allowed" title="当前会话不可用，请重新登录；旧建议不会恢复。" type="warning" :closable="false" />
    <section v-if="aiSuggestion && allowed" class="ai-context" aria-label="AI 原始文字提示">
      <el-alert title="已带入 AI 建议，尚未创建工单。请核对正式表单，并自主选择真实分类和位置。" type="info" :closable="false" />
      <p>以下为交接时的 AI 文字提示，不是正式目录选项，也不是当前表单的提交内容。</p>
      <p>分类线索：{{ aiSuggestion.category_hint || '未提供' }}</p>
      <p>位置线索：{{ aiSuggestion.location_hint || '未提供' }}</p>
      <p>待补充信息：{{ aiSuggestion.missing_information.join('；') || '未提供' }}</p>
      <p>AI 安全提醒：{{ aiSuggestion.safety_note || '未提供' }}</p>
      <el-alert title="冒烟、电火花、漏电或其他危险情况，请停止操作、远离危险源并联系学校应急值班渠道及专业人员；不要带电检修或拆机。" type="warning" :closable="false" />
    </section>
    <template v-if="allowed">
    <el-alert v-if="error" :title="error" type="error" :closable="false" show-icon />
    <el-alert v-if="flow.draft" :title="`已保存草稿 #${flow.draft.id}，版本 ${flow.draft.version}；尚未确认正式提交。重试只提交同一草稿。`" type="warning" :closable="false" />
    <el-alert v-if="flow.uncertain" title="创建结果不确定，已阻止重复创建。请到“我提交的报修”核查，不要直接重新填报同一问题。" type="warning" :closable="false" />
    <el-form label-position="top" :disabled="flow.busy || confirming || !!flow.draft || flow.uncertain" @submit.prevent="send">
      <el-form-item label="故障分类（必填，须从真实目录选择）" required><CatalogPicker v-model="form.category_id" kind="categories" :disabled="flow.busy || confirming || !!flow.draft || flow.uncertain" /></el-form-item>
      <el-form-item label="故障位置（必填，须选择足够精确的真实位置）" required><CatalogPicker v-model="form.location_id" kind="locations" :disabled="flow.busy || confirming || !!flow.draft || flow.uncertain" /></el-form-item>
      <el-form-item label="标题（1～120 字）" required><el-input v-model="form.title" /></el-form-item>
      <el-form-item label="故障描述（1～2000 字）" required><el-input v-model="form.description" type="textarea" :rows="4" /></el-form-item>
      <el-form-item label="申报紧急程度" required><el-select v-model="form.requested_urgency"><el-option v-for="(label, value) in urgencies" :key="value" :label="label" :value="value" /></el-select></el-form-item>
      <el-alert v-if="form.requested_urgency === 'URGENT'" title="先联系学校应急值班渠道并确保现场安全，本系统不替代应急处置。" type="warning" :closable="false" />
      <el-form-item label="联系人（1～80 字）" required><el-input v-model="form.contact_name" autocomplete="name" /></el-form-item>
      <el-form-item label="联系电话（1～32 字）" required><el-input v-model="form.contact_phone" autocomplete="tel" /></el-form-item>
      <el-form-item label="期望上门时间（可选，本地时间）"><el-date-picker v-model="form.expected_visit_at" type="datetime" placeholder="选择日期时间" /></el-form-item>
    </el-form>
    <el-button type="primary" :loading="flow.busy || confirming" :disabled="flow.busy || confirming || flow.uncertain" @click="send">{{ flow.draft ? '重试提交同一草稿' : '提交报修' }}</el-button>
    <el-button :disabled="flow.busy || confirming" @click="router.push('/my-orders')">查看我提交的报修</el-button>
    </template>
  </main>
</template>
<style scoped>.order-page { padding: 28px; max-width: 900px; width: 100%; box-sizing: border-box; } .el-alert { margin: 14px 0; } h1 { margin-top: 0; } p { color: #606266; } .ai-context p { white-space: pre-wrap; overflow-wrap: anywhere; } @media (max-width:480px) { .order-page { padding:16px; } }</style>
