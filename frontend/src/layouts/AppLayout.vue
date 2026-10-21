<template>
  <div class="app-layout">
    <header class="topbar">
      <span class="brand">CampusFix</span>
      <span class="subtitle">校园维修工单与智能调度系统</span>
    </header>

    <div class="content">
      <aside class="sidebar" aria-label="未来角色导航">
        <p class="nav-title">角色导航 · 占位</p>
        <p class="nav-note">业务入口将在后续阶段开放</p>
        <ul>
          <li>报修人</li>
          <li>维修人员</li>
          <li>调度管理员</li>
          <li>系统管理员</li>
        </ul>
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

  .content {
    grid-template-columns: 1fr;
  }

  .sidebar {
    border-right: 0;
    border-bottom: 1px solid #e4e7ed;
  }
}
</style>
