# W5-D1：管理员工单审核 API 实施与实测报告

日期：2026-10-24。执行主体：Codex（本机 MSVC、SQLite 与正式 Drogon HTTP 自动化）；本轮没有管理员 Vue 页面或浏览器人工验收。

## 1. 基线与实施边界

- 基线：`main`，`a814f4e94eea598f95440a49627a6da082ee54e0`，与本地 `origin/main` 对齐；开始时工作区干净。未联网 fetch，远端状态以本地引用为准。
- 实现 FR-005 / API-13：`POST /api/work-orders/{id}/review`，对应 TC-WF-01～03、T-03/04/06、既有 D-011/012/016 权限和事务约定。
- 仅新增审核写入口；没有 API-07、API-12、派单、维修人员管理或前端审核页面。Schema、seed、W1 冻结设计、W3 认证/RBAC 规则和依赖版本均未修改。
- 历史两份 W1-D3 交接文件在本轮实际工作区中均不存在，也不在 Git 跟踪列表；没有创建、恢复或修改它们。
- 本轮没有 `git add`（包括 intent-to-add）、commit、push；暂存区保持为空。

## 2. 实现与安全顺序

1. 正式路由包装现有 AuthGuard，获取服务端认证的用户 ID 与新 request_id。
2. Controller 只接收 `action/expected_version/category_id/approved_urgency/reason/internal_note`，按 JSON 实际整数类型、正数、路径整数等规则检查；伪造身份、目标状态、编号及任意多余字段均拒绝。
3. `WorkOrderService::review` 开启现有 `SqliteTransaction`（BEGIN IMMEDIATE），再次通过现有 AuthService 验证 Session。
4. 现有 AuthorizationService 从 SQLite 重新读取启用状态、角色、有效授权、工单位置祖先、状态和版本；dispatcher 使用 AREA_SCOPE，sysadmin 只用 REVIEW 自己的范围，不借用其他能力的范围。
5. 权限判断先于资源状态/版本详情：可见但无动作权限为 403，需要隐藏存在性为 404；无有效身份 401。授权成功后才提供 409 当前状态/版本。
6. Service 校验动作与对应字段、真实有效分类，按实际动作查 WorkOrderStateMachine，不将 RETURN/REJECT 当作 APPROVE。
7. Repository 以参数绑定执行 `WHERE id=? AND status='PENDING_REVIEW' AND version=?` 条件更新，检查受影响行数。整数版本达到 int64 上限时返回 409，避免 SQLite 提升为 REAL 和 C++ 溢出。
8. 在同一事务中追加一条 StatusLogRepository 事件，然后 COMMIT。任何异常由事务析构回滚，Controller 不返回 SQL/内部异常。

三动作共同更新 status/version/updated_at；不改原 order_no/submitted_at/requested_urgency，不创建 assignment 或 dispatch_record。

| 动作 | 状态机 | 目标状态 | 额外变化 | 审计动作 |
| --- | --- | --- | --- | --- |
| APPROVE | T-03 | PENDING_DISPATCH | category_id、approved_urgency | ORDER_REVIEWED |
| RETURN | T-04 | PENDING_SUPPLEMENT | public_note 保存 reason | ORDER_RETURNED |
| REJECT | T-06 | CANCELLED | cancelled_at 为 UTC，public_note 保存 reason | ORDER_REJECTED |

APPROVE 审计 metadata 仅含 review_action/category_id/approved_urgency；RETURN/REJECT 仅 review_action。internal_note 与公开 reason 分列，不记录请求正文或 Token；API-13 成功仅返回 id/review_action/status/version，所有结果 no-store。现有时间线按独立读取权限投影，绝不返回原始 metadata_json。

原因复用现有 Unicode 标量计数与合法 UTF-8 检查，非空白 1～500 字符；已有报修表单校验语义不变。冻结 API/SRS 未规定 internal_note 数字上限；按用户本轮提交前修复授权，现限制为最多 **2000 个 Unicode 标量字符**，拒绝 NUL、非法 UTF-8 和非法控制字符。允许省略或空串；纯空白（空格、Tab、CR/LF、U+3000）与空串统一存 NULL，不修改非空备注正文。先验证长度再归一化，2001 个空格也拒绝；公开 reason 的必填和 1～500 字符约束不变。

