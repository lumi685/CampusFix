# CampusFix V1.2 扩展 REST 契约

日期：2026-10-24。独立于冻结 API-01～33；不改变原接口、状态机或数据库。

## AI-01 AI 对话辅助报修

- 需求：FR-AI-01。
- `POST /api/ai/report-assistant/chat`。
- 现有 AuthGuard；Authorization: Bearer。只有 reporter、worker 可用；角色来自服务端有效 Session，管理能力不授予使用权。没有工单资源 ID，不新增原报修人权限。
- Content-Type：application/json（可含 UTF-8 charset）；不接受查询参数。

```json
{"messages":[{"role":"user","content":"宿舍灯不亮"},{"role":"assistant","content":"其他插座有电吗？"},{"role":"user","content":"插座有电"}]}
```

请求严格白名单：根对象只有 messages；非空数组、最多12项；每项只有字符串 role/content。role 只允许 user/assistant，首尾均为 user。每条非空且非纯空白，UTF-8 Unicode 标量最多1500，总计最多9000；正文最多65536字节。拒绝非法 UTF-8/代理字符/NUL/非法控制字符、未知字段、重复 JSON 键、尾随数据。允许换行和制表；不接受 system/developer/tool、URL、模型、用户 ID、角色授权或 API Key。

只有服务端可以注入可信 system。历史 assistant 消息不可信，不改变授权；AI 不获得 SQL、工具或业务能力。客户端可按需求文档删除最旧上下文；服务端不将超限请求静默改成有效请求。

成功200（形状示例，不冒充真实 DeepSeek 回执）：

```json
{"code":0,"message":"success","data":{"reply":"请先停止使用异常灯具，不要自行拆装。没有其他危险迹象时，可以前往普通报修页面联系维修人员。"},"request_id":"req_<server-generated>"}
```

成功 data 仅含 reply；非空文字，最多1500个 Unicode 标量。所有响应复用统一 envelope，成功/失败均生成真实 request_id；路由响应 Cache-Control:no-store。不向浏览器返回模型 Key、Token、上游正文、system 提示词、思维链、工具结果或数据库内容。

错误示例：

```json
{"code":"AI_NOT_CONFIGURED","message":"AI service is not configured","data":null,"request_id":"req_<server-generated>"}
```

| HTTP | code | 意义 |
|---|---|---|
| 400 | MALFORMED_REQUEST | JSON 解析失败、重复键或非法代理转义 |
| 401 | AUTH_REQUIRED / AUTH_INVALID / AUTH_EXPIRED | 既有认证语义；禁用/撤销也拒绝 |
| 403 | ACTION_FORBIDDEN | 角色不允许，管理能力不例外 |
| 413 | AI_REQUEST_TOO_LARGE | 正文、单条、总内容或条数超限 |
| 415 | AI_UNSUPPORTED_MEDIA_TYPE | 非 JSON 内容类型 |
| 422 | AI_VALIDATION_ERROR | 类型、角色、空白、未知字段或 Unicode 非法 |
| 429 | AI_REQUEST_IN_PROGRESS | 同 Session 已有请求 |
| 429 | AI_BUSY | 进程4个并发已满 |
| 429 | AI_RATE_LIMITED | 单 Session 10/分钟、60/小时，或进程200/小时额度 |
| 503 | AI_NOT_CONFIGURED | 配置缺失或无效，不返回假回复 |
| 502 | AI_PROVIDER_AUTH_FAILED | 上游401/403，非客户端 Session 过期 |
| 503 | AI_PROVIDER_RATE_LIMITED | 上游429；用户明确手动重试 |
| 504 | AI_UPSTREAM_TIMEOUT | 有限超时或读取截止时间到达 |
| 502 | AI_UPSTREAM_UNAVAILABLE | 上游5xx/其他非200、重定向、TLS/连接失败 |
| 502 | AI_INVALID_RESPONSE | 非法 JSON、错误结构、工具/截断/空白/超长回复 |
| 502 | AI_RESPONSE_TOO_LARGE | 上游正文接收中超过64KiB |
| 500 | INTERNAL_ERROR | 本地意外错误，固定安全文案 |

不向前端转发上游错误原文，不自动重试/重定向。前端不能把模型服务401当成本应用401：Adapter 映射为502，避免错误注销当前 Session。

## 读写与事务

AI 模块没有数据库操作和工单状态转换，不写 status_logs，不开启事务；AuthGuard 只读验证现有会话/身份。多轮上下文仅当前页面内存，后端仅在途内存；额度仅记录 Token 摘要、计数和时间，不存聊天。清空/退出不保证撤回已发送的第三方数据。

## 测试定位

- `ai_chat_validation_limits`：C++ 严格输入及限额，296个断言。
- `ai_chat_http`：独立临时 SQLite + 正式 Drogon + 本地独立假模型，63次 AI-01 HTTP，29次上游调用；包括上下文顺序、安全错误、无写入、角色、并发及额度。不是 DeepSeek 验收。
- `frontend/tests/report-assistant.test.js`：12项流程/API/角色/SSR转义/真实SFC清理测试，非浏览器 E2E。
- AI-D1真实DeepSeek六场景已由用户确认通过，见AI-D1验收报告；390px等未反馈专项不自动视为通过。上述测试数字为AI-D1基线，AI-D2结果独立记录。

## AI-02 根据当前保留对话生成文字报修建议

