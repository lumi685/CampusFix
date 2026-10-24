# W5-D4 管理审核与人员界面：最终验收报告

补记日期：2026-10-24。状态：**用户真实浏览器人工验收通过（用户确认14/14）；此前自动化验证通过，等待最终源码与Git审查。** 不进入W5-D5或W6。具体人工页面时间未提供，不由本补记日期推断。

## 1. 起始基线与边界

- HEAD为`5a11de88c8834991b25da72d8d3a6d1288c4e975`，main与本地origin/main对齐，已包含W5-D3提交。起始工作区/暂存为空；未fetch，不将本地引用核对冒充远端重新拉取。
- 阅读AGENTS、W5开发计划、冻结契约/原型/权限/架构/字段/需求/决策及既有验收日志，核对Worker/Review/Authorization/WorkOrderRead与前端路由、认证、目录和现有测试。
- 本轮只替换/admin/orders、/admin/review、/admin/workers三个占位页面；原报修/维修任务/派单/统计路由保持原有边界。没有新增正式REST路由、数据库字段、依赖、账号或授权后台。
- 未修改Schema、正式seed、W1冻结文档或W3权限语义。临时夹具使用虚构账号和随机Session，未触碰此前人工库，没有设置或记录演示密码。

## 2. 显式批准的API-14只读扩展

原管理列表缺少PUT必填的active/emergency_duty，没有独立技能目录。前端猜false或从当前页人员汇总技能，都会造成完整PUT覆盖真实档案或漏掉未分配技能。因此按本次用户正式批准扩展已有只读接口，而非声称冻结契约原来已包含这些字段。

精确扩展：

| 位置 | 新增字段 | 来源与受众 |
| --- | --- | --- |
| 管理视图data.items每项 | active:boolean、emergency_duty:boolean | workers真实列；仅已通过原管理鉴权的dispatcher/sysadmin |
| 管理视图data | skill_options:[{id,code,name,requires_certificate}] | skills WHERE active=1，code/id稳定排序；独立于当前页、筛选结果或是否有人 |
| worker mine=true | 无扩展 | 不包含上面两布尔字段，也不包含skill_options；原白名单保持 |

原items/page/page_size/total及原字段均保留。能力和区域过滤复用原Service，选项与列表同一只读快照读取；没有增加认证方式、授权或GET写操作。技能目录不返回私人数据、联系方式、账号/授权记录或密码/Token。API-15/16请求和成功响应不变，API-16仍是局部响应，前端刻意不把它当完整档案。

兼容性：旧管理调用方忽略新增字段即可；严格白名单测试显式更新为管理受众10字段，并保留worker受众8字段。冻结api-contract.md不修改，本报告和progress-log记录本次获准扩展。Worker模型/Repository新增读取及Controller受众投影，不改变管理写事务或资格规则。

## 3. 前端关键实现与安全边界

### 管理工单/审核

- AdminOrdersView始终调用API-09 view=managed，仅发送状态、category_id和分页，不传location_id。没有全量请求后自行按区域过滤。列表错误、空数据、加载、重试与分页收缩处理均有明确状态。
- 管理详情使用独立AdminOrderDetailDrawer调用API-10/11，不借用带my-orders权限的/orders/:id。显示公开时间线及实际响应中存在的internal_note/按键白名单management_detail；不stringify原始metadata、不使用v-html。
- /admin/review复用同一管理列表并默认PENDING_REVIEW。BUSINESS_READ控制列表可读，REVIEW仅作审核按钮提示；sysadmin只有REVIEW没有BUSINESS_READ时显示解释，不请求其他视图。capabilities仅菜单提示，不从scope_location_ids推断资源/能力范围，最终每次写入由后端判定。
- 审核抽屉每次从API-10读取当前版本，切换工单清空原因与内部备注。APPROVE只发送确认分类/紧急度，RETURN/REJECT只发送公开原因；可选内部备注独立。Unicode标量限长500/2000，控制字符及孤立代理项拒绝。三个动作均有二次确认，REJECT明确终止不可恢复。
- 写入中禁重；成功后重新读取详情并刷新列表。409、网络/5xx触发只读核对并锁定重放，重新打开后用户自行确认。核对时权限丢失清理旧管理信息；未实现动作不显示可执行假按钮。

