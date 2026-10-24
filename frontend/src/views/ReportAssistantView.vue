<script setup>
import { reactive, ref, watch, nextTick, onMounted, onBeforeUnmount } from 'vue'
import { authState } from '../stores/auth.js'
import { chatWithReportAssistant, generateReportDraft } from '../api/reportAssistant.js'
import AiReportDraftPreview from '../components/AiReportDraftPreview.vue'
import { reportAssistantFlow, dataDisclosure } from '../utils/reportAssistant.js'

const flow = reactive(reportAssistantFlow(chatWithReportAssistant, generateReportDraft))
const transcript = ref(null)
watch(() => [flow.messages.length, flow.busy], async () => {
  await nextTick()
  if (transcript.value) transcript.value.scrollTop = transcript.value.scrollHeight
})
watch(() => authState.status, (status) => { if (status !== 'authenticated') flow.clear() })
function keydown(event) {
  if (event.key === 'Enter' && !event.shiftKey && !event.isComposing && event.keyCode !== 229) {
    event.preventDefault()
    flow.send()
  }
}
const clear = () => flow.clear()
const copy = () => flow.copyDraft((text) => navigator.clipboard.writeText(text))
onMounted(() => window.addEventListener('pagehide', clear))
onBeforeUnmount(() => { window.removeEventListener('pagehide', clear); flow.clear() })
</script>

<template>
  <main class="assistant-page">
    <header class="assistant-heading">
      <div><h1>AI 辅助报修</h1><p>先描述问题，AI 会协助排查</p></div>
      <RouterLink to="/report/new"><el-button>前往普通报修</el-button></RouterLink>
    </header>
    <el-alert title="AI 建议不能替代专业维修判断。冒烟、电火花、漏电、燃气泄漏或其他危险情况，请停止操作、远离危险源并立即寻求专业帮助。" type="warning" :closable="false" show-icon />
    <p class="retention">对话仅在当前页面临时保留；刷新、离开页面、退出或清空后会丢失。长对话仅发送最近最多12条、合计9000字以内的有效上下文。AI 不会自动创建、填写或提交工单。</p>
    <section ref="transcript" class="transcript" aria-label="当前对话" aria-live="polite" :aria-busy="flow.busy">
      <p v-if="!flow.messages.length" class="empty">例如：宿舍空调不制冷了。请勿输入密码、验证码或不必要的个人信息。</p>
      <article v-for="(message, index) in flow.messages" :key="index" class="message" :class="message.role">
        <strong>{{ message.role === 'user' ? '你' : 'CampusFix AI' }}</strong>
        <p>{{ message.content }}</p>
      </article>
      <p v-if="flow.busy" role="status">正在思考…</p>
    </section>
    <el-alert v-if="flow.error" :title="flow.error" type="error" :closable="false" show-icon />
    <div v-if="flow.failed" class="actions">
      <el-button :disabled="flow.busy || !flow.consent" @click="flow.retry()">重试本次发送</el-button>
      <el-button :disabled="flow.busy" @click="flow.editFailed()">修改后再发送</el-button>
      <span>不会自动重试，也不会自动提交报修。</span>
    </div>
    <label class="disclosure"><input v-model="flow.consent" type="checkbox" :disabled="flow.busy || flow.draftBusy">我已阅读并同意：{{ dataDisclosure }}</label>
    <label for="assistant-input" class="input-label">描述故障（每条最多1500个字符）</label>
    <el-input id="assistant-input" v-model="flow.input" type="textarea" :rows="4" :disabled="flow.busy || flow.failed || flow.draftBusy" placeholder="描述故障；Enter发送，Shift+Enter换行" @keydown="keydown" />
    <div class="actions">
      <el-button type="primary" :loading="flow.busy" :disabled="!flow.consent || flow.busy || flow.failed || flow.draftBusy || !flow.input.trim()" @click="flow.send()">发送</el-button>
      <el-button :loading="flow.draftBusy" :disabled="!flow.canGenerateDraft" @click="flow.generateDraft()">生成报修建议</el-button>
      <el-button @click="flow.clear()">清空对话</el-button>
      <small>清空仅清除本地上下文，不能撤回已经发送给服务商的数据或费用。</small>
    </div>
    <p class="retention">建议基于当前保留的对话内容生成，请检查是否遗漏较早的信息。输入框中尚未发送的文字不参与整理。</p>
    <p v-if="flow.draftBusy" role="status">正在整理报修建议…</p>
    <el-alert v-if="flow.draftError" :title="flow.draftError" type="error" :closable="false" />
    <el-button v-if="flow.draftBusy" @click="flow.cancelDraft()">取消等待建议</el-button>
    <AiReportDraftPreview v-if="flow.draft" :model-value="flow.draft" :stale="flow.draftStale" :busy="flow.busy || flow.draftBusy" :can-generate="flow.canGenerateDraft" :copy-notice="flow.copyNotice" @update:model-value="flow.editDraft($event)" @copy="copy" @regenerate="flow.generateDraft()" @cancel="flow.cancelDraft()" />
  </main>
</template>

<style scoped>
.assistant-page { width: 100%; max-width: 1000px; min-width: 0; margin: 0 auto; padding: 24px; }
.assistant-heading { display: flex; align-items: center; justify-content: space-between; gap: 16px; }
h1 { margin: 0; font-size: 1.5rem; }
.retention, .empty, small, .actions span { color: #606266; font-size: .85rem; line-height: 1.6; }
.transcript { margin: 16px 0; padding: 16px; height: min(48vh, 440px); min-height: 200px; overflow-y: auto; background: white; border: 1px solid #dcdfe6; border-radius: 8px; }
.message { padding: 12px 16px; margin-bottom: 12px; border-radius: 8px; max-width: 92%; background: #f1f4f8; overflow-wrap: anywhere; }
.message.user { margin-left: auto; background: #eaf2ff; }
.message p { margin: 8px 0 0; white-space: pre-wrap; line-height: 1.7; }
.disclosure { display: block; font-size: .9rem; line-height: 1.7; margin: 14px 0; cursor: pointer; }
.disclosure input { margin-right: 8px; }
.input-label { display: block; font-size: .9rem; margin: 10px 0; }
.actions { display: flex; flex-wrap: wrap; align-items: center; gap: 10px; margin-top: 12px; }
.actions :deep(.el-button + .el-button) { margin-left: 0; }
@media (max-width: 480px) { .assistant-page { padding: 16px; } .assistant-heading { align-items: flex-start; flex-direction: column; margin-bottom: 16px; } .message { max-width: 100%; } }
</style>
