# W7-D2：转派申请、管理员审批与到场/维修进度验收报告

## 1. 基线与受控范围

- 日期：2026-11-01；分支 `feature/w7-workflow`。
- 起始 HEAD 与 fetch 后 origin/feature/w7-workflow 均为 `023c24646f13af655cb4d3907a59d7cd7900218a`，W7-D1已提交；工作区与暂存区起始干净。没有reset/stash/覆盖或合并。
- 依据原API-21/22/33、T-15～T-18、E-01/E-02和用户正式批准D-021实现。先完成API-34独立只读与HTTP测试，确认通过后才实现写接口。
- 已登记D-021，并最小同步SRS接口索引、API-34完整契约、FR-008说明及UI调用映射；保留原33接口冻结及已接受决策历史。没有修改Schema、seed、权限矩阵/Policy、状态机编码、W6过滤/评分/权重或AI行为，未引入新依赖。

## 2. API-34：真实申请发现与权限

`TransferController → TransferQueryService → TransferRepository`，使用专用SQLite连接，PRAGMA query_only=ON，BEGIN只读快照内重新认证、取角色/授权、计数及分页；参数全部绑定。

- view必填mine/managed；page/page_size默认1/20，最大100；order_id可选正int64；偏移有溢出检查，未知参数422。稳定created_at DESC,id DESC。
- mine只允许worker，按数据库requested_by等于认证用户过滤，包含本人历史申请和公开审批结果。不会赋予历史派单维修权限。
- managed只允许dispatcher的AREA_SCOPE或sysadmin的DISPATCH自身范围。不要求BUSINESS_READ/INTERNAL_NOTE_READ，也不由此获得这两项权限。仅返回PENDING、关联当前有效派单、工单为PENDING_ACCEPT/IN_PROGRESS的申请。
- 递归位置范围使用UNION和IN/EXISTS，不因重叠授权重复；total/items共享相同过滤和快照。跨区order_id与不存在筛选均返回空结果，无额外存在性提示；无有效能力403，停用/过期/撤销认证401。
- 固定11字段：transfer_request_id/order_id/order_no/assignment_id/reason/status/created_at/order_status/order_version/review_reason/resolved_at。ID与版本从SQLite整数读取；order_version来自当前work_orders.version，不是申请快照。无联系人、电话、内部备注、原始metadata、密码、Token、路径。
- 申请前版本4、随后真实接单使当前版本5：GET返回5，transfer_requests.order_version仍为4，已实际断言。
- 一致读专项：临时库WAL下，在COUNT与items查询之间由另一个连接真实提交新申请，本次items/total仍属于原快照，下一次查询才看到新增行。此为刻意外部写入测试，不是GET写入。

原API-10/11及时间线白名单未改。仅有DISPATCH的sysadmin实际通过API-34读取队列，同时对原时间线仍获404。管理员必须从真实队列选择request_id和当前版本，不提供猜ID输入框。

## 3. 写事务与版本语义

Controller严格解析JSON（重复键、非法UTF-8/surrogate、未知字段、非整数版本等拒绝），Service复用AuthGuard、AuthService、AuthorizationService、WorkOrderStateMachine、SqliteTransaction和StatusLogRepository。所有业务写入均单连接短BEGIN IMMEDIATE，提交前再次认证/检查授权，失败全部回滚。

| 动作 | 前置/归属 | 成功效果 | 工单版本 |
| --- | --- | --- | --- |
| API-21 申请转派 | 当前有效worker；PENDING_ACCEPT/IN_PROGRESS；期望版本；非空白1～500Unicode原因 | 插入PENDING申请，保存真实派单/申请人/申请时版本与UTC时间；TRANSFER_REQUESTED公开留痕 | 不变 |
| API-22 APPROVE | AREA_SCOPE/DISPATCH对应范围；真实PENDING申请属于该工单当前派单；当前状态/版本合法 | 原子更新申请审批人/原因/解决时间；旧派单TRANSFERRED结束且派单version+1；T-17/T-18回PENDING_DISPATCH并记录日志 | +1 |
| API-22 REJECT | 同上，两种决定均需1～500Unicode有效原因 | 仅申请REJECTED及TRANSFER_REJECTED日志；原派单、状态、容量保留 | 不变 |
| API-33 ARRIVED | 当前有效负责人且已接单IN_PROGRESS；版本一致；可选note≤500Unicode | 只追加WORKER_ARRIVED，关联真实assignment_id | 不变 |
| API-33 PROGRESS_NOTE | 当前有效负责人IN_PROGRESS；版本一致；note必填1～1000Unicode非空白 | 可多次追加WORKER_PROGRESS_NOTE，真实UTC顺序 | 不变 |

