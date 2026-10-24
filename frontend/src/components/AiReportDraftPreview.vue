<script setup>
import { draftFields } from '../utils/reportAssistant.js'
const props = defineProps({ modelValue: { type: Object, required: true }, stale: Boolean, busy: Boolean, canGenerate: Boolean, copyNotice: String })
const emit = defineEmits(['update:modelValue', 'regenerate', 'copy', 'cancel'])
function edit(field, value) { emit('update:modelValue', { ...props.modelValue, [field]: value }) }
function editMissing(index, text) {
  const list = [...props.modelValue.missing_information]; list[index] = text; edit('missing_information', list)
}
function removeMissing(index) { edit('missing_information', props.modelValue.missing_information.filter((_, i) => i !== index)) }
</script>
<template>
  <section class="draft-preview" aria-label="可编辑报修建议">
    <h2>AI 建议，尚未创建或提交工单</h2>
    <p>请逐项核对和编辑。分类与位置只是文字线索，不能代替正式报修中的真实选项；本页不会自动回填报修表单。</p>
    <el-alert v-if="stale" title="对话已变化或正在重新生成，此建议不再是当前有效建议，请重新生成。" type="warning" :closable="false" />
    <el-form label-position="top" :disabled="busy || stale" @submit.prevent>
      <el-form-item v-for="[field, label, max] in draftFields" :key="field" :label="`${label}（最多${max}字，可删除）`" :for="`ai-draft-${field}`">
        <el-input :id="`ai-draft-${field}`" :model-value="modelValue[field]" :type="field === 'description' ? 'textarea' : 'text'" :rows="4" @update:model-value="edit(field, $event)" />
      </el-form-item>
      <fieldset><legend>待补充信息（最多5条，可修改或删除）</legend>
        <div v-for="(text, index) in modelValue.missing_information" :key="index" class="missing-row">
          <el-input :model-value="text" :aria-label="`待补充信息${index + 1}`" @update:model-value="editMissing(index, $event)" />
          <el-button :disabled="busy || stale" @click="removeMissing(index)">删除</el-button>
        </div>
        <el-button :disabled="modelValue.missing_information.length >= 5 || busy || stale" @click="edit('missing_information', [...modelValue.missing_information, ''])">添加待补充信息</el-button>
      </fieldset>
    </el-form>
    <div class="draft-actions">
      <el-button :disabled="busy || stale" @click="emit('copy')">复制建议</el-button>
      <el-button :disabled="!canGenerate" @click="emit('regenerate')">重新生成</el-button>
      <RouterLink to="/report/new"><el-button>前往普通报修</el-button></RouterLink>
      <el-button @click="emit('cancel')">取消建议</el-button>
    </div>
    <p v-if="copyNotice" role="status">{{ copyNotice }}</p>
    <small>重新生成会替换已编辑的建议。复制是你主动选择的系统剪贴板操作，不会保存到CampusFix数据库。</small>
  </section>
</template>
<style scoped>
.draft-preview { margin-top: 22px; padding: 20px; border: 1px solid #dcdfe6; border-radius: 8px; background: #fff; min-width: 0; }
h2 { font-size: 1.1rem; margin-top: 0; }
p, small { color: #606266; line-height: 1.7; overflow-wrap: anywhere; }
fieldset { border: 1px solid #dcdfe6; min-width: 0; padding: 12px; }
legend { font-size: .9rem; }
.missing-row, .draft-actions { display: flex; gap: 10px; margin: 12px 0; }
.missing-row :deep(.el-input) { min-width: 0; }
.draft-actions { flex-wrap: wrap; }
.draft-actions :deep(.el-button + .el-button) { margin-left: 0; }
@media (max-width: 480px) { .draft-preview { padding: 12px; } .missing-row { flex-wrap: wrap; } }
</style>
