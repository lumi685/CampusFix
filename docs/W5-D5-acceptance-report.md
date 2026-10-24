# W5-D5 管理员权限、安全边界与跨模块一致性验收报告

记录日期：2026-10-24。状态：**本轮自动化集成验收完成，等待用户最终源码、Git与阶段验收；未进入W6。**

## 1. 基线、审计和实施边界

- 起始HEAD：`794bb240383010f684f901fafbe3c74c0a2220ae`，`feat(admin): add review and worker management views`；main与本地origin/main对齐，起始工作区/暂存为空。实际执行git status、log和branch -vv，未fetch，不声称本轮重新查询了远端状态。
- 审阅AGENTS、W5-D5计划、FR-002/005/006、API-03/07～16、角色/状态机/字段/架构/决策及W5-D1～D4报告、人工记录和progress-log，检查授权、认证、审核、编辑、人员、日志、前端抽屉及现有CTest/HTTP夹具。
- 已有22项CTest和前端认证9、报修/补充16、管理23项测试；已有单模块审核/补充/人员回滚测试。本轮新增统一多角色跨模块HTTP套件，以及先失败后修复的前端安全回归，不无理由改写原测试。
- 未修改任何后端业务实现、权限语义、写接口、Schema、正式seed、vcpkg依赖或W1冻结文档。所有数据库构造/授权/故障注入均只在独立临时库内；不创建正式账号密码或管理API。
- 未实现API-17/18、EligibilityFilter、DispatchStrategy/WeightedScoreStrategy、Top3、派单/转派或W7维修业务。TC-DSP及派单TC-CON不计入本轮已完成覆盖。

## 2. 统一真实数据库夹具及授权矩阵

新增`W5D5SecurityFixture.cpp`复用原WorkerTestData/ReviewTestData及真实migration/正式seed，随后在测试专用库建立独立主体。账号密码是不可登录占位值；随机Bearer Session来自既有安全Token实现，只放临时凭据文件，不放源码/日志/Git。HTTP使用正式campusfix_backend.exe，不链接测试路由。

| 测试主体 | 主要角色/有效授权 | 工单管理读取 | 审核 | 人员维护 |
| --- | --- | --- | --- | --- |
| demo_dispatcher（A） | dispatcher / AREA_SCOPE(A) | A | A | A |
| test_dispatcher_b（B） | dispatcher / AREA_SCOPE(B) | B | B | B |
| test_no_grants（0） | sysadmin / 无业务授权 | 无 | 无 | 无 |
| test_admin_r（R） | sysadmin / REVIEW(A) | 无 | A | 无 |
| test_admin_rb | sysadmin / REVIEW(B) | 无 | B | 无 |
| test_admin_b（B） | sysadmin / BUSINESS_READ(B) | B | 无 | 无 |
| test_admin_w（W） | sysadmin / WORKER_MANAGE(B) | 无 | 无 | B |
| test_admin_mix（MIX） | sysadmin / BUSINESS_READ(B)、REVIEW(A)、WORKER_MANAGE(B)、AREA_SCOPE(A) | 仅B | 仅A | 仅B；AREA_SCOPE(A)不能借给WORKER_MANAGE |
| test_admin_area | sysadmin / 仅AREA_SCOPE(A) | 无 | 无 | 无 |
| test_admin_global | sysadmin / BUSINESS_READ(NULL) | 全局只读 | 无 | 无 |
| reporter A/B | demo_reporter01/02 | 无管理视图；本人资源 | 无 | 无 |
| worker A/B | demo_worker01/04 | 无管理视图；本人/获准任务 | 无 | 本人只读 |

夹具还包含A/B待审核单、草稿、已退回/待派单/已拒绝单，6名不同技能/资质/状态/容量人员，以及有效未接单派单和已结束历史派单。A/B是固定演示位置ID1/2的测试简称，不在生产界面硬编码。授权恢复/撤销使用显式mode，顺序确定；每次mode前后检查业务历史摘要不变。

## 3. 测试计数与本轮真实结果

