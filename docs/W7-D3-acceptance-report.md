# CampusFix W7-D3 — 维修结果提交与待验收流转验收报告

日期：2026-11-01。执行主体：Codex 实现与自动化验证；未取得用户本阶段浏览器人工验收结果。

## 1. 基线、范围与冻结边界

- 分支：`feature/w7-workflow`。
- 起始 HEAD：`f78c992c35e7d79e576ea64ff7f66caef1b2b7df`，`git fetch origin` 成功，起始 HEAD 与 origin/feature/w7-workflow 相同，工作区、暂存区干净，diff 空白检查无错误。
- 本轮仅实现 FR-009 的 API-23 / T-19：当前有效维修负责人将 `IN_PROGRESS` 提交到 `PENDING_VERIFY`。
- 复用 AuthGuard、AuthorizationService、WorkOrderStateMachine、workflow::note、SqliteTransaction、TransferRepository、StatusLogRepository、公开时间线、前端 workflowFlow 和共享 workflowGate。
- 未更改冻结 API 文档、Schema、seed、角色/区域读取规则、调度算法/权重、AI 功能；未实现 API-24/25/27/28，不提前验收、返工、评价、归档或释放容量。

## 2. 分层及事务实现

WorkflowController 复用既有严格 JSON 解析与统一响应，在既有生产注册入口增加 `POST /api/work-orders/{id}/complete`。只接受 `repair_summary` 和正整数 `expected_version`；拒绝未知字段、查询参数、重复键、布尔/字符串版本、非法 UTF-8/代理项。维修说明按 Unicode scalar 计数，1～2000，拒绝纯 Unicode 空白和非法控制字符；超长返回 422 `VALIDATION_ERROR`、field=`repair_summary`。中文不按 UTF-8 字节误计数。

CompletionService 在单连接 `BEGIN IMMEDIATE` 内：

1. 重新验证持久 Session 与启用账号，按 Action::Complete 校验主要角色、服务器读取的当前派单归属和 T-19 状态/版本。
2. 读取真实有效派单，确认 workers.user_id 为当前认证用户、accepted_at 有效；检查工单和派单 int64 版本可递增。
3. CompletionRepository 用状态/版本条件更新工单结果、状态、version、updated_at；用派单 ID、工单、版本、有效性、已接单及服务器归属条件更新 result_submitted_at、派单 version。检查实际受影响行数，防止零行更新被当作成功。
4. SQLite julianday 比较拒绝接单时间或既有最终结果时间晚于本次服务器 UTC 的无效写入；允许非空 result_submitted_at 在合法返工重提时更新。
5. 仅将同一工单、同一有效派单的 PENDING 申请改为 SUPERSEDED，写公开系统原因和 resolved_at，reviewed_by=NULL；检查预期/实际行数。其他派单申请不变。
6. 追加一条 ORDER_COMPLETED：真实 actor_id、assignment_id、request_id，from=IN_PROGRESS、to=PENDING_VERIFY；public_note 保存本次完整说明，时间与提交时间一致，不写敏感 metadata。不更新/删除旧日志；检查实际插入行数。
7. 提交前再次认证及校验 Complete 动作权限（不能重用已改变状态的 operation 检查），并检查原有效派单 ID/归属/版本/接单时间及已保存的状态、版本、结果、UTC、无未决申请不变量，随后提交。

`ended_at/end_reason` 保持 NULL，accepted_at 不变；派单版本递增一次但并未结束。实时负载由未结束派单查询，保持不变。原 dispatch_records 与历史派单保留。rework_count、completed_at 不在本操作修改。任何 SQL、认证或不变量检查失败都由事务回滚。

## 3. 真实 HTTP 与独立 SQLite 证据

`completion_http` 启动隔离四线程 Drogon（注册生产 Controller）及真实 Vite 代理，不是模拟 HTTP，更不是浏览器 E2E。测试数据库、随机 Session 及故障触发器仅位于临时目录。所有正常链路均通过真实 API-06→08→13→18→19 构造前置状态，不通过 UPDATE 冒充业务操作。

