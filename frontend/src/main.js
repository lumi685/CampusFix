import { createApp } from 'vue'
import { ElCard, ElTag } from 'element-plus'
import 'element-plus/dist/index.css'

import App from './App.vue'
import router from './router/index.js'

createApp(App).use(router).use(ElCard).use(ElTag).mount('#app')