## 3. 初轮实际构建、测试命令与结果

本节保留修复前一轮的真实记录；本次提交前修复后的最新结果见第 8 节，不将旧前端/冒烟结果冒充本次重跑。

在项目根目录执行（使用已安装工具，不写死到启动脚本）：

```powershell
& "D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build backend/build --config Debug --parallel 4
& "D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir backend/build -C Debug --output-on-failure
Push-Location frontend
npm.cmd run test:auth
npm.cmd run test:orders
npm.cmd run build
Pop-Location
# 本轮另以自有进程启动后端和 Vite 后执行；该脚本本身不启动服务：
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke.ps1
git diff --check
```

| 检查 | 本轮实际结果 | 退出码 |
| --- | --- | --- |
| CMake 配置 | CMakeLists 变化触发自动配置，Configuring/Generating done；依赖全部已安装，无新下载 | 0 |
| 最终 MSVC Debug 构建 | 成功，未见编译或链接警告 | 0 |
| 最终全量 CTest | **18/18**，39.52 秒 | 0 |
| 新 work_order_review_service | **238 个 SQLite/Service 断言**；包含真实双连接竞争 | 0 |
| 新 work_order_review_http | **106 次正式 HTTP 请求**（含两次并发）；不是测试专用服务路由 | 0 |
| 新纯 Policy 测试 | **0**；原 authorization_policy 保留并回归通过 | — |
| 前端 test:auth | **9/9** | 0 |
| 前端 test:orders | **7/7** | 0 |
| 前端生产构建 | 成功，保留既有 chunk>500 kB 提示，JS 664.18 kB / gzip 220.64 kB | 0 |
| W2 冒烟 | 后端直连两次 200 JSON、代理两次 200 JSON、首页 200；request_id 不重复 | 0 |
| SQLite integrity_check | 新 Service 与 HTTP 临时库均 `ok` | 0 |
| SQLite foreign_key_check | 两类临时库均无违规记录 | 0 |

HTTP 状态分布：200×23、400×4、401×5、403×7、404×4、409×7、422×53、500×3。两次并发分别为 200/409，已计入上述 106 次；启动就绪探测不计入业务 HTTP 测试次数。三个 500 是有意注入日志失败、分类/紧急度写入失败、取消时间写入失败，不是未修复业务故障。

CTest 其余 16 项覆盖原 W3 数据库/种子/密码/Session/Guard/RBAC 和 W4 创建、提交、目录、列表、详情、时间线及全链路回归。

调试历史保留：新增 HTTP 测试首次将 `data.field="metadata_json"`（明确拒绝的字段名）误判为泄漏数据库字段，导致测试失败；修正断言为检测实际敏感 JSON 键而非字段名字符串后通过，没有放宽生产响应白名单。随后增加版本极值保护和断言，再次全量运行得到上述最终结果。

## 4. 三种真实 HTTP 请求、响应与 SQLite 审计

以下均来自最终 CTest 的独立临时库，不是用户人工演示库。ID、编号相同不代表与旧人工样本为同一工单。Authorization 使用运行时随机 Session，仅在内存和临时凭据文件使用，下列记录不含 Token。

### APPROVE

`POST /api/work-orders/9/review`

```json
{"action":"APPROVE","expected_version":2,"category_id":2,"approved_urgency":"HIGH","internal_note":"REVIEW_PRIVATE_MARKER"}
```

实际 HTTP 200，Content-Type application/json，Cache-Control no-store：

```json
{"code":0,"message":"success","data":{"id":9,"review_action":"APPROVE","status":"PENDING_DISPATCH","version":3},"request_id":"req_d8bb2eb7-c9c0-44ff-bb82-2b29676a469f"}
```

SQLite：order_no=WX202610240001，category_id=2，requested_urgency=NORMAL、approved_urgency=HIGH；submitted_at=2026-10-24T04:01:08.056Z 保持不变；updated_at=2026-10-24T04:01:10.136Z，cancelled_at=NULL。新增 ORDER_REVIEWED，actor_id=2，PENDING_REVIEW→PENDING_DISPATCH，public_note=NULL，内部备注单独保存，metadata 仅三个审核白名单键；created_at 与 updated_at 相同，事件 request_id 与响应一致。

