# W5-D2：原报修人编辑、补充与重新提交验收报告

日期：2026-10-24。执行主体：Codex（本机编译、SQLite、生产 Drogon HTTP、前端逻辑测试）。

结论：**既有自动化验收通过，全部既定浏览器人工验收已由用户确认通过；暂停等待最终 Git 与源码审查，不进入 W5-D3。** 人工补记与证据边界见第8节。

## 1. 基线、范围及技术审计

- HEAD：`b9dc0ac5703001f4c2349947332ed6fff4880706`，main，与本地 origin/main 对齐；起始工作区干净，暂存为空。本轮未 fetch，不把本地引用视为另行联网核验。
- 依据：AGENTS、W5-D2 开发计划、API-06～11/13、T-02/04/05、FR-003/005/TC-WF-02、D-011/012/016/017，以及既有架构、数据库、权限与 UI 设计。
- API-07 原来未实现；原 AuthGuard、Session、AuthorizationService::operation(Action::Edit)、表单校验、SQLite 事务、状态日志和 API-08 可复用。没有新建认证、权限、事务或状态机框架。
- API-10 不增加 reporter_id/can_edit 等白名单字段。前端使用真实 API-09 的 reported 视图确认资源归属；worker 维修任务不会因此获得本人报修编辑入口。后端仍以数据库 reporter_id 判定最终权限。
- 原 CatalogPicker 只支持 lazy 获取新选项，不能可靠回填任意已有节点。现使用真实目录接口有界顺序遍历构建路径，不硬编码标签/ID。
- 只开发 API-07、原工单补充 UI 和相关测试/文档。没有 API-12、人员 CRUD、管理审核页面、调度、上传、评价或后续业务；Schema、seed、冻结设计、依赖与认证语义不变。

## 2. 后端实现与不变量

`PUT /api/work-orders/{id}` 接收 API-06 的全部可编辑字段（expected_visit_at 显式为 UTC 字符串或 null）及 expected_version。路径和版本为正整数，未知字段/伪造身份/审核字段拒绝，非法 JSON/非对象返回400，输入错误422，认证401，权限403或保密型404，状态/版本409。

事务顺序：BEGIN IMMEDIATE → 再认证 Session → 读取角色、归属、状态、版本并调用既有 Edit Policy → 完整输入和有效引用校验 → 按 id/status/version 条件更新并检查行数 → 追加 ORDER_CONTENT_UPDATED → COMMIT。版本 int64 上限时安全返回409，不产生非整数版本。

编辑只允许 DRAFT/PENDING_SUPPLEMENT 原报修人（reporter 或 worker）；不改状态，仅 version+1/updated_at。SQL 更新列严格限于可编辑字段，不触碰原编号、首次提交、创建时间、审核紧急度、取消时间等。编辑不是新增 T 状态转换，不为内容修改增加新状态码。

审计 public_note 只记录真实变化字段名，例如 `Updated fields: description, requested_urgency, contact_phone, expected_visit_at`；未变化字段不列出，无变化时明确 `none`。不保存字段值、联系方式、描述全文或请求 JSON；metadata_json=NULL。from/to 为同一原状态，actor/request_id/UTC 对应实际请求，不修改删除旧日志。

共用引用校验还对齐了已有目录接口的有效层级：节点自身及其祖先必须启用，且祖先链可到根；UNION 避免循环递归。创建、编辑、重新提交均用这一规则，不能绕过目录选择直接提交“停用父节点下的启用子节点”。这是复用现有有效选项语义，不修改 Schema/seed 或新增业务字段。

原 API-08 未另建端点。补充后重新提交用原 ID、最新版本，保留原 order_no 和首次 submitted_at，不增长流水、不新建工单，追加原有 ORDER_SUBMITTED 事件。

## 3. 前端实现与安全恢复

