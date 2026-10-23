# W4-D4 人工浏览器验收（未通过，控件修复后待重新验收）

## 2026-10-23 人工阻塞与修复复验

- 用户本人反馈：reporter报修页分类、位置、紧急程度、日期时间只显示标签，普通输入框正常；因此本阶段人工验收**未通过**。这是用户提供的人工结果，不是Codex本轮浏览器观察；未提供本轮截图或Console日志。
- 已确认代码原因：main.js仅注册原7个组件，未注册ElSelect/ElOption/ElCascader/ElDatePicker，列表/详情所用表格、分页、描述及时间线等组件也漏注册。模板未用v-if/v-show隐藏这四类控件，CSS亦无display:none/visibility:hidden；禁用状态不应移除控件。
- 本轮沿用原按需导入与.use注册，补齐实际模板使用的13个组件（总20个），未重构注册方式，未改变表单/API/权限语义。
- 新增静态注册回归检查先失败（列出13个缺失组件），修复后通过；该检查不等于浏览器渲染/Console验收。认证8/8、报修7/7、生产构建、MSVC Debug与CTest15/15本轮均重新执行通过。
- 本轮按computer-use技能检查浏览器连接，返回apps=[]/browsers=[]；未继续原先受限的Windows浏览器路径。因此四控件的实际可见/打开/逐级选择、Console无Failed to resolve component及浏览器真实提交链路仍全部**待用户复验**。后端真实HTTP接口通过不能替代上述结论。
- 请重启Vite或整页刷新并重新登录（内存Token会清空），检查：分类根节点可选且category_id正确；位置逐级展开且location_id为真实ID、无停用项；NORMAL默认显示“普通”，URGENT触发应急提示；日期时间可选且本地时区转UTC；最后提交一张虚构工单并核查列表/详情/时间线。不要截图或分享Authorization、密码、Token。
- 修复前静态扫描和修复后构建均不证明没有其他浏览器缺陷；最终状态保持“待重新人工验收”，不得改为已通过。

## 证据边界

- 本轮实际完成 MSVC Debug 构建、CTest、前端认证/报修逻辑测试、Vite 生产构建、正式后端 HTTP 与 W2 冒烟检查，具体命令和结果见 `progress-log.md`。
- 真实 HTTP 不等同于浏览器端到端验证，Axios adapter 单元测试也不等同于真实 HTTP。
- 已尝试浏览器验收：浏览器连接清单为空；Windows computer-use 启动 Edge 后，捕获窗口时被安全机制停止（无法可靠确认浏览器 URL）。此后未继续浏览器操作。没有页面截图、视觉通过或浏览器登录证据。
- 以下全部是**待人工验证**，不是已通过清单。

## 准备独立验收数据库

在项目根目录 PowerShell 中执行；若同名数据库已经存在，先核对其用途，不要覆盖或删除旧库。

```powershell
.\backend\build\Debug\campusfix_backend.exe --migrate --db .\backend\build\w4d4-manual.sqlite3 --migrations-dir .\backend\migrations
.\backend\build\Debug\campusfix_backend.exe --seed --db .\backend\build\w4d4-manual.sqlite3 --seed-file .\backend\seeds\001_demo_data.sql
.\backend\build\Debug\campusfix_backend.exe --set-demo-password --db .\backend\build\w4d4-manual.sqlite3 --username demo_reporter01
.\backend\build\Debug\campusfix_backend.exe --enable-demo --db .\backend\build\w4d4-manual.sqlite3 --username demo_reporter01
.\backend\build\Debug\campusfix_backend.exe --set-demo-password --db .\backend\build\w4d4-manual.sqlite3 --username demo_worker01
.\backend\build\Debug\campusfix_backend.exe --enable-demo --db .\backend\build\w4d4-manual.sqlite3 --username demo_worker01
```

密码只在终端隐藏输入，不要提供到聊天、截图或 Git。既有密码已设置时不要尝试覆盖；可改用确认过的新验收库文件名。其他账户保持禁用；若需测试管理角色路由，可按已有受控命令单独设置并启用一个管理账户。

