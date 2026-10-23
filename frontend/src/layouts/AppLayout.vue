<script setup>
import { computed } from 'vue'
import { useRouter } from 'vue-router'
import { authState, signOut } from '../stores/auth.js'
import { roleLabels, visibleNavigation } from '../router/navigation.js'

const router = useRouter()
const entries = computed(() => visibleNavigation(authState.user))
async function logout() {
  await signOut()
  await router.replace('/login')
}
</script>

<template>
  <div class="app-layout">
    <header class="topbar">
      <span class="brand">CampusFix</span>
      <span class="subtitle">校园维修工单与智能调度系统</span>
      <div class="account">
        <template v-if="authState.user">
          <span>{{ authState.user.display_name }} · {{ roleLabels[authState.user.role] }}</span>
          <el-button :loading="authState.busy" :disabled="authState.busy" @click="logout">退出登录</el-button>
        </template>
        <RouterLink v-else to="/login">登录系统</RouterLink>
      </div>
    </header>

    <div class="content">
      <aside class="sidebar" aria-label="角色导航">
        <p class="nav-title">{{ authState.user ? roleLabels[authState.user.role] : 'CampusFix' }}</p>
        <p class="nav-note">本人报修已开放；其他业务模块仍为占位。</p>
        <nav v-if="authState.user" aria-label="已授权功能入口">
          <RouterLink v-for="entry in entries" :key="entry.id" :to="entry.path">{{ entry.title }}</RouterLink>
        </nav>
        <p v-else class="nav-note">登录后按服务端身份显示导航。</p>
        <RouterLink class="health-link" to="/">服务状态</RouterLink>
      </aside>

      <RouterView />
    </div>
  </div>
</template>

<style scoped>
.app-layout {
  min-height: 100vh;
  background: #f5f7fa;
}

.topbar {
  display: flex;
  align-items: baseline;
  gap: 20px;
  padding: 18px 28px;
  background: #fff;
  border-bottom: 1px solid #e4e7ed;
}

.account { margin-left: auto; display: flex; align-items: center; gap: 16px; font-size: .9rem; }
nav { display: grid; gap: 6px; }
nav a { padding: 10px 12px; text-decoration: none; border-radius: 6px; color: #43526a; }
nav a.router-link-exact-active { background: #eaf2ff; color: #205bab; font-weight: 600; }
.health-link { display: block; margin-top: 24px; font-size: .85rem; }

.brand {
  font-size: 1.35rem;
  font-weight: 700;
}

.subtitle,
.nav-note {
  color: #909399;
}

.content {
  display: grid;
  grid-template-columns: 230px minmax(0, 1fr);
  min-height: calc(100vh - 67px);
}

.sidebar {
  padding: 24px;
  background: #fff;
  border-right: 1px solid #e4e7ed;
}

.nav-title {
  margin-top: 0;
  font-weight: 600;
}

.nav-note {
  font-size: 0.85rem;
}

ul {
  padding-left: 20px;
  line-height: 2.2;
  color: #606266;
}

@media (max-width: 720px) {
  .topbar {
    display: block;
  }
  .account { margin-top: 16px; flex-wrap: wrap; }

  .content {
    grid-template-columns: 1fr;
  }

  .sidebar {
    border-right: 0;
    border-bottom: 1px solid #e4e7ed;
  }
}
</style>
