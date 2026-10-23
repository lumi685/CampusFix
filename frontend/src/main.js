import { createApp } from 'vue'
import { ElAlert, ElButton, ElCard, ElTag, ElForm, ElFormItem, ElInput } from 'element-plus'
import 'element-plus/dist/index.css'

import App from './App.vue'
import router from './router/index.js'

createApp(App).use(router).use(ElAlert).use(ElButton).use(ElCard).use(ElTag)
  .use(ElForm).use(ElFormItem).use(ElInput).mount('#app')