本轮最终 HTTP 记录：**8 场景、136 次 HTTP、175 次独立连接全 16 表内容指纹**；检查关键阶段工单、派单、申请、日志、调度快照和实时负载。指纹是对每表每行每列内容的摘要，不是只比行数；不与重跑数字累计。`--inspect` 另从独立 SQLite 连接读取真实行，属于数据库检查，不作为客户端返回白名单。

### 链路 A：提交结果、公开结果与不释放负载

真实请求：

```http
POST /api/work-orders/5/complete
Content-Type: application/json

{"expected_version":5,"repair_summary":"已更换虚构损坏部件，测试正常。\n完整维修结果 😀"}
```

实际 HTTP 200 响应（未记录凭据）：

```json
{"code":0,"message":"success","data":{"order_id":5,"repair_summary":"已更换虚构损坏部件，测试正常。\n完整维修结果 😀","status":"PENDING_VERIFY","version":6},"request_id":"req_7130a317-fbaf-4c04-be2a-384625461415"}
```

- assignment_id=1，worker_id=1；accepted_at=`2026-11-01T10:00:57.694Z` 保持原值。
- result_submitted_at=`2026-11-01T10:00:58.405Z`，派单 version=3，ended_at/end_reason 均 NULL；工单 version=6，rework_count=0、completed_at=NULL。
- 此样本维修员实际负载 **1→1**，不是提前释放。调度快照不变；旧日志逐条保持，仅追加一条完成日志。
- 完成日志 actor_id=6、assignment_id=1、from/to 正确、public_note 为全文、UTC 及 request_id 与实际提交一致。
- 原报修人真实 GET timeline 200，显示完整结果且无内部审核备注/原始 metadata；其他报修人、无业务读取授权 sysadmin 均 404。
- API-10 仍不返回 repair_summary，没有扩大详情结构；完成结果通过公开时间线取得。
- `view=assigned&current_only=true&status=PENDING_VERIFY` 能查询本人的待验收有效任务；再次 complete 返回 409。

### 链路 B：未决转派原子失效

工单6先真实接单、申请转派，再 complete 200。transfer_request_id=1、assignment_id=2，原 PENDING 变 SUPERSEDED，reviewed_by=NULL，公开系统原因 `Repair result submitted; pending transfer superseded`；resolved_at 与该派单 result_submitted_at 一致。负载未释放。本人 API-34 可读历史失效结果，managed 队列不再返回该申请；旧页面提交 API-22 返回409、全表指纹不变。

### 链路 C 与其他真实竞争

第三个独立 SQLite 连接先持有写锁；通过 SQLite trace 确认两个 HTTP 请求都已尝试 BEGIN IMMEDIATE，才释放锁，不用 JavaScript 串行调用伪造竞争。

最终一次实际结果：

| 竞争 | HTTP 结果（complete 在前列） | 最终证据 |
|---|---|---|
| complete / complete | 200 / 409 | 一条完成日志，PENDING_VERIFY |
| complete / 批准转派 | 403 / 200 | 转派先成功，PENDING_DISPATCH，无完成日志 |
| complete / 进度 | 200 / 201 | 进度先追加后完成，PENDING_VERIFY |
| complete / 申请转派 | 200 / 201 | 先申请后完成，PENDING_VERIFY，申请失效 |

批准转派若先结束原派单，旧维修员失去动作权限而403，这是原权限规则，不能强行改成所有竞争均409。进度/申请不改变工单版本，可在完成前合法串行成功；完成先提交则后续事件状态409。审批拒绝不改变状态/版本，可在完成前合法发生，另由 SQLite Service 并发测试覆盖。

### 安全、失败和重提边界

