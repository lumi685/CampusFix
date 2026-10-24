# W4-D5 正式验收报告

## 1. 结论与基线

- 日期：2026-10-23；HEAD与本地origin/main均为`d4d2d72`，工作区开始时干净。没有执行fetch，因此这里不声称重新联网核对了远端当前HEAD。
- W4-D4基线：`c1f3881 feat(work-orders): add catalogs and reporter workflow pages`与`d4d2d72 fix(frontend): include missing W4-D4 frontend files`，分别17和11个文件，遗漏前端文件已实际进入后一次提交。
- 最终结论（2026-10-24补齐G3～G6）：**W4-D5人工与自动化验收通过；A～G及七项附加测试均由用户确认通过，同一工单关联已由新增截图补齐。favicon404与Chrome Built-In AI提示为非阻塞问题，本轮不修复。**原失败、补图及自动化历史保留；等待提交批准，不进入W5。
- 仅验收报修提交前半段：创建→提交→本人列表→详情→时间线；不是维修/验收/关闭生命周期已完成。
- W4-D4用户后续反馈已补记：控件恢复、功能通过、Network无错误。保留最初失败、注册修复及自动化结果，不把用户概述当成本轮精确HTTP/SQLite或截图证据。

## 2. 可复现命令与环境

沿用Windows/MSVC、VS2026、C++17、Drogon1.9.13#2、SQLite3.53.4#2、libsodium1.0.22#1；CMake自动配置仅复用当前vcpkg缓存，没有新增依赖或修改baseline。

从仓库根目录执行；本机实际CMake/CTest路径来自backend/build/CMakeCache.txt，不保证其他机器路径一致：

```powershell
& 'D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build backend/build --config Debug --parallel 4
& 'D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir backend/build -C Debug --output-on-failure
```

单独复现本轮HTTP集成并打印细节：

```powershell
& 'D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir backend/build -C Debug -R '^report_acceptance_http$' -V
```

frontend目录实际执行：`npm.cmd run test:auth`、`npm.cmd run test:orders`、`npm.cmd run build`。W2回归在本轮自建前后端服务运行时实际执行`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke.ps1`。

### 实测结果

| 层级 | 结果 |
|---|---|
| MSVC Debug | 退出0，未见编译警告 |
| 全部CTest | 16/16通过，退出0，29.50秒 |
| 新report_acceptance_http | 91次生产HTTP请求、20次独立SQLite快照；约7.50秒 |
| 前端认证/状态测试 | 9/9通过，含新增切换角色清理身份/授权/Token逻辑测试 |
| 前端报修测试 | 7/7通过，含注册检查、UTC、同草稿重试及409恢复 |
| Vite生产构建 | 退出0；有chunk超过500kB提示，JS664.18kB/gzip220.64kB |
| W2冒烟 | 退出0；直连/代理各两次200JSON、不同request_id及首页200通过 |
| SQLite完整性 | 20次快照中integrity_check=ok、foreign_key_check违规0 |
| 浏览器 | 受控入口apps=[]/browsers=[]；没有Codex本轮实测或截图 |

## 3. 隔离环境与证据位置

- 新夹具使用原迁移/seed以及既有测试数据构造器，独立临时库内有reporter A/B/C、worker、dispatcher、sysadmin，两个校区及建筑/楼层/房间、三种有效根分类、历史工单及合法筛选为空场景。
- 只在临时库为六个具名虚构账号生成每轮随机密码，复用Argon2id设置和显式启用函数；通过真实login/me发会话，不用伪造前端登录。另有既有夹具的任务/历史派单仅用于权限测试，不冒充正式派单业务已实现。
- 新夹具拒绝覆盖已有数据库。故障触发器仅在测试库内建立/移除，没有改001迁移、seed、业务表或正式HTTP路由。
- 自动化数据库、临时凭据及服务运行日志在测试结束清理，既有人工/演示库未读取、重置或覆盖。
- 本次可审计的脱敏HTTP响应和SQL记录保存于 `backend/build/Testing/w4d5-evidence.json`（被Git忽略）；生产login响应中的token已删除，密码和Token经扫描不在证据或服务日志。重新运行会覆盖为新一轮证据，先置running，失败标failed，避免旧pass冒充本轮成功。
- 全量实测输出为 `backend/build/Testing/Temporary/LastTest.log`；同样被Git忽略。文件含虚构业务数据，不含真实个人资料；不自动把整个build目录提交。

