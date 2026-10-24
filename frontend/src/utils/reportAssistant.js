export const dataDisclosure = '你的故障描述及本次对话上下文将发送至所配置的 AI 服务提供方，用于生成回复。请勿输入密码、验证码或不必要的个人信息。'

export function messageLength(text) {
  if (typeof text !== 'string' || !text.trim() || /^[\s\u200b\ufeff]*$/u.test(text)) throw new Error('请先描述故障')
  for (const c of text) {
    const n = c.codePointAt(0)
    if ((n >= 0xd800 && n <= 0xdfff) || n === 0 || (n < 32 && ![9, 10, 13].includes(n)) || n === 127) {
      throw new Error('内容包含不支持的字符')
    }
  }
  return Array.from(text).length
}

function conversationContext(messages, allowAssistantEnd) {
  const result = messages.map(({ role, content }) => ({ role, content }))
  for (const m of result) {
    if (!['user', 'assistant'].includes(m.role) || messageLength(m.content) > 1500) throw new Error('每条消息最多 1500 个字符')
  }
  if (!result.length || (!allowAssistantEnd && result.at(-1).role !== 'user')) throw new Error('请先描述故障')
  // Keep a valid recent conversation, never cut the current message or the system prompt.
  while (result.length && (result.length > 12 || result[0]?.role !== 'user' ||
      result.reduce((sum, m) => sum + Array.from(m.content).length, 0) > 9000)) result.shift()
  if (!result.length) throw new Error('当前保留的上下文缺少可用用户描述，请补充故障后再生成')
  return result
}
export function chatContext(messages) { return conversationContext(messages, false) }
export function draftContext(messages) { return conversationContext(messages, true) }

export function assistantError(error) {
  const messages = {
    AI_NOT_CONFIGURED: 'AI 服务尚未配置，请使用普通报修。',
    AI_UPSTREAM_TIMEOUT: 'AI 回复超时；没有收到有效回复，你可以手动重试或普通报修。',
    AI_PROVIDER_RATE_LIMITED: '模型服务繁忙，请稍后手动重试。',
    AI_PROVIDER_AUTH_FAILED: '模型服务配置异常，请联系平台维护人员或普通报修。',
    AI_INVALID_RESPONSE: '模型未返回完整有效的文字回复，请手动重试或普通报修。',
    AI_DRAFT_INVALID_RESPONSE: '模型未返回有效的报修建议，请手动重新生成或普通报修。',
    AI_RESPONSE_TOO_LARGE: '模型回复超出限制，请手动重试或普通报修。',
    AI_REQUEST_TOO_LARGE: '对话内容超出限制，请缩短内容或清空对话。',
    AI_VALIDATION_ERROR: '对话内容不符合要求，请检查输入。',
    AI_RATE_LIMITED: '本会话或平台 AI 请求额度已用尽，请稍后再试或普通报修。',
    AI_REQUEST_IN_PROGRESS: '当前会话已有请求处理中，请稍后手动重试。',
    AI_BUSY: 'AI 服务当前并发已满，请稍后手动重试。',
  }
  let text = error?.status === 401 ? '登录已失效，请重新登录。'
    : error?.status === 403 ? '当前账号无权使用 AI 辅助报修。'
      : messages[error?.code] || 'AI 请求失败或连接已断开；没有收到有效回复，请手动重试或普通报修。'
  if (/^req_[\w-]{1,100}$/.test(error?.requestId || '')) text += ` 请求编号：${error.requestId}`
  return text
}

