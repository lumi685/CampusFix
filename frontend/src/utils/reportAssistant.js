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

export function chatContext(messages) {
  const result = messages.map(({ role, content }) => ({ role, content }))
  for (const m of result) {
    if (!['user', 'assistant'].includes(m.role) || messageLength(m.content) > 1500) throw new Error('每条消息最多 1500 个字符')
  }
  if (!result.length || result.at(-1).role !== 'user') throw new Error('请先描述故障')
  // Keep a valid recent conversation, never cut the current message or the system prompt.
  while (result.length > 12 || result[0]?.role !== 'user' ||
      result.reduce((sum, m) => sum + Array.from(m.content).length, 0) > 9000) result.shift()
  return result
}

export function assistantError(error) {
  const messages = {
    AI_NOT_CONFIGURED: 'AI 服务尚未配置，请使用普通报修。',
    AI_UPSTREAM_TIMEOUT: 'AI 回复超时；没有收到有效回复，你可以手动重试或普通报修。',
    AI_PROVIDER_RATE_LIMITED: '模型服务繁忙，请稍后手动重试。',
    AI_PROVIDER_AUTH_FAILED: '模型服务配置异常，请联系平台维护人员或普通报修。',
    AI_INVALID_RESPONSE: '模型未返回完整有效的文字回复，请手动重试或普通报修。',
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
export function reportAssistantFlow(request) {
  let epoch = 0, abort
  return {
    consent: false, input: '', messages: [], busy: false, error: '', failed: false,
    async send() {
      if (this.busy || this.failed) return
      this.error = ''
      try {
        if (!this.consent) throw new Error('请先阅读并同意第三方数据告知')
        if (messageLength(this.input) > 1500) throw new Error('每条消息最多 1500 个字符')
      } catch (error) { this.error = error.message; return }
      this.messages.push({ role: 'user', content: this.input })
      this.messages = this.messages.slice(-12)
      this.input = ''
      await this.execute()
    },
    async retry() {
      if (this.busy || !this.failed || !this.consent) return
      await this.execute()
    },
    async execute() {
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
      ++epoch; abort?.abort(); abort = undefined
      this.messages = []; this.input = ''; this.error = ''; this.failed = false; this.busy = false; this.consent = false
    },
  }
}
