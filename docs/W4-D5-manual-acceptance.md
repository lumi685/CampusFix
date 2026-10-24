# W4-D5 真实浏览器验收与截图索引

## 状态与证据边界

- 最终结论（2026-10-24补齐G3～G6后）：**W4-D5人工与自动化验收通过。A～G和七项附加测试由用户确认通过；新增截图已补齐同一工单关联。favicon404与Chrome Built-In AI提示为非阻塞问题，本轮不修复。**以下早期“待确认”和旧G3内容不匹配的段落保留为历史，不再表示当前仍有该缺口。等待用户批准Git提交，不进入W5。

- W4-D4：用户已反馈注册修复后四类控件正常、浏览器功能通过、F12 Network提交及获取无错误；该人工结论已补记，原失败历史保留。
- W4-D5自动化：见 `W4-D5-acceptance-report.md`。真实HTTP/SQLite、Vue逻辑单测和生产构建不能代替浏览器端到端。
- 本轮受控浏览器入口返回 `apps=[]/browsers=[]`，没有可操作连接，停止对应自动化路径；没有绕过URL安全核验、借助不受控浏览器或生成假截图。
- 2026-10-23原始状态：W4-D5精确操作与A～G截图待提供；Codex浏览器实测项为零。2026-10-24已收到并审阅下面8张本地截图，但不是Codex重新操作浏览器；缺失的精确结果仍待用户确认。

## 2026-10-24 人工截图补记（部分证据已核验，未宣告全部通过）

- 执行主体按用户记录为项目开发者，环境为Windows/Chrome/Vue3-Vite/Drogon/SQLite。2026-10-24为本次文档补记与截图审阅日期，实际人工执行日期未单独确认；页面工单时间显示2026/10/24。
- 用户提交的文字仍包含“通过 / 失败 / 未测”、数据库名及工单号占位符，附加测试结果为空。因此不能直接记录为“人工全部通过”。
- 数据库文件名：**待用户确认**，不根据截图工单编号或旧自动化库推断。
- 截图可读到Reporter工单`WX202610240002`（/orders/7，测试3），Worker本人列表工单`WX202610240001`（W4D5测试-宿舍灯具不亮）；请用户确认它们是否为本次指定验收单。
- C/D/E对应Reporter同一编号，详情待审核/version2；时间线有Demo Reporter 1的创建草稿及提交报修，页面时间分别2026/10/24 10:22:19与10:22:20。页面显示不是本轮独立SQLite核对，不用此前自动化临时库ID9/10/11替代人工库证据。
- B截图实际是worker填写页，并非Reporter填写页；A选择“高”，C详情显示“普通”，不能将A→C强行描述为同一次未改变的表单提交。
- G截图可见目录、列表、详情与时间线GET为200，但详情请求名为6，与C/E的工单7不同；没有POST创建201、submit200或响应体证据。DevTools顶部有1条错误指示，内容未展开，不能声称Console无错误，也不能猜测错误原因。
- 刷新重新登录、后端离线恢复、退出路由保护、非法表单、跨用户访问、同草稿失败重试、重复提交保护：**用户本次未填写结果，均保留人工待确认**。此前自动测试通过原样保留，本轮未重跑。

## 1. 选择验收数据库

### 本次实际人工环境（用户已确认）

- 数据库：`backend/build/w4d4-manual.sqlite3`，本轮仅检查文件存在，没有读取、迁移、seed、覆盖或修改它。
- Reporter验收单：`WX202610240002`；Worker本人报修单：`WX202610240001`。来源为用户明确确认，并与既有截图编号相符。
- 执行人：项目开发者。环境：Windows + Chrome + Vue3/Vite + C++ Drogon + SQLite。本次补记日期2026-10-24，不编造每项实际操作的精确时间。
- A～G各项结果：**全部通过（用户人工确认）**，不是Codex本轮重新操作浏览器。G3～G6后续补拍样本的POST状态及响应ID关联现已由Codex对实际图片核验，详见下方最终补记。

| 附加测试 | 用户实际确认 | 本轮证据性质 |
|---|---|---|
| 刷新重新登录 | 通过 | 用户人工确认，非本轮自动重跑 |
| 后端离线及恢复 | 通过 | 用户人工确认，非Codex浏览器操作 |
| 退出后的路由保护 | 通过 | 用户人工确认 |
| 非法表单校验 | 通过 | 用户人工确认 |
| 跨用户工单访问 | 通过 | 用户人工确认 |
| 同草稿失败重试 | 通过 | 用户人工确认；保留此前独立逻辑/HTTP自动化结果 |
| 重复提交保护 | 通过 | 用户人工确认 |