- HTTP 覆盖匿名、非法/过期/撤销 Token、禁用用户、错误角色/非当前/历史负责人、旧版本、未接单、终态、未知字段、非法版本、Unicode 空白、2000/2001 中文字符、控制字符、畸形 UTF-8、JSON 重复键及不完整代理项。所有拒绝操作前后全表指纹不变。
- HTTP 在 work_orders、assignments、transfer_requests、status_logs 四个实际写入阶段逐一注入 SQL ABORT，返回500，所有16表无部分写入。Service 还覆盖各阶段 RAISE(IGNORE) 零行更新/插入及事务末尾账号停用回滚。
- 真实 SQLITE_BUSY 返回503，写入前后指纹一致。
- 客户端在真实服务端提交后主动丢弃成功回执，前端保持 unknown_result，只发有权限 GET；观察到 ORDER_COMPLETED 也不当作本次 POST 成功证明，不允许自动重发。
- 再次提交兼容性：第一次通过真实 complete 保存；隔离夹具明确将该单改回 IN_PROGRESS/version+1/rework_count+1，模拟未来返工前置，再调用真实 complete。新摘要及 result_submitted_at 更新，先前完成日志/时间/全文独立保留，complete 不增加返工计数。**这不是已实现或已测试 API-24**。
- 临时数据库 `integrity_check=ok`，foreign_key_check 无记录。测试后自有进程、端口、随机 Session 和临时数据库清理；没有操作用户演示数据库。

## 4. 前端行为与验证

- WorkerWorkflowPanel 仅 IN_PROGRESS 提供“提交维修结果”；确认对话框显示工单号/服务器版本、1～2000 Unicode 字符实时计数及“不立即释放负责人和负载”提示。取消不写入，确认才调用真实 API。
- 复用相同 workflowGate；complete 提交中阻止其他任务的 complete、转派、到场、进度与接/拒单确认。身份、任务变化及卸载使旧回调失效，操作 ticket 防止旧请求释放新账号的新锁。
- 严格核验200统一 envelope（code/message/request_id）、order_id、PENDING_VERIFY、version=原值+1、repair_summary 精确相同；无效200/201及网络/5xx均未知，不伪造成功。
- 422 保留结果草稿及修正提示；409 清确认、旧版本/父页面任务，需重新 GET；401/403/404 清未授权资料；未知结果仅只读核实，不自动重发。
- 成功重新读取公开日志（多页时读取最新页），通知父页面重新 GET 待验收任务，不在客户端猜写状态。新增 PENDING_VERIFY 查询始终 `current_only=true`；待验收区域只读，不提供再次提交/转派/进度写按钮。
- 原报修人复用 OrderDetailView→OrderTimeline 查看结果；没有扩展 API-10/11 白名单。

新增前端 **24/24**，原217项保留，全量 **241/241、退出0**。包括真实 SFC setup/watch/卸载、跨组件共享写锁、校验/错误/未知回执及身份切换；OrderTimeline 编译后的真实 render 通过 Vue SSR 显示公开完成全文及待验收标签，并转义 HTML。SSR/单元测试**不等于真实浏览器人工验收**。

`npm.cmd run build` 退出0；Vite 8.3.5，JS809.13kB/gzip260.23kB，保留既有大于500kB chunk警告。Node v24.18.1、npm11.16.0，无新增依赖/锁文件变化。

## 5. 构建、CTest、冒烟及首次失败历史

命令（仓库根目录；前端命令在 frontend 执行）：

```powershell
$cmakeBin = 'D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
& "$cmakeBin\cmake.exe" --build backend/build --config Debug --parallel 4
& "$cmakeBin\ctest.exe" --test-dir backend/build -C Debug --output-on-failure -R '^completion_'
& "$cmakeBin\ctest.exe" --test-dir backend/build -C Debug --output-on-failure
```

```powershell
node --test tests/auth.test.js tests/orders.test.js tests/admin.test.js tests/admin-security.test.js tests/dispatch.test.js tests/dispatch-security.test.js tests/report-assistant.test.js tests/report-draft.test.js tests/report-draft-handoff.test.js tests/report-draft-integration.test.js tests/worker-tasks.test.js tests/workflow.test.js tests/completion.test.js
npm.cmd run build
```

