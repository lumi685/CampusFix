<template>
  <main class="welcome">
    <el-card class="welcome-card" shadow="never">
      <p class="eyebrow">校园维修工单与智能调度系统</p>
      <h1>CampusFix</h1>
      <p class="status">前端工程已启动</p>
      <el-tag type="success" effect="plain">W2-D3 · Vue 3 + Vite</el-tag>
    </el-card>
  </main>
</template>

<style scoped>
.welcome {
  min-height: 100vh;
  display: grid;
  place-items: center;
  padding: 24px;
  background: #f5f7fa;
}

.welcome-card {
  width: min(100%, 480px);
  text-align: center;
}

.eyebrow {
  color: #606266;
  letter-spacing: 0.08em;
}

h1 {
  margin: 12px 0;
  color: #303133;
  font-size: 2.5rem;
}

.status {
  margin-bottom: 24px;
  color: #409eff;
  font-size: 1.2rem;
}
</style>