以下新库初始化命令是早期可选复现指引，不是本次实际操作记录；不要对已确认的人工库重新初始化。

自动化数据库已清理，不能在浏览器中查询报告中的临时工单ID9/10/11。人工验收会生成自己的真实ID/编号。

推荐新建只含虚构数据的独立人工库；本轮不会代替用户创建或重置既有人工库。若选择已有演示库，先确认其实际路径、用途、内容均为虚构，再告知Codex是否需要协助；不要提供密码。

以下在仓库根目录PowerShell手动执行。如果同名文件已存在，应改用另一个未存在文件名，不要覆盖、删除或重置它：

```powershell
if (Test-Path .\backend\build\w4d5-manual.sqlite3) { throw '该验收库已存在，请核对用途或选择新文件名' }
.\backend\build\Debug\campusfix_backend.exe --migrate --db .\backend\build\w4d5-manual.sqlite3 --migrations-dir .\backend\migrations
.\backend\build\Debug\campusfix_backend.exe --seed --db .\backend\build\w4d5-manual.sqlite3 --seed-file .\backend\seeds\001_demo_data.sql
.\backend\build\Debug\campusfix_backend.exe --set-demo-password --db .\backend\build\w4d5-manual.sqlite3 --username demo_reporter01
.\backend\build\Debug\campusfix_backend.exe --enable-demo --db .\backend\build\w4d5-manual.sqlite3 --username demo_reporter01
.\backend\build\Debug\campusfix_backend.exe --set-demo-password --db .\backend\build\w4d5-manual.sqlite3 --username demo_worker01
.\backend\build\Debug\campusfix_backend.exe --enable-demo --db .\backend\build\w4d5-manual.sqlite3 --username demo_worker01
```

每条命令须确认成功再继续。密码只在终端隐藏输入，不贴到聊天，不保存到Git；不批量启用全部账号。

终端一：

```powershell
$env:CAMPUSFIX_DB_PATH = (Resolve-Path .\backend\build\w4d5-manual.sqlite3).Path
.\scripts\start-backend.ps1
```

终端二：

```powershell
.\scripts\start-frontend.ps1
```

打开 `http://127.0.0.1:5173/login`。任何网络截图都不能显示登录响应中的Token、请求Authorization或密码。

## 2. reporter主链路

1. 登录demo_reporter01，Network中确认login为200（不要截图其完整响应）；me为200且身份为本人，菜单包含“我提交的报修/提交报修”。取得证据A。
2. 打开报修页，四控件均可见；Console不得有Failed to resolve component。分类GET200，根分类即使无子节点也可选择。位置根GET200，依次展开校区→建筑→楼层→房间，各parent_id请求200且只含直接子级。ID来自返回，不手输固定ID。取得证据B/G。
3. 初始NORMAL显示“普通”；选URGENT出现应急提醒，再按需要选择NORMAL。期望时间可空，也可填本地08:30并在请求Payload确认转换为对应UTC；禁止把无时区本地字符串当Z时间。未填写必填项时不得正常创建。
4. 输入仅虚构的标题“W4-D5人工报修A”、描述、联系人及电话00000000000。点击一次提交，Network应是创建201（DRAFT/version1/order_no null）→同ID submit200（PENDING_REVIEW/version2/真实编号/UTC submitted_at）。记录两次request_id，但不要记录Bearer。
5. 跳转详情，标题/描述/分类/位置/编号/状态/版本与响应一致；无未实现操作按钮。取得证据C。本人列表能找到该单，状态与分类筛选、分页和空结果合理。取得证据D。
6. 时间线仅含公开信息，创建→提交，actor为本人，时间及状态正确，无internal_note/metadata_json/stored_path/凭据哈希。取得证据E。
7. 点击退出（logout200），未登录不能直接打开受保护URL。停止后端，使用**同一库路径**重启；重新登录后原单仍可由列表/详情/时间线读取。整页刷新需重新登录符合内存Token策略。

## 3. worker本人报修与隔离

1. reporter退出后登录demo_worker01，顶部身份和菜单应改变，不能残留上个角色数据。
2. 使用同样流程提交“W4-D5人工报修W”；取得真实、与reporter不同的编号。工单的reporter_id应为worker当前用户，不因主要角色仍是worker而冒充他人。
3. “我提交的报修”固定view=reported，只列本人报修；“待处理任务/维修记录”仍为独立后续占位，不能把本人报修混入维修派单。
4. 以worker查看无权访问的reporter工单，应为统一404（若另有合法派单阅读资格，须按真实资格判断，不把可读任务误判为原报修人权限）；当前新建人工库没有该派单。取得证据F。

