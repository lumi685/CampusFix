# CampusFix W7-D5 一次性评价与关闭验收报告

实施日期：2026-11-02。实现与自动化验证完成，等待独立源码审查；尚未进行W7总体浏览器人工验收，未合并main。

- 起始分支 `feature/w7-workflow`，HEAD及fetch后远端一致：`0dad330913aaf75da8a17ccbe89de378b0ba974c`，工作区/暂存区干净。
- 用户正式批准D-022/API-35，编号检查无占用后先同步最小查询契约；不扩大原详情/时间线/评价权限。
- W7-D1～D5真实浏览器统一延期至D5独立源码审查通过并提交后，本轮不宣称人工通过。

## 1. 实现及安全边界

- API-25：复用AuthGuard及Action::Evaluate/D-017。Controller严格校验真实JSON整数score=1～5、expected_version、可选1000 Unicode评论，拒绝bool/小数/错误类型、未知/重复键、无效UTF-8和非法控制字符。BEGIN IMMEDIATE内重新验证Session、ACTIVE用户、reporter/worker原报修人、COMPLETED状态、版本及未评价；真实reporter_id写入evaluations，原UNIQUE(order_id)最终保护一次性。追加真实actor/UTC/request_id的EVALUATION_CREATED同状态日志；无评论/评分原始metadata；提交前重新鉴权并核对工单整行、派单/调度历史及评价。**不改变工单status/version/updated_at、派单或负载，不写评分缓存**。
- API-26：使用原Action::Read，不把评价写入资格误用为读取限制；允许原报修人、授权历史维修员及合法管理读取者查看既有评价，包括CLOSED。API专用query_only连接、一致只读事务、参数绑定、明确DTO；无评价返回null，不返回reporter_id/账号/联系字段/内部数据。
- API-27：复用Action::Close/T-23；dispatcher按AREA_SCOPE，sysadmin按CLOSE自身范围，BUSINESS_READ等能力不能代替。BEGIN IMMEDIATE内重新认证/授权、状态和版本；检查有效completed_at、匹配ORDER_VERIFIED的COMPLETED结束派单历史、接单≤最终结果≤结束时间及无有效派单，防int64版本溢出。条件更新CLOSED/version+1/closed_at/updated_at，追加ORDER_CLOSED公开说明；复查认证、工单保留字段、派单/调度/评价不变量后提交。可不评价直接关闭；异常有效派单拒绝，不静默结束；重复关闭/终态写入冲突且不重复日志。
- API-35/D-022：编号无占用后按用户批准先补契约。仅有效AREA_SCOPE/CLOSE范围内COMPLETED、合法UTC completed_at、无有效派单的真实记录；递归UNION+IN防重叠重复，NULL范围全局，能力参数绑定；专用query_only连接、同一快照认证/计数/分页，updated_at DESC,id DESC。复用领域UTC校验器的连接局部SQLite函数过滤非法日期，不改变Schema。返回项仅id/order_no/status/version/updated_at；**不扩大API-10/11/26**。队列只是发现路径，关闭时必须重新校验更强的完成证据及当前授权。
- 正常全链路前置均通过真实HTTP业务接口；SQL修改仅用于明确的异常/溢出/撤销及故障测试夹具，不冒充正常业务操作。不改Schema/seed、状态机、原AuthorizationPolicy/Service、W6算法权重、AI、依赖或API-01～34的既有契约。

## 2. 前端