### RETURN

`POST /api/work-orders/10/review`

```json
{"action":"RETURN","expected_version":2,"reason":"Fictional public reason","internal_note":"REVIEW_PRIVATE_MARKER"}
```

实际 HTTP 200：

```json
{"code":0,"message":"success","data":{"id":10,"review_action":"RETURN","status":"PENDING_SUPPLEMENT","version":3},"request_id":"req_8b498eee-e204-4228-bad9-0f4f270fe339"}
```

SQLite：order_no=WX202610240002，category_id=1，requested_urgency=NORMAL、approved_urgency=NULL；submitted_at=2026-10-24T04:01:08.073Z 保持不变；updated_at=2026-10-24T04:01:10.352Z，cancelled_at=NULL。新增 ORDER_RETURNED，actor_id=2，PENDING_REVIEW→PENDING_SUPPLEMENT，公开原因为上述 reason，内部备注分列；created_at 与 updated_at 相同，request_id 与响应一致。

### REJECT

`POST /api/work-orders/11/review`

```json
{"action":"REJECT","expected_version":2,"reason":"Fictional public reason","internal_note":"REVIEW_PRIVATE_MARKER"}
```

实际 HTTP 200：

```json
{"code":0,"message":"success","data":{"id":11,"review_action":"REJECT","status":"CANCELLED","version":3},"request_id":"req_d23f6907-e2ca-4c68-8e1b-a3fa92aed2d4"}
```

SQLite：order_no=WX202610240003，category_id=1，requested_urgency=NORMAL、approved_urgency=NULL；submitted_at=2026-10-24T04:01:08.093Z 保持不变；cancelled_at=updated_at=2026-10-24T04:01:10.754Z。新增 ORDER_REJECTED（不是普通取消动作），actor_id=2，PENDING_REVIEW→CANCELLED，公开原因与内部备注分列，request_id 与响应一致。

三单原创建与提交事件逐字段保持不变，每单现恰好三个事件；所有审核 version 从 2 变 3。HTTP 比对列表、详情和时间线均一致，重复审核返回 ORDER_STATUS_CONFLICT 且全业务指纹不变。完整脱敏请求/响应/SQL审计保存在被忽略的 `backend/build/Testing/w5d1-evidence.json`，不会提交。

## 5. 验收要求覆盖说明

| 用户场景 | 本轮实际执行层级与检查 |
| --- | --- |
| 1～11 三动作与保留字段 | SQLite/Service + 正式 HTTP；状态、版本、审核字段、UTC、编号/首次提交时间/原日志、公开原因、无派单 |
| 12～15 身份/普通角色 | HTTP 匿名/非法/过期/撤销 401，reporter/worker 本人及当前维修任务不能审核；Service 再验证真实 Session |
| 16～25 能力/范围 | 两层验证 dispatcher A/B、撤销 AREA_SCOPE、sysadmin 无 REVIEW/BUSINESS_READ/AREA_SCOPE/STATS_VIEW 不替代 REVIEW、REVIEW 自己的范围和撤销、停用旧 Token；Service 额外校区/建筑/楼层/房间及全局 REVIEW |
| 26～38 参数/状态/失败无副作用 | HTTP 严格类型、畸形 JSON、未知/跨动作/伪造字段、分类有效性、版本冲突、原因长度与空白；Service 三动作逐一拒绝其余九种状态（含 CLOSED），验证 500 个汉字不是 500 字节，所有预期失败比对完整业务行/日志指纹 |
| 39～40 并发 | 两个真实 SQLite 连接同时进入审核，APPROVE/APPROVE 和 APPROVE/RETURN 均一成功一 409；另以两个管理员的正式 HTTP 并发 APPROVE/RETURN 得到 200/409，版本和事件只增一次 |
| 41～44 回滚/完整性 | SQLite trigger 故障注入：三动作日志失败、审核字段 UPDATE 失败、拒绝取消时间写入后失败；Service 与 HTTP 均整笔回滚；另验证 RAISE(IGNORE) 导致 affected_rows=0 时不追加日志；两类库 integrity/FK 通过 |
| 45～49 字段安全 | HTTP 和 Service 验证 reporter/worker 本人时间线公开原因但无内部备注；dispatcher 授权区内可读白名单；sysadmin REVIEW 不隐含 INTERNAL_NOTE_READ，单独授予后才可读取；REVIEW 不隐含 BUSINESS_READ |
| 50～52 读写一致/真实路由 | 正式 backend 可执行程序上调用 API-13、09、10、11；既有全链路 CTest 同时验证 create/submit，路由无冲突；不将 fixture 创建测试数据说成 API-06/08 的本轮审核 HTTP 调用 |

