# W5-D2 浏览器人工验收清单

记录日期：2026-10-24。执行主体：项目开发者本人。状态：**全部既定浏览器人工验收项目由用户确认通过，等待最终 Git 与源码审查**。

实施阶段没有可用的受控浏览器工具，声明的浏览器技能文件也无法读取，Codex 没有操作真实浏览器或生成截图。此后用户已亲自在真实浏览器执行全部既定检查，确认未发现功能错误，包含正常补充、字段回填、权限、409冲突与失败恢复。以下分别记录用户反馈、用户所述截图可见内容和此前自动化结果，不将三者混同。

## 1. 独立测试环境准备说明（保留原指导，不代表本轮重新执行）

以下路径和命令是此前提供的准备示例；用户本次未补充实际数据库文件名、浏览器版本、工单ID或编号，不能据此倒推实际使用值，也不要求为了补记重新初始化数据库。

不要复用或覆盖原人工验收库。先检查新路径不存在；如果已存在，请选择另一个文件名，不要删除已有库。

在项目根目录：

```powershell
Test-Path .\backend\build\w5d2-manual.sqlite3
# 只有 False 时才按以下步骤创建新测试库：
.\backend\build\Debug\campusfix_backend.exe --migrate --db .\backend\build\w5d2-manual.sqlite3 --migrations-dir .\backend\migrations
.\backend\build\Debug\campusfix_backend.exe --seed --db .\backend\build\w5d2-manual.sqlite3 --seed-file .\backend\seeds\001_demo_data.sql
```

对 demo_reporter01、demo_worker01、demo_dispatcher 分别执行已有受控 CLI，密码由本人在终端隐藏输入，不发到聊天或写入 Git；只启用这三个指定账号，不批量启用其他账号：

```powershell
.\backend\build\Debug\campusfix_backend.exe --set-demo-password --db .\backend\build\w5d2-manual.sqlite3 --username demo_reporter01
.\backend\build\Debug\campusfix_backend.exe --enable-demo --db .\backend\build\w5d2-manual.sqlite3 --username demo_reporter01
# 对 demo_worker01、demo_dispatcher 逐个替换 username，重复上述两个命令。
```

终端一设置 CAMPUSFIX_DB_PATH 为这个新测试库，运行后端；终端二运行前端。端口占用时先确认归属，不结束不明进程：

```powershell
# 终端一；如已有变量，请先记录旧值，验收后恢复。
$env:CAMPUSFIX_DB_PATH = (Resolve-Path .\backend\build\w5d2-manual.sqlite3).Path
$env:CAMPUSFIX_PORT = '8848'
.\backend\build\Debug\campusfix_backend.exe
# 终端二
Set-Location frontend
npm.cmd run dev -- --port 5173 --strictPort
```

后台管理页面尚未开发。RETURN 需通过既有 API-13 操作：可以用本机受控 HTTP 客户端登录 demo_dispatcher，取得会话后调用下列请求。密码仅交互输入，Token 仅本机内存使用；不要拍摄登录响应/Authorization 或导出含凭据的 HAR。

```http
POST /api/work-orders/<原工单ID>/review
Authorization: Bearer <仅本机使用，不提供截图或聊天>
Content-Type: application/json

{"action":"RETURN","expected_version":<真实当前版本>,"reason":"请补充虚构故障现象","internal_note":"仅用于验证隔离的虚构内部备注"}
```

## 2. 浏览器人工结果（A～M 全部由用户确认通过）

通过结论的执行主体均为项目开发者，不是 Codex 自动化浏览器。时间线截图只直接支持第3节列出的可见内容，不能作为其余所有交互都存在截图的证据。