审批只复用 `auth.order(Action::Assign)` 的DISPATCH权限检查，**不调用其PENDING_DISPATCH操作状态前置**，而是独立按T-17/T-18检查。申请的历史order_version不是审批CAS版本；GET可审批也不替代API-22写事务重查。

每有效派单最多一个PENDING申请、每派单最多一次到场，复用既有部分唯一索引；重复请求409。真实SQL直接重复插入也已被唯一索引拒绝。批准有工单/派单int64溢出检查，不新增派单、不自动推荐或分配，不删除旧派单和dispatch_records。原负责人失去后续接单/进度等操作权限。

## 4. 测试方法与实际证据

所有数据为隔离临时SQLite库中的虚构夹具，迁移/seed不改。正向链路通过真实API-18开始、生产Drogon Controller及真实Vite代理；创建/提交/审核的前置复用既有Service。负例中直接构造跨区、历史派单、已结束、PENDING_VERIFY/COMPLETED及授权记录，不冒充尚未实现的完工/验收API。

### 4.1 独立SQLite测试

- transfer_query_service：**89断言通过**；本人/历史、角色、能力独立、范围/重叠、撤销/停用、当前版本、分页参数、query_only写入实际被SQLITE_READONLY拒绝；成功和失败的16表全列指纹无变化；含上述并发一致读快照。
- workflow_service：**10场景114断言通过**；申请不释放容量、申请后接单、拒绝/批准两种状态、错误身份/请求归属/状态/版本、Unicode边界、事件不改变派单/容量/结果时间、真实唯一索引、int64溢出、各SQL阶段回滚及事务中撤销授权失败关闭。
- workflow_concurrency：**8场景24断言通过**；并行申请、approve/approve、approve/reject、reject/reject、审批/拒单、审批/接单、到场/到场、审批/进度。
- 每组竞争由第三独立连接持写锁，确认两个连接均尝试BEGIN IMMEDIATE后才释放，非单进程业务mutex。相同申请只有一个最终审批结果；进度先成功再批准可合法串行双成功，批准先完成时原负责人进度403，不把“不改版本”的合法进度错误地算成必须冲突。
- 注入覆盖申请INSERT/日志、批准申请UPDATE/派单UPDATE/工单UPDATE/日志、拒绝申请UPDATE/日志、到场和进度日志失败，16表数据全部回滚。另有AFTER UPDATE触发撤销DISPATCH的测试，最终授权检查拒绝，并原子恢复授权及所有部分业务修改。
- PRAGMA integrity_check=ok，foreign_key_check无记录。无生产Schema修改；触发器只在临时测试库创建并移除。

### 4.2 独立真实HTTP与前端API