| 层次 | 本轮实际结果 | 退出码 |
| --- | --- | --- |
| MSVC Debug | 构建/链接成功，无编译或链接警告 | 0 |
| CMake/依赖 | 因注册测试重新生成；原依赖均already installed，未下载/升级 | 0 |
| 最终全量CTest | **23/23通过，95.12秒** | 0 |
| 新增跨模块正式HTTP | **180次请求，406次SQLite快照核查，9轮重叠竞争**；26.93秒，已包含在23项CTest中 | 0 |
| 新增C++ Service断言 | **0**；C++新增文件是夹具/SQL快照工具，不冒充Service测试 | — |
| 原审核Service / HTTP | 283个断言 / 149次请求，回归通过 | 0 |
| 原补充Service / HTTP | 60个断言 / 74次请求，回归通过 | 0 |
| 原Worker Service / HTTP | 62个断言 / 113次请求，回归通过 | 0 |
| 原W3/W4 | 认证、Guard/Policy、迁移/seed、工单创建/提交/列表/详情/时间线及报修集成均通过 | 0 |
| 前端认证 / 报修补充 / 管理 | **9/9、16/16、23/23** | 0 |
| 新前端安全 | **10/10**；含实际SFC setup方法执行，无DOM/浏览器 | 0 |
| 前端模块真实代理HTTP | 原W5-D4套件本轮重跑，**4/4、44次请求**；不计入新增180次 | 0 |
| npm run build | 成功；JS729.28kB/gzip237.92kB；保留>500kB chunk警告 | 0 |
| W2冒烟 | 直连与代理各两次200 JSON，request_id不同；首页200 HTML | 0 |
| SQLite | 新跨模块库、既有测试库及代理联调库integrity_check=ok，foreign_key_check无违规 | 0 |
| Git | diff --check及新增文件独立空白检查通过，暂存为空 | 0 |

本轮所有“通过”均有本轮实际执行；W5-D4用户14/14浏览器结论是此前用户人工反馈，不冒充本轮重跑。前端非浏览器单元/逻辑测试共58项（9+16+23+10），真实模块HTTP4项另计。定向、多轮全量重复执行不累加独立用例或请求数量。

### 新增180次HTTP结果分布

| HTTP | 次数 | 解释 |
| --- | ---: | --- |
| 200 | 51 | 42次串行读取/更新等成功，加9次并发获胜 |
| 201 | 10 | 通过正式接口建立临时草稿 |
| 400 | 4 | 畸形/非对象JSON，MALFORMED_REQUEST |
| 401 | 4 | 匿名、过期、撤销及禁用账号旧Session |
| 403 | 18 | 无动作/视图权限或不允许目标区域 |
| 404 | 20 | 不存在或隐藏范围外资源 |
| 409 | 17 | 8次串行状态/版本冲突，加9次并发失败 |
| 422 | 54 | 审核/人员参数、枚举、长度、字段或引用不合法 |
| 500 | 2 | 刻意注入技能写失败和审计追加失败，均完整回滚 |

180=162次串行请求+18次并发请求（9轮×2）。原始逐次路径、状态、业务码、request_id及前后摘要在被忽略的`backend/build/Testing/w5d5-evidence.json`，没有原始Token。不是所有拒绝都强制403：按冻结可见性区分403/404，所有错误带统一JSON/no-store和request_id。

## 4. 正式HTTP授权与非法输入核验

