<script setup>
import { ref, watch, onBeforeUnmount } from 'vue'
import { useRoute } from 'vue-router'
import { orderDetail, orderTimeline } from '../api/orders.js'
import { dateText, urgencies, orderError } from '../utils/orders.js'
import OrderStatusTag from '../components/OrderStatusTag.vue'
import OrderTimeline from '../components/OrderTimeline.vue'
const route = useRoute(), detail = ref(null), items = ref([]), busy = ref(false), error = ref(''), timelineError = ref(''), page = ref(1), total = ref(0), timelineBusy = ref(false)
let sequence = 0, timelineSequence = 0
async function loadTimeline(parentSequence = sequence) {
  const request = ++timelineSequence; timelineBusy.value = true; timelineError.value = ''; items.value = []
  try {
    const data = await orderTimeline(route.params.id, { page: page.value, page_size: 20 })
    if (parentSequence !== sequence || request !== timelineSequence) return
    items.value = data.items; total.value = data.total
  } catch (e) {
    if (parentSequence === sequence && request === timelineSequence) {
      if ([401, 403, 404].includes(e.status)) { detail.value = null; error.value = orderError(e) }
      else timelineError.value = orderError(e)
    }
  } finally { if (parentSequence === sequence && request === timelineSequence) timelineBusy.value = false }
}
async function load() {
  const request = ++sequence; ++timelineSequence; busy.value = true; detail.value = null; items.value = []; error.value = ''; timelineError.value = ''; page.value = 1; total.value = 0
  try {
    const data = await orderDetail(route.params.id)
    if (request !== sequence) return
    detail.value = data; await loadTimeline(request)
  } catch (e) { if (request === sequence) error.value = orderError(e) }
  finally { if (request === sequence) busy.value = false }
}
watch(() => route.params.id, load, { immediate: true })
onBeforeUnmount(() => { sequence++; timelineSequence++ })
</script>
<template>
  <main class="order-page">
    <RouterLink to="/my-orders">返回我提交的报修</RouterLink><h1>工单详情</h1>
    <p v-if="busy" role="status">正在加载工单…</p>
    <el-result v-else-if="error" title="无法加载工单" :sub-title="error"><template #extra><el-button @click="load">重试</el-button></template></el-result>
    <template v-else-if="detail">
      <h2>{{ detail.order_no || `草稿 #${detail.id}` }} · {{ detail.title }}</h2><OrderStatusTag :status="detail.status" />
      <el-descriptions :column="1" border class="summary">
        <el-descriptions-item label="版本">{{ detail.version ?? '—' }}</el-descriptions-item>
        <el-descriptions-item label="分类">{{ detail.category?.name || '—' }}</el-descriptions-item>
        <el-descriptions-item label="位置">{{ detail.location?.name || '—' }}</el-descriptions-item>
        <el-descriptions-item label="申报紧急度">{{ urgencies[detail.requested_urgency] || '—' }}</el-descriptions-item>
        <el-descriptions-item label="确认紧急度">{{ urgencies[detail.approved_urgency] || '尚未确认' }}</el-descriptions-item>
        <el-descriptions-item v-if="typeof detail.description === 'string'" label="描述"><span class="note">{{ detail.description }}</span></el-descriptions-item>
        <el-descriptions-item v-if="typeof detail.contact_name === 'string'" label="联系人">{{ detail.contact_name }}</el-descriptions-item>
        <el-descriptions-item v-if="typeof detail.contact_phone === 'string'" label="联系电话">{{ detail.contact_phone }}</el-descriptions-item>
        <el-descriptions-item v-if="Object.hasOwn(detail, 'expected_visit_at')" label="期望上门时间">{{ dateText(detail.expected_visit_at) }}</el-descriptions-item>
        <el-descriptions-item v-if="detail.current_assignment" label="当前负责人">{{ detail.current_assignment.worker_name || '—' }}</el-descriptions-item>
        <el-descriptions-item label="创建时间">{{ dateText(detail.created_at) }}</el-descriptions-item>
        <el-descriptions-item label="提交时间">{{ dateText(detail.submitted_at) }}</el-descriptions-item>
      </el-descriptions>
      <p>当前详情为只读。编辑、取消、验收、评价与图片功能尚未开放。</p>
      <h2>公开时间线</h2>
      <el-alert v-if="timelineError" :title="timelineError" type="error" :closable="false" />
      <el-button v-if="timelineError" @click="loadTimeline()">重新加载时间线</el-button>
      <p v-if="timelineBusy">正在加载时间线…</p><OrderTimeline v-else-if="!timelineError" :items="items" />
      <el-pagination v-if="total" v-model:current-page="page" :page-size="20" :total="total" :disabled="timelineBusy" layout="prev, pager, next, total" @current-change="loadTimeline()" />
    </template>
  </main>
</template>
<style scoped>.order-page { padding: 28px; min-width: 0; } .summary { margin: 20px 0; } .note { white-space: pre-wrap; overflow-wrap: anywhere; } h2 { overflow-wrap: anywhere; }</style>
