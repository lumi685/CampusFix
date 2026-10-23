import { createApp } from 'vue'
import {
  ElAlert, ElButton, ElCard, ElTag, ElForm, ElFormItem, ElInput,
  ElSelect, ElOption, ElCascader, ElDatePicker,
  ElTable, ElTableColumn, ElPagination, ElDescriptions, ElDescriptionsItem,
  ElTimeline, ElTimelineItem, ElEmpty, ElResult,
} from 'element-plus'
import 'element-plus/dist/index.css'

import App from './App.vue'
import router from './router/index.js'

createApp(App).use(router).use(ElAlert).use(ElButton).use(ElCard).use(ElTag)
  .use(ElForm).use(ElFormItem).use(ElInput)
  .use(ElSelect).use(ElOption).use(ElCascader).use(ElDatePicker)
  .use(ElTable).use(ElTableColumn).use(ElPagination).use(ElDescriptions).use(ElDescriptionsItem)
  .use(ElTimeline).use(ElTimelineItem).use(ElEmpty).use(ElResult).mount('#app')