// Page-scoped state, deliberately no persistence. Tests inject only the transport.
export function reportAssistantFlow(request, draftRequest) {
  let epoch = 0, abort, draftEpoch = 0, draftAbort
  return {
    consent: false, input: '', messages: [], busy: false, error: '', failed: false,
    draft: null, draftBusy: false, draftStale: false, draftError: '', copyNotice: '',
    get canGenerateDraft() {
      return this.consent && !this.busy && !this.failed && !this.draftBusy &&
        this.messages.some((m) => m.role === 'user' && typeof m.content === 'string' && m.content.trim())
    },
    get canUseDraft() {
      if (!this.draft || this.draftStale || this.busy || this.draftBusy || this.failed || this.error || this.draftError) return false
      if (Object.keys(this.draft).length !== 6 || [...draftFields.map(([key]) => key), 'missing_information'].some((key) => !Object.hasOwn(this.draft, key))) return false
      try { draftText(this.draft); return true } catch { return false }
    },
    invalidateDraft() {
      ++draftEpoch; draftAbort?.abort(); draftAbort = undefined
      this.draftBusy = false; this.draftStale = !!this.draft; this.draftError = ''; this.copyNotice = ''
    },
    cancelDraft() {
      this.invalidateDraft(); this.draft = null; this.draftStale = false
    },
    editDraft(value) { this.draft = value; this.copyNotice = '' },
    async generateDraft() {
      if (!this.canGenerateDraft) return
      this.invalidateDraft()
      const current = ++draftEpoch
      this.draftBusy = true; draftAbort = new AbortController()
      try {
        const result = checkedDraft(await draftRequest(draftContext(this.messages), draftAbort.signal))
        if (current !== draftEpoch) return
        this.draft = result; this.draftStale = false
      } catch (error) { if (current === draftEpoch) this.draftError = assistantError(error) }
      finally { if (current === draftEpoch) { this.draftBusy = false; draftAbort = undefined } }
    },
    async copyDraft(writeText) {
      if (!this.draft || this.draftBusy || this.draftStale || this.busy) return
      const current = draftEpoch, snapshot = this.draft
      this.copyNotice = ''
      try {
        await writeText(draftText(snapshot))
        if (current === draftEpoch && snapshot === this.draft) this.copyNotice = '已复制当前编辑后的建议；尚未创建或提交工单。'
      } catch {
        if (current === draftEpoch && snapshot === this.draft) this.copyNotice = '复制失败，建议仍保留，请手动选中文字复制。'
      }
    },
    async send() {
      if (this.busy || this.failed || this.draftBusy) return
      this.error = ''
      try {
        if (!this.consent) throw new Error('请先阅读并同意第三方数据告知')
        if (messageLength(this.input) > 1500) throw new Error('每条消息最多 1500 个字符')
      } catch (error) { this.error = error.message; return }
      this.invalidateDraft()
      this.messages.push({ role: 'user', content: this.input })
      this.messages = this.messages.slice(-12)
      this.input = ''
      await this.execute()
    },
    async retry() {
      if (this.busy || this.draftBusy || !this.failed || !this.consent) return
      await this.execute()
    },
    async execute() {
      this.invalidateDraft()
      this.busy = true; this.failed = false; this.error = ''
      const current = ++epoch
      abort = new AbortController()
      try {
        const context = chatContext(this.messages)
        const reply = await request(context, abort.signal)
        if (current !== epoch) return
        if (messageLength(reply) > 1500) throw new Error('Invalid model reply')
        this.messages.push({ role: 'assistant', content: reply })
        this.messages = this.messages.slice(-12)
      } catch (error) {
        if (current === epoch) { this.error = assistantError(error); this.failed = true }
      } finally { if (current === epoch) { this.busy = false; abort = undefined } }
    },
    editFailed() {
      if (this.busy || !this.failed) return
      this.input = this.messages.pop()?.content || ''
      this.failed = false; this.error = ''
    },
    clear() {
      this.cancelDraft()
      ++epoch; abort?.abort(); abort = undefined
      this.messages = []; this.input = ''; this.error = ''; this.failed = false; this.busy = false; this.consent = false
    },
  }
}

export const draftFields = [
  ['title', '故障标题', 120], ['description', '故障描述', 2000],
  ['category_hint', '建议分类（仅文本，非正式分类）', 120], ['location_hint', '位置线索（非正式位置）', 200],
  ['safety_note', '安全提示', 300],
]
export function checkedDraft(data) {
  if (!data || data.requires_review !== true || !data.draft || Array.isArray(data.draft) ||
      Object.keys(data.draft).length !== 6) throw new Error('Invalid draft response')
  const result = {}
  for (const [field, , max] of draftFields) {
    const value = data.draft[field]
    if (value !== null && (typeof value !== 'string' || messageLength(value) > max)) throw new Error('Invalid draft response')
    result[field] = value === null ? '' : value
  }
  const missing = data.draft.missing_information
  if (!Array.isArray(missing) || missing.length > 5 || missing.some((text) => messageLength(text) > 120)) throw new Error('Invalid draft response')
  result.missing_information = [...missing]
  return result
}
export function draftText(draft) {
  const lines = ['AI 建议，尚未创建或提交工单；请自行核对。']
  for (const [field, label, max] of draftFields) {
    const value = draft[field]
    if (typeof value !== 'string' || (value !== '' && messageLength(value) > max)) throw new Error('Invalid edited field')
    lines.push(`${label}：${value || '（未填写）'}`)
  }
  if (!Array.isArray(draft.missing_information) || draft.missing_information.length > 5) throw new Error('Invalid missing information')
  const missing = draft.missing_information.filter((text) => text !== '')
  if (missing.some((text) => messageLength(text) > 120)) throw new Error('Invalid missing information')
  lines.push(`待补充信息：${missing.length ? missing.join('；') : '（无）'}`)
  return lines.join('\n')
}