## 4. 主链路逐步证据

以下为2026-10-23T14:49:45Z开始的一轮真实测试，所有ID由HTTP/SQLite取得，不是前端硬编码。完整逐请求request_id及响应在上述JSON。

| 步骤 | HTTP结果 | 核对内容 |
|---|---|---|
| 六个账号login/me | 均200 | 服务端真实身份；凭据不输出 |
| 根位置与逐级查询 | 均200 | 根1/2；选择1→3→7→11，parent_id与直接父一致，最终ROOM |
| 根分类及叶子查询 | 均200 | 有效根分类2可作为选择项，子数组为空 |
| reporter创建 | 201 | id9、DRAFT、version1、order_no=null；SQLite仅新增一行及一条创建日志 |
| reporter版本冲突 | 409 | ORDER_VERSION_CONFLICT；整库指纹不变 |
| reporter正式提交 | 200 | id9、PENDING_REVIEW、version2、WX202610230001 |
| reported分页/筛选 | 均200 | 第一/二页不重叠；total与SQL精确相等；状态/分类组合正确 |
| 详情/时间线 | 均200 | HTTP字段逐项对SQLite；两事件稳定顺序，无内部字段；GET整库指纹不变 |
| worker主链路 | 201→409→200→200 | 本人id10、独立编号WX202610230002；与assigned视图分离 |
| 退出及旧Token | 200→401 | 服务端撤销当前会话 |
| 停服务/重启/重新登录 | login/me/list/detail/timeline均200 | 原编号、version2、待审核及历史事件ID/时间不变 |

新测试91次请求状态统计：200×48、201×3、401×5、403×4、404×3、409×5、422×21、500×2。两次500为测试明确注入的预期故障，不是未解决的异常。

### 实际SQLite工单与日志

| 场景 | order_id | reporter_id | order_no | 最终状态/版本 | 创建UTC | 提交UTC |
|---|---:|---:|---|---|---|---|
| reporter | 9 | 3 | WX202610230001 | PENDING_REVIEW / 2 | 2026-10-23T14:49:48.966Z | 2026-10-23T14:49:49.117Z |
| worker本人 | 10 | 6 | WX202610230002 | PENDING_REVIEW / 2 | 2026-10-23T14:49:49.365Z | 2026-10-23T14:49:49.576Z |
| 同草稿故障重试 | 11 | 3 | WX202610230003 | PENDING_REVIEW / 2 | 2026-10-23T14:49:50.426Z | 2026-10-23T14:49:50.686Z |

上述三单category_id=2、location_id=11、虚构描述及电话、requested_urgency=NORMAL、expected_visit_at=2030-01-01T00:30:00.000Z，均与请求体/详情相等。20次快照包含原始SQL结果，非只检查成功提示。

| 工单 | 创建日志ID / request_id | 提交日志ID / request_id |
|---|---|---|
| 9 | 10 / req_95527375-88a4-42a6-ad42-29a0ed6b3c8a | 11 / req_dba1671b-699d-4727-bce8-5a9a0337fc9a |
| 10 | 12 / req_eedd807a-2ed8-4952-880b-f1c8bcdbf4ea | 13 / req_0bb6b6a3-70a2-456b-ac7b-71a85dfb445e |
| 11 | 14 / req_ac52bd34-d880-49e9-b2ea-77d1c300235f | 15 / req_e8ff4be0-f257-4d9e-8105-d1378f90a0a8 |

