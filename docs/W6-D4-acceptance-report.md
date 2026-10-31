# W6-D4：真实数据推荐与事务派单验收报告

日期：2026-10-31。执行主体：Codex；测试数据全部为隔离临时库中的虚构数据，不代表真实校园人员或生产派单。人工与独立源码审查尚待执行。

## 1. 基线与范围

- 分支：`feature/w6-dispatch`。
- 起始 HEAD：`1fd636322bef80ed741ae9f6c9bca8e7906ef82a`。
- 开始时实际 `git fetch origin` 成功；HEAD 与更新后的 `origin/feature/w6-dispatch` 一致，工作区/暂存区干净。`origin/main...HEAD` 为落后 0、领先 3；收尾再次核对 HEAD 未变化。
- 对应 FR-002/006/007/008/013、T-10/T-11、冻结 API-17/API-18。复用既有 AuthGuard、AuthService、AuthorizationService、SqliteConnection/Transaction、WorkOrderStateMachine、StatusLogRepository、EligibilityFilter 与 WeightedScoreStrategy。
- 新接口为真实 C++ Drogon 路由；不使用假候选 Controller。未修改冻结需求、Schema、seed、状态机、既有算法、AI 或前端；未开发接单、拒单、转派、维修及 W6-D5。

## 2. 文件清单

相对项目根目录 `G:\A_Study\CppProject\CampusFix`：

修改：

1. `backend/CMakeLists.txt`：连接新分层代码；JsonCpp 公开头依赖传递；新增四项 CTest，原 30 项保留。
2. `backend/src/main.cpp`：注册两个正式调度路由，不调整正式服务器线程数。
3. `backend/src/middleware/AuthGuard.h`。
4. `backend/src/middleware/AuthGuard.cpp`：增加默认关闭的数据库忙映射参数，仅新调度路由启用；原路由默认认证与错误语义不变。
5. `docs/progress-log.md`：追加本阶段实测记录。

新增：

1. `backend/src/controllers/DispatchController.h`
2. `backend/src/controllers/DispatchController.cpp`
3. `backend/src/repositories/DispatchRepository.h`
4. `backend/src/repositories/DispatchRepository.cpp`
5. `backend/src/repositories/AssignmentRepository.h`
6. `backend/src/repositories/AssignmentRepository.cpp`
7. `backend/src/services/DispatchService.h`
8. `backend/src/services/DispatchService.cpp`
9. `backend/src/services/AssignmentService.h`
10. `backend/src/services/AssignmentService.cpp`
11. `backend/tests/integration/DispatchRepositoryTest.cpp`
12. `backend/tests/integration/DispatchTestData.h`
13. `backend/tests/integration/DispatchServiceTest.cpp`
14. `backend/tests/integration/DispatchHttpFixture.cpp`
15. `backend/tests/integration/DispatchHttpTest.mjs`
16. `docs/W6-D4-acceptance-report.md`

合计预期修改 5、新增 16、删除 0，共 21 文件；最终 Git 实测范围见收尾核验。

## 3. 数据适配与信任边界

### 工单、人员、技能和位置

- 读取 `work_orders` 的实际 ID/version/status、审核后的 category_id 与 approved_urgency；绝不以 requested_urgency、AI 建议或 HTTP 字段替代审核事实。
- 联查有效 fault_categories/skills，读取必需技能、两级资质要求及目标耗时。必需配置缺失/停用、历史读取失败或时间不可解析均阻断操作，不伪装为无候选或零历史。
- 人员来自 users/workers，保留真实角色、启停、可用性、容量、值班等输入；实时负载直接聚合 `assignments WHERE ended_at IS NULL`。不复用分页 API-14 或客户端缓存。
- 仅有效 worker_skills 和有效 skills 能形成技能；完整查询成功后才标记 skillsLoaded。空结果与 SQL 失败明确区分。
- 位置从真实 locations 批量读取，验证每个节点存在、active、无环、直接父关系与类型，直到唯一 CAMPUS 根；ancestors 顺序为最近父节点到校区。campusId/buildingId 取真实节点类型。
- ROOM 必须经 FLOOR/BUILDING/CAMPUS；公共 AREA 可挂在校区、建筑或楼层，保留 D2/D3 已验收的校区公共区域语义，不把 SRS 的建议图变成新限制。断链、停用、错误类型及循环均失败关闭。
- 候选池为服务区域 ID 位于本工单已验证祖先链（含工单位置本身）的全部 workers；不预先按技能、在岗或有效性裁剪，不分页。它包含 P0 所有可能覆盖该位置的校区/楼宇人员，其他楼宇/校区不纳入统计。更细粒度的非法服务区域即使进入输入，也不能绕过既有领域校验。跨区放宽保持关闭。
- `excluded_summary` 只统计上述相关池，独立原因分别计数；不返回无关人员名单、排除人员详情或私人联系方式。

