<script setup>
import { ref, reactive, watch, onBeforeUnmount, computed } from 'vue'
import { ElMessageBox } from 'element-plus'
import { orderDetail, reviewOrder } from '../api/orders.js'
import { reviewFlow, reviewPayload, adminError, managementAbility } from '../utils/adminOrders.js'
import { authState } from '../stores/auth.js'
import { urgencies, statuses } from '../utils/orders.js'
import CatalogPicker from './CatalogPicker.vue'
const props = defineProps({ orderId: Number })
const emit = defineEmits(['close', 'changed'])
const detail = ref(null), flow = ref(null), error = ref(''), loading = ref(false), confirming = ref(false)
const form = reactive({ action: 'APPROVE', category_id: undefined, approved_urgency: '', reason: '', internal_note: '' })
const permitted = computed(() => managementAbility(authState.user, 'REVIEW'))
const locked = computed(() => loading.value || confirming.value || flow.value?.busy)
let sequence = 0
watch(() => props.orderId, async (id) => {
  const request = ++sequence; detail.value = null; flow.value = null; error.value = ''; loading.value = false
  Object.assign(form, { action: 'APPROVE', category_id: undefined, approved_urgency: '', reason: '', internal_note: '' })
  if (!id) return
  loading.value = true
  try {
    const data = await orderDetail(id)
    if (request !== sequence) return
    detail.value = data
    form.category_id = data.category?.id; form.approved_urgency = data.approved_urgency || data.requested_urgency
    flow.value = reactive(reviewFlow({ review: reviewOrder, detail: orderDetail }, data))
  } catch (e) { if (request === sequence) error.value = adminError(e) }
  finally { if (request === sequence) loading.value = false }
}, { immediate: true })
async function send() {
  if (locked.value || !flow.value || flow.value.blocked || !permitted.value) return
  const current = sequence, operation = flow.value
  confirming.value = true; error.value = ''
  try {
    reviewPayload(detail.value, form)
    await ElMessageBox.confirm(`确认${{ APPROVE: '通过审核（进入待派单）', RETURN: '退回补充（原因对报修人公开）', REJECT: '拒绝并终止工单（不可恢复）' }[form.action]}？工单 ${detail.value.order_no || detail.value.id}，版本 ${detail.value.version}。`, '审核确认', { type: 'warning', confirmButtonText: '确认执行', cancelButtonText: '返回修改' })
    if (current !== sequence) return
    const result = await operation.send({ ...form })
    if (current === sequence && result) { emit('changed'); emit('close') }
  } catch (e) {
    if (current !== sequence || e === 'cancel' || e === 'close') return
    error.value = adminError(e)
    if (operation.blocked) { error.value += ' 请求不会自动重放；请核对查询结果，关闭后重新读取。'; emit('changed') }
    if (operation.denied || [401, 403, 404].includes(e.status)) { detail.value = null; form.reason = ''; form.internal_note = '' }
  } finally { if (current === sequence) confirming.value = false }
}
onBeforeUnmount(() => { sequence++ })
</script>
<template>
  <el-drawer :model-value="!!orderId" title="工单审核" size="min(95vw, 620px)" :close-on-click-modal="!locked" :close-on-press-escape="!locked" :show-close="!locked" @update:model-value="!$event && !locked && emit('close')">
    <p v-if="loading" role="status">正在读取真实工单及版本…</p>
    <el-alert v-if="error" :title="error" type="error" :closable="false" />
    <el-alert v-if="!permitted" title="未获得审核动作权限" type="warning" :closable="false" />
    <template v-if="detail">
      <p>{{ detail.order_no || `#${detail.id}` }} · {{ detail.title }} · v{{ detail.version }}</p>
      <el-alert v-if="detail.status !== 'PENDING_REVIEW'" title="该工单已不是待审核状态，不能执行审核" type="warning" :closable="false" />
      <el-form label-position="top" :disabled="locked || flow?.blocked || !permitted || detail.status !== 'PENDING_REVIEW'" @submit.prevent="send">
        <el-form-item label="审核动作"><el-select v-model="form.action"><el-option label="通过" value="APPROVE" /><el-option label="退回补充" value="RETURN" /><el-option label="拒绝并终止工单" value="REJECT" /></el-select></el-form-item>
        <template v-if="form.action === 'APPROVE'">
          <el-form-item label="确认故障分类（真实目录）" required><CatalogPicker v-model="form.category_id" kind="categories" :disabled="locked || flow?.blocked" /></el-form-item>
          <el-form-item label="确认紧急程度" required><el-select v-model="form.approved_urgency"><el-option v-for="(label, value) in urgencies" :key="value" :label="label" :value="value" /></el-select></el-form-item>
        </template>
        <el-form-item v-else label="公开原因（报修人可见，最多500个Unicode字符）" required><el-input v-model="form.reason" type="textarea" :rows="4" /></el-form-item>
        <el-form-item label="内部备注（可选，仅授权管理人员可读，最多2000个Unicode字符）"><el-input v-model="form.internal_note" type="textarea" :rows="4" /></el-form-item>
        <el-button v-if="permitted && detail.status === 'PENDING_REVIEW'" type="primary" :loading="locked" :disabled="locked || !flow || flow.blocked" @click="send">核对并提交审核</el-button>
      </el-form>
    </template>
    <p v-if="flow?.blocked">最新查询：{{ flow.verification ? `${statuses[flow.verification.status] || flow.verification.status} · v${flow.verification.version}` : '未能确认，请人工核查后重新打开；不要盲目重试写入。' }}</p>
  </el-drawer>
</template>