每单恰好ORDER_CREATED(NULL→DRAFT)、ORDER_SUBMITTED(DRAFT→PENDING_REVIEW)两条，actor_id为表中reporter_id；时间分别等于created_at/submitted_at，request_id与对应201/200请求一致。共3个合法新单、流水累计3，故障/重复提交无额外行或编号。

## 5. 权限、异常与失败重试

- 新集成HTTP：匿名列表/详情/时间线401；A访问B归属工单隐藏404；dispatcher/sysadmin创建403；worker当前派单可读但不具有原报修人提交资格，返回403，不是404。worker本人的reported与assigned查询分离。
- 管理范围：dispatcher仅AREA_SCOPE内；sysadmin按自己的BUSINESS_READ；将读取改为全局但INTERNAL_NOTE_READ仍只在另一校区时，目标时间线只有公开字段；撤销BUSINESS_READ后详情404、managed403。完整授权撤销/内部字段投影/祖先后代关系还有原有timeline/read/authorization测试回归，不冒充新HTTP脚本覆盖全部组合。
- 缺必填、无效/停用分类位置、错误紧急度、超长标题/描述/联系字段、伪造reporter_id、非法expected_version均拒绝；故障/无权请求批次整库指纹不变。合法长度边界和并发/补充提交由既有WorkOrderService/HTTP测试覆盖，不新增API-07。
- 创建日志故障500：工单与创建事件整体回滚。提交日志故障500：状态、版本、流水号、日志整体回滚；关闭注入后用相同id11/version1提交成功，只一个正式编号、两事件。重复submit409无额外写入。现有Service测试继续验证工单UPDATE故障回滚；未模拟真实磁盘满/断电或物理COMMIT故障。
- 前端同草稿ID/version保持与重试由现有reportSubmission逻辑单测通过；新HTTP脚本独立验证后端失败重试不重复。二者相互补充，**不是“已在真实浏览器复现页面失败/重试”**。
- 新增前端角色切换逻辑测试确认退出后user/Token清空、受保护路由回登录，下一worker登录取得新身份/无旧授权；这不是浏览器切换角色的截图证据。

## 6. 关键代码与安全审查

- 生产Controller路由没有扩大，仍仅health/auth/catalog/create/submit/list/detail/timeline；测试工具未链接进正式服务，无测试授权/故障HTTP入口。
- 创建/提交继续由服务端会话确定身份；Service用同一BEGIN IMMEDIATE完成编号、主体和追加日志，SQL绑定和条件版本更新保留。查询按角色/资源授权及显式DTO返回，不直接序列化数据库行。
- 位置/分类从真实API读，Vue请求无reporter_id/role/status/order_no；根分类可选；日期使用toISOString，不将无时区字符串强贴Z。内存Token策略及跨请求凭证保护未改。
- 追加日志的T-01/T-02与T-05补充规则未变，重启后事件保留。本轮没有实现审核/派单/维修/验收/评价/关闭，也没有把测试历史派单当作业务实现。
- 新测试工具只在未存在的临时库初始化，实际断言独立SQL，不把Service返回当数据库证据；快照投影不含密码/Token哈希。原演示库未动。
- 发现的是两处新测试预期错误，已修正：reporter03有种子历史，空列表改用合法CLOSED筛选；已可见但无提交权限的任务应403而非404。没有发现需修改生产业务代码的阻塞缺陷。

## 7. 浏览器与截图

2026-10-23自动化期间受控浏览器无连接，未继续原先URL安全校验受限的Windows自动化路径，未进行登录/点击/截图/Console实测。2026-10-24用户提供的截图目录已逐张审阅：这属于对用户人工证据的核验，不是Codex重新执行浏览器测试。

实际目录`backend/build/Testing/w4d5-screenshots/`现有A、B、C、D、E、F1、F2、G及最终G3～G6共12张PNG，完整索引见`W4-D5-manual-acceptance.md`第6节。原8张先前已审阅，本次实际打开最终G3/G4/G5/G6；未搬移、编辑、合成或暂存图片，build规则继续排除它们。可见区域未发现密码、Bearer、Authorization或真实私人资料，不能据此声称检查了隐藏网络内容。

