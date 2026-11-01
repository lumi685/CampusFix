# CampusFix W7-D4 — 原报修人验收与返工验收报告

日期：2026-11-01。实现/自动化执行：Codex。真实浏览器人工验收尚未执行，不冒充用户操作。

## 1. 基线与范围

- 仓库：`G:\A_Study\CppProject\CampusFix`；分支：`feature/w7-workflow`。
- 起始HEAD：`6972bcf4bc80b9cd96d3fd4f57f026119b8add90`。实际fetch成功，本地/远端相同，工作区与暂存区干净，初始diff空白检查通过。
- 目标仅FR-009、API-24、T-20/T-21。复用AuthGuard、Action::VerifyPass/VerifyFail、AuthorizationService、WorkOrderStateMachine、workflow::note、SqliteTransaction、追加日志和既有公开详情/时间线。
- 不改Schema/seed、冻结契约、D-017权限语义、调度算法/权重/负载口径及AI；不新增角色能力，不实现评价、关闭、归档、附件、通知或自动验收。

## 2. 后端与事务

在既有WorkflowController中注册 `POST /api/work-orders/{id}/verify`，复用严格JSON解析、统一响应与request_id；approved必须是真实Boolean，expected_version为正int64，拒绝未知字段、重复键、非法类型/查询参数。false必须有1～500 Unicode字符非空白reason；true原因可选，提供时按既有文本限制校验。UTF-8、多字节中文、代理项、控制字符均走公共验证。

新增VerificationService/Repository，单连接短 `BEGIN IMMEDIATE`：

1. 从持久Session再次认证账户；通过Action::VerifyPass/Fail重新查询并校验主要角色reporter/worker且数据库reporter_id=认证用户ID。维修派单或管理授权不能替代该身份。不可见资源404，可见但无动作权403。
2. 核对PENDING_VERIFY及expected_version；读取当前真实有效派单、版本、worker、已保存摘要、accepted_at、result_submitted_at、rework_count及实际负载；拒绝无派单/未接单/缺结果、异常时间或已结束派单等不一致数据。
3. 校验工单版本、所需派单版本/返工次数的int64递增上限；UTC时间不得早于实际结果提交时间，结果时间不得早于接单。
4. T-20：带reporter/status/version/rework_count/原摘要条件更新工单COMPLETED、version+1、completed_at/updated_at；带原派单ID/工单/worker/version/有效性/原接单和提交时间条件更新原派单ended_at、end_reason=COMPLETED、version+1。检查sqlite3_changes=1。
5. T-21：只将工单IN_PROGRESS、version+1、rework_count+1、updated_at，保留摘要；派单整行保持原样，不清空accepted_at/result_submitted_at、不创建新派单、不重新推荐。
6. 追加ORDER_VERIFIED或ORDER_REWORK_REQUESTED，真实actor/assignment/request_id、前后状态与统一UTC；返工public_note=原原因，metadata仅新的rework_count。检查实际插入行数，不能忽略日志零行写入。
7. 结束前再次认证、重新校验原报修人动作资格，并验证保存的工单/派单/结果/时间、返工次数、该工单有效派单数及维修员真实负载：通过为原负载-1，返工为原负载。异常全部回滚，成功才commit。

T-20不自动关闭/评价、不写评分；原维修摘要、最终结果提交时间、旧日志和dispatch_records保持。W6历史效率仍由既有end_reason=COMPLETED、accepted_at与最终result_submitted_at计算，不改算法。

## 3. 前端归属、本轮结果与交互

新增 `OrderVerifyPanel`、`verifyFlow`、verification API封装，扩展OrderDetailView。

