<script setup>
import { ref, watch, onBeforeUnmount } from 'vue'
import { useRoute } from 'vue-router'
import { orderDetail, orderTimeline, reportedOrders } from '../api/orders.js'
import { dateText, urgencies, orderError } from '../utils/orders.js'
import OrderStatusTag from '../components/OrderStatusTag.vue'
import OrderTimeline from '../components/OrderTimeline.vue'
import OrderEditForm from '../components/OrderEditForm.vue'
import OrderVerifyPanel from '../components/OrderVerifyPanel.vue'
import { authState } from '../stores/auth.js'
import { ownsReportedOrder, latestReturn } from '../utils/orderSupplement.js'
const route = useRoute(), detail = ref(null), items = ref([]), busy = ref(false), error = ref(''), timelineError = ref(''), page = ref(1), total = ref(0), timelineBusy = ref(false)
let sequence = 0, timelineSequence = 0
const owner = ref(false), ownerError = ref(''), returned = ref(null), returnError = ref(''), editing = ref(false)
let reasonSequence = 0
async function loadReason(parentSequence = sequence) {
  const request = ++reasonSequence; returned.value = null; returnError.value = ''
  if (detail.value?.status !== 'PENDING_SUPPLEMENT') return
  try {
    const event = await latestReturn(route.params.id, orderTimeline)
    if (request === reasonSequence && parentSequence === sequence) returned.value = event
  } catch (e) { if (request === reasonSequence && parentSequence === sequence) returnError.value = orderError(e) }
}
async function saved(result) {
  const request = sequence
  detail.value = { ...detail.value, ...result }
  try {
    const current = await orderDetail(result.id)
    if (request !== sequence) return
    detail.value = current
    if (!['DRAFT', 'PENDING_SUPPLEMENT'].includes(current.status)) { owner.value = false; editing.value = false }
  } catch (e) { if (request === sequence) timelineError.value = `内容已保存，但详情刷新失败：${orderError(e)}` }
  if (request === sequence) { loadTimeline(); loadReason() }
}
async function loadTimeline(parentSequence = sequence) {
  const request = ++timelineSequence; timelineBusy.value = true; timelineError.value = ''; items.value = []
  try {
    const data = await orderTimeline(route.params.id, { page: page.value, page_size: 20 })
    if (parentSequence !== sequence || request !== timelineSequence) return
    items.value = data.items; total.value = data.total
  } catch (e) {
    if (parentSequence === sequence && request === timelineSequence) {
      if ([401, 403, 404].includes(e.status)) forgetVerification(orderError(e))
      else timelineError.value = orderError(e)
    }
  } finally { if (parentSequence === sequence && request === timelineSequence) timelineBusy.value = false }
}
async function load() {
  owner.value = false; ownerError.value = ''; returned.value = null; returnError.value = ''; editing.value = false; ++reasonSequence
  const request = ++sequence; ++timelineSequence; busy.value = true; detail.value = null; items.value = []; error.value = ''; timelineError.value = ''; page.value = 1; total.value = 0
  try {
    const data = await orderDetail(route.params.id)
    if (request !== sequence) return
    detail.value = data
    if (['DRAFT', 'PENDING_SUPPLEMENT'].includes(data.status)) {
      try {
        const owned = await ownsReportedOrder(data.id, authState.user?.role, reportedOrders)
        if (request !== sequence) return
        owner.value = owned
      } catch (e) { if (request === sequence) ownerError.value = orderError(e) }
    }
    await Promise.all([loadTimeline(request), loadReason(request)])
  } catch (e) { if (request === sequence) error.value = orderError(e) }
  finally { if (request === sequence) busy.value = false }
}
function forgetVerification(message) { sequence++; timelineSequence++; reasonSequence++;detail.value=null;items.value=[];owner.value=false;editing.value=false;returned.value=null;returnError.value='';ownerError.value='';timelineError.value='';total.value=0;page.value=1;error.value=message;busy.value=false;timelineBusy.value=false }
watch(() => [route.params.id,authState.status,authState.user?.id,authState.user?.role].join('|'), () => { if(authState.status==='authenticated')load();else forgetVerification('请重新登录，旧工单资料已清除。') }, { immediate: true, flush:'sync' })
onBeforeUnmount(() => { sequence++; timelineSequence++; reasonSequence++ })
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
      <section v-if="detail.status === 'PENDING_SUPPLEMENT'">
        <h2>管理员要求补充信息</h2>
        <p v-if="returned" class="note">{{ returned.public_note || '本轮未提供公开文字原因，请联系授权管理人员确认。' }}<br>{{ dateText(returned.created_at) }}</p>
        <p v-else>{{ returnError || '正在核对本轮公开退回原因…' }}</p>
        <el-button v-if="returnError" @click="loadReason()">重试获取本轮退回原因</el-button>
      </section>
      <el-alert v-if="ownerError" :title="`尚未确认原报修人身份：${ownerError}`" type="warning" :closable="false" />
      <el-button v-if="owner && !editing" type="primary" @click="editing = true">补充信息 / 编辑</el-button>
      <OrderEditForm v-if="owner && editing" :key="detail.id" :detail="detail" @saved="saved" @complete="load" @refresh="load" />
      <p v-else>仅原报修人的草稿或待补充工单可以编辑。取消、评价与图片功能尚未开放。</p>
      <OrderVerifyPanel v-if="detail.status==='PENDING_VERIFY'" :key="`${detail.id}:${detail.version}`" :detail="detail" @changed="load" @forbidden="forgetVerification('身份、权限或资源变化，旧工单已清除。')" @stale="forgetVerification('状态或版本变化，请重新获取工单。')"/>
      <h2>公开时间线</h2>
      <el-alert v-if="timelineError" :title="timelineError" type="error" :closable="false" />
      <el-button v-if="timelineError" @click="loadTimeline()">重新加载时间线</el-button>
      <p v-if="timelineBusy">正在加载时间线…</p><OrderTimeline v-else-if="!timelineError" :items="items" />
      <el-pagination v-if="total" v-model:current-page="page" :page-size="20" :total="total" :disabled="timelineBusy" layout="prev, pager, next, total" @current-change="loadTimeline()" />
    </template>
  </main>
</template>
<style scoped>.order-page { padding: 28px; min-width: 0; } .summary { margin: 20px 0; } .note { white-space: pre-wrap; overflow-wrap: anywhere; } h2 { overflow-wrap: anywhere; }</style>
