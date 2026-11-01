<script setup>
import { computed, reactive, watch, onBeforeUnmount } from 'vue'
import { ElDialog } from 'element-plus'
import { authState } from '../stores/auth.js'
import { workerTasks, taskAction, refusalReason } from '../api/workerTasks.js'
import { orderDetail } from '../api/orders.js'
import { workerTaskFlow } from '../utils/workerTasks.js'
import OrderStatusTag from '../components/OrderStatusTag.vue'
const allowed = computed(() => authState.status === 'authenticated' && authState.user?.role === 'worker')
const flow = reactive(workerTaskFlow({ list: workerTasks, action: taskAction, detail: orderDetail }, () => allowed.value))
const busy = computed(() => ['loading', 'submitting', 'unknown_result'].includes(flow.phase))
const urgency = { LOW: '低', NORMAL: '普通', HIGH: '高', URGENT: '紧急' }
async function confirm() {
  try { if (flow.selected?.kind === 'refuse') refusalReason(flow.reason); await flow.send() }
  catch (e) { flow.message = e.message }
}
watch(() => [authState.status, authState.user?.id, authState.user?.role].join('|'), () => { flow.clear(); if (allowed.value) flow.load(1, 'PENDING_ACCEPT') }, { immediate: true, flush: 'sync' })
onBeforeUnmount(() => flow.clear())
</script>
<template>
  <main class="worker-tasks">
    <h1>我的维修任务</h1><p>仅查询当前有效派单属于本人的任务，与“我提交的报修”及历史维修记录分离。服务器在每次操作时重新核验权限。</p>
    <el-alert v-if="!allowed" title="当前身份无维修任务权限，旧数据已清除。" type="warning" :closable="false" />
    <template v-else>
      <div class="toolbar"><el-button :disabled="busy" @click="flow.load(1, 'PENDING_ACCEPT')">待接任务</el-button><el-button :disabled="busy" @click="flow.load(1, 'IN_PROGRESS')">进行中</el-button><el-button :disabled="busy" @click="flow.load(flow.page)">刷新任务</el-button></div>
      <p v-if="flow.phase === 'loading'" role="status">正在读取真实任务…</p>
      <el-alert v-if="flow.message" :title="flow.message" type="info" :closable="false" />
      <p v-if="flow.phase === 'empty'">当前页暂无{{ flow.status === 'PENDING_ACCEPT' ? '待接' : '进行中' }}任务。</p>
      <article v-for="task in flow.items" :key="task.id" class="task-card">
        <h2>{{ task.order_no }} · {{ task.title }}</h2><p>{{ task.category }} · {{ task.location }}</p><p>审核紧急度：{{ urgency[task.urgency] }} · 版本 {{ task.version }} · <OrderStatusTag :status="task.status" /></p>
        <template v-if="task.status === 'PENDING_ACCEPT'"><el-button type="primary" :disabled="flow.phase !== 'ready'" @click="flow.choose(task.id, 'accept')">接单</el-button><el-button type="danger" :disabled="flow.phase !== 'ready'" @click="flow.choose(task.id, 'refuse')">拒单</el-button></template>
        <p v-else>服务器已确认正在维修。转派、到场及完工功能不在本阶段开放。</p>
      </article>
      <el-pagination v-if="flow.total" :current-page="flow.page" :total="flow.total" :page-size="20" :disabled="busy" layout="prev, pager, next" @current-change="flow.load($event)" />
      <section v-if="flow.phase === 'unknown_result'" aria-live="polite"><el-button :loading="flow.verifying" :disabled="flow.verifying" @click="flow.verify()">只读核实最新状态</el-button><p v-if="flow.verification">工单 #{{ flow.verification.id }} · 版本 {{ flow.verification.version }} · <OrderStatusTag :status="flow.verification.status" /></p><p>未知写入不会重试。请核实后联系管理员；离开页面再返回会重新读取服务器任务。</p></section>
      <ElDialog :model-value="!!flow.selected && ['ready','submitting'].includes(flow.phase)" :title="flow.selected?.kind === 'accept' ? '确认接单' : '填写拒单原因'" width="min(92vw, 520px)" :close-on-click-modal="false" :close-on-press-escape="flow.phase !== 'submitting'" :show-close="flow.phase !== 'submitting'" @close="flow.cancel()">
        <p>工单 {{ flow.selected?.order_no }} · 版本 {{ flow.selected?.version }}。提交后由服务器执行事务，不会自动重试。</p>
        <el-input v-if="flow.selected?.kind === 'refuse'" v-model="flow.reason" type="textarea" :rows="4" :disabled="flow.phase === 'submitting'" aria-label="拒单原因" placeholder="必填1～500字符；将记录在工单公开时间线" />
        <p v-if="flow.message">{{ flow.message }}</p>
        <template #footer><el-button :disabled="flow.phase === 'submitting'" @click="flow.cancel()">取消</el-button><el-button type="primary" :loading="flow.phase === 'submitting'" :disabled="flow.phase === 'submitting'" @click="confirm">确认提交</el-button></template>
      </ElDialog>
    </template>
  </main>
</template>
<style scoped>
.worker-tasks { padding:28px; min-width:0; } .toolbar { display:flex; gap:8px; flex-wrap:wrap; margin:20px 0; } .toolbar .el-button { margin:0; } .task-card { padding:18px; border:1px solid #dcdfe6; border-radius:8px; margin:14px 0; } h2 { font-size:1.05rem; } p,h2 { overflow-wrap:anywhere; } @media(max-width:600px) { .worker-tasks { padding:16px; } }
</style>
