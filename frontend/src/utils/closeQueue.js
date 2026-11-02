import { workflowText } from '../api/workflow.js'
const restricted=e=>[401,403,404].includes(e.status)
export function closeQueue(api,authorized=()=>false){let generation=0,read=0
  return {
    items:[],page:1,total:0,busy:false,phase:'idle',selection:null,text:'',confirmation:null,message:'',verification:null,
    clear(){generation++;read++;this.items=[];this.page=1;this.total=0;this.busy=false;this.phase='idle';this.selection=null;this.text='';this.confirmation=null;this.message='';this.verification=null},
    async load(){if(this.phase==='submitting'||!authorized())return false;const g=generation,r=++read;this.busy=true;this.items=[];this.total=0;this.selection=null;this.confirmation=null;this.text=''
      try{const d=await api.list({page:this.page,page_size:20});if(g!==generation||r!==read||!authorized())return false
        if(this.phase==='unknown_result'){this.verification={visible_ids:d.items.map(x=>x.id),total:d.total};return true}
        this.items=d.items;this.total=d.total;return true
      }catch(e){if(g!==generation||r!==read)return false;if(restricted(e)){this.clear();this.phase='forbidden';this.message='身份或关闭授权变化，旧队列已清除。'}else this.message='读取失败，请重试；不会扩大范围或重放关闭。';return false}finally{if(g===generation&&r===read)this.busy=false}
    },
    select(row){if(!row||this.busy||['submitting','unknown_result','forbidden'].includes(this.phase)||!authorized())return false;const actual=this.items.find(x=>x.id===row.id&&x.version===row.version);if(!actual)return false;this.selection={...actual};this.text='';this.confirmation=null;this.phase='ready';return true},
    choose(){if(this.phase!=='ready'||!this.selection||!authorized())return false;workflowText(this.text,500,true);this.confirmation={...this.selection,note:this.text};return true},
    cancel(){if(this.phase==='ready')this.confirmation=null},
    async send(){if(this.phase==='submitting')return null;if(this.phase!=='ready'||!this.confirmation||!authorized())throw new Error('请选择真实队列记录并人工确认，未知结果不得重发。');const g=generation,c={...this.confirmation};this.phase='submitting'
      try{const d=await api.post(c.id,c.version,c.note);if(g!==generation||!authorized())return null;if(d?.order_id!==c.id||d.status!=='CLOSED'||d.version!==c.version+1||typeof d.closed_at!=='string'||!d.closed_at.endsWith('Z')||!Number.isFinite(Date.parse(d.closed_at)))throw new Error('关闭回执无效')
        this.selection=null;this.confirmation=null;this.text='';this.phase='idle';this.message='服务器已确认关闭，正在重新查询真实队列。';await this.load();return g===generation&&authorized()?d:null
      }catch(e){if(g!==generation)return null
        if(e.status===422){this.phase='ready';this.confirmation=null;this.message='说明校验失败，已保留输入，请修正。'}
        else if(restricted(e)){this.clear();this.phase='forbidden';this.message='身份或关闭权限变化，旧队列已清除。'}
        else if(!e.status||e.status>=500){this.phase='unknown_result';this.items=[];this.total=0;this.selection=null;this.confirmation=null;this.text='';this.message='关闭结果未知。仅可只读核实；队列中消失不证明本次POST成功，禁止重发。';await this.load()}
        else{this.phase='conflict';this.selection=null;this.confirmation=null;this.text='';this.message='状态或版本冲突，旧选择已清除，已重新查询；请人工重新选择。';await this.load()}return null
      }
    },
  }
}
