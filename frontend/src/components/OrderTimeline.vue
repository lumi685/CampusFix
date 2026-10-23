<script setup>
import { actions, statuses, dateText } from '../utils/orders.js'
defineProps({ items: { type: Array, default: () => [] } })
</script>
<template>
  <el-empty v-if="!items.length" description="暂无公开时间线" />
  <el-timeline v-else>
    <el-timeline-item v-for="item in items" :key="item.id" :timestamp="dateText(item.created_at)">
      <strong>{{ actions[item.action] || '工单事件' }}</strong>
      <p>{{ item.from_status ? (statuses[item.from_status] || '未知状态') + ' → ' : '' }}{{ statuses[item.to_status] || '未知状态' }}</p>
      <p v-if="item.public_note" class="note">{{ item.public_note }}</p>
      <small>{{ item.actor?.display_name || '系统' }}</small>
    </el-timeline-item>
  </el-timeline>
</template>
<style scoped>.note { white-space: pre-wrap; overflow-wrap: anywhere; } small { color: #606266; }</style>