- 工单：A/B调度员管理列表不交叉，page_size=1的total仍为授权总量；跨区详情/时间线404。reporter/worker、无授权、仅REVIEW、仅WORKER_MANAGE和仅AREA_SCOPE管理列表403。全局/区域/重叠BUSINESS_READ不重复列表项，重叠AREA_SCOPE不增加total。
- 审核：A审核B、B审核A均拒绝；无授权/仅AREA_SCOPE不能审核；BUSINESS_READ(B)对可读B单无审核权返回403，REVIEW(A)不能审核B，REVIEW(B)不能审核A。MIX能审核A但不能用BUSINESS_READ(B)拼接出REVIEW(B)；R撤销REVIEW后立即404，停用A调度员旧会话401。
- 人员：A/B列表分别为1/2/3与4/5/6，WORKER_MANAGE(B)仅返回B子集。BUSINESS_READ/REVIEW/AREA_SCOPE不能替代人员维护；本人worker只有mine=true只读，不能提升容量或技能，且本人响应不含管理布尔/skill_options。
- 双区域更新：MIX只有WORKER_MANAGE(B)时不能借AREA_SCOPE(A)移B人员到A；补齐同一能力A/B授权后移动成功；撤销目标区域授权时403，撤销原区域授权时404。每次失败原档案/技能/账号/派单全库不变，随后受控恢复后才移动回来。
- 非法审核：未知/缺动作，缺/零/小数/布尔/字符串版本，APPROVE缺分类/紧急度、无效/停用分类、非法枚举、不允许的reason，RETURN/REJECT缺/空/空白/超长公开原因、错误携带批准字段，internal_note超过2000个Unicode标量、畸形/非对象JSON及未知字段，均拒绝。草稿、待补充、待派单重复审核、CANCELLED和旧版本也拒绝。
- 非法人员：容量0/101/低于真实任务量、技能0/6级、不存在/停用技能、重复技能、非布尔certified/active/emergency_duty、缺完整必填、禁止user_id/worker_no/users.role、无效/停用/楼层/房间区域和越权目标均拒绝。
- 每次串行GET或预期失败前后都使用独立SQLite连接做全库序列化摘要比较，包含全部workers、worker_skills历史、users、assignments、work_orders、status_logs及时间字段；不只比较状态或行数。授权mode是显式准备步骤，不混入失败请求快照；认证只读，不通过排除凭据/时间字段掩盖变化。

业务码实际包括AUTH_REQUIRED/AUTH_EXPIRED/AUTH_INVALID、ACTION_FORBIDDEN、RESOURCE_NOT_FOUND、ORDER_STATUS_CONFLICT/ORDER_VERSION_CONFLICT、VALIDATION_ERROR及注入故障INTERNAL_ERROR。返回白名单检查禁止密码/Token哈希、stored_path、原始metadata_json、授权记录和SQL/故障内部信息；内部审核备注只通过获准时间线读取。

## 5. 工单与人员的跨模块前后证据

### 审核、日志和补充链路

本轮临时样本（不得与用户人工库同ID混用）：

| 工单ID | 操作 | 状态变化 | version | 本单日志数 |
| --- | --- | --- | --- | --- |
| 10 | dispatcher A APPROVE | PENDING_REVIEW→PENDING_DISPATCH | 2→3 | 2→3 |
| 11 | dispatcher A RETURN | PENDING_REVIEW→PENDING_SUPPLEMENT | 2→3 | 2→3 |
| 12 | dispatcher A REJECT | PENDING_REVIEW→CANCELLED | 2→3 | 2→3 |
| 11 | 原报修人编辑、再提交、管理员APPROVE | PENDING_SUPPLEMENT→PENDING_REVIEW→PENDING_DISPATCH | 3→4→5→6 | 3→4→5→6 |
| 14 / 15 | R审核A / MIX审核A | 批准 / 退回成功，读取能力不被自动增加 | 2→3 | 2→3 |
| 20 | dispatcher B RETURN | B区单→PENDING_SUPPLEMENT | 2→3 | 2→3 |
| 21 | worker本人报修，由A管理员退回 | PENDING_REVIEW→PENDING_SUPPLEMENT | 2→3 | 2→3 |

每次合法审核核对actor_id、from/to、追加一条日志、响应request_id与审计记录一致，批准分类=2/紧急度HIGH；RETURN/REJECT公开原因与内部备注分别存储。A/B报修人及worker原报修人的公开时间线均无internal_note/私密标记，dispatcher可读获准备注。失败审核全库摘要一致，无状态/version/分类/紧急度/日志/时间字段半写。

