<script setup>
import { statuses } from '../utils/orders.js'
defineProps({ status: String })
</script>
<template><el-tag>{{ statuses[status] || '未知状态' }}</el-tag></template>
