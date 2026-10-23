<script setup>
import { ref, onMounted, onBeforeUnmount } from 'vue'
import { useRouter } from 'vue-router'
import { reportedOrders } from '../api/orders.js'
import { statuses, dateText, orderError } from '../utils/orders.js'
import OrderStatusTag from '../components/OrderStatusTag.vue'
import CatalogPicker from '../components/CatalogPicker.vue'
const router = useRouter(), items = ref([]), total = ref(0), page = ref(1), status = ref(''), category = ref(), busy = ref(false), error = ref('')
let sequence = 0
async function load(reset = false) {
  if (reset) page.value = 1
  const request = ++sequence; busy.value = true; error.value = ''; items.value = []
  try {
    const data = await reportedOrders({ page: page.value, page_size: 20, ...(status.value ? { status: status.value } : {}), ...(category.value ? { category_id: category.value } : {}) })
    if (request !== sequence) return
    items.value = data.items; total.value = data.total
    if (!items.value.length && total.value > 0 && page.value > 1) { page.value = Math.max(1, Math.ceil(total.value / 20)); await load() }
  } catch (e) { if (request === sequence) { error.value = orderError(e); total.value = 0 } }
  finally { if (request === sequence) busy.value = false }
}
onMounted(() => load())
onBeforeUnmount(() => { sequence++ })
</script>
<template>
  <main class="order-page">
    <h1>我提交的报修</h1><p>这里只显示本人作为报修人的工单，不包含维修任务。</p>
    <div class="filters">
      <el-select v-model="status" clearable placeholder="全部状态" aria-label="状态筛选"><el-option v-for="(label, value) in statuses" :key="value" :label="label" :value="value" /></el-select>
      <CatalogPicker v-model="category" kind="categories" />
      <el-button :disabled="busy" @click="load(true)">查询</el-button>
      <el-button type="primary" @click="router.push('/report/new')">提交报修</el-button>
    </div>
    <el-alert v-if="error" :title="error" type="error" :closable="false" /><el-button v-if="error" @click="load()">重新加载列表</el-button>
    <p v-if="busy" role="status">正在加载工单…</p>
    <el-empty v-else-if="!error && !items.length" description="暂无符合条件的本人报修工单" />
    <el-table v-if="!error && items.length" :data="items" style="width:100%">
      <el-table-column label="工单编号" min-width="170"><template #default="{ row }">{{ row.order_no || `草稿 #${row.id}` }}</template></el-table-column>
      <el-table-column prop="title" label="标题" min-width="180" />
      <el-table-column label="分类" min-width="140"><template #default="{ row }">{{ row.category?.name || '—' }}</template></el-table-column>
      <el-table-column label="位置" min-width="140"><template #default="{ row }">{{ row.location?.name || '—' }}</template></el-table-column>
      <el-table-column label="状态" min-width="110"><template #default="{ row }"><OrderStatusTag :status="row.status" /></template></el-table-column>
      <el-table-column label="创建时间" min-width="180"><template #default="{ row }">{{ dateText(row.created_at) }}</template></el-table-column>
      <el-table-column label="操作" width="90"><template #default="{ row }"><el-button link type="primary" @click="router.push(`/orders/${row.id}`)">详情</el-button></template></el-table-column>
    </el-table>
    <el-pagination v-if="total" v-model:current-page="page" :page-size="20" :total="total" :disabled="busy" layout="prev, pager, next, total" @current-change="load()" />
  </main>
</template>
<style scoped>.order-page { padding: 28px; min-width: 0; } .filters { display: flex; flex-wrap: wrap; align-items: start; gap: 12px; margin: 20px 0; } .filters>.el-select,.filters>.picker { width: 230px; } .el-pagination { margin-top: 20px; } h1 { margin-top: 0; }</style>
