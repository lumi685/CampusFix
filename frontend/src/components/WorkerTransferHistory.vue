<script setup>
import { computed, ref, watch, onBeforeUnmount } from 'vue'
import { authState } from '../stores/auth.js'
import { transferRequests } from '../api/workflow.js'
import { transferPage, transferStatuses } from '../utils/workflow.js'
import { dateText } from '../utils/orders.js'
const emit=defineEmits(['forbidden'])
const allowed=computed(()=>authState.status==='authenticated'&&authState.user?.role==='worker')
const items=ref([]),page=ref(1),total=ref(0),busy=ref(false),error=ref('')
let sequence=0
async function load(){const current=++sequence;items.value=[];total.value=0;error.value='';if(!allowed.value)return;busy.value=true
  try{const d=transferPage(await transferRequests({view:'mine',page:page.value}));if(current!==sequence||!allowed.value)return;items.value=d.items;total.value=d.total}
  catch(e){if(current===sequence){items.value=[];total.value=0;error.value=[401,403,404].includes(e.status)?'身份或权限已变化，旧申请已清除。':'本人申请读取失败，请重试。';if([401,403,404].includes(e.status))emit('forbidden')}}
  finally{if(current===sequence)busy.value=false}
}
watch(()=>[authState.status,authState.user?.id,authState.user?.role].join('|'),()=>{sequence++;items.value=[];total.value=0;busy.value=false;error.value='';page.value=1;if(allowed.value)load()},{immediate:true,flush:'sync'})
onBeforeUnmount(()=>{sequence++;items.value=[]})
</script>
<template>
  <section v-if="allowed" class="transfer-history"><h2>我提交的转派申请</h2><p>包含本人历史申请及公开审批结果；原任务被转派后仍可在这里查询，不赋予历史任务维修权限。</p>
    <el-button :disabled="busy" @click="load()">刷新本人申请</el-button><p v-if="busy" role="status">读取申请…</p><el-alert v-if="error" :title="error" type="warning" :closable="false"/>
    <p v-if="!busy&&!error&&!items.length">当前页暂无本人转派申请。</p>
    <ul><li v-for="t in items" :key="t.transfer_request_id">{{t.order_no||`工单 #${t.order_id}`}} · 申请 #{{t.transfer_request_id}} · {{transferStatuses[t.status]}} · {{dateText(t.created_at)}}<p>申请：{{t.reason}}</p><p v-if="t.review_reason">公开审批/解决说明：{{t.review_reason}} · {{dateText(t.resolved_at)}}</p></li></ul>
    <el-pagination v-if="total" v-model:current-page="page" :total="total" :page-size="20" :disabled="busy" layout="prev, pager, next" @current-change="load()"/>
  </section>
</template>
<style scoped>.transfer-history{margin:24px 0;padding:16px;border:1px solid #dcdfe6;border-radius:8px}li{margin:14px 0}li,p{overflow-wrap:anywhere;white-space:pre-wrap}</style>
