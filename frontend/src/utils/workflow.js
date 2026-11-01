import { workflowText } from '../api/workflow.js'
const integer = n => Number.isSafeInteger(n) && n > 0 && n < Number.MAX_SAFE_INTEGER
export const transferStatuses = { PENDING: '待审批', APPROVED: '已批准', REJECTED: '已拒绝', SUPERSEDED: '已失效' }
function invalid() { throw new Error('服务器返回数据无效或超出安全整数范围，已禁止操作。') }
export function transferPage(data, managed = false) {
  if (!Array.isArray(data?.items) || !Number.isSafeInteger(data.total) || data.total < 0 || !integer(data.page) || !integer(data.page_size) || data.page_size > 100) invalid()
  const items = data.items.map(t => {
    if (!t || !['transfer_request_id','order_id','assignment_id','order_version'].every(k => integer(t[k])) || !Object.hasOwn(transferStatuses,t.status) || typeof t.reason !== 'string' || typeof t.created_at !== 'string' || typeof t.order_status !== 'string' || ![t.order_no,t.review_reason,t.resolved_at].every(v => v === null || typeof v === 'string') || (managed && (t.status !== 'PENDING' || !['PENDING_ACCEPT','IN_PROGRESS'].includes(t.order_status)))) invalid()
    return Object.fromEntries(['transfer_request_id','order_id','order_no','assignment_id','order_version','reason','status','created_at','order_status','review_reason','resolved_at'].map(k => [k,t[k]]))
  })
  if (new Set(items.map(t => t.transfer_request_id)).size !== items.length) invalid()
  return { items, page: data.page, page_size: data.page_size, total: data.total }
}
function receipt(d, c) {
  if (c.kind === 'transfer') {
    if (!integer(d?.transfer_request_id) || d.order_id !== c.orderId || d.status !== 'PENDING' || d.order_status !== c.status) invalid()
    return { transfer_request_id: d.transfer_request_id, order_id: d.order_id, status: d.status, order_status: d.order_status }
  }
  if (c.kind === 'decision') {
    const approve = c.decision === 'APPROVE'
    if (d?.transfer_request_id !== c.requestId || d.decision !== c.decision || d.order_status !== (approve ? 'PENDING_DISPATCH' : c.status) || d.version !== c.version + (approve ? 1 : 0) || d.assignment_ended !== approve) invalid()
    return { transfer_request_id: d.transfer_request_id, decision: d.decision, order_status: d.order_status, version: d.version, assignment_ended: d.assignment_ended }
  }
  if (!integer(d?.id) || d.order_id !== c.orderId || !integer(d.assignment_id) || d.type !== c.kind || d.order_status !== 'IN_PROGRESS' || d.order_version !== c.version || typeof d.created_at !== 'string' || !d.created_at.endsWith('Z')) invalid()
  return { id:d.id,order_id:d.order_id,assignment_id:d.assignment_id,type:d.type,order_status:d.order_status,order_version:d.order_version,created_at:d.created_at }
}
// Single explicit POST, never auto-retried; read-only reconciliation cannot
// promote an uncertain write to success. clear() invalidates late async results.
export function workflowFlow(api, authorized = () => false) {
  let generation = 0
  return {
    phase:'idle',context:null,note:'',message:'',result:null,verification:null,verifying:false,
    clear() { generation++;this.phase='idle';this.context=null;this.note='';this.message='';this.result=null;this.verification=null;this.verifying=false },
    open(context) {
      if (['submitting','unknown_result'].includes(this.phase) || !authorized()) return false
      if (!integer(context.orderId) || !integer(context.version) || !['PENDING_ACCEPT','IN_PROGRESS'].includes(context.status) || !['transfer','decision','ARRIVED','PROGRESS_NOTE'].includes(context.kind) || (['ARRIVED','PROGRESS_NOTE'].includes(context.kind) && context.status !== 'IN_PROGRESS') || (context.kind === 'decision' && (!integer(context.requestId) || !['APPROVE','REJECT'].includes(context.decision)))) invalid()
      this.clear();this.context={...context};this.phase='ready';return true
    },
    cancel() { if (this.phase === 'ready') this.clear() },
    async verify() {
      if (this.phase !== 'unknown_result' || this.verifying || !this.context || !authorized()) return false
      const current=generation;this.verifying=true
      try { const d=await api.verify({...this.context});if(current!==generation||!authorized())return false;this.verification=d;return true }
      catch(e) { if(current!==generation)return false;if([401,403,404].includes(e.status)){this.clear();this.phase='forbidden';this.message='权限或身份已变化，旧数据已清除。'}else this.message='结果仍未确认，只读核实失败；禁止自动重发。';return false }
      finally { if(current===generation)this.verifying=false }
    },
    async send() {
      if (this.phase === 'submitting') return null
      if (this.phase !== 'ready' || !this.context || !authorized()) throw new Error('请重新读取后人工确认，未知操作不得重发。')
      const c={...this.context};workflowText(this.note,c.kind==='PROGRESS_NOTE'?1000:500,c.kind==='ARRIVED')
      const current=generation,note=this.note;this.phase='submitting';this.message=''
      try { const d=receipt(await api.post(c,note),c);if(current!==generation||!authorized())return null;this.result=d;this.note='';this.phase='success';this.message='服务器已确认操作成功。';return d }
      catch(e) {
        if(current!==generation)return null
        this.result=null
        if(e.status===422){this.phase='ready';this.message='字段校验失败，已保留备注草稿，请修正后再次人工确认。'}
        else if([401,403,404].includes(e.status)){this.clear();this.phase='forbidden';this.message='身份、权限或资源已变化，旧数据已清除。'}
        else if(!e.status||e.status>=500){this.note='';this.phase='unknown_result';this.message='操作结果未确认，可能已提交；只读状态不能证明本次请求成功，禁止重发。';await this.verify()}
        else {this.context=null;this.note='';this.phase=e.status===409?'conflict':'error';this.message=e.status===409?'状态、版本或申请已变化（也可能重复到场）。旧操作已清除，请重新读取。':'操作未被接受，请重新读取。'}
        return null
      }
    },
  }
}