- OrderEvaluationPanel、evaluationFlow与严格API适配器：ActionRead展示与原报修人写资格分离。COMPLETED未评价时通过服务端reported分页确认本人（reporter/worker），前后详情版本栅栏；管理/维修身份不能替代。已有评价及CLOSED只读展示，公开时间线新增冻结EVALUATION_CREATED中文映射。
- OrderClosePanel接入原管理详情，可读且有CLOSE提示的上下文才开放；成功刷新真实管理列表、详情及授权时间线，不自行拼接CLOSED。没有评价也可关闭。
- `/admin/close`独立队列与菜单/路由守卫不依赖BUSINESS_READ，只调用API-35/API-27，**不调用详情、时间线或评价查询**。选择实际队列ID/version，二次确认可选公开说明；关闭成功重新GET队列。
- 评价和关闭都锁定确认快照并防双击；422留输入、409清旧选择/版本、401/403/404清资料；网络/5xx/异常成功回执保持unknown_result，只读核实、不自动重发。查到评价或队列消失都不是原POST成功证明。身份/授权摘要/路由/卸载generation丢弃迟到结果；原管理列表在相同能力但不同账号/范围切换时也清旧抽屉和列表。
- 复用全局ElSelect/ElOption/ElInput/Table/Pagination，ElDialog局部导入。实际SFC编译/setup测试不代表组件已在浏览器目视验收。
- 保持内存Token，不新增本地持久化、新依赖或未实现业务按钮。

## 3. 实际验证

| 检查 | 本轮最终实际结果 |
|---|---|
| MSVC Debug完整构建 | 退出0；既有CMake配置自动重生成并复用vcpkg缓存；未安装/升级依赖，最终未观察到新增编译/链接警告 |
| evaluation_service | 7场景109断言，退出0 |
| evaluation_concurrency | 6场景18断言，退出0；独立SQLite连接实际争锁及强制两种取得锁顺序 |
| closable_query_service | 5场景86断言，退出0；包括WAL下另一连接真实关闭与COUNT/items一致快照 |
| evaluation_http | 13场景、418次脚本记录请求、364次独立16业务表全列/全行指纹采集，退出0；其中API-35 GET 26次，不累计其他重跑 |
| 全量CTest | **52/52，失败0，退出0，285.74秒**；从起始HEAD展开foreach注册名核对原48项全部保留，新加上述4项 |
| 前端全量单测 | **309/309，退出0**，保留原270项，新增39项；含真实SFC setup/模板编译与模拟生命周期，不是浏览器E2E |
| Vite生产构建 | 退出0；JS842.68kB/gzip268.22kB，CSS371.93kB；保留既有>500kB chunk警告，不擅自分包 |
| W2 smoke.ps1 | 真实Drogon及Vite随机端口，直连/代理各两次200 JSON、request_id不同、首页200；全部通过，退出0 |
| SQLite | 临时库integrity_check=ok；foreign_key_check为空；失败及只读操作全表指纹不变 |
| 浏览器/桌面/390px/Network/Console | **未验证**；按用户批准统一延期，见W7-overall-acceptance-checklist.md |

418计数是脚本记录的业务/权限HTTP请求，不含启动探活与独立W2脚本请求；364为指纹采集次数，不是364个不同数据库状态。所有测试使用独立临时库、随机会话、虚构数据和自有回环进程；日志检查无测试Token，结束检查自有端口释放并清理临时库/会话，不操作用户演示数据库。

### 权限、并发和故障证据

- D-017：worker02本人报修由worker01维修，worker02可评价；worker01即使参与维修也不能代评价；其他报修人隐藏404，管理人员不能代评价。授权历史维修员/管理者可以读取评价。
- 分值1/5、0/6/小数/bool、评论缺失/空/1000Unicode/超长/控制/无效UTF-8、重复JSON/错误字段/版本、CLOSED拒绝均真实验证；原SQLite唯一约束直接重复INSERT实际失败。
- close/close、evaluate/evaluate、evaluate/close分别在第三连接持锁时让两个请求真实到达BEGIN后释放；只有一次关闭/评价。另Service测试让第一事务真实持锁后第二连接竞争，分别证明：**评价先提交201，保持原版本，关闭随后200；关闭先提交200，评价随后409**。没有用业务mutex替代SQLite事务。HTTP另使用dispatcher与CLOSE sysadmin两名不同管理员竞争关闭，仅一个200。
- 管理范围、CLOSE-only、BUSINESS_READ/DISPATCH等不能替代CLOSE、全局与祖先/重叠授权、撤销/停用、非法角色、分页/末页/排序/无效日期/有效派单异常均覆盖。CLOSE-only实际代理关闭成功但详情/时间线/评价仍404；静态closable路由200与真实数字ID详情路由均实测。
- native覆盖评价/工单/日志各写入阶段ABORT与IGNORE零行故障，以及事务末尾账号停用；HTTP覆盖评价INSERT/日志及关闭UPDATE/日志4个SQL失败节点，全部回滚16表。实际SQLITE_BUSY503不留下部分写入。未冒称验证物理断电、磁盘损坏或生产压力。
- 实际服务端已提交后由测试适配器丢弃响应，评价及CLOSE-only流程仍保持未知结果，仅GET核实且第二次POST被阻止，不把读取结果视为原请求回执。

