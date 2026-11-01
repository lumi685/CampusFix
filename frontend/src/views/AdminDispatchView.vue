<script setup>
import { computed, reactive, ref, watch, onBeforeUnmount } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { getDispatchCandidates, assignWorker } from '../api/dispatch.js'
import { managedOrders, orderDetail } from '../api/orders.js'
import { authState } from '../stores/auth.js'
import { managementAbility, managedQuery } from '../utils/adminOrders.js'
import { dispatchFlow, dispatchId, dispatchFailure, scoreLabels, exclusionRows, emptyDispatchText } from '../utils/dispatch.js'
import DispatchCandidateCard from '../components/DispatchCandidateCard.vue'
import DispatchAssignDialog from '../components/DispatchAssignDialog.vue'
import OrderStatusTag from '../components/OrderStatusTag.vue'
import TransferQueue from '../components/TransferQueue.vue'

const route = useRoute(), router = useRouter()
const canDispatch = computed(() => authState.status === 'authenticated' && managementAbility(authState.user, 'DISPATCH'))
const canRead = computed(() => canDispatch.value && managementAbility(authState.user, 'BUSINESS_READ'))
const flow = reactive(dispatchFlow({ candidates: getDispatchCandidates, assign: assignWorker, detail: orderDetail }, () => canRead.value))
const inputId = ref(''), entryError = ref(''), list = ref([]), page = ref(1), total = ref(0), listBusy = ref(false), listError = ref('')
let listGeneration = 0
const selected = computed(() => [...(flow.snapshot?.top_candidates || []), ...(flow.snapshot?.other_eligible_candidates || [])].find(c => c.worker_id === flow.selected) || null)
async function loadList(reset = false) {
  if (reset) page.value = 1
  const current = ++listGeneration; list.value = []; total.value = 0; listError.value = ''; listBusy.value = false
  if (!canRead.value) return
  listBusy.value = true
  try {
    const data = await managedOrders(managedQuery({ page: page.value, status: 'PENDING_DISPATCH' }))
    if (current !== listGeneration || !canRead.value) return
    if (!Array.isArray(data?.items) || !Number.isSafeInteger(data.total) || data.total < 0 || data.items.some(o => !Number.isSafeInteger(o.id) || o.id <= 0 || o.status !== 'PENDING_DISPATCH')) throw new Error('列表包含无效或超出安全整数范围的ID，已停止操作。')
    list.value = data.items.map(o => ({ id: o.id, title: typeof o.title === 'string' ? o.title : '工单', order_no: typeof o.order_no === 'string' ? o.order_no : '' })); total.value = data.total
  } catch (e) { if (current === listGeneration) listError.value = e.status ? dispatchFailure(e) : e.message }
  finally { if (current === listGeneration) listBusy.value = false }
}
async function open(id) {
  if (!canDispatch.value || flow.phase === 'submitting') return
  try { const value = dispatchId(id); entryError.value = ''; await router.push(`/admin/dispatch/${value}`) }
  catch (e) { entryError.value = e.message }
}
async function confirm() {
  try { await flow.send(); if (flow.phase === 'success') await loadList() }
  catch (e) { flow.message = e.message }
}
async function recommendAfterTransfer(id) {
  if (!canDispatch.value || ['submitting','unknown_result'].includes(flow.phase)) return
  flow.clear()
  if (String(route.params.id || '') === String(id)) await flow.load(id)
  else await open(id)
}
watch(() => [route.params.id || '', authState.status, authState.user?.id || '', authState.user?.role || '', (authState.user?.capabilities || []).join(','), (authState.user?.scope_location_ids || []).join(',')].join('|'), () => {
  flow.clear(); ++listGeneration; list.value = []; total.value = 0; listError.value = ''; listBusy.value = false; entryError.value = ''
  inputId.value = route.params.id || ''
  if (!canDispatch.value) return
  if (route.params.id) flow.load(route.params.id)
  loadList(true)
}, { immediate: true, flush: 'sync' })
onBeforeUnmount(() => { flow.clear(); ++listGeneration; list.value = [] })
</script>
<template>
  <main class="dispatch-page">
    <h1>智能派单</h1><p class="intro">先看真实推荐，再由管理员确认。评分只作建议，服务端始终负责资格、授权和事务一致性。</p>
    <el-alert v-if="!canDispatch" title="当前身份没有调度入口权限，旧数据已清除。" type="warning" :closable="false" />
    <template v-else>
      <TransferQueue :order-id="route.params.id || null" :blocked="['submitting','unknown_result'].includes(flow.phase)" @recommend="recommendAfterTransfer" @changed="loadList()" @forbidden="flow.clear()"/>
      <section class="selector" aria-label="选择工单">
        <el-form @submit.prevent="open(inputId)"><el-form-item label="工单ID"><el-input v-model="inputId" :disabled="flow.phase === 'submitting'" placeholder="输入正整数工单ID" aria-label="调度工单ID" /></el-form-item><el-button native-type="submit" :disabled="flow.phase === 'submitting'">读取推荐</el-button></el-form>
        <el-alert v-if="entryError" :title="entryError" type="error" :closable="false" />
      </section>
      <section v-if="canRead" class="pending-list" aria-label="授权待派工单">
        <h2>授权范围内待派工单</h2><el-button :disabled="listBusy || flow.phase === 'submitting'" @click="loadList()">刷新列表</el-button>
        <p v-if="listBusy" role="status">正在读取授权列表…</p><el-alert v-if="listError" :title="listError" type="error" :closable="false" />
        <p v-if="!listBusy && !listError && !list.length">当前页没有待派工单。</p>
        <ul><li v-for="o in list" :key="o.id"><el-button link type="primary" :disabled="flow.phase === 'submitting'" @click="open(o.id)">{{ o.order_no || `#${o.id}` }} · {{ o.title }}</el-button></li></ul>
        <el-pagination v-if="total" v-model:current-page="page" :total="total" :page-size="20" layout="prev, pager, next" :disabled="listBusy || flow.phase === 'submitting'" @current-change="loadList()" />
      </section>
      <el-alert v-else title="当前有 DISPATCH 授权，但没有 BUSINESS_READ；不会调用工单列表或详情。可输入工单ID或通过深链接，由推荐接口核验具体资源权限。" type="info" :closable="false" />

      <section class="recommendation" :data-phase="flow.phase" aria-live="polite">
        <p v-if="flow.phase === 'idle'">选择待派工单后加载推荐；不会自动选择或派单。</p>
        <p v-if="flow.phase === 'loading'" role="status">正在读取服务器最新推荐…</p>
        <el-alert v-if="flow.message" :title="flow.message" :type="flow.phase === 'success' ? 'success' : 'warning'" :closable="false" />
        <template v-if="flow.snapshot">
          <h2>工单 #{{ flow.snapshot.order_id }} · 版本 {{ flow.snapshot.order_version }}</h2>
          <p>策略：{{ flow.snapshot.strategy }}</p>
          <dl class="weights"><div v-for="(label, key) in scoreLabels" :key="key"><dt>{{ label }}权重</dt><dd>{{ flow.snapshot.weights[key] }}</dd></div></dl>
          <el-button :disabled="flow.phase === 'submitting'" @click="flow.load(route.params.id)">刷新推荐</el-button>
          <el-alert v-if="flow.phase === 'empty'" :title="emptyDispatchText(flow.snapshot.excluded_summary)" type="warning" :closable="false" />
          <h2 v-if="flow.snapshot.top_candidates.length">Top 3 推荐</h2>
          <div class="candidate-grid"><DispatchCandidateCard v-for="c in flow.snapshot.top_candidates" :key="c.worker_id" :candidate="c" :disabled="flow.phase !== 'ready'" @choose="flow.choose($event)" /></div>
          <h2 v-if="flow.snapshot.other_eligible_candidates.length">其他合格维修员</h2>
          <p v-if="flow.snapshot.other_eligible_candidates.length">保留服务器真实排名；选择非前三名需要填写人工调整原因。</p>
          <div class="candidate-grid"><DispatchCandidateCard v-for="c in flow.snapshot.other_eligible_candidates" :key="c.worker_id" :candidate="c" :disabled="flow.phase !== 'ready'" @choose="flow.choose($event)" /></div>
          <section v-if="Object.keys(flow.snapshot.excluded_summary).length"><h2>排除原因摘要</h2><p>仅统计覆盖范围内候选池；同一人可有多个原因，以下次数之和不是人员总数。</p><ul><li v-for="r in exclusionRows(flow.snapshot.excluded_summary)" :key="r.key">{{ r.label }}：{{ r.count }}</li></ul></section>
        </template>
        <el-button v-if="['error','conflict','forbidden'].includes(flow.phase)" @click="flow.load(route.params.id)">重新读取推荐</el-button>
        <section v-if="flow.result" class="receipt"><h2>服务器派单回执</h2><p>工单 #{{ flow.result.order_id }} · 派单 #{{ flow.result.assignment_id }}</p><p>人员 #{{ flow.result.worker.id }} · {{ flow.result.worker.display_name }}</p><OrderStatusTag :status="flow.result.status" /><p>最新版本 {{ flow.result.version }}</p></section>
        <section v-if="['success','unknown_result'].includes(flow.phase)">
          <el-button v-if="canRead" :loading="flow.verifying" :disabled="flow.verifying" @click="flow.verify()">只读核实最新状态</el-button>
          <p v-else>没有业务读取授权，不能查询列表或详情核实结果；请联系具备对应读取权限的管理员。</p>
          <p v-if="flow.phase === 'unknown_result'">即使工单已离开待派状态，也不能证明本次请求成功。本页面不会重发该派单。</p>
          <p v-if="flow.verification">只读查询：工单 #{{ flow.verification.id }}，<OrderStatusTag :status="flow.verification.status" />，版本 {{ flow.verification.version }}<span v-if="flow.verification.worker_name">，当前负责人 {{ flow.verification.worker_name }}</span></p>
        </section>
      </section>
      <DispatchAssignDialog :candidate="selected" :snapshot="flow.snapshot" v-model:reason="flow.reason" :busy="flow.phase === 'submitting'" :message="flow.message" @cancel="flow.cancel()" @confirm="confirm" />
    </template>
  </main>
</template>
<style scoped>
.dispatch-page { padding:28px; min-width:0; color:#303133; } h1 { margin-top:0; } .intro { color:#606266; } .selector,.pending-list,.recommendation { margin:24px 0; }
.el-form { display:flex; align-items:flex-start; gap:12px; flex-wrap:wrap; } .el-form-item { flex:1; min-width:180px; } .weights { display:flex; flex-wrap:wrap; gap:18px; font-size:.85rem; } dd { margin:4px 0; font-weight:600; }
.candidate-grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(min(100%,300px),1fr)); gap:16px; margin:16px 0; } li,p { overflow-wrap:anywhere; } .pending-list ul { max-height:230px; overflow:auto; padding-left:20px; } .el-button { max-width:100%; white-space:normal; height:auto; min-height:32px; } .receipt { padding:16px; background:#f0f9eb; margin-top:16px; }
@media(max-width:600px) { .dispatch-page { padding:16px; } .weights { gap:12px; } }
</style>