- CMake自动重新配置/生成成功；保留既有x64-windows baseline，Drogon1.9.13#2、SQLite3.53.4#2、libsodium1.0.22#1全为现有已安装依赖，无下载或升级。
- 完整 MSVC Debug 构建退出0，无新增编译/链接警告。
- 新增 completion_service **7场景95断言**，completion_concurrency **5场景20断言**；completion_http 如前述。原42个CTest保留，新增3个，总45个；最终全量结果在下方结项补记。
- completion_http 通过真实已启动 Drogon/Vite 调用既有 `scripts/smoke.ps1 -BackendUrl <本轮随机后端地址> -FrontendUrl <本轮随机前端地址>`：后端/代理各两次200 JSON、request_id变化、首页HTML，退出0。不是重写一套冒烟断言冒充原脚本，也不是浏览器测试。
- 首次沙箱构建在 MSBuild FileTracker 初始化报 MSB4018/E_ACCESSDENIED，已通过本机授权执行相同工具链命令解决，未更换编译器/依赖。
- 新增 C++ 测试首次编译 C2678（std::string 与 Json::Value 比较）最小修正 `.asString()`；因编译失败未生成测试程序，随后两项 CTest 曾显示 Not Run，并非业务测试通过，修正重建后执行通过。
- 并发测试重复运行暴露新测试错误：两个 complete 的后启动调用也可以合法胜出，断言曾错误按第一个调用是否成功推断日志数。现按实际任一胜者检查仅一次完成/一条日志，保留完整竞争及无部分写入断言，不删除失败场景。后续全量重跑结果独立记录、不累计。
- 既有认证/授权/数据库/AI/调度/转派回归若通过，是本机自动化及本地替身AI，不冒充本轮真实DeepSeek或跨机器部署验证。

## 6. 真实浏览器状态与待人工验收

**桌面、390px、Console、真实页面鼠标/键盘交互：未验证，不能宣布人工验收通过。**

按 computer-use 技能初始化并检查：本机 sky.list_apps 能枚举运行中的 Edge 窗口；专用浏览器连接清单为空并返回 browser access host unavailable / native pipe unavailable。没有将窗口枚举当作页面测试；没有输入用户密码、没有登录浏览器、没有截图或用户人工反馈。

请用户在独立临时演示库及本人隐藏输入设置的虚构账户中补验：

1. 管理员通过既有页面派单；当前 worker 接单、到场、填写进度，打开结果确认框，核对工单号、版本、字数、取消不请求。
2. 空白/超长不发送，2000中文及emoji按Unicode计数；有效提交只发一次，其他任务按钮同步禁用。
3. 成功后进入真实待验收只读区域，当前负责人/负载未释放，公开时间线显示完整结果；报修人可见，其他用户不可见。
4. 有待处理转派时提交结果，worker本人历史显示失效、管理员队列消失，旧审批页面冲突。
5. 409清旧选择/任务，422可修改草稿；网络中断/丢失回执只读核实不自动重发；切换账户/离开页面不恢复旧资料。
6. 桌面和390px检查对话框、实时计数、按钮换行、公开时间线与Console组件警告。不要把本阶段没有实现的验收、评价按钮视为可用功能。

## 7. 完整文件范围

修改11个：

- `backend/CMakeLists.txt`
- `backend/src/controllers/WorkflowController.cpp`
- `backend/tests/integration/WorkflowHttpFixture.cpp`（隔离测试注册真实创建/提交/审核；增加独立结果检查与明确标记的返工夹具，不增加正式测试HTTP路由）
- `frontend/package.json`（仅新增测试命令，无新增依赖）
- `frontend/src/api/workerTasks.js`
- `frontend/src/api/workflow.js`
- `frontend/src/components/WorkerWorkflowPanel.vue`
- `frontend/src/utils/workflow.js`
- `frontend/src/views/WorkerTasksView.vue`
- `frontend/tests/workflow.test.js`（仅补新增completeTask注入，原断言不删）
- `docs/progress-log.md`