## 4. 真实HTTP闭环样本

以下是最终运行生成的临时虚构样本，不是用户真实工单；请求在真实Vite→Drogon链路中执行，独立SQLite连接检查，不是浏览器操作。

| 链路 | 工单ID | 最终状态/版本 | 评价/历史 |
|---|---:|---|---|
| A 创建→提交→审核→推荐→派单→接单→到场→进度→完成→通过→评价→关闭 | 5 | CLOSED/8 | COMPLETED/7评价后仍7；一条评价；同派单结束COMPLETED |
| B 无评价直接关闭，再评价失败 | 6 | CLOSED/8 | 评价0条；失败全表不变 |
| C 结果0→返工→结果1→返工→结果2→通过→评价→关闭 | 7 | CLOSED/12 | rework_count=2，三份完整结果、两次原因及最终提交时间保留 |
| D 转派申请→真实API-34队列→审批→重新推荐/派给worker02→接单维修→通过→评价→关闭 | 8 | CLOSED/10 | 两份assignment/dispatch_records，旧TRANSFERRED、新COMPLETED，申请APPROVED |
| E worker02本人报修、worker01维修、本人评价关闭 | 9 | CLOSED/8 | 维修身份不能代评价 |

通过验收前后独立比较实际worker_loads，恰好释放该有效派单1份容量；返工不释放，派单整行及旧快照保留。评价和关闭均未额外改变人员负载/派单/结果；所有最终有效派单数为0。

API-25实测：`POST /api/work-orders/5/evaluations`，请求`{"expected_version":7,"score":5,"comment":"Fictional evaluation"}`，HTTP201/code0。返回白名单评价ID1、order_id5、score5、comment同请求、created_at=`2026-11-02T02:35:18.235Z`，request_id=`req_a51073b2-275d-4987-8cbf-0fbbcdd67f82`；工单仍COMPLETED/version7。

API-26实测：`GET /api/work-orders/5/evaluations`返回200、统一JSON及非空request_id；evaluation白名单为id/score/comment/created_at（不含order_id/reporter_id等）。无评价工单返回evaluation:null；CLOSED后可授权读取，其他报修人和无BUSINESS_READ的CLOSE-only用户仍404。成功/失败GET通过独立指纹证明不写业务表。

API-27实测：`POST /api/work-orders/5/close`，请求`{"expected_version":7,"note":"Fictional archive"}`，HTTP200：

```json
{"code":0,"message":"success","data":{"order_id":5,"status":"CLOSED","version":8,"closed_at":"2026-11-02T02:35:18.504Z"},"request_id":"req_4cb1622e-57f6-4ad5-a268-2a56f1114dfa"}
```

CLOSE-only样本ID15从真实API-35取得version7，经同一前端适配器与真实代理POST后返回CLOSED/version8、closed_at=`2026-11-02T02:35:35.646Z`；队列重新读取移除该项，不开放完整详情。该结果是HTTP集成证据，不是用户浏览器确认。

公开时间线实测包含ORDER_CREATED、ORDER_SUBMITTED、ORDER_REVIEWED、ORDER_ASSIGNED、ORDER_ACCEPTED、WORKER_ARRIVED、WORKER_PROGRESS_NOTE、ORDER_COMPLETED、ORDER_VERIFIED、EVALUATION_CREATED、ORDER_CLOSED；评价日志无评论原始metadata，评分/评论从API-26读。

