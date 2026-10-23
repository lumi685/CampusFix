// Component-owned state: no persistence, tokens, or identity supplied by the form.
export function reportSubmission(api) {
  return {
    draft: null, busy: false, uncertain: false,
    async send(payload) {
      if (this.busy) return null
      if (this.uncertain) throw new Error('创建结果尚未确认，请先到“我提交的报修”核查，不可直接重建。')
      this.busy = true
      try {
        if (!this.draft) {
          let created
          try { created = await api.create(payload) } catch (error) {
            // A transport/5xx response cannot prove that a write was not committed.
            if (!error.status || error.status >= 500) this.uncertain = true
            throw error
          }
          if (!Number.isSafeInteger(created?.id) || !Number.isSafeInteger(created?.version) || created.id <= 0 || created.version <= 0 || created.status !== 'DRAFT') {
            this.uncertain = true
            throw new Error('创建响应不完整，请先查询本人报修列表核查结果。')
          }
          this.draft = { id: created.id, version: created.version }
        }
        try { return await api.submit(this.draft.id, this.draft.version) } catch (error) {
          // Recover a lost submit response or a version conflict by reading this same resource.
          if (error.status === 409 || !error.status || error.status >= 500) {
            try {
              const current = await api.detail(this.draft.id)
              if (current.status !== 'DRAFT' && current.status !== 'PENDING_SUPPLEMENT' && current.order_no && current.submitted_at) return current
              if (Number.isSafeInteger(current.version) && current.version > 0) this.draft.version = current.version
            } catch { /* Keep the known draft; never silently create another. */ }
          }
          throw error
        }
      } finally { this.busy = false }
    },
  }
}
