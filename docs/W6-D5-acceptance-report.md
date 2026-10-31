# W6-D5：管理员智能调度面板与端到端验收

日期：2026-10-31。执行主体：Codex。

**状态：代码实现和自动化验证完成，真实浏览器人工验收未完成，等待独立源码审查。W6不能标记为最终验收通过；功能分支尚未合并main。**

## 1. 起始基线与范围

- 实际 `git fetch origin` 成功；分支 `feature/w6-dispatch`，起始 HEAD 和 `origin/feature/w6-dispatch` 均为 `a086eab55fff2580be8802bf0d432a7e02f0ba27`。
- 起始工作区/暂存区干净，没有 reset、stash、覆盖修改或自动提交。W6-D4 已作为该提交保存。
- 按 AGENTS 优先级阅读 SRS14/50/51、W6-D5计划、API-17/18、FR-007/008/013、D-006/012/013、D4报告、现有前端路由/权限/认证/API/管理页与测试，核对后端正式推荐和派单实现。
- 本轮只接入调度界面与测试，不改后台过滤/评分/事务、状态机、认证授权、Schema、seed、冻结API或AI业务。没有W7接单、拒单、维修、转派或P1策略。
- 新增一个独立C++测试夹具及CMake测试构建目标，用于真实登录和临时虚构数据；它不是生产HTTP接口，也没有增加正式账号管理能力。没有新依赖，package-lock无需因脚本变更而改动。

## 2. 完整文件清单

项目根目录：`G:\A_Study\CppProject\CampusFix`。以下均为相对该根目录路径。

修改8个：

1. `backend/CMakeLists.txt`：仅新增隔离UI测试夹具构建目标；原34项CTest注册不变。
2. `frontend/package.json`：增加调度单测及可独立准备隔离环境的HTTP脚本。
3. `frontend/src/api/client.js`：仅对422 WORKER_INELIGIBLE提取九种已知公开reason码；不传递原始data、Axios config或敏感字段。
4. `frontend/src/router/index.js`：用真实调度页替换占位，新增指定工单路由。
5. `frontend/src/views/AdminOrdersView.vue`：对有调度菜单资格、已合法读到的PENDING_DISPATCH工单提供导航链接，不触发POST。
6. `frontend/src/layouts/AppLayout.vue`：更新过时占位说明。
7. `frontend/src/components/AdminOrderDetailDrawer.vue`：更新过时说明，不在抽屉新增业务动作。
8. `docs/progress-log.md`：追加本阶段记录。

新增12个：

1. `backend/tests/integration/DispatchUiFixture.cpp`
2. `frontend/src/api/dispatch.js`
3. `frontend/src/utils/dispatch.js`
4. `frontend/src/views/AdminDispatchView.vue`
5. `frontend/src/components/DispatchCandidateCard.vue`
6. `frontend/src/components/DispatchScoreDetail.vue`
7. `frontend/src/components/DispatchAssignDialog.vue`
8. `frontend/tests/dispatch.test.js`
9. `frontend/tests/dispatch-security.test.js`
10. `frontend/tests/dispatch-http.test.js`
11. `frontend/tests/run-dispatch-http.mjs`
12. `docs/W6-D5-acceptance-report.md`

最终实际修改8/新增12/删除0，共20文件，暂存区为空，无无关未跟踪文件。不包含原有W1-D3交接文件、运行数据库、凭据、node_modules、dist或构建缓存。

## 3. 页面、组件与权限边界

- `/admin/dispatch` 为工单选择入口；`/admin/dispatch/:id` 为指定工单推荐页，均复用 `requiresAuth`、`entryId=admin-dispatch`、routeAccess与现有navigation的DISPATCH判定。
- dispatcher依AREA_SCOPE显示入口，sysadmin依明确DISPATCH显示；这些只是体验控制，API-17/18继续由后端判定真实资源范围。
- 有BUSINESS_READ时才请求授权范围内、PENDING_DISPATCH、分页的managed列表；没有BUSINESS_READ时不请求列表、详情、联系人或人员管理接口，只允许输入合法ID/授权深链接尝试API-17。
- 新页面不使用AdminOrderDetailDrawer代替调度授权，不调用API-14获取其他区域人员，也不合并scope_location_ids推断某能力的真实范围。
- CandidateCard展示真实rank/总分/负载/容量/推荐理由/风险；ScoreDetail逐项显示服务器六项分数及两种样本数。没有前端公式、加分、重新排序或自动选择第1名。
- 0～2样本时说明对应历史分为冷启动中性分，不冒充实际维修成绩。风险列表逐条使用服务器返回值，没有风险时不制造“历史不足”警告。
- 其余合格候选完整展示原rank；排除码九项中文映射，未知码仅给安全兜底文案（含原型属性名也不会被当作已知码），不展示原始敏感人员资料。说明原因次数之和不是人员总数。
- 空候选且summary为空时明确显示：**“该工单位置当前没有可推荐的覆盖范围内维修人员，请核对人员服务区域或联系有权限的管理员。”** 不推断“全校没有维修员”，不为补统计越权读取其他区域。
- ElDialog在确认组件中显式局部导入注册，其余标签复用现有注册；未重构全局组件框架。卡片网格、分项、对话框及页面有窄屏CSS，但真实390px布局仍待浏览器核查。