## 5. 命令与本地证据

项目根目录使用既有工具链：

```powershell
& 'D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build backend/build --config Debug --parallel 4
& 'D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir backend/build -C Debug --output-on-failure
```

本机HTTP_PROXY/HTTPS_PROXY指向本地7890且未配置NO_PROXY，曾导致Node Axios回环请求502；完整CTest执行进程临时补入`127.0.0.1,localhost`并在finally恢复原NO_PROXY，没有修改系统代理或用户持久环境。新增HTTP脚本的Node测试适配器明确proxy:false直连自有回环Vite，不改变浏览器客户端。

前端目录：

```powershell
node --test tests/auth.test.js tests/orders.test.js tests/admin.test.js tests/admin-security.test.js tests/dispatch.test.js tests/dispatch-security.test.js tests/report-assistant.test.js tests/report-draft.test.js tests/report-draft-handoff.test.js tests/report-draft-integration.test.js tests/worker-tasks.test.js tests/workflow.test.js tests/completion.test.js tests/verification.test.js tests/evaluation.test.js
npm.cmd run build
node tests/run-evaluation-http.mjs
```

最后一条在独立临时库中注册正式handlers并启动Vite、执行原scripts/smoke.ps1、清理。不是要求用户手工设置测试账号密码，也不复用用户演示库。

被忽略的证据：`backend/build/Testing/w7d5-http-evidence.json`、`backend/build/Testing/w7d5-ctest-final.log`及CTest Temporary日志。首轮全量失败日志`w7d5-ctest.log`保留；重跑不累计计数。

## 6. 首次失败与修复历史

1. 沙箱MSBuild FileTracker MSB4018/E_ACCESSDENIED；申请并使用既有本机MSVC构建，不切换工具链或升级依赖。
2. 新C++测试缺WorkOrderValidation头文件及混合类型auto声明（C3861/C3538），最小修复后通过；未删除测试。
3. 首次前端宽泛`tests/*.test.js`误包含需要隔离服务环境的独立HTTP入口，产生未配置/代理失败；后按原单测清单执行全部270+39，HTTP按CTest正式夹具独立执行。另新组件注入需补到旧SFC测试夹具，旧断言全部保留；原安全静态断言禁止JSON.stringify，改为授权摘要join而非削弱断言。
4. 新完整HTTP脚本将API-34的transfer_request_id误写id，修正新测试字段引用，不改既有API。
5. Node Axios回环502定位为本机代理；修复仅限测试传输/进程NO_PROXY，实际GET及CLOSE-only流程随后通过。
6. 首轮全量51/52、267.34秒：新终态编辑负例缺完整API-07字段，先得422而非状态409。补齐合法完整请求后保留终态拒绝/全表不变断言，最终52/52、285.74秒；不修改业务规则迁就测试。
7. 编码检查工具首轮Node -e受PowerShell参数引号影响出现SyntaxError，未执行检查；改用PowerShell/.NET严格UTF-8检查，以最终核验结果为准。

## 7. 完整文件范围

根目录：`G:\A_Study\CppProject\CampusFix`。以下路径均相对于该根；修改15、新增17、删除0，共32项，全部未暂存。

修改：

```text
backend/CMakeLists.txt
backend/src/controllers/WorkflowController.cpp
backend/tests/integration/WorkflowHttpFixture.cpp
docs/api-contract.md
docs/decisions.md
docs/progress-log.md
docs/requirements-trace.md
docs/ui-wireframes.md
frontend/src/components/AdminOrderDetailDrawer.vue
frontend/src/router/index.js
frontend/src/router/navigation.js
frontend/src/utils/orders.js
frontend/src/views/AdminOrdersView.vue
frontend/src/views/OrderDetailView.vue
frontend/tests/verification.test.js
```

新增：

