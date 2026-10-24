<script setup>
import { reactive, ref } from 'vue'
import { useRouter, onBeforeRouteLeave } from 'vue-router'
import { ElMessageBox } from 'element-plus'
import CatalogPicker from '../components/CatalogPicker.vue'
import { createOrder, submitOrder, orderDetail } from '../api/orders.js'
import { urgencies, reportPayload, orderError } from '../utils/orders.js'
import { reportSubmission } from '../utils/reportSubmission.js'
const router = useRouter(), error = ref(''), completed = ref(false)
const form = reactive({ category_id: undefined, location_id: undefined, title: '', description: '', requested_urgency: 'NORMAL', contact_name: '', contact_phone: '', expected_visit_at: null })
const flow = reactive(reportSubmission({ create: createOrder, submit: submitOrder, detail: orderDetail }))
async function send() {
  if (flow.busy) return
  error.value = ''
  try {
    const payload = flow.draft ? undefined : reportPayload(form)
    const result = await flow.send(payload)
    if (result) { completed.value = true; await router.push(`/orders/${result.id}`) }
  } catch (e) { error.value = orderError(e) }
}
onBeforeRouteLeave(async () => {
  if (flow.busy) return false
  if (!completed.value && (flow.draft || flow.uncertain)) {
    try { await ElMessageBox.confirm('离开后当前页面的草稿重试信息将丢失。可在本人报修列表打开原工单详情，继续编辑或提交。确定离开？', '报修尚未确认完成', { type: 'warning' }); return true } catch { return false }
  }
})
</script>
<template>
  <main class="order-page">
    <h1>提交报修</h1><p>仅为当前登录账户本人报修。图片上传为后续功能，本页不接收图片。</p>
    <el-alert v-if="error" :title="error" type="error" :closable="false" show-icon />
    <el-alert v-if="flow.draft" :title="`已保存草稿 #${flow.draft.id}，版本 ${flow.draft.version}；尚未确认正式提交。重试只提交同一草稿。`" type="warning" :closable="false" />
    <el-alert v-if="flow.uncertain" title="创建结果不确定，已阻止重复创建。请到“我提交的报修”核查，不要直接重新填报同一问题。" type="warning" :closable="false" />
    <el-form label-position="top" :disabled="flow.busy || !!flow.draft || flow.uncertain" @submit.prevent="send">
      <el-form-item label="故障分类（必填）" required><CatalogPicker v-model="form.category_id" kind="categories" :disabled="flow.busy || !!flow.draft || flow.uncertain" /></el-form-item>
      <el-form-item label="故障位置（必填）" required><CatalogPicker v-model="form.location_id" kind="locations" :disabled="flow.busy || !!flow.draft || flow.uncertain" /></el-form-item>
      <el-form-item label="标题（1～120 字）" required><el-input v-model="form.title" /></el-form-item>
      <el-form-item label="故障描述（1～2000 字）" required><el-input v-model="form.description" type="textarea" :rows="4" /></el-form-item>
      <el-form-item label="申报紧急程度" required><el-select v-model="form.requested_urgency"><el-option v-for="(label, value) in urgencies" :key="value" :label="label" :value="value" /></el-select></el-form-item>
      <el-alert v-if="form.requested_urgency === 'URGENT'" title="先联系学校应急值班渠道并确保现场安全，本系统不替代应急处置。" type="warning" :closable="false" />
      <el-form-item label="联系人（1～80 字）" required><el-input v-model="form.contact_name" autocomplete="name" /></el-form-item>
      <el-form-item label="联系电话（1～32 字）" required><el-input v-model="form.contact_phone" autocomplete="tel" /></el-form-item>
      <el-form-item label="期望上门时间（可选，本地时间）"><el-date-picker v-model="form.expected_visit_at" type="datetime" placeholder="选择日期时间" /></el-form-item>
    </el-form>
    <el-button type="primary" :loading="flow.busy" :disabled="flow.busy || flow.uncertain" @click="send">{{ flow.draft ? '重试提交同一草稿' : '提交报修' }}</el-button>
    <el-button :disabled="flow.busy" @click="router.push('/my-orders')">查看我提交的报修</el-button>
  </main>
</template>
<style scoped>.order-page { padding: 28px; max-width: 900px; width: 100%; box-sizing: border-box; } .el-alert { margin: 14px 0; } h1 { margin-top: 0; } p { color: #606266; }</style>