### 人员管理/完整回填

- 列表使用API-14真实授权子集，状态/技能/分页筛选，真实API-15核对负载。技能名称和资质要求全部来自skill_options，不硬编码演示ID、不汇总本页技能冒充目录。
- 每次编辑通过授权、无筛选的管理列表重新定位目标人员，校验active/emergency_duty确为boolean、技能集合及有效选项完整；缺字段、失败、未知区域均不能保存。没有默认开关替代未返回值；添加一条新技能的level=1/certified=false只是用户明确新增行的初值，不是补齐既有档案。
- 独立serviceAreas逻辑经API-04取得真实校区/直接楼宇及完整显示文字，仅允许CAMPUS/BUILDING；不修改原CatalogPicker/catalogPath，不影响W5-D2位置回填。停用原区域明确显示其真实ID和错误，不静默选择另一项。
- PUT发送完整允许字段，不发送active_tasks、user_id、role或expected_version。容量1～100整数/不低于当前已知任务数，技能1～5整数且ID唯一。保存前确认影响，后端仍重新校验实时负载与旧/目标区域权限。
- 保存后忽略API-16局部响应，重新GET API-14完整档案和API-15负载；表格也刷新。422保留编辑；403/404清理失权数据；网络/5xx先读取核对并标记结果未知，禁止自动再次PUT。核对结果展示完整标志、区域、容量、技能及负载，而非冒充本次写入确定成功。
- API-16没有版本字段。本轮没有人员乐观锁，重开读取不能保证并发不会最后写入覆盖；界面确认和本报告均明确此限制。
- Element Plus新增ElDrawer、ElInputNumber、ElSwitch均显式导入并注册；ElMessageBox直接按需导入调用，不当成模板组件注册。保留原有组件注册及内存Token策略。

## 4. 本轮实际测试与退出码

本节保留此前W5-D4实施轮实际执行结果，不是本次文档补记重新运行；本次没有重跑构建、CTest、前端测试或HTTP。前端测试是逻辑/适配器/静态检查，正式HTTP和浏览器交互另列。

| 项目 | 实际结果 | 退出码 |
| --- | --- | --- |
| MSVC Debug | 构建/链接成功，无编译链接警告；复用缓存，无依赖下载或版本变化 | 0 |
| 首轮Worker定向CTest | 2/2通过，15.39秒 | 0 |
| 全量CTest | **22/22通过，66.10秒** | 0 |
| Worker Service | **62个SQLite/Service断言**（原60+2），含管理真实布尔及独立技能选项 | 0 |
| Worker正式HTTP | **113次请求**（原107+6），全库指纹验证GET/失败无副作用；包含回填与容量单字段修改 | 0 |
| API-13审核回归 | 原Service283个断言、正式HTTP149次，已计入22项CTest | 0 |
| W5-D2补充回归 | 原Service60个断言、正式HTTP74次，已计入22项CTest | 0 |
| W3/W4回归 | 原认证/权限、迁移/seed、创建/提交/读/时间线及报修集成通过 | 0 |
| 前端认证 | 9/9 | 0 |
| 前端报修/补充 | 16/16，含全部El模板组件导入/注册静态检查 | 0 |
| 新管理逻辑 | **23/23**；回填、缺字段失败关闭、局部响应后重读、技能/范围、409/422/未知请求/失权核对、重复点击等 | 0 |
| 前端模块真实代理HTTP | **4/4，44次请求**；走Vite→正式Drogon，不是浏览器E2E | 0 |
| npm run build | 成功；JS729.02kB/gzip237.85kB，保留chunk>500kB警告 | 0 |
| W2冒烟 | 直连/代理各两次200 JSON、合法动态request_id，首页200 HTML | 0 |
| SQLite | Worker Service/HTTP临时库和前端真实HTTP临时库均integrity_check=ok，foreign_key_check无违规 | 0 |
| Git检查 | diff --check和新增文件独立空白检查通过；暂存为空 | 0 |

