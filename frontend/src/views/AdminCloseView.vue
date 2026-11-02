<script setup>
import { computed, reactive, watch, onBeforeUnmount } from 'vue'
import { ElDialog } from 'element-plus'
import { authState } from '../stores/auth.js'
import { managementAbility } from '../utils/adminOrders.js'
import { closeQueue } from '../utils/closeQueue.js'
import { closableOrders, closeOrder } from '../api/evaluations.js'
import { dateText } from '../utils/orders.js'
const allowed=computed(()=>authState.status==='authenticated'&&managementAbility(authState.user,'CLOSE'))
const flow=reactive(closeQueue({list:closableOrders,post:closeOrder},()=>allowed.value))
function choose(){try{flow.choose()}catch(e){flow.message=e.message}}
async function confirm(){try{await flow.send()}catch(e){flow.message=e.message}}
watch(()=>[authState.status,authState.user?.id,authState.user?.role,authState.user?.capabilities?.join(','),authState.user?.scope_location_ids?.join(',')].join('|'),()=>{flow.clear();if(allowed.value)flow.load()},{immediate:true,flush:'sync'})
onBeforeUnmount(()=>flow.clear())
</script>
<template>
  <main class="close-page"><h1>可关闭工单队列</h1><p>仅按关闭能力查询最小工单字段。此入口不提供详情、时间线或评价读取权限；每次关闭仍由服务器重新校验。</p>
    <el-alert v-if="!allowed" title="没有已确认的关闭授权。" type="warning" :closable="false"/>
    <template v-else><el-alert v-if="flow.message" :title="flow.message" type="info" :closable="false"/>
      <el-button :disabled="flow.busy||flow.phase==='submitting'" @click="flow.load()">{{flow.phase==='unknown_result'?'只读核实，不重发':'刷新真实队列'}}</el-button>
      <p v-if="flow.busy" role="status">正在加载…</p><el-empty v-else-if="!flow.items.length" description="当前没有可显示的授权待关闭工单"/>
      <el-table v-if="flow.items.length" :data="flow.items"><el-table-column prop="id" label="ID"/><el-table-column prop="order_no" label="工单编号" min-width="190"/><el-table-column label="状态"><template #default>已完成</template></el-table-column><el-table-column prop="version" label="版本"/><el-table-column label="更新时间" min-width="180"><template #default="{row}">{{dateText(row.updated_at)}}</template></el-table-column><el-table-column label="操作"><template #default="{row}"><el-button :disabled="flow.busy||['submitting','unknown_result','forbidden'].includes(flow.phase)" @click="flow.select(row)">选择关闭</el-button></template></el-table-column></el-table>
      <el-pagination v-if="flow.total" v-model:current-page="flow.page" :total="flow.total" :page-size="20" :disabled="flow.busy||flow.phase==='submitting'" layout="prev, pager, next, total" @current-change="flow.load()"/>
      <section v-if="flow.selection"><p>已选择 {{flow.selection.order_no}} · ID {{flow.selection.id}} · 版本 {{flow.selection.version}}</p><el-input v-model="flow.text" type="textarea" :disabled="!!flow.confirmation||flow.phase==='submitting'" aria-label="公开关闭说明" placeholder="可选公开说明，最多500个Unicode字符"/><el-button :disabled="flow.phase!=='ready'" type="warning" @click="choose">确认关闭</el-button></section>
      <p v-if="flow.verification">当前页可见工单数 {{flow.verification.visible_ids.length}}，授权总数 {{flow.verification.total}}。队列变化不证明原POST成功。</p>
      <ElDialog :model-value="!!flow.confirmation&&['ready','submitting'].includes(flow.phase)" title="确认关闭真实工单" width="min(92vw,540px)" :close-on-click-modal="false" :close-on-press-escape="flow.phase!=='submitting'" :show-close="flow.phase!=='submitting'" @close="flow.cancel()"><p>{{flow.confirmation?.order_no}} · ID {{flow.confirmation?.id}} · 版本 {{flow.confirmation?.version}}</p><p class="note">{{flow.confirmation?.note||'不填写说明'}}</p><p>可不经评价关闭，关闭后不可补评或恢复。</p><p>{{flow.message}}</p><template #footer><el-button :disabled="flow.phase==='submitting'" @click="flow.cancel()">取消</el-button><el-button type="warning" :disabled="flow.phase==='submitting'" :loading="flow.phase==='submitting'" @click="confirm">确认归档</el-button></template></ElDialog>
    </template>
  </main>
</template>
<style scoped>.close-page{padding:28px;min-width:0}.note{white-space:pre-wrap;overflow-wrap:anywhere}.el-button{margin:8px 0}section{margin:20px 0}</style>
