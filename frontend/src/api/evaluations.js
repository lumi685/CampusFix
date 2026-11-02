import { apiClient } from './client.js'
import { orderId } from './orders.js'
import { workflowText } from './workflow.js'
const integer=n=>Number.isSafeInteger(n)&&n>0&&n<Number.MAX_SAFE_INTEGER
const utc=s=>typeof s==='string'&&/^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(?:\.\d{1,9})?Z$/.test(s)&&Number.isFinite(Date.parse(s))
function receipt(r,status){if(r.status!==status||r.data?.code!==0||r.data.message!=='success'||!/^req_[\w-]{1,100}$/.test(r.data.request_id||''))throw new Error('服务端回执不完整，结果未知。');return r.data.data}
export function validEvaluation(e){return !!e&&integer(e.id)&&Number.isInteger(e.score)&&e.score>=1&&e.score<=5&&(e.comment===null||typeof e.comment==='string')&&utc(e.created_at)}
export async function readEvaluation(id){const d=receipt(await apiClient.get(`/work-orders/${orderId(id)}/evaluations`,{authRequired:true}),200);if(!d||!Object.hasOwn(d,'evaluation')||(d.evaluation!==null&&!validEvaluation(d.evaluation)))throw new Error('评价读取格式不正确');return {evaluation:d.evaluation===null?null:project(d.evaluation)}}
function project(e){return {id:e.id,score:e.score,comment:e.comment,created_at:e.created_at}}
export async function createEvaluation(id,version,score,comment=''){
  if(!integer(version)||!Number.isInteger(score)||score<1||score>5)throw new Error('请核对真实版本和1～5整数评分。');workflowText(comment,1000,true)
  const d=receipt(await apiClient.post(`/work-orders/${orderId(id)}/evaluations`,{expected_version:version,score,comment},{authRequired:true}),201)
  if(d?.order_id!==Number(id)||!validEvaluation(d)||d.score!==score||d.comment!==comment)throw new Error('评价回执不完整，结果未知。');return {...project(d),order_id:d.order_id}
}
export async function closeOrder(id,version,note=''){
  if(!integer(version))throw new Error('请读取真实工单版本。');workflowText(note,500,true)
  const d=receipt(await apiClient.post(`/work-orders/${orderId(id)}/close`,{expected_version:version,note},{authRequired:true}),200)
  if(d?.order_id!==Number(id)||d.status!=='CLOSED'||d.version!==version+1||!utc(d.closed_at))throw new Error('关闭回执不完整，结果未知。');return {order_id:d.order_id,status:d.status,version:d.version,closed_at:d.closed_at}
}
export async function closableOrders({page=1,page_size=20}={}){
  if(!integer(page)||!integer(page_size)||page_size>100||!Number.isSafeInteger((page-1)*page_size))throw new Error('关闭队列分页无效')
  const d=receipt(await apiClient.get('/work-orders/closable',{authRequired:true,params:{page,page_size}}),200)
  if(!Array.isArray(d?.items)||!Number.isSafeInteger(d.total)||d.total<0||d.page!==page||d.page_size!==page_size||d.items.length!==Math.min(page_size,Math.max(0,d.total-(page-1)*page_size)))throw new Error('关闭队列响应无效')
  const seen=new Set();let previous
  const items=d.items.map(e=>{if(!integer(e.id)||!integer(e.version)||e.status!=='COMPLETED'||!(e.order_no===null||typeof e.order_no==='string')||!utc(e.updated_at)||seen.has(e.id)||(previous&&(e.updated_at>previous.updated_at||e.updated_at===previous.updated_at&&e.id>=previous.id)))throw new Error('关闭队列响应无效');seen.add(e.id);previous=e;return {id:e.id,order_no:e.order_no,status:e.status,version:e.version,updated_at:e.updated_at}})
  return {items,total:d.total,page,page_size}
}