- OrderDetailView：仅在服务端 reported 列表确认本人归属、且为可编辑状态时开放入口；归属查询失败默认关闭。管理角色不冒充原报修人。归属查找最多50页、每页100，逐页执行。
- 本轮退回原因：时间线先读取总量，再从最后页向前按页查最近合法 ORDER_RETURNED，跳过内容修改事件，遇到较新的 ORDER_SUBMITTED 则不误用上一轮原因；每次最多50页、顺序请求，失败显示重试而非旧原因。
- CatalogPicker：原选项从真实 API-04/05 解析树与标签，保留 checkStrictly 和根分类可选；预选路径遍历有界、顺序，不并发全量抓取。停用或目录过大时明确失败，允许重新选择有效节点，不生成假选项。
- OrderEditForm 复用 reportPayload 校验和已有注册控件，无新依赖；UTC 字符串转换为 Date 本地显示，保存再转 ISO UTC；URGENT 应急提示保留。
- supplementFlow 只有 PUT/detail/submit，没有 create：保存成功使用真实新版本；保存并提交严格先 PUT 后 submit；重复点击忽略；保存结果不确定或409时阻止重放/覆盖，用户确认刷新最新数据后再继续。
- 保存成功而 submit 网络/5xx失败，保留原 ID/版本，GET 核对原工单；若已待审核则识别成功，仍为同一待补充版本时仅重试 submit。版本已变化时要求刷新，不盲目提交别人的修改。整页刷新重登后可直接提交当前已保存内容，不强迫再次 PUT。
- 保存后刷新详情/时间线；只显示公开字段。ORDER_CONTENT_UPDATED 有中文文案。未开放上传、取消、维修、验收、评价假按钮。新建报修离页提示仅做一处文字更新，以免错误声称仍不支持原草稿编辑。
- 所有 Element Plus 模板控件经过既有静态注册回归；静态检查不等于浏览器渲染通过。

## 4. 实际执行命令与最终结果

项目根目录（前端命令在 frontend 下）：

```powershell
& "D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build backend/build --config Debug --parallel 4
& "D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir backend/build -C Debug --output-on-failure
Push-Location frontend
npm.cmd run test:auth
npm.cmd run test:orders
npm.cmd run build
Pop-Location
# 本轮先启动自有后端/Vite进程，再执行该已有脚本：
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke.ps1
git diff --check
```

| 类别 | 最终实测 | 退出码 |
| --- | --- | --- |
| CMake/MSVC Debug | 配置复用已有依赖，编译成功，无编译链接警告 | 0 |
| 全量 CTest | **20/20，52.04秒**，含原W3/W4及W5-D1全部回归 | 0 |
| 新 edit Service/SQLite | **60个断言**，真实临时库和双连接竞争 | 0 |
| 新 edit 正式 HTTP | **74次请求**，生产可执行文件的正式路由 | 0 |
| 新纯 Policy 测试 | 0，复用并回归既有 Policy | — |
| 前端认证 | **9/9** | 0 |
| 前端报修/补充逻辑 | **16/16**（原7+新9），不是浏览器E2E | 0 |
| 前端 build | 成功；JS674.60kB/gzip223.17kB，已有chunk>500kB警告仍存在 | 0 |
| W2 冒烟 | 直连/代理各两次200JSON、请求ID不同、首页200HTML | 0 |
| SQLite | Service/HTTP均 integrity_check=ok、foreign_key_check无违规 | 0 |
| git diff --check | 无空白错误；仅Git LF→CRLF提示 | 0 |

HTTP分布：200×21、201×3、400×3、401×4、403×2、404×4、409×4、422×29、500×4。四个500为两次提交日志失败、编辑主体/日志故障的预期注入。健康探测不计入74次业务请求。

测试覆盖：reporter/worker本人编辑草稿或待补充；他人、当前维修负责人、管理角色被拒绝；无效/过期/撤销会话；所有非可编辑状态；旧版本；字符串最大合法/越界、非法UTF-8、控制符/NUL；无效/停用目录及祖先；日期/枚举/JSON/额外字段。所有预期失败逐次比对完整业务行与日志摘要，不只看版本和数量。

并发采用两个真实 SQLite 连接同步竞争同一版本，只有一个成功、一个409。故障触发器分别使主体更新或内容日志失败，整体回滚；PUT已保存后人为使submit日志失败，已保存版本/内容仍存在，解除注入后只提交同一ID。

首次全量曾失败：新Service测试的读取摘要游标生命周期过长，使另一个连接提交时遇到 database is locked；已只修测试查询作用域，不放宽生产事务/超时。随后全量通过，再补有效祖先与多轮测试后执行得到上述最终20/20结果，历史失败不隐去。

## 5. 真实 HTTP 与数据库证据

以下是本次临时库的首轮快照，不是原人工库，即使编号相同也不能混用。密码未使用/输出；会话来自隔离测试夹具的有效持久化 Session，未把夹具伪装成完整浏览器登录。

Reporter：ID9，编号WX202610240001，首次提交 `2026-10-24T05:19:32.433Z`。创建201/version1→提交200/version2→RETURN200/version3，公开原因为 `Fictional public return reason`。

API-07 实际请求：`PUT /api/work-orders/9`

```json
{"expected_version":3,"category_id":1,"location_id":11,"title":"Fictional edit HTTP","description":"PRIVATE_CHANGED_DESCRIPTION","requested_urgency":"HIGH","contact_name":"Fictional contact","contact_phone":"00012345678","expected_visit_at":"2030-01-01T00:30:00.000Z"}
```

实际200响应：