## 4. API与本地流程

- `getDispatchCandidates(orderId)` 调用真实GET；`assignWorker(orderId,payload)` 调用真实POST，均使用现有apiClient、相对地址和authRequired。POST白名单只有 worker_id、expected_version、可选reason，丢弃rank/得分/容量/目标状态/操作者等覆盖字段。
- orderId沿用既有正整数校验；工单/人员/版本/回执ID均检查Number.isSafeInteger，超出安全范围明确阻断，绝不截断或把字符串ID擅改进冻结协议。这是当前JSON整数在JS中的限制，不改Schema或后端64位类型。
- 推荐数据先检查完整类型、已知策略/权重、排名连续性与Top3/其余分组、重复ID、六项有限数值、样本数等结构，再按白名单投影。校验不是前端重算资格或评分；缺失/异常DTO不产生可提交候选。
- 状态明确区分idle/loading/ready/empty/error/forbidden/conflict/submitting/unknown_result/success。刷新真正重新GET，不拿本地分数冒充实时推荐。
- 本地generation机制清理切换工单、身份/权限/范围变化、退出和路由卸载后的旧快照、版本、选择、理由、回执及核实结果；晚到GET/POST/详情响应不能覆盖新页面。
- 点击候选只打开确认对话框。显示ID、版本、名称、rank、负载、得分和服务端重查提示；明确点击确认才POST。非Top3理由必填，最多500个Unicode字符，拒绝非法控制/孤立代理及纯Unicode空白。
- submitting期间按钮与选择受限，重复调用不产生第二个POST，不自动超时重试。200回执校验成功才显示assignment_id、worker、PENDING_ACCEPT和新version，清除旧候选；如有读授权再查询真实详情核实，不只改本地状态。

### 错误与未知结果

| 情况 | 行为 |
| --- | --- |
| 401 | 复用认证拦截器清理失效会话、受保护路由处理；旧候选失效 |
| 403/404 | 清除旧快照/选择/理由，不显示曾经读到的候选，不扩大读取 |
| 三类409 | 清除旧版本及候选，提示重新读取；不替换expected_version自动重放 |
| 422 WORKER_INELIGIBLE | 使用后端允许披露的已知reason中文说明，清除不再可用快照，要求刷新 |
| 其他422 | 保留当前可编辑理由和原版本；允许用户修正，不绕过后端最终排名校验 |
| POST超时/断网/5xx/异常回执 | unknown_result，禁止重发；可能已提交，不能断定成功或失败 |
| 未知结果只读核实 | 仅有BUSINESS_READ时查询详情；即使现态PENDING_ACCEPT也仍不认定是本次POST成功；无读取权限保持未知并联系授权管理员 |

没有把GET推荐409当成本次成功证明。未知结果同一工单不能通过“刷新推荐”重置成可直接重发状态。

## 5. 真实HTTP与独立数据库证据

执行 `npm.cmd run test:dispatch:http`，脚本自动创建独立临时库，迁移/seed后由测试夹具设置选定虚构账号的随机Argon2id凭据，启动指定Debug后端和真实Vite代理。只覆盖该测试Vite实例的代理目标到随机回环端口，正式vite.config.js及用户5173/8848服务不改。测试密码/Session不在源码、命令行或输出中，不使用用户手工业务库。

**6/6集成测试通过，35次实际前端模块代理请求、10次独立SQLite全表指纹、4条派单关联链路。** 不累计重跑次数。启动健康探测和首页/health检查不计入35；匿名无凭据在apiClient本地阻止，另有真实已撤销Token的401，不能把本地拒绝冒充匿名HTTP。

实际流程和状态：

```text
POST /auth/login → 200；GET /auth/me → 200（真实dispatcher身份）
GET /work-orders?status=PENDING_DISPATCH&view=managed... → 200
GET /work-orders/5/dispatch-candidates → 200
POST /work-orders/5/assign → 200
GET /work-orders/5 → 200（真实状态与负责人核实）
GET /work-orders/6/dispatch-candidates → 200
POST /work-orders/6/assign → 200（第4名，人工原因留存）
POST /work-orders/7/assign → 200；旧快照再次POST → 409
POST /work-orders/8/assign → 422 WORKER_INELIGIBLE（预览后停岗）
POST /work-orders/8/assign，旧version → 409
GET /work-orders/10/dispatch-candidates → 200（多重排除/无覆盖两种空候选）
GET /work-orders/14/dispatch-candidates → 404（sysadmin调度范围之外）
reporter的候选GET与派单POST → 403
真实退出后使用旧Token GET候选 → 401
```

