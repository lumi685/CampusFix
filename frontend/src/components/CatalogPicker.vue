<script setup>
import { ref, watch, onBeforeUnmount } from 'vue'
import { categories, locations } from '../api/orders.js'
import { orderError } from '../utils/orders.js'
import { catalogPath, catalogContains } from '../utils/catalogPath.js'
const props = defineProps({ modelValue: Number, kind: String, disabled: Boolean })
const emit = defineEmits(['update:modelValue'])
const failure = ref(''), generation = ref(0), empty = ref(false)
const initialOptions = ref([]), preparing = ref(false)
let selectionSequence = 0
watch(() => [props.modelValue, props.kind], async () => {
  if (!props.modelValue || catalogContains(initialOptions.value, props.modelValue)) return
  const request = ++selectionSequence; preparing.value = true; failure.value = ''
  try {
    const tree = await catalogPath(props.modelValue, props.kind === 'locations' ? locations : categories)
    if (request !== selectionSequence) return
    initialOptions.value = tree; generation.value++
  } catch (e) { if (request === selectionSequence) failure.value = orderError(e) }
  finally { if (request === selectionSequence) preparing.value = false }
}, { immediate: true })
onBeforeUnmount(() => { selectionSequence++ })
const options = { value: 'id', label: 'name', emitPath: false, checkStrictly: true, lazy: true,
  async lazyLoad(node, resolve) {
    const current = generation.value
    try {
      if (node.level === 0 && initialOptions.value.length) return resolve(initialOptions.value)
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
function retry() { selectionSequence++; preparing.value = false; initialOptions.value = []; generation.value++; failure.value = ''; empty.value = false; emit('update:modelValue', undefined) }
</script>
<template>
  <div class="picker">
    <p v-if="preparing" role="status">正在解析原分类/位置路径…</p>
    <el-cascader v-else :key="generation" :options="initialOptions" :model-value="modelValue" :props="options" :disabled="disabled" clearable placeholder="选择节点；点击箭头展开下一级" @update:model-value="emit('update:modelValue', $event || undefined)" />
    <span v-if="empty">暂无可用选项</span>
    <div v-if="failure" role="alert">{{ failure }} <el-button :disabled="disabled" size="small" @click="retry">重新加载选项</el-button></div>
  </div>
</template>
<style scoped>.picker,.el-cascader { width: 100%; } [role=alert] { color: #b42318; margin-top: 6px; }</style>
