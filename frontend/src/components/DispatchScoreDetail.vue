<script setup>
import { scoreLabels } from '../utils/dispatch.js'
defineProps({ candidate: { type: Object, required: true } })
</script>
<template>
  <dl class="score-grid" aria-label="服务器评分明细">
    <div v-for="(label, key) in scoreLabels" :key="key"><dt>{{ label }}</dt><dd>{{ candidate.score_detail[key] }}</dd></div>
  </dl>
  <p>效率样本：{{ candidate.sample_counts.efficiency }}；评价样本：{{ candidate.sample_counts.rating }}</p>
  <p v-if="candidate.sample_counts.efficiency < 3 || candidate.sample_counts.rating < 3" class="hint">有效样本少于3条的对应历史分为冷启动中性分，不代表实际维修成绩。</p>
</template>
<style scoped>
.score-grid { display:grid; grid-template-columns:repeat(3,minmax(0,1fr)); gap:12px; margin:16px 0; }
dt,.hint { color:#606266; font-size:.85rem; } dd { margin:4px 0 0; font-weight:600; } p { overflow-wrap:anywhere; }
@media(max-width:480px) { .score-grid { grid-template-columns:repeat(2,minmax(0,1fr)); } }
</style>
