<script setup>
import { useRoute } from 'vue-router'
import { authState } from '../stores/auth.js'
const route = useRoute()
</script>
<template>
  <main class="placeholder">
    <el-card shadow="never">
      <el-tag type="info">后续开发入口</el-tag>
      <h1>{{ route.meta.title }}</h1>
      <p>当前仅提供导航占位，本模块的业务页面和操作将在后续阶段实现。</p>
      <p v-if="authState.user?.role === 'worker' && ['my-orders', 'report-new'].includes(route.meta.entryId)">
        本人报修与维修任务分开处理：你只能以原报修人身份验收、返工和评价自己创建的报修（D-017），参与他人维修不会获得这些权限。
      </p>
      <p>此页面不读取工单、执行派单或提交业务数据。</p>
    </el-card>
  </main>
</template>
<style scoped>
.placeholder { padding: 32px; max-width: 900px; } p { color: #606266; line-height: 1.8; }
h1 { font-size: 1.6rem; }
</style>
