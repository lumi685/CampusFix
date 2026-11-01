import { ownsReportedOrder } from './orderSupplement.js'
import { workflowText } from '../api/workflow.js'
const integer=n=>Number.isSafeInteger(n)&&n>0&&n<Number.MAX_SAFE_INTEGER
const restricted=e=>[401,403,404].includes(e.status)
function unavailable(){throw new Error('本轮维修结果或本人归属尚未可靠确认，请重新读取，不会开放验收。')}
function detail(d,id,version){if(d?.id!==id||d.status!=='PENDING_VERIFY'||!integer(d.version)||(version!=null&&d.version!==version))unavailable();return d}
// Bounded complete public history, fenced by real order versions. Never guess
// the last result from page one or use private metadata / client role as ownership.
export async function verificationContext(id,role,api) {
  if(!integer(id)||!['reporter','worker'].includes(role))unavailable()
  let ownTotal
  const owned=await ownsReportedOrder(id,role,async params=>{
    const p=await api.reported(params)
    if(!Array.isArray(p?.items)||!Number.isSafeInteger(p.total)||p.total<0||p.total>5000||p.page!==params.page||p.page_size!==100||p.items.length!==Math.min(100,Math.max(0,p.total-(params.page-1)*100))||p.items.some(x=>!integer(x.id))||(ownTotal!=null&&p.total!==ownTotal))unavailable()
    ownTotal=p.total;return p
  })
  if(!owned)unavailable()
  const before=detail(await api.detail(id),id),items=[],seen=new Set();let total,previous,latest=null,reworks=0
  for(let page=1;page<=50;page++){
    const p=await api.timeline(id,{page,page_size:100})
    if(!Array.isArray(p?.items)||!Number.isSafeInteger(p.total)||p.total<1||p.total>5000||p.page!==page||p.page_size!==100||(total!=null&&p.total!==total)||p.items.length!==Math.min(100,p.total-(page-1)*100))unavailable()
    total=p.total
    for(const e of p.items){
      if(!integer(e.id)||seen.has(e.id)||typeof e.created_at!=='string'||!Number.isFinite(Date.parse(e.created_at))||!e.created_at.endsWith('Z')||(previous&&(Date.parse(e.created_at)<Date.parse(previous.created_at)||(Date.parse(e.created_at)===Date.parse(previous.created_at)&&e.id<=previous.id))))unavailable()
      seen.add(e.id)
      // Allowlist public fields only; never retain internal/raw DTO fields.
      const item=Object.fromEntries(['id','action','from_status','to_status','public_note','created_at'].map(k=>[k,e[k]]));item.actor=e.actor&&integer(e.actor.id)&&typeof e.actor.display_name==='string'?{id:e.actor.id,display_name:e.actor.display_name}:null;items.push(item);previous=item
      if(e.action==='ORDER_COMPLETED'){if(latest||e.from_status!=='IN_PROGRESS'||e.to_status!=='PENDING_VERIFY')unavailable();workflowText(e.public_note,2000);latest=item}
      if(e.action==='ORDER_REWORK_REQUESTED'){if(!latest||e.from_status!=='PENDING_VERIFY'||e.to_status!=='IN_PROGRESS')unavailable();workflowText(e.public_note,500);latest=null;reworks++}
    }
    if(page*100>=total)break
    if(page===50)unavailable()
  }
  detail(await api.detail(id),id,before.version)
  if(!latest||items.at(-1)?.id!==latest.id||items.length!==total)unavailable()
  return {orderId:id,version:before.version,reworks,result:latest,items}
}
export function verifyFlow(api,authorized=()=>false) {
  let generation=0
  return {
    phase:'idle',context:null,approved:null,reason:'',message:'',receipt:null,verification:null,verifying:false,
    clear(){generation++;this.phase='idle';this.context=null;this.approved=null;this.reason='';this.message='';this.receipt=null;this.verification=null;this.verifying=false},
    async load(id,role){if(['submitting','unknown_result'].includes(this.phase))return false;this.clear();const current=generation;if(!authorized())return false;this.phase='loading';try{const c=await verificationContext(id,role,api);if(current!==generation||!authorized())return false;this.context=c;this.phase='ready';return true}catch(e){if(current!==generation)return false;this.clear();this.phase=restricted(e)?'forbidden':'error';this.message=restricted(e)?'身份或权限变化，旧数据已清除。':'归属或本轮结果核验失败，请只读重新获取。';return false}},
    choose(approved){if(this.phase!=='ready'||!this.context||!authorized()||typeof approved!=='boolean')return false;this.approved=approved;this.reason='';this.message='';return true},
    cancel(){if(this.phase==='ready'){this.approved=null;this.reason=''}},
    async reconcile(){if(this.phase!=='unknown_result'||this.verifying||!this.context||!authorized())return false;const current=generation;this.verifying=true;try{const d=await api.detail(this.context.orderId);if(current!==generation||!authorized())return false;if(d?.id!==this.context.orderId||!integer(d.version)||typeof d.status!=='string')unavailable();this.verification={id:d.id,status:d.status,version:d.version};return true}catch(e){if(current!==generation)return false;if(restricted(e)){this.clear();this.phase='forbidden';this.message='无法合法核实，旧资料已清除。'}else this.message='只读核实失败，结果仍未知；禁止重发。';return false}finally{if(current===generation)this.verifying=false}},
    async send(){
      if(this.phase==='submitting')return null
      if(this.phase!=='ready'||!this.context||typeof this.approved!=='boolean'||!authorized())throw new Error('请重新读取并人工确认；未知操作不能重发。')
      const c=this.context,approved=this.approved,reason=this.reason;workflowText(reason,500,approved);const current=generation;this.phase='submitting';this.message=''
      try{
        const d=await api.post(c.orderId,c.version,approved,reason)
        if(current!==generation||!authorized())return null
        if(d?.order_id!==c.orderId||d.approved!==approved||d.status!==(approved?'COMPLETED':'IN_PROGRESS')||d.version!==c.version+1||!Number.isSafeInteger(d.rework_count)||d.rework_count!==c.reworks+(approved?0:1))unavailable()
        this.phase='success';this.reason='';this.approved=null;this.receipt={order_id:d.order_id,approved:d.approved,status:d.status,version:d.version,rework_count:d.rework_count};this.message='服务器已确认验收操作成功，请查看重新读取的详情及时间线。';return this.receipt
      }catch(e){
        if(current!==generation)return null
        if(e.status===422){this.phase='ready';this.message='返工原因校验失败，已保留草稿，请修正。'}
        else if(restricted(e)){this.clear();this.phase='forbidden';this.message='身份、权限或资源变化，旧资料已清除。'}
        else if(!e.status||e.status>=500){this.reason='';this.approved=null;this.phase='unknown_result';this.message='验收结果未知，可能已经写入；仅可只读核实，不能证明本次POST成功，禁止重发。';await this.reconcile()}
        else{this.context=null;this.approved=null;this.reason='';this.phase=e.status===409?'conflict':'error';this.message='状态或版本变化，旧操作已清除，请重新获取。'}
        return null
      }
    },
  }
}
