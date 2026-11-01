# W7-D1：维修员接单、拒单与任务工作台验收报告

## 1. 基线、范围与证据主体

- 起始 main / origin/main：`9d7b78d0afcd6b16df75a2f6d324dc88c4686503`，`Merge pull request #2 from lumi685/feature/w6-dispatch`。
- 实际执行 git fetch，确认 W6 最后提交 `f8aa95e` 已在 origin/main；起始工作区和暂存区干净。随后 main 快进并创建 `feature/w7-workflow`，未创建提交。
- W6 浏览器验收、源码审查已完成是用户本次明确提供的阶段确认；main 合并结果另有本轮实际 Git 检查。保留 W6-D5 原先浏览器工具失败/待补的历史记录，不将用户确认伪称为 Codex 独立浏览器测试，没有新增 W6 截图或测试耗时。
- 本轮只实现 API-19/20、当前维修任务读取与 Vue 工作台，复用既有认证、授权、T-13/T-14、日志和 SQLite 基础设施。无新依赖；未改 Schema、seed、冻结文档、权限矩阵、W6 算法或 AI 行为。

## 2. 后端实现与安全边界

`WorkerTaskController → WorkerTaskService → WorkerTaskRepository` 分层；所有 SQL 参数绑定。Controller 严格解析 JSON，拒绝重复键、未知字段、非法 ID/正数 int64 版本、非字符串原因和多余查询参数。

两个操作均在单连接 `BEGIN IMMEDIATE` 中重新认证，按服务端身份执行 Action::Accept / Refuse、当前派单归属、PENDING_ACCEPT、期望版本和 accepted_at 检查；工单和派单均有 int64 版本溢出保护。

- 接单：只更新原有效派单 accepted_at / version，CAS 工单到 IN_PROGRESS / version+1，追加 ORDER_ACCEPTED，关联真实 assignment_id。不会新增派单或增加负载。
- 拒单：结束原派单，end_reason=REFUSED、version+1；同事务将该派单 PENDING 转派申请标为 SUPERSEDED，reviewed_by=NULL，写系统原因和 resolved_at；CAS 工单到 PENDING_DISPATCH / version+1，追加 ORDER_REFUSED / public_note。原管理员 assign_reason、派单和 dispatch_records 均保留。
- 提交前再次核验 Session、账户角色与派单所属用户；所有失败由事务回滚。日志只记录 HTTP 状态和 request_id，不记录 Authorization、完整 Token、密码或拒单内容。
- 未认证 401；已知可见资源无动作权限 403；隐藏不存在/越权资源 404；状态/版本冲突 409；参数 400/422；真实 SQLITE_BUSY/LOCKED 安全返回 503，不把数据库原始错误发给客户端。
- 重放刚刚完成的本人拒单：仅在服务端仍有读取权限、无新派单、最新记录确为本人 REFUSED 且工单版本正好为请求版本+1时返回 409，不授予历史负责人任何操作权。一般已转派历史负责人仍为 403/404。此识别只报告冲突、不产生写入，不修改 Policy。

### 当前任务与历史查询分离

API-09 增加任务要求允许的最小只读过滤 `view=assigned&current_only=true`，默认 false/省略保持原“历史上分配给本人”的语义。仅 assigned 接受该参数，值必须为 true/false；其他 view 不接受。相同服务端有效派单 EXISTS 条件同时用于计数和分页，身份取认证用户，不接受客户端 worker_id / user_id。

## 3. 真实 HTTP 与 SQLite 证据

隔离测试库由迁移/seed 创建，所有账号和数据虚构、Token 随机。订单通过现有创建、提交、审核 Service 到待派状态，再通过**真实 API-18** 派单；接单/拒单使用生产 Controller、四线程 Drogon 和真实 Vite 代理。没有 UPDATE 工单来冒充 API 成功。