- FR-AI-02；`POST /api/ai/report-assistant/draft`，同一AuthGuard与统一响应。不是原API-02退出接口，不改API-01～33或AI-01。
- 仅reporter/worker，有效Bearer Session；管理授权不授予使用权。Content-Type与查询参数规则同AI-01。
- 根对象只有messages，条目恰好role/content。类型、Unicode、空白、未知字段、JSON重复键及大小限制同AI-01；第一条user，最后允许user或assistant。不能上传URL、模型、Key、system prompt、授权、工单ID或工具消息。

```json
{"messages":[{"role":"user","content":"宿舍洗手池下方排水管漏水"},{"role":"assistant","content":"请停止使用并注意防滑，具体楼栋房号待补充。"}]}
```

成功200（结构示例，不是本轮真实模型回执）：

```json
{"code":0,"message":"success","data":{"draft":{"title":"宿舍洗手池下方排水管漏水","description":"用户反映洗手池下方排水管漏水，具体楼栋与房号待补充。","category_hint":"给排水","location_hint":"宿舍洗手池","missing_information":["楼栋与房号"],"safety_note":"停止使用漏水设备，注意防滑。"},"requires_review":true},"request_id":"req_<server-generated>"}
```

data只含draft和服务端固定true的requires_review。draft是**未受信的建议文本DTO，不是WorkOrder**；不包含id、category_id、location_id、状态或执行指令。

| 必需字段 | 类型与上限（Unicode标量） |
|---|---|
| title | null或非空白字符串，120 |
| description | null或非空白字符串，2000 |
| category_hint | null或非空白字符串，120；不得作为正式分类ID |
| location_hint | null或非空白字符串，200；不得作为正式位置ID |
| missing_information | 字符串数组，可空、最多5条；每条非空白且≤120，不允许null条目 |
| safety_note | null或非空白字符串，300 |

六字段必须全部出现，未知/缺失/错类型、非法Unicode/控制字符、超限、空对象、Markdown围栏、截断或尾随数据均拒绝。未知信息应null而非空白字符串。模型若返回requires_review、数据库ID等额外字段也拒绝。外层工具调用/非stop/空输出不视为成功。不返回上游原文、思维链或异常细节。

新增错误：502 `AI_DRAFT_INVALID_RESPONSE`，统一data=null，固定安全文案和真实request_id。其余输入400/401/403/413/415/422、额度429、配置503、上游401/403映射502、上游429映射503、超时504、连接/5xx502及接收64KiB超限502均复用AI-01。接口Cache-Control:no-store，不自动重试。

AI-01和AI-02共享同一Session/进程额度与并发槽，不能切换路由绕过。AI-02 max_tokens=4096、JSON模式及report-draft-v1提示词只在此模式生效；AI-01仍512 token、1500字、最后user。客户端保留窗口允许assistant结尾，但不会将此放宽传给AI-01。

### AI-02 数据与测试边界

AI模块不读业务表、不持久化建议、不开事务、不写work_orders/status_logs等表；AuthGuard仍必要只读认证，不宣称整个请求无数据库访问。生成、取消、复制或跳转普通报修均不调用工单创建/编辑/提交接口；分类与位置需要后续正式表单真实选择，AI-D2不预填。

测试：ai_draft_service（严格字段/Unicode/服务编排）、ai_draft_http（独立假模型+正式HTTP+SQLite前后快照）、frontend/tests/report-draft.test.js（流程/API/真实SFC/SSR，非浏览器）。AI-D1原测试独立回归。实现阶段AI-D2真实DeepSeek A～H及浏览器专项曾待用户人工验收，不能用AI-D1人工结果替代；此前自动化记录保留，本次仅同步状态、不重跑测试。

验收状态同步（2026-10-24）：用户已在真实 DeepSeek 浏览器中完成 A～H 八项人工验收，8/8 通过。执行人为用户本人，非 Codex；未提供原始 Network、模型名、额外数据库快照等证据。390px 窄屏、剪贴板权限拒绝等未单独反馈的专项仍未验证。

## FR-AI-03：AI-D3前端安全预填（无新增REST接口）

2026-10-24用户批准：AI-D3复用既有API-04、05、06、08、10，未新增REST端点；详情时间线验证另复用API-11。AI-01/02请求、响应、共享额度及后端只读边界不变。上文AI-D1/D2“不预填”描述保留为其阶段范围，不代表本节批准后的前端交接功能被禁止。

只有用户确认“使用此建议填写报修”后，当前编辑快照可在同一认证会话中经一次性内存交接预填title/description，不进行任何HTTP写入。不通过URL、持久化存储或请求传递建议快照，分类/位置线索不转ID。表单的category_id/location_id必须选择真实目录；紧急度默认NORMAL，联系人/电话/日期不自动填。

AI来源表单经原reportPayload校验及第二次用户确认后，复用reportSubmission调用原API-06创建DRAFT，再以服务端返回id/version调用API-08。创建body仅包含原八个允许字段（可选日期按原规则省略），不得含hint/missing_information/safety_note/身份/状态/单号/模型属性；submit仅expected_version。原权限、事务、编号、状态版本及409/422语义均不改变；失败重试原草稿、创建未知结果阻止再次创建。普通导航与接收建议本身不等于正式提交。

AI-D3验收独立于原AI-D1/D2：真实HTTP/SQLite和组件路由测试见docs/V1.2-AI-D3-acceptance-report.md；本阶段真实DeepSeek浏览器人工验收仍待执行，不复用旧人工通过结论。
