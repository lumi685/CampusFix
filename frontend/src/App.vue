<script setup>
import { watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { authState } from './stores/auth.js'

const route = useRoute()
const router = useRouter()
watch(() => authState.status, (status) => {
  if (status === 'anonymous' && route.meta.requiresAuth) router.replace('/login')
})
</script>

<template>
  <div v-if="authState.status === 'checking' || authState.status === 'loggingOut'" class="session-transition" role="status">
    {{ authState.status === 'checking' ? '正在核验服务端身份…' : '正在退出当前会话…' }}
  </div>
  <RouterView v-else-if="!route.meta.requiresAuth || (authState.status === 'authenticated' && authState.user)" />
</template>

<style>
* {
  box-sizing: border-box;
}

body {
  margin: 0;
  font-family: Arial, "Microsoft YaHei", sans-serif;
  color: #303133;
}

.session-transition { min-height: 100vh; display: grid; place-items: center; background: #f5f7fa; }
a { color: #2863b5; }
</style>