- 仅本人资格可靠确认后显示可执行验收入口。复用ownsReportedOrder，但以严格适配器验证服务端reported视图的page/page_size/total/真实安全整数ID；不能因URL、角色或维修负责人可读详情而开放。
- 归属和公开历史均有50页/5000条上限；超出、页缺失、错误、身份变化均失败关闭，不推断权限。
- 完整扫描API-11升序分页；验证total稳定、每页数量、UTC+id顺序及ID去重，按实际ORDER_COMPLETED/ORDER_REWORK_REQUESTED轮次取最终合法结果。不是第一页最后一条，不取旧轮次摘要或管理metadata。
- 前后两次真实API-10核对同一工单PENDING_VERIFY及版本相同；版本变化则关闭确认。完整历史导出已有公开返工事件次数用于严格核验回执rework_count，未能完整核验不会允许提交。旧/不完整历史宁可失败关闭；服务端数据库仍是最终权威。
- 时间线仅保留公开字段，actor仅id/display_name；不扩大API-10/11、列表或内部字段权限。
- 通过与返工均先二次确认；返工显示Unicode字数与原因验证，提交期间锁定，双击只发一次。成功回执必须200统一结构且真实order_id、approved、目标status、version+1、rework_count符合本轮完整历史；异常200视为未知，不能假装成功。
- 422留可修正原因；409清旧选择/版本及父详情，重新获取；401/403/404清未授权资料；网络/5xx未知，只GET核实、不自动重发，也不能仅凭最新状态证明原POST成功。
- 成功由父页面重读详情及时间线，不本地伪造状态；账号/登录状态/路由变化同步清理，generation防旧读取/POST回调恢复新账号资料。OrderDetailView补充身份监视，保留原编辑/补充功能。
- 返工后WorkerTasks原IN_PROGRESS查询和API-23继续可用，当前维修员通过既有分页公开时间线读取原因，不因旧result_submitted_at存在而禁用重提。

## 4. Service、SQLite与真实HTTP证据

新增 `verification_service`：**7场景100断言**；`verification_concurrency`：**5场景15断言**。使用虚构临时库，覆盖角色/归属、D-017、通过/反复返工、版本/次数/时间、溢出/不一致前置、各写入阶段ABORT和IGNORE零行更新/日志、事务末尾停用原报修人回滚。全表指纹包含16业务表全部列，不是只比行数。W6真实Repository确认已验收派单成为合法效率样本，实时负载与SQL计数一致。

`verification_http` 使用真实Vite代理、真实Drogon生产路由与隔离SQLite：**8场景、289HTTP、216次独立连接16表全内容指纹**；另有独立连接实际行检查。含101次真实进度事件，实际跨过100条时间线分页边界，前端读取本轮B而非A。重跑不累计；不是浏览器E2E。

### A：维修提交后验收通过

一轮已实际通过的HTTP样本：

```http
POST /api/work-orders/5/verify
Content-Type: application/json

{"approved":true,"expected_version":6}
```

```json
{"code":0,"message":"success","data":{"approved":true,"order_id":5,"rework_count":0,"status":"COMPLETED","version":7},"request_id":"req_5d186b4e-ee14-4881-a77f-5894372522c7"}
```

独立数据库检查：assignment1、worker1、version4、end_reason=COMPLETED，ended_at与工单completed_at/updated_at及新日志UTC一致；accepted_at=`2026-11-01T10:44:26.896Z`，result_submitted_at=`2026-11-01T10:44:27.167Z`均保留，ended_at=`2026-11-01T10:44:27.461Z`。有效派单数0、实际负载1→0，repair_summary=Result A，旧日志/调度快照不变，只追加ORDER_VERIFIED，request_id与回执一致。重复验收409且全表指纹不变。

### B：真实返工、再次提交、最终通过

所有前置业务均经真实HTTP创建→提交→审核→派单→接单→进度/维修提交，不使用UPDATE伪装业务操作。

工单6，首次结果A后版本6，返工请求：

```json
{"approved":false,"reason":"Fictional socket still broken","expected_version":6}
```

实际200：

```json
{"code":0,"message":"success","data":{"approved":false,"order_id":6,"rework_count":1,"status":"IN_PROGRESS","version":7},"request_id":"req_2dacfb8a-bfb1-45ca-9898-994169de2166"}
```

| 阶段 | 状态 | 工单版本 | 返工次数 | 实际负载 |
|---|---|---:|---:|---:|
| 提交A | PENDING_VERIFY | 6 | 0 | 1 |
| 要求返工 | IN_PROGRESS | 7 | 1 | 1 |
| API-23再次提交B | PENDING_VERIFY | 8 | 1 | 1 |
| 原报修人通过 | COMPLETED | 9 | 1 | 0 |

返工时原assignments整行与此前一致、摘要A与旧提交时间不变。A时间=`2026-11-01T10:44:29.952Z`，B时间=`2026-11-01T10:44:30.440Z`，最终保留B；ORDER_COMPLETED两条分别完整保留Result A/Result B，返工原因及次数可追溯，只有最终通过才结束同一派单。无新增assignment/dispatch_records。现有W6数据适配器真实样本actual_minutes与accepted_at→B的分钟数一致，不误用A。

Service另外做两次真实返工/重提后通过，保留3轮结果及2条返工日志；不复用W7-D3中模拟返工的SQL夹具冒充API-24验证。

### C：D-017 worker本人报修