### 历史与最近 20 样本

- 效率联查 assignments→work_orders→fault_categories；要求正常 COMPLETED 派单、结束/接单/最终提交时间存在，工单为 COMPLETED/CLOSED，且该派单是最终归属，不将转派/拒单或旧负责人计入。
- UTC 日历与最多九位小数显式解析，不使用本地时区/DST；保留小数分钟。耗时取 accepted_at 到当前最终 result_submitted_at，检查非负且结束不早于提交；零耗时原样交给领域公式处理。
- 评价联查最终正常完成人员的 evaluations，保留 ID、真实 1～5 星；按评价时间及 ID 排序。
- SQL 先限定有效业务集合，不先 LIMIT 20 再丢弃。批量读取后按解析后的 UTC 时间降序、64 位 ID 降序保留各人最新 20 条；`Z` 与 `.000Z` 的同一时刻按 ID 破同分，不按文本误排序。
- Repository 不计算冷启动或分数。仅查询完整成功才产生已加载的空/非空原始样本；0/1/2 样本的 60 分由真实 WeightedScoreStrategy 独立计算。
- Repository 测试包含 30 条有效完成、25 条拒单及无接单/无提交/取消等反例，验证筛选后 20 条、最终归属、最终返工提交、UTC 闰日/小数及异常；另通过真实 SQLite 1/2 样本验证冷启动计数。
- 实测适配器每次 load 固定 **6 次批量 SELECT**，三次 load 为 18 次，全部 sqlite3_stmt_readonly；这不包含身份/授权查询，也不表示历史扫描行数恒定。

## 4. API-17：只读推荐

`GET /api/work-orders/{id}/dispatch-candidates`

- 复用 AuthGuard；在 Service 的同一个 `BEGIN` 读快照中再次认证并检查 Recommend 权限、资源范围、PENDING_DISPATCH 状态和完整输入。没有客户端 expected_version。
- dispatcher 受 AREA_SCOPE 限制；sysadmin 必须有 DISPATCH 本身覆盖该位置。AREA_SCOPE 或 BUSINESS_READ 单独均不能调度；非可见资源 404，可见但无动作权限 403，无有效 Session 401。
- Controller 的专用连接开启 query_only；Service/Repository 只 SELECT，不写推荐快照、日志或业务状态。
- 直接调用已验收的 WeightedScoreStrategy，按领域顺序拆前三与全部其余合格候选。白名单返回六项分数、总分、样本量、真实负载、原因和风险；无候选仍 200，返回空数组和多原因摘要。
- 独立夹具对 16 张业务表的所有列做稳定序列化哈希，不仅比较计数或工单状态。正常 GET、失败请求及回滚逐次比对；不把浏览器 Network 观察称为数据库快照证明。

最终 HTTP 运行中的真实样本摘要（独立虚构库，非生产工单）：

```text
GET /api/work-orders/6/dispatch-candidates → 200 application/json
code=0, message=success, order_id=6, order_version=3
strategy=WEIGHTED_SCORE_V1
Top3 worker_id/score = 1/81, 2/74, 3/67
other eligible worker_id/score = 4/60, 5/53, 6/53
worker1 parts = skill100, area70, load100, efficiency60, rating60, duty0
worker1 sample_counts = efficiency0, rating0
```

完整实际候选、理由/风险和请求 ID 保存于忽略的 `backend/build/Testing/w6d4-evidence.json`；没有用文档示例代替执行结果。

## 5. API-18：事务派单

`POST /api/work-orders/{id}/assign`

- 严格 JSON 对象，仅 worker_id、expected_version、reason；拒绝重复键、尾随数据、未知字段、浮点/布尔/字符串 ID、非正整数、超过 int64、非法 UTF-8。理由最多 500 Unicode 字符；当前 rank>3 必须非空且不能仅为 Unicode 空白（含 NBSP、细空格、全角空格）。
- `BEGIN IMMEDIATE` → 同连接再次验证 Session/账号/授权范围 → 现有有效派单、状态/version 与 int64 上限检查 → 重新读取全部资格/实时容量/历史 → 真正重新评分 → 检查所选当前合格人员和最终 rank/reason。
- 插入 assignments，保存绑定 assignment_id 的 dispatch_records 最终权重、全部合格候选和排除摘要；CAS 更新工单为状态机 T-11 的 PENDING_ACCEPT、version+1；追加 ORDER_ASSIGNED 日志，最终复核身份/授权后提交。
- 候选快照保留所有排名及六项分数、理由、风险和样本量，不仅 Top3。日志仅写既有白名单 assignment_id/worker_id/candidate_rank/assign_reason，不将内部候选快照放进公开时间线或 public_note。
- 权限在事务内复核；外部撤销在写锁前提交则拒绝。写锁已取得后其他写事务只能排队，线性化顺序是派单先提交或撤销先提交，不宣称锁能观察尚未提交的外部撤销。测试还通过本事务触发器撤销验证最终复核与整体回滚。
- 真实部分唯一索引 `uq_assignments_active_order` 独立保护；唯一有效派单竞争 409，状态/version 409。重新读取后已满载按本任务允许的 `422 WORKER_INELIGIBLE` 拒绝；不可明确归类的 SQLite BUSY/LOCKED 为可重试 503，非预期故障 500，均无部分写入/SQL 泄漏。