```json
{"code":0,"message":"success","data":{"id":9,"status":"PENDING_SUPPLEMENT","updated_at":"2026-10-24T05:19:32.593Z","version":4},"request_id":"req_1197fa26-0d32-4ba0-81bf-4b63997e4d6b"}
```

SQLite中修改描述/电话/申报紧急度/期望时间正确，编号、首次提交、创建时间、approved_urgency、cancelled_at、reporter_id均未变化。内容事件actor_id=3，from/to均PENDING_SUPPLEMENT，created_at同updated_at，request_id同响应；public_note只含上述四个变化字段名，metadata_json=NULL，没有示例描述或电话值。

第一次 `POST /api/work-orders/9/submit`（expected_version4）故意注入失败返回500，整体提交事务回滚，原版本4及补充内容仍持久化；解除注入后只重试submit成功：

```json
{"id":9,"status":"PENDING_REVIEW","version":5,"order_no":"WX202610240001","submitted_at":"2026-10-24T05:19:32.433Z"}
```

首轮日志顺序为 ORDER_CREATED、ORDER_SUBMITTED、ORDER_RETURNED、ORDER_CONTENT_UPDATED、ORDER_SUBMITTED；原三事件逐字段不变，最后事件request_id=`req_bc3ab5ed-925f-4a25-91ba-954fe28edeb6`，UTC `2026-10-24T05:19:33.014Z`。流水仍1、订单总数仍9（都与退回前相同）。随后额外执行第二轮RETURN→PUT→submit到版本8，仍同ID/编号/首次提交时间，真实分页时间线共8条。

Worker本人：ID10、WX202610240002、actor_id6，首次提交 `2026-10-24T05:19:33.662Z`。编辑200/version4/request_id=`req_4161b706-9a4a-40c2-8659-4ef16d47a883`；故障提交回滚后同ID重试至version5，最后事件request_id=`req_ad882bea-83d0-4bdd-a60f-c4b7b333ccdb`、UTC `2026-10-24T05:19:34.484Z`。五事件同序，编号、首次提交与流水均不变。

完整首轮SQL快照和HTTP回执在被忽略的 `backend/build/Testing/w5d2-evidence.json`；本次CTest明细在 `backend/build/Testing/Temporary/LastTest.log`。内容为虚构信息，不提交Token/测试库/构建产物。服务日志扫描未出现原始Token、修改后的描述或电话。

## 6. 文件清单

新增10文件：

| 完整路径 | 用途 |
| --- | --- |
| G:/A_Study/CppProject/CampusFix/backend/src/services/WorkOrderEditService.cpp | 编辑事务、归属授权、字段名审计 |
| G:/A_Study/CppProject/CampusFix/backend/tests/integration/WorkOrderEditTest.cpp | 60个真实SQLite/Service断言 |
| G:/A_Study/CppProject/CampusFix/backend/tests/integration/EditHttpFixture.cpp | 隔离库、受控故障/SQL快照，不进入生产路由 |
| G:/A_Study/CppProject/CampusFix/backend/tests/integration/WorkOrderEditHttpTest.ps1 | 74次正式HTTP及同单全链路 |
| G:/A_Study/CppProject/CampusFix/frontend/src/components/OrderEditForm.vue | 原工单补充/保存/提交交互 |
| G:/A_Study/CppProject/CampusFix/frontend/src/utils/orderSupplement.js | 归属、最新原因、保存提交恢复逻辑 |
| G:/A_Study/CppProject/CampusFix/frontend/src/utils/catalogPath.js | 真实目录预选路径检索 |
| G:/A_Study/CppProject/CampusFix/frontend/tests/supplement.test.js | 9个前端补充逻辑测试 |
| G:/A_Study/CppProject/CampusFix/docs/W5-D2-acceptance-report.md | 本报告 |
| G:/A_Study/CppProject/CampusFix/docs/W5-D2-manual-acceptance.md | 用户人工通过记录、证据边界及原准备说明 |

修改13文件：