定向与全量重复跑同一Worker套件，不累加成额外独立用例。没有新增纯Policy测试，原Policy在全量中回归。

### 真实HTTP关键证据

- Worker正式HTTP检查管理项active/emergency_duty布尔类型及技能选项精确白名单，worker mine=true没有扩展。空管理筛选仍含全部有效技能选项；停用技能被排除。
- Worker回填样本为active=false、emergency_duty=true、OFF_DUTY、capacity3、技能A Lv4持证/B Lv2无证；只改capacity4，重读逐字段一致。API-16仍不返回active，不以响应局部值拼完整快照。
- 前端模块通过真实API创建临时工单ID10：create201/v1 → submit200/v2 → RETURN200/v3 → 原报修人编辑200/v4 → resubmit200/v5 → APPROVE200/v6/PENDING_DISPATCH；同一ID。原报修人时间线可见退回原因但不含internal_note，dispatcher时间线可见获准备注。另一个虚构样本REJECT到CANCELLED。ID只属于本轮临时库，不是用户人工工单。
- 前端HTTP实测reporter/worker管理列表403，sysadmin可读但无审核能力的工单403、范围外工单404，无WORKER_MANAGE时人员列表403；旧审核请求409，空拒绝原因422；容量低于有效任务数422、目标区域越权403。前端错误只复制安全data.field，不带Axios config/请求头或原数据行。
- 事后核对未知网络/5xx、不自动重放及后续权限丢失的失败关闭，已通过逻辑夹具测试；正式后端触发器回滚仍由原CTest覆盖。**没有把这些写成真实浏览器断网或鼠标交互通过**。

### 测试调试历史（不抹除首次失败）

1. 新前端HTTP首轮3/4、32次请求：测试错误预期“跨区sysadmin审核403”，实际后端正确隐藏资源返回404。仅修正测试，补充可读但无审核权限403的独立断言，未放宽权限。
2. 第二轮3/4、39次请求：测试构造完整API-07编辑时遗漏expected_visit_at，实际422/data.field=expected_visit_at。按已有编辑契约补null，仅改测试，不改后端/API。
3. 修正后4/4、44次通过；最终补齐“事后核对中失权清理旧数据”逻辑与测试后，再跑前端9/16/23测试、build及真实HTTP4/4/44请求、冒烟/完整性，均通过。失败轮和重复成功轮不计入最终44次独立套件请求数。

## 5. 执行命令、证据及服务清理

```powershell
# 项目根目录，使用已有VS工具；未重新初始化依赖
& "D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build backend/build --config Debug --parallel 4
& "D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir backend/build -C Debug -R '^worker_' --output-on-failure
& "D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir backend/build -C Debug --output-on-failure
# frontend目录
npm.cmd run test:auth
npm.cmd run test:orders
npm.cmd run test:admin
npm.cmd run build
```

前端HTTP套件需要独立服务与夹具；未配置时会拒绝运行而不是跳过后冒充通过。测试者可按下面步骤在**新临时路径**重现（不要对用户库执行--init）：

```powershell
# 项目根目录，先创建新目录backend/build/Testing/<唯一新目录>，确认里面尚无数据库
.\backend\build\Debug\campusfix_worker_http_fixture.exe --init <新临时库> <同目录临时credentials.json> .\backend\migrations .\backend\seeds\001_demo_data.sql
# 仅本次终端环境；先保存旧值，结束恢复；不输出凭据文件内容
$env:CAMPUSFIX_DB_PATH = '<新临时库的绝对路径>'
$env:CAMPUSFIX_PORT = '8848'
$env:CAMPUSFIX_ADMIN_TEST_BASE = 'http://127.0.0.1:5173'
$env:CAMPUSFIX_ADMIN_TEST_CREDENTIALS = '<临时credentials.json的绝对路径>'
# 分终端启动正式后端、frontend npm run dev -- --port 5173 --strictPort
node.exe --test frontend/tests/admin-http.test.js
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke.ps1
sqlite3.exe <新临时库> 'PRAGMA integrity_check; PRAGMA foreign_key_check;'
```

