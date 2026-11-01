<script setup>
import { computed, reactive, watch, onBeforeUnmount } from 'vue'
import { ElDialog } from 'element-plus'
import { authState } from '../stores/auth.js'
import { orderDetail, orderTimeline, reportedOrders } from '../api/orders.js'
import { verifyOrder } from '../api/verification.js'
import { verifyFlow } from '../utils/verification.js'
import { dateText } from '../utils/orders.js'
const props=defineProps({detail:{type:Object,required:true}}),emit=defineEmits(['changed','forbidden','stale'])
const allowed=computed(()=>authState.status==='authenticated'&&['reporter','worker'].includes(authState.user?.role)&&props.detail.status==='PENDING_VERIFY')
const flow=reactive(verifyFlow({reported:reportedOrders,detail:orderDetail,timeline:orderTimeline,post:verifyOrder},()=>allowed.value))
let generation=0
async function load(){const current=generation;await flow.load(props.detail.id,authState.user?.role);if(current!==generation)return;if(flow.context&&flow.context.version!==props.detail.version){flow.clear();emit('stale')}if(flow.phase==='forbidden')emit('forbidden')}
async function confirm(){const current=generation;try{const r=await flow.send();if(current!==generation)return;if(r)emit('changed');else if(flow.phase==='forbidden')emit('forbidden');else if(flow.phase==='conflict')emit('stale')}catch(e){if(current===generation)flow.message=e.message}}
watch(()=>[props.detail.id,props.detail.version,props.detail.status,authState.status,authState.user?.id,authState.user?.role].join('|'),()=>{generation++;flow.clear();if(allowed.value)load()},{immediate:true,flush:'sync'})
onBeforeUnmount(()=>{generation++;flow.clear()})
</script>
<template>
  <section class="verify-panel">
    <h2>原报修人验收</h2><p v-if="flow.phase==='loading'">正在核验本人报修归属及本轮完整维修记录…</p>
    <el-alert v-if="flow.message" :title="flow.message" type="info" :closable="false"/>
    <template v-if="flow.context">
      <p>工单 {{detail.order_no}} · 版本 {{flow.context.version}} · 已返工 {{flow.context.reworks}} 次</p>
      <h3>本轮维修结果</h3><p class="result">{{flow.context.result.public_note}}</p><p>提交时间：{{dateText(flow.context.result.created_at)}} · {{flow.context.result.actor?.display_name||'维修人员'}}</p>
      <p v-if="detail.current_assignment?.worker_name">当前维修人员：{{detail.current_assignment.worker_name}}</p>
      <el-button type="primary" :disabled="flow.phase!=='ready'" @click="flow.choose(true)">验收通过</el-button><el-button type="warning" :disabled="flow.phase!=='ready'" @click="flow.choose(false)">验收不通过 / 要求返工</el-button>
    </template>
    <el-button v-if="['error','conflict'].includes(flow.phase)" @click="load">重新核验归属与本轮结果</el-button>
    <section v-if="flow.phase==='unknown_result'"><el-button :disabled="flow.verifying" @click="flow.reconcile().then(()=>{if(flow.phase==='forbidden')emit('forbidden')})">只读核实，不重发</el-button><p v-if="flow.verification">最新状态 {{flow.verification.status}} · 版本 {{flow.verification.version}}；不证明原请求成功。</p></section>
    <ElDialog :model-value="flow.approved!==null&&['ready','submitting'].includes(flow.phase)" :title="flow.approved?'确认验收通过':'填写返工原因并确认'" width="min(92vw,540px)" :close-on-click-modal="false" :close-on-press-escape="flow.phase!=='submitting'" :show-close="flow.phase!=='submitting'" @close="flow.cancel()">
      <p>工单 {{detail.order_no}} · 当前核验版本 {{flow.context?.version}}</p><p>{{flow.approved?'确认维修结果符合要求？通过后结束当前派单并释放负载，不会自动评价或关闭。':'返工将保留原维修人员、历史结果及任务负载。原因会进入公开时间线。'}}</p>
      <el-input v-if="flow.approved===false" v-model="flow.reason" type="textarea" :rows="4" :disabled="flow.phase==='submitting'" aria-label="返工原因" placeholder="必填1～500个Unicode字符"/><p v-if="flow.approved===false">{{[...flow.reason].length}} / 500 字符</p>
      <p>{{flow.message}}</p><template #footer><el-button :disabled="flow.phase==='submitting'" @click="flow.cancel()">取消</el-button><el-button type="primary" :loading="flow.phase==='submitting'" :disabled="flow.phase==='submitting'" @click="confirm">确认提交</el-button></template>
    </ElDialog>
  </section>
</template>
<style scoped>.verify-panel{margin:20px 0;padding:16px;border:1px solid #dcdfe6;border-radius:8px}.result,p{white-space:pre-wrap;overflow-wrap:anywhere}.el-button{margin:4px}</style>
