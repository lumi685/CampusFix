<script setup>
import DispatchScoreDetail from './DispatchScoreDetail.vue'
defineProps({ candidate: { type: Object, required: true }, disabled: Boolean })
defineEmits(['choose'])
</script>
<template>
  <el-card class="candidate" shadow="never">
    <header><el-tag>{{ candidate.rank <= 3 ? 'Top' : '排名' }} {{ candidate.rank }}</el-tag><h3>{{ candidate.worker_name }}</h3></header>
    <p>人员 #{{ candidate.worker_id }} · 综合得分 <strong>{{ candidate.total_score }}</strong></p>
    <p>当前任务 / 容量：{{ candidate.active_tasks }} / {{ candidate.max_concurrent_tasks }}</p>
    <DispatchScoreDetail :candidate="candidate" />
    <p class="reason">推荐理由：{{ candidate.reason }}</p>
    <ul v-if="candidate.risks.length" class="risks" aria-label="服务端风险提示"><li v-for="(risk, i) in candidate.risks" :key="i">{{ risk }}</li></ul>
    <el-button type="primary" :disabled="disabled" :aria-label="`选择排名${candidate.rank}的${candidate.worker_name}`" @click="$emit('choose', candidate.worker_id)">选择并核对</el-button>
  </el-card>
</template>
<style scoped>
.candidate { min-width:0; } header { display:flex; align-items:center; gap:10px; flex-wrap:wrap; } h3 { margin:0; } strong { font-size:1.4rem; color:#205bab; }
.reason,.risks,p { overflow-wrap:anywhere; white-space:pre-wrap; } .risks { padding-left:20px; color:#8a5716; font-size:.85rem; } .el-button { margin-top:8px; }
</style>