工单7由demo_worker02通过真实API创建，由独立demo_worker01维修。当前维修员代验收403；原报修人worker02可返工，worker01通过API-23重提B，worker02通过验收。实际最终回执order_id7、approved=true、COMPLETED、version9、rework_count1。没有授予worker全局VERIFY能力。

### D：拒绝、并发与回滚

- HTTP覆盖无凭据、非法/过期/撤销Session、停用原报修人、其他报修人、当前/历史维修员、dispatcher，以及已有BUSINESS_READ+DISPATCH的sysadmin都不可代验收；跨区域不可见工单404，不能通过操作泄露内容。
- 旧版本/重复/非待验收/溢出拒绝；Boolean/版本类型、未知字段、重复键、不完整代理项、非法UTF-8/控制字符、空白和500/501中文边界实测，失败前后指纹不变。
- 独立第三SQLite连接持写锁，确认两个真实HTTP请求均尝试BEGIN IMMEDIATE后释放，覆盖通过/通过、通过/返工、返工/返工、验收/重复complete、验收/进度。前3组只一个200和一个409，且只有一条验收/返工日志；派单结束后维修员失去动作权限，其他后到写入可为403而非强行409。Service同样覆盖5组独立连接竞争。
- HTTP在通过路径工单/派单/日志3节点、返工路径工单/日志2节点逐一真实SQL故障注入，500且全16表回滚；Service另覆盖IGNORE零行及事务末尾身份失效。
- 真实SQLITE_BUSY返回503、无部分写入；客户端在真实提交后丢失成功回执进入unknown_result，有权GET显示COMPLETED也不声称原POST成功，不允许自动重发。
- 所有HTTP测试链路最终integrity_check=ok、foreign_key_check为空。测试结束清理自有服务/端口/临时库/随机Session，未操作用户的演示库。

## 5. 构建、回归与首次失败历史

实际工具：原Visual Studio2026/MSVC工具链、C++17，既有Drogon1.9.13#2、SQLite3.53.4#2、libsodium1.0.22#1。CMake配置/生成复用现有缓存，无升级/下载/新增依赖。Debug完整构建退出0，无新增编译/链接警告。

前端原241项全部保留，新增29项，最终 **270/270、退出0**。包含API/归属分页、历史分页/同时间戳/稳定性、异常回执、互斥/未知结果/迟到响应及实际SFC setup/watch/父详情身份路由清理。它们不是浏览器测试。

Node v24.18.1、npm11.16.0。Vite生产构建退出0，JS819.55kB/gzip263.35kB；保留原>500kB chunk警告，不提高阈值掩盖。

真实HTTP测试中实际启动Drogon/Vite并执行既有scripts/smoke.ps1（传本轮随机直连/代理地址）：HTTP200 JSON/唯一request_id及首页HTML通过、退出0，不以新写断言冒充原脚本。

首次失败如实保留：新增测试把派单结束后的旧worker预期写为409、把跨区域不可见管理员工单预期写为403；原策略正确返回403/404，按真实权限语义修正新测试，未改Policy。HTTP并发返工测试曾漏填必填原因导致422/422，补齐合法竞争输入并保留缺原因422负例。没有删除场景或弱化原45项/241项断言。

命令：

```powershell
$cmakeBin='D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
& "$cmakeBin\cmake.exe" --build backend/build --config Debug --parallel 4
& "$cmakeBin\ctest.exe" --test-dir backend/build -C Debug -R '^verification_' --output-on-failure
& "$cmakeBin\ctest.exe" --test-dir backend/build -C Debug --output-on-failure
# frontend目录
node --test tests/auth.test.js tests/orders.test.js tests/admin.test.js tests/admin-security.test.js tests/dispatch.test.js tests/dispatch-security.test.js tests/report-assistant.test.js tests/report-draft.test.js tests/report-draft-handoff.test.js tests/report-draft-integration.test.js tests/worker-tasks.test.js tests/workflow.test.js tests/completion.test.js tests/verification.test.js
npm.cmd run build
```

最终完整CTest及Git结项结果在下方补记，不把仍在运行的项目预写通过。AI回归为原本地替身测试，不冒充本轮DeepSeek调用。

## 6. 浏览器人工验收与风险

**桌面/390px/Network/Console实际页面验收未验证**。依computer-use技能检查本轮连接，apps/browsers为空，browser host/native pipe unavailable；没有登录、点击、截图或用户人工结果，不将HTTP/SFC测试当成人工浏览器验收。

建议在独立临时演示库由用户本人通过受控CLI隐藏输入设置/启用所需虚构账号，按以下步骤人工补验：

