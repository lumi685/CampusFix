<script setup>
import { ref } from 'vue'
import { authState, refreshIdentity } from '../stores/auth.js'
import { roleLabels } from '../router/navigation.js'

const errorMessage = ref('')
async function refresh() {
  errorMessage.value = ''
  try { await refreshIdentity() } catch (error) { errorMessage.value = error.message }
}
</script>

<template>
  <main class="home-page" v-if="authState.user">
    <el-card shadow="never">
      <p class="eyebrow">服务端身份已确认</p>
      <h1>你好，{{ authState.user.display_name }}</h1>
      <p>用户名：{{ authState.user.username }}</p>
      <p>主要角色：<el-tag>{{ roleLabels[authState.user.role] }}</el-tag></p>
      <h2>授权摘要</h2>
      <p>能力码：{{ authState.user.capabilities.join('、') || '无额外管理授权' }}</p>
      <p>区域编号：{{ authState.user.scope_location_ids.join('、') || '未返回具体区域编号' }}</p>
      <p class="note">此处仅展示 /auth/me 返回的摘要。每项能力的实际范围、资源归属及操作资格由后端核验；未返回区域编号不表示全局权限。</p>
      <p v-if="authState.user.role === 'sysadmin'" class="note">系统概览仅显示本人身份与已获授权入口；账号、权限管理后台不在当前范围内。</p>
      <el-alert v-if="errorMessage" :title="errorMessage" type="error" :closable="false" />
      <el-button :loading="authState.busy" @click="refresh">重新查询身份</el-button>
    </el-card>
  </main>
</template>

<style scoped>
.home-page { padding: 32px; max-width: 900px; }
.eyebrow { color: #2863b5; }
h1 { font-size: 1.6rem; } h2 { font-size: 1.1rem; margin-top: 28px; }
.note { color: #606266; line-height: 1.8; }
.el-button { margin-top: 16px; }
</style>
