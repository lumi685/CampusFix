<script setup>
import { ref, computed, watch, onBeforeUnmount } from 'vue'
import { managedOrders } from '../api/orders.js'
import { authState } from '../stores/auth.js'
import { managementAbility, managedQuery, adminError } from '../utils/adminOrders.js'
import { statuses, dateText } from '../utils/orders.js'
import CatalogPicker from '../components/CatalogPicker.vue'
import OrderStatusTag from '../components/OrderStatusTag.vue'
import OrderReviewDrawer from '../components/OrderReviewDrawer.vue'
import AdminOrderDetailDrawer from '../components/AdminOrderDetailDrawer.vue'
const props = defineProps({ reviewOnly: Boolean })
const canRead = computed(() => managementAbility(authState.user, 'BUSINESS_READ'))
const canReview = computed(() => managementAbility(authState.user, 'REVIEW'))
const canDispatch = computed(() => managementAbility(authState.user, 'DISPATCH'))
const items = ref([]), total = ref(0), page = ref(1), status = ref(props.reviewOnly ? 'PENDING_REVIEW' : ''), category = ref(), busy = ref(false), error = ref('')
const detailId = ref(), reviewId = ref()
let sequence = 0
async function load(reset = false) {
  if (reset) page.value = 1
  const request = ++sequence; items.value = []; total.value = 0; error.value = ''; busy.value = false
  if (!canRead.value) { detailId.value = undefined; reviewId.value = undefined; return }
  busy.value = true
  try {
    const data = await managedOrders(managedQuery({ page: page.value, status: status.value, category: category.value }))
    if (request !== sequence) return
    items.value = data.items; total.value = data.total
    if (!data.items.length && data.total > 0 && page.value > 1) { page.value = Math.max(1, Math.ceil(data.total / 20)); await load() }
  } catch (e) {
    if (request === sequence) { error.value = adminError(e); if ([401, 403, 404].includes(e.status)) { detailId.value = undefined; reviewId.value = undefined } }
  }
  finally { if (request === sequence) busy.value = false }
}
watch(canRead, () => load(), { immediate: true })
onBeforeUnmount(() => { sequence++ })
function changed() { detailId.value = undefined; load() }
</script>
<template>
  <main class="admin-page">
    <h1>{{ reviewOnly ? '工单审核' : '工单管理' }}</h1>
    <p>数据由后端按当前业务读取授权范围过滤；范围摘要不能代替资源授权。</p>
    <el-alert v-if="!canRead" title="当前有审核入口，但没有 BUSINESS_READ 业务列表读取授权，无法获取可审核工单。请联系授权负责人；不会扩大读取范围。" type="warning" :closable="false" />
    <template v-else>
      <div class="filters">
        <el-select v-model="status" clearable placeholder="全部状态" aria-label="状态筛选"><el-option v-for="(label, value) in statuses" :key="value" :value="value" :label="label" /></el-select>
        <CatalogPicker v-model="category" kind="categories" :disabled="busy" />
        <el-button :disabled="busy" @click="load(true)">查询</el-button><el-button :disabled="busy" @click="load()">刷新</el-button>
      </div>
      <el-alert v-if="error" :title="error" type="error" :closable="false" />
      <p v-if="busy" role="status">正在加载授权工单…</p><el-empty v-else-if="!error && !items.length" description="当前授权范围没有符合条件的工单" />
      <el-table v-if="!error && items.length" :data="items">
        <el-table-column label="工单号" min-width="180"><template #default="{ row }">{{ row.order_no || `草稿 #${row.id}` }}</template></el-table-column>
        <el-table-column prop="title" label="标题" min-width="180" />
        <el-table-column label="分类 / 位置" min-width="180"><template #default="{ row }">{{ row.category?.name || '—' }} / {{ row.location?.name || '—' }}</template></el-table-column>
        <el-table-column label="状态" min-width="110"><template #default="{ row }"><OrderStatusTag :status="row.status" /></template></el-table-column>
        <el-table-column prop="version" label="版本" width="70" />
        <el-table-column label="创建时间" min-width="180"><template #default="{ row }">{{ dateText(row.created_at) }}</template></el-table-column>
        <el-table-column label="操作" min-width="190"><template #default="{ row }"><el-button link type="primary" @click="detailId = row.id">详情</el-button><el-button v-if="canReview && row.status === 'PENDING_REVIEW'" link type="primary" @click="reviewId = row.id">审核</el-button><RouterLink v-if="canDispatch && row.status === 'PENDING_DISPATCH' && Number.isSafeInteger(row.id) && row.id > 0" :to="`/admin/dispatch/${row.id}`">智能派单</RouterLink></template></el-table-column>
      </el-table>
      <el-pagination v-if="total" v-model:current-page="page" :page-size="20" :total="total" :disabled="busy" layout="prev, pager, next, total" @current-change="load()" />
    </template>
    <AdminOrderDetailDrawer :order-id="detailId" @close="detailId = undefined" />
    <OrderReviewDrawer :order-id="reviewId" @close="reviewId = undefined" @changed="changed" />
  </main>
</template>
<style scoped>.admin-page { padding: 28px; min-width: 0; } .filters { display: flex; flex-wrap: wrap; gap: 12px; margin: 20px 0; } .filters>.el-select,.filters>.picker { width: 230px; } .el-pagination { margin-top: 20px; } h1 { margin-top: 0; }</style>
