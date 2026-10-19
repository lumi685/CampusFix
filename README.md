# CampusFix —— 校园维修工单与智能调度系统

> 基于 C++ / Drogon 与 Vue3 的前后端分离课程项目。当前处于 **W1-D1：项目仓库与需求基线初始化**，尚未实现业务功能，也暂不可运行 Web 服务。

## 1. 项目背景与目标

校园宿舍、教学楼和公共设施的报修通常面临记录分散、调度依赖人工、状态不可追踪的问题。CampusFix 将实现从在线报修、审核、智能调度、人工确认，到维修处理、验收评价和归档统计的闭环。

**目标**：单人结合 AI 辅助，在约 10 周完成可演示、可测试、可复现启动的 Web 课程设计。时间计划是估计值而非保证。

## 2. 技术方案（W1 暂定，W1-D5 冻结）

| 组件 | 选型 | 备注 |
|---|---|---|
| 后端 | C++17 + Drogon | REST API、JSON、业务服务与调度算法 |
| 前端 | Vue 3 + Vite | Element Plus、Axios、ECharts |
| 数据库 | SQLite | 课程阶段优先；后续可迁移 MySQL/PostgreSQL |
| 构建 | CMake / npm | 后端与前端分别管理依赖 |
| 版本管理 | Git | 每次提交保持文档或代码状态可核查 |

## 3. 角色与业务主线

角色：`reporter` 报修人、`worker` 维修人员、`dispatcher` 调度管理员、`sysadmin` 系统管理员。

主线：`提交报修 → 审核 → 推荐候选人 → 管理员确认派单 → 维修人员接单处理 → 报修人验收 → 评价 → 关闭归档`。

后端拥有角色鉴权、数据范围检查和状态变化的最终权限；前端不直接写数据库。

## 4. 本期范围

- **P0 必须做**：四角色访问控制、报修与审核、技能和区域约束、候选推荐与人工派单、维修处理与验收、评价、基础附件与统计、操作历史、数据库与安全测试。
- **P1 有余力做**：站内通知、排班、超时提醒、策略切换、可配置权重、Docker、报表导出。
- **P2 本期不做**：大模型图片识别、全局路径规划、学校统一身份认证、短信与微信对接、自动验收。

P0 范围依据 `docs/SRS.md` 的第 45 章；新增需求需记录对数据库/API/测试的影响。

## 5. 目录结构

```text
CampusFix/
├── README.md                     # 项目入口
├── AGENTS.md                     # AI 协作规范
├── .gitignore
├── docs/
│   ├── SRS.md                    # V1.1 需求基线（当前待 W1-D5 冻结）
│   ├── development-plan.md       # 10 周计划
│   ├── decisions.md              # 架构决策记录
│   ├── flows.md                  # W1-D1 业务流程草案
│   └── W1-D1-guide.md            # Windows 初始化、验证与 Git 操作
├── backend/                      # W2 建立 Drogon/CMake 工程
│   ├── config/ migrations/ seeds/ include/
│   ├── src/                     # controllers/services/repositories/models/dispatch/...
│   └── tests/                   # unit/integration
├── frontend/                     # W2 用 create-vue/Vite 生成工程
│   └── src/                     # api/router/stores/layouts/views/components/utils
├── scripts/                      # 未来脚本
└── uploads/                      # 运行附件目录（不提交真实附件）
```

空目录使用 `.gitkeep` 占位。**当前的 backend/frontend 只是规划目录，不能执行构建命令。** 后续用官方脚手架时，注意前端预建目录已有占位文件：建议在 W2 从临时目录生成 Vue 脚手架，再合并到 frontend；不要直接覆盖现有内容。

## 6. 本地操作（Windows PowerShell）

1. 解压本骨架包，将 `CampusFix` 目录放到工作区（建议 `$HOME\Projects`）。
2. 在 VS Code 中选择 **文件 → 打开文件夹**，打开 `CampusFix`。
3. 在终端确认 `Get-Location` 位于项目根目录，然后依次执行：

```powershell
git --version
git init -b main
git status --short
Test-Path .\docs\SRS.md
Test-Path .\AGENTS.md
Test-Path .\docs\decisions.md
```

4. 先阅读 `docs/decisions.md`，再执行首次提交：

```powershell
git add .
git status --short
git commit -m "docs: initialize CampusFix requirements baseline"
git log -1 --oneline
git status
```

如首次提交提示缺少 Git 作者身份，请先运行 `git config --global user.name "你的显示名"` 和 `git config --global user.email "你的邮箱"`，再重新提交。**不要把私人密钥或真实学生数据提交到 Git**。

## 7. 本日验收

- [ ] 能打开 `CampusFix` 项目根目录
- [ ] `docs/SRS.md` 存在且包含 V1.1 第 44～56 章
- [ ] `README.md`、`AGENTS.md`、`docs/decisions.md`、`docs/flows.md` 存在
- [ ] `git log -1 --oneline` 出现首次提交
- [ ] `git status` 显示工作区干净
- [ ] 明确 W1-D2 才开始把 FR 转成逐条验收任务；W2 才运行真正的 Drogon/Vue 项目

## 8. 文档入口

- [需求基线](docs/SRS.md)
- [10 周开发计划](docs/development-plan.md)
- [决策日志](docs/decisions.md)
- [流程草案](docs/flows.md)
- [当天操作指导](docs/W1-D1-guide.md)

## 9. 参考资料

- Drogon 官方仓库：https://github.com/drogonframework/drogon
- Vue 官方快速开始：https://vuejs.org/guide/quick-start.html
- Git 初始化文档：https://git-scm.com/docs/git-init.html
