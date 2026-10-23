<script setup>
defineProps({ notFound: Boolean })
</script>
<template>
  <main class="access-page">
    <h1>{{ notFound ? '页面不存在' : '无权访问此功能' }}</h1>
    <p>{{ notFound ? '请检查页面地址。' : '当前账号的角色或授权摘要不允许进入此页面。' }}</p>
    <RouterLink to="/home">返回身份首页</RouterLink>
  </main>
</template>
<style scoped>
.access-page { padding: 40px 32px; } h1 { font-size: 1.6rem; } p { color: #606266; }
</style>