## 4. 提交失败后重试

- 仅在此独立库与浏览器DevTools使用请求阻止，临时阻止 `*/api/work-orders/*/submit`，不要阻止创建。
- 提交一张新的虚构报修：创建成功，但submit被浏览器阻止。页面须保留草稿ID/version，说明尚未正式提交。
- 解除阻止后点击“重试提交同一草稿”，必须仅调用同ID的submit，不能再POST创建。最终一个编号、两条日志。
- 请求阻止属于网络失败，不是HTTP409。真实409的后端状态保护、Service回滚、前端逻辑已自动化覆盖；浏览器409展示仍待受控复现，不能以本步骤冒充。
- 若创建响应丢失、页面明确提示结果不确定，先查本人列表，不能盲目重新创建。离开/刷新会丢失本页重试状态，没有复杂草稿恢复功能。

## 5. 人工库SQLite核对（仅虚构库）

可用本轮只读测试工具保存独立SQL快照，不修改库中逻辑数据：

```powershell
.\backend\build\Debug\campusfix_report_acceptance_fixture.exe --snapshot .\backend\build\w4d5-manual.sqlite3 .\backend\build\Testing\w4d5-manual-snapshot.json
```

工具不会打印密码/会话哈希。快照仍可能含完整报修描述和联系人，只在上述全虚构库使用，不要直接上传含真实个人信息的快照。

按本次ID核查orders中的reporter_id/status/version/order_no/created_at/submitted_at与页面及HTTP一致；logs中两条事件的actor_id、from/to、时间、request_id与创建/提交请求对应。integrity应为ok，foreign_key_violations应为0。为了验证GET只读，可在只有查询、没有登录退出/提交/改授权发生的区间，对比前后digest；登录/退出会合法改变会话表，不能误判为GET写入。

## 6. 实际截图索引（2026-10-24逐张审阅）

真实目录为`backend/build/Testing/w4d5-screenshots/`，原8张加最终G3/G4/G5/G6共12张图，均保持原样且被build忽略规则排除。本轮未复制到docs、未编辑/合成或重命名截图，也不自动加入Git。

| ID | 实际文件名 | 可见事实 | 尚不能证明 |
|---|---|---|---|
| A | `A-report-form.png` | 报修人导航、分类选择器、已展开校区/建筑/楼层/房间、紧急度“高”及已填日期控件 | 完整交互过程、请求ID及UTC转换 |
| B | `B-filled-report.png` | 维修人员导航、Demo Pipe Fault、已选位置、紧急与应急提醒、虚构标题/联系人及零值电话 | Reporter填写流程；真实请求Payload和目录来源须结合Network |
| C | `C-submitted-detail.png` | Demo Reporter 1，/orders/7，WX202610240002，待审核/version2及详情 | 创建201/submit200、数据库行及日志request_id |
| D | `D-my-orders.png` | Reporter列表包含同一WX202610240002，另两条历史行，页面Total3 | 筛选/分页操作、SQL total一致性 |
| E | `E-order-timeline.png` | /orders/7同一编号；创建/提交两事件、Demo Reporter 1及上述页面时间 | SQL原始actor_id/request_id及响应字段完整白名单 |
| F1 | `F1-worker-navigation.png` | Demo Worker 1/demo_worker01；本人报修与维修任务入口独立 | 越权请求是否被后端拒绝 |
| F2 | `F2-worker-own-order.png` | Worker本人列表包含WX202610240001及一条seed本人报修，Total2 | Worker工单详情/时间线、所有角色数据隔离场景 |
| G | `G-network-results.png` | 可见fault-categories/locations/列表/详情6/timeline请求均200 | 创建201、submit200、工单7全链路；Console顶部1条错误原因待补 |
| G3（最终新版） | `G3-network-create-submit.png` | Network中work-orders201、submit200、详情名10为200、timeline查询200 | 与G4/G5/G6共同核验新样本，不冒充原/orders/7的POST截图 |
| G4 | `G4-create-response.png` | 选中work-orders响应：code0、id10、order_no=null、DRAFT、version1 | request_id右侧裁切，不抄成完整ID |
| G5 | `G5-submit-response.png` | 选中submit响应：code0、id10、WX202610240005、PENDING_REVIEW、version2、submitted_at=2026-10-24T03:30:20.075Z | 属新增补拍样本，不覆盖Reporter/Worker原验收单 |
| G6 | `G6-order-detail-response.png.png` | 实际为submit的General：POST、/api/work-orders/10/submit、200 OK | 文件名并非实际详情响应；请求头折叠，未读取隐藏Authorization |

