# AGENTS.md — CampusFix AI 编程协作规范

> 适用于 AI 代码助手及人工协作者。阅读任务涉及文件和本文件后再修改项目。

## 1. 唯一项目事实来源

按优先级：
1. `docs/SRS.md` 第 44～56 章（V1.1 可实施约束）；
2. `docs/SRS.md` 第 1～43 章（原有业务需求）；
3. `docs/decisions.md` 中已接受的具体决策；
4. `docs/development-plan.md` 当前周任务；
5. 本文件约束。

若遇互相矛盾的要求，先指出文件路径/章节和冲突，不得私自改变项目路线。W1-D5 前的决策为暂定，不代表已正式冻结。

## 2. 技术栈不可擅改

- 后端：**C++17 + Drogon**，REST API + JSON，CMake；
- 前端：**Vue3 + Vite + Element Plus + Axios + ECharts**；
- 首版数据：SQLite；
- **不得擅自更换为 Qt 桌面应用、Python/Node 后端或纯前端模拟系统。**

## 3. 分层与代码约定（W2 起）

- `controllers`：路由、请求解析、基本输入检查、响应；
- `services`：业务规则、权限范围、事务与状态转换；
- `repositories`：SQL 和数据存取；
- `models`：领域数据结构；
- `dispatch`：调度候选过滤、纯评分、推荐理由；
- `middleware`：认证、请求身份和资源授权；
- 前端 API 封装在 `frontend/src/api/`，页面不直连数据库。

数据库表推荐使用复数蛇形，如 `users`、`work_orders`、`assignments`；API 统一 `/api/...`。变更字段/接口/状态码前必须先说明对其他模块和测试的影响。

## 4. 业务与安全硬约束

- 角色代码：`reporter`、`worker`、`dispatcher`、`sysadmin`。
- 工单状态码以 SRS 第 46 章为准：`DRAFT`、`PENDING_REVIEW`、`PENDING_SUPPLEMENT`、`PENDING_DISPATCH`、`PENDING_ACCEPT`、`IN_PROGRESS`、`PENDING_VERIFY`、`COMPLETED`、`CLOSED`、`CANCELLED`。
- 每个受保护 API **后端鉴权且验证数据所属/授权范围**，不能信任前端提供的 `reporter_id`、`operator_id` 或目标状态。
- 调度必须先满足资格、技能、在岗、服务区域和容量硬约束；管理员确认派单；P0 不允许绕过技能/容量派单。
- 推荐只是纯计算，写入派单历史、修改状态、记录日志必须在事务中处理，防止重复派单或超载。
- 不记录明文密码、完整 Token、个人敏感信息、真实学生数据，不提交真实附件或密钥。
- P0 不实现自动验收、短信微信推送和不必要的大模型功能。

## 5. AI 每次开发任务的固定工作流

1. 阅读本文件、对应 SRS 需求编号、当前周任务及涉及的代码。
2. **先简述实现计划**：目标需求编号、可能更改的文件、依赖与风险。
3. 一次只做单个可验收任务；尽量小改动，禁止无关大规模重构。
4. 修改后提供编译/运行/测试命令；未运行的检查必须标为“未验证”，不得声称测试通过。
5. 涉及数据库迁移或破坏性变更必须显式说明，并要求人工确认。
6. 给出修改文件清单、实际验证证据、未解决问题和下一步建议。
7. 出现编译失败，先定位并修复当前任务，不要盲目重写整个项目。

## 6. 文档与决策变更

- 不允许默默修改需求优先级、接口契约、表名或工单状态。先提出影响分析；获准后更新 `docs/decisions.md` 和相应文档。
- W1-D5 才正式冻结 P0；冻结后新增功能优先进入 P1/P2。
- 保持中文业务说明、英文稳定代码标识，统一使用 UTF-8。

## 7. W1-D1 今日特殊限制

**只允许**：建立目录、维护 `README.md`、`AGENTS.md`、`docs/SRS.md`、`docs/development-plan.md`、`docs/decisions.md`、`docs/flows.md` 与 Git 初始化。

**禁止**：开发登录页面、连接真实数据库、生成 Controller/Service 业务代码、编写工单调度算法、批量安装依赖、声称前后端已联调。Drogon/Vue 工程初始化与健康检查留到 W2。

## 8. 给 AI 的任务输出格式

- 本次目标：
- 对应需求/决策：
- 计划更改文件：
- 实际执行步骤：
- 验证方法与实际结果：
- 已知问题：
- 下一步：
