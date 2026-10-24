<script setup>
import { ref, onMounted, onBeforeUnmount } from 'vue'
import { workers, workerWorkload } from '../api/workers.js'
import { locations } from '../api/orders.js'
import { availabilityLabels, skillOptions, workerForm, serviceAreas } from '../utils/workers.js'
import { adminError } from '../utils/adminOrders.js'
import WorkerEditDrawer from '../components/WorkerEditDrawer.vue'
const items = ref([]), options = ref(null), areas = ref([]), page = ref(1), total = ref(0), availability = ref(''), skill = ref(), busy = ref(false), error = ref(''), areaError = ref(''), editingId = ref(), workload = ref(null), checkingId = ref()
let sequence = 0, loadSequence = 0, areaSequence = 0
async function load(reset = false) {
  if (reset) page.value = 1
  const request = ++sequence; items.value = []; total.value = 0; options.value = null; error.value = ''; busy.value = true
  try {
    const data = await workers({ page: page.value, page_size: 20, ...(availability.value ? { availability: availability.value } : {}), ...(skill.value ? { skill_id: skill.value } : {}) })
    if (request !== sequence) return
    options.value = skillOptions(data.skill_options); items.value = data.items; total.value = data.total
    if (!data.items.length && data.total > 0 && page.value > 1) { page.value = Math.max(1, Math.ceil(data.total / 20)); await load() }
  } catch (e) { if (request === sequence) error.value = adminError(e) }
  finally { if (request === sequence) busy.value = false }
}
async function loadAreas() {
  const request = ++areaSequence; areaError.value = ''; areas.value = []
  try { const result = await serviceAreas(locations); if (request === areaSequence) areas.value = result }
  catch (e) { if (request === areaSequence) areaError.value = adminError(e) }
}
function editable(row) { try { workerForm(row, options.value); return true } catch { return false } }
async function checkLoad(id) {
  if (checkingId.value) return
  const request = ++loadSequence; checkingId.value = id; workload.value = null
  try { const result = await workerWorkload(id); if (request === loadSequence) workload.value = result }
  catch (e) { if (request === loadSequence) error.value = adminError(e) }
  finally { if (request === loadSequence) checkingId.value = undefined }
}
onMounted(() => { load(); loadAreas() })
onBeforeUnmount(() => { sequence++; loadSequence++; areaSequence++ })
</script>
<template>
  <main class="admin-page">
    <h1>维修人员管理</h1><p>只管理后端授权范围内的既有档案；不提供账号、角色、授权、人员新增或删除操作。</p>
    <div class="filters">
      <el-select v-model="availability" clearable placeholder="全部可用性"><el-option v-for="(label, value) in availabilityLabels" :key="value" :label="label" :value="value" /></el-select>
      <el-select v-model="skill" clearable placeholder="全部技能" :disabled="!options"><el-option v-for="option in options || []" :key="option.id" :label="option.name" :value="option.id" /></el-select>
      <el-button :disabled="busy" @click="load(true)">查询</el-button><el-button :disabled="busy" @click="load()">刷新</el-button>
    </div>
    <el-alert v-if="error" :title="error" type="error" :closable="false" />
    <el-alert v-if="areaError" :title="`区域名称加载失败：${areaError}`" type="warning" :closable="false" /><el-button v-if="areaError" @click="loadAreas">重试目录</el-button>
    <p v-if="busy" role="status">正在读取真实人员和有效技能目录…</p><el-empty v-else-if="!error && !items.length" description="授权范围暂无符合条件的人员" />
    <el-table v-if="!error && items.length" :data="items">
      <el-table-column prop="worker_no" label="编号" min-width="130" /><el-table-column prop="display_name" label="显示名称" min-width="150" />
      <el-table-column label="服务区域" min-width="180"><template #default="{ row }">{{ areas.find((a) => a.id === row.service_area_id)?.label || `#${row.service_area_id}（未解析或已停用）` }}</template></el-table-column>
      <el-table-column label="可用性" min-width="100"><template #default="{ row }">{{ availabilityLabels[row.availability] || '未知' }}</template></el-table-column>
      <el-table-column label="档案启用" min-width="90"><template #default="{ row }">{{ typeof row.active === 'boolean' ? row.active ? '启用' : '停用' : '未读取' }}</template></el-table-column>
      <el-table-column label="任务 / 容量" min-width="100"><template #default="{ row }">{{ row.active_tasks }} / {{ row.max_concurrent_tasks }}</template></el-table-column>
      <el-table-column label="有效技能" min-width="250"><template #default="{ row }"><div v-for="s in row.skills" :key="s.skill_id">{{ options?.find((o) => o.id === s.skill_id)?.name || `技能 #${s.skill_id}` }} Lv{{ s.level }} · {{ s.certified ? '持证' : '无证' }}</div><span v-if="!row.skills?.length">无有效技能</span></template></el-table-column>
      <el-table-column label="操作" min-width="180"><template #default="{ row }"><el-button link type="primary" :disabled="!editable(row)" @click="editingId = row.id">编辑</el-button><el-button link :disabled="!!checkingId" @click="checkLoad(row.id)">核对负载</el-button><small v-if="!editable(row)">完整档案缺失，禁止编辑</small></template></el-table-column>
    </el-table>
    <p v-if="workload">人员 #{{ workload.worker_id }} 实时负载：{{ workload.active_tasks }} / {{ workload.max_concurrent_tasks }}，剩余 {{ workload.remaining_capacity }}（{{ workload.calculated_at }}）</p>
    <el-pagination v-if="total" v-model:current-page="page" :page-size="20" :total="total" :disabled="busy" layout="prev, pager, next, total" @current-change="load()" />
    <WorkerEditDrawer :worker-id="editingId" @close="editingId = undefined" @saved="load()" />
  </main>
</template>
<style scoped>.admin-page { padding: 28px; min-width: 0; } .filters { display: flex; flex-wrap: wrap; gap: 12px; margin: 20px 0; } .filters>.el-select { width: 200px; } .el-pagination { margin-top: 20px; } h1 { margin-top: 0; } small { display: block; }</style>