最终完整 CTest 中新 HTTP 测试输出：**8 个场景、80 次请求、88 次独立 16 表全列指纹检查**。该数字只取最后一次完整执行，不累计重跑。安全证据：`backend/build/Testing/w7d1-http-evidence.json`（忽略、不提交、无凭据）。HTTP/Node 测试不是浏览器 DOM 或视觉验收。

实际隔离样本（非用户工单）：

| 链路 | 真实响应 | 数据库核验 |
| --- | --- | --- |
| B：API-18 → POST /api/work-orders/5/refuse | 200，order_id=5，PENDING_DISPATCH，version=5，assignment_ended=true | assignment_id=1 保留、version=2、ended_at UTC、REFUSED；负载 1→0；旧推荐快照保留；待审转派失效；ORDER_REFUSED 关联相同派单及真实请求ID |
| A：API-18 → POST /api/work-orders/6/accept | 200，order_id=6，IN_PROGRESS，version=5，accepted_at=`2026-10-31T15:49:58.233Z` | assignment_id=2 保留且有效，accepted_at 与响应一致、version=2；负载 1→1；ORDER_ACCEPTED 关联相同派单及真实请求ID |

请求体分别为 `{"expected_version":4,"reason":"Fictional HTTP refusal"}` 和 `{"expected_version":4}`。

真实拒单响应统一结构中的 request_id 为 `req_8b3a989d-b99f-4b7a-bfbf-59017fc03043`，真实接单为 `req_91060043-8ae2-4cb3-bb41-e2e8b70a01ca`；code=0，message=success，data 如表。重复请求各返回 409 ORDER_STATUS_CONFLICT，不新增日志。测试未打印或归档完整 Token。

HTTP 检查还包括：匿名/非法/过期/撤销/停用 401；其他维修员未获可见性 404、历史负责人 403、reporter/dispatcher/显式授权 sysadmin 403；Unicode 原因空白/501字符/控制字符 422，500个 emoji 成功；未配对 surrogate 被严格 JSON 解析拒绝为 400；非法 int64、重复键、未知字段、旧版本均拒绝且数据库指纹不变。

故障注入覆盖接单的派单/工单/日志写失败，以及拒单的派单/工单/转派/日志写失败，HTTP 500 无 SQL 泄漏，16表指纹均恢复。独立 SQLite 检查 `integrity_check=ok`、`foreign_key_check` 无记录。拒单释放容量后同维修员再次出现在真实合法推荐中。

并发分别验证 accept/accept、refuse/refuse、accept/refuse：第三个独立连接持写锁，等待两个真实请求的 BEGIN IMMEDIATE 均已尝试后才释放；每组一个200、一个409，仅一条业务动作日志、版本只递增一次。Service 多连接测试另行覆盖同三组。真正持锁超过 busy_timeout 的 HTTP 请求为503且无写入。

## 4. 前端行为

- `/worker/tasks` 使用真实 WorkerTasksView，只查询当前有效派单，待接/进行中分页与本人报修、历史记录分离。所有工单编号、分类、位置、审核紧急度、状态和版本来自后端，不计算权限或伪造本地成功状态。
- 接单二次确认；拒单对话框明确原因公开可见，1～500 Unicode 字符；显式局部注册 ElDialog，复用已注册组件，无整体 UI 重构。
- POST 携带真实工单版本、无身份字段；禁止双击/自动重发。成功后重新 GET 真实列表，接单切至进行中，拒单从待接任务消失。
- 409 清除旧操作要求刷新；401/403/404 清除旧列表、选择和原因；网络/5xx/异常回执保持 unknown_result，只通过授权详情 GET 核实，不将当前状态当作本次请求成功证明，不重放 POST。
- generation + 同步身份 watch 清理退出、换用户、卸载后的迟到读写；内存 Token 策略不变，无持久化凭据。
- HTTP 集成使用生产前端 API/flow 成功接单、拒单并 GET 回读；模拟“真实服务器已提交后客户端丢回执”保持未知且无重发。该注入不是声称现场真实断网。

