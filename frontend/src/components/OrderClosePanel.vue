<script setup>
import { computed, reactive, watch, onBeforeUnmount } from 'vue'
import { ElDialog } from 'element-plus'
import { authState } from '../stores/auth.js'
import { orderDetail } from '../api/orders.js'
import { closeOrder } from '../api/evaluations.js'
import { closeFlow } from '../utils/evaluation.js'
import { managementAbility } from '../utils/adminOrders.js'
const props=defineProps({detail:{type:Object,required:true}}),emit=defineEmits(['changed','forbidden','stale'])
const allowed=computed(()=>authState.status==='authenticated'&&managementAbility(authState.user,'CLOSE')&&managementAbility(authState.user,'BUSINESS_READ')&&props.detail.status==='COMPLETED')
const flow=reactive(closeFlow({detail:orderDetail,post:closeOrder},()=>allowed.value))
let generation=0
async function load(){const g=generation;await flow.load(props.detail.id,authState.user?.role);if(g!==generation)return;if(flow.context&&flow.context.version!==props.detail.version){flow.clear();emit('stale')}if(flow.phase==='forbidden')emit('forbidden')}
function choose(){try{flow.choose()}catch(e){flow.message=e.message}}
async function confirm(){const g=generation;try{const r=await flow.send();if(g!==generation)return;if(r)emit('changed');else if(flow.phase==='forbidden')emit('forbidden');else if(flow.phase==='conflict')emit('stale')}catch(e){if(g===generation)flow.message=e.message}}
watch(()=>[props.detail.id,props.detail.version,props.detail.status,authState.status,authState.user?.id,authState.user?.role,authState.user?.capabilities?.join(','),authState.user?.scope_location_ids?.join(',')].join('|'),()=>{generation++;flow.clear();if(allowed.value)load()},{immediate:true,flush:'sync'})
onBeforeUnmount(()=>{generation++;flow.clear()})
</script>
<template>
  <section v-if="allowed" class="close-panel"><h3>管理员归档关闭</h3><p>可以不评价直接关闭。关闭后不可新增评价、修改工单或恢复；历史记录保留。</p>
    <p v-if="flow.phase==='loading'">正在重新读取真实状态及版本…</p><el-alert v-if="flow.message" :title="flow.message" type="info" :closable="false"/>
    <section v-if="flow.phase==='ready'"><p>工单版本：{{flow.context?.version}}</p><el-input v-model="flow.text" type="textarea" :disabled="!!flow.confirmation" aria-label="公开关闭说明" placeholder="可选公开关闭说明，最多500个Unicode字符"/><p>{{[...flow.text].length}} / 500</p><el-button type="warning" @click="choose">归档关闭</el-button></section>
    <el-button v-if="['error','conflict'].includes(flow.phase)" @click="load">重新读取，不重放旧操作</el-button>
    <section v-if="flow.phase==='unknown_result'"><el-button :disabled="flow.verifying" @click="flow.reconcile().then(()=>{if(flow.phase==='forbidden')emit('forbidden')})">只读核实，不重发</el-button><p v-if="flow.verification">当前 {{flow.verification.status}} · 版本 {{flow.verification.version}}，不证明本次POST成功。</p></section>
    <ElDialog :model-value="!!flow.confirmation&&['ready','submitting'].includes(flow.phase)" title="确认归档关闭" width="min(92vw,540px)" :close-on-click-modal="false" :close-on-press-escape="flow.phase!=='submitting'" :show-close="flow.phase!=='submitting'" @close="flow.cancel()"><p>工单 {{detail.order_no}} · 版本 {{flow.context?.version}}</p><p class="text">{{flow.confirmation?.text||'不填写关闭说明'}}</p><p>确认完成归档？关闭后不可恢复，也不能补评价。</p><p>{{flow.message}}</p><template #footer><el-button :disabled="flow.phase==='submitting'" @click="flow.cancel()">取消</el-button><el-button type="warning" :loading="flow.phase==='submitting'" :disabled="flow.phase==='submitting'" @click="confirm">确认关闭</el-button></template></ElDialog>
  </section>
</template>
<style scoped>.close-panel{margin:20px 0;padding:16px;border:1px solid #dcdfe6;border-radius:8px}.text{white-space:pre-wrap;overflow-wrap:anywhere}</style>
