import { ownsReportedOrder } from './orderSupplement.js'
import { workflowText } from '../api/workflow.js'
import { validEvaluation } from '../api/evaluations.js'
const integer=n=>Number.isSafeInteger(n)&&n>0&&n<Number.MAX_SAFE_INTEGER
const denied=e=>[401,403,404].includes(e.status)
function invalid(){throw new Error('真实工单、归属或回执尚未确认，请只读重新获取。')}
function detail(d,id){if(d?.id!==id||!integer(d.version)||typeof d.status!=='string')invalid();return {id:d.id,version:d.version,status:d.status}}
function rating(d){if(!d||!Object.hasOwn(d,'evaluation')||(d.evaluation!==null&&!validEvaluation(d.evaluation)))invalid();return d.evaluation===null?null:Object.fromEntries(['id','score','comment','created_at'].map(k=>[k,d.evaluation[k]]))}
async function own(id,role,api){
  if(!['reporter','worker'].includes(role))return false
  let total;const seen=new Set()
  return ownsReportedOrder(id,role,async q=>{const p=await api.reported(q)
    if(!Array.isArray(p?.items)||!Number.isSafeInteger(p.total)||p.total<0||p.total>5000||p.page!==q.page||p.page_size!==100||(total!=null&&total!==p.total)||p.items.length!==Math.min(100,Math.max(0,p.total-(q.page-1)*100)))invalid()
    total=p.total;for(const x of p.items){if(!integer(x.id)||seen.has(x.id))invalid();seen.add(x.id)}return p
  })
}
// ActionRead and original-reporter write eligibility remain deliberately separate.
// A successful reconciliation is current read evidence, never a POST receipt.
export function terminalFlow(api,authorized=()=>false,closing=false){
  let generation=0
  return {
    phase:'idle',context:null,evaluation:null,score:5,text:'',confirmation:null,message:'',receipt:null,verification:null,verifying:false,
    clear(){generation++;this.phase='idle';this.context=null;this.evaluation=null;this.score=5;this.text='';this.confirmation=null;this.message='';this.receipt=null;this.verification=null;this.verifying=false},
    async load(id,role){
      if(['submitting','unknown_result'].includes(this.phase))return false
      this.clear();const g=generation;if(!authorized()||!integer(id))return false;this.phase='loading'
      try{const before=detail(await api.detail(id),id);const e=closing?null:rating(await api.read(id));let allowed=before.status==='COMPLETED'&&(closing||e===null&&await own(id,role,api));const after=detail(await api.detail(id),id);if(after.status!==before.status||after.version!==before.version)invalid();if(g!==generation||!authorized())return false;this.context=before;this.evaluation=e;this.phase=allowed?'ready':'readonly';return true}
      catch(e){if(g!==generation)return false;this.clear();this.phase=denied(e)?'forbidden':'error';this.message=denied(e)?'身份、权限或资源已变化，旧资料已清除。':'读取或归属核验失败，不开放写操作。';return false}
    },
    choose(){if(this.phase!=='ready'||!this.context||!authorized())return false;if(!closing&&(!Number.isInteger(this.score)||this.score<1||this.score>5))throw new Error('请选择1～5整数评分');workflowText(this.text,closing?500:1000,true);this.confirmation={score:this.score,text:this.text};return true},
    cancel(){if(this.phase==='ready')this.confirmation=null},
    async reconcile(){if(this.phase!=='unknown_result'||this.verifying||!this.context||!authorized())return false;const g=generation;this.verifying=true;try{const d=closing?detail(await api.detail(this.context.id),this.context.id):rating(await api.read(this.context.id));if(g!==generation||!authorized())return false;this.verification=closing?d:{evaluation:d};return true}catch(e){if(g!==generation)return false;if(denied(e)){this.clear();this.phase='forbidden';this.message='无法授权核实，旧资料已清除。'}else this.message='只读核实失败，原请求结果仍未知；禁止重发。';return false}finally{if(g===generation)this.verifying=false}},
    async send(){
      if(this.phase==='submitting')return null
      if(this.phase!=='ready'||!this.context||!this.confirmation||!authorized())throw new Error('请重新读取并人工确认；禁止重放未知请求。')
      const g=generation,c=this.context,f={...this.confirmation};this.phase='submitting';this.message=''
      try{const r=closing?await api.post(c.id,c.version,f.text):await api.post(c.id,c.version,f.score,f.text);if(g!==generation||!authorized())return null
        if(closing?!(r?.order_id===c.id&&r.status==='CLOSED'&&r.version===c.version+1&&typeof r.closed_at==='string'&&r.closed_at.endsWith('Z')&&Number.isFinite(Date.parse(r.closed_at))):!(r?.order_id===c.id&&validEvaluation(r)&&r.score===f.score&&r.comment===f.text))invalid()
        this.receipt=r;this.confirmation=null;this.text='';this.phase='success';this.message='服务器已确认成功，将重新读取真实资料。';return r
      }catch(e){if(g!==generation)return null
        if(e.status===422){this.phase='ready';this.confirmation=null;this.message='字段校验失败，已保留输入，请修正后重新确认。'}
        else if(denied(e)){this.clear();this.phase='forbidden';this.message='身份、权限或资源变化，旧资料已清除。'}
        else if(!e.status||e.status>=500){this.confirmation=null;this.text='';this.phase='unknown_result';this.message='操作结果未知，可能已提交。只读查询不能证明本次POST成功；禁止重发。';await this.reconcile()}
        else{this.context=null;this.evaluation=null;this.confirmation=null;this.text='';this.phase=e.status===409?'conflict':'error';this.message='状态、版本或重复操作冲突，旧操作已清除，请重新读取。'}return null
      }
    },
  }
}
export const evaluationFlow=(api,authorized)=>terminalFlow(api,authorized,false)
export const closeFlow=(api,authorized)=>terminalFlow(api,authorized,true)