1. reporter创建并提交，管理员真实审核/推荐/派单，worker接单及提交A；reporter详情显示正确本轮结果和提交时间，待核验时按钮不开放。
2. 通过二次确认取消不写；选择返工填原因，确认回IN_PROGRESS，worker能读取原因且同一任务继续维修，负载不释放。
3. worker再提交B，reporter读取B而非A，通过二次确认；COMPLETED才释放负载，同一派单结束，A/B/原因均保留。
4. worker本人创建的单由另一worker维修，原报修worker能验收；当前维修员、历史维修员、管理员不能代验收，手输URL也不获得资格。
5. 409清旧资料、422留原因、双击一次POST；网络中断/丢失回执未知不重发；退出/换账号/切换工单旧数据不恢复。
6. 桌面和390px核查对话框、全文、按钮换行、字数与Console/Network，不期待尚未实现的评价/关闭按钮。

残余风险：有界5000条归属/日志核验超过上限会关闭前端验收入口而非猜测，需要人工确认最小未来读取方案；不擅自增接口。版本栅栏与前端确认不代替后端原子权限检查；SQLite单写者、超时/丢失回执、无跨刷新幂等证明、内存Token刷新重登、离线退出撤销不确定、完整限流/压测/跨机器/断电等仍未验证。用户公开填写的原因/维修结果按文本转义，不能承诺其中绝无用户主动填写的私人内容。

W7-D5前置：先完成本轮独立源码及浏览器验收；后续才按原契约实现一次评价及COMPLETED关闭。当前COMPLETED没有自动变成CLOSED，未创建评价或调整评分。

## 7. 完整文件清单

修改6个：

- backend/CMakeLists.txt
- backend/src/controllers/WorkflowController.cpp
- backend/tests/integration/WorkflowHttpFixture.cpp（仅隔离夹具命令：W6真实负载/样本检查与负例数据；无新增正式测试HTTP接口）
- frontend/package.json（仅测试命令，无新依赖）
- frontend/src/views/OrderDetailView.vue
- docs/progress-log.md

新增11个：

- backend/src/repositories/VerificationRepository.h
- backend/src/repositories/VerificationRepository.cpp
- backend/src/services/VerificationService.h
- backend/src/services/VerificationService.cpp
- backend/tests/integration/VerificationTest.cpp
- frontend/src/api/verification.js
- frontend/src/utils/verification.js
- frontend/src/components/OrderVerifyPanel.vue
- frontend/tests/verification.test.js
- frontend/tests/run-verification-http.mjs
- docs/W7-D4-acceptance-report.md

总17文件，删除0，均不自动暂存。默认git diff不包含未跟踪的新文件，完整源码审查须同时查看上述新增文件。不改W7-D3历史验收报告及原W1-D3交接文件。

## 8. 最终结项检查

- 最终完整CTest **48/48、失败0、退出0、280.02秒**，原45项注册名从起始CMake展开与实际CTest清单比较，遗漏0；新增verification_service、verification_concurrency、verification_http。首轮完整48/48的282.35秒保留为历史，不累计。
- 最终Debug完整构建退出0；前端270/270与最终Vite构建退出0，既有chunk警告保留。每轮返工派单整行不变的加强断言及详情页身份/时间线401/403/404清理均已实际复验。
- HTTP8场景/289请求/216次独立全表指纹，W2 smoke退出0，integrity=ok、foreign_key_check为空。报告HTTP响应样本取先前一轮实际通过的记录，不把重跑生成的新request_id/时间混成同一轮；本地忽略证据文件记录最终轮次。
- 修改6、新增11、删除0，共17文件，全部未暂存，暂存区为空；HEAD保持原值，无无关未跟踪项，不git add/commit/push/merge。
- 已跟踪部分 `git diff --stat`：6 files changed, 46 insertions(+), 8 deletions(-)；不含11个未跟踪新文件。`git diff --cached --stat` 为空。完整清单见第7节，新文件同样须独立审查。
- git diff --check、git diff --cached --check退出0；全文件严格UTF-8/末尾换行与新增文件no-index空白检查通过，LF→CRLF仅为Git提示。
- 冻结文档、Schema/seed、domain权限/状态机、AuthorizationService、W6调度算法与既有CompletionService、vcpkg清单及npm锁文件均未改变；build/依赖/SQLite/WAL/本地证据仍忽略，无跟踪产物。测试确认自有随机端口/临时库清理；最终8848/5173监听数0，未终止用户其他服务。

**CampusFix W7-D4 原报修人验收与返工实现与自动化验证已完成，等待独立源码审查；未进入W7-D5。**真实浏览器人工验收仍待补。