| 项目 | 操作与预期 | 结果 |
| --- | --- | --- |
| A 原报修人与退回原因 | reporter 新建并提交；记录 ID/编号；授权管理员 RETURN 后打开本人详情，明显显示待补充、公开原因和时间，没有内部备注 | 用户确认通过 |
| B 原选项回填 | 点击编辑，分类和位置显示真实文字及完整校区/建筑/楼层/房间路径，展开可选其他有效节点；无子类的根分类也可选择 | 用户确认通过 |
| C 全部表单 | 检查标题、描述、紧急度、联系人、电话、可选日期；本地显示时间与原 UTC 对应；URGENT 提示正常 | 用户确认通过 |
| D 仅保存 | 修改描述后保存：PUT 原 ID，状态仍待补充，版本 +1；详情与时间线更新，摘要不含描述/电话号码值 | 用户确认通过 |
| E 重新提交 | 点击仅提交已保存内容并确认：POST 同一 ID/submit，使用保存后的版本，回到待审核；编号和首次提交时间不变 | 用户确认通过 |
| F worker 本人 | worker 自建报修、退回后重复 A～E；维修任务与本人报修分离，他人派给自己的工单不出现编辑入口 | 用户确认通过 |
| G 多轮和分页 | 同一单再次 RETURN，显示本轮最新原因；多次保存使时间线超过一页后刷新重登，不能退回显示旧一轮原因 | 用户确认通过 |
| H 状态/角色 | 待审核、待派单、终态无编辑；dispatcher/sysadmin 不冒充原报修人；匿名/其他报修人由后端拒绝 | 用户确认通过 |
| I 冲突 | 两个页面打开同一版本，先保存一个，再保存另一个：409提示刷新，不覆盖或新建 | 用户确认通过 |
| J 保存后提交失败 | 保存后离线提交：提示保存完成但提交未确认；恢复后只重试原 ID 的 submit，不再次 PUT/POST 创建 | 用户确认通过 |
| K 响应丢失 | PUT 发出后结果不确定：禁止盲目重放，刷新并核对服务器数据、确认后继续；submit 响应丢失但已成功时识别待审核 | 用户确认通过 |
| L 刷新与保护 | 整页刷新按内存 Token 策略重新登录，再读取原工单；保存后的待补充单可直接提交已保存内容，无需重复 PUT | 用户确认通过 |
| M 校验与控件 | 非法表单、空数据、目录/时间线失败可理解且可重试；Console 无 Failed to resolve component；没有未实现功能的假按钮 | 用户确认通过 |

原 Network 核对要求为：create201 → first submit200/version2 → RETURN200/version3 → PUT200/version4 → resubmit200/version5 → detail/timeline200。用户确认既定项目通过，但本次未提供可独立引用的 Network 响应截图、人工请求ID或版本截图；不将既有自动化临时库回执套用到本次人工工单。

建议截图分别包含：公开退回原因、完整回填表单、PUT响应、resubmit响应、同ID Network总览、最终详情与时间线、worker本人流程。可存到被忽略的 `backend/build/Testing/w5d2-screenshots/`，截图不要包含密码、Token、Authorization、私人资料；不要用截图文件名代替证据核查。

## 3. 用户所述时间线截图直接证据

按用户本次明确提供的真实截图内容记录：同一工单存在以下五条公开事件。时间按页面显示原样保存，不臆造UTC、时区或毫秒。

| 顺序 | 公开事件 | 页面显示时间 |
| --- | --- | --- |
| 1 | 创建草稿 | 2026-10-24 14:14:41 |
| 2 | 首次提交 | 2026-10-24 14:14:41 |
| 3 | 管理员退回 | 2026-10-24 14:18:54 |
| 4 | 补充修改报修信息 | 2026-10-24 14:21:52 |
| 5 | 再次提交审核 | 2026-10-24 14:25:23 |

- 公开退回原因：“请补充插座故障的具体情况和检测结果”。
- 内容修改摘要：`Updated fields: description`，仅描述字段发生修改；没有在摘要中复制描述全文。
- 用户确认所述公开时间线没有内部审核备注标记。该结论限于所述可见区域，不扩展为已审阅隐藏响应头、完整HAR或未提供的截图。
- 本轮输入为用户对截图可见内容的明确说明，未附可由Codex独立打开的图片路径/文件；因此不宣称Codex本轮逐张读图，不虚构文件名、截图数量、工单ID、编号、Network回执或request_id。

## 4. 自动化历史、待补信息与非阻塞事项

- 之前自动化实测保持：CTest20/20、编辑Service60个断言、正式HTTP74次、前端认证9/9和报修/补充16/16、构建与W2冒烟通过；本次只补文档，没有重新执行这些测试。
- 首次测试夹具读游标未释放导致database locked、修复作用域后通过的历史完整保留在报告与进度日志。
- 执行人：项目开发者本人；截图事件日期为2026-10-24，浏览器具体版本、实际人工数据库、Reporter/Worker工单ID和编号未补充，不猜测。上述归档信息缺失不推翻用户已明确确认的人工通过结论。
- 英文摘要`Updated fields: description`记为**非阻塞UI文案优化项**，本阶段不修改业务事件或显示逻辑。
- Codex未操作浏览器、未生成截图、未重跑自动化；用户人工结果不冒充Codex自动化E2E。
- 物理断电、磁盘满、COMMIT I/O故障、高压、跨机器部署，以及内存Token/离线退出/完整限流等既有风险继续保留，不因人工通过而自动标记已解决。

最终结论：**W5-D2既有自动化验收通过，全部既定浏览器人工验收由用户确认通过；等待最终Git与源码审查，不进入W5-D3。**

测试结束用 Ctrl+C 停止自建前后端进程，恢复终端环境变量；保留测试库供核验，未获确认前不要删除用户文件或使用 git clean。