上述占位路径须替换，不是可直接粘贴的原样命令。Node套件使用Axios真实HTTP adapter，只将测试传输目标映射到本机Vite；响应完全来自Drogon/SQLite，不stub响应、不模拟登录成功。随机Session来自原测试夹具，只在临时文件与内存存在。不是浏览器，也不把这套测试说成输入密码登录或点击按钮。

最终实际输出摘要：

```text
ACTUAL review chain temporary order id=10 final_status=PENDING_DISPATCH version=6
PASS frontend modules made 44 real proxied HTTP requests (not browser E2E)
ADMIN_HTTP_EXIT=0
PASS backend #1 request_id=req_4c6e968d-1dcc-49fc-865f-ca3a60eaeae7
PASS backend #2 request_id=req_7124dcea-9ed9-4b8d-a6be-b5586b27d84c
PASS proxy #1 request_id=req_e45af315-99e7-40da-8391-cd6d888610fa
PASS proxy #2 request_id=req_2d70cb31-d6d7-4aa1-82e3-a94e6344caaf
PASS all W2-D5 smoke checks
SMOKE_EXIT=0
ok
INTEGRITY_EXIT=0
FINAL_LISTENERS=0
```

证据位置均被Git忽略：CTest完整输出`backend/build/Testing/Temporary/LastTest.log`，Worker当前回执`backend/build/Testing/w5d3-evidence.json`（沿用原测试输出名，现为本轮113请求，勿与此前107请求回执混用），最终服务日志在`backend/build/Testing/w5d4-live-71aa9598570d46f1b73c108fc7091d4a/`。临时库/凭据在服务停止后清理，不提交Git。

先确认8848/5173无占用，再启动隐藏测试服务；最终仅清理自行创建的后端PID31892及Vite PID35716。首次失败轮自行创建的7672/40356、14520/3832及前一成功轮41380/11468也均在各自finally中收尾，未终止用户既有进程。测试环境变量恢复，最终两端口监听0；随机端口CTest同样自行收尾。用户旧验收库没有迁移、重置或清理。

## 6. 完整文件范围（相对以下仓库根目录）

根目录：`G:/A_Study/CppProject/CampusFix/`。**修改12、新增13、删除0，共25文件**；本轮不自动暂存、commit、push。

修改：

- backend/src/controllers/WorkerController.cpp：受众化只读扩展序列化。
- backend/src/models/Worker.h：读取真实布尔与独立SkillOption模型。
- backend/src/repositories/WorkerRepository.cpp：真实列/有效技能选项查询。
- backend/tests/integration/WorkerHttpTest.ps1：扩展白名单、空列表/停用选项/本人隔离、容量单字段保存回归。
- backend/tests/integration/WorkerServiceTest.cpp：真实布尔与独立选项断言。
- frontend/package.json：新增两个管理测试命令，无依赖变更。
- frontend/src/api/client.js：仅增加安全字段级错误提示，不复制原始响应/请求配置。
- frontend/src/api/orders.js：真实managed列表与review封装。
- frontend/src/layouts/AppLayout.vue：更新已开放模块说明。
- frontend/src/main.js：新增组件显式注册。
- frontend/src/router/index.js：只替换三个管理占位页面。
- docs/progress-log.md：追加本轮真实执行及失败修复历史。

新增：

- frontend/src/api/workers.js：API-14/15/16认证调用。
- frontend/src/utils/adminOrders.js：动作提示、查询/审核白名单、未知请求核对逻辑。
- frontend/src/utils/workers.js：完整DTO校验、真实目录、回填/保存/核对状态机。
- frontend/src/views/AdminOrdersView.vue：管理列表与详情/审核入口。
- frontend/src/views/AdminReviewView.vue：明确审核入口。
- frontend/src/views/AdminWorkersView.vue：人员表格、真实筛选/负载与编辑入口。
- frontend/src/components/AdminOrderDetailDrawer.vue：管理详情与授权时间线。
- frontend/src/components/OrderReviewDrawer.vue：三动作审核、当前版本与二次确认。
- frontend/src/components/WorkerEditDrawer.vue：完整档案/技能编辑和真实回读。
- frontend/tests/admin.test.js：23项逻辑/适配器/静态回归。
- frontend/tests/admin-http.test.js：4项前端模块真实代理HTTP集成。
- docs/W5-D4-acceptance-report.md：本报告。
- docs/W5-D4-manual-acceptance.md：原浏览器验收指导、用户14/14通过补记与原细项历史清单。