最终运行中实际 Top1 响应：

```json
{"code":0,"data":{"assignment_id":3,"order_id":8,"status":"PENDING_ACCEPT","version":4,"worker":{"display_name":"Demo Worker 1","id":1}},"message":"success","request_id":"req_3bdbdccc-a7f9-43c5-ae13-9408f0ba2e27"}
```

独立 SQLite 查询确认 order8 的 assignments.id=3，dispatch_records.assignment_id=3，ORDER_ASSIGNED.assignment_id=3；actor_id=requested_by=assigned_by，日志 request_id 等于上述真实响应，快照 order_version=3，六名候选齐全，工单 version=4，worker1 有效负载立即为1。第4名有理由成功、无/空白理由拒绝；预览后改变技能使原 Top1 跌出前三，确认时重新校验理由且保存新排名而非旧 GET 数据。

## 6. 并发与故障证据

### 两类独立并发验证

1. **SQLite Service 测试**：两个 std::async、两个独立连接，第三连接先持写锁；trace 确认两者均尝试 BEGIN IMMEDIATE 才放锁。三类竞争恰有一胜，败者为409或422；订单/派单/快照/日志独立查验。另有排队期间已提交撤销 Session/授权的两项检查。
2. **真实 HTTP 测试**：先对正式 campusfix_backend 运行普通请求；另启仅测试可执行程序的四线程 Drogon，注册完全相同的正式 DispatchController，没有测试 HTTP 授权接口。两个 HTTP 请求并行到达，第三进程持锁；每个真实连接由仅测试程序启用的 SQLite trace 记录 BEGIN 尝试，达到2后才释放。没有用顺序请求或全局业务 mutex 代替并发；测试 trace 的文件/同步仅在测试程序，不进入正式事务实现。

最终全量运行的 HTTP 同步竞争：

| 竞争 | 放锁前 BEGIN 尝试 | HTTP 状态 | 成功数 |
| --- | --- | --- | --- |
| 同单、不同人员 | 2 | 200 / 409 | 1 |
| 同单、相同人员 | 2 | 409 / 200 | 1 |
| 两单抢最后容量 | 2 | 422 / 200 | 1 |

各组均独立查询确认仅一条有效派单、一条最终快照、一条新增派单日志；容量竞争败方仍为 PENDING_DISPATCH/version3。最终全表指纹分别为：

```text
52ea201791d2d53db4e54c6df71dcf0464439c9b19d7cd714dec0390a4528410
201e79d55b41e3126b9784e0caf3b6bc3901d8928b36a3ef2914edd3df836671
0c6412241d52dd0aac73bcf9f9d9578ba1f15b696752ecb7f6cf2a276a94e114
```

胜出请求顺序允许变化，不将某一客户端必胜作为正确性条件。真实 HTTP 503 通过另一连接持续持写锁超过现有 busy_timeout=5000ms 触发，放锁后同一工单重试成功。

### 回滚、错误和安全

- 对 assignments 插入、dispatch_records 插入、work_orders 状态更新和 status_logs 插入分别注入真实 SQLite 失败；HTTP 返回安全 500，16 表全部列指纹不变，订单仍原状态/version，无半条派单/快照/日志。
- 历史表临时不可读时，推荐和派单均失败；恢复表名后与失败前全表指纹一致，不用冷启动伪装读取成功。
- 直接绕过 Service 预检查插入第二条有效派单，真实数据库部分唯一索引拒绝且数据不变。
- 响应 DTO 精确字段检查；日志/响应扫描未发现夹具完整 Session Token、Authorization、内部审核标记或注入 SQL 错误。固定 token/password 没有进入跟踪文件；夹具凭据只在受控临时文件，结束时删除。
- `PRAGMA integrity_check=ok`，`foreign_key_check` 无违规；新测试不访问用户手工验收数据库。

## 7. 实测命令与结果

在项目根目录 PowerShell：

```powershell
$cmake = 'D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = 'D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
& $cmake --build backend/build --config Debug --parallel 4
& $ctest --test-dir backend/build -C Debug -R '^dispatch_' --output-on-failure -V
& $ctest --test-dir backend/build -C Debug --output-on-failure
& $ctest --test-dir backend/build -C Debug -N
git diff --check
git status --short
git diff --cached --name-only
```