所有审核错误不回传 SQL、凭据、内部备注或原始 metadata；按预期失败逐次对比 work_orders/status_logs/assignments/dispatch_records/order_number_sequences 的全部列摘要，而不仅是版本和日志数量。授权/账号/分类变更为测试夹具专用，不存在生产测试后门。

## 6. 完整文件范围

新增 11 文件：

| 完整路径 | 用途 |
| --- | --- |
| G:/A_Study/CppProject/CampusFix/backend/src/controllers/ReviewController.h | 审核路由注册声明 |
| G:/A_Study/CppProject/CampusFix/backend/src/controllers/ReviewController.cpp | 正式 API-13、严格 JSON 解析与统一响应 |
| G:/A_Study/CppProject/CampusFix/backend/src/models/WorkOrderReview.h | 最小审核输入/结果模型 |
| G:/A_Study/CppProject/CampusFix/backend/src/repositories/ReviewRepository.h | 审核 SQL 访问边界 |
| G:/A_Study/CppProject/CampusFix/backend/src/repositories/ReviewRepository.cpp | 有效分类、参数绑定、条件更新与行数检查 |
| G:/A_Study/CppProject/CampusFix/backend/src/services/WorkOrderReviewService.cpp | WorkOrderService::review、事务授权、三动作与日志 |
| G:/A_Study/CppProject/CampusFix/backend/tests/integration/ReviewTestData.h | 临时数据、授权、完整业务指纹及完整性工具 |
| G:/A_Study/CppProject/CampusFix/backend/tests/integration/ReviewServiceTest.cpp | 238 个数据库/Service 断言、并发与回滚 |
| G:/A_Study/CppProject/CampusFix/backend/tests/integration/ReviewHttpFixture.cpp | 独立生产 HTTP 测试库与受控 SQL 断言；不链接进正式服务 |
| G:/A_Study/CppProject/CampusFix/backend/tests/integration/ReviewHttpTest.ps1 | 106 次真实 HTTP、失败无副作用、日志脱敏检查 |
| G:/A_Study/CppProject/CampusFix/docs/W5-D1-acceptance-report.md | 本报告与证据索引 |

修改 6 文件：

| 完整路径 | 原因 |
| --- | --- |
| G:/A_Study/CppProject/CampusFix/backend/CMakeLists.txt | 链接审核源文件并注册两个新 CTest |
| G:/A_Study/CppProject/CampusFix/backend/src/controllers/WorkOrderController.cpp | 挂接唯一新审核路由 |
| G:/A_Study/CppProject/CampusFix/backend/src/domain/WorkOrderValidation.h | 暴露最小共用 UTF-8 文字校验 |
| G:/A_Study/CppProject/CampusFix/backend/src/domain/WorkOrderValidation.cpp | 复用原有校验，允许可选内部备注空串；原表单规则不变 |
| G:/A_Study/CppProject/CampusFix/backend/src/services/WorkOrderService.h | 增加审核方法与模型引用 |
| G:/A_Study/CppProject/CampusFix/docs/progress-log.md | 追加本轮实测，不覆盖旧记录 |

删除 0。普通 `git diff --stat` 只显示已跟踪文件，不代表这 11 个新增文件已包含；本轮另外对新增文件使用 no-index 检查，不通过暂存来制造完整 Diff。

## 7. Git、安全与剩余风险

