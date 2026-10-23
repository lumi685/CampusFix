<script setup>
import { ref } from 'vue'
import { categories, locations } from '../api/orders.js'
import { orderError } from '../utils/orders.js'
const props = defineProps({ modelValue: Number, kind: String, disabled: Boolean })
const emit = defineEmits(['update:modelValue'])
const failure = ref(''), generation = ref(0), empty = ref(false)
const options = { value: 'id', label: 'name', emitPath: false, checkStrictly: true, lazy: true,
  async lazyLoad(node, resolve) {
    const current = generation.value
    try {
      const result = await (props.kind === 'locations' ? locations : categories)(node.level === 0 ? undefined : node.value)
      if (current !== generation.value) return resolve([])
      failure.value = ''
      if (node.level === 0) empty.value = !result.items.length
      resolve(result.items.map((item) => ({ id: item.id, name: item.name })))
    } catch (error) {
      if (current === generation.value) failure.value = orderError(error)
      resolve([])
    }
  },
}
function retry() { generation.value++; failure.value = ''; empty.value = false; emit('update:modelValue', undefined) }
</script>
<template>
  <div class="picker">
    <el-cascader :key="generation" :model-value="modelValue" :props="options" :disabled="disabled" clearable placeholder="选择节点；点击箭头展开下一级" @update:model-value="emit('update:modelValue', $event || undefined)" />
    <span v-if="empty">暂无可用选项</span>
    <div v-if="failure" role="alert">{{ failure }} <el-button :disabled="disabled" size="small" @click="retry">重新加载选项</el-button></div>
  </div>
</template>
<style scoped>.picker,.el-cascader { width: 100%; } [role=alert] { color: #b42318; margin-top: 6px; }</style>
