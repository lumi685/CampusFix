<script setup>
import { reactive, ref, computed } from 'vue'
import { ElMessageBox } from 'element-plus'
import CatalogPicker from './CatalogPicker.vue'
import { editOrder, submitOrder, orderDetail } from '../api/orders.js'
import { editForm, editPayload, supplementFlow } from '../utils/orderSupplement.js'
import { urgencies, orderError } from '../utils/orders.js'
const props = defineProps({ detail: { type: Object, required: true } })
const emit = defineEmits(['saved', 'complete', 'refresh'])
const form = reactive(editForm(props.detail)), flow = reactive(supplementFlow({ edit: editOrder, submit: submitOrder, detail: orderDetail }, props.detail))
const error = ref(''), notice = ref('')
const confirming = ref(false), original = JSON.stringify(editPayload(form, 1))
const dirty = computed(() => { try { return JSON.stringify(editPayload(form, 1)) !== original } catch { return true } })
async function save(andSubmit = false) {
  if (flow.busy) return
  error.value = ''; notice.value = ''
  try {
    const result = await flow.save(form)
    if (!result) return
    emit('saved', result); notice.value = '补充内容已保存，状态未改变。'
    if (andSubmit) await send()
  } catch (e) {
    error.value = orderError(e)
    if (flow.blocked) notice.value = '保存结果或版本尚未确认。请刷新核对原工单，确认后再编辑；不会自动重放 PUT。'
  }
}
async function send() {
  if (flow.busy || confirming.value) return
  confirming.value = true
  error.value = ''
  try {
    await ElMessageBox.confirm('将使用当前已保存内容提交同一工单审核，不会创建新工单。确认继续？', '重新提交审核', { type: 'warning' })
  } catch { return } finally { confirming.value = false }
  try { if (await flow.submit()) emit('complete') }
  catch (e) { error.value = orderError(e); notice.value = '补充内容已保存，但尚未确认重新提交。请核对原工单状态；只重试提交，不重复保存或创建工单。' }
}
async function refresh() {
  try { await ElMessageBox.confirm('刷新会丢弃表单中未保存的修改。请核对服务器数据后再继续。', '刷新最新数据', { type: 'warning' }); emit('refresh') } catch { /* User cancelled. */ }
}
</script>
<template>
  <section class="edit-form">
    <h2>补充信息 / 编辑原工单</h2><p>工单 #{{ flow.id }} · 当前已确认版本 {{ flow.version }}。保存不等于提交审核。</p>
    <el-alert v-if="error" :title="error" type="error" :closable="false" />
    <el-alert v-if="notice" :title="notice" type="warning" :closable="false" />
    <el-form label-position="top" :disabled="flow.busy || flow.saved || flow.blocked" @submit.prevent="save(false)">
      <el-form-item label="故障分类（必填）" required><CatalogPicker v-model="form.category_id" kind="categories" :disabled="flow.busy || flow.saved || flow.blocked" /></el-form-item>
      <el-form-item label="故障位置（必填）" required><CatalogPicker v-model="form.location_id" kind="locations" :disabled="flow.busy || flow.saved || flow.blocked" /></el-form-item>
      <el-form-item label="标题（1～120 字）" required><el-input v-model="form.title" /></el-form-item>
      <el-form-item label="故障描述（1～2000 字）" required><el-input v-model="form.description" type="textarea" :rows="4" /></el-form-item>
      <el-form-item label="申报紧急程度" required><el-select v-model="form.requested_urgency"><el-option v-for="(label, value) in urgencies" :key="value" :label="label" :value="value" /></el-select></el-form-item>
      <el-alert v-if="form.requested_urgency === 'URGENT'" title="先联系学校应急值班渠道并确保现场安全，本系统不替代应急处置。" type="warning" :closable="false" />
      <el-form-item label="联系人（1～80 字）" required><el-input v-model="form.contact_name" /></el-form-item>
      <el-form-item label="联系电话（1～32 字）" required><el-input v-model="form.contact_phone" /></el-form-item>
      <el-form-item label="期望上门时间（可选，本地时间）"><el-date-picker v-model="form.expected_visit_at" type="datetime" clearable /></el-form-item>
    </el-form>
    <el-button :loading="flow.busy" :disabled="flow.busy || flow.saved || flow.blocked" @click="save(false)">保存补充</el-button>
    <el-button type="primary" :disabled="flow.busy || flow.saved || flow.blocked" @click="save(true)">保存并提交审核</el-button>
    <el-button v-if="flow.saved || !dirty" type="primary" :disabled="flow.busy || flow.blocked || confirming" @click="send">仅提交当前已保存内容</el-button>
    <el-button :disabled="flow.busy" @click="refresh">刷新并核对最新数据</el-button>
    <p>图片上传、取消、维修、验收和评价尚未开放。</p>
  </section>
</template>
<style scoped>.edit-form { max-width: 850px; } .el-alert { margin: 12px 0; } .el-button { margin-top: 8px; }</style>