- git diff --check：退出 0，无空白错误；LF→CRLF 为 Git 提示，不是业务/编译错误。新增文件另行 no-index 检查；返回 1 表示存在新增差异，不是空白错误。
- 本轮使用临时库，测试 finally/作用域结束后关闭连接、停止自建服务、清理临时库和凭据。W2 冒烟只停止自建 PID 7104/25912，8848/5173 监听为 0；没有删除或写入用户原人工库。
- build、SQLite/WAL、运行时证据、node_modules/dist 均由现有规则忽略，不改 .gitignore。不向 Git 添加数据库、密码或原始 Token。
- 没有新的阻塞冲突。未做物理断电、磁盘满、COMMIT I/O 故障、持续高压或跨机器验证；trigger 回滚不能等同这些故障已经测试。
- 本轮没有浏览器管理员操作或截图；管理员 Vue 页面、报修人补充编辑/API-07 和后续业务仍未开发。内存 Token、离线退出、完整限流等历史风险保持，不借本轮改变认证语义。
- 当前为 W5-D1 **自动化验收完成，待用户功能、关键代码和 Git 文件完整性验收**；到此暂停，不进入 W5-D2。

## 8. 2026-10-24 提交前最小修复与最终重验

- 额外文件核查：实际 Git 根目录为 `G:/A_Study/CppProject/CampusFix`；`git status --short` 和未跟踪文件列表均无 `tatus --short`。以 LiteralPath 核查 `G:/A_Study/CppProject/CampusFix/tatus --short` 与上级 `G:/A_Study/CppProject/tatus --short`，两处均不存在，无法复核旧文件内容或来源。本轮没有删除任何文件，也没有执行 git clean。
- 约束依据：重新核查 API-13、SRS 和 status_logs 字段字典，仅规定可选字符串/TEXT，没有明确长度上限。依据用户本轮授权，将原 size_t 最大值替换为 2000 字符常量；不编辑冻结文件、Schema 或 seed，不改变审核授权、状态机、事务检查顺序或响应形状。
- 归一化：长度以原有 UTF-8 Unicode 标量计数，不按字节或 UTF-16 代码单元计数；可选空串和纯空白转 NULL，非空备注不 trim，reason 单独校验且完全不变。
- Service 新增边界：三动作的 1999/2000 字符成功并回读原值，2001 字符/超长空白拒绝且所有业务行与日志不变；省略/空串/纯空白均存 NULL，RETURN/REJECT 公开原因保持原值；非法 UTF-8/NUL 拒绝。四字节字符另验证 2000 个（8000 UTF-8 字节）成功、2001 个拒绝。
- 正式 HTTP 新增边界：三动作 2000 个汉字成功且 SQLite 原样保存，2001 个汉字返回 422 VALIDATION_ERROR、data.field=internal_note，无状态/版本/日志副作用；省略/空串/混合空白及 U+3000 存 NULL；补充四字节字符 2000/2001 的成功/拒绝。测试工单通过真实创建和提交 API 获得，不写入用户演示库。
- 本轮实际 MSVC Debug 增量构建退出 **0**，无编译/链接警告；没有配置依赖升级或新下载。命令沿用第 3 节完整 CMake 路径及 `--build backend/build --config Debug --parallel 4`。
- 本轮实际 `ctest --test-dir backend/build -C Debug --output-on-failure`：**18/18，退出 0，42.50 秒**；审核 Service **283 个断言**，正式审核/读写 HTTP 回归 **149 次请求**，新增纯 Policy 测试仍为 0。Review HTTP 用时 11.88 秒，作为全量 CTest 的一部分执行，不重复计数。
- SQLite 实检：Service 与正式 HTTP 临时数据库均 `integrity_check=ok`、`foreign_key_check` 无违规。原权限、三动作、并发及故障回滚检查继续通过；临时服务/凭据和库按现有清理逻辑收尾。没有重跑浏览器、前端构建或 W2 Vite 冒烟；第 3 节原结果仅作历史保留。
- 本轮仅修改 5 个已有 W5-D1 文件：WorkOrderReviewService.cpp、ReviewServiceTest.cpp、ReviewHttpTest.ps1、本报告与 progress-log.md；新增/删除文件均 0。git diff --check 退出 0，只有 LF→CRLF 提示。
- 用户已将上轮 17 文件暂存；本轮不执行 git add/commit/push，因此这 5 文件有**新的未暂存修改**。暂存内容仍为旧版本（17 files changed, 880 insertions(+), 2 deletions(-)），不能直接把旧暂存区视为包含本次修复。提交前请用户自行审阅并决定是否重新暂存这 5 文件。
- 到此暂停，等待用户验收；未进入 W5-D2。其他未验证风险仍按第 7 节保留。
