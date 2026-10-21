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

## 2026-10-21 W2-D3：Vue3 前端工程初始化

- 开始前复核 `main` 与 `origin/main` 对齐，最近提交为 `fbc3ebe feat: add W2-D2 health check endpoint`。原有两份未跟踪 W1-D3 交接文件保留未改；`frontend/src` 的七个预留目录和 `.gitkeep` 均保留。
- 本机实测 Node.js `v24.18.1`、npm `11.16.0`；满足 Vue/Vite 官方文档的 Node 要求。npm 默认缓存目录因 `EPERM` 无法写入，改用已加入 `.gitignore` 的 `frontend/.npm-cache/`，未修改全局 npm 配置。
- 在现有 `frontend/` 以最小文件创建 Vue3 + Vite JavaScript 工程，加入单路由欢迎页、Element Plus 按需组件注册和未发请求的 Axios 客户端边界。没有 TypeScript、Pinia、登录/工单页面或健康接口联调。
- `npm install` 实际成功，新增 88 个安装包及 `package-lock.json`（lockfile v3）。顶层版本：Vue `3.5.44`、Vite `8.3.5`、Vue Router `4.6.4`、Element Plus `2.14.7`、Axios `1.20.0`、ECharts `6.1.0`、`@vitejs/plugin-vue` `6.0.10`。Vue Router 4 避免引入本阶段不需要的状态管理依赖。
- `npm run dev -- --port 5173 --strictPort` 实际启动 Vite，输出 `http://127.0.0.1:5173/`；HTTP 请求首页返回 200，包含 CampusFix 标题和应用挂载点，欢迎页 Vue 模块返回 200 且包含“前端工程已启动”。未进行浏览器视觉检查。停止测试进程后，5173 端口不再监听。
- `npm run build` 成功。首次全量注册 Element Plus 时出现单包大于 500 kB 的提示；改为仅注册欢迎页所用组件后再次构建成功，最终 JS 入口约 112.40 kB，无该警告。
- `node_modules/`、`dist/`、`.npm-cache/` 均按忽略规则不进入 Git；仅修改前端工程、根 `.gitignore` 和本进度记录，未修改后端或 W1 冻结设计。本阶段未 commit 或 push，W2-D4 联调未执行。

## 2026-10-21 W2-D4：Vue3 与 Drogon 健康检查联调

- 开始前核对 `main` 与 `origin/main` 对齐，最近提交为 `2b13157 build: initialize W2-D3 Vue frontend`；只有两份原有 W1-D3 交接文件未跟踪。按 W2-D4 范围仅修改前端和本进度记录，未改后端代码、数据库或 W1 冻结文档。
- Vite 开发服务器新增 `/api` 代理到 `http://127.0.0.1:8848`；Axios 保持相对 `baseURL=/api`，健康请求使用 `/health`，5 秒超时。健康请求验证统一响应字段，对网络错误、超时及非 2xx 给出失败信息；未加入 Token 鉴权。
- 欢迎页增加最小布局、四角色非交互式导航占位、加载中/在线/离线状态及重新检测按钮。在线时服务名与 `request_id` 均来自真实健康检查响应；未建立业务页面或权限功能。
- 实际运行 MSVC Debug 构建，`campusfix_backend.exe` 成功生成，无编译警告。启动 Drogon 和 Vite 后，直连 `127.0.0.1:8848/api/health` 与代理 `127.0.0.1:5173/api/health` 均返回 HTTP 200、`application/json` 和相同结构的后端 JSON；代理不是 `index.html`。
- 在真实浏览器打开 Vue 页面并目视检查。页面显示“后端在线”、`campusfix-backend` 和 `req_1035ff10-84a7-41c3-bd8a-fe6ba5034c34`；点击重新检测后 ID 变为 `req_341031ff-1997-4780-b51a-fbc65e6ffcf0`。
- 关闭 Drogon 并确认 8848 端口释放后，在同一页面点击重新检测，显示“后端离线或请求失败”和“后端请求失败（HTTP 502）”，按钮仍可用。重启 Drogon 后再点击，恢复“后端在线”，显示新的 `req_e0636ebb-4e22-4f42-ae1b-963a8e39f189`。由于本机请求很快，加载中状态由代码覆盖，但未捕获到浏览器可见画面。
- `npm run build` 成功，Vite `8.3.5` 构建无警告；最终 JS 入口约 190.75 kB。测试完成后按可执行路径及命令行核对进程并停止，确认 8848 和 5173 均无监听。本阶段未执行 W2-D5 启动脚本/完整冒烟测试，未 commit 或 push。
