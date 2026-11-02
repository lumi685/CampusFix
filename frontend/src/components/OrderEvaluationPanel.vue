<script setup>
import { computed, reactive, watch, onBeforeUnmount } from 'vue'
import { ElDialog } from 'element-plus'
import { authState } from '../stores/auth.js'
import { orderDetail, reportedOrders } from '../api/orders.js'
import { readEvaluation, createEvaluation } from '../api/evaluations.js'
import { evaluationFlow } from '../utils/evaluation.js'
import { dateText } from '../utils/orders.js'
const props=defineProps({detail:{type:Object,required:true}}),emit=defineEmits(['changed','forbidden','stale'])
const allowed=computed(()=>authState.status==='authenticated')
const flow=reactive(evaluationFlow({detail:orderDetail,reported:reportedOrders,read:readEvaluation,post:createEvaluation},()=>allowed.value))
let generation=0
async function load(){const g=generation;await flow.load(props.detail.id,authState.user?.role);if(g!==generation)return;if(flow.context&&(flow.context.version!==props.detail.version||flow.context.status!==props.detail.status)){flow.clear();emit('stale')}if(flow.phase==='forbidden')emit('forbidden')}
function choose(){try{flow.choose()}catch(e){flow.message=e.message}}
async function confirm(){const g=generation;try{const r=await flow.send();if(g!==generation)return;if(r)emit('changed');else if(flow.phase==='forbidden')emit('forbidden');else if(flow.phase==='conflict')emit('stale')}catch(e){if(g===generation)flow.message=e.message}}
watch(()=>[props.detail.id,props.detail.version,props.detail.status,authState.status,authState.user?.id,authState.user?.role,authState.user?.capabilities?.join(','),authState.user?.scope_location_ids?.join(',')].join('|'),()=>{generation++;flow.clear();if(allowed.value)load()},{immediate:true,flush:'sync'})
onBeforeUnmount(()=>{generation++;flow.clear()})
</script>
<template>
  <section class="evaluation-panel"><h2>维修评价</h2>
    <p v-if="flow.phase==='loading'">正在读取真实评价并核验原报修人资格…</p>
    <el-alert v-if="flow.message" :title="flow.message" type="info" :closable="false"/>
    <section v-if="flow.evaluation"><p>评分：{{flow.evaluation.score}} / 5 · {{dateText(flow.evaluation.created_at)}}</p><p class="text">{{flow.evaluation.comment||'未填写评论'}}</p><p>评价仅可查看，不可修改或删除。</p></section>
    <p v-else-if="flow.phase==='readonly'">尚无评价。仅已完成且未关闭工单的原报修人可评价一次；维修身份不能代评价。</p>
    <section v-if="flow.phase==='ready'">
      <el-select v-model="flow.score" aria-label="评价评分" :disabled="!!flow.confirmation"><el-option v-for="n in 5" :key="n" :value="n" :label="`${n} 分`"/></el-select>
      <el-input v-model="flow.text" type="textarea" :rows="3" :disabled="!!flow.confirmation" placeholder="可选评论，最多1000个Unicode字符" aria-label="评价评论"/><p>{{[...flow.text].length}} / 1000</p><el-button type="primary" @click="choose">确认评价</el-button>
    </section>
    <el-button v-if="['error','conflict'].includes(flow.phase)" @click="load">重新读取评价及归属</el-button>
    <section v-if="flow.phase==='unknown_result'"><el-button :disabled="flow.verifying" @click="flow.reconcile().then(()=>{if(flow.phase==='forbidden')emit('forbidden')})">只读核实评价，不重发</el-button><p v-if="flow.verification">当前{{flow.verification.evaluation?'存在评价':'尚未查到评价'}}；不证明本次POST成功。</p></section>
    <ElDialog :model-value="!!flow.confirmation&&['ready','submitting'].includes(flow.phase)" title="确认一次性评价" width="min(92vw,540px)" :close-on-click-modal="false" :close-on-press-escape="flow.phase!=='submitting'" :show-close="flow.phase!=='submitting'" @close="flow.cancel()">
      <p>工单 {{detail.order_no}} · 评分 {{flow.confirmation?.score}} / 5</p><p class="text">{{flow.confirmation?.text||'未填写评论'}}</p><p>提交后不可修改或删除，评价不会关闭工单。</p><p>{{flow.message}}</p>
      <template #footer><el-button :disabled="flow.phase==='submitting'" @click="flow.cancel()">取消</el-button><el-button type="primary" :loading="flow.phase==='submitting'" :disabled="flow.phase==='submitting'" @click="confirm">确认提交评价</el-button></template>
    </ElDialog>
  </section>
</template>
<style scoped>.evaluation-panel{margin:20px 0;padding:16px;border:1px solid #dcdfe6;border-radius:8px}.text{white-space:pre-wrap;overflow-wrap:anywhere}.el-select{width:160px;margin-bottom:12px}</style>