35次状态统计：200×28，409×2，422×1，404×1，403×2，401×1。

实际Top1数据回执摘要（独立虚构库）：

```json
{"order_id":5,"assignment_id":1,"worker":{"id":1,"display_name":"Demo Worker 1"},"status":"PENDING_ACCEPT","version":4}
```

API-17真实样本为Top3人员1/2/3、其他人员4/5/6；初始分数81/74/67/60/53/53，人员1分项100/70/100/60/60/0，样本0/0。前端保留这些服务器值，不再计算总分。

独立C++夹具读取真实SQLite，而非前端直连数据库：

| 场景 | order_id | worker_id | assignment_id | snapshot.assignment_id | log.assignment_id | 最终状态/version |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| Top1正常回执 | 5 | 1 | 1 | 1 | 1 | PENDING_ACCEPT/4 |
| 非Top3有原因 | 6 | 4 | 2 | 2 | 2 | PENDING_ACCEPT/4 |
| 客户端丢回执、有读授权 | 9 | 1 | 4 | 4 | 4 | PENDING_ACCEPT/4 |
| 客户端丢回执、DISPATCH-only | 15 | 1 | 5 | 5 | 5 | PENDING_ACCEPT/4 |

上述四条均检查唯一有效派单、完整六名最终候选快照、ORDER_ASSIGNED，以及日志request_id与实际成功HTTP回执请求ID相同；非Top3原因在SQLite中正确保存。首次推荐前后指纹不变，冲突/资格拒绝前后数据不变，integrity_check=ok、foreign_key_check为空。完整安全证据保存于忽略的 `backend/build/Testing/w6d5-frontend-evidence.json`，不保存原始登录Token/密码。

**客户端响应丢失是明确的测试注入**：真实POST已由Drogon提交并返回200后，测试HTTP adapter丢弃回执并模拟ECONNABORTED；不是伪造后端评分或成功响应。前端对这两个场景均保持unknown_result、回执为空、不可重发；有读授权时实际GET显示当前状态，但仍不把它升格成本次成功，无BUSINESS_READ时没有列表/详情HTTP。数据库提交事实由独立夹具另行证明。

## 6. 构建与回归实测

Windows PowerShell项目根目录：

```powershell
$cmake = 'D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = 'D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
& $cmake --build backend/build --config Debug --parallel 4
& $ctest --test-dir backend/build -C Debug --output-on-failure
& $ctest --test-dir backend/build -C Debug -N
cd frontend
npm.cmd run test:auth
npm.cmd run test:orders
npm.cmd run test:admin
npm.cmd run test:security
npm.cmd run test:dispatch
npm.cmd run test:dispatch:http
npm.cmd run test:ai
npm.cmd run test:ai:draft
npm.cmd run test:ai:handoff
npm.cmd run build
```

| 检查 | 实际结果 |
| --- | --- |
| MSVC Debug完整构建 | 退出0；新增测试夹具可链接运行，未见编译/链接警告 |
| CMake配置与依赖 | 自动重配成功，现有包全部already installed，无下载/升级 |
| 后端全量CTest | **34/34，失败0，退出0，196.32秒** |
| CTest注册 | 34项，按HEAD的静态及foreach展开名核对无缺失；没有新增/删除CTest项 |
| test:auth | 9/9，退出0 |
| test:orders | 16/16，退出0，含既有组件注册回归 |
| test:admin | 23/23，退出0 |
| test:security | 10/10，退出0 |
| test:ai | 12/12，退出0 |
| test:ai:draft | 19/19，退出0 |
| test:ai:handoff | 30/30，退出0 |
| 既有前端单元测试小计 | **119/119**；不是复用此前历史结论 |
| 新test:dispatch | **35/35，退出0**；含实际SFC setup/watch执行、异步生命周期及模拟API状态测试，不冒充浏览器 |
| 前端单元测试合计 | **154/154**，不含独立真实HTTP的6项 |
| 新test:dispatch:http | **6/6，退出0**；全流程自建服务与临时数据清理成功 |
| Vite生产构建 | 退出0；实际JS约775.89kB/gzip252.62kB，存在>500kB chunk警告，未掩盖 |