可见证据支持：表单/级联/日期等控件已显示；worker填写页有URGENT应急提醒；Reporter的`WX202610240002`在C详情、D本人列表与E时间线一致，/orders/7、待审核/version2、创建与提交事件可见；F1为Demo Worker 1，F2本人列表显示`WX202610240001`。这些人工工单与第4节2026-10-23自动化临时工单是不同记录，不混用数据库证据。

限制：数据库文件名与人工各项结果仍为未填写；B为worker表单，不是Reporter表单。G仅显示若干GET200，详情请求名6而非C/E的7，无创建201或submit200；顶部有1条错误指示，内容未知。因此不能宣布同一工单完整Network链路、Console无错误、角色越权防护或所有附加人工场景通过。请用户确认数据库名、两个截图工单是否为指定验收单，填写7项附加测试，并补安全裁剪的POST状态及错误内容。未验证项目和既有自动化结果保留，本轮未重新构建/运行测试。

### 2026-10-24最终人工确认及G3实际检查（更新上段历史待确认事项）

- 用户明确指定数据库`backend/build/w4d4-manual.sqlite3`，Reporter工单WX202610240002、Worker本人单WX202610240001；本轮只确认文件存在，不读取或修改人工库。
- 用户确认A～G全部通过，并确认刷新重新登录、后端离线恢复、退出路由保护、非法表单、跨用户访问、同草稿重试、重复提交保护七项全部通过。这些记录为“用户人工确认”，不是Codex本轮浏览器/自动化实测。
- G3实图只包含Console的favicon.ico404和Chrome Built-In AI LanguageDetector/content_main.js信息，文件名虽然含create-submit，**内容没有POST创建201、submit200、工单id或对应order_no**。不能核验两次POST是否属于同一工单；原G只有GET200及请求名6，同样不能补足。
- Console图片与用户结论一致：favicon缺失为非阻塞UI资源问题，本轮不修复；浏览器内建AI提示无证据指向CampusFix业务异常，不改业务代码。可见区域不含凭据或私人信息。
- 自动化16/16、前端9/9与7/7、构建/冒烟及SQL证据保持原历史结果，本轮未重跑或改写。最终状态为“自动化与用户人工场景确认通过，写请求截图归档待补”；收到真正Network的POST状态/同ID关联证据后，再完成最终完整证据验收。

### 用户替换G3后的复核（以上Console旧图记录保留为历史）

- 本次重新打开同一路径，当前36294字节的图片确为Network，而非此前6070字节的Console图。可见work-orders201、submit200、详情名9为200及timeline查询200；来源orders.js:12/13/5。可见区域无密码/Token/Authorization或私人信息，图片未被Codex修改。
- 状态码部分已补齐；截图没有Method列、创建响应data.id、submit完整URL或响应体，尚不能独立确认创建与提交同一工单。当前详情名9也不同于原C/E的工单7，不能推断其编号为WX202610240002；亦不与第4节自动化库order9混用。
- 请仅补创建响应id及submit完整路径/响应id关联；如果这是新的人工验证单，确认其角色和工单编号即可。用户A～G/七项测试的通过确认保持有效，不要求重复整套验收；当前剩余为精确证据关联而非已知业务失败。

### 最终G3～G6补齐结论（上述缺口记录为历史，现已解决）

| 实图 | 实际核验事实 |
|---|---|
| G3-network-create-submit.png | work-orders201、submit200、详情10为200、timeline查询200 |
| G4-create-response.png | 创建响应data.id10、DRAFT、version1、order_no null |
| G6-order-detail-response.png.png | 实际为submit General：POST http://127.0.0.1:5173/api/work-orders/10/submit，200 OK |
| G5-submit-response.png | submit响应id10、order_no WX202610240005、PENDING_REVIEW、version2、submitted_at 2026-10-24T03:30:20.075Z |

