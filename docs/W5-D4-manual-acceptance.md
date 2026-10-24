# W5-D4 浏览器人工验收清单

补记日期：2026-10-24。执行主体：项目开发者本人。状态：**用户真实浏览器人工验收通过，用户确认14/14项全部通过；等待最终源码与Git审查。**

本结论来自用户本次明确反馈，不是Codex独立操作浏览器或重新执行自动化浏览器测试。此前实施时浏览器技能文件不可读取、无可用受控浏览器工具的历史保留。用户未提供本次截图、Network回执、请求ID、人工工单编号、具体页面时间、实际数据库文件名或浏览器版本，不补造这些证据；本页日期是补记日期，不推断实际人工操作时刻。

## 1. 安全准备（保留原指导，不代表本轮重新执行或用户实际使用的环境记录）

不要使用或覆盖W4/W5此前人工验收库。先确认下列新路径不存在；如已存在，选择另一新文件名并同步替换后续命令，不删除已有库。

```powershell
# 项目根目录
Test-Path .\backend\build\w5d4-manual.sqlite3
# 仅当返回False时执行：
.\backend\build\Debug\campusfix_backend.exe --migrate --db .\backend\build\w5d4-manual.sqlite3 --migrations-dir .\backend\migrations
.\backend\build\Debug\campusfix_backend.exe --seed --db .\backend\build\w5d4-manual.sqlite3 --seed-file .\backend\seeds\001_demo_data.sql
```

对demo_reporter01、demo_worker01、demo_dispatcher、demo_sysadmin按需逐个设置密码并显式启用。密码只在本机隐藏输入，不发给Codex、不写命令参数/源码/SQL/日志，不批量启用其他账号：

```powershell
.\backend\build\Debug\campusfix_backend.exe --set-demo-password --db .\backend\build\w5d4-manual.sqlite3 --username demo_reporter01
.\backend\build\Debug\campusfix_backend.exe --enable-demo --db .\backend\build\w5d4-manual.sqlite3 --username demo_reporter01
# 对其他指定虚拟账号逐个替换username执行。
```

seed中的dispatcher管辖A校区，默认只能看到6名人员中该区域的3名，而非全局6名。sysadmin不因为角色取得审核或人员管理授权。默认账号和授权保持seed定义；如要验证额外能力，仅在上述独立库使用已有受控测试CLI：

```powershell
# 可选；确认--db对应独立测试库后才执行，不修改正式seed或原人工库。
# 这些已有Fixture接受位置参数，不是生产HTTP管理接口。
.\backend\build\Debug\campusfix_review_http_fixture.exe --mode .\backend\build\w5d4-manual.sqlite3 admin-read
.\backend\build\Debug\campusfix_review_http_fixture.exe --mode .\backend\build\w5d4-manual.sqlite3 admin-review
.\backend\build\Debug\campusfix_worker_http_fixture.exe --mode .\backend\build\w5d4-manual.sqlite3 admin-worker-a
# 分别授予sysadmin A校区BUSINESS_READ、A校区中既有楼宇的REVIEW、A校区WORKER_MANAGE。
# 能力摘要不会自动刷新旧前端身份；退出并重新登录后检查菜单。
```

如需复核“只有REVIEW、没有任何BUSINESS_READ”的浏览器情形，应使用另一个独立库的受控授权夹具撤销BUSINESS_READ后再登录，不能从现有scope_location_ids推断已撤销。此前实施时该浏览器情形未测；本次用户确认权限隔离及总体14/14通过，但未提供该授权组合的独立夹具或Network证据，不编造具体授权变更步骤。前端逻辑测试和后端授权回归仍单独记录，不冒充人工证据。

分别在两个终端启动；若已有环境变量，先记录原值，测试后恢复。端口被其他服务占用时先核实，不批量停止进程：

```powershell
# 终端一，项目根目录
$env:CAMPUSFIX_DB_PATH = (Resolve-Path .\backend\build\w5d4-manual.sqlite3).Path
$env:CAMPUSFIX_PORT = '8848'
.\backend\build\Debug\campusfix_backend.exe
# 终端二，项目根目录
Set-Location frontend
npm.cmd run dev -- --port 5173 --strictPort
```

打开http://127.0.0.1:5173/login。以下操作只针对虚构数据。截图放在被Git忽略的`backend/build/Testing/w5d4-screenshots/`，不要截登录响应、Authorization、密码、Token或带凭据的HAR。建议记下实际浏览器版本、库文件名、工单ID/编号、人员编号；未知值留空。

## 2. 用户真实浏览器人工验收：14/14通过

用户明确确认14/14项全部通过，特别确认管理员审核、退回补充再次提交、完整字段回填、仅改容量不改变其他字段、技能修改、权限隔离及窄屏检查。下面按原清单主题归档为14个汇总项；M01～M14是本次文档归档编号，不是用户另行提供的逐项原始编号。