- transfer_query_http：**3场景、34次HTTP、60次独立16表指纹检查**。
- workflow_http：**10场景、156次HTTP、156次独立16表指纹检查**，包括查询场景及三条完整链路，不与前一项或重跑次数合计成独立覆盖。
- 链路A：实际派单→接单→到场一次→两次进度。工单仍IN_PROGRESS/version5；除status_logs外其余15表全列指纹不变；派单时间/结果时间/负载不变；公开时间线真实排序。
- 链路B：实际派单→API-21→从API-34真实返回项选择→前端API-22 REJECT。状态PENDING_ACCEPT/version4、派单/负载不变，申请已解决且公开审批原因可见；旧页面再次提交409。
- 链路C：实际派单→申请（版本4）→接单（版本5）→API-34返回当前5→DISPATCH-only sysadmin通过前端API-22批准。旧派单结束，工单PENDING_DISPATCH/version6，真实负载少一单，历史快照保留；原负责人进度403；管理员再GET API-17，**未自动派单**。
- HTTP还覆盖过期/撤销/停用、角色冒名、跨区隐藏、错误request_id/跨工单、旧派单请求、SUPERSEDED、PENDING_VERIFY/COMPLETED、旧版本、缺失/超长/Unicode空白/控制字符、400严格JSON及422字段、重复申请和到场、10个SQL故障点、8组同步并发、真实忙锁503。
- 使用生产前端API/flow发出真实POST后，客户端刻意丢弃成功回执，保持unknown_result，只GET核实、不自动重发；这是明确的客户端传输注入，不是假后端，也不是声称真实浏览器断网。
- 日志检查未发现完整随机Session。证据仅在忽略路径 `backend/build/Testing/w7d2-query-http-evidence.json` 与 `w7d2-workflow-http-evidence.json`，无账号密码或Token。结束后自有后端/Vite停止、随机端口释放，临时库/凭据已清理；没有操作用户演示库。

### 4.3 本次真实HTTP样本（均为临时虚构工单）

| 请求 | 实际响应摘录 | SQLite关联 |
| --- | --- | --- |
| POST /api/work-orders/9/transfer-requests，expected_version=4、reason=Fictional specialist needed | 201；transfer_request_id=4，order_id=9，status=PENDING，order_status=PENDING_ACCEPT | 申请与真实有效派单关联；重复申请409并返回同申请ID |
| API-34 managed选择申请4 → POST /api/work-orders/9/transfer-requests/4/decision，REJECT、expected_version=4 | 200；decision=REJECT，order_status=PENDING_ACCEPT，version=4，assignment_ended=false | 申请REJECTED；有效派单/容量与审批前相同；再次审批409 |
| API-34 managed选择申请5（当前版本5）→ POST /api/work-orders/10/transfer-requests/5/decision，APPROVE | 200；order_status=PENDING_DISPATCH，version=6，assignment_ended=true | assignment_id=6，TRANSFERRED，原快照保留；累计有效任务数5→4（释放恰好1单） |
| POST /api/work-orders/8/progress-events，type=ARRIVED、expected_version=5 | 201；id=27，order_id=8，assignment_id=4，order_version=5，created_at=2026-11-01T08:55:04.024Z | 真实日志WORKER_ARRIVED，重复409；后续两条进度保留同状态/版本 |

相应真实request_id：申请 `req_e11df341-b918-4a18-bfb6-dd99bb90ebac`；拒绝 `req_3e155f34-9e8e-4c59-ad8e-d572e7adc873`；批准 `req_57de756e-e2a8-4691-8544-4b12e2a5b9cb`；到场 `req_4c69b5f1-3991-4a59-b277-87718576830f`。这些是自动化HTTP证据，不是用户Network截图。

## 5. Web实现与自动化边界

- 维修员当前任务内提供申请、到场/进度、本人申请和公开时间线；另有独立“我提交的转派申请”分页区，任务结束后仍可查看本人历史公开审批结果，不赋历史操作权。说明均按文本渲染。
- AdminDispatchView嵌入TransferQueue；只从API-34真实队列选择申请和当前版本，approve/reject二次确认。仅有DISPATCH的sysadmin不调用详情/时间线，也不借字段白名单获取私密信息。批准回执明确PENDING_DISPATCH，并提供人工重新读取API-17入口，不自动创建负责人。
- 写入中防重；422保留备注草稿；409清旧版本/选择要求刷新；401/403/404清理本组件及父页面未授权状态；超时/5xx未知结果不重发。读取序号与身份/上下文generation分离，操作ticket防止旧POST释放新操作的锁；换用户/卸载晚到数据不能恢复。
- 到场成功后当前页面禁用到场按钮；页面重新进入时不从不含assignment_id的旧公开日志推断当前派单到场资格，仍由服务端唯一索引及409最终保护，不额外扩展原详情/时间线DTO。
- 前端新增 **35/35**；原182项断言保留，**全量217/217，退出0**。包含真实SFC setup/watch/模板编译、API载荷、受控错误与迟到响应测试，不冒充浏览器点击。旧SFC测试仅补注入新增子组件，未削弱原断言。
- npm.cmd run build成功、退出0，JS806.82kB/gzip259.64kB；保留>500kB chunk非阻塞警告，没有提高阈值掩盖。