## 5. 实际命令与结果

在项目根目录使用已验证的 VS CMake/CTest（未安装或升级环境）：

```powershell
$cmakeBin = 'D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
& "$cmakeBin\cmake.exe" --build backend/build --config Debug --parallel 4
& "$cmakeBin\ctest.exe" --test-dir backend/build -C Debug -R '^worker_task_' --output-on-failure -V
& "$cmakeBin\ctest.exe" --test-dir backend/build -C Debug --output-on-failure
```

- CMake 自动重生成成功，依赖均 already installed；MSVC Debug 完整构建退出0，无新增编译/链接警告。复用原 vcpkg baseline、x64-windows、Drogon 1.9.13#2 / SQLite 3.53.4#2 / libsodium 1.0.22#1。
- 新 CTest `worker_task_service`：8 场景、77 断言通过；`worker_task_concurrency`：3 场景、12 断言通过；`worker_task_http`：上述8场景通过。
- 完整 CTest：**37/37，失败0、退出0，177.35秒**，包含原34项，未删除/跳过用例。原认证、授权、迁移、seed、审核、编辑、时间线、调度和本地替身 AI HTTP 回归通过；不是本轮真实 DeepSeek 调用。
- frontend 下实际 `node --test tests/auth.test.js tests/orders.test.js tests/admin.test.js tests/admin-security.test.js tests/dispatch.test.js tests/dispatch-security.test.js tests/report-assistant.test.js tests/report-draft.test.js tests/report-draft-handoff.test.js tests/report-draft-integration.test.js tests/worker-tasks.test.js`：**182/182，退出0**，旧154+新28。新增包含实际 SFC setup/watch 与模板编译、安全生命周期、API 和流程测试，非浏览器点击。
- frontend 下 `npm.cmd run build` 退出0；JS 785.76kB / gzip254.97kB，大于500kB chunk 警告保留，未提高阈值掩盖。
- 额外在自有随机端口启动正式后端（CAMPUSFIX_DB_PATH 为空）和 Vite，真实执行 `scripts/smoke.ps1 -BackendUrl <随机后端地址> -FrontendUrl <随机前端地址>`：直连/代理各两次200 JSON、request_id不同、首页200 HTML，退出0。健康检查仍独立于数据库。

### 首次失败与修复历史

- 新 C++ 测试最初找不到 validUtcDateTime；补正为实际 `domain/WorkOrderValidation.h` 后构建通过，未掩盖警告。
- 前端最初两条 API adapter 单测夹具缺少 request_id，被已有响应校验拒绝；补齐测试 envelope 后28/28。
- HTTP 夹具初版误用 transfer_requests.expected_version；对照真实迁移修正为 order_version，不修改 Schema。
- 未配对 surrogate 的 HTTP 测试最初期待422，但严格 JSON 解析实际400；保留此用例并改为解析层正确预期，另有合法 JSON 字段的422覆盖。没有删除失败用例获取通过。

## 6. 真实浏览器：尚未验证

按 computer-use 技能尝试实际连接：Windows native pipe unavailable，Browser Use host unavailable，apps/browsers 清单为空。未点击页面，没有 Console、桌面/390px或截图证据；本轮自动化通过不等于人工浏览器通过。

请在受控虚拟演示库中人工补验（不要发送密码/Token，不展示 Authorization 截图）：