- CMake 自动重配/生成成功，现有依赖全部 already installed，无下载升级。MSVC Debug 完整构建退出0，最终构建未见编译/链接警告；Drogon1.9.13#2、SQLite3.53.4#2、libsodium1.0.22#1、JsonCpp1.9.6不变。
- 新 Repository：**39 场景、132 断言**。
- 新 Service/Assignment：**13 场景、125 断言**。
- 新直接连接并发：**5 场景、22 断言**。
- 新 HTTP：**86 次记录请求、151 次16表全列快照、3组真实同步HTTP竞争**；不是86项 CTest，也不将重复运行累计。健康检查启动探测不计入86。HTTP 状态统计为200×13、400×4、401×9、403×9、404×7、409×5、422×32、500×6、503×1。
- 定向 dispatch 测试 **7/7，退出0，29.55秒**，包含原 D1 15场景104断言、D2 41场景1019断言、D3 57场景1149断言，旧断言未删除/弱化。
- `ctest -N` 实际 **34项**；从起始 HEAD 的 CMake 提取原30名，与当前注册集合比对，缺失0。新增 dispatch_repository、dispatch_service、dispatch_concurrency、dispatch_http。
- 最终全量 CTest **34/34，失败0，退出0，186.03秒**；原30项全部保留且实际通过，包括认证/Guard、授权Policy、迁移/seed、审核/补充/人员/目录/列表/详情/时间线及本地假模型AI回归。不是沿用D3的30/30结果。
- HTTP 测试确认自建随机端口全部释放、临时数据库/凭据清理；不停止用户已有服务。

### 真实失败及修复历史

- 首轮编译新增 Service 测试失败：同条auto声明推导不同类型，且函数 concurrency 与 MSVC Concurrency 冲突；拆分声明并改名 runRaces 后完整构建成功，未换编译器或依赖。
- 首轮 HTTP 86次业务断言满足预期，但重复等待已由信号终止的子进程导致 Node unsettled top-level await，CTest退出1；不能记为通过。修复清理时检查 exitCode 与 signalCode，证据和PASS仅在清理及端口检查后写出；重跑定向7/7通过。
- 首次失败留下的单个临时夹具目录经精确绝对路径核对后删除（包含虚构 Session 文件），不可恢复且无业务价值；没有删除用户数据库或其他文件。受限终端的CIM访问被拒后，本机核对无残留测试进程，未批量杀进程。
- 位置适配审查中修正公共AREA层级过严的问题，另删掉测试内临时对象的无用悬空引用；真实场景断言保留并补足。

## 8. Git 与边界核验

- 最终范围修改5/新增16/删除0，共21文件，暂存区为空，无无关未跟踪文件；完整清单见第2节。`git diff --stat`只统计既有跟踪文件，新增文件另行检查，未为得到Diff而暂存。
- 实际跟踪文件Diff：`5 files changed, 65 insertions(+), 5 deletions(-)`；不含16个未跟踪新增文件，不能将这个数字冒充21文件的全部代码行数。
- `git diff --check`退出0；16个新增文件另以no-index检查无空白诊断；全部21文件UTF-8与末尾换行检查通过。Git的LF→CRLF提示不是空白错误；no-index存在文件差异的退出1不等于空白失败，收尾检查脚本已区分。
- 不 git add/commit/push/merge；暂存区应保持为空。构建、证据JSON、本地数据库/WAL/SHM及缓存不纳入跟踪；没有修改已有W1-D3交接文件。
- 静态 diff 已核对 frontend、dispatch既有领域源码、migration、seed、冻结SRS/数据库/API/流程/权限、AuthService、AuthorizationPolicy、manifest均无变动。

## 9. 风险、未验证项与下一步

- 6次批量查询消除按人员逐条N+1，但仍扫描相关池历史及位置表，在大历史库中可能增加写锁持有时间。仅有小规模虚构数据正确性证据，**未做生产规模性能、长时间负载或跨机器验证**；未为性能擅改索引/Schema或截断事实。
- 严格数据异常会阻断整个推荐，不以局部结果掩盖坏配置/历史；后续应修复数据源而非默认60。
- 正式程序线程配置未改。多线程HTTP竞争由隔离测试可执行程序复用正式路由完成；不冒称已经改变正式服务吞吐配置。
- 等待独立源码与人工审查；本日无前端调度页，未运行新的浏览器/视觉验收、前端构建/单测、独立W2冒烟、额外AI-D3全链路脚本或真实DeepSeek调用。全量旧CTest中的本地假模型与HTTP回归不等于上述未运行项目。
- W6-D5前需审查候选池隐私/完整性、历史时间和最终归属、事务重查、最终排名理由与并发证据。没有开始下一日开发。

**CampusFix W6-D4 真实推荐API与事务派单实现及自动化验证已完成，等待人工和独立源码审查；未进入W6-D5。**