## 6. 构建、回归和首次失败记录

实际使用原Windows VS 2026/MSVC、C++17、现有x64-windows vcpkg缓存。Drogon1.9.13#2、SQLite3.53.4#2、libsodium1.0.22#1均already installed，未更改baseline或下载新库。

```powershell
$cmakeBin = 'D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
& "$cmakeBin\cmake.exe" --build backend/build --config Debug --parallel 4
& "$cmakeBin\ctest.exe" --test-dir backend/build -C Debug -R '^transfer_query_' --output-on-failure -V
& "$cmakeBin\ctest.exe" --test-dir backend/build -C Debug -R '^workflow_' --output-on-failure -V
& "$cmakeBin\ctest.exe" --test-dir backend/build -C Debug --output-on-failure
# frontend目录
npm.cmd run test:workflow
# 全量前端实际命令（不是运行需要额外夹具的HTTP *.test.js glob）
node --test tests/auth.test.js tests/orders.test.js tests/admin.test.js tests/admin-security.test.js tests/dispatch.test.js tests/dispatch-security.test.js tests/report-assistant.test.js tests/report-draft.test.js tests/report-draft-handoff.test.js tests/report-draft-integration.test.js tests/worker-tasks.test.js tests/workflow.test.js
npm.cmd run build
```

- MSVC Debug完整构建成功、退出0，最终无新增编译/链接警告。CMake自动重生成成功，无环境切换。
- 补齐旧派单及COMPLETED审批负例后，最终完整CTest **42/42、失败0、退出0、253.35秒**。原37项注册名已静态展开核对，无遗漏；新增5项transfer_query_service/transfer_query_http/workflow_service/workflow_concurrency/workflow_http。此前完整回归也曾42/42通过（268.01秒），数字不累计。
- 既有认证/授权/数据库/审核编辑/派单及本地替身AI回归包含在全量CTest；不是本轮真实DeepSeek调用。
- 另用随机端口正式后端（CAMPUSFIX_DB_PATH为空）和Vite，实际执行既有scripts/smoke.ps1：直连/代理各两次200 JSON、不同request_id、首页200 HTML，通过退出0。服务/端口已释放，健康检查仍不依赖数据库。

首次问题及修复保留：新增C++测试漏校验头文件导致C3861，后来不同类型放在同一auto声明导致C3538，均按编译错误最小修复。首次前端回归因旧SFC夹具未注入新增子组件报ReferenceError，仅补测试依赖。新用例真实发现“核实读取序号误当上下文变化”导致未知结果共享锁未设置，已分离contextGeneration并复验；保留失败用例。没有删减/跳过失败断言。

## 7. 浏览器人工验收：未验证

按computer-use技能检查，native pipe不存在、browser access host unavailable，apps/browsers为空。没有桌面/390px实际点击、Console或截图证据，不能将Node、SFC、HTTP和SQLite测试当成人工验收。

在受控虚拟库中补验以下项目（不要把密码、Token、Authorization或敏感联系方式放入反馈截图）：

1. worker待接任务申请转派，取消不写入；确认后仍可接单，状态不变/版本不因申请递增，公开时间线可见申请。
2. 管理员从真实队列选择申请，拒绝后原任务/容量不变，worker本人申请历史展示公开审批结果。
3. 已接单任务申请转派，管理员批准后原任务移出当前列表，历史申请仍可查；管理员看到待派回执，人工重新推荐，不自动派单。
4. 到场成功一次，当前页按钮禁用；进度可多次提交，时间线真实排序；刷新重登后状态仍在，重复到场被服务器409拒绝且无重复事件。
5. DISPATCH-only sysadmin可读审批队列，无权详情/时间线/内部字段仍不可访问；跨区、无DISPATCH和历史负责人不能越权。
6. 双击、422草稿保留、409刷新、未知结果只读核实、退出/切换账号/页面后的迟到数据清理。
7. 桌面与390px布局、弹窗、分页、Console组件解析警告及Network真实201/200/409；分享前隐藏凭据。均等待用户真实反馈，不默认通过。

