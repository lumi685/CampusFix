<script setup>
import { ref, watch, onBeforeUnmount } from 'vue'
import { orderDetail, orderTimeline } from '../api/orders.js'
import { adminError } from '../utils/adminOrders.js'
import { dateText, urgencies } from '../utils/orders.js'
import OrderStatusTag from './OrderStatusTag.vue'
import OrderTimeline from './OrderTimeline.vue'
import OrderEvaluationPanel from './OrderEvaluationPanel.vue'
import OrderClosePanel from './OrderClosePanel.vue'
import { authState } from '../stores/auth.js'
const props = defineProps({ orderId: Number })
const emit = defineEmits(['close','changed'])
const detail = ref(null), items = ref([]), busy = ref(false), error = ref(''), page = ref(1), total = ref(0)
// Explicit display keys only; never stringify management_detail or raw metadata.
const labels = { review_action: '审核动作', category_id: '确认分类ID', approved_urgency: '确认紧急度', assignment_id: '派单ID', worker_id: '维修员ID', assign_reason: '派单原因', candidate_rank: '候选排名', transfer_request_id: '转派申请ID', decision: '转派决定', rework_count: '返工次数' }
let sequence = 0
async function load() {
  const request = ++sequence; busy.value = true; error.value = ''; detail.value = null; items.value = []; total.value = 0
  if (!props.orderId) { busy.value = false; return }
  try {
    const [d, t] = await Promise.all([orderDetail(props.orderId), orderTimeline(props.orderId, { page: page.value, page_size: 20 })])
    if (request !== sequence) return
    detail.value = d; items.value = t.items; total.value = t.total
  } catch (e) { if (request === sequence) error.value = adminError(e) }
  finally { if (request === sequence) busy.value = false }
}
function forget(){sequence++;detail.value=null;items.value=[];total.value=0;page.value=1;busy.value=false;error.value='身份、权限或状态变化，请重新获取。'}
async function changed(){emit('changed');await load()}
watch(() => [props.orderId,authState.status,authState.user?.id,authState.user?.role,authState.user?.capabilities?.join(','),authState.user?.scope_location_ids?.join(',')].join('|'), () => { page.value = 1;if(authState.status==='authenticated')load();else forget() }, { immediate: true,flush:'sync' })
onBeforeUnmount(() => { sequence++ })
</script>
<template>
  <el-drawer :model-value="!!orderId" title="管理工单详情" size="min(95vw, 720px)" @update:model-value="!$event && emit('close')">
    <p v-if="busy" role="status">正在查询详情与授权时间线…</p>
    <el-alert v-if="error" :title="error" type="error" :closable="false" /><el-button v-if="error" @click="load">重试读取</el-button>
    <template v-if="detail">
      <h2>{{ detail.order_no || `草稿 #${detail.id}` }} · {{ detail.title }}</h2><OrderStatusTag :status="detail.status" />
      <el-descriptions :column="1" border>
        <el-descriptions-item label="版本">{{ detail.version }}</el-descriptions-item>
        <el-descriptions-item label="分类 / 位置">{{ detail.category?.name || '—' }} / {{ detail.location?.name || '—' }}</el-descriptions-item>
        <el-descriptions-item label="紧急度">申报：{{ urgencies[detail.requested_urgency] || '—' }}；确认：{{ urgencies[detail.approved_urgency] || '未确认' }}</el-descriptions-item>
        <el-descriptions-item v-if="typeof detail.description === 'string'" label="描述"><span class="note">{{ detail.description }}</span></el-descriptions-item>
        <el-descriptions-item v-if="typeof detail.contact_name === 'string'" label="联系人">{{ detail.contact_name }}</el-descriptions-item>
        <el-descriptions-item v-if="typeof detail.contact_phone === 'string'" label="联系电话">{{ detail.contact_phone }}</el-descriptions-item>
        <el-descriptions-item v-if="Object.hasOwn(detail, 'expected_visit_at')" label="期望上门时间">{{ dateText(detail.expected_visit_at) }}</el-descriptions-item>
        <el-descriptions-item v-if="detail.current_assignment" label="当前负责人">{{ detail.current_assignment.worker_name || '—' }}</el-descriptions-item>
        <el-descriptions-item label="创建 / 提交">{{ dateText(detail.created_at) }} / {{ dateText(detail.submitted_at) }}</el-descriptions-item>
      </el-descriptions>
      <OrderEvaluationPanel :key="`evaluation:${detail.id}:${detail.version}`" :detail="detail" @forbidden="forget" @stale="forget"/>
      <OrderClosePanel v-if="detail.status==='COMPLETED'" :key="`close:${detail.id}:${detail.version}`" :detail="detail" @changed="changed" @forbidden="forget" @stale="forget"/>
      <h3>公开时间线</h3><OrderTimeline :items="items" />
      <section v-for="item in items" :key="item.id" class="note">
        <template v-if="typeof item.internal_note === 'string' || Object.keys(labels).some((key) => Object.hasOwn(item.management_detail || {}, key))">
          <h4>授权管理信息 · {{ dateText(item.created_at) }}</h4>
          <p v-if="typeof item.internal_note === 'string'">内部备注：{{ item.internal_note }}</p>
          <template v-for="(label, key) in labels" :key="key"><p v-if="['string','number'].includes(typeof item.management_detail?.[key])">{{ label }}：{{ item.management_detail[key] }}</p></template>
        </template>
      </section>
      <el-pagination v-if="total" v-model:current-page="page" :total="total" :page-size="20" :disabled="busy" layout="prev, pager, next, total" @current-change="load" />
      <p>有调度授权时可从智能派单入口读取真实转派审批队列；不会从历史时间线推断待审申请。取消尚未开放。关闭仍由后端逐资源验证权限。</p>
    </template>
  </el-drawer>
</template>
<style scoped>.note { white-space: pre-wrap; overflow-wrap: anywhere; } .el-descriptions { margin: 20px 0; } h2 { overflow-wrap: anywhere; }</style>
