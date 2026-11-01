<script setup>
import { computed, reactive, ref, watch, onBeforeUnmount } from 'vue'
import { ElDialog } from 'element-plus'
import { authState } from '../stores/auth.js'
import { managementAbility } from '../utils/adminOrders.js'
import { transferRequests, decideTransfer } from '../api/workflow.js'
import { workflowFlow, transferPage } from '../utils/workflow.js'
import OrderStatusTag from './OrderStatusTag.vue'
const props=defineProps({orderId:{type:[String,Number],default:null},blocked:Boolean})
const emit=defineEmits(['recommend','changed','forbidden'])
const allowed=computed(()=>authState.status==='authenticated'&&managementAbility(authState.user,'DISPATCH'))
const items=ref([]),page=ref(1),total=ref(0),busy=ref(false),error=ref(''),lastReceipt=ref(null)
let sequence=0
const options=()=>({view:'managed',page:page.value,...(props.orderId?{order_id:props.orderId}:{})})
async function query(){const d=transferPage(await transferRequests(options()),true);if(d.page!==page.value||d.page_size!==20)throw new Error('分页响应不一致');return d}
const flow=reactive(workflowFlow({post:(c,note)=>decideTransfer(c.orderId,c.requestId,c.version,c.decision,note),verify:async()=>{const d=await query();return `已只读核实当前授权队列（${d.total}项）；申请消失不代表本次审批成功。`}},()=>allowed.value))
async function load(){
  if(['submitting','unknown_result'].includes(flow.phase))return
  const current=++sequence;items.value=[];total.value=0;error.value='';flow.clear();if(!allowed.value)return;busy.value=true
  try{const d=await query();if(current!==sequence||!allowed.value)return;items.value=d.items;total.value=d.total}
  catch(e){if(current===sequence){items.value=[];total.value=0;error.value=[401,403,404].includes(e.status)?'身份或调度授权已变化，旧队列已清除。':'审批队列读取失败，请重试。';if([401,403,404].includes(e.status)){lastReceipt.value=null;emit('forbidden')}}}
  finally{if(current===sequence)busy.value=false}
}
function choose(t,decision){
  if(props.blocked||busy.value||!items.value.includes(t))return
  flow.open({kind:'decision',orderId:t.order_id,requestId:t.transfer_request_id,version:t.order_version,status:t.order_status,decision})
}
async function confirm(){const current=sequence;try{
  const result=await flow.send()
  if(current!==sequence)return
  if(result){lastReceipt.value={...result,order_id:flow.context.orderId};await load();emit('changed')}
  else if(['conflict','forbidden','unknown_result'].includes(flow.phase)){items.value=[];total.value=0;if(flow.phase==='forbidden'){lastReceipt.value=null;emit('forbidden')}}
}catch(e){flow.message=e.message}}
async function verify(){await flow.verify();if(flow.phase==='forbidden'){items.value=[];total.value=0;lastReceipt.value=null;emit('forbidden')}}
watch(()=>[props.orderId,authState.status,authState.user?.id,authState.user?.role,(authState.user?.capabilities||[]).join(','),(authState.user?.scope_location_ids||[]).join(',')].join('|'),()=>{sequence++;flow.clear();items.value=[];total.value=0;lastReceipt.value=null;busy.value=false;error.value='';page.value=1;if(allowed.value)load()},{immediate:true,flush:'sync'})
onBeforeUnmount(()=>{sequence++;flow.clear();items.value=[];lastReceipt.value=null})
</script>
<template>
  <section class="transfer-queue" aria-label="授权转派审批队列">
    <h2>转派申请审批</h2><p>仅显示 API-34 返回的当前可审批申请；不会扩大工单详情或内部备注权限，不支持管理员无申请主动改派。</p>
    <el-alert v-if="!allowed" title="没有调度授权，不能读取或审批申请。" type="warning" :closable="false"/>
    <template v-else>
      <el-button :disabled="blocked||busy||['submitting','unknown_result'].includes(flow.phase)" @click="load()">刷新真实审批队列</el-button><p v-if="busy" role="status">读取授权队列…</p>
      <el-alert v-if="error" :title="error" type="warning" :closable="false"/><el-alert v-if="flow.message" :title="flow.message" type="info" :closable="false"/>
      <p v-if="!busy&&!error&&!items.length">当前页没有可审批申请，或旧操作已失效需刷新。</p>
      <article v-for="t in items" :key="t.transfer_request_id"><h3>{{t.order_no||`工单 #${t.order_id}`}} · 申请 #{{t.transfer_request_id}}</h3><p>{{t.reason}}</p><p>当前工单版本 {{t.order_version}} · 派单 #{{t.assignment_id}} · <OrderStatusTag :status="t.order_status"/></p><el-button :disabled="blocked||flow.phase==='submitting'||flow.phase==='unknown_result'" @click="choose(t,'APPROVE')">批准转派</el-button><el-button :disabled="blocked||flow.phase==='submitting'||flow.phase==='unknown_result'" @click="choose(t,'REJECT')">拒绝申请</el-button></article>
      <el-pagination v-if="total" v-model:current-page="page" :total="total" :page-size="20" :disabled="busy||['submitting','unknown_result'].includes(flow.phase)" layout="prev, pager, next" @current-change="load()"/>
      <section v-if="lastReceipt"><p>服务器审批回执：申请 #{{lastReceipt.transfer_request_id}} · {{lastReceipt.decision==='APPROVE'?'批准':'拒绝'}} · 版本 {{lastReceipt.version}} · <OrderStatusTag :status="lastReceipt.order_status"/></p><el-button v-if="lastReceipt.assignment_ended" :disabled="blocked" @click="emit('recommend',lastReceipt.order_id)">重新读取该工单推荐（不自动派单）</el-button></section>
      <section v-if="flow.phase==='unknown_result'"><el-button :disabled="flow.verifying" @click="verify()">只读核实，不重发</el-button><p>{{flow.verification}}</p></section>
      <ElDialog :model-value="['ready','submitting'].includes(flow.phase)" title="确认转派审批" width="min(92vw,520px)" :close-on-click-modal="false" :close-on-press-escape="flow.phase!=='submitting'" :show-close="flow.phase!=='submitting'" @close="flow.cancel()">
        <p>申请 #{{flow.context?.requestId}} · 工单 #{{flow.context?.orderId}} · 版本 {{flow.context?.version}}。{{flow.context?.decision==='APPROVE'?'批准将结束原派单并释放容量，不自动分配新人员。':'拒绝仅解决申请，原工单状态、版本及人员占用不变。'}}</p>
        <el-input v-model="flow.note" type="textarea" :rows="4" :disabled="flow.phase==='submitting'" aria-label="转派审批原因" placeholder="必填1～500个Unicode字符，将进入公开时间线"/><p>{{flow.message}}</p>
        <template #footer><el-button :disabled="flow.phase==='submitting'" @click="flow.cancel()">取消</el-button><el-button type="primary" :loading="flow.phase==='submitting'" :disabled="flow.phase==='submitting'" @click="confirm">确认{{flow.context?.decision==='APPROVE'?'批准':'拒绝'}}</el-button></template>
      </ElDialog>
    </template>
  </section>
</template>
<style scoped>.transfer-queue{margin:24px 0;padding:18px;border:1px solid #dcdfe6;border-radius:8px}article{margin:14px 0;padding:14px;background:#f5f7fa}p,h3{overflow-wrap:anywhere;white-space:pre-wrap}@media(max-width:600px){.transfer-queue{padding:12px}}</style>