工单11的原ID、order_no、首次submitted_at保持不变；编辑/再提交没有增加工单行，历史退回原因保留；v2旧审核在补充期被状态拒绝，再提交后被版本拒绝。管理员不能代原报修人编辑，原报修人不能代管理员审核。

实际请求ID示例：ID10批准`req_ee1c994a-c884-4dec-8a2f-fce8fdebad88`，ID11退回`req_d10349fc-d5ff-41cb-abc5-802efbd42461`，ID12拒绝`req_6053ab8a-dd74-413c-81c3-f2497a94eb53`，ID11最后批准`req_cad18588-834a-4141-86cc-f6272f69c9f2`。仅为本轮正式HTTP测试回执，不是浏览器证据。

### 人员容量单字段更新与历史保护

人员1真实基准为active=0、emergency_duty=1、OFF_DUTY、capacity3、有效任务2、skill1 Lv4持证/skill2 Lv2无证；只修改capacity4后：

- active=0、emergency_duty=1、OFF_DUTY、service_area_id及两技能的ID/等级/持证/活跃状态/effective_at均保持。
- users.role=worker、users.status=ACTIVE不变，不把人员停用等同账号停用。
- 其他人员及其技能逐列摘要不变；停用档案和容量更新均不自动结束/改动既有assignments、工单或历史。
- API-14与API-15均读到active_tasks=2，capacity4、remaining2；统计只按ended_at IS NULL，历史结束派单不计入，未接单有效派单仍计入。
- 完整PUT会按既有逻辑更新updated_at；“其他字段保留”不被夸大成审计更新时间也永远不变。API-16无expected_version，未新增乐观锁。

### 注入失败与并发

- 技能故障Trigger在后续技能INSERT时抛错，确保此前workers UPDATE和另一技能变更已进入同一事务；HTTP500后全库摘要与之前一致，全部资质与旧关联历史回滚。
- 审核审计INSERT故障Trigger使主体更新后追加失败，HTTP500后工单与历史整体回滚。Trigger只在临时库创建/删除，不修改001迁移或正式Schema。
- 9轮审核竞争：APPROVE/APPROVE、APPROVE/RETURN、APPROVE/REJECT各3轮。独立HttpClient连接的两个SendAsync同时挂起在真实SQLite写锁门闩后，确认二者均未完成才释放；不是两个串行调用，也不是Service调用冒充HTTP。
- 每轮恰好200/409，version2→3、日志2→3；日志actor/request_id和分类/紧急度均属于获胜请求，不含失败请求的有效历史。交替发送顺序，最终实际包含RETURN胜出（ID26）和REJECT胜出（ID29）；失败APPROVE没有改变原分类/紧急度。其余轮APPROVE获胜。
- 并发轮的整库必然含获胜写入，不能机械要求“冲突响应前后整库不变”；用最终仅一次合法状态/version/审计、无失败者日志和不涉及人员的摘要不变证明一致性。原始双方request_id/结果/前后行在evidence.races中。

## 6. 已复现缺陷、最小修复与失败历史

### 产品代码修复

| 项目 | 根因与复现 | 最小修复 / 结果 |
| --- | --- | --- |
| F1 非人员422误提示容量（低严重度UI） | adminError无条件附加容量说明；先写category_id/reason失败测试复现 | 只有max_concurrent_tasks字段附加容量提示，其他422通用说明；通过 |
| F2 旧负载快照（低严重度UI，含迟到响应） | AdminWorkersView.load不清workload或更新loadSequence；刷新成功/失败及晚到旧请求均复现 | 刷新清快照、使旧序号失效、清核对状态；加载中禁止新核对；通过 |
| F3 失权后残留抽屉（中等前端数据可见性风险） | 列表403/404虽清表格，仍保留已打开的编辑/详情/审核ID；执行实际SFC setup复现两个失败 | 人员/工单列表401/403/404时关闭旧抽屉；工单失去读取提示能力时也清理。无服务端越权读取漏洞被证实；回归通过 |

