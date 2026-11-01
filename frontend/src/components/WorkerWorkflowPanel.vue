<script setup>
import { computed, reactive, ref, watch, onBeforeUnmount } from 'vue'
import { ElDialog } from 'element-plus'
import { authState } from '../stores/auth.js'
import { requestTransfer, transferRequests, progressEvent } from '../api/workflow.js'
import { orderTimeline } from '../api/orders.js'
import { workflowFlow, transferPage, transferStatuses } from '../utils/workflow.js'
import OrderTimeline from './OrderTimeline.vue'
const props=defineProps({task:{type:Object,required:true},blocked:Boolean,gate:{type:Object,required:true}})
const emit=defineEmits(['changed','forbidden'])
const allowed=computed(()=>authState.status==='authenticated'&&authState.user?.role==='worker')
const history=ref([]),timeline=ref([]),busy=ref(false),error=ref(''),page=ref(1),total=ref(0),arrived=ref(false)
let sequence=0,contextGeneration=0,owner=null
function release(){if(owner&&props.gate.owner===owner){props.gate.owner=null;props.gate.busy=false;props.gate.unknown=false}owner=null}
async function read(){
  const current=++sequence;history.value=[];timeline.value=[];total.value=0;error.value='';if(!allowed.value)return;busy.value=true
  try{const [h,t]=await Promise.all([transferRequests({view:'mine',order_id:props.task.id}),orderTimeline(props.task.id,{page:page.value,page_size:20})]);if(current!==sequence||!allowed.value)return;history.value=transferPage(h).items;if(!Array.isArray(t?.items)||!Number.isSafeInteger(t.total))throw new Error();timeline.value=t.items;total.value=t.total}
  catch(e){if(current===sequence){history.value=[];timeline.value=[];error.value='公开记录读取失败，请重试。';if([401,403,404].includes(e.status)){emit('forbidden');throw e}}}
  finally{if(current===sequence)busy.value=false}
}
const flow=reactive(workflowFlow({post:(c,note)=>c.kind==='transfer'?requestTransfer(c.orderId,c.version,note):progressEvent(c.orderId,c.version,c.kind,note),verify:async()=>{await read();if(error.value)throw new Error();return '已只读重新查询本人申请和公开时间线；不能证明本次POST成功。'}},()=>allowed.value))
function open(kind){if(props.blocked||props.gate.busy||props.gate.unknown)return;flow.open({orderId:props.task.id,version:props.task.version,status:props.task.status,kind})}
async function confirm(){
  if(props.gate.busy||props.gate.unknown)return
  const current=contextGeneration,ticket=Symbol('worker-workflow-operation');owner=ticket;props.gate.owner=ticket;props.gate.busy=true
  try{const result=await flow.send();if(current!==contextGeneration)return;if(flow.phase==='unknown_result'){props.gate.unknown=true;return}if(flow.phase==='forbidden'){history.value=[];timeline.value=[];emit('forbidden')}if(result){if(result.type==='ARRIVED')arrived.value=true;await read()}}
  catch(e){flow.message=e.message}
  finally{if(props.gate.owner===ticket){props.gate.busy=false;if(flow.phase!=='unknown_result')release()}}
}
watch(()=>[props.task.id,props.task.version,authState.status,authState.user?.id,authState.user?.role].join('|'),()=>{sequence++;contextGeneration++;flow.clear();release();history.value=[];timeline.value=[];arrived.value=false;error.value='';busy.value=false;page.value=1;total.value=0},{flush:'sync'})
onBeforeUnmount(()=>{sequence++;contextGeneration++;flow.clear();release();history.value=[];timeline.value=[]})
</script>
<template>
  <section class="workflow-panel">
    <div class="buttons"><el-button :disabled="blocked||gate.busy||gate.unknown" @click="open('transfer')">申请转派</el-button><template v-if="task.status==='IN_PROGRESS'"><el-button :disabled="blocked||gate.busy||gate.unknown||arrived" @click="open('ARRIVED')">{{arrived?'已确认到场':'记录到场'}}</el-button><el-button :disabled="blocked||gate.busy||gate.unknown" @click="open('PROGRESS_NOTE')">填写维修进度</el-button></template><el-button :disabled="busy||gate.busy" @click="read().catch(()=>{})">查看/刷新公开记录</el-button></div>
    <el-alert v-if="error" :title="error" type="warning" :closable="false"/><el-alert v-if="flow.message" :title="flow.message" type="info" :closable="false"/>
    <p v-if="busy" role="status">读取本人申请与公开时间线…</p>
    <ul><li v-for="t in history" :key="t.transfer_request_id">申请 #{{t.transfer_request_id}} · {{transferStatuses[t.status]}}：{{t.reason}}<span v-if="t.review_reason">；审批/解决说明：{{t.review_reason}}</span></li></ul>
    <OrderTimeline v-if="timeline.length" :items="timeline"/>
    <el-pagination v-if="total" v-model:current-page="page" :total="total" :page-size="20" :disabled="busy" layout="prev, pager, next" @current-change="read().catch(()=>{})"/>
    <section v-if="flow.phase==='unknown_result'"><el-button :disabled="flow.verifying" @click="flow.verify()">只读核实，不重发</el-button><p>{{flow.verification}}</p></section>
    <ElDialog :model-value="['ready','submitting'].includes(flow.phase)" :title="flow.context?.kind==='transfer'?'申请转派':flow.context?.kind==='ARRIVED'?'确认已到场':'提交维修进度'" width="min(92vw,520px)" :close-on-click-modal="false" :close-on-press-escape="flow.phase!=='submitting'" :show-close="flow.phase!=='submitting'" @close="flow.cancel()">
      <p>工单 {{task.order_no}} · 当前版本 {{task.version}}。说明会进入公开时间线；申请不代表管理员批准，不释放容量。</p>
      <el-input v-model="flow.note" type="textarea" :rows="4" :disabled="flow.phase==='submitting'" :placeholder="flow.context?.kind==='ARRIVED'?'可选，最多500字符':'必填原因/进度，进度最多1000字符，其余最多500字符'" aria-label="转派或进度说明"/>
      <p>{{flow.message}}</p><template #footer><el-button :disabled="flow.phase==='submitting'" @click="flow.cancel()">取消</el-button><el-button type="primary" :loading="flow.phase==='submitting'" :disabled="flow.phase==='submitting'" @click="confirm">确认提交</el-button></template>
    </ElDialog>
  </section>
</template>
<style scoped>.workflow-panel{margin-top:14px}.buttons{display:flex;gap:8px;flex-wrap:wrap}.buttons .el-button{margin:0}li,p{overflow-wrap:anywhere;white-space:pre-wrap}</style>
