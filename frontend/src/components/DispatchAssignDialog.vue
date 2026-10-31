<script setup>
import { computed } from 'vue'
import { ElDialog } from 'element-plus'
import { validateAssignReason } from '../utils/dispatch.js'
const props = defineProps({ candidate: Object, snapshot: Object, reason: String, busy: Boolean, message: String })
defineEmits(['cancel', 'confirm', 'update:reason'])
const validation = computed(() => { try { validateAssignReason(props.reason || '', props.candidate?.rank > 3); return '' } catch (e) { return e.message } })
</script>
<template>
  <ElDialog :model-value="!!candidate" title="人工确认派单" width="min(94vw, 600px)" :close-on-click-modal="false" :close-on-press-escape="!busy" :show-close="!busy" @update:model-value="!$event && !busy && $emit('cancel')">
    <template v-if="candidate && snapshot">
      <p>工单 #{{ snapshot.order_id }} · 推荐版本 {{ snapshot.order_version }}</p>
      <p>人员 #{{ candidate.worker_id }} · {{ candidate.worker_name }} · 排名 {{ candidate.rank }}</p>
      <p>当前负载 {{ candidate.active_tasks }} / {{ candidate.max_concurrent_tasks }} · 综合得分 {{ candidate.total_score }}</p>
      <el-alert title="服务器将在提交时重新校验资格、排名和容量；推荐不保证人员仍然可用。" type="info" :closable="false" />
      <p v-if="candidate.rank > 3">选择非前三名候选，需要填写人工调整原因。</p>
      <el-form label-position="top"><el-form-item :label="candidate.rank > 3 ? '人工调整原因（必填）' : '派单原因（可选）'" :error="validation">
        <el-input :model-value="reason" type="textarea" :rows="4" :disabled="busy" aria-label="人工派单原因" @update:model-value="$emit('update:reason', $event)" />
        <small>{{ [...(reason || '')].length }} / 500 Unicode字符</small>
      </el-form-item></el-form>
      <el-alert v-if="message" :title="message" type="warning" :closable="false" />
    </template>
    <template #footer><el-button :disabled="busy" @click="$emit('cancel')">取消</el-button><el-button type="primary" :loading="busy" :disabled="busy || !!validation || !candidate || !snapshot" @click="$emit('confirm')">确认派单</el-button></template>
  </ElDialog>
</template>
<style scoped>p,small { overflow-wrap:anywhere; } .el-form { margin-top:18px; } small { color:#606266; }</style>