原清单A1～E3实际有27条细项，并非14条。因此保留原细项及实施阶段状态作历史参照，不把用户的14/14改写成27项逐条独立实测，也不将总体反馈扩充为每个极端场景都有截图或Network证据。

| 归档编号 | 原清单主题对应 | 用户人工验收结论 |
| --- | --- | --- |
| M01 | A1～A2：管理员真实工单列表、筛选、分页及页面状态 | 用户确认通过 |
| M02 | A3～A4：管理详情、公开时间线与获准内部信息展示 | 用户确认通过 |
| M03 | B1：审核入口及真实分类/紧急度控件 | 用户确认通过 |
| M04 | B2：管理员退回补充、公开原因与内部备注分离 | 用户确认通过 |
| M05 | B3：报修人补充并再次提交原工单 | 用户确认通过 |
| M06 | B4：管理员审核通过及结果刷新 | 用户确认通过 |
| M07 | B5：审核拒绝、必填原因及二次确认 | 用户确认通过 |
| M08 | B6～B7：审核表单隔离、防重及冲突核对交互 | 用户确认通过 |
| M09 | C1～C2：人员真实列表、授权范围及负载展示 | 用户确认通过 |
| M10 | C3～C4：人员完整字段原样回填，区分active、emergency_duty、availability及技能 | 用户确认通过 |
| M11 | C5：只修改容量，其他开关、状态、区域、技能等级及持证状态不被改变 | 用户确认通过 |
| M12 | C6～C8：技能修改、校验与不同人员表单隔离 | 用户确认通过 |
| M13 | C9、D1～D4：完整数据读取边界、角色/能力与资源权限隔离 | 用户确认通过 |
| M14 | E1～E3：异常连接、会话行为及窄屏控件交互 | 用户确认通过 |

上述14项均为用户真实浏览器反馈，没有把此前HTTP测试或静态检查当成人工操作。具体极端组合的独立证据如未提供，仍不推断；物理故障、跨机器等风险继续保留。

### 2.1 原27条细项：实施阶段交付时的历史清单

下表“未测”仅保留实施交付时Codex未操作浏览器的原始状态，不代表当前用户14/14的最新结论；最新结论以上方汇总为准。本轮没有逐条补造这些细项的独立过程或证据。

| 编号 | 操作与预期 | 实施阶段原始状态 |
| --- | --- | --- |
| A1 | reporter提交一张A校区工单，记录真实编号/ID；退出后dispatcher登录进入/admin/orders，只有授权工单，view=managed，无location_id查询参数 | 未测 |
| A2 | 列表状态/真实分类筛选、分页、空数据、刷新及加载提示正常；没有假派单/取消/关闭按钮 | 未测 |
| A3 | 点击管理详情，在抽屉显示真实状态/版本/描述/联系方式和公开时间线；不跳转报修人/orders/:id | 未测 |
| A4 | dispatcher只在实际返回时看到内部备注/逐键管理字段；没有原始metadata_json或敏感凭据 | 未测 |
| B1 | 进入/admin/review，待审核入口明确；真实分类和紧急度控件可用，Console没有Failed to resolve component | 未测 |
| B2 | RETURN填公开原因与不同的内部备注，确认后只发送reason/internal_note/action/expected_version；成功进入PENDING_SUPPLEMENT并刷新列表/时间线 | 未测 |
| B3 | reporter重新登录，可读公开退回原因但不能读内部备注；补充并重新提交同一工单，编号不变 | 未测 |
| B4 | dispatcher重新打开审核，当前版本准确；APPROVE发送真实category_id/approved_urgency，不发送reason，成功进入PENDING_DISPATCH | 未测 |
| B5 | 对另一虚构工单执行REJECT；空原因不能提交，二次确认取消不写入，确认后CANCELLED | 未测 |
| B6 | 打开另一工单，公开原因和内部备注均清空，不串单；提交中不能重复点击或关闭抽屉 | 未测 |
| B7 | 两个浏览器会话读取同一待审核版本，一方审核成功后另一方提交得到409；不自动重放，核对最新查询并重新打开 | 未测 |
| C1 | /admin/workers显示该账号授权范围的人员子集、真实区域名、active/availability、负载/容量及完整技能；筛选、分页和刷新正常 | 未测 |
| C2 | “核对负载”真实调用API-15，任务量与列表聚合一致；不是前端自行累计 | 未测 |
| C3 | 打开人员编辑时重新GET管理列表，真实回填校区/楼宇、active、emergency_duty、availability、容量和技能 | 未测 |
| C4 | 先有意识地准备虚拟人员为active=false、emergency_duty=true、OFF_DUTY、容量3、技能A Lv4持证/B Lv2无证；保存后关闭并重新打开确认基准 | 未测 |
| C5 | 再次打开C4人员，只将容量3改为4；确认并保存后重新GET API-14+15。false/true/OFF_DUTY及两技能等级/持证状态原样保留 | 未测 |
| C6 | 增加目录中尚未出现在当前人员页的有效技能，再移除一条，保存后重新读取准确；不能重复技能、不能自动授予持证 | 未测 |
| C7 | 容量0/101/小数、熟练度0/6/小数均不可提交；后端422显示安全字段提示且保留表单；低于有效任务数拒绝 | 未测 |
| C8 | 切换编辑不同人员、关闭后重新打开均读取实际数据，不残留前一人技能/开关 | 未测 |
| C9 | 原服务区域停用或缺失、管理字段/技能目录加载失败时显示明确错误并禁用保存，不默认替换区域/开关/技能 | 未测 |
| D1 | reporter/worker通过手输URL不能进入三个管理页面；原本人报修和维修任务入口仍分离 | 未测 |
| D2 | 默认sysadmin仅显示实际已获授权入口；BUSINESS_READ不带来REVIEW或WORKER_MANAGE；额外受控授权后菜单与身份摘要匹配 | 未测 |
| D3 | REVIEW-only且无BUSINESS_READ时审核页说明无法获取列表，不请求reported或全局列表 | 未测 |
| D4 | 范围外人员更新或撤销授权后返回403/404，界面不加载其他区域、不保留失权管理快照 | 未测 |
| E1 | 后端离线/恢复，列表错误可重试；审核/人员写入网络结果未知时只核对读取、不自动重放POST/PUT | 未测 |
| E2 | 退出及整页刷新仍遵循内存Token规则，旧管理内容不显示；没有密码、Token或完整请求头日志 | 未测 |
| E3 | 窄屏能访问表单/表格/确认框，ElDrawer/ElSwitch/ElInputNumber可见且可操作 | 未测 |

