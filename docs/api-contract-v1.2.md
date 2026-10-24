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
- 真实 DeepSeek 六场景、HTTPS真实服务链路及浏览器390px验收：待用户执行，未验证。