新增8个：

- `backend/src/repositories/CompletionRepository.h`
- `backend/src/repositories/CompletionRepository.cpp`
- `backend/src/services/CompletionService.h`
- `backend/src/services/CompletionService.cpp`
- `backend/tests/integration/CompletionTest.cpp`
- `frontend/tests/completion.test.js`
- `frontend/tests/run-completion-http.mjs`
- `docs/W7-D3-acceptance-report.md`

共19文件、删除0，均不自动暂存。默认 `git diff --stat` 不含尚未跟踪的新文件，不能把其数字误称为完整源码差异；新增源文件亦须独立审查。完整清单如上，没有把原W1-D3交接文件、数据库/WAL、构建缓存、node_modules、dist、凭据或日志纳入范围。

## 8. 风险及 W7-D4 前置条件

- 尚未执行上述真实浏览器人工验收；通过源代码和自动化不替代人工结论。
- SQLite单写者、busy_timeout及Axios超时仍可能造成未知结果；无跨刷新幂等回执，不允许自动重放，人工核实也不能仅凭当前状态推断某一次POST成功。
- 提交结果保留有效派单，所以PENDING_VERIFY继续占用容量，这是设计而非漏释放。W7-D4才实现原报修人验收、通过时事务结束派单/释放容量、不通过时保留负责人并返工。
- W7-D4需沿用最终result_submitted_at供历史效率统计；重提更新最新时间，旧结果由不可覆盖的日志保留。本轮返工夹具不代表完整验收写接口已验证。
- 公共结果由用户填写，可能自行写入私人信息，页面明确公开提示且按文本转义；不承诺内容脱敏或用户内容绝对无敏感信息。程序不额外写入Token/联系字段/内部metadata。
- 继续保留内存Token刷新需重登、离线撤销不确定、完整登录限流/压测/物理断电故障/跨机器部署未验证，以及既有大chunk等风险。
- 完成本阶段后仅等待人工及独立源码审查，不进入W7-D4，不git add/commit/push/merge。

## 9. 最终结项检查

- 最终完整CTest：**45/45，失败0，退出0，279.45秒**；原42个名称从起始HEAD的CMake展开与本轮实际CTest清单核对，遗漏0；新增 completion_service、completion_concurrency、completion_http。
- 前端完整241/241，新增24项，退出0；完整Debug构建及Vite构建退出0。没有删除既有断言或调整冻结规则来获取通过。
- 完整回归之后另实际重复运行 completion_concurrency：5场景20断言退出0，包含第二个complete调用胜出409/200、进度或申请后到409、先拒绝审批后完成双200；不与完整CTest累计。
- git diff --check、git diff --cached --check 均退出0；暂存区为空。新增文件单独no-index空白检查、所有变更文件严格UTF-8及末尾换行检查通过；LF→CRLF提示仅为Git换行提示，不是空白错误。
- 修改11、新增8、删除0，共19文件，全部未暂存；无无关未跟踪文件。HEAD未改变，没有git add/commit/push/merge。
- 已跟踪文件的实际 `git diff --stat`：11 files changed, 76 insertions(+), 27 deletions(-)；不包含8个未跟踪新文件。`git diff --cached --stat` 为空。
- Schema/seed、冻结SRS/API/权限/流程/决策/需求追踪、domain权限/状态机、dispatch算法、AuthorizationService、vcpkg清单及npm锁文件静态差异检查均未改变。
- git check-ignore确认build证据、SQLite/WAL、node_modules、dist被忽略；没有将随机凭据或本地测试产物纳入Git。测试程序确认自有随机端口释放及临时数据清理，结项8848/5173监听数0；未停止用户的其他服务。

**CampusFix W7-D3 维修结果提交与待验收流转实现与自动化验证已完成，等待独立源码审查；未进入W7-D4。**浏览器人工验收仍待补，不将自动化结论扩大为人工已通过。