```text
backend/src/repositories/EvaluationRepository.cpp
backend/src/repositories/EvaluationRepository.h
backend/src/services/EvaluationService.cpp
backend/src/services/EvaluationService.h
backend/tests/integration/ClosableQueryTest.cpp
backend/tests/integration/EvaluationTest.cpp
backend/tests/integration/EvaluationTestData.h
docs/W7-D5-acceptance-report.md
docs/W7-overall-acceptance-checklist.md
frontend/src/api/evaluations.js
frontend/src/components/OrderClosePanel.vue
frontend/src/components/OrderEvaluationPanel.vue
frontend/src/utils/closeQueue.js
frontend/src/utils/evaluation.js
frontend/src/views/AdminCloseView.vue
frontend/tests/evaluation.test.js
frontend/tests/run-evaluation-http.mjs
```

常规`git diff`/`--stat`只包含15个已跟踪修改，不包含17个新文件；源码审查必须同时读取新增项，不用git add -N制造范围。旧W1-D3两交接文件未修改，本轮无相关未跟踪项；无无关文件、SQLite/WAL/缓存/证据纳入暂存。最终空白、编码、Git统计见结项核验。

## 8. 风险、未验证与最终W7验收条件

- 本轮没有执行真实浏览器、桌面/390px、Network/Console或用户人工登录；不能声称这些通过。保留D1～D4历史待人工记录，新增总体清单18组均未验证。独立源码审查及提交后由用户统一执行并提供实际结果，未批准不合并main。
- JS不安全整数ID/version失败关闭；本人归属扫描沿用最多50页/5000条，超限或漂移不开放评价，需之后按真实规模评估，不擅自增加所有权接口。
- SQLite单写者、busy_timeout与前端timeout均约5秒，客户端可能先超时；未知结果不自动重发。没有新增幂等键或跨刷新回执查询，队列消失/已有评价不证明原POST成功。
- 内存Token刷新重登、离线退出不能保证服务器Session撤销、完整登录限流、跨机器部署、生产压力及进程崩溃/断电/磁盘损坏仍为未验证/后续风险。保留大chunk警告、原API-16无乐观锁等已有非本轮问题。
- W8附件、看板/统计、P1通知/排班、评价编辑删除、自动评价/关闭、终态恢复均未开发。没有扩大管理员代验收/评价或原业务读取范围。

**CampusFix W7-D5 一次性评价、管理员关闭及完整W7业务闭环实现与自动化验证已完成，等待独立源码审查。W7全阶段浏览器人工验收将在D5通过源码审查并提交后统一执行；未合并main。**

## 9. 最终Git及文件核验

- `git diff --check`与`git diff --cached --check`实际退出0；17个新增文件逐项no-index空白检查无诊断（不以其差异退出码1冒充空白错误）。32文件.NET严格UTF-8解码、末尾换行及指定真实凭证模式扫描均无错误；前次引号失败不算验证通过。未发现硬编码真实凭据，测试日志另逐个检查随机Token未出现。
- 最终修改15/新增17/删除0；暂存0，HEAD仍为起始`0dad330913aaf75da8a17ccbe89de378b0ba974c`。常规已跟踪Diff为`15 files changed, 126 insertions(+), 16 deletions(-)`，另17个新文件均需审查。`git diff --cached --stat`为空；无无关未跟踪项。
- Schema/seed、domain状态机及Policy、AuthorizationService/AuthService、W6 dispatch、vcpkg/npm锁文件、SRS/roles/flows/database-design差异为空；冻结文档仅按明确批准补D-022/API-35及关联说明，原接口权限不变。全量AI相关回归是既有本地替身测试，不是重新进行真实DeepSeek验收。
- git check-ignore确认build证据/CTest日志、SQLite/WAL、dist及node_modules仍被忽略。结项8848/5173监听数0；未停止其他用户服务。没有git add/commit/push/merge，没有进入W8或合并main。LF→CRLF提示保留，为Git换行提示，不是空白检查失败。
