<script setup>
import { onMounted, ref } from 'vue'

import { getHealth } from '../api/health.js'

const phase = ref('loading')
const service = ref('')
const requestId = ref('')
const errorMessage = ref('')

async function checkBackend() {
  phase.value = 'loading'
  service.value = ''
  requestId.value = ''
  errorMessage.value = ''

  try {
    const response = await getHealth()
    service.value = response.data.service
    requestId.value = response.request_id
    phase.value = 'online'
  } catch (error) {
    errorMessage.value = error instanceof Error ? error.message : '后端请求失败'
    phase.value = 'offline'
  }
}

onMounted(checkBackend)
</script>

<template>
  <main class="welcome">
    <el-card class="welcome-card" shadow="never">
      <p class="eyebrow">前后端联调</p>
      <h1>CampusFix</h1>
      <p class="intro">前端工程已启动，下面的状态来自 Drogon 健康检查接口。</p>

      <section class="health" aria-label="后端服务状态" aria-live="polite">
        <h2>后端服务状态</h2>
        <p v-if="phase === 'loading'" class="status">加载中，正在检测后端…</p>
        <template v-else-if="phase === 'online'">
          <el-tag type="success">后端在线</el-tag>
          <p>服务名称：{{ service }}</p>
          <p class="request-id">request_id：{{ requestId }}</p>
        </template>
        <template v-else>
          <el-alert title="后端离线或请求失败" type="error" :closable="false" />
          <p class="error-detail">{{ errorMessage }}</p>
        </template>
        <el-button :loading="phase === 'loading'" @click="checkBackend">重新检测</el-button>
      </section>
    </el-card>
  </main>
</template>

<style scoped>
.welcome {
  display: grid;
  place-items: start center;
  padding: 48px 24px;
}

.welcome-card {
  width: min(100%, 600px);
}

.eyebrow {
  color: #409eff;
  letter-spacing: 0.08em;
}

h1 {
  margin: 10px 0;
  font-size: 2.5rem;
}

h2 {
  margin-top: 0;
  font-size: 1.1rem;
}

.intro,
.request-id,
.error-detail {
  color: #606266;
}

.health {
  margin-top: 28px;
  padding-top: 24px;
  border-top: 1px solid #e4e7ed;
}

.status {
  color: #909399;
}

.request-id {
  overflow-wrap: anywhere;
  font-family: Consolas, monospace;
  font-size: 0.85rem;
}

.health .el-button {
  margin-top: 12px;
}
</style>
