# CampusFix 进度与交接记录

> 本记录根据 2026-10-21 用户补充的丢失对话、仓库文件和本地 Git 状态整理。历史对话中的检查结果按“当时记录”标注；当前状态以再次核对的仓库为准。本文件只记录进度，不修改已冻结需求。

## W1 设计阶段

| 阶段 | 结果 | 仓库证据 |
|---|---|---|
| W1-D1 | 项目仓库、需求基线和开发规范初始化 | `cb37371 docs: initialize CampusFix requirements baseline` |
| W1-D2 | FR-001～FR-015 需求追踪矩阵完成 | `393e08b docs: add W1-D2 requirements trace matrix` |
| W1-D3 | 工单状态机与四角色权限矩阵完成 | `1c9c5e9 docs: finalize W1-D3 workflow and role permissions` |
| W1-D4 | 数据库、REST API、UI 原型文档完成 | `5deb925 docs: add W1-D4 database API and UI designs` |
| W1-D5 | 设计评审、冲突修复、P0 文档基线冻结 | `30ce1b5 docs: freeze W1 P0 design baseline` |

### W1-D5 第一阶段评审

只读审查了 `AGENTS.md`、SRS、开发计划、需求追踪、决策日志、流程、权限、数据库、API 和 UI 原型。发现并报告六项主要问题：测试编号语义冲突；到场/维修进度缺少 API/UI；非 Top 3 合格人员缺少候选数据；历史效率与评价评分来源未固定；管理员主动改派范围不明确；SRS 与 API 契约复用不同接口编号。评审后等待用户决定，没有在第一阶段修改文档。

### W1-D5 第二阶段同步与冻结

用户确认评审建议并补充约束后，更新了 `docs/SRS.md`、`docs/requirements-trace.md`、`docs/decisions.md`、`docs/flows.md`、`docs/roles.md`、`docs/database-design.md`、`docs/api-contract.md`、`docs/ui-wireframes.md`、`docs/development-plan.md`，新增 `docs/architecture.md`。原有决策记录保留，以新增记录说明冻结结果。

冻结的 P0 口径：13 项 P0 FR、10 个工单状态、T-01～T-23、E-01～E-02、16 张表、API-01～API-33。新增轻量到场/进度事件 API-33；推荐返回 Top 3 和其他合格候选；历史评分公式、样本阈值与测试固定；管理员无申请主动改派列 P1；报修图和维修结果图各最多 3 张、每张不超过 5 MiB；推荐预览不改业务数据，确认派单时保存最终快照；验收通过释放负载，失效转派申请标记 `SUPERSEDED`；sysadmin 业务权限须显式授权。测试编号以需求追踪矩阵为准。

当时的文档检查记录为：FR 15 个、状态转换 23 个、过程事件 2 个、API 33 个、16 张表、W1-D3 场景 26 个；`git diff --check` 通过。该检查是文档一致性检查，未运行业务代码测试。

## W2-D1：Windows C++ 与 Drogon 最小工程

### 用户要求

在 Windows 上使用 MSVC + CMake + vcpkg 建立 C++17 Drogon 最小后端工程，实际完成配置、编译、启动和运行检查。未获确认前不得安装缺失依赖；不提前实现 W2-D2 的 `/api/health`，也不修改 W1 冻结需求、自动 commit 或 push。

### 丢失对话中的环境审计（当时记录）

| 项目 | 当时记录 |
|---|---|
| Windows | Windows NT 10.0，Build 26200，x64，25H2 |
| PowerShell | 5.1.26100.9168 |
| Visual Studio Community | 2026，18.9.2 |
| MSVC | 19.51.36256，x64 |
| CMake | 4.3.1-msvc1 |
| Ninja | 1.13.2 |
| vcpkg | 2026-05-27 |
| Drogon | 1.9.13#2，记录为已存在于其他 vcpkg 安装树 |
| SQLite CLI | 3.51.0 |
| SQLite 开发包 | 当时未找到可用于此项目链接的 `sqlite3.h`、`sqlite3.lib`、`sqlite3.dll` |

助手当时建议采用 `backend/vcpkg.json` 声明 `drogon`、`sqlite3`，并使用 Visual Studio 2026 的 CMake 与 vcpkg 工具链配置工程；该回复明确在用户确认前暂停安装。

### 2026-10-21 仓库现状复核

