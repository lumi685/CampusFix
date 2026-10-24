import { apiClient } from './client.js'

export async function chatWithReportAssistant(messages, signal) {
  const { data } = await apiClient.post('/ai/report-assistant/chat', { messages }, {
    authRequired: true, timeout: 185000, signal,
  })
  if (typeof data.data?.reply !== 'string' || !data.data.reply.trim()) {
    throw new Error('AI 回复格式不正确')
  }
  return data.data.reply
}

export async function generateReportDraft(messages, signal) {
  const { data } = await apiClient.post('/ai/report-assistant/draft', { messages }, {
    authRequired: true, timeout: 185000, signal,
  })
  return data.data
}