先新增8项安全测试得到5通过/3失败（F1、F2两种复现）；修复后8/8。随后新增失权抽屉两项得到8通过/2失败（F3），最小修复后最终10/10。SFC测试用已有compiler-sfc编译并执行实际setup/load/checkLoad方法，模拟异步API和Vue refs，不修改模板生成假结果；没有DOM或鼠标交互，不声称浏览器E2E。

### 测试工具失败与诊断

首轮新增HTTP套件在权限/输入/回滚及第一轮竞争（已得到200/409）之后，报`Concurrency gate failed`。未认定后端失败或放宽竞争条件。独立临时库诊断输出：

```text
READY=True
BEFORE_EXITED=False
AFTER_EXITED=True
EXIT_BEFORE_REFRESH=
EXIT_AFTER_REFRESH=
# 对照：子进程退出前缓存Handle
HANDLE_CACHED=True
READY=True
EXIT_CODE=0
```

根因是本机PowerShell5.1 Start-Process对象退出后ExitCode为空，Refresh也不恢复；缓存进程句柄后能正确读到0。只给测试锁夹具缓存Handle，保留退出码严格检查，不改业务。诊断日志在被忽略的build/Testing/w5d5-gate-diagnostic-*和w5d5-gate-handle-*，原stderr为空；上面空值不是补造一个非零退出码。

修复后定向通过，再运行完整回归；补齐竞争获胜字段断言并交替发送次序后最终23/23。中间全量90.01秒通过是历史结果，最终记录用95.12秒，不累加轮次。没有本轮MSVC编译失败或库升级。

## 7. 命令、原始输出与测试收尾

```powershell
# 项目根目录；复用已安装依赖和现有build
& "D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build backend/build --config Debug --parallel 4
& "D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir backend/build -C Debug -R '^w5d5_security_http$' --output-on-failure
& "D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir backend/build -C Debug --output-on-failure
# frontend目录
npm.cmd run test:auth
npm.cmd run test:orders
npm.cmd run test:admin
npm.cmd run test:security
npm.cmd run build
```

前端真实模块HTTP沿用W5-D4的独立Worker HTTP夹具准备新库/临时随机凭据，两个终端环境变量CAMPUSFIX_DB_PATH指向新库，后端8848/Vite5173；Node测试的CAMPUSFIX_ADMIN_TEST_BASE设本机Vite、CAMPUSFIX_ADMIN_TEST_CREDENTIALS指临时文件，执行`node.exe --test frontend/tests/admin-http.test.js`。响应来自正式Drogon，不模拟返回；初始化方式见W5-D4报告第5节，不对用户原库执行--init。

```powershell
# 服务已在本轮独立环境启动后，从根目录
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke.ps1
sqlite3.exe <独立临时库> 'PRAGMA integrity_check; PRAGMA foreign_key_check;'
git diff --check
```

实际最后冒烟回执摘要：

```text
PASS frontend modules made 44 real proxied HTTP requests (not browser E2E)
ADMIN_HTTP_EXIT=0
PASS backend #1 request_id=req_b70d2faf-d846-4e32-8de3-82cf1a5fb535
PASS backend #2 request_id=req_3a3d9014-3b15-4c3a-8aaa-4389c6a44cc9
PASS proxy #1 request_id=req_6986141c-4165-4de3-adc8-c575de27d270
PASS proxy #2 request_id=req_7ffcbf76-c6e0-40c0-8266-aab185e6b596
PASS all W2-D5 smoke checks
SMOKE_EXIT=0
ok
INTEGRITY_EXIT=0
FINAL_LISTENERS=0
```

证据：新增跨模块完整JSON在`backend/build/Testing/w5d5-evidence.json`，全量原始输出在`backend/build/Testing/Temporary/LastTest.log`。旧套件在本轮重跑后仍更新自己的w5d1/2/3-evidence输出名，不把旧回执与本轮混用。