普通git diff --stat只统计已跟踪修改，不含13个新文件。本轮对所有新增文件另用no-index检查，未用git add -N制造Diff；退出1表示存在新文件差异，不等于空白错误。暂存为空，无无关未跟踪文件。原两份W1-D3交接文件在本轮起始不在未跟踪列表中，本轮未创建、修改或加入它们。SQLite/WAL/SHM、build、node_modules/dist及临时证据继续被忽略，无本地配置或凭据入Git。

## 7. 尚未验收及残余风险

- **用户真实浏览器人工验收通过，14/14**：管理员审核、退回补充再提交、人员完整字段回填、只改容量保留其他字段、技能修改、权限隔离及窄屏均由用户确认。具体证据边界见第8节；不是Codex独立浏览器测试。
- API-16无版本字段，存在最后写入覆盖风险；API-03只给能力/范围摘要，不给每能力资源范围映射，前端不猜权限，最终后端403/404为准。
- 完整人员定位最多50页×100条，超过界限失败关闭，不增加新端点。停用原服务区域阻止保存，需人工核查而非默认迁移区域。
- 本轮build保留>500kB chunk警告；未额外拆包/引依赖。内存Token刷新需重登、离线退出不保证服务端撤销、完整登录限流尚未完成等风险不变。
- 物理COMMIT I/O故障、断电、磁盘满、高负载、跨机器首次部署未测；逻辑/触发器回滚不代表物理故障已验证。
- W5-D4此前自动化与用户人工验收均通过；本轮只做文档补记和Git核对，等待最终源码及Git完整性审查，未暂存/提交/推送，不进入W5-D5。

## 8. 用户真实浏览器人工验收补记

- 来源与主体：项目开发者本人明确反馈“真实浏览器14/14项全部通过”。管理员审核、退回补充再次提交、人员完整字段原样回填、仅改容量不改变其他字段、技能修改、权限隔离及窄屏检查均记录为**用户确认通过**。
- 清单数量说明：原人工文档A1～E3有27条细项，用户本次反馈口径为14/14。人工文档按现有主题建立M01～M14汇总映射并逐项记录通过，保留原27条实施阶段清单；不擅自声称获得27项逐条独立实测或将归档编号冒充用户原始编号。
- 证据分层：用户反馈支持本次人工结论；第4～5节保留此前真实HTTP/SQLite结果与自动化数字。Codex本轮没有独立操作浏览器、重跑自动化浏览器测试或读图，也没有执行新的业务请求。
- 未提供的证据：本次没有截图文件、Network响应、人工request_id、工单编号/ID、具体页面时间、实际库名或浏览器版本，不补造或从旧自动化临时样本推断。原HTTP样本ID10、版本链和request_id仅属于此前自动化测试，不充当用户人工样本。
- 历史与限制：保留22/22 CTest、62个Worker断言、113次Worker HTTP、审核149/补充74次HTTP、前端9/16/23、代理HTTP4/4及44次请求、构建/冒烟/完整性结果；原两次HTTP失败及测试修复历史不变。本轮未重跑这些检查。
- API-14显式只读DTO扩展第2节完整保留；API-16无乐观锁、并发最后写入覆盖、有界读取、大chunk、内存Token/离线退出/限流以及物理故障/高压/跨机器未测风险均未消除，不因人工通过写成“已修复”。
- 本轮实际只编辑本报告、manual-acceptance与progress-log三份文档，其他W5-D4实现保留原样，累计仍修改12/新增13/删除0共25文件；暂存为空，未git add、commit或push。**W5-D4用户真实浏览器人工验收通过，等待最终源码与Git审查；到此暂停。**