没有安装测试框架、修改PowerShell执行策略、更新全局npm或添加新HTTP库。首次直接`npm`受本机npm.ps1执行策略阻止，改用已有npm.cmd成功。

## 7. 真实浏览器状态：未完成

按computer-use技能尝试可用UI通道：原生管道返回“系统找不到指定的文件”，备用浏览器控制返回“browser access host is unavailable”，浏览器清单为空。无法连接真实页面；没有截图、Console目视检查或鼠标/触屏操作证据。

曾临时保留隔离服务准备浏览器检查，但通道不可用后中断该待机脚本（退出1，不是6项HTTP断言失败）。核对自建进程/端口已停止，精确删除其单个临时库/随机凭据目录及忽略的运行元数据；没有删除用户业务文件。随后普通模式完整重跑6/6、脚本退出0，自动验证端口、进程及临时库清理。

以下均为**待用户人工验收**，不能依据CSS或Node测试改成通过：

1. 桌面页面卡片、六分项、样本、原因、风险和确认对话框真实可见；Console无未注册组件警告。
2. 390px下卡片、权重、原因和按钮可读、可操作，无严重横向遮挡。
3. dispatcher登录、待派列表与详情深链接、普通Top3选择、取消确认无POST、确认后真实回执。
4. 非Top3原rank、必填原因、空白/500字符边界与派单成功后的显示。
5. 多原因无候选，以及覆盖池为空时的精确提示。
6. 两个浏览器会话同单竞争后409提示/快照清理，且无自动重发。
7. reporter越权/匿名保护；DISPATCH-only sysadmin不显示或查询列表/详情，深链接按后端授权。
8. 页面切换、退出、离线/未知结果及恢复时的实际交互。

### 可复现的隔离人工验收准备

1. 构建完成后在frontend运行 `npm.cmd run test:dispatch:http -- --manual`。它先执行真实HTTP测试，再保留**随机端口**的隔离服务，最长20分钟；不要拿用户日常业务库代替。
2. 仅在本机查看忽略的 `backend/build/Testing/w6d5-browser-runtime.json`：base是浏览器URL，secrets指向临时凭据文件。使用该文件passwords中的选定虚构账号密码登录；**不要将密码、整个凭据JSON或Authorization截图/发送到聊天/Git**。
3. 可用dispatcher、sysadmin（DISPATCH-only）、reporter账号；临时凭据中browser_top/browser_fourth/browser_empty为尚未派单的样本ID。以页面实时排名为准，不硬编码人员资格。
4. 空覆盖池可在**项目根目录**先读取运行元数据到 `$dispatchRuntime` 后，调用其中fixture路径的 `--mode <db> no-coverage`；恢复用`coverage`。这些命令仅用于该脚本创建的临时库，不能指向用户数据库。
5. 结束时在项目根目录执行 `New-Item -ItemType File -Path .\backend\build\Testing\w6d5-browser-stop`，待脚本打印清理成功；不要批量终止Node或后台服务。

人工验收结果补齐后才可更新本节，不得自动假定通过。

## 8. Git核验、风险与W6状态

- 收尾实际修改8/新增12/删除0、暂存0，无无关文件；未执行git add/commit/push/merge。完整清单见第2节。
- `git diff --check`退出0，12个新增文件另做no-index检查无空白诊断；全部20文件UTF-8与末尾换行检查通过。Git的LF→CRLF提示不是空白错误。跟踪文件实际Diff为`8 files changed, 32 insertions(+), 7 deletions(-)`，不含12个新增未跟踪文件，未把它当作全部代码行数。
- 冻结需求/设计、Schema/seed、backend/src、D2/D3算法及AI文件diff为空。package.json只增加脚本，依赖版本与lock不变。
- 构建和证据JSON、测试数据库/WAL、随机凭据、node_modules/dist仍被忽略。所有新增文件另做no-index空白、UTF-8与末尾换行检查；不靠暂存新增文件取得Diff。
- 生产构建chunk警告保留；没有为此重构全站或提高阈值。浏览器UI、跨机器、真实生产规模性能、真实DeepSeek、独立admin:http/AI-D3全链路脚本及独立W2冒烟未执行，不与本轮局部HTTP/后端CTest混淆。
- 内存Token与刷新需重登保持不变；前端显示权限摘要不能代替服务端逐资源授权；JS安全整数限制需后续跨端契约讨论，不能静默丢精度。
- W6-D1～D4基线已提交，D5本轮代码与自动化已完成；真实浏览器验收和独立源码审查仍待完成，尚未满足W6最终验收条件。没有进入W7，没有合并main。

**CampusFix W6-D5 管理员智能调度面板与端到端验收未完成（代码与自动化已完成，真实浏览器验收待补），等待独立源码审查；W6功能分支尚未合并main。**