创建响应ID、submit路径ID、提交响应ID一致；结合详情10/时间线总览及用户明确确认，同一工单关联证据成立。本次是后续补拍的新样本，不覆盖先前Reporter WX202610240002或Worker WX202610240001；也不混用第4节自动化临时库的同号ID。

本轮只审阅图片，没有另跑浏览器或SQL；右侧截断的request_id未臆造，G6折叠的请求头未展开。可见区域未发现密码、Bearer、Authorization或私人资料。G6名称带双.png且内容是submit，不改名，不把它写成实际详情JSON。

最终验收状态为**人工与自动化验收通过**：既有16/16 CTest、9/9认证、7/7报修、构建/冒烟及SQL结果保留且本轮未重跑；用户A～G和七项附加测试通过；新增样本的关联截图已补齐。favicon缺失404和浏览器Built-In AI提示按已确认结论记录为非阻塞，本轮不修复；W4-D4历史失败保留。

## 8. Git文件完整性与风险

| 完整路径 | 类型与目的 |
|---|---|
| `G:/A_Study/CppProject/CampusFix/backend/tests/integration/ReportAcceptanceFixture.cpp` | 新增：独立虚构库、只读SQL快照、仅测试故障/授权夹具 |
| `G:/A_Study/CppProject/CampusFix/backend/tests/integration/ReportAcceptanceHttpTest.ps1` | 新增：真实登录/目录/工单/SQLite/重启/回滚/脱敏证据与清理 |
| `G:/A_Study/CppProject/CampusFix/docs/W4-D5-manual-acceptance.md` | 新增：人工详细脚本、A～G截图索引与安全要求 |
| `G:/A_Study/CppProject/CampusFix/docs/W4-D5-acceptance-report.md` | 新增：本报告，区分自动化/用户确认/浏览器未验证 |
| `G:/A_Study/CppProject/CampusFix/backend/CMakeLists.txt` | 修改：仅测试目标和第16项CTest注册 |
| `G:/A_Study/CppProject/CampusFix/frontend/tests/auth.test.js` | 修改：新增角色切换状态回归 |
| `G:/A_Study/CppProject/CampusFix/docs/W4-D4-manual-acceptance.md` | 修改：补记用户后续通过反馈，保留失败历史 |
| `G:/A_Study/CppProject/CampusFix/docs/progress-log.md` | 修改：W4-D5真实进度与证据边界 |

最终已实际执行git status --short / git diff --check / git diff --stat：范围4新增、4修改、0删除，暂存区0；已跟踪与4个新文件空白检查均无错误，仅Git LF→CRLF提示。git diff --stat只统计4个已跟踪修改文件，不包含4个新文件。当前仓库未发现原两份W1-D3交接文件，不在Git跟踪或本轮变更中，未执行删除/恢复；没有其他起始未跟踪文件。

```text
 M backend/CMakeLists.txt
 M docs/W4-D4-manual-acceptance.md
 M docs/progress-log.md
 M frontend/tests/auth.test.js
?? backend/tests/integration/ReportAcceptanceFixture.cpp
?? backend/tests/integration/ReportAcceptanceHttpTest.ps1
?? docs/W4-D5-acceptance-report.md
?? docs/W4-D5-manual-acceptance.md
```

所有数据库/WAL/SHM、随机凭据、二进制/build、node_modules/dist、日志与本地证据JSON均不纳入Git；没有暂存/commit/push。测试仅停止自建进程，W2冒烟PID48184/42544已退出，8848/5173剩余监听0。

风险与待办：W4-D5同单截图缺口现已补齐。内存Token刷新需重新登录、离线退出不能保证服务端撤销属于既有设计边界，不因人工离线恢复测试通过而消失；完整限流、跨机器首次部署、压力/断电/磁盘满测试仍未完成。构建chunk警告不影响本轮成功，后续可在专门性能任务评估拆包，不为本次归档重构。

下一步仅核对本报告所列8文件并等待用户批准提交；不自动git add/commit/push，不进入W5。建议提交信息：`test(work-orders): add auditable report acceptance regression`。