- `main` 与 `origin/main` 显示对齐，最新提交为 W1-D5 冻结提交 `30ce1b5`。
- 工作区已有未提交的 `.gitignore` 修改，以及未跟踪的 `backend/CMakeLists.txt`、`backend/src/main.cpp`、`backend/vcpkg.json`。这些文件表明 W2-D1 最小工程已开始建立；本记录不推断其创建过程或安装授权。
- `CMakeLists.txt` 设置 C++17，查找并链接 Drogon 与 SQLite；`main.cpp` 在 `127.0.0.1` 监听默认端口 8848，允许用 `CAMPUSFIX_PORT` 设置端口；尚无 `/api/health`。
- `backend/build/vcpkg-manifest-install.log` 的当前内容是从 GitHub 获取 registry 信息后报 `drogon does not exist` 和 `sqlite3 does not exist`。未见 `backend/build/CMakeCache.txt` 或 `backend/build/Debug/campusfix_backend.exe`。因此尚无配置成功、编译成功或启动成功的证据。
- 两份原有未跟踪交接文件 `docs/AI-HANDOFF-W1-D3.md`、`docs/W1-D3-CODEX-PROMPT.md` 仍存在；本次未修改。

**2026-10-21 早前判断**：W1-D5 已冻结并提交；当时 W2-D1 尚未通过配置、编译、启动和运行检查。此判断保留为历史记录，后续验收见下一节。

### 2026-10-21 W2-D1 最终验收补记

- 用户在 Windows 本机 PowerShell 完成 CMake 配置和 vcpkg 安装。仓库中实际检查到 `backend/build/CMakeCache.txt`：Visual Studio 18 2026、x64、`x64-windows`、Visual Studio 自带 vcpkg toolchain；安装状态文件记录 Drogon `1.9.13#2`、SQLite `3.53.4#2`，安装日志以 `All requested installations completed successfully` 结束。
- 助手直接使用现有构建目录运行 `cmake --build backend/build --config Debug --target campusfix_backend --parallel 4`。受限沙箱中 MSBuild `FileTracker` 曾返回 `E_ACCESSDENIED`；在 Windows 本机执行环境重跑同一命令成功。MSVC 仅报告 `main.cpp` 中 `getenv` 的 C4996 警告。
- 实际生成 `backend/build/Debug/campusfix_backend.exe`。运行日志显示 `CampusFix backend starting on http://127.0.0.1:8848`；`netstat` 和进程路径核对确认 `127.0.0.1:8848` 的监听 PID 为本次生成的程序。随后停止该 PID，再次检查确认进程与 LISTEN 均不存在。
- `git diff --check` 通过。`git check-ignore -v` 确认 exe、CMakeCache 和 vcpkg 安装状态均由 `backend/build/` 规则忽略。本阶段没有实现 `/api/health`，也没有提交或推送。
- W2-D1 的最小工程编译和启动验收已完成；C4996 警告可在后续小范围清理，不影响本次运行结果。下一任务才进入 W2-D2。

## 2026-10-21 W2-D2：健康检查 API

- 新增无需认证的 `GET /api/health` 工程基础设施接口，不占用冻结的 API-01～API-33 编号，也不访问 SQLite。成功响应遵循统一结构：`code=0`、`message=success`、`data.status=ok`、`data.service=campusfix-backend`，`request_id` 由服务端逐请求生成。
- 新增最小 `HealthController` 与公共 JSON 响应构造工具，注册独立 GET 路由；请求成功时记录方法、路径、状态和请求 ID，异常路径返回不泄露内部信息的 JSON 500。仅健康检查路由应用该响应工具，其他业务接口未实现。
- 复用 `backend/build` 的 CMake 配置与已安装依赖。修改 CMake 源文件后自动重新配置成功，vcpkg 报所有依赖已安装；MSVC Debug 编译成功，生成 `backend/build/Debug/campusfix_backend.exe`。受限沙箱中 MSBuild FileTracker 报 `E_ACCESSDENIED`，在获准的 Windows 本机环境重跑成功。
- 对 `main.cpp` 的环境变量读取使用 MSVC `_dupenv_s`，保留其他平台的 `getenv` 路径；最终 Debug 构建无 C4996 警告。
- 实际启动日志：`CampusFix backend starting on http://127.0.0.1:8848`。`curl.exe -i` 返回 HTTP 200、`content-type: application/json; charset=utf-8` 和预期 JSON；PowerShell 解析验证所有要求字段，两次请求 ID 不同。无认证头请求可用。
- 不存在的 `/api/not-a-real-route` 实测返回 HTTP 404 和 Drogon 默认页面，未误返回健康检查内容。`127.0.0.1:8848` 监听 PID 与本次生成的程序路径一致；测试后停止进程并确认端口释放。
- `git diff --check` 通过；可执行文件、CMake 缓存和测试日志均由 `backend/build/` 忽略。未测试故意触发的异常响应分支，未进行登录、数据库或其他业务测试。本阶段未 commit 或 push。