已检查可见区域：未见明文密码、Bearer Token、Authorization请求头、登录响应Token或可识别的真实私人资料；出现Demo身份、测试文字及零值电话。不把截图安全目视检查说成已检查隐藏请求头、HAR或完整浏览器日志。A～G现已由用户明确确认通过，不再是空白模板；写请求截图仍须补充，不能据文件名声称内容齐全。

### 旧G3与Console核验历史（随后由用户替换）

- 2026-10-24本轮实际打开G3图片：仅看到“Failed to load resource ... 404 ... :5173/favicon.ico”和“This page uses Chrome's Built-In AI features (LanguageDetector)! ... content_main.js:5633”。
- favicon404按用户裁定记为非阻塞UI图标资源缺失，不影响业务，本轮不修复。
- LanguageDetector信息与content_main.js来源在图中可见；按用户提供结论记录为浏览器内建AI相关提示，没有证据表明CampusFix业务异常，不修改业务代码。没有宣称Console完全无提示/错误。
- G3可见区域不含密码、Token、Authorization或私人资料，但无法确认创建/提交两次POST、具体工单ID及关联；这是证据归档缺口，不是新发现的业务失败。
- 请补一张或一组真正的Network证据：POST创建201的响应data.id、同ID的`/api/work-orders/{id}/submit`200，及与验收工单的关联（如响应order_no）。隐藏Authorization、密码、Token；不要将登录响应或未经清理HAR发出。本轮不强行重建旧工单来制作证据。

### 正确G3替换后的复核

- 用户明确通知已替换；Codex重新读取同一路径，当前PNG为36294字节，与旧Console图6070字节不同，未复用旧图判断。用户覆盖前的Console图不再位于该路径，其内容仅保留为此前实际审阅记录，未恢复或复制旧图。
- 新图可见work-orders201、submit200、详情请求名9及timeline查询均200。这补齐了创建/提交状态码的可见证据；但HTTP方法列、创建响应data.id、submit完整URL和响应体均未展开，不能凭顺序断言两个请求同ID。
- 当前详情9与此前C/E的/orders/7不同，不能将这组写请求直接归为WX202610240002，也不能与自动化临时库order9混同。可能是新的人工验证单，具体角色/编号需用户确认。
- 新图可见区域未发现密码、Token、Authorization或私人信息；顶部1条错误指示仍存在，其此前Console内容已按非阻塞问题记录，不要求本轮修复业务。
- 尚需最小关联证据：创建请求Response中的data.id；submit的General区完整URL/方法或Response中的id（可含order_no）。若新图对应新工单，请确认编号即可，不要求重做用户已通过的全部场景。

### 最终补齐：新增样本ID10的G3～G6关联核验

- 2026-10-24用户补充新的四张证据后，Codex实际逐张打开当前G3/G4/G5/G6，而非仅按用户文字或文件名判断。
- G3总览work-orders201→submit200→10为200→timeline200；G4创建响应data.id=10、DRAFT/version1/order_no null；G6确认POST /api/work-orders/10/submit及200；G5响应id10、WX202610240005、PENDING_REVIEW/version2。请求路径与两次响应ID一致，结合总览中的详情10/时间线查询及用户明确确认，同一新增样本链路成立，原缺口已补齐。
- 新样本工单ID10/编号WX202610240005与此前Reporter WX202610240002（/orders/7）、Worker WX202610240001分别保留。也不与2026-10-23自动化临时库ID10混同。本轮不读取或修改用户人工库，不声称对新样本另做SQL核对。
- 四图可见区域未发现密码、Token、Authorization或私人信息；G6的请求头折叠，不据此宣称检查了隐藏内容。G4/G5右侧request_id裁切，未补写未知字符。没有伪造/合成图片。
- 最终状态：自动化验收通过（原历史实测保留，本轮未重跑）；A～G及七项附加浏览器人工验收通过（项目开发者确认）；同一工单关联截图核验通过（Codex只读审阅用户截图，不是Codex重新操作浏览器）。favicon404、Chrome Built-In AI提示保留为非阻塞问题，本轮不修复。

截图前检查：没有明文密码、Bearer、Authorization、登录响应Token、真实姓名/电话、私人资料或生产数据。不要直接导出未经清理的HAR（常含凭证）。

测试结束在两个终端Ctrl+C停止服务，不结束其他Node进程。人工库由用户保留供审计，不由Codex删除。验收反馈请提供：执行日期、所用虚构库用途、reporter/worker各自工单编号、A～G证据与失败场景结果；不要提供密码或Token。
