<script setup>
import { reactive, ref } from 'vue'
import { useRouter } from 'vue-router'
import { authState, signIn } from '../stores/auth.js'

const router = useRouter()
const form = reactive({ username: '', password: '' })
const formRef = ref()
const errorMessage = ref('')
const submitting = ref(false)
const rules = {
  username: [
    { required: true, message: '请输入用户名', trigger: 'blur' },
    { min: 3, max: 64, message: '用户名长度为 3～64 个字符', trigger: 'blur' },
    { pattern: /^[a-zA-Z0-9_.-]+$/, message: '用户名只能包含字母、数字、下划线、点或短横线', trigger: 'blur' },
  ],
  password: [
    { required: true, message: '请输入密码', trigger: 'blur' },
    { max: 128, message: '密码不能超过 128 个字符', trigger: 'blur' },
  ],
}

async function submit() {
  if (submitting.value || authState.busy) return
  submitting.value = true
  errorMessage.value = ''
  try {
    if (!(await formRef.value.validate().catch(() => false))) return
    if (await signIn(form.username, form.password)) await router.replace('/home')
  } catch (error) {
    errorMessage.value = error.status === 401 ? '用户名或密码错误，请核对后重试' : error.message
  } finally {
    form.password = ''
    submitting.value = false
  }
}
</script>

<template>
  <main class="login-page">
    <el-card class="login-card" shadow="never">
      <p class="eyebrow">CAMPUSFIX · 校园维修</p>
      <h1>登录 CampusFix</h1>
      <p class="intro">使用已启用的虚拟演示账号进入系统。</p>
      <el-alert v-if="authState.notice" :title="authState.notice" type="info" :closable="false" />
      <el-alert v-if="errorMessage" :title="errorMessage" type="error" :closable="false" role="alert" />
      <el-form ref="formRef" :model="form" :rules="rules" label-position="top" :disabled="submitting || authState.busy" @submit.prevent="submit">
        <el-form-item label="用户名" prop="username">
          <el-input v-model="form.username" autocomplete="username" maxlength="64" placeholder="请输入用户名" />
        </el-form-item>
        <el-form-item label="密码" prop="password">
          <el-input v-model="form.password" type="password" autocomplete="current-password" maxlength="128" placeholder="请输入密码" />
        </el-form-item>
        <el-button type="primary" native-type="submit" :loading="submitting || authState.busy" :disabled="submitting || authState.busy" class="login-button">登录</el-button>
      </el-form>
      <p class="session-note">当前会话仅保存在内存中。整页刷新后需要重新登录。</p>
      <RouterLink to="/">查看后端服务状态</RouterLink>
    </el-card>
  </main>
</template>

<style scoped>
.login-page { min-height: 100vh; display: grid; place-items: center; padding: 32px 20px; background: #f2f6fb; }
.login-card { width: min(100%, 430px); padding: 12px; border-radius: 14px; }
.eyebrow { color: #2863b5; font-size: .8rem; letter-spacing: .08em; }
h1 { margin: 14px 0; font-size: 1.7rem; }
.intro, .session-note { color: #606266; line-height: 1.7; }
.session-note { font-size: .85rem; }
.el-form { margin-top: 24px; }
.el-alert { margin-top: 12px; }
.login-button { width: 100%; }
</style>