1. 有权限管理员通过真实调度页面给同一虚拟 worker 派两单；登录该 worker，待接列表只显示本人当前有效任务，并正确显示所有字段。
2. 接单前取消不发送POST；确认接单仅一条POST，真实状态/版本进入“进行中”，刷新/重登仍存在。
3. 对另一单填写拒单原因（空白/超长被拦截），取消不写；确认后待接列表消失，管理员可见待派状态、公开拒单原因与释放后的真实负载，历史未删除。
4. 非负责人、历史负责人、reporter、dispatcher、sysadmin 不能代为接单/拒单；worker 本人报修入口仍独立。Console 无组件解析警告。
5. 双击、409、断网/后端离线、只读核实、恢复后刷新行为符合上文；未知写入不重试，退出/切换身份/离开页面不恢复旧数据。
6. 检查窄屏及对话框可操作性。仅分享去除凭据的截图或结论；人工结果另行补记，不能假定通过。

## 7. 文件清单与 Git

所有路径均相对 `G:\A_Study\CppProject\CampusFix`。

修改10项：

- backend/CMakeLists.txt
- backend/src/controllers/WorkOrderQueryController.cpp
- backend/src/main.cpp
- backend/src/models/WorkOrderRead.h
- backend/src/repositories/WorkOrderQueries.cpp
- backend/src/services/WorkOrderReadService.cpp
- frontend/package.json
- frontend/src/layouts/AppLayout.vue
- frontend/src/router/index.js
- docs/progress-log.md

新增15项：

- backend/src/controllers/WorkerTaskController.h
- backend/src/controllers/WorkerTaskController.cpp
- backend/src/repositories/WorkerTaskRepository.h
- backend/src/repositories/WorkerTaskRepository.cpp
- backend/src/services/WorkerTaskService.h
- backend/src/services/WorkerTaskService.cpp
- backend/tests/integration/WorkerTaskTestData.h
- backend/tests/integration/WorkerTaskServiceTest.cpp
- backend/tests/integration/WorkerTaskHttpFixture.cpp
- frontend/src/api/workerTasks.js
- frontend/src/utils/workerTasks.js
- frontend/src/views/WorkerTasksView.vue
- frontend/tests/worker-tasks.test.js
- frontend/tests/run-worker-tasks-http.mjs
- docs/W7-D1-acceptance-report.md

共25项，删除0。未修改两份 W1-D3 交接文件；本轮起始和最终都没有它们的未跟踪变更，不伪称本地必然存在。测试进程及随机监听均已关闭，临时测试库/凭据已清理，未操作用户演示库。build、dist、node_modules、SQLite/WAL等仍被忽略，无此类跟踪产物。暂存区为空，不 git add/commit/push/merge。

最终实际核查：`git diff --check`、`git diff --cached --check` 退出0；15项新增文件逐项 `git diff --no-index --check` 无诊断；全部25项UTF-8严格解码及末尾换行通过。暂存文件数0，无删除或无关文件。按起始HEAD展开 CMake 注册名核查原34项均保留，目前37项。

常规 `git diff --stat` 仅列10项已跟踪修改（51插入、4删除），不包含15项未跟踪新增文件；不能用该stat漏审新增实现。Git 的 LF→CRLF 提示不等于空白错误，未通过暂存制造统计。

## 8. 保留风险与阶段结论

- 真实浏览器、390px、Console与实际用户网络故障尚未验收；自动化仅证明已覆盖范围，不声称生产压测、跨机器部署、崩溃/断电/磁盘故障通过。
- SQLite 单写者模式，busy_timeout=5秒；客户端同为5秒可能先超时，此时按未知结果处理，不重试写。没有新增幂等键/跨刷新请求回执服务。
- JS 不安全整数 ID/版本明确拒绝，不能取整；后端 int64 与溢出检查已验证。
- 离线退出不保证服务端 Session 已撤销、内存 Token 刷新重登、未完整登录限流等既有风险保留。
- 未实现转派 API、到场/进度、完工、验收、返工、评价或归档；只对既有 PENDING 转派夹具执行冻结要求的事务性失效，不冒称 W7-D2 完成。

**CampusFix W7-D1 接单/拒单及维修员任务工作台实现与自动化验证已完成，等待独立源码审查；未进入W7-D2。真实浏览器人工验收仍待补。**