## 8. Git文件范围

以下路径相对 `G:\A_Study\CppProject\CampusFix`，完整范围共40文件：修改15、新增25、删除0，全部未暂存。

修改：

- backend/CMakeLists.txt
- backend/src/main.cpp
- docs/SRS.md
- docs/api-contract.md
- docs/decisions.md
- docs/requirements-trace.md
- docs/ui-wireframes.md
- docs/progress-log.md
- frontend/package.json
- frontend/src/components/AdminOrderDetailDrawer.vue
- frontend/src/layouts/AppLayout.vue
- frontend/src/views/AdminDispatchView.vue
- frontend/src/views/WorkerTasksView.vue
- frontend/tests/dispatch-security.test.js
- frontend/tests/worker-tasks.test.js

新增：

- backend/src/controllers/TransferController.h
- backend/src/controllers/TransferController.cpp
- backend/src/controllers/WorkflowController.h
- backend/src/controllers/WorkflowController.cpp
- backend/src/models/Transfer.h
- backend/src/repositories/TransferRepository.h
- backend/src/repositories/TransferRepository.cpp
- backend/src/services/TransferQueryService.h
- backend/src/services/TransferQueryService.cpp
- backend/src/services/TransferService.h
- backend/src/services/TransferService.cpp
- backend/src/services/ProgressEventService.h
- backend/src/services/ProgressEventService.cpp
- backend/src/services/WorkflowSupport.h
- backend/tests/integration/TransferQueryTest.cpp
- backend/tests/integration/WorkflowServiceTest.cpp
- backend/tests/integration/WorkflowHttpFixture.cpp
- frontend/src/api/workflow.js
- frontend/src/utils/workflow.js
- frontend/src/components/WorkerWorkflowPanel.vue
- frontend/src/components/WorkerTransferHistory.vue
- frontend/src/components/TransferQueue.vue
- frontend/tests/workflow.test.js
- frontend/tests/run-workflow-http.mjs
- docs/W7-D2-acceptance-report.md

常规git diff不含未跟踪新文件，独立审查必须同时读取以上新增项，未用git add -N改变暂存区。构建/依赖/测试证据、数据库及WAL文件仍被忽略，无跟踪产物。两份旧W1-D3交接文件未修改，本轮没有相关未跟踪变更，不假定本地必然存在。没有git add/commit/push/merge。

最终实际核查：git diff --check与git diff --cached --check退出0；25项新增文件逐项no-index空白检查无诊断，全部40文件UTF-8严格解码与末尾换行通过，暂存文件数0。常规已跟踪stat为15文件、115插入/10删除，另有上述25项未跟踪新文件，不能漏审。LF→CRLF为Git提示，不是检查失败。

## 9. 保留风险与W7-D3前置条件

- 浏览器/窄屏/Console尚未验收；跨机器、生产压力、真实网络中断、进程崩溃/断电/磁盘故障尚未验证，事务SQL故障测试不能替代这些测试。
- SQLite单写者、busy_timeout5秒，前端5秒可能先超时，必须保持未知结果，不自动重试；没有新增跨刷新幂等键或请求回执查询服务。
- 保留内存Token刷新重登、离线退出撤销不确定、完整限流未完成及大chunk警告。JS不安全整数明确拒绝，不截断SQLite64位ID。
- 下一阶段需要延续本轮当前有效派单归属/事务规则，维修提交时按冻结契约原子取代PENDING转派申请；本轮**没有实现**维修完工、验收/返工、评价/归档或自动重新分配。
- 等待本轮独立源码及人工验收，未经批准不进入W7-D3。

**CampusFix W7-D2 转派申请、管理员审批及维修进度事件实现与自动化验证已完成，等待独立源码审查；未进入W7-D3。真实浏览器验收仍待补。**