C9/D4等涉及临时数据停用或授权变化的测试应使用隔离夹具，不能修改正式seed。浏览器Network里只核对必要业务字段和状态码，截图避开凭据。网络未知场景和409应分别测试，不能以正常成功截图代替。

## 3. 自动化与证据边界

- 此前实施阶段已实际通过：MSVC Debug、22/22 CTest（Worker Service62个断言、Worker HTTP113次，含新增只读扩展检查）、前端认证9/9、报修/补充16/16、管理逻辑23/23、前端模块真实代理HTTP4/4（44次请求）、build和W2冒烟、SQLite完整性。**本轮文档补记未重跑这些测试。**
- 自动化、真实HTTP与用户人工反馈是三类不同记录。HTTP证明接口/DTO/事务链路；14/14浏览器通过来自用户反馈，不来自静态注册检查或Codex浏览器操作。
- 用户已提供人工通过结论，但未附本次截图或Network回执，不填造人工工单号、截图文件名、请求ID、具体页面时间或浏览器版本，也不套用自动化临时工单与请求ID。

## 4. 补充证据模板与收尾（非14/14结论的额外前置条件）

用户已确认14/14通过；以下保留供后续自愿补充实际环境和证据，不要求为文档补记重做验收，不将空白模板当成已填写事实。

```text
执行人：项目开发者本人
日期 / 浏览器：
实际独立数据库文件名：
工单ID / 编号（退回→补充→通过）：
REJECT工单ID / 编号：
容量单字段验收的维修员编号及前后值：
A1～E3：分别通过 / 失败 / 未测（不要把未测写通过）
截图目录及支持场景：
Console错误（如有）：
```

结束时在本次启动的两个终端Ctrl+C，确认8848/5173释放、恢复此前环境变量。暂时保留独立人工库用于核验，不加入Git。未验收截图不能提交私人数据。

## 5. 保留风险

- API-16无expected_version：重新打开读取只能降低旧快照风险，不保证并发不会最后写入覆盖。
- 内存Token刷新需重登；离线退出不保证服务端Session撤销；完整登录限流尚未完成。
- 全量人员定位以授权列表最多50页、每页100条为界，超过界限失败关闭而非扩大范围。
- 用户已确认真实浏览器14/14通过；跨机器首次部署、持续高压、断电/磁盘满/COMMIT I/O故障未验证，不因人工通过而自动标为已测。
- API-14获准只读DTO扩展的受众/字段/兼容性记录仍见验收报告第2节；不将它写成冻结契约原本已有字段。
- 大chunk警告保留；后续派单、取消、关闭、转派、上传、维修/评价未因此开放。W5-D4人工与此前自动化验收通过，等待最终源码及Git审查，不进入W5-D5。