新HTTP服务使用随机端口，Process对象只管理本轮启动PID，锁夹具同样自行等待/收尾；临时库/Token/凭据目录最终删除，已核对不存在遗留campusfix-w5d5-*目录。运行日志扫描无原始Session Token和内部备注标记。并发测试凭据不在报告JSON内。

固定端口联调前确认无占用；仅启动并清理本轮后端PID40012与Vite39484。环境变量在finally恢复，结束后8848/5173监听为0。该次服务日志在被忽略的`backend/build/Testing/w5d4-live-76feec1836c9406681511b8c1bd5013a/`（复用测试命名，不表示旧结果）；库及凭据已清理。独立门闩诊断也仅删除自己的临时数据库/凭据，保留无敏感日志。没有停止用户原服务或其他Node、VS、系统进程。

## 8. 完整Git范围

仓库根：`G:/A_Study/CppProject/CampusFix/`。**修改6、新增4、删除0，共10文件。**

| 类型 | 相对根目录路径 | 用途 |
| --- | --- | --- |
| 修改 | backend/CMakeLists.txt | 注册一个测试夹具与w5d5_security_http，生产目标不变 |
| 修改 | frontend/package.json | 增加test:security命令，不加依赖 |
| 修改 | frontend/src/utils/adminOrders.js | F1字段级422提示修复 |
| 修改 | frontend/src/views/AdminWorkersView.vue | F2负载失效及F3失权抽屉清理 |
| 修改 | frontend/src/views/AdminOrdersView.vue | F3失权详情/审核抽屉清理 |
| 修改 | docs/progress-log.md | 本轮审计、测试、失败/修复与边界记录 |
| 新增 | backend/tests/integration/W5D5SecurityFixture.cpp | 统一虚构角色/范围/工单/负载、SQL快照与锁门闩 |
| 新增 | backend/tests/integration/W5D5SecurityHttpTest.ps1 | 正式HTTP矩阵、全库不变、事务回滚和重叠竞争 |
| 新增 | frontend/tests/admin-security.test.js | 10项逻辑与实际SFC setup安全回归 |
| 新增 | docs/W5-D5-acceptance-report.md | 本报告 |

`git status --short`只包含上述文件；无无关未跟踪文件，原两份W1-D3交接文件未创建或修改。暂存为空，未执行git add（含-N）、commit或push。普通diff --stat不含4个新增文件，另用no-index做新增空白/数量核对。diff --check退出0；LF→CRLF是Git提示，不是业务或编译警告。构建、SQLite/WAL/SHM、凭据、node_modules/dist及运行时证据继续被忽略。W5-D1～D4报告及人工记录保持原样。

## 9. 剩余风险、未验证项与W6准备度

- 本轮测试范围内未发现尚未解决的高严重度服务端越权或半写入缺陷；F1/F2/F3均已复现、最小修复并通过回归。测试只能证明列出的场景，不是形式化证明所有安全问题不存在。
- API-16没有版本/expected_version，人员并发最后写入覆盖风险仍存在，不宣称人员乐观锁。API-14获准管理只读扩展继续有效，worker本人受众隔离回归通过。
- 完整登录限流尚未完成；内存Token刷新需重登、离线退出不保证服务端撤销等W3/W5已知风险未改。发布前仍需处理，未由本次集成测试“消除”。
- 原大chunk警告与有界目录/人员检索限制保留，不做无关拆包或新端点。未测物理COMMIT I/O失败、断电、磁盘满、持续高压、跨机器首次部署，Trigger回滚不能替代这些故障。
- 本轮没有操作真实浏览器或新增截图。W5-D4的用户14/14人工验收继续作为此前结果保留；本次三处前端修复已做实际setup/异步逻辑、构建与HTTP回归，但视觉/点击补验收未执行，不伪造为已通过。
- 当前认证/授权/审核/补充/人员基础在本轮自动化范围内具备申请W5-D5最终验收的条件。经用户源码/Git/阶段批准后，方可另行开始W6；本轮没有实现或测试API-17/18或算法，也不将审核竞争冒充派单TC-CON。**完成后暂停，等待最终验收。**