| 完整路径 | 原因 |
| --- | --- |
| G:/A_Study/CppProject/CampusFix/backend/CMakeLists.txt | 链接编辑Service，注册两项新CTest |
| G:/A_Study/CppProject/CampusFix/backend/src/controllers/WorkOrderController.cpp | 复用创建解析增加完整PUT/最小响应 |
| G:/A_Study/CppProject/CampusFix/backend/src/models/WorkOrder.h | 最小OrderEditResult |
| G:/A_Study/CppProject/CampusFix/backend/src/repositories/WorkOrderRepository.h | 编辑方法声明 |
| G:/A_Study/CppProject/CampusFix/backend/src/repositories/WorkOrderRepository.cpp | 参数化条件编辑、有效祖先校验 |
| G:/A_Study/CppProject/CampusFix/backend/src/services/WorkOrderService.h | 编辑方法声明 |
| G:/A_Study/CppProject/CampusFix/frontend/src/api/orders.js | 认证PUT封装 |
| G:/A_Study/CppProject/CampusFix/frontend/src/components/CatalogPicker.vue | 原选项真实文字路径回填 |
| G:/A_Study/CppProject/CampusFix/frontend/src/utils/orders.js | 内容更新事件中文文案 |
| G:/A_Study/CppProject/CampusFix/frontend/src/views/OrderDetailView.vue | 待补充原因、归属确认、编辑组件 |
| G:/A_Study/CppProject/CampusFix/frontend/src/views/ReportNewView.vue | 仅修正离页提示，告知原草稿现在可编辑 |
| G:/A_Study/CppProject/CampusFix/frontend/tests/orders.test.js | 将补充测试纳入现有test:orders |
| G:/A_Study/CppProject/CampusFix/docs/progress-log.md | 追加本轮真实记录 |

删除0。普通git diff --stat不含未跟踪文件，验收不能遗漏上述10个新增文件。

## 7. Git、安全与待验收事项

- `git status --short` 的目标范围为上述13修改/10新增，无无关文件；本轮没有git add（含intent-to-add）、commit或push，暂存为空。原两份W1-D3交接文件仍未创建或修改。
- `git diff --check`退出0；新增文件另做no-index空白检查，不用暂存制造完整Diff。冻结文档、Schema、seed和依赖无差异，数据库/WAL、node_modules/dist和运行时证据继续被忽略。
- W2冒烟只停止自建PID31548/34680，8848/5173最终监听数0；随机端口HTTP进程也由finally停止，临时库/凭据清理。未修改、迁移或删除旧人工库。
- 实施轮历史：Codex浏览器操作/截图均为0，当时无受控浏览器工具，声明的浏览器技能文件不可读，人工体验尚待确认；此历史不改写为Codex自动化E2E。此后用户已完成全部既定人工项目并确认通过，补记见第8节。
- 有界检索为当前不扩张DTO的折中：本人记录超过5000条、目录过大或最新事件距离超过50页时会失败关闭/提示重试；没有为此增加P0端点。更大规模索引/契约优化不在本轮。
- 未测试物理断电、磁盘满、COMMIT I/O错误、高压、跨机器部署；trigger回滚不代表这些故障已验证。既有内存Token刷新需重登、离线退出未必撤销服务端Session、完整限流未完成等风险不变。
- 没有需要修改冻结规则的未决冲突；自动化与人工结果分别归档，当前均已通过，最终Git与源码审查仍待用户执行。到此暂停，不进入W5-D3。

## 8. 2026-10-24 用户浏览器人工验收补记

- **用户实际操作反馈**：执行主体为项目开发者本人；用户明确全部既定项目完成且未发现功能错误，包含正常补充流程、字段回填、权限、409冲突和失败恢复。人工清单A～M统一标为“用户确认通过”，不说成Codex重新操作浏览器或自动化E2E通过。
- **用户所述截图直接可见内容**：同一工单公开时间线依次为创建草稿2026-10-24 14:14:41、首次提交14:14:41、管理员退回14:18:54、补充修改报修信息14:21:52、再次提交审核14:25:23。退回原因是“请补充插座故障的具体情况和检测结果”，修改摘要为`Updated fields: description`，所述公开时间线未出现内部审核备注标记。
- **证据边界**：上述时间为页面显示，不补写未知UTC、时区或毫秒；本轮未附可独立读取的截图文件路径，Codex依据用户明确提供的截图内容补记，不宣称重新读图。没有人工Network响应截图、request_id、工单ID/编号、浏览器版本或实际数据库名，不编造这些值；不把第5节自动化临时样本ID9/10或其请求ID套用到人工工单。
- **既有自动化结果**：第4、5节的20/20 CTest、60个Service断言、74次正式HTTP、前端9/9与16/16、构建/冒烟/完整性结果原样保留。本轮仅做文档补记和Git检查，未重跑编译、CTest、前端或HTTP，不把旧结果说成这次重新执行。
- **历史与优化项**：首次测试夹具游标未释放及修复记录保留；英文审计摘要记为非阻塞UI文案优化，本阶段不要求修复，也不改变稳定事件码或摘要内容。
- **本轮改动**：仅编辑本报告、人工验收文档及progress-log.md三份文件；保留工作区原有W5-D2实现，不改冻结设计、Schema、seed、数据库或业务代码。无本轮新增/删除文件，累计范围仍13修改/10新增/0删除，不自动暂存、commit、push。
- **最终状态**：W5-D2自动化及用户浏览器人工验收通过，等待最终Git与源码审查；第7节的未验证风险继续保留，不进入W5-D3。