终端一：

```powershell
$env:CAMPUSFIX_DB_PATH = (Resolve-Path .\backend\build\w4d4-manual.sqlite3).Path
.\scripts\start-backend.ps1
```

终端二：

```powershell
.\scripts\start-frontend.ps1
```

访问 `http://127.0.0.1:5173/login`。测试后分别 Ctrl+C 停止服务；不要结束其他 Node 进程。验收数据库保留在被忽略的 build 目录，是否删除由用户确认。

## 待验收步骤

1. 未登录直接访问 `/report/new`、`/my-orders`、`/orders/1`，应跳转登录，不闪现工单内容。
2. reporter 登录后打开“提交报修”：分类和位置应从真实 API 加载。通过开发者工具 Network 核查 `/api/locations`、`/api/fault-categories`，不要截图 Authorization 请求头。
3. 位置逐级展开校区、楼栋、楼层和房间；选择框的圆形选择控件可选择节点，箭头用于展开。分类允许直接选择有效根分类，不要求必须有子分类。
4. 不填字段点击提交，应显示校验提示且不发送创建请求；检查标题120字、描述2000字、联系人80字、电话32字边界。期望时间可空，有值按本地时间选择并转 UTC。
5. 选择 URGENT，确认显示“先联系学校应急值班渠道并确保现场安全，本系统不替代应急处置。”；没有假的图片上传组件。
6. 填写虚构信息提交，Network 应为一次 POST 创建 DRAFT，再一次 POST submit；请求无 reporter_id、role、order_no、status。提交成功跳详情，显示真实编号、版本2、待审核状态。
7. 详情显示授权描述/联系方式、真实分类/位置、创建/提交时间；公开时间线有创建及提交事件。无编辑、取消、验收、评价、图片或假管理按钮，无内部备注或原始 JSON。
8. “我提交的报修”能看到刚创建工单；状态、分类筛选与分页请求固定 `view=reported`。空结果、加载和失败提示合理，点击详情可回到同一工单。
9. 整页刷新需要重新登录；重新登录后，列表及原详情 URL 中工单仍存在。
10. 退出 reporter、登录 worker，重复本人报修流程。“我提交的报修”与“待处理任务”入口分离；后者仍为后续占位，不混入本人报修列表。
11. 使用另一报修人不可见工单 ID 或不存在 ID，确认显示同一“资源不存在或无权访问”。管理角色直接进入本轮本人报修页面应被路由拒绝；后端授权始终为最终保障。
12. 停止后端，测试分类/位置重试、列表重试及详情失败提示；恢复后端后可重试，不能显示假成功。
13. 可使用浏览器请求阻止工具临时阻止 `*/api/work-orders/*/submit`：创建成功后应保留草稿 ID/version 并提示尚未正式提交。取消阻止后点击“重试提交同一草稿”，Network 不应再次 POST 创建。此项是网络失败恢复，不冒充409注入。
14. 连续点击提交不重复发起操作；离开含待提交草稿的页面应提示重试信息将丢失。创建响应丢失时会阻止盲目重建，需先核查本人列表；本阶段没有草稿编辑/恢复功能。
15. 真正409的页面展示仍待受控浏览器故障注入；本轮已有后端HTTP冲突回归及前端逻辑单测，但不能据此标记浏览器场景通过。
16. 确认浏览器页面/控制台/通知中没有密码或 Token。截图只保留虚构工单和页面布局，不包含密码、Authorization 或私人资料。

## 保留风险

- Token 只存内存，刷新后需重新登录；离线退出不能保证服务端 Session 已撤销。
- 草稿重试状态只在当前页面内存中；离开/刷新后不提供复杂草稿恢复。创建结果不确定时先查询本人报修列表，不自动重建。
- 生产构建/逻辑测试不能代替真实组件交互、键盘操作、窄屏布局与级联控件验收。
- 完整登录限流、跨机器部署、图片、补充编辑/取消/验收/评价及后续业务闭环尚未完成。
