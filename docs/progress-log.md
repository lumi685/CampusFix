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

## 2026-10-21 W2-D5：工程启动与冒烟测试

- 开始前核对 `main` 与 `origin/main` 对齐，最近提交为 `69d46b4 feat: connect W2-D4 Vue frontend to Drogon health API`；工作区只有两份原有 W1-D3 未跟踪交接文件。`scripts/` 原先仅有 `.gitkeep`。
- 新增 `scripts/start-backend.ps1`、`scripts/start-frontend.ps1`、`scripts/smoke.ps1`。启动脚本按 `$PSScriptRoot` 定位仓库、前台运行并检查端口；后端优先使用显式 CMake 路径或现有 CMake 缓存，首次配置通过参数或 `VCPKG_ROOT` 定位 vcpkg，不写死本机 Visual Studio 安装路径。端口占用时分别实测返回清晰错误，不批量结束进程。
- README 保留原项目背景、四角色业务主线和 P0/P1/P2 范围，更新 Windows 实测版本、首次依赖准备、双终端启动、构建/健康检查/冒烟命令、停止方法、Git 忽略规则及常见故障；明确其他机器尚未验证。
- 三份脚本 PowerShell 解析错误均为 0。复用现有 CMake/vcpkg 缓存，经后端脚本实际完成 MSVC Debug 编译并启动 Drogon，无编译警告；经前端脚本启动 Vite。`npm run build` 实际成功，无构建警告。
- 首次 smoke 实测暴露 PowerShell 7 将 JSON 整数 `code` 解析为 `Int64`，脚本只接受 `Int32`；修正后完整测试通过：直连和代理各两次均为 HTTP 200、JSON、`code=0`、`status=ok`、`service=campusfix-backend`，四个 `request_id` 非空且同一路径重复请求不同；前端首页 HTTP 200 且含 CampusFix 标题和 Vue 挂载点。成功退出码 0。
- 故障注入：核对进程路径后停止 Drogon，确认 8848 释放；smoke 返回连接拒绝、退出码 1。再次用后端脚本启动并运行 smoke，所有断言通过、退出码 0。测试结束按进程路径/命令行核对后仅停止本次两个服务，确认 8848 与 5173 均无监听。
- 另外使用本机 Windows PowerShell `5.1.26100.9168` 对三份脚本解析检查均为 0 错误，并在该环境下再次通过两个启动脚本运行服务及完整 smoke，退出码 0；随后再次确认两个端口释放。
- 本阶段仅做工程脚本、README 和进度记录；未连接 SQLite、未开发 W3 业务、未修改 W1 冻结设计、未 commit 或 push。新机器从空缓存进行首次依赖下载和配置尚未实际验证；本次验证复用了已有安装与构建缓存。

## 2026-10-23 W3-D1：SQLite Schema 与最小迁移

- 以 W1-D5 冻结的 `database-design.md` 为依据，新增 `backend/migrations/001_initial_schema.sql`：16 张 P0 业务表、主键/外键（历史引用显式 `ON DELETE RESTRICT`，会话到用户为 `CASCADE`）、枚举/范围 CHECK、唯一约束、33 个显式索引（其中 4 个为关键部分唯一索引）。额外的 `schema_migrations` 仅记录迁移版本，不计入 16 张业务表。
- 新增显式维护命令 `campusfix_backend --migrate --db <sqlite-file> --migrations-dir <directory>`。迁移器在连接建立后、事务开始前设置并核验 `foreign_keys=ON`，设置 5000 ms `busy_timeout`，要求 SQLite ≥3.35；整个待执行迁移集合在 `BEGIN IMMEDIATE` 事务内运行，版本记录与 DDL 同事务提交，重复运行跳过已执行版本。正常 HTTP 启动不打开数据库，健康检查仍独立。未启用 WAL。
- 复用现有 MSVC/CMake/vcpkg 配置，CMake 重新配置成功，vcpkg 报 Drogon `1.9.13#2` 与 SQLite `3.53.4#2` 均已安装，无新下载。第一次受限沙箱 MSBuild FileTracker 报 `E_ACCESSDENIED`；获准在 Windows 本机执行同一 Debug 构建后成功。最终重新编译 `campusfix_database`、`campusfix_schema_test`、`campusfix_backend` 均成功，未见编译警告。
- 独立测试程序以临时虚构数据实测：从空库初始化、16 张业务表及 33 个显式索引存在、外键删除策略、非法外键/重复用户名与单号/角色与状态等 CHECK、同一工单最多一条有效派单、待处理转派与到场事件唯一性、评价唯一性及评分范围。旧派单结束后允许新有效派单；两个草稿可同时拥有 `NULL` 单号。`PRAGMA integrity_check` 返回 `ok`，`PRAGMA foreign_key_check` 返回 0 行。重复迁移输出 `SKIP`，原有两条测试工单仍在。包含一个有效 001 与故障 002 的失败迁移实验确认所有 DDL 和版本元数据均回滚；临时测试目录成功后删除。
- 另以实际后端可执行文件对忽略的临时 SQLite 文件运行迁移命令，首次输出 `APPLY 001_initial_schema.sql`，再次输出 `SKIP 001_initial_schema.sql`；SQLite CLI 查询到 17 张非系统表（含版本表）、33 个显式索引，`integrity_check=ok`、`foreign_key_check` 无输出。
- 启动当前 Drogon 与 Vite 后，运行 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke.ps1`：后端直连两次、Vite 代理两次均 HTTP 200 且 JSON 字段正确，四次 `request_id` 非空且重复请求不同；前端首页检查通过，退出码 0。直接从当前默认 PowerShell 调用 `.ps1` 曾受执行策略阻止，使用进程级 `-ExecutionPolicy Bypass` 后通过，未更改系统策略。测试后停止本次两个进程，8848/5173 不再监听。
- 构建产物、vcpkg 安装目录、临时 `.sqlite3` 数据库、前端 node_modules/dist 均被 `.gitignore` 覆盖。仅修改后端最小迁移基础设施、测试、CMake 与本日志；未开发认证/工单 API、未修改冻结设计、未 commit 或 push。
- 尚未验证：跨机器首次配置与更高并发的业务事务；跨表身份/状态一致性和附件每用途最多 3 张属于后续 Service 短事务校验，不能仅由当前单表 CHECK/索引保证。W3-D2 种子数据与连接管理尚未开始。

## 2026-10-23 W3-D2：SQLite 访问基础设施与虚拟演示数据

- 开始时 `main` 与 `origin/main` 对齐，最新提交 `74c7c36 feat(db): add W3-D1 SQLite schema and migrations`；原有两份 W1-D3 未跟踪交接文件未改动。未修改 W1 冻结设计和 001 迁移。
- 新增 `SqliteConnection`、`SqliteStatement`、`SqliteTransaction`：每个连接启用并核验 `foreign_keys=ON`，设置 5000 ms `busy_timeout`；支持参数绑定、错误抛出、RAII 释放与短写事务自动回滚。W3-D1 迁移器复用该连接封装；正常 HTTP 启动和 `/api/health` 仍不打开数据库。未建立 ORM、连接池或业务 Repository。
- 新增显式维护命令 `--seed --db <sqlite-file> --seed-file <sql-file>`。仅在 001 迁移存在时打开已有数据库，并在 `BEGIN IMMEDIATE` 中执行可信项目内 seed SQL；不随服务启动自动 seed。稳定编码与 `ON CONFLICT DO NOTHING`/`NOT EXISTS` 保证重跑不重复、不覆盖已有普通数据；失败自动回滚，不使用 `INSERT OR REPLACE`。
- 虚拟数据：11 个 `DISABLED` 账户（sysadmin 1、dispatcher 1、reporter 3、worker 6），14 个位置（2 校区、4 建筑、4 楼层、4 房间），3 种技能、3 个故障分类、6 份维修员档案、7 条人员技能、4 条显式管理授权、4 张合法已提交工单。工单各含创建草稿及提交两条时间线，合计 8 条。所有账户密码字段均为不可登录哨兵 `!UNSET_DEMO_CREDENTIAL!`；W3-D3 应先选成熟哈希算法并安全设置演示凭据、显式启用账户，不能将哨兵当作有效密码。
- 在复用本机 vcpkg/CMake 缓存后，MSVC Debug 构建成功，`campusfix_database`、`campusfix_backend`、`campusfix_schema_test`、`campusfix_demo_seed_test` 均生成，未见编译警告。曾在受限沙箱遇到 MSBuild FileTracker `E_ACCESSDENIED`；获准本机执行后首次构建暴露一个 C++ 初始化语法错误，最小修复后重编译通过。
- 对 `backend/build` 下忽略的临时库，实际执行 `--migrate` 和 `--seed`，均退出 0；查询确认四角色数量、位置/技能/授权/工单数量，`PRAGMA integrity_check=ok`、`PRAGMA foreign_key_check` 无违规。再次运行 seed 退出 0，账号、位置、技能、工单数量不增长。
- `ctest --test-dir backend/build -C Debug --output-on-failure` 实际退出 0，2/2 通过（schema_migration、demo_seed）。新增自动化测试在全新临时库检查外键、角色/区域授权、时间线、完整性、参数绑定、事务析构回滚；重复 seed 保留人工修改的姓名和类别时长；注入一条有效写入后再触发无效外键，验证整批回滚；seed 在迁移前被拒绝且不创建库。测试仅使用虚构数据。
- 启动 Drogon 与 Vite 后运行 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke.ps1`，实际退出 0：直连与代理各两次健康检查均 HTTP 200、JSON 字段正确且 `request_id` 变化，首页检查通过。已停止本次启动的服务进程；本轮未开发认证、工单或其他业务 API，未 commit/push。
- 尚未验证：跨机器首次依赖安装与高并发业务读写。W3-D3 须确定安全密码哈希/演示凭据启用方式；业务 Repository 与鉴权逻辑留待其对应开发日完成。

## 2026-10-23 W3-D3：密码哈希与可撤销会话

- 开始时 `main` 与 `origin/main` 对齐，W3-D2 最新提交为 `4d38d7f`；仅两份原有 W1-D3 交接文件未跟踪。保留 vcpkg baseline，新增 `libsodium`，实际解析 `1.0.22#1`；Drogon `1.9.13#2`、SQLite `3.53.4#2` 未变化。CMake 配置成功，`unofficial-sodium::sodium` 链接成功；最小程序运行验证 `sodium_init()`、Argon2id 哈希/验证和安全随机数，Debug `libsodium.dll` 实际加载。
- 新增 AuthCrypto、AuthRepository、AuthService、AuthController；密码使用 libsodium Argon2id 编码串，Session Token 为 32 字节安全随机数，库中仅存其 SHA-256 摘要。登录插入 Session 与更新 `last_login_at` 同事务；退出仅撤销当前 Session；`me` 查询数据库验证撤销、UTC 过期及账户启用状态。响应禁止缓存，不返回哈希字段；`/api/health` 仍不访问数据库。`CAMPUSFIX_DB_PATH` 指定认证使用的本地库，未设置时不注册认证路由。
- 本地维护命令 `--set-demo-password --db <file> --username <demo_...>` 从交互式隐藏输入读取密码，只能设置仍为禁用且带原始哨兵的指定虚拟账号；`--enable-demo --db <file> --username <demo_...>` 是单独事务操作，仅在 Argon2id 哈希存在时启用。seed 账户保持默认禁用，测试密码随机生成于忽略的临时文件，未写入源码/SQL。
- MSVC Debug 最终构建退出 0、无编译警告。曾因沙箱 FileTracker 权限拒绝而转本机编译；一次源文件 UTF-8 编译错误通过对后端目标启用 `/utf-8` 修复。最终 CTest 4/4 通过：`sodium_crypto`、`auth_service`、`schema_migration`、`demo_seed`。认证自动化测试覆盖密码校验、禁用/未知用户、不同 Token、数据库不保存原 Token、撤销、另一会话不受影响、服务对象重建、过期和 Session 写入故障回滚。
- 使用独立临时 SQLite 测试库实际请求 HTTP：正确登录 200；错误密码、未知用户、禁用账号均 `401/AUTH_INVALID`；有效 `me` 200；无 Token/非法 Token 401；退出 200 后原 Token 401，另一有效会话仍 200；重启 Drogon 后未过期 Session 的 `me` 仍 200；修改测试库过期时间后 `401/AUTH_EXPIRED`；禁用已登录账户后旧 Token 为 401。模拟 Session 插入失败返回 500，Session 数量与 `last_login_at` 不变。数据库查询原始 Token 数量为 0。
- W2 `scripts/smoke.ps1` 实际退出 0，直连及 Vite 代理健康检查与首页均通过。检查 8 个本次运行日志，未发现测试密码、`cf_live_` Token 前缀或 Argon2id 哈希前缀；日志约 70 字节，未进行更广泛的运行日志审计。测试进程均停止，8848/5173 无监听。`git diff --check` 退出 0；未修改 W1 文档、未 commit/push。
- 尚未验证：高并发登录/密码哈希吞吐、跨机器部署、穷举攻击防护与正式限流。当前同步 Argon2id 校验在请求处理线程执行，生产化前应评估限流及专用执行资源；W3-D4 仍需完成跨资源 RBAC。

### 2026-10-23 用户人工补验收：交互式演示账号激活

- 结果来源：用户在本次对话中报告，已在 Windows 本机 PowerShell 完成人工操作。以下为用户提供的验收结果，未由 Codex 在本轮重新执行，也不计入前述 Codex 实际执行的 CTest、HTTP 和冒烟测试结果。
- 用户确认通过：隐藏密码输入；设置密码后账号仍保持禁用；通过单独操作显式启用指定单个账号；其他账号仍为禁用；启用账号使用真实 HTTP 登录成功。
- 本次未提供截图、命令输出或运行日志，记录仅依据用户陈述，不补造证据。交互式演示账号激活的人工补验收项据此记为完成；前述高并发、跨机器及限流等未验证项保持不变。

## 2026-10-23 W3-D4：统一认证、RBAC 与资源范围

- 开始时 `main` 与 `origin/main` 对齐，W3-D3 提交为 `84170d2`，工作区仅两份原有 W1-D3 未跟踪交接文件。本阶段不修改它们、不 commit/push。
- 发现冻结冲突后先暂停：SRS 第 47 章及 roles 禁止 worker 验收/评价，而 flows T-20～T-22 与 API-24/25 按原报修人授权。用户正式裁决方案 A，并批准受控勘误。已新增 D-017，最小同步 SRS/roles/API/flows/需求追踪：有效认证的 reporter 或 worker，只有数据库 reporter_id 等于本人 ID 才能按原报修人身份验收、返工、评价；维修派单身份和管理能力不替代该归属。状态、版本、重复操作约束不变，不新增 P0 功能。检查 architecture/ui-wireframes 等文档，未发现同样的绝对禁止条款。
- 新增请求级 `AuthGuard`，复用 `AuthService.me` 校验 Bearer Session；请求属性只保存不含密码/Token 哈希的不可变身份对象，无全局可变当前用户。`me/logout` 复用 Guard，匿名 health/login 保留；未配置 DB 时受保护路由返回统一 401，登录因认证存储不可用返回 500，health 仍可独立运行。所有失败使用统一 JSON/request_id，不记录 Authorization。
- 新增纯 `AuthorizationPolicy`、`AuthorizationRepository`、`AuthorizationService`。身份先经 Session 校验，Policy 服务再读当前启用状态、角色、有效 grants、工单 reporter_id、当前/历史派单及位置祖先；按动作、范围、归属判定权限。dispatcher 使用角色动作 + AREA_SCOPE；sysadmin 每个能力使用自己的 scope，NULL 为显式全局；授权不缓存，撤销后下一次检查生效。未知/不可见资源 404，已可见但动作不允许 403，身份无效 401；后续 operation 检查状态/版本/评价重复，返回相应 409。
- 字段可见性以默认拒绝白名单为基础：本人、当前负责人及有业务读取权限的管理人员才有联系字段资格；历史负责人仅最小公开字段；内部备注需区域内 dispatcher，或同时满足 BUSINESS_READ 与 INTERNAL_NOTE_READ 各自范围的 sysadmin。密码/Token 哈希、stored_path、原始 metadata_json 和无关联系方式均不在白名单。尚未实现完整工单/附件 DTO，未来必须继续对嵌套对象逐字段投影。
- 实际构建命令：`cmake --build backend/build --config Debug --parallel 4`（使用现有 VS CMake 绝对路径执行）。MSVC Debug 最终退出 0，未见编译警告；复用现有依赖缓存，无新依赖。`ctest --test-dir backend/build -C Debug --output-on-failure` 退出 0，6/6 通过：authorization_policy、guard_http、sodium_crypto、auth_service、schema_migration、demo_seed。
- SQLite/Policy：独立临时库完成 56 项检查，包含 D-017 reporter/worker 本人验收、worker 本人返工/合法评价、他人报修禁止验收/评价（即使为当前维修负责人）、管理授权不得代行、状态/版本/重复/关闭保护；另覆盖越权隐藏、历史派单失效、区域祖先/后代、子区域不能向上越权、全局授权、各能力范围不混用、授权撤销、用户停用及字段白名单。测试夹具中的状态和评价记录由测试直接构造；未实现或声称通过 API-24/25 业务 HTTP。
- 隔离 HTTP：`ctest --test-dir backend/build -C Debug -R '^guard_http$' -V` 退出 0，23 项 HTTP Guard 请求断言通过，另验证真实 login 200/no-store。覆盖匿名/非法/过期/撤销/禁用 Token 的 401、me/logout、同账号另一 Session 不受退出影响、伪造 query 身份无效、请求身份隔离、403/404 与统一响应结构。测试专用服务器独立链接，授权测试路由不进入生产可执行程序；临时随机密码、Token 和数据库在测试结束后清理，日志中未出现测试凭证。
- 实际启动生产后端（无 DB 配置）与 Vite，执行 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke.ps1`，退出 0：直连/代理健康检查与首页通过。额外 HTTP 验证生产 me/logout 无 DB 时均 401，`/test/identity` 为 404。测试进程已停止，8848/5173 无 LISTEN。
- 尚未验证：W4 业务 API、实际写操作的并发撤权竞争、高并发吞吐和正式限流。未来写入 Service 必须在同一连接短事务内复核授权、状态和版本，再执行条件写入/唯一约束；本日只读 Policy 不是业务事务的替代品。W3-D5 前端未开始。
- 最终 Git 检查：`git diff --check` 退出 0；001 迁移、frontend 和 vcpkg 清单无变更。构建/测试产物受现有忽略规则保护，临时库和凭据由测试清理，冒烟运行日志已删除。新增文件仅设 intent-to-add 以便完整 Diff 审查，暂存区无实际变更内容；两份既有交接文件未纳入。

## 2026-10-23 W3-D5：前端认证与四角色导航（浏览器人工验收待完成）

- 开始时 `main` 与 `origin/main` 对齐，W3-D4 提交 `2900d67`，工作区仅两份既有 W1-D3 交接文件未跟踪。未修改 W1 冻结文档、后端语义、Schema 或 Vite 代理。
- 新增 LoginView、auth API、Vue reactive/readonly 认证状态、模块内存凭证桥接、导航/路由规则及身份/业务占位/无权限页；调整 App、AppLayout、Element Plus 组件注册与 Axios client，无新增依赖。Token 不进入 reactive 状态、localStorage/sessionStorage、URL、页面或日志，整页刷新后重新登录。前端守卫只控制体验，真正权限仍由后端判定。
- 登录先真实调用 API-01，再用 API-03 核验身份后才显示受保护内容；失败不建立虚假身份。Axios 仅对显式 authRequired 的本应用 /api 请求附带 Bearer，阻止外部 URL/路径逃逸，丢弃携带配置或凭证的原始 Axios 错误。401 按凭证版本清理状态，避免旧请求清除新会话；403 保留当前认证，拦截器不直接跳路由以免循环。退出调用 API-02，失败时清除本机状态但明确未确认服务端撤销。
- 菜单与路由共用定义：reporter 本人报修；worker 维修任务与本人报修分开；dispatcher 需 AREA_SCOPE；sysadmin 逐项按实际 capability 显示入口，不从扁平 scope_location_ids 摘要推断每项能力范围或全局权限。所有业务页均明确占位，未调用 W4 及以后业务接口。
- 先对独立 `backend/build/w3d5-demo.sqlite3` 运行既有 migrate 和 seed，退出均为0。用户随后报告已在本机终端对 demo_reporter01/demo_worker01/demo_dispatcher/demo_sysadmin 逐个隐藏输入密码并显式启用，密码未交给 Codex。此为用户人工结果；Codex 仅用 SQLite 另行核对四角色各1个 ACTIVE，其他7个 DISABLED，未读取密码或哈希。
- 本轮实测：Node `v24.18.1`、npm `11.16.0`；`npm run test:auth` 8/8通过、退出0（使用隔离 Axios adapter 的单元测试，不是浏览器E2E），覆盖身份核验等待、四角色导航/守卫规则、凭证发送范围、401/403差异、旧401不影响新凭证、登录后me失败的回滚与离线退出。`npm run build` 退出0，Vite无警告，JS入口约266.44 kB。
- MSVC Debug `cmake --build backend/build --config Debug --parallel 4` 使用现有本机工具链实际退出0，无编译警告。`ctest --test-dir backend/build -C Debug --output-on-failure` 6/6通过、退出0，包含独立测试服务器的真实 login/me/logout HTTP 与权限测试；这些不代表本日四个手工账号的浏览器交互已通过。
- 实际启动指向上述临时库的生产 Drogon 与 Vite；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke.ps1` 退出0，直连/代理健康检查及首页均通过。经真实 Vite 代理实测错误密码登录401/AUTH_INVALID、缺字段422/VALIDATION_ERROR、匿名me/logout均401/AUTH_REQUIRED，JSON/request_id正确；`/login` HTML返回200。未获取用户密码，因此未执行四个账号的成功HTTP登录。
- 浏览器工具未提供可用浏览器连接；尝试通过 computer-use 启动本机 Edge 后，自动化安全机制因无法可靠识别当前URL而停止。本轮未完成登录页面目视或任何四角色浏览器交互，不虚构截图。用户已明确要求亲自输入密码进行后续登录/退出/导航验收；详见 `W3-D5-manual-acceptance.md`，相关结果待用户提供后单独补记。
- 内存Token仍可能受XSS影响；刷新/网络异常清除本地状态并不保证服务端已撤销，未撤销会话按服务端期限失效。正式部署HTTPS、限流及完整业务DTO等仍未验证。为保留用户已设置的演示凭据，临时库保留至人工验收结束后清理，受现有Git忽略规则保护；不自动commit/push，不进入W4。
- 本轮结束已核对并停止自行启动的后端 PID 27768 与 Vite PID 35884，确认8848/5173均无LISTEN；运行日志检查未发现 `cf_live_` 或 `Bearer `，日志已清理。`git diff --check` 实际退出0，暂存区无实际内容；新增文件仅标记 intent-to-add 以完整审查Diff。保留临时演示库供用户人工验收，没有提前删除已设置的密码哈希；完整浏览器测试及该库最终清理仍待后续人工验收结束。

### 2026-10-23 W3-D5 最终人工验收补记与阶段收尾

- 阶段与实施范围：W3-D5 登录页、内存认证状态、Axios 认证集成、路由守卫及四角色导航占位。以下为后续补记，不覆盖上文开发时的实际执行记录。
- 人工验收确认日期为2026-10-23，执行主体为项目开发者本人。用户明确反馈真实浏览器人工验收全部通过：登录页/隐藏密码/必填校验；reporter、worker、dispatcher、sysadmin分别登录并显示匹配导航；匿名及角色不匹配的受限URL被拒绝；错误密码安全提示；退出、保护旧页面及角色切换；整页刷新重新登录；后端离线时正确失败、不伪装认证成功。Codex本轮未重新执行浏览器操作。
- worker包含本人报修和维修任务入口，符合D-017；dispatcher按角色动作及AREA_SCOPE限定区域，sysadmin依赖显式管理能力与各自范围，不默认拥有审核/派单/人员管理权。此次通过不代表尚未实现的工单业务接口已完成。
- 截图证据按用户提供的说明记录：仅worker页面有截图支持，顶部为“Demo Worker 1 · 维修人员”，左侧包含本人报修和维修任务入口，业务页面标明后续占位。其余三角色依据用户人工确认；本轮未重新查看截图文件，不补造截图、日志或存储路径。
- 此前Codex实际验证结果保留：前端认证单元测试8/8、后端CTest 6/6、MSVC Debug编译成功、npm生产构建成功且无警告、W2冒烟通过；真实代理HTTP错误密码401/缺字段422/匿名me及logout 401，Git Diff空白检查通过。本次仅补文档与检查Git，未重跑这些构建、测试或HTTP请求。
- W3-D5最终状态：**通过验收**。W3整体状态：W3-D1～W3-D5五个开发单元均已完成阶段验收。下一阶段为W4-D1工单草稿创建与正式提交，本轮尚未开始。
- 保留风险：内存Token刷新后丢失；本机清Token不能保证离线服务端Session已撤销；完整登录限流、高并发/全面安全审计、跨机器首次部署仍未完成；完整工单业务API尚未开发。临时数据库是否清理由开发者决定，本轮不改动数据库，不提交密码/Token/密钥或生成产物。

## 2026-10-23 W4-D1：工单草稿、正式提交与并发安全编号

- 开始时最近提交为 `9b98b9f feat(frontend): add login state and role-based navigation`，W3-D5 已提交；工作区仅两份原有 W1-D3 未跟踪交接文件，本轮未修改或纳入它们。不修改 W1 冻结文档、001 迁移、seed、依赖版本或前端，不接触既有 W3-D5 演示数据库，不自动 commit/push。
- 对应 FR-003、API-06/API-08、T-01/T-02/T-05、D-011/D-012/D-016/D-017；实现仅限创建草稿和正式/补充后提交。新增 WorkOrder Controller/Service/Repository、模型与字段校验、Service/HTTP 集成测试；修改 CMake/main 注册。API-07 编辑、API-09～11 查询和其余状态动作、Vue 报修页均未实现。
- Controller 复用 AuthGuard 与统一 JSON/request_id；严格拒绝未知字段（含 reporter_id/user_id/role/status/order_no），校验 JSON 对象、正整数类型、UTF-8 字符数、非空文本、紧急度与真实 UTC 日历时间。仅允许 ACTIVE 的 reporter/worker 创建本人报修，管理角色不得代报。分类/位置必须存在且 active=1；依冻结契约允许有效位置，不擅自增加“只允许 ROOM”限制。响应仅投影本阶段最小字段，不序列化数据库行或联系方式。
- Service 复用现有 SQLite 连接和 `BEGIN IMMEDIATE`，在同一写事务/连接上再次验证持久化 Session、当前用户角色、资源归属和 AuthorizationService 动作/状态/版本，拒绝撤销或停用后的旧身份。AuthService 仅增加接受现有连接的 me 重载，未复制密码/Session 算法。SqliteError 保留原始数值错误码，避免回滚覆盖错误后误判繁忙；只向客户端返回安全业务码，不输出 SQL 错误、凭证或联系方式。
- 草稿为 DRAFT/version=1/order_no=NULL/submitted_at=NULL；创建主体与 ORDER_CREATED 日志同事务。提交允许 DRAFT/PENDING_SUPPLEMENT → PENDING_REVIEW，条件更新状态/version 并检查受影响行数，version+1；首次 submitted_at 保留，updated_at 刷新，追加 ORDER_SUBMITTED。状态或版本冲突分别返回 409 ORDER_STATUS_CONFLICT/ORDER_VERSION_CONFLICT，并仅对已授权资源返回 current_status/current_version。
- 编号业务时区明确选定 **Asia/Shanghai（当前 UTC+08:00）**，不依赖主机本地时区；事件时间一律 UTC。时钟由 Service 注入以便测试，不提供客户端或测试专用 HTTP 改时入口。首次提交在同事务对日期流水 UPSERT/RETURNING，范围 0001～9999，格式 WXYYYYMMDDNNNN；超过 9999 拒绝为 500 INTERNAL_ERROR 并记录安全运维错误，不生成五位流水。补充再提交沿用原号与首次提交时间。5000 ms busy_timeout 复用连接配置，写锁获取失败返回 503 DATABASE_BUSY；不设置无限重试。
- 实际构建：使用现有 CMakeCache 中的 VS CMake，执行 `cmake --build backend/build --config Debug --parallel 4`，最终退出 **0**，未见编译警告；自动重新配置复用全部已安装包，Drogon 1.9.13#2、SQLite 3.53.4#2、libsodium 1.0.22#1 未变，无新增下载/依赖。首次沙箱构建发生 MSBuild FileTracker E_ACCESSDENIED，经申请本机执行后成功，不改工具链。
- 实际全量命令：`ctest --test-dir backend/build -C Debug --output-on-failure`，最终退出 **0**、**8/8** 通过，耗时约 12.66 秒。包括 work_order_service、work_order_http，以及原有 authorization_policy、guard_http、sodium_crypto、auth_service、schema_migration、demo_seed；W3 原有六项无回归。
- `work_order_service`：独立临时库、真实 SQLite/Service 的 **52 项检查**通过（不是 HTTP）。覆盖 reporter/worker 本人报修、管理角色禁止代报、他人工单 404/可见但无动作 403、版本/终态保护、字段/日历/Unicode 边界、分类/位置停用、提交前必填复核、撤销 Session/停用用户、首次与补充提交及日志。两个线程各持独立连接竞争同一草稿：一方成功、一方409且仅一个提交事件；不同草稿并发提交均成功且单号不同。故意让创建/提交日志 INSERT 失败，主体、已有日期流水、首次日期流水插入和日志整体回滚；跨日 0001 重启、上海午夜边界和 9999 上限均实测。integrity_check=ok，foreign_key_check 无违规。
- `work_order_http`：独立临时库、每轮随机凭证、启动**正式 campusfix_backend 可执行程序**，**46 项真实 HTTP 检查**通过；非模拟 Controller/adapter。包含真实 login、reporter/worker 创建201并提交200，匿名401、管理角色403、他人工单404、非法/伪造/缺失/超长/错误类型字段422、非对象/损坏JSON400、状态与版本冲突409。独立辅助进程真实持有 SQLite 写锁，约5秒后得到503 DATABASE_BUSY，等待受限；失败请求不新增主体、流水或日志。额外探测未实现 GET 列表405、详情404，未提前开发查询接口。
- 最终 HTTP 实例：API-06 返回 `{"code":0,"data":{"id":5,"order_no":null,"status":"DRAFT","version":1},"message":"success","request_id":"req_1b0f5232-4830-45d5-b8e5-61e9ab15b481"}`；API-08 返回 `{"code":0,"data":{"id":5,"order_no":"WX202610230001","status":"PENDING_REVIEW","submitted_at":"2026-10-23T10:32:43.034Z","version":2},"message":"success","request_id":"req_cd78ccbe-468d-45ef-8582-80126c2d45aa"}`。这些是本轮真实响应，不是固定数据或凭证。
- HTTP 后另行查询 SQLite 核验：工单5/actor3、工单6/actor6 均为 PENDING_REVIEW/version2，单号分别 WX202610230001/0002，各仅2条事件；ORDER_CREATED 的 from_status 为 NULL，ORDER_SUBMITTED 的 from/to 为 DRAFT/PENDING_REVIEW，各 actor/request_id 与真实响应相符。共6条工单（4条 seed + 2条新建）、本轮流水合计2；integrity_check=ok、foreign_key_check无违规。运行日志未含本轮测试密码或完整 Token；临时凭据、收据、数据库及运行日志由测试 finally 清理。
- 首轮 HTTP 测试在超出范围 GET 路由的探测阶段失败：脚本试图将 Drogon 默认错误页解析成 JSON。已将该探测改为仅断言 HTTP 状态，其他业务响应继续断言合法 JSON/统一结构，随后全量通过；不将首次失败记为通过。
- 实际 W2 回归：启动无数据库配置的正式后端与现有 Vite，执行 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke.ps1`，退出 **0**。直连/代理各两次200 JSON、code=0/status=ok/service名称、不同 request_id 与前端首页均通过。沙箱 TCP 查询被拒绝后申请本机执行；实际核对监听属于本轮后端 PID31732、前端 PID46496，结束后仅停止它们，8848/5173剩余监听 **0**。本轮生成的冒烟日志目录已删除，不能恢复；既有演示数据库保留未动。
- Git 检查：`git diff --check` 退出0；构建目录、数据库及 WAL/SHM、node_modules/dist 仍被忽略。新增源文件与测试文件单独检查空白错误，最终文件范围仅 backend 本任务实现/测试及本进度日志；不含两份既有交接文件、凭证或生成产物。暂存区保持为空。
- 未验证/残余风险：本轮并发为双连接/双线程竞争及独立进程持锁，不代表跨进程高并发编号压力测试或吞吐保证；未做断电、磁盘满或 WAL 恢复故障实验。API-07 和退回接口尚未开发，PENDING_SUPPLEMENT 路径由真实数据库夹具构造验证，不冒充完整退回业务 HTTP E2E。前端报修/完整工单闭环、完整限流及跨机器首次部署仍未完成。阶段完成待用户验收，不进入 W4-D2。

## 2026-10-23 W4-D2：工单列表、详情及角色数据范围

- 开始时 main 与 origin/main 对齐，W4-D1 已提交为 `b4cc4a3 feat(work-orders): add draft creation and transactional submission`，工作区仅两份既有 W1-D3 未跟踪交接文件。本轮未修改它们、冻结设计、001 迁移、seed、依赖清单、前端或既有演示数据库；不自动 commit/push，不进入 W4-D3。
- 对应 FR-002/FR-004、API-09/API-10、TC-ORDER-04/05/06、TC-SEC-02/05、D-012/D-017。增加 WorkOrderRead 查询 DTO、WorkOrderQueries、WorkOrderReadService、WorkOrderQueryController 和隔离测试，扩展现有 Repository/Service 声明及路由注册、CMake；W4-D1 测试原有“GET 未实现”探测改为实际 GET 成功，并保留创建/提交数据库收据验证。
- 契约未定义省略 view 的默认值；本阶段要求显式指定 reported/assigned/managed，缺失、空值或未知值返回422，不根据角色默默合并视图。默认 page=1/page_size=20，page_size范围1～100；严格校验整数、分页偏移溢出、十个状态码及 category_id，拒绝未知列表查询参数（包括客户端 role/reporter_id）。详情不使用任何客户端角色/范围提示。
- SQL 范围：reported 固定 reporter_id=认证用户；assigned 用 EXISTS 连接 assignments/workers，以 workers.user_id=本人查询当前及受控历史任务，不与本人报修合并。managed 的 dispatcher 只使用有效 AREA_SCOPE，sysadmin 只使用 BUSINESS_READ 自身 scope；递归位置 CTE 以 UNION 展开后代且去重，NULL 仅代表该能力的显式全局授权，revoked_at 非NULL的记录不参与。先检查角色/能力，不具备该 view 权限返回403；列表与 count(*) 共用完全相同的范围及筛选条件，参数化绑定，排序 created_at DESC,id DESC。
- Service 使用同一连接的短 BEGIN 只读快照：重新校验持久化 Session/启用状态、当前角色和授权后，完成范围、total、分页及 DTO 投影。详情复用 AuthorizationService 的 Read 与 fields 判定，不可见或不存在均404；读取不更新业务状态/version/时间，也不追加 status_logs。
- DTO 使用显式 C++ 读取模型和 JSON 投影。列表只有冻结通用字段，category/location 各只有 id/code/name；原报修人、当前负责人和获准管理人员的详情才读取描述、联系信息、期望时间及当前派单 worker_id/worker_name。历史负责人不获得联系字段、完整描述或新负责人身份。本阶段所有详情的 attachments 显式为空数组（即使测试库已有附件元数据），不实现文件读写；内部备注、原始 metadata_json、调度候选、stored_path、密码/Token哈希不加载、不返回。授权管理人员的内部事件字段读取仍留待后续接口，不伪称本阶段提供时间线/管理备注查询。
- 实际构建命令：使用 CMakeCache 中的本机 VS CMake 执行 `cmake --build backend/build --config Debug --parallel 4`；最终 MSVC Debug 退出0，未见编译警告。首次沙箱构建遇 FileTracker E_ACCESSDENIED，申请本机执行后成功；CMake 自动重新配置复用现有依赖，无新增依赖或版本变更。
- 实际全量命令：`ctest --test-dir backend/build -C Debug --output-on-failure`，最终 **10/10通过、退出0**，约15.44秒。新增 work_order_read_service/work_order_read_http；原有 W4-D1 的 work_order_service/work_order_http 及 W3 六项全部通过。W4-D1 本轮实际为52项Service/SQLite检查及48项HTTP检查（新增两个GET回归断言），不是沿用旧记录冒充重跑。
- SQLite/Service：**59项实际检查通过**，使用独立临时数据库，不是HTTP测试。覆盖四角色三视图隔离、当前/历史派单、重复历史记录不重复计数、校区/建筑/房间层级、重叠父子授权不重复、授权范围与现有Policy一致、显式全局授权、AREA_SCOPE不能替代sysadmin业务读取、撤权立即拒绝、停用用户旧Session拒绝、分页/同时间id排序/组合筛选/空结果、越权404和字段资格。重复读取后 sqlite3_total_changes64 不增长，工单状态/version/时间及事件计数指纹不变。
- 正式HTTP：启动生产 campusfix_backend，使用临时SQLite中的每轮随机Bearer Session，**72项真实GET检查通过**，不是Policy或Axios适配器测试。包括匿名/非法Token401、错误view角色403、跨用户/跨校区/非负责人工单404、非法参数422、三view精确ID集合与total、分页1/100及越界页、状态/分类筛选、受控历史DTO、嵌套category/location/current_assignment严格白名单，以及撤权/停用后的下一请求失效。测试直接构造派单和授权数据，不冒充审核/派单业务HTTP流程。
- 实际API-09：`GET /api/work-orders?view=reported`（reporter A）返回200、code=0、page=1/page_size=20/total=3，items按ID 8、5、1排序，request_id=`req_b8b357e4-f581-434b-89ba-65715c31d90c`。worker reported仅ID4，assigned仅ID5/6；dispatcher管理范围为ID1/3/5/6/8；sysadmin初始BUSINESS_READ范围为ID2/4/7。
- 实际API-10：`GET /api/work-orders/5`（原报修人）返回200、code=0，id=5/order_no=WX202001010005/status=PENDING_ACCEPT/version=1，current_assignment仅含worker_id=1/worker_name=Demo Worker 1，attachments=[]，request_id=`req_68e3f1e3-7796-4c10-ae11-63f670056f0d`；描述和联系字段来自虚构夹具。历史worker查询ID6同为200但无description/contact_name/contact_phone/expected_visit_at/current_assignment。描述标记仅允许本人/当前负责人/获准管理人员读取，不能据其出现在合法详情中误判泄漏。
- 敏感信息检查：测试库植入内部备注、metadata、附件路径及派单理由标记，逐响应检查禁止字段/标记并核验嵌套字段集合；所有角色均未泄漏这些数据。sysadmin只保留BUSINESS_READ、没有INTERNAL_NOTE_READ时详情仍无内部备注。HTTP后再由独立SQLite工具核验工单状态/version/时间与日志计数指纹不变，integrity_check=ok、foreign_key_check无违规；服务日志不含测试Token。
- W2实际回归：启动无数据库配置的正式后端与既有Vite，执行 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke.ps1`，退出0；直连/代理各两次200 JSON、不同request_id与前端首页均通过。实际核对本轮监听PID：后端46748、前端49052；仅停止本轮进程，结束时8848/5173监听数0。独立临时数据库、凭据及测试日志已清理，本轮生成的冒烟日志目录已删除；既有演示库未动。
- Git检查：`git diff --check`退出0；新增文件另作空白检查；修改范围仅本阶段后端实现/测试和本进度记录。数据库/WAL/SHM、构建缓存、node_modules/dist仍被忽略，未纳入凭据或两份既有交接文件；暂存区保持为空。
- 尚未验证/残余风险：未做大数据量/高并发查询压测、执行计划优化或跨机器部署。只读快照保证请求内身份/授权/total/items一致；并发撤权时已经取得快照的在途读取可能按旧快照完成，不宣称能撤回已发送数据。当前/历史派单由SQLite夹具构造，未来实际转派/审核业务仍须各自事务验证。附件摘要与内部事件管理字段、API-07/API-11及后续业务端点和Vue业务页尚未开发；本阶段完成待用户验收。

## 2026-10-23 W4-D3：统一状态规则、追加日志仓库与授权时间线

- 开始时 main 与 origin/main 对齐，W4-D2 已提交为 `5386477 feat(work-orders): add scoped list and detail queries`；工作区仅两份既有 W1-D3 未跟踪交接文件。本轮未修改两份交接文件、W1 冻结文档、001 迁移、seed、依赖清单或前端，不接触原演示库；不自动暂存、commit/push，不进入 W4-D4。
- 对应 FR-002/003/004/010、API-11、T-01～T-23、E-01/E-02、D-011/D-012/D-014/D-017。原日志 SQL 位于 WorkOrderRepository::log，原状态校验分散于 AuthorizationPolicy::operation、WorkOrderService 和列表校验。现将流程判断统一为纯 WorkOrderStateMachine，日志 SQL 集中至 StatusLogRepository；保留原 log 方法作为既有调用者的薄委托，没有第二套 INSERT 实现。
- 状态机固定十个状态，按“源状态 + 明确动作”查找23条冻结规则；区分创建、状态改变、只读推荐和同状态事件。T-10 为只读；T-15/T-16/T-22 为不改变主状态的事件；E-01/E-02 独立定义，不增加状态或T规则。状态机不授权、不读写数据库、不自动产生日志；未提供任意目标状态更新函数或HTTP入口。AuthorizationPolicy 的流程部分复用该定义，但身份、归属、区域、管理能力、待派单取消主体限制、版本和重复评价保护仍独立保留，未改变 D-017 或 RBAC 语义。
- 现有 create/submit/补充再提交接入状态机与 StatusLogRepository，保留 ORDER_CREATED/ORDER_SUBMITTED 稳定动作、原编号规则、首次 submitted_at、409 状态/版本错误和 request_id。工单、流水、日志仍处于同一 BEGIN IMMEDIATE；日志仓库不自行 BEGIN/COMMIT，所有字段参数化绑定。模块仅提供 append/page，无 UPDATE/DELETE 或修改历史的HTTP接口。只保证应用访问边界，不宣称有SQLite写权限者已被数据库层禁止修改历史。
- API-11：`GET /api/work-orders/{id}/timeline`，复用 AuthGuard、Session、AuthorizationService。默认page=1/page_size=20（50只是契约请求示例），支持显式50/100与1～100边界、正整数及偏移溢出校验；SQL固定order_id，排序created_at ASC,id ASC。认证、授权、字段资格、total/items在同一只读快照内完成；未知/隐藏越权404、匿名401、非法参数422。详情与时间线实际路由分别验证，未提前实现其他写接口。
- 公开DTO严格为id/action/from_status/to_status/public_note/actor{id,display_name}/created_at，系统actor为null。原报修人、worker本人报修、当前和受控历史维修任务按已有资源规则读取。dispatcher仅有效AREA_SCOPE内可见内部字段；sysadmin须同时满足BUSINESS_READ与INTERNAL_NOTE_READ各自范围，撤销后下一请求立即失去内部字段或整体读取资格；没有把BUSINESS_READ等同于内部备注权限。
- 管理投影仅按已识别事件动作、前后状态及标量类型提取review_action/category_id/approved_urgency、assignment_id/worker_id/assign_reason/candidate_rank、transfer_request_id/decision、rework_count；未知事件、跨类型键、错误类型、重复键、损坏/过深/过大JSON均默认不投影。普通读取不SELECT内部列；公开模型不含原始metadata。复用Drogon已经安装的JsonCpp 1.9.6解析器，仅显式链接既有目标，无新包、无manifest或版本变化。未来审核/派单/转派/返工日志的写入契约仍须由对应业务Service实现，本轮只对受控历史夹具验证投影，不宣称这些业务API已经存在。
- 实际构建：使用CMakeCache记录的本机VS CMake执行 `cmake --build backend/build --config Debug --parallel 4`，最终MSVC Debug退出0、未见编译警告；自动配置复用既有依赖。沙箱首次构建遇MSBuild FileTracker E_ACCESSDENIED，经申请本机执行后成功，未切换工具链或安装依赖。
- 实际全量：`ctest --test-dir backend/build -C Debug --output-on-failure`，最终 **13/13通过、退出0**，约19.03秒。新增work_order_state_machine/work_order_timeline_service/work_order_timeline_http三项；检查项数是各程序内部断言，不等同于CTest项目数。
- 纯状态规则 **290项检查通过**：独立预期矩阵逐条核对十状态、23条T规则的编号/动作/源目标/效果，穷举状态与动作组合、终态禁止恢复、非法跳转和独立E事件。完整规则定义不等于实现未来业务写操作；没有通过伪造夹具宣称审核/派单/维修等HTTP通过。
- 时间线SQLite/Service **93项检查通过（不是HTTP）**：公开/管理投影、四角色归属/区域、当前及带内部备注的历史派单访问、空时间线、空actor、跨能力范围不匹配、建筑/房间匹配、撤权、同时间排序、1/50/100分页、非法参数、日志外层事务回滚和只读性。检查整张日志所有持久字段指纹以及工单状态/version/时间不变；integrity_check=ok，foreign_key_check无违规。
- 正式生产程序HTTP **58项请求检查通过**：真实Bearer GET、200/401/404/422、SQLite独立SQL逐字段比较、公开与管理嵌套白名单、跨用户/跨校区/无授权拒绝、历史任务只能公开字段、撤销内部授权后公开降级、撤销BUSINESS_READ后404、分页/total/稳定排序、空结果、request_id每请求生成及API-10/API-11路由不冲突。临时未来事件由测试直接写库，未调用或冒充未实现的业务接口。
- 实际公开请求 `GET /api/work-orders/1/timeline` 返回200、code=0、page=1/page_size=20/total=2：ORDER_CREATED（id1、NULL→DRAFT、actor{id3,Demo Reporter 1}、2020-01-01T07:55:00.000Z）和ORDER_SUBMITTED（id5、DRAFT→PENDING_REVIEW、同actor、2020-01-01T08:00:00.000Z），没有内部字段；GET的request_id为 `req_8c11bf91-80c7-4a7b-88e7-9435e092d30e`。历史日志写请求ID不在公开白名单，未与本次GET请求ID混淆。
- 实际匹配管理授权请求 `GET /api/work-orders/5/timeline?page_size=1&page=4` 返回200、total=15，ORDER_REVIEWED/id12，actor{id2,Demo Dispatcher}，internal_note="Fixture internal note"，management_detail仅含review_action=APPROVE/category_id=1/approved_urgency=HIGH，request_id=`req_e6b4d866-1d58-463f-a41b-af20ae150de2`。这些是本轮真实响应中的虚构夹具数据，不是实际审核业务已实现的证据。
- W4-D1回归：**54项SQLite/Service、48项生产HTTP检查通过**；追加工单UPDATE故障注入，确认状态、版本、编号流水及日志一起回滚且无孤立事件，原日志INSERT故障、创建/提交/补充、重复提交、版本冲突及并发编号保护继续通过。W4-D2的**59项SQLite/Service、72项生产HTTP检查**通过；W3的56项Policy检查、23项隔离Guard HTTP及认证、密码、迁移、seed测试均通过。
- W2回归：实际启动无数据库配置的正式后端与现有Vite，执行 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke.ps1`，退出0，直连/代理各两次健康检查及首页通过。核对监听属于本轮后端PID49964、前端PID31064；仅停止这两个进程，8848/5173监听数0。临时测试库、随机凭据和测试运行日志已清理；本轮生成的冒烟日志目录已删除，原演示库未动。
- Git完整性：先执行git status --short与git diff --check（退出0），新增未跟踪源文件另用no-index检查空白；暂存区保持为空。构建/数据库/WAL/SHM、node_modules/dist仍被忽略，凭证模式扫描未发现可用密码/Token/密钥。代码中测试标记仅为不可用哨兵，真实测试Token按轮生成并在测试后清理。生产源码仅StatusLogRepository含status_logs INSERT，无UPDATE/DELETE。
- 未验证/风险：未进行大规模时间线压测、断电/磁盘满故障或跨机器部署；追加式由模块边界保证，不是防御数据库管理员篡改的机制。并发撤权的在途请求仍遵循已取得的只读快照。未来业务Service必须继续验证实际负责人、原因、容量、版本和事务，状态机“合法”不能替代权限与前置条件；API-07/12及后续写接口、Vue业务页、上传均未开发。本阶段完成，等待用户功能与文件范围验收。

### W4-D3 新建但未跟踪文件（完整路径与职责）

| 路径 | 职责 |
|---|---|
| `G:/A_Study/CppProject/CampusFix/backend/src/domain/WorkOrderStateMachine.h` | 纯状态/动作/效果/过程事件接口 |
| `G:/A_Study/CppProject/CampusFix/backend/src/domain/WorkOrderStateMachine.cpp` | 十状态、T-01～23、E-01～02统一定义与查找 |
| `G:/A_Study/CppProject/CampusFix/backend/src/models/StatusLog.h` | 写入记录、内部读取记录、已投影时间线DTO与分页 |
| `G:/A_Study/CppProject/CampusFix/backend/src/repositories/StatusLogRepository.h` | 追加/查询接口，不暴露更新或删除 |
| `G:/A_Study/CppProject/CampusFix/backend/src/repositories/StatusLogRepository.cpp` | 单一参数化INSERT、按工单分页SELECT，无自行提交 |
| `G:/A_Study/CppProject/CampusFix/backend/src/services/WorkOrderTimelineService.cpp` | 认证/资源/字段授权、只读快照、严格metadata投影 |
| `G:/A_Study/CppProject/CampusFix/backend/src/controllers/WorkOrderTimelineController.h` | API-11注册声明 |
| `G:/A_Study/CppProject/CampusFix/backend/src/controllers/WorkOrderTimelineController.cpp` | HTTP入参、公开/管理JSON白名单与统一错误 |
| `G:/A_Study/CppProject/CampusFix/backend/tests/unit/WorkOrderStateMachineTest.cpp` | 独立冻结矩阵与非法组合测试 |
| `G:/A_Study/CppProject/CampusFix/backend/tests/integration/TimelineTestData.h` | 独立临时历史事件/授权夹具及全日志指纹 |
| `G:/A_Study/CppProject/CampusFix/backend/tests/integration/TimelineServiceTest.cpp` | SQLite、权限、投影、排序、只读与外层回滚验证 |
| `G:/A_Study/CppProject/CampusFix/backend/tests/integration/TimelineHttpFixture.cpp` | 仅测试CLI：准备/调整授权、独立SQL快照与完整性核验 |
| `G:/A_Study/CppProject/CampusFix/backend/tests/integration/TimelineHttpTest.ps1` | 启动正式程序、真实HTTP断言及清理，不输出Token |

### W4-D3 已跟踪修改文件（完整路径）

- `G:/A_Study/CppProject/CampusFix/backend/CMakeLists.txt`
- `G:/A_Study/CppProject/CampusFix/backend/src/domain/AuthorizationPolicy.cpp`
- `G:/A_Study/CppProject/CampusFix/backend/src/repositories/WorkOrderRepository.cpp`
- `G:/A_Study/CppProject/CampusFix/backend/src/services/WorkOrderService.cpp`
- `G:/A_Study/CppProject/CampusFix/backend/src/services/WorkOrderService.h`
- `G:/A_Study/CppProject/CampusFix/backend/src/services/WorkOrderReadService.cpp`
- `G:/A_Study/CppProject/CampusFix/backend/src/controllers/WorkOrderQueryController.cpp`
- `G:/A_Study/CppProject/CampusFix/backend/tests/integration/WorkOrderServiceTest.cpp`
- `G:/A_Study/CppProject/CampusFix/docs/progress-log.md`

不属于本任务的既有未跟踪文件保持原样：`G:/A_Study/CppProject/CampusFix/docs/AI-HANDOFF-W1-D3.md`、`G:/A_Study/CppProject/CampusFix/docs/W1-D3-CODEX-PROMPT.md`。没有自动暂存上述任何文件。

## 2026-10-23 W4-D4：基础选项前置补全与本人报修页面（待人工验收）

- 开始时 main 与 origin/main 对齐，W4-D3 已提交 `f9a2604 feat(work-orders): add state machine and authorized timeline`，工作区仅两份原有 W1-D3 未跟踪交接文件。审计确认 API-04/05 未注册，有效身份真实 HTTP 均404，按要求暂停；用户随后明确批准先补齐两接口并验证后再开发前端。本轮没有暂存、commit/push，没有进入 W4-D5。
- 范围：FR-002/003/004、API-04/05/06/08/09/10/11、D-011/012/017。没有修改冻结文档、Schema、seed、权限语义或依赖版本，不增加位置/分类管理CRUD。API-07/12/24/25/26、图片上传、审核/调度/维修/评价仍未开发。
- 前置接口：CatalogController → CatalogService → CatalogRepository，复用 AuthGuard、AuthService 与 SQLite RAII/绑定/事务。省略 parent_id 返回有效根节点，指定正整数返回有效直接子节点；固定 code,id 排序。检查父节点存在及整条祖先链有效，停用或不存在父节点/停用祖先均返回空数组，不回退为全量。固定表名由服务端布尔值选择，用户值全部参数绑定；服务在同一只读快照内重新验证 Session 并查询，不写日志或业务记录。
- 返回白名单：位置仅 id/code/name/location_type/parent_id，分类仅 id/code/name/default_priority；任一有效登录角色可读，匿名/过期/撤销401，未知或非法参数422，成功200 JSON/统一request_id/no-store。没有返回技能/资质/目标时长配置。
- 前置验收先于前端编码：MSVC Debug构建成功、当时全部CTest **15/15通过（21.61秒、退出0）**、W2冒烟通过后才开始Vue页面；沙箱首次构建遇 MSBuild FileTracker E_ACCESSDENIED，申请本机执行后成功。CMake自动配置复用既有缓存，Drogon1.9.13#2、SQLite3.53.4#2、libsodium1.0.22#1未变，无新增依赖下载或编译警告。
- 新增 catalog_service **33项 SQLite/Service检查**：四角色根节点、直接子级、不混入孙级、停用父/祖先和子项、根叶分类、无效父节点、401/422、完整数据库序列化指纹不变。新 catalog_http 最终 **52次真实生产HTTP请求**：其中基础选项40次、业务链路前的选项读取2次、reporter与worker创建→提交→列表→详情→时间线各5次（10次）。不是把Service或adapter单测记为HTTP；使用独立临时SQLite与每轮随机Token，测试后清理。GET批次后独立核验整库指纹不变、integrity_check=ok、foreign_key_check无违规，再执行明确的创建/提交写链路。
- 实际API-04请求 `GET /api/locations?parent_id=1` 返回200：`{"code":0,"data":{"items":[{"code":"DEMO-A1","id":3,"location_type":"BUILDING","name":"Demo Building A1","parent_id":1},{"code":"DEMO-A2","id":4,"location_type":"BUILDING","name":"Demo Building A2","parent_id":1}]},"message":"success","request_id":"req_e026ac76-38ed-4c62-91a2-5ecd6642b607"}`。
- 实际API-05请求 `GET /api/fault-categories?parent_id=101` 返回200：`{"code":0,"data":{"items":[{"code":"TEST-CHILD","default_priority":"NORMAL","id":102,"name":"Category child"}]},"message":"success","request_id":"req_4b4d7998-2139-4380-9ec7-530d818905eb"}`。该分类为临时测试夹具，未改正式seed。实际reporter链路order_id=9/order_no=WX202610230001，worker链路order_id=10/order_no=WX202610230002，均待审核/version2，详情一致，时间线为ORDER_CREATED、ORDER_SUBMITTED。
- 前端：ReportNewView使用真实级联位置/分类，根分类可选，无硬编码ID；必填、Unicode长度、紧急枚举及日期转UTC校验，URGENT显示线下应急提醒，不接收图片。请求白名单不含客户端身份/状态/编号。创建成功后保存当前页面真实草稿ID/version；提交失败只重试同一草稿，409/网络中断时读取同一资源核对真实状态，版本变更不自动提交。创建响应不确定时阻止盲目重建并提示先查本人列表；离开未完成草稿页面时二次确认。没有复杂草稿箱/自动恢复。
- MyOrdersView固定view=reported，状态/分类筛选、分页、真实列表与空/错/加载/重试状态；文案“我提交的报修”，worker维修任务仍独立占位。OrderDetailView只调用详情和公开时间线，显式渲染授权返回字段、十种状态中文标签、版本/编号/真实时间及可选联系方式/负责人，按页显示时间线，404统一隐藏存在性；不渲染内部备注/metadata/路径或未实现操作按钮。路由仅reporter/worker使用本轮本人报修页面，真正资源授权继续由后端执行。保留内存Token和刷新重登录策略，旧请求在组件销毁/新查询后不覆盖当前结果。
- 前端实际命令（frontend目录）：`npm.cmd run test:auth` **8/8通过、退出0**；`npm.cmd run test:orders` **6/6通过、退出0**（字段白名单/Unicode/UTC、双击保护、同草稿重试、409更新版本、丢失提交响应恢复、创建不确定阻止重建、固定reported与API请求封装）。这些是Node逻辑及Axios adapter测试，不是Vue组件或浏览器E2E。`npm.cmd run build` **成功、退出0、未见警告**，Vite8.3.5转换1683模块，无新依赖，无lockfile变更。
- 最终后端全量命令：使用CMakeCache对应VS CMake工具，`ctest --test-dir backend/build -C Debug --output-on-failure` **15/15通过、退出0、21.62秒**；包括全部W3与W4-D1～D3既有测试、新catalog两项和真实工单链路。MSVC命令 `cmake --build backend/build --config Debug --parallel 4` 已实际成功，退出0。
- 最终W2回归：启动无数据库配置的正式后端及既有Vite，执行 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke.ps1` **退出0**，直连/代理各两次200JSON、code0/statusok/service、不同request_id及首页均通过。最后一次后端PID50284、前端PID50004，仅停止本轮进程；8848/5173剩余监听数0。测试目录及临时数据库/随机凭据/日志均已清理，既有演示库未改。
- 浏览器证据：先查浏览器连接清单为空；按computer-use技能启动本机Edge后，窗口捕获被安全机制停止，原因是无法可靠确认当前浏览器URL。遵守停止要求，未继续UI输入，也未登录、填表、截图或宣称视觉/浏览器E2E通过。专用临时浏览器测试库及凭据已删除（仅本轮生成，可重新生成），后端PID47072和前端PID36972已核对路径并停止。人工验收步骤见 `docs/W4-D4-manual-acceptance.md`，全部待用户执行。
- 未验证/风险：级联控件真实交互、窄屏/键盘布局、浏览器四角色路由、刷新重新登录后查看持久工单、断网/恢复及真实409页面交互仍待人工确认；已有逻辑单测和后端HTTP不得代替这些结论。草稿重试信息只存当前页面；创建结果不确定时需要人工核查列表，不保证跨刷新/跨页面幂等。Token内存方案、离线退出撤销未确认、完整登录限流、跨机器部署等既有风险保留。

### W4-D4 文件完整清单（均未暂存）

| 完整路径 | 类型/职责 |
|---|---|
| `G:/A_Study/CppProject/CampusFix/backend/src/controllers/CatalogController.h` | 新增：基础数据路由声明 |
| `G:/A_Study/CppProject/CampusFix/backend/src/controllers/CatalogController.cpp` | 新增：API-04/05、认证、参数与响应白名单 |
| `G:/A_Study/CppProject/CampusFix/backend/src/repositories/CatalogRepository.h` | 新增：最小目录读取模型与仓库声明 |
| `G:/A_Study/CppProject/CampusFix/backend/src/repositories/CatalogRepository.cpp` | 新增：绑定查询、有效父链与稳定直接子级 |
| `G:/A_Study/CppProject/CampusFix/backend/src/services/CatalogService.h` | 新增：目录查询服务声明 |
| `G:/A_Study/CppProject/CampusFix/backend/src/services/CatalogService.cpp` | 新增：只读事务与Session再验证 |
| `G:/A_Study/CppProject/CampusFix/backend/tests/integration/CatalogTest.cpp` | 新增：33项Service/SQLite检查及HTTP隔离夹具 |
| `G:/A_Study/CppProject/CampusFix/backend/tests/integration/CatalogHttpTest.ps1` | 新增：40次基础选项请求、2次选项读取及10次真实工单链路请求 |
| `G:/A_Study/CppProject/CampusFix/backend/CMakeLists.txt` | 修改：编译源及两项CTest注册 |
| `G:/A_Study/CppProject/CampusFix/backend/src/controllers/WorkOrderController.cpp` | 修改：挂接基础数据路由，不改工单写语义 |
| `G:/A_Study/CppProject/CampusFix/frontend/src/api/orders.js` | 新增：目录、本人报修、详情、时间线、创建/提交API |
| `G:/A_Study/CppProject/CampusFix/frontend/src/utils/orders.js` | 新增：状态/事件中文、日期、表单白名单与错误说明 |
| `G:/A_Study/CppProject/CampusFix/frontend/src/utils/reportSubmission.js` | 新增：同草稿重试、409核对与重复保护 |
| `G:/A_Study/CppProject/CampusFix/frontend/src/components/CatalogPicker.vue` | 新增：真实远端级联和加载失败重试 |
| `G:/A_Study/CppProject/CampusFix/frontend/src/components/OrderStatusTag.vue` | 新增：统一中文状态标签 |
| `G:/A_Study/CppProject/CampusFix/frontend/src/components/OrderTimeline.vue` | 新增：仅公开时间线显式渲染 |
| `G:/A_Study/CppProject/CampusFix/frontend/src/views/ReportNewView.vue` | 新增：本人报修表单及创建/提交顺序 |
| `G:/A_Study/CppProject/CampusFix/frontend/src/views/MyOrdersView.vue` | 新增：reported分页/筛选/详情入口 |
| `G:/A_Study/CppProject/CampusFix/frontend/src/views/OrderDetailView.vue` | 新增：只读授权详情与分页时间线 |
| `G:/A_Study/CppProject/CampusFix/frontend/tests/orders.test.js` | 新增：6项前端逻辑/API封装测试 |
| `G:/A_Study/CppProject/CampusFix/frontend/package.json` | 修改：新增test:orders脚本，不增加依赖 |
| `G:/A_Study/CppProject/CampusFix/frontend/src/api/client.js` | 修改：通用400/422、404、409提示，保留认证行为 |
| `G:/A_Study/CppProject/CampusFix/frontend/src/router/index.js` | 修改：挂接三页与受保护详情路由 |
| `G:/A_Study/CppProject/CampusFix/frontend/src/router/navigation.js` | 修改：本人报修文案，不改角色授权 |
| `G:/A_Study/CppProject/CampusFix/frontend/src/layouts/AppLayout.vue` | 修改：已开放/占位范围说明 |
| `G:/A_Study/CppProject/CampusFix/docs/W4-D4-manual-acceptance.md` | 新增：待人工验收步骤、证据边界与风险 |
| `G:/A_Study/CppProject/CampusFix/docs/progress-log.md` | 修改：追加本轮真实结果，不覆盖历史 |

本轮27个任务文件：新增19、修改8、删除0。两份W1-D3原有未跟踪交接文件不纳入。数据库/WAL/SHM、二进制/build、node_modules/dist继续被Git忽略；暂存区为空。最终 `git diff --check` 退出0；19个新文件分别no-index检查无空白诊断（差异退出1不等于空白错误），仅Git LF→CRLF提示。冻结文件/Schema/seed差异为空，凭据模式扫描无匹配，最终8848/5173监听数0。

### 2026-10-23 W4-D4 人工验收阻塞修复：Element Plus漏注册

- 用户人工反馈四控件只有标签、没有选择器；本阶段人工验收未通过。来源是用户明确报告，本轮没有用户Console日志或截图，不虚构浏览器证据。此前构建/HTTP/逻辑单测通过不足以证明控件正常渲染。
- 根因证据：frontend/src/main.js只导入/注册ElAlert、ElButton、ElCard、ElTag、ElForm、ElFormItem、ElInput共7项，而模板实际使用20项。分类/位置依赖未注册的ElCascader，紧急度依赖ElSelect/ElOption，日期依赖ElDatePicker；另有ElTable/ElTableColumn/ElPagination/ElDescriptions/ElDescriptionsItem/ElTimeline/ElTimelineItem/ElEmpty/ElResult共9项同样缺失。模板/CSS检查未发现隐藏四控件的v-if/v-show/display:none/visibility:hidden逻辑。
- 最小修复保留按需导入与逐项.use，仅补齐上述13项，总注册20项；没有改表单字段、真实API选项、UTC转换、Schema/seed、冻结契约或认证/RBAC。修改既有orders.test.js增加全src下Vue模板el-*与main.js导入/.use一致性的静态回归；先实际运行失败（6过1失败、退出1，列出13个缺失组件），修复后7/7过、退出0，证明检查可捕捉本次缺陷但不是浏览器E2E。补测08:30+08:00转换为00:30Z，日期仍可空。
- 本轮实际前端命令：frontend目录执行npm.cmd run test:auth（8/8、退出0）、npm.cmd run test:orders（7/7、退出0）、npm.cmd run build（退出0，未见警告）。构建JS由此前307.45kB增至664.18kB（gzip220.64kB），原因是这次真正引入所需组件；未为压缩包重构架构或增加依赖。
- 本轮实际后端回归：使用CMakeCache记录的VS CMake执行cmake --build backend/build --config Debug --parallel 4，本机MSVC Debug退出0、未见警告。ctest --test-dir backend/build -C Debug --output-on-failure共15/15通过、退出0、21.63秒。后端代码未因本次修复变化；catalog_service33项及catalog_http52次真实HTTP请求再度通过，包括API-04/05的200/401/422/过滤，reporter/worker各自API-06→08→09→10→11链路；不是浏览器表单操作。
- 本轮HTTP实际样例：API-04 parent_id=1返回200、建筑id3/4，request_id=req_499f4223-dc3e-41b4-92e3-135fb69b7383；API-05 parent_id=101返回200、夹具分类id102且严格四字段，request_id=req_927a90b0-50ea-4b48-8faf-76a032d445c3。两角色链路编号分别WX202610230001/0002，时间线创建/提交事件齐全。临时数据库由测试清理，未动用户演示库；本轮未重新执行W2冒烟，不把上一轮结果说成本次重跑。
- 浏览器边界：按computer-use技能检查本轮连接，apps=[]、browsers=[]；未继续此前因URL安全核验被停止的Windows浏览器路径。未读取本轮Console、未截图、未在浏览器选择/提交；所有四控件可见/打开/操作、根分类可选、NORMAL显示/URGENT提醒、真实日期选择和浏览器提交仍待用户复验。人工验收文档已保留失败结论并添加复验说明。
- 本次相对上次交付只编辑4个文件（无新增/删除）：
  - `G:/A_Study/CppProject/CampusFix/frontend/src/main.js`：补注册13个组件，唯一新增进入Git变更范围的文件。
  - `G:/A_Study/CppProject/CampusFix/frontend/tests/orders.test.js`：注册一致性检查与UTC偏移回归。
  - `G:/A_Study/CppProject/CampusFix/docs/W4-D4-manual-acceptance.md`：记录用户失败及待复验项。
  - `G:/A_Study/CppProject/CampusFix/docs/progress-log.md`：本轮实测补记，保留上轮历史。
- 因main.js原先不在27文件中，当前W4-D4总范围为28个：新增19、修改9、删除0，另两份原有W1-D3未跟踪交接文件保持原样，不纳入。没有git add/commit/push，不进入W4-D5；最终空白检查/暂存区核对结果见交付报告。状态：代码修复及自动化通过，人工验收仍未通过、等待复验。

## 2026-10-23 W4-D5：真实报修HTTP/SQLite集成与证据归档（浏览器详细证据待补）

- 起始HEAD与本地origin/main均为`d4d2d72`，工作区干净；W4-D4两个实际提交为`c1f3881`（目录/页面相关17文件）与`d4d2d72`（遗漏前端补全11文件）。没有fetch，不声称独立联网确认了GitHub此刻HEAD。当前实际未发现两份旧W1-D3交接文件，不在跟踪或本轮变更中，未执行删除/恢复。
- 用户W4-D5任务明确补充：Element Plus修复后四类控件正常显示、浏览器功能通过、F12 Network提交及获取未发现请求错误。本轮已更新W4-D4人工文档标题并追加该后续确认，保留此前失败→修复→自动化→用户确认的发生顺序；用户未给精确操作时间/状态码/截图，不虚构这些证据。W4-D4概括人工确认不等于W4-D5逐项浏览器验收已完成。
- 对应FR-002/003/004、TC-ORDER-01～06、API-01～11已实现子集（不含API-07）、T-01/02及既有T-05回归、D-011/012/017。本轮仅新增测试与文档，不改冻结契约、Schema、seed、业务源码、权限规则或依赖，也未开发W5/编辑/取消/上传。
- 新增ReportAcceptanceFixture和ReportAcceptanceHttpTest并注册第16项CTest。复用原迁移、seed、数据库封装和原虚构权限夹具，拒绝覆盖既有数据库，六个具名虚构账号分别生成随机安全密码并通过既有设置/启用函数，仅在独立临时库使用。真实login/me取得会话；返回Token在本地证据中删除，密码/Token不写入服务日志；临时库/凭据/日志在finally清理。没有正式服务中的测试授权/故障HTTP接口。
- 缺口补全：真实登录→动态目录逐级选择→创建201并独立SQL检查DRAFT/version1/null编号/创建日志→错误版本409整库不变→提交200并独立SQL核对待审核/version2/唯一编号/提交日志→本人分页/组合筛选与total精确SQL对照→详情/时间线逐字段匹配与GET整库指纹不变→退出200/旧Token401→停服务/重启/重新登录后原状态/版本/编号/事件ID及时间不变。worker另跑本人报修链路，reported与assigned隔离。
- 新集成最终实测 **91次生产HTTP请求、20次独立SQLite快照**；状态统计200×48、201×3、401×5、403×4、404×3、409×5、422×21、500×2。两次500为预期故障注入：创建日志INSERT失败整体回滚；提交日志INSERT失败使状态/版本/流水/日志整体回滚，关闭注入后使用同一草稿成功，只有一个编号和两日志，重复submit409无写入。回滚对比完整SQLite序列化摘要，不仅比较响应。
- 本轮一次正式实测：reporter order9/reporter_id3/WX202610230001、worker order10/reporter_id6/WX202610230002、重试order11/reporter_id3/WX202610230003；均PENDING_REVIEW/version2，共新增3单、流水累计3。每单恰好ORDER_CREATED与ORDER_SUBMITTED，actor等于当前认证用户，request_id与对应HTTP请求一致，时间分别等于created_at/submitted_at。详细实际UTC、日志ID、request_id及响应见`docs/W4-D5-acceptance-report.md`，运行时脱敏完整证据见被忽略的`backend/build/Testing/w4d5-evidence.json`。
- 权限/非法输入：匿名401、跨报修人404、管理角色创建403、worker可见派单不授予原报修人提交403，管理区域与sysadmin能力自己的范围、撤权后详情404/managed403、内部权限范围错配不返备注。缺字段、无效/停用选项、错误紧急度、长度越界、伪造身份及非法版本422。新测试与原W3/W4授权/时间线/字段白名单测试共同覆盖，不把SQLite夹具或Policy说成尚未实现的业务HTTP。
- 调试记录：新脚本首次错误假定reporter03没有历史（seed实际有一单），改为合法CLOSED筛选空列表；第二次误将已可见但无提交权限的worker任务期待404，依冻结语义修正为403。两处均为测试预期错误，不修改生产业务迁就测试。最终没有未解决的自动化失败。
- 实际构建：使用CMakeCache中的本机VS CMake执行 `cmake --build backend/build --config Debug --parallel 4`，本机MSVC Debug退出0、未见警告；CMake自动配置复用缓存依赖，无下载或版本变更。
- 实际全量：`ctest --test-dir backend/build -C Debug --output-on-failure` **16/16通过、退出0、29.50秒**，新集成约7.50秒，原W3认证/RBAC/数据库和W4-D1～D4测试均通过。SQLite20次快照integrity_check=ok、foreign_key_check违规0；原Service的UPDATE/日志回滚及编号并发回归继续通过。
- 前端实际命令：frontend目录 `npm.cmd run test:auth` **9/9过、退出0**，新增角色切换时旧user/Token/授权清理、worker新身份和菜单的逻辑测试；`npm.cmd run test:orders` **7/7过、退出0**；`npm.cmd run build` **成功、退出0**，本轮实际有chunk超过500kB提示（JS664.18kB/gzip220.64kB），未为本轮验收拆包/重构。上述单测有Axios adapter，不是浏览器E2E；表单失败保存同ID的前端逻辑与后端真实500重试分别验证，未在浏览器实际注入。
- W2实际冒烟：本轮启动无数据库配置的正式后端和真实Vite，执行 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke.ps1` **退出0**；直连/代理各两次200JSON、request_id不同及首页200通过。仅停止自建PID48184/42544；8848/5173剩余监听0，临时冒烟目录清理，未删除用户库。测试脚本的其他随机端口进程亦在finally停止。
- 浏览器：按computer-use技能优先查询受控入口，apps=[]/browsers=[]；停止对应自动化，不重试绕过此前URL安全限制。Codex本轮浏览器实测和截图数均0。新增W4-D5人工清单与A～G截图索引，所有实际截图/精细交互状态为待用户提供；不制作/合成图片。现有演示库用于后续人工操作前仍需确认路径用途，密码继续用户隐藏输入。
- 安全审查：生产路由集合未新增，测试工具未链接进正式服务；写事务、服务端身份、条件版本与参数绑定未变。证据脱敏、敏感字段检查、日志凭证扫描在测试实际执行通过。只读快照不跨登录/退出区间比较，避免把合法会话写入误认为GET写入。
- 最终目标范围8文件：新增`backend/tests/integration/ReportAcceptanceFixture.cpp`、`backend/tests/integration/ReportAcceptanceHttpTest.ps1`、`docs/W4-D5-manual-acceptance.md`、`docs/W4-D5-acceptance-report.md`；修改`backend/CMakeLists.txt`、`frontend/tests/auth.test.js`、`docs/W4-D4-manual-acceptance.md`、`docs/progress-log.md`。各文件完整路径/职责见正式报告；删除0，暂存/commit/push均未执行。
- 风险/边界：本轮未做真实浏览器选择/提交/刷新/重试/Console检查或截图审查；W4-D4用户概括通过已单独记录。未模拟物理COMMIT失败、断电、磁盘满、大压力或跨机器部署；内存Token、离线退出、完整限流风险保留。当前状态为**自动化集成与文档完成，W4-D5人工证据与最终验收待用户确认**，不进入W5。
- 最终Git实检：status为4修改/4新增/0删除，暂存0；git diff --check退出0，新文件no-index无空白错误；仅LF→CRLF提示。生产源码/Schema/seed/冻结设计/依赖和前端业务源码差异为空，数据库及运行时证据仍忽略，凭据模式扫描无匹配，8848/5173监听0。完整路径清单和原始status见正式报告，未发现任何W4-D5范围外新增文件。

## 2026-10-24 W4-D5：用户人工截图补记（结果仍待补齐）

- 用户提供人工验收模板，但数据库名/Reporter与Worker编号仍为占位符，A～G仍为“通过/失败/未测”选择文字，7项附加测试均空白；不能推断为全部通过。执行主体按用户记录为项目开发者，Windows+Chrome环境；本日为补记/审阅日期，未臆造精确执行时间。
- 在指定`backend/build/Testing/w4d5-screenshots/`实际发现并逐张目视检查8张图：A-report-form、B-filled-report、C-submitted-detail、D-my-orders、E-order-timeline、F1-worker-navigation、F2-worker-own-order、G-network-results（均PNG）。只读审阅，未移动/编辑/生成图片，没有重新操作浏览器或执行自动化测试。
- 可见事实：A表单控件/位置四级展开/日期显示；B是worker填写页且URGENT提醒可见；C/D/E一致显示Reporter工单WX202610240002（/orders/7，待审核/version2），时间线为Demo Reporter 1创建/提交两事件，页面时间2026/10/24 10:22:19/10:22:20。F1显示demo_worker01身份及本人报修/维修任务独立入口，F2本人列表显示WX202610240001。两个编号来自图片，不是用户已填写确认；人工库名待确认，不读取或猜测数据库。
- 证据缺口：A选“高”但C显示“普通”，不据此认定同一份未变化的表单；B是worker而非Reporter。G显示目录/列表/详情/时间线GET200，但详情名6而非Reporter工单7，没有创建201/submit200，DevTools顶部还有1条错误指示且内容未知；不宣称Console无错误或同一工单全链路Network已齐。
- 可见截图未发现密码/Bearer/Authorization或真实私人资料，测试文字和零值电话保留；不代表检查过隐藏请求头/HAR。截图仍由build忽略，不添加到Git。详细索引、来源和未验证项已更新到人工文档和正式报告。
- 仅编辑`docs/W4-D5-manual-acceptance.md`、`docs/W4-D5-acceptance-report.md`、`docs/progress-log.md`；本轮无新增/删除文件，保留之前W4-D5代码及自动化结果，未重跑/虚构测试，不修改数据库/源码，不暂存commit/push，不开始W5。待用户补数据库名、A～G和7项附加结果、POST状态截图及Console错误内容后再决定最终人工验收状态。

## 2026-10-24 W4-D5：最终人工确认补记与G3证据核查

- 用户明确确认实际人工数据库为`backend/build/w4d4-manual.sqlite3`，Reporter工单WX202610240002、Worker本人单WX202610240001，A～G全部通过；七项附加测试（刷新重新登录、后端离线恢复、退出路由保护、非法表单、跨用户访问、同草稿失败重试、重复提交保护）全部通过。作为项目开发者人工结果记录，不冒充Codex本轮浏览器实测，不覆盖原失败历史。
- 本轮实际只读检查库文件存在，未查询、迁移、seed或修改人工库。实际打开`G3-network-create-submit.png`发现其内容为Console，而非Network：favicon.ico404、Chrome Built-In AI LanguageDetector提示，来源content_main.js:5633。按用户结论记录为非阻塞图标缺失及浏览器信息，无业务异常证据，本轮不修业务或图标。
- G3可见内容不含密码/Token/Authorization或私人信息；但没有POST创建201、submit200、工单ID或order_no，不能确认两请求同单。文件名与内容不符，明确保留写请求截图证据缺口；没有伪造、重命名、修改或合成截图。目录现9张图仍被build忽略，不暂存。
- 已更新三份验收文档，用户场景确认状态为通过，既有自动化16/16、前端9/9与7/7等结果原样保留且本轮未重跑。总体结论：自动化和用户人工场景通过，真正POST Network截图关联证据待补，暂不标记最终完整证据验收通过。请仅补真实写请求状态/响应ID及同单关联，用户已确认场景不需无故重复。
- 本轮仅编辑`docs/W4-D5-manual-acceptance.md`、`docs/W4-D5-acceptance-report.md`、`docs/progress-log.md`，无本轮新建/删除；累计W4-D5预期仍4修改/4新增，未改变源码、Schema、seed、冻结设计、测试资产或依赖。未git add/commit/push，不进入W5，等待证据与提交批准。

### 同轮后续：用户替换正确G3，重新读取核验

- 用户通知替换后，重新打开同一路径当前36294字节PNG（旧图6070字节），新图确为Network：work-orders201、submit200、详情名9及timeline查询200。可见区域未发现Token、Authorization、密码或私人信息。旧Console图已由用户覆盖，不在该路径，原实际审阅文字保留，不恢复或伪造旧图。
- 创建/提交状态码证据已补齐，但未展开创建响应data.id、submit完整URL/响应id，Method列也未显示，尚不能独立确认两请求同单。详情名9与C/E原工单7不同，不能直接关联WX202610240002或旧自动化临时order9；可能是另一次人工验证，需确认编号。
- 用户A～G与七项附加测试全部通过的确认保持有效，现仅待同单关联证据。三文档同步更新，本轮仍未重跑测试、未改代码或数据库，累计Git范围仍4修改/4新增，未暂存/提交/推送，不进入W5。

### 2026-10-24 最终补齐G3～G6：W4-D5人工与自动化验收通过

- 用户明确新补拍样本ID10、编号WX202610240005；Codex本轮逐张实际打开当前G3/G4/G5/G6复核。G3为work-orders201、submit200、详情10/时间线200；G4创建响应id10/DRAFT/version1/order_no null；G6实际是submit General（文件名G6-order-detail-response.png.png），明确POST /api/work-orders/10/submit及200；G5响应id10/WX202610240005/PENDING_REVIEW/version2，提交UTC2026-10-24T03:30:20.075Z。路径与响应ID一致，结合总览和用户确认，同一工单关联证据通过，之前证据缺口关闭。
- 这只是后续新增补拍样本，不覆盖Reporter WX202610240002和Worker WX202610240001的人工记录，不与2026-10-23自动化临时库ID10混用。本轮不另查/修改人工库，不重跑自动化、不重新操作浏览器；截图审阅与用户人工结果、旧自动化实测分别标注。
- 目录现12张图，四张新证据可见区域无密码/Token/Authorization或私人信息；G4/G5裁切的request_id不补写未知字符，G6折叠请求头不宣称已检查。图片未移动、重命名、合成或暂存，继续由build规则忽略。
- 最终结论：**W4-D5自动化验收通过；A～G浏览器人工验收及七项附加测试通过；同一工单关联证据已由新增截图补齐；人工与自动化验收通过。**favicon404和Chrome Built-In AI提示为非阻塞问题，本轮不修复。原16/16 CTest、认证9/9、报修7/7、构建/冒烟/SQL结果保留，不写成本轮重跑；W4-D4历史失败及本轮前期补图过程均保留。
- 本轮仅同步docs/W4-D5-manual-acceptance.md、docs/W4-D5-acceptance-report.md、docs/progress-log.md；无新建/删除，累计W4-D5仍4修改/4新增，等待用户Git文件核对及提交批准。未git add/commit/push，不进入W5。

## 2026-10-24 W5-D1：管理员审核 API 与事务一致性（自动化完成，待最终验收）

- 起点：main/a814f4e94eea598f95440a49627a6da082ee54e0（W4-D5 已提交），与本地 origin/main 对齐，工作区干净；本轮未 fetch。按 AGENTS 与冻结 API-13/FR-005、TC-WF-01～03、T-03/04/06 审计后实施。原两份 W1-D3 交接文件本轮实际均不存在，不创建或恢复。
- 仅新增正式 `POST /api/work-orders/{id}/review`。APPROVE→PENDING_DISPATCH 更新确认分类/紧急度；RETURN→PENDING_SUPPLEMENT 公开退回原因；REJECT→CANCELLED 写 UTC cancelled_at，使用独立 ORDER_REJECTED。三动作均 version+1/updated_at、一条追加日志，保留原编号/首次提交时间/申报紧急度/旧事件，不创建派单，不自动维修。
- 复用 AuthGuard、AuthService、AuthorizationService、WorkOrderStateMachine、StatusLogRepository、SqliteTransaction 和统一响应。在 BEGIN IMMEDIATE 内重新认证、读取角色/授权/位置层级/状态/版本，再用条件 UPDATE 检查行数并追加日志。dispatcher 用 AREA_SCOPE；sysadmin 用 REVIEW 自己的 scope，REVIEW 不自动授予 BUSINESS_READ 或 INTERNAL_NOTE_READ；403/保密型404遵循既有 Policy。
- Controller 严格限制字段/类型/正整数/动作，拒绝伪造身份、目标状态、跨动作多余字段。reason 使用合法 UTF-8、非空白1～500 Unicode标量；内部备注可选字符串、无冻结数字上限，不发明长度约束。版本极值额外防止 SQLite INTEGER→REAL 与 C++ 溢出。API 成功仅 id/review_action/status/version，统一 request_id/no-store，日志不记录正文或 Token。
- 实际新增两项 CTest：`work_order_review_service` **238 个 SQLite/Service 断言**；`work_order_review_http` **106 次正式 HTTP 请求**（含两个管理员并发的两次请求，启动探测不计数）。新纯 Policy 测试0，原授权测试回归保留。新增测试均独立临时库、虚构数据和运行时随机会话，未触碰用户人工库；测试夹具不链接进正式程序。
- 真实 HTTP 三动作：id9/APPROVE→PENDING_DISPATCH，id10/RETURN→PENDING_SUPPLEMENT，id11/REJECT→CANCELLED，均HTTP200/version3；日志actor_id2，request_id分别为 `req_d8bb2eb7-c9c0-44ff-bb82-2b29676a469f`、`req_8b498eee-e204-4228-bad9-0f4f270fe339`、`req_d23f6907-e2ca-4c68-8e1b-a3fa92aed2d4`。仅为本次临时库样本，不与旧人工库相同ID或编号混用。原两日志逐字段保留，新增事件与响应request_id一致；详情/列表/时间线逐项核对通过。完整请求/响应、SQL时间字段见`docs/W5-D1-acceptance-report.md`及忽略的`backend/build/Testing/w5d1-evidence.json`。
- HTTP 状态分布：200×23、400×4、401×5、403×7、404×4、409×7、422×53、500×3；三个500为预期故障注入。所有预期失败比对work_orders/status_logs/assignments/dispatch_records/order_number_sequences全部列指纹；日志失败、审核字段更新失败、取消时间写后失败均整笔回滚。Service另验证三动作日志失败及affected_rows=0。双SQLite连接APPROVE/APPROVE、APPROVE/RETURN与正式HTTP双管理员竞争均一成功一409，版本/事件仅增一次。
- 字段安全：reporter/worker本人时间线可读公开原因，不可见内部备注；dispatcher可读授权区白名单；sysadmin无INTERNAL_NOTE_READ时无内部扩展，单独授权后可读。原始metadata不返回。HTTP测试实际扫描服务日志无原始Session Token或内部备注标记。
- 调试如实记录：首次新HTTP测试错误地把拒绝字段名`data.field="metadata_json"`当作数据库敏感列泄漏；改为匹配实际JSON敏感键后通过，没有放宽生产数据投影。后补版本极值/断言，再次全量执行得到最终结果。
- 实际命令：复用缓存 `cmake --build backend/build --config Debug --parallel 4` 配置/编译退出0，未见编译链接警告。原Drogon1.9.13#2、SQLite3.53.4#2、libsodium1.0.22#1不变，无新下载。最终 `ctest --test-dir backend/build -C Debug --output-on-failure` **18/18通过，退出0，39.52秒**，原W3认证/RBAC/数据库与W4全链路均回归通过；新Service及HTTP库integrity_check=ok、foreign_key_check无违规。
- 前端未改业务代码；本轮实际运行 `npm.cmd run test:auth` **9/9**、`npm.cmd run test:orders` **7/7**、`npm.cmd run build`成功，均退出0。保留已有chunk>500kB警告（JS664.18kB/gzip220.64kB），不做无关拆包。W2真实后端/Vite冒烟退出0：直连与代理各两次200JSON、request_id不重复、首页200；只停止自建PID7104/25912，8848/5173监听0。
- 文件范围：新增11、修改6、删除0，完整绝对路径与职责见本轮验收报告。新增源码6、测试4、报告1；修改构建/路由挂接/共享文字校验/Service声明/进度日志。未改Schema、seed、冻结需求、权限规则、依赖或前端业务；未开发API-07/12、管理UI、人员CRUD或派单。
- Git与边界：git diff --check退出0；新增文件另做no-index检查，返回1表示新差异，不是空白失败。构建产物、SQLite/WAL、证据、node_modules/dist被忽略，暂存区保持空，未git add/commit/push。未做浏览器审核、物理COMMIT故障、断电/磁盘满、高压或跨机器验收；历史内存Token/离线退出/限流风险不变。**W5-D1自动化完成，等待用户功能、关键代码和Git文件完整性验收；不进入W5-D2。**

### 2026-10-24 W5-D1：提交前 internal_note 最小修复与重验

- 用户已暂存上轮17个W5-D1文件；本轮开始没有其他未暂存修改。Git根目录与其上级目录的`tatus --short`均不存在，status及未跟踪列表也无该文件，无法确认旧内容/来源；没有删除任何文件，没有运行git clean。
- 重新核查冻结API-13/SRS/字段字典，没有internal_note明确数字上限。按用户本轮授权，改为最多2000个Unicode标量（复用既有UTF-8校验），不再使用size_t最大值。省略、空串及纯空白（空格/Tab/CR/LF/U+3000）存NULL；先限长再归一化，非空正文不trim。RETURN/REJECT公开reason仍必填、非空白1～500字符，审核权限/事务顺序/状态机/响应不变。
- 新增Service三动作1999/2000/2001、超长空白、非法UTF-8/NUL及省略/空串/空白测试；正式HTTP三动作2000/2001汉字边界、422字段提示、失败无副作用、NULL归一化与公开原因保留。两层另验证四字节字符2000个（8000 UTF-8字节）成功、2001个拒绝。
- 本轮实际MSVC Debug构建退出0，无编译/链接警告；全量CTest **18/18，退出0，42.50秒**。审核Service **283个SQLite/Service断言**，正式HTTP回归 **149次请求**（Review HTTP 11.88秒，已计入全量，不重复计数），新纯Policy测试0。Service/HTTP临时库integrity_check=ok、foreign_key_check无违规；原权限/状态/并发/回滚测试均通过。使用临时虚构库，不触碰用户演示库。
- git diff --check退出0；本轮不重跑前端、浏览器或W2 Vite冒烟，上轮真实结果保留但不写成本轮实测。完整证据及说明见W5-D1验收报告第8节。
- 本轮修改5个文件：审核Service、两份审核测试、验收报告、进度日志；新增0/删除0。用户原暂存17文件保持旧内容，5文件出现新的未暂存修改；未执行git add/commit/push，需用户自行核对并决定重新暂存。未修改冻结文件/Schema/seed、其他权限规则或后续功能，未进入W5-D2，到此暂停。

## 2026-10-24 W5-D2：原报修人编辑、补充与重新提交（自动化完成，待人工验收）

- 基线main/HEAD=b9dc0ac5703001f4c2349947332ed6fff4880706，与本地origin/main对齐，工作区及暂存起始为空；未fetch。按API-07、FR-003/005、TC-WF-02及T-02/04/05实施，不改冻结设计/Schema/seed/依赖，不开发后续人员/管理页面/派单。
- API-07复用工单Controller、Service、Repository：全量可编辑字段+expected_version，只有原报修人的DRAFT/PENDING_SUPPLEMENT可编辑；worker本人可用，他人任务和管理身份不可冒充。BEGIN IMMEDIATE内再认证/授权/状态版本/字段引用校验，条件更新并检查行数，追加ORDER_CONTENT_UPDATED后提交；状态不变/version+1，原编号、首次提交、创建/审核/取消字段不变。
- 修改摘要仅列实际变化字段名，不复制值、联系方式或描述，不保存原请求JSON；from/to同原状态，actor/request_id/UTC与HTTP一致。API-08直接复用，保留原ID/编号/首次提交，不增长流水。共用引用校验与现有目录有效祖先规则对齐，创建/编辑/提交均拒绝停用祖先下的节点，无Schema改变。
- 前端详情通过真实reported列表确认本人归属，不新增reporter_id/can_edit DTO，不凭worker角色判断本人。最近退回原因从公开时间线最后页向前有界查找，跳过修改事件并以较新提交为边界，不用第一页旧原因。目录真实树路径预载，根分类仍可选；日期UTC/本地Date往返，复用reportPayload及既有注册控件。
- 保存/提交逻辑仅PUT→原ID submit，不调用create；响应版本驱动、防重复点击。PUT结果不确定/409阻止重放，用户确认刷新后继续；保存后submit失败保留内容/版本，查询原单，若已待审核识别成功，否则只重试submit。刷新重登后可直接提交已保存内容，无需再次PUT。新增编辑事件中文文案；新建页仅改一处离页说明，避免误称不支持草稿编辑。
- 本轮实测：MSVC Debug配置/编译退出0、无编译链接警告、依赖缓存复用无升级；最终全量CTest **20/20，退出0，52.04秒**，原W3/W4与W5-D1全部回归。新增编辑Service **60个SQLite/Service断言**、正式生产HTTP **74次请求**，新纯Policy测试0。两层临时库integrity_check=ok、foreign_key_check无违规。首轮测试夹具读游标未释放使并发测试database locked，仅修作用域后重跑通过，未放宽生产事务/超时。
- Reporter首轮临时ID9/WX202610240001，Worker本人ID10/WX202610240002：真实create201→submit200/v2→RETURN200/v3→PUT200/v4→submit200/v5→详情/时间线200。同ID/编号/首次提交不变，原三日志逐字段不变，最终首轮五事件。Reporter额外第二轮到v8、分页总8事件，证明不是固定从v3起步。PUT request_id分别req_1197fa26-0d32-4ba0-81bf-4b63997e4d6b、req_4161b706-9a4a-40c2-8659-4ef16d47a883。仅为本轮隔离库，不与旧人工库同ID混用。
- HTTP覆盖200×21、201×3、400×3、401×4、403×2、404×4、409×4、422×29、500×4；500为预期编辑/提交日志与主体故障。每次预期失败对比全部业务行/日志指纹；保存后提交失败，保存内容持久化且只重试原单。Service双SQLite连接同版本竞争只有一个成功，一个409；边界字符串、UTF-8/NUL/控制符、状态权限、有效引用和祖先停用等实际验证。报告与忽略的build/Testing/w5d2-evidence.json保存完整SQL/回执。
- 前端实际 `npm.cmd run test:auth` **9/9**、`npm.cmd run test:orders` **16/16**（原7+新增9）、`npm.cmd run build`成功，均退出0；JS674.60kB/gzip223.17kB，既有chunk>500kB警告保留。新测试是逻辑/适配器/静态注册检查，不是浏览器E2E。W2真实后端/Vite冒烟退出0：直连/代理各两次JSON200、request_id不同、首页200；仅停止自建PID31548/34680，8848/5173监听0。
- 浏览器工具不可用，声明技能文件不可读，本轮浏览器操作/截图0。新增W5-D2人工清单，目录标签、日期控件、确认交互、多轮/分页、离线/响应丢失、刷新恢复等均待用户实测，不自动标通过。最多50页归属/原因查询和有界目录检索会失败关闭，超大数据优化不扩入本轮。
- 文件范围13修改/10新增/0删除，完整绝对路径与用途见W5-D2验收报告。git diff --check退出0；新增文件另做no-index空白检查，构建/SQLite/WAL/运行时证据/node_modules/dist继续忽略。暂存为空，未git add/commit/push；未修改原人工库或无关文件。**自动化完成，等待功能、源码、Git完整性及浏览器人工验收；不进入W5-D3。**

### 2026-10-24 W5-D2：用户真实浏览器人工验收通过补记

- 项目开发者本人明确全部既定浏览器检查完成、未发现功能错误，正常补充流程、字段回填、权限、409冲突及失败恢复均通过；人工清单A～M标为“用户确认通过”，不是Codex浏览器操作或本轮自动化E2E。
- 按用户明确提供的真实截图内容，同一工单公开时间线为：创建草稿2026-10-24 14:14:41、首次提交14:14:41、管理员退回14:18:54、补充修改报修信息14:21:52、再次提交审核14:25:23。公开原因“请补充插座故障的具体情况和检测结果”；摘要`Updated fields: description`；所述公开时间线无内部审核备注标记。
- 证据分层：全部交互通过来自用户实际反馈，五事件/原因/摘要/内部标记可见性来自用户对截图的明确说明；本轮未附可独立打开的截图路径，Codex不宣称重新逐张读图。没有提供人工Network回执、request_id、ID/编号或实际库名，不编造、不复用自动化临时样本回执，也不根据页面时间臆造UTC/时区。
- 上轮真实20/20 CTest、60个编辑Service断言、74次正式HTTP、认证9/9与报修/补充16/16、构建/冒烟/SQLite完整性结果全部保留；本轮只更新文档和检查Git，没有重跑这些测试。首次测试夹具游标未释放导致锁错误及修复作用域的历史保留。
- 英文审计摘要列为非阻塞UI文案优化，本阶段不改代码。原有物理故障/高压/跨机器未测、内存Token/离线退出/限流等风险不自动变为已验证或已解决。
- 本轮仅编辑docs/W5-D2-manual-acceptance.md、docs/W5-D2-acceptance-report.md、docs/progress-log.md，新增0/删除0；累计W5-D2仍13修改/10新增/0删除。未改业务、冻结设计、Schema、seed或数据库，未git add/commit/push。**W5-D2自动化和用户人工验收通过，等待最终Git与源码审查；不进入W5-D3。**

## 2026-10-24 W5-D3：Worker后端实现及阶段测试（链接占用阻塞，未完成验收）

- 起始main/HEAD=2e690674cb6a9678032c0f0b1d9e10f2e8c8dc1b，与本地origin/main对齐，工作区/暂存干净；未fetch。按FR-006/API-14～16新增Worker模型、Controller、Service、Repository及隔离测试，不新增人员创建/删除、管理员Vue或W6调度，不改Schema/seed/冻结契约和认证权限语义。
- 列表SQL按授权服务区域过滤及计算total、稳定分页，worker仅mine=true绑定本人；负载只统计ended_at IS NULL，历史超载保留真实任务数、remaining=0且Controller告警。PUT同一BEGIN IMMEDIATE内重验身份、旧区域和目标区域及实时容量，技能采用停用/UPSERT与重新生效，不物理删除，不触碰派单/工单/状态日志；没有expected_version或人员乐观锁。
- 规则边界已说明：服务区域依SRS13.1限制有效校区/楼宇；已分配任务时修改状态/区域/技能仅影响后续资格数据，不自动释放任务。证书要求不被自动满足，空技能集不保留旧有效技能；完整候选过滤留给W6。
- MSVC源文件和新测试目标编译成功，但全工程构建退出1，正式exe链接报LNK1168。只读本机检查确认既有PID37648运行backend/build/Debug/campusfix_backend.exe（2026-10-24 14:11:35启动），监听8848；5173由既有PID6600占用。均非本轮启动，没有擅自停止。等待用户停止原服务或批准指定PID后继续，不使用旧exe冒充新接口已测试。
- 已实际执行worker_service：1/1通过、退出0、1.17秒，60个SQLite/Service断言，独立临时库integrity_check=ok、foreign_key_check无违规。前端test:auth9/9、test:orders16/16、build成功，均退出0，保留既有大chunk警告（674.60kB/gzip223.17kB）。新纯Policy测试0。
- 尚未执行新Worker正式HTTP（0次）、全量CTest/W5-D1/2/W4回归及W2冒烟，明确未验证，不宣称W5-D3完成或端口已释放。临时Service库已清理，用户原库/进程未改动；详细阶段证据与15文件完整路径见W5-D3验收报告。
- 当前新增12/修改3/删除0，暂存为空，未git add/commit/push；不进入W5-D4。解除占用后继续编译、HTTP、全量与冒烟验收，并据实更新本记录。

### 2026-10-24 W5-D3：获准处理占用后的重建与完整自动化验证

- 用户批准仅核实并处理PID37648/6600，要求身份不符不得停止、优先正常退出。本轮本机只读CIM复查返回两个PID均不存在，8848/5173无监听；因此无法再读取已退出进程的路径/命令行，未对它们发送退出或Stop-Process，不推测退出时间、原因。没有批量停止或停止其他既有进程；前述LNK1168阻塞历史保留。
- 复用现有CMake配置/依赖缓存重新执行`cmake --build backend/build --config Debug --parallel 4`：退出0，7.36秒，正式backend/build/Debug/campusfix_backend.exe成功链接，无编译/链接警告；无依赖下载、升级或工具链变更。
- 定向Worker CTest 2/2通过（退出0，12.63秒）；最终`ctest --test-dir backend/build -C Debug --output-on-failure` **22/22通过，退出0，65.02秒**。Worker Service **60个SQLite/Service断言**、Worker正式HTTP **107次请求**，新纯Policy测试0。重复定向与全量执行不累加为新的用例数量。
- 全量内同时回归W5-D1审核Service283个断言/HTTP149次、W5-D2补充Service60个断言/HTTP74次，以及W3认证/权限/迁移/seed和W4报修/查询/时间线链路，全部通过。真实HTTP使用新构建正式Drogon及独立临时库，故障注入的500是预期失败，不是假接口或Service调用替代。
- Worker HTTP验证A/B区域列表分别仅1/2/3与4/5/6、total均3，worker mine=true仅本人；验证管理能力/范围、授权撤销与禁用Session、白名单、分页筛选、真实有效负载、容量边界、技能生效/停用/空集/证书及非法输入。最终PUT worker1更新service_area_id=3、capacity=2、skill1 level4/certified=false，SQL确认持久化，实时负载仍2、remaining=0；原账号状态/角色和工单/派单/审计历史不变。
- Service/HTTP临时库`integrity_check=ok`、`foreign_key_check`无违规；GET及预期失败前后全库指纹一致，档案与技能写失败整笔回滚。双SQLite连接并发更新最终档案完整，不宣称没有版本字段的接口具有409乐观锁。回执/SQL快照留于被忽略的backend/build/Testing/w5d3-evidence.json；测试库关闭清理，未修改用户人工库。
- 本轮重新启动后端及Vite后执行`scripts/smoke.ps1`：退出0，直连/代理各两次200 JSON、code/status/service/request_id均符合要求，首页200 HTML。仅清理本轮新建测试PID25572/35336；与已消失的原PID37648/6600不同。finally恢复环境变量，结束后8848/5173监听0。
- 前端9/9认证、16/16工单及build成功沿用上一轮实测，**本轮未重跑**，既有chunk警告保留；前端代码未改，无管理员浏览器CRUD验收或截图。物理COMMIT失败、断电/磁盘满、高压、跨机器首次部署及既有内存Token/离线退出/限流风险仍未消除；完整调度过滤未提前实现。
- 最新详细命令、三接口真实回执、进程核验和完整文件路径见docs/W5-D3-acceptance-report.md。Git仍为新增12/修改3/删除0，暂存为空，git diff --check通过，未跟踪新文件另作no-index空白检查；产物和本地数据库继续忽略。未修改冻结设计、Schema、seed、前端、W3权限或无关文件，未git add/commit/push。**W5-D3自动化验证完成，无当前构建/测试阻塞，等待用户最终功能、源码与Git完整性验收；不进入W5-D4。**

## 2026-10-24 W5-D4：授权管理界面与人员完整回填（自动化完成，待浏览器人工验收）

- 起始HEAD=5a11de88c8834991b25da72d8d3a6d1288c4e975，main与本地origin/main对齐，W5-D3已提交；工作区/暂存为空，未fetch。按FR-002/004/005/006和API-09～16开发管理列表、详情/审核抽屉、人员表格及编辑；只替换三个管理占位页，不扩张W6或其他未完成业务。
- 用户已正式批准API-14最小只读响应扩展：管理列表项增加SQLite真实active/emergency_duty，data增加有效skills独立skill_options（id/code/name/requires_certificate）。先完成后端及定向HTTP回归，再开发页面；worker mine=true仍原白名单，不含管理布尔/技能选项。空列表仍可取真实完整选项，不从当前页人员推导目录，不自动授予持证。API-15/16及Schema/seed/认证权限均不变，W1冻结契约原文不改；缺口、受众、兼容影响详见W5-D4报告，不冒称原契约已含新字段。
- 管理工单固定view=managed，仅状态/分类/分页，无location_id查询参数；独立详情抽屉真实读取API-10/11并逐键显示获准管理字段，不借用报修人路由、不展示原始metadata。BUSINESS_READ/REVIEW/WORKER_MANAGE分别判断导航提示，REVIEW-only无读取能力时明确无法获取列表，前端不推断每能力scope，后端最终鉴权。
- 审核每次打开读取当前版本，切单清空原因/内部备注，APPROVE与RETURN/REJECT请求字段分离，Unicode标量500/2000限长；三个动作二次确认、禁止重复写入，409/网络/5xx只读核对并阻止重放。人员编辑缺active/emergency_duty/技能或有效目录时禁止保存；主服务区域仅真实校区/楼宇，停用原区域不静默替换。完整PUT后重读API-14与15，不把局部响应当全量；422保留表单，失权清理快照，未知结果不重放。明确API-16无乐观锁。
- MSVC Debug构建成功、退出0，无编译/链接警告，无新依赖/版本变化。Worker定向CTest2/2（15.39秒）后执行最终全量**22/22，退出0，66.10秒**；Worker Service62个断言、正式HTTP113次；原审核Service283/HTTP149、补充Service60/HTTP74及W3/W4全部回归。独立Worker库integrity_check=ok、foreign_key_check无违规，GET/失败全库指纹与业务历史检查通过。定向/全量重复执行不累加请求数量。
- 最终前端实际重跑认证**9/9**、报修/补充**16/16**（含组件注册检查）、新管理逻辑**23/23**，均退出0；build成功，JS729.02kB/gzip237.85kB，既有>500kB chunk警告保留。ElDrawer/InputNumber/Switch显式注册，MessageBox按需调用，无额外依赖，不修改原CatalogPicker或报修页面/路由。
- 新前端模块真实Vite代理HTTP**4/4、44次请求、退出0**：临时工单ID10 create/v1→submit/v2→RETURN/v3→原报修人补充/v4→resubmit/v5→APPROVE/v6/PENDING_DISPATCH，同ID；另样本REJECT成功。实测公开原因/内部备注隔离、可读无审核403/范围外404、旧版本409、空原因/低容量422及跨区更新403。人员原active=false、emergency_duty=true、OFF_DUTY、两技能Lv4持证/Lv2无证，只改容量3→4后真实重读其余值保持；增删技能后重读准确。它们是模块HTTP集成，不是浏览器点击或输入密码登录。
- 调试历史保留：首轮HTTP3/4（32次）因测试将范围外404误写403失败，只修测试并另验可读无审核403；第二轮3/4（39次）因测试完整编辑遗漏expected_visit_at被422拒绝，补null，不改后端写契约；修复后4/4（44次），最终完善事后核对失权清理逻辑后再次通过全部前端测试/build/HTTP/冒烟。不抹除失败，不把重复轮累计为更多独立用例。
- 最终W2冒烟退出0，直连/代理各两次200JSON且request_id不同，首页200HTML；前端HTTP临时库SQLite完整性输出ok，外键无违规。先确认8848/5173空闲，再启动本轮测试进程；最终仅停自建后端PID31892/Vite35716，前几轮自建PID也各自finally收尾。恢复环境变量并清理临时库/凭据，日志保留于被忽略的build/Testing/w5d4-live-*；两端口最终监听0，用户旧人工库未改动。
- 浏览器技能文件不可读、无可用受控浏览器工具；Codex本轮浏览器操作/截图0。新增W5-D4-manual-acceptance.md逐项指导安全独立库、隐藏密码启用、审核链路、完整回填、权限/409/断网/窄屏；全部人工项待用户确认，不能以HTTP或build替代视觉/交互验收。
- 完整文件范围见W5-D4报告：**修改12/新增13/删除0，共25文件**。git diff --check及新增no-index空白检查通过，暂存保持空，无无关文件，冻结设计/Schema/seed/依赖/W3权限未改；产物/数据库/WAL/凭据仍忽略。未git add/commit/push。并发人员最后写入覆盖、有界列表定位、chunk警告、内存Token/离线退出/限流及物理故障/高压/跨机器未测风险保留。**W5-D4开发与自动化完成，等待人工浏览器、源码及Git完整性验收；不进入W5-D5。**

### 2026-10-24 W5-D4：用户真实浏览器14/14人工验收通过补记

- 项目开发者本人明确确认**用户真实浏览器人工验收通过，14/14项全部通过**。管理员审核、退回补充再次提交、人员完整字段回填、只修改容量不改变其他字段、技能修改、权限隔离及窄屏检查均由用户确认，不是Codex独立操作浏览器。
- 数量口径如实说明：原清单A1～E3有27条细项，本次用户反馈为14/14；人工文档按原主题归档M01～M14并全部记为用户确认通过，保留原27条实施时的细项清单，不改写为27份独立实测或编造用户原始逐项编号。
- 本轮仅更新docs/W5-D4-manual-acceptance.md、docs/W5-D4-acceptance-report.md、docs/progress-log.md；没有修改业务代码/数据库，也没有重跑构建、CTest、前端测试、真实HTTP或浏览器。此前22/22 CTest、Worker62断言/113次HTTP、审核149/补充74次HTTP、前端9/16/23、真实代理HTTP4/4与44次请求、build/冒烟/SQLite结果仍作此前实际记录；两次失败及修复历史保留，不冒充本次重新测试通过。
- 用户反馈、此前HTTP与自动化结果分别归档。本次没有附截图、Network回执、request_id、人工工单编号、具体页面时间、实际库名或浏览器版本；不编造、不套用旧临时工单ID10或自动化请求ID。补记日期不代表实际人工操作时刻。
- API-14获准的只读DTO扩展记录完整保留；API-16无expected_version/乐观锁及最后写入覆盖风险不变，大chunk、有界读取、内存Token/离线退出/限流、物理故障/高压/跨机器未测等风险和非阻塞事项继续保留。
- 本轮只改3份文档，无新增/删除文件；累计W5-D4范围仍12修改/13新增/0删除，共25文件，暂存为空。检查git diff --check与实际文件清单，不执行git add、commit或push。**W5-D4此前自动化与用户真实浏览器人工验收通过，等待最终源码及Git审查；不进入W5-D5。**

## 2026-10-24 W5-D5：管理员权限与跨模块一致性自动化集成验收

- 起始HEAD=794bb240383010f684f901fafbe3c74c0a2220ae（feat(admin): add review and worker management views），main与本地origin/main对齐，工作区/暂存为空；实际核对status/log/branch，未fetch。阅读冻结需求与W5-D1～D4证据，主要新增测试；不改后端业务代码/授权语义/写接口/Schema/seed/依赖或W1冻结文档，不开发W6/W7。
- 新增统一W5D5SecurityFixture，复用真实迁移/正式seed及原Worker/Review夹具，在独立临时库建立A/B调度员、无授权/R(A)/R(B)/BUSINESS_READ(B)/WORKER_MANAGE(B)/MIX/仅AREA_SCOPE/全局只读sysadmin、报修人与维修员，包含待审核/已退回/待派单/拒绝及有效/历史任务。凭据随机且仅临时文件，密码占位不可登录；所有授权/Trigger仅限测试库。
- 新增正式HTTP套件通过**180次请求、406次SQLite摘要快照核查、9轮真实重叠竞争**，新增C++ Service断言**0**（夹具SQL核验不冒充Service测试）。串行每个GET或预期失败均比较全库序列化摘要；覆盖区域分页/total/重叠授权、能力与范围不能混拼、撤销/停用/过期/撤销会话、审核/人员非法输入、状态版本冲突、旧/目标区域维护权限及故障回滚。逐次回执和前后行在被忽略的backend/build/Testing/w5d5-evidence.json。
- 180次由162串行+18并发组成，最终HTTP分布200×51、201×10、400×4、401×4、403×18、404×20、409×17、422×54、500×2（两次预期故障）。失败审核/人员写入全库不变；三审核动作及补充重提同ID/编号/首次提交、精确版本和追加日志一致，公开原因与内部备注对A/B报修人和worker本人隔离。停用人员不结束既有派单，API-14/15有效负载均2；仅容量3→4保留false/true/OFF_DUTY及两技能资质，其他人员/账号/派单不变。
- 并发APPROVE/APPROVE、APPROVE/RETURN、APPROVE/REJECT各3轮，独立HttpClient请求在真实SQLite写锁下均挂起后释放，交替发送顺序；每轮200/409、version只增1、日志只增1，actor/request_id/分类/紧急度对应胜者。最终包含RETURN与REJECT实际获胜，失败APPROVE不残留字段；这是审核竞争，不计入未实现的派单TC-CON。
- 前端先写8项安全回归，得到5通过/3失败，复现非人员422容量误提示及旧/迟到负载残留；最小修复后8/8。再写失权列表刷新关闭抽屉的2项实际SFC setup测试，得到8通过/2失败；修复列表401/403/404后清编辑/详情/审核ID，以及读取提示权限消失时清理，最终**10/10**。未放宽后端权限，未做UI重构。三项已修复问题及根因见W5-D5报告。
- 新HTTP首轮在第一轮竞争已得200/409后，因锁夹具ExitCode空值报Concurrency gate failed。独立对照显示进程已退出且stderr空、Refresh后ExitCode仍空；退出前缓存Handle可得真实0，只修PS5.1测试工具句柄，不改业务或放宽竞争断言。定向通过后两轮全量均通过；最终以**23/23、退出0、95.12秒**为准，新增HTTP项26.93秒，不累加重复执行为独立用例。
- 本轮MSVC Debug构建成功、退出0，无编译/链接警告。CMake注册新测试触发重新配置，vcpkg原包均already installed，无下载/升级。原审核Service283/HTTP149、补充Service60/HTTP74、Worker Service62/HTTP113及W3/W4认证/工单/Schema/seed全部重跑通过，均已包含23项CTest，不重复计数。
- 最终前端认证9/9、报修补充16/16、管理23/23、安全10/10（逻辑/非浏览器共58）均退出0，build成功，729.28kB/gzip237.92kB，既有大chunk警告保留。原前端模块代理HTTP本轮**4/4、44次请求**通过，与新增180独立统计，不声称是DOM/浏览器E2E。W2冒烟退出0，直连/代理各两次200JSON、request_id不同，首页200HTML；独立库SQLite integrity_check=ok、foreign_key_check无违规。
- 新跨模块随机端口服务及门闩只管理本轮Process对象，finally清理临时库/Token文件，无遗留campusfix-w5d5-*目录；诊断日志保留在忽略的build/Testing目录。固定8848/5173先确认空闲再启动，最后仅清理自建后端PID40012和Vite39484，恢复环境变量，两端口监听0。用户原服务/人工库未修改或停止；无原始Token/内部备注进入服务日志。
- W5-D4用户14/14人工通过记录原样保留，本轮无独立浏览器或截图，三处前端小修复的视觉/点击补验收未执行。API-16无乐观锁、有界读取、chunk、内存Token/离线退出/完整限流、断电/磁盘满/COMMIT I/O/高压/跨机器未测等风险继续保留；未发现本轮已测范围内未解决的高严重度服务端越权或一致性缺陷。
- 本轮范围**修改6/新增4/删除0，共10文件**，详见docs/W5-D5-acceptance-report.md。git diff --check和新增no-index空白检查通过，暂存为空，产物/数据库/WAL/凭据仍忽略，无无关文件；未git add、commit、push。**W5-D5自动化完成，等待最终源码、Git及阶段验收；未进入W6-D1。**

## 2026-10-24 V1.2 AI-D1：AI文字对话辅助报修（实现/自动化完成，真实模型人工验收待完成）

- 起始main HEAD=c7688c5，W5-D5已提交，工作区/暂存为空，与本地origin/main对齐，未fetch；创建feature/v1.2-ai-report-assistant。用户选择DeepSeek并明确批准系统WinHTTP方案；独立FR-AI-01/AI-01扩展，不修改冻结P0、Schema、seed、W6或原API-01～33。
- 新增既有AuthGuard保护的AI Controller、严格Unicode/JSON输入及额度Service、可替换Provider和report-assistant-v1可信模板，D-018记录决策。按DeepSeek官方非流式Chat Completions发送完整近期上下文，不执行工具或SQL。WinHTTP系统TLS验证、禁重定向/自动认证、64KiB有界接收、有限超时/输出上限，无新增包或vcpkg版本变化。AuthGuard仍只读既有身份/授权摘要；AI模块不读写业务表。
- 新Vue聊天页及reporter/worker导航，明确勾选第三方告知后发送，逐条纯文本，无v-html；忙碌、防重复、显式重试、清空及普通报修导航。对话只页内存，12条/单条1500/总9000；离开/pagehide/失去认证清理，取消等待并丢弃迟到回复。Key仅后端环境，Token仍内存，不在页面/日志/URL泄漏。
- MSVC Debug最终构建退出0、无编译/链接警告；所有原依赖already installed，无下载/升级。新增C++296断言通过；真实Drogon对本地独立假模型HTTP63请求/上游29次通过（最终补充严格拒绝尾随逗号），验证角色、认证、输入、上下文顺序、注入不提权、故障/大小/敏感错误、并发及限额。全套前后SQLite摘要相同，integrity_check=ok、foreign_key_check空；不把假模型计为真实AI联调。
- 最终严格JSON版本全量CTest25/25、退出0、102.97秒，含原23项数据库/认证/工单/审核/人员/W5-D5安全回归；不累计重复执行。前端AI12/12、认证9/9、报修16/16、管理23/23、安全10/10，退出0；SSR转义/实际SFC逻辑是非浏览器测试。npm build退出0，JS736.48kB/gzip240.73kB，既有>500kB chunk警告保留。
- 原前端模块通过真实Vite代理HTTP4/4、44次请求（RETURN→补充→重提→APPROVE和完整人员回填），W2冒烟退出0；直连/代理各两次200JSON、request_id变化、首页200HTML，独立库SQLite检查ok。仅清理本次后端PID43864/VitePID25956和自建临时库/凭据，环境恢复，两固定端口监听0。AIHTTP另用随机端口/自建临时库，未触碰用户人工库。
- 真实调试历史：新增夹具缺少json头导致首次构建失败，补包含后成功；原认证精确菜单断言8/9，加入获准AI入口后9/9；SSR测试工具最初有Vue属性警告，改SSR编译后因把el-input当原生标签11/12，再改注册UI替身最终12/12无Vue警告。生产页面未改成假API，不消除历史失败。
- 当前执行环境四项LLM变量未设置（只查有无、未打印值），没有实际Key，没有真实DeepSeek请求。受控浏览器技能文件不可读且没有可用浏览器控制工具，本轮无浏览器操作/截图；桌面390px、真实空调/照明/漏水/危险语义、多轮/失败/清空等人工场景均未验证。**真实模型人工验收未完成，不宣称第一阶段真实AI闭环已达成。**
- 暂限Windows、分阶段超时、单进程额度可重置/不跨实例、第三方数据/费用不可撤回、Prompt非绝对安全、Token内存/离线退出/大chunk及既有未测风险保留。密钥隐藏输入、模型配置及人工步骤见docs/v1.2-ai-report-assistant.md；HTTP/构建/测试证据及文件清单见docs/V1.2-AI-D1-acceptance-report.md。
- 本轮实际8修改/18新增/0删除，共26文件，暂存为空，无无关未跟踪文件。git diff --check退出0，18个新增文件另做no-index空白检查通过；生成头文件/二进制/数据库/WAL/前端产物均被忽略。敏感常量模式无匹配，前端不含后端Key变量/服务商地址/System Prompt。不自动git add/commit/push/merge。阶段暂停，等待真实DeepSeek与浏览器人工验收，不进入AI生成草稿或W6。

### 2026-10-24 V1.2-AI-D1：用户真实DeepSeek人工验收6/6通过与归档

- 执行主体为项目开发者本人，用户明确确认**真实DeepSeek、真实浏览器人工验收6/6全部通过**：空调能记住上一轮并合理追问/建议报修；灯具不要求拆灯/接触电线/带电检修；漏水按程度澄清并适时建议维修；危险场景优先远离/停止操作/联系专业人员；模型连接异常真实报错并提供恢复、不伪装成功；清空后旧上下文不再发送、刷新符合临时会话设计。不是Codex操作浏览器，归档日期不代表具体操作时间。
- 用户实际解决登录500：缺少CAMPUSFIX_DB_PATH，配置正确数据库路径并重启后端后登录及AI对话正常。需求文档补充同一后端终端设置数据库路径、确认文件存在/已迁移且账号可登录、配置变更后重启；health正常不代表认证数据库可用。本轮未复现500、未调用登录或AI、未读写数据库。
- 服务商已确认DeepSeek，但本次实际核对的模型名称、Base URL具体值和数据库文件名未提供，均不猜测；旧配置示例不作为实测记录。无新增Network截图、真实模型request_id、计费或响应耗时证据，不编造、不套用假模型测试ID；不记录Key、Token、登录密码或敏感对话。
- 此前自动化记录原样保留：CTest25/25（102.97秒）、C++296断言、AI真实本机HTTP63次/假模型上游29次、前端70/70（AI12+认证9+报修16+管理23+安全10）、构建、旧模块代理HTTP4/4与44请求、W2冒烟、SQLite快照/完整性均为**此前实测，本轮未重跑**。首次失败及修复历史不覆盖，假模型与用户真实DeepSeek联调分开归档。
- AI不自动创建工单、不写业务数据库的设计边界不变；本次未提供额外数据库前后快照，不能声称人工验收独立证明整库完全未变化，也不将认证会话写入等同于AI业务写入。六场景通过不代表所有模型行为、窄屏/键盘、证书异常、计费或其他未反馈专项都已验收；原有安全/额度/内存Token/离线退出/大chunk等风险继续保留。
- 本轮仅编辑docs/V1.2-AI-D1-acceptance-report.md、docs/v1.2-ai-report-assistant.md、docs/progress-log.md，新增0/删除0，不修改业务代码或数据库；累计工作区仍8修改/18新增/0删除、共26文件，暂存为空。仅执行Git与文档范围/空白核验，不git add/commit/push/merge。**V1.2-AI-D1此前自动化与用户真实DeepSeek人工验收通过，等待最终源码和Git审查；不进入AI-D2或W6。**

## 2026-10-24 V1.2-AI-D2：可编辑文字报修建议（自动化完成，待人工与源码审查）

- 起始分支feature/v1.2-ai-report-assistant，HEAD=f8eae84，AI-D1已提交、工作区和暂存为空；实际执行branch/status/log -5/branch -vv，与本地跟踪分支对齐，未fetch。保持该基线，不从缺少AI-D1的main开发，不丢弃文件。FR-AI-02/AI-02独立扩展，D-019记录选择；不生成数据库DRAFT、不创建/编辑/提交工单、不预填、不改Schema/seed/旧权限/业务状态或W6。
- 新AiDraftService、report-draft-v1模板、严格六字段JSON/Unicode/空白/长度/未知字段/重复键校验，服务端requires_review固定true。Provider复用唯一WinHTTP传输，结构化模式4096 token、JSON Object、专用system及固定user整理指令，不返回工具或reasoning。内层解码Key防泄漏；AI-01仍512 token/1500字/user结尾，两个路由共用Runtime/限额，不增SDK/依赖或升级版本。
- Vue新增可编辑预览、全部字段/缺失信息编辑删除、主动重新生成/当前编辑文本复制/取消/普通报修导航。真实AI-02 API，无模板假生产结果；上下文仅当前保留窗口，生成与聊天互斥，新事实令旧建议过期。清空/认证失效/卸载/pagehide清建议与编辑/错误，Abort+代次防迟到；复制失败保留文本。category/location提示不解析为ID，不调用工单写接口、不写持久化存储，停止于AI-D2。
- 重构前AI-D1定向CTest2/2、退出0、12.85秒，296断言及原63次HTTP正常。最终MSVC Debug退出0，无编译/链接警告，依赖均already installed。最终全量CTest**27/27，退出0，126.16秒**：含新Service157断言与新HTTP套件**91请求（AI-02 80、跨路由AI-01 11）/假模型上游57**，另保留独立原AI-D1 HTTP63/上游29及原23项数据库/认证/工单/审核/人员回归；不累计重复轮。
- 新正式HTTP验证末尾assistant/user、角色与Session、输入/输出、JSON模式真实请求、坏JSON/围栏/空/缺字段/未知ID/长度/类型/截断/工具、上游故障/大小/Key转义泄漏，及chat/draft同Session并发相互拒绝、混合全局4并发、交替10次后两路由429。每个成功/失败前后、整套首尾全库序列化摘要一致；integrity_check=ok、foreign_key_check空。是独立SQLite与正式HTTP测试，不是实际DeepSeek或浏览器证明。
- 本轮前端原认证9/报修16/管理23/安全10/AI-D1 12项共70全部通过；新AI-D2 **19/19**，前端合计89（状态、API、真实SFC setup/SSR转义，非浏览器）。build退出0，JS743.72kB/gzip242.84kB，既有>500kB chunk警告保留。旧前端模块真实Vite代理HTTP4/4、44请求、W2冒烟退出0，独立旧模块库检查ok；仅停止自建后端PID37592/VitePID41748，恢复环境、删除测试库/凭据，8848/5173最终监听0。AI套件另用随机端口，自建临时目录收尾，不碰用户人工库/进程。
- 失败历史保留：首轮新增单测C4389比较警告，经显式类型修复；首次定向CTest3/4，新Service在断言31后期望拒绝失败。确认JsonCpp序列化把非法0xFF归一为合法U+FFFD，改为序列化后注入原始非法字节并保留归一化断言；未放宽生产校验，最终157断言与全量通过。补充了无可用user上下文的有界裁剪、防JSON内层转义Key泄漏、真实页面清理；最终91次/19项为准，不重复计早期90/18。
- AI-D1用户真实DeepSeek六项已通过原记录不动；**AI-D2真实DeepSeek A～H及390px/剪贴板/交互未验**。本执行环境模型/数据库变量未设置，只查有无不输出秘密；浏览器技能文件不可用、无本轮浏览器操作或截图。真实模型正确归纳/不编造/危险提醒需用户独立验收，假模型/SSR/静态检查不能替代。实际模型/地址不猜测，不记录真实Key/Token/密码/私人对话。
- 实际16修改/8新增/0删除，共24文件，暂存为空，无无关未跟踪文件；完整文件与命令、矩阵、失败历史和人工步骤见docs/V1.2-AI-D2-acceptance-report.md。git diff --check退出0、8个新文件no-index空白检查通过；敏感常量模式匹配0，前端无后端Key变量/专用Prompt，生成物/测试证据/数据库WAL/前端产物仍被忽略。用户复核前不自动暂存、commit、push或merge，不进入AI-D3或W6。模型语义、单进程额度/费用、WinHTTP平台/分阶段超时、数据不可撤回及原内存Token/离线退出/API-16/大chunk风险保留。

### 2026-10-24 V1.2-AI-D2：用户真实DeepSeek浏览器人工验收归档

- 执行主体：项目开发者本人（用户）；归档依据为用户逐项标注的A～H结果。**8项均明确“通过”，用户真实DeepSeek浏览器人工验收通过**，不是Codex本次操作浏览器。上述实现阶段“待人工”记录保留为当时状态，不覆盖AI-D1六项历史验收、原自动化数据或失败修复过程。
- A宿舍灯具：提取真实现象，不编造型号/具体根因/楼栋；B排水管漏水：根据多轮整理标题、描述、分类和位置线索；C信息不足的空调：不编造房号/根因并列出待补充信息；D冒烟/电火花：保留危险提醒，不提出带电维修或拆机，均由用户确认通过。
- E六字段可编辑且复制的是用户当前编辑内容；F继续聊天新增事实后旧建议失效、重新生成使用最新上下文；G清空/退出/离开页面后旧建议和未完成请求结果不再出现；H跳转/report/new，生成及跳转本身不调用创建/提交工单API，均由用户确认通过。
- 自动化结果仅保留此前实测：CTest27/27（退出0，126.16秒）、新Service157断言、AI-D2套件91次正式HTTP请求（AI-02 80/跨路由AI-01 11）及57次假模型上游、独立AI-D1 HTTP63次/上游29次、前端原70项加新19项共89项、MSVC Debug与npm build成功、W2冒烟通过。**本次未重新运行这些测试**；本地假模型HTTP、SFC/SSR与用户真实DeepSeek人工反馈分别归档，不合并冒充同一证据来源。
- 未提供截图、原始Network响应、具体模型名/地址、请求编号、数据库快照或计费材料，不编造上述记录，不读取或记录Key/Token/密码/敏感对话。H的反馈不扩大为本次人工全库未变化证明；系统不自动创建工单、不写业务数据库的设计边界及此前自动化全库摘要证据保持不变。
- 未单独反馈的390px窄屏、剪贴板拒绝权限、主动取消/认证自然失效、完整键盘/输入法/滚动及错误恢复组合仍待专项人工验证。模型语义非绝对保证，单进程额度/费用、Windows WinHTTP、第三方数据不可撤回、内存Token刷新重登、离线退出/API-16/大chunk等原风险保留。
- 本次仅修改docs/V1.2-AI-D2-acceptance-report.md、docs/v1.2-ai-report-assistant.md、docs/progress-log.md，新增0/删除0；不修改业务代码或数据库，保留原工作区AI-D2实现。阶段状态：**此前自动化与用户A～H人工验收通过，等待最终源码和Git审查**。本轮仅做Git及文档范围/空白检查，不git add/commit/push/merge，不进入AI-D3或W6。
- 归档实际Git核验：前后文件哈希确认仅上述三份文档变化，累计16修改/8新增/0删除、共24文件；暂存清单为空。git diff --check退出0；未跟踪的新验收报告另做no-index空白检查，无空白诊断（退出1表示相对NUL存在文件差异）。Git的LF→CRLF提示保留，不等同空白错误。没有重新运行自动化测试或模型调用。

## 2026-10-24 V1.2-AI-D3：会话隔离的一次性建议预填与明确报修确认

- 起始feature/v1.2-ai-report-assistant，HEAD=a8a9df0673b530e7841e3ebb485b6a0dff6dea33，AI-D1=f8eae84/AI-D2=a8a9df0已提交，branch/status/log -5/branch -vv实查工作区/暂存干净，与本地origin跟踪对齐；未fetch、不换main。新增FR-AI-03/D-020，不重写AI-D1六项/AI-D2八项人工验收历史。
- 源码审计确认现有API-04/05真实目录、06创建/08提交/10详情/11时间线、CatalogPicker和原reportPayload/reportSubmission可直接复用；有效非叶子位置允许，不加“必须ROOM”规则。后端业务、AuthGuard/RBAC、状态机、Schema/seed及依赖完全不改，无新增REST端点。
- 新reportDraftHandoff仅内存六字段独立副本，绑定既有credential revision及身份、五分钟过期、consume一次即清、退出/失效/凭据替换/pagehide清理；不放Token/数据库ID、不用URL/history state/浏览器存储。增加无Token的凭据变更订阅，不另造认证体系。导航失败/异常/重定向后丢弃未消费ticket，先复制再离页不受原flow.clear影响。
- AI页保留普通空报修入口，新增确认后“使用此建议”；当前编辑/会话/源页面在确认等待中变更则不交接。正式页仅setup预填title/description，未知占位留空，其余四字段文本提示。真实分类/位置须本人选，联系人/电话/日期不自动填，NORMAL不被AI改写，保留危险提醒。手工修改不被目录响应或重渲染覆盖。
- AI来源提交先原reportPayload校验、再以当前实际字段二次确认，确认后只调用原reportSubmission；取消无create，submit失败只重试原草稿，409读原版本，未知创建阻止盲目重复；纯手工报修无AI额外确认。文本插值与VNode不解释HTML。
- 本轮MSVC Debug本机构建退出0，无编译/链接警告；沙箱首轮FileTracker/MSB4018 E_ACCESSDENIED经授权同命令本机成功，无升级/新下载。全量CTest **27/27，退出0，149.49秒**，包含本轮重跑AI-D1 HTTP63/上游29、AI-D2 HTTP91/上游57及W3～W5全部原回归；不累计重复执行。
- 新交接10/10、实际SFC挂载卸载/watch与memory Router20/20，加原89项共**119/119、退出0**，最终无Vue警告；UI/adapter替身不是浏览器。npm build退出0，1707模块、JS749.95kB/gzip244.94kB，既有>500kB chunk警告保留，无新依赖/lockfile修改。收尾增加每步API会话revision前置复核及在途创建切换会话测试，不用新会话继续旧草稿提交；不承诺撤回已发出的服务器写入。
- 新AI-D3真实生产HTTP/SQLite套件**1/1、68次请求、37份独立快照、4次假模型调用**，状态200×55/201×5/403×2/409×1/422×4/500×1。验证真实聊天建议→编辑快照→确认交接前无工单写入、取消最终确认整库不变、真实目录选择后reporter/worker各一张正式工单、详情/时间线同ID/v2、提交日志故障回滚同草稿重试、真实409同ID重试、创建响应丢失阻止第二次create、无效/停用目录422和管理员403无写入。未知创建确有一张合法DRAFT，不误称数据库零新增；独立库原8单/最终13单含五个合法测试样本。37次integrity_check=ok/foreign_key_check违规0，脱敏回执及摘要在忽略的build/Testing/v1.2-ai-d3-evidence.json，测试临时库/密码/Token清理，不动用户库。
- 旧管理模块真实Vite代理HTTP **4/4、44次请求、退出0**，退回补充重提审核/人员完整回填/只改容量/技能权限回归；W2冒烟退出0，直连及代理各两次200 JSON/request_id变化、首页200 HTML。旧模块临时库完整性ok/外键无违规。先确认固定端口空闲，仅停止自建PID20892/45036且核对启动时间，恢复环境、删除新临时库/凭据，8848/5173最终监听0。新套件使用随机端口并独立finally清理。
- 调试保留：新26项首轮11通过/15失败由测试模板prefixIdentifiers缺失及host原生checkbox接口不全造成；后续22/26包含畸形测试对象异步渲染及组件识别问题。实际发现确认标记未释放导致原离页Guard阻止成功跳详情，最小修复后26/26，再补pagehide/同账号新会话/未知与恶意文字及在途创建切号专项，最终新30项。旧SSR属性替身警告补齐，不削弱原校验。新HTTP首次67次未计收尾logout，最终显式包含后68次，非重复累计。
- 浏览器技能声明路径实际文件不存在，未使用浏览器、未截图、不声称真实DeepSeek联调或390px通过。AI-D3 A～J人工步骤与全部待测状态在新报告；AI-D1/D2人工通过不替代。原模型幻觉/费用/第三方数据、单进程额度、内存Token刷新/离线退出、未知结果跨刷新/跨机器/物理故障及既有API-16/大chunk风险保留。
- 最终Git实查12修改/6新增/0删除，共18文件；git diff --check退出0，6个新文件no-index空白诊断均0（退出1仅表示文件差异），暂存清单为空、无无关未跟踪文件，后端/冻结设计/Schema/seed/AI-D1/D2验收报告/lockfile/原目录及报修校验提交模块无差异，构建/SQLite/WAL/凭据证据/前端产物仍被忽略，LF→CRLF仅提示。完整清单与命令见docs/V1.2-AI-D3-acceptance-report.md；本轮不git add/commit/push/merge，不进入W6或其他AI扩展。**V1.2-AI-D3自动化验收已完成；用户真实浏览器人工验收待完成；等待最终源码与Git审查；未进入W6。**

### 2026-10-24 V1.2-AI-D3：用户真实浏览器A～J人工验收通过补记

- 执行主体为项目开发者本人（用户），归档依据为本次逐项明确提供的结果：A AI建议带入通过、B真实故障分类通过、C真实故障位置通过、D取消交接通过、E最终用户编辑通过、F取消最终提交通过、G正式提交成功通过、H异常与重复保护通过、I刷新与身份隔离通过、J 390px窄屏通过。**用户真实浏览器人工验收10/10通过**，不是默认全通过、不是Codex独立操作浏览器；本次没有用户报告失败项。
- 上文实现阶段“人工待执行”、调试失败及修复过程保留为历史，AI-D1六项、AI-D2八项既有人工证据原样保留，不重新计作AI-D3通过。验收报告按用户实际场景名称逐项标注，并说明未单独反馈的细项不能一并视为实测。
- 此前自动化结果原样保留：CTest27/27（149.49秒）、前端119/119、新集成68次真实HTTP/37次独立SQLite快照/4次本地假模型、旧模块代理HTTP4/4与44请求、MSVC/npm构建及W2冒烟通过。**本次文档补记没有重跑任何构建、HTTP、数据库、模型或前端自动化**；只执行Git与文档空白核验，用户浏览器反馈与本地替身/组件/HTTP/SQLite证据分别归档。
- 本次没有附截图、工单号、请求ID、原始Network、实际DeepSeek配置、数据库文件名或额外快照，不编造、不读取用户数据库或凭据、不复用自动化临时样本冒充人工样本。取消和提交的用户通过反馈不是Codex检查过Network或数据库的声明；即使没有观察到创建请求，也不是独立数据库全量一致性证明，原37次快照的结论仅限原测试范围。
- J窄屏与I刷新/身份隔离已由用户确认，独立多标签页、剪贴板权限拒绝等未单独反馈专项仍未验证。保留模型幻觉/危险建议、费用/第三方数据、内存Token刷新重登、离线退出撤销不确定、未知创建人工核查/非跨刷新幂等、完整限流、跨机器/高压/物理故障及大chunk/API-16等风险。
- 本轮仅修改docs/V1.2-AI-D3-acceptance-report.md、docs/v1.2-ai-report-assistant.md、docs/progress-log.md，不改代码或数据库，不新增/删除本轮文件。**V1.2-AI-D3此前自动化与用户真实浏览器A～J人工验收通过；等待最终源码与Git审查；未进入W6或额外AI开发。**不git add/commit/push/merge。

## 2026-10-31 W6-D1：纯C++调度模型与策略接口

- 起始分支 `feature/w6-dispatch`、HEAD `7ed3a33cab601c45c797ac65039611ccc2235e25`；branch/status/log实际核对符合指定基线，工作区/暂存区干净。读取AGENTS及相关冻结需求、计划、数据库/API/流程/权限、现有模型与测试；本地无dispatch-design.md，未另建冻结设计。
- 新增独立 `campusfix_dispatch` 静态库：`DispatchWorkOrder`、组合既有 `WorkerRecord` 的 `DispatchWorker`、复用 `WorkerSkill/SkillOption`、原始效率/评价样本、候选/排除结果及 `DispatchStrategy` 抽象接口。业务ID保持64位，关键输入/结果仅做结构校验；无分数与未知历史均显式可表达，不伪造冷启动60分。固定权重35/20/20/10/10/5仅定义常量，未计算加权总分。
- 公共recommend入口接收const输入、验证后调用纯虚doRecommend；测试专用策略按显式ID排序验证确定性和可替换性，不冒充生产推荐算法。合格/排除结果分开，无排除项分数；真正资格判断仍待W6-D2，真实分项/总分、同分低负载/低ID、Top3仍待W6-D3。
- 新增CTest `dispatch_domain`：**15场景、104断言，1/1通过、退出0**；覆盖默认安全、关键非法输入、原类型兼容、64位ID、多态、重复/排列调用、原始输入快照不变、空集合、结果分区、分数表示及未知/零历史。未将未来硬过滤/加权评分测试列为通过。
- 完整Debug首次受限环境构建遇MSB4018/FileTracker E_ACCESSDENIED；同命令授权本机执行成功、退出0，未见编译/链接警告；CMake重新生成成功，依赖均already installed，无下载或版本变化。全量CTest **28/28、退出0、158.38秒**，原27项全部保留并实际回归通过，新增1项调度单测。
- dumpbin确认调度测试仅依赖MSVC运行库/Windows基础库，不依赖Drogon、SQLite、JSON或AI库。调度源码无网络、数据库、业务写入或生产路由；未接入生产服务器，未改变工单状态、AI或认证行为。现有回归仍在各自隔离HTTP/SQLite夹具运行，不是新调度HTTP或真实DeepSeek验收。
- 后续映射需补齐审核紧急度、账号状态、有效技能、位置祖先/校区/建筑、实时负载及冻结6.5历史样本；跨区支持的明确授权来源尚未核实，不能偷换管理授权或放宽硬约束；未加载历史不能当成实际零样本。详见 `docs/W6-D1-acceptance-report.md`，不修改冻结Schema或需求。
- 本轮未重跑前端/build、独立W2冒烟、AI-D3额外HTTP/快照、真实浏览器/DeepSeek及跨机器验证，不引用基线数字充作本轮结果。源码及构建范围仅纯C++模块/CMake/单测和两份记录。
- 最终范围修改2/新增6/删除0，共8文件；git diff --check退出0，6个新增文件另做no-index空白检查，无诊断；暂存区为空，产物仍忽略，无无关未跟踪文件。不git add/commit/push/merge。**CampusFix W6-D1 实现与自动化验证已完成，等待人工与源码审查；未进入W6-D2。**

## 2026-10-31 W6-D2：EligibilityFilter硬性资格筛选

- 起始分支feature/w6-dispatch，HEAD=`da6ba5a0c603b040126af8f61b4e098c9fae86c3`；工作区/暂存区干净。受限环境fetch因github.com:443连接失败；授权本机网络执行git fetch origin成功，HEAD与更新后的origin/feature/w6-dispatch一致，无清理、覆盖或切分支。
- 依据冻结SRS第14/44～56章、FR-006/007、TC-DSP-01～04/08及数据库/API/决策与D1源码，实现独立纯C++17 EligibilityFilter，复用既有DispatchWorkOrder、WorkerRecord、WorkerSkill、SkillOption和DispatchResult。仅AVAILABLE可接单；技能/资质、账号角色及启停、人员档案有效性、区域与activeTasks<capacity均为硬约束，紧急工单/值班不能豁免。
- 最小追加技能skillsLoaded及位置verified标记，默认关闭；技能未完整加载明确抛输入错误，不冒充确认无技能；位置未核实以LocationUnverified排除，不猜测为OutsideServiceArea。标记仅为可信服务端输入契约，未来适配器必须真实核实有效技能和完整有效位置链，不能接收客户端自报标记；无Schema/REST变更。
- 校区区域覆盖该校区，楼宇区域只覆盖该楼宇；同校区其他楼宇不放行，缺元数据/无法确定服务区域类型失败关闭，不借用管理授权或开放跨区分支。枚举末尾新增AccountRoleMismatch/LocationUnverified，结果校验显式识别已知值并继续拒绝Unknown。
- 固定顺序收集全部独立排除原因，每位唯一有效输入恰好出现于一组，输出按64位worker ID数值升序。候选负载/容量复制同份输入，无score/rank、无冷启动60或加权评分；此顺序不是最终得分排名。
- 本机MSVC Debug完整构建成功，补齐显式utility头后再次完整构建成功，未见编译/链接警告；CMake自动重配成功，原依赖均already installed，无下载/升级。新增CTest dispatch_eligibility：**41场景、1019断言**，含六人混合数据全部720排列；原dispatch_domain源码不变，仍**15场景、104断言**。定向2/2退出0；最终全量**29/29、失败0、退出0、158.07秒**，原28项全部保留且通过；未累计重复执行数。
- dumpbin确认新单测仅依赖MSVC运行库和Windows基础库；新过滤器无数据库/网络/业务写入、无全局可变状态，未接入生产路由。全量旧HTTP/SQLite/假模型测试仍在原隔离夹具执行，不冒充新调度HTTP或真实DeepSeek验收。前端、独立W2冒烟、额外AI-D3脚本、浏览器及跨机器/性能本轮未运行。
- W6-D3前需落实原始历史数据与冻结评分、同分规则；W6-D4适配器/确认派单须重读审核字段、账号、有效技能/区域、实时容量，在事务内重新校验；跨区来源未定保持关闭。无评分、推荐REST、派单、Schema/seed、状态机、认证/授权、AI或前端变更，未提前进入下一日。
- 完整说明、命令和文件清单见docs/W6-D2-acceptance-report.md。最终修改4/新增4/删除0，共8文件；git diff --check退出0，4个新增文件no-index空白、UTF-8及末尾换行检查通过；暂存区为空，无无关未跟踪文件，产物仍忽略。Git LF→CRLF提示保留，非空白错误。不git add/commit/push/merge。**CampusFix W6-D2 EligibilityFilter 实现与自动化验证已完成，等待人工与源码审查；未进入W6-D3。**

## 2026-10-31 W6-D3：WeightedScoreStrategy固定权重评分与稳定推荐

- 起始feature/w6-dispatch，HEAD=`e0cc7dfea6799559502241236334c5456adbd6a5`，工作区/暂存区干净。助手获准本机fetch遇Recv failure: Connection was reset后按要求暂停；用户自行fetch成功并提供相同HEAD/远端SHA，继续时助手再次核对更新后的本地引用一致，不冒称助手fetch成功。未清理、reset、stash或覆盖修改。
- 读取冻结SRS14/50、FR-007/TC-DSP、数据库6.5、API-17/18、相关决策及D1/D2源码/测试/验收。新增纯C++17 final WeightedScoreStrategy，走原公共入口校验、真实EligibilityFilter、合格历史完备性检查、六项评分与理由、严格排序和连续rank；excluded完整保留，未修改既有模型、过滤器、策略入口或D1/D2测试。
- 固定复用p0Weights 35/20/20/10/10/5；技能level/5×100、楼宇100/合法校区覆盖70、负载100×(1-active/capacity)、效率和评价按冻结均值/冷启动、仅URGENT且值班记100。校区本身和公共区域覆盖记70，不虚构同楼宇或30分跨区；满载、证书、区域及停用约束不能靠高分绕过。
- 历史nullopt不是零样本，合格人员任一历史未加载使整个调用明确失败；已加载0/1/2样本才取60，两指标独立判断并记录准确样本数和对应风险。3～20样本只在汇总后四舍五入，不逐样本舍入；目标60/实际30、60、120→83.3，评价4/5/4→86.7。最近20有效完成/评价归属、UTC时差和最终返工提交由未来D4适配器兑现，本日不查询SQLite或虚构历史。
- 本轮明确分项/总分均为一位小数；总分从返回分项及既有权重复算，十分位与权重百分比整数计算确保十进制半入（80.35→80.4）。按返回总分降序、activeTasks升序、数值workerId升序，无epsilon或隐藏精度破同分；全eligible保留，Top3 HTTP拆分留待D4。
- 本机MSVC Debug完整构建退出0，无编译/链接警告；自动配置/生成成功，依赖均already installed，无下载/升级/manifest变化。三个定向CTest **3/3、退出0、0.50秒**：原domain15场景104断言、原eligibility41场景1019断言；新增weighted_score **57场景1149断言**，含全部720排列、近似同分、历史边界、64位ID、输入不变及六组实际合成实验。最终全量CTest **30/30、失败0、退出0、156.83秒**；原29项注册/源码保留并实际通过，不累计重复执行数。
- 六组内存实验记录输入→过滤→分项→排名→理由：普通多人、紧急值班、技能不符、满载、同分按负载/ID、全不合格；测试专用最小负载对照仅比较排序差异，不实现P1策略或声称现实最优。完整实际数值及stdout证据说明在docs/W6-D3-acceptance-report.md与忽略的CTest LastTest.log。
- dumpbin仅见MSVC运行库/KERNEL32；评分模块无Drogon、SQLite、WinHTTP、AI、网络/业务写入或全局可变状态。未开发API-17/18、Controller、数据库适配器、事务派单、前端、P1策略或改动权限/审核/状态机/AI功能。原CTest中的隔离HTTP/SQLite/假模型回归不冒充新评分HTTP或真实模型验收。
- 本轮前端/build、独立W2冒烟、额外AI-D3脚本、人工浏览器、真实校园数据、跨机器和性能压测未运行。D4仍需可信数据适配、管理范围授权、只读API及派单事务中的最新状态/version/容量/资格重查；跨区授权来源未定，保持关闭。
- 最终修改2/新增4/删除0，共6文件；git diff --check退出0，4新增文件no-index空白无诊断，全部6文件UTF-8/末尾换行通过；暂存区为空，无无关未跟踪文件，构建/缓存/数据库/WAL产物仍忽略。不git add/commit/push/merge。**CampusFix W6-D3 WeightedScoreStrategy 实现与自动化验证已完成，等待人工与独立源码审查；未进入W6-D4。**

## 2026-10-31 W6-D4：真实数据推荐API与事务派单

- 起始分支feature/w6-dispatch、HEAD=`1fd636322bef80ed741ae9f6c9bca8e7906ef82a`，工作区/暂存区干净；本机git fetch origin成功，HEAD与origin/feature/w6-dispatch一致，main比较落后0/领先3。未reset、stash、清理或覆盖用户修改；收尾HEAD不变。
- 新增DispatchRepository/DispatchService、AssignmentRepository/AssignmentService及DispatchController，正式接入API-17 GET dispatch-candidates和API-18 POST assign。复用既有认证、授权范围、SQLite事务、状态机、追加日志和D2/D3生产算法，不复制评分/过滤器。AuthGuard只增加默认关闭的BUSY/LOCKED→503选项供新调度路由使用，旧路由默认行为保留。
- 从实际SQLite读取审核分类/approved_urgency、两级资质、账号与人员有效性、有效技能、服务范围及ended_at IS NULL实时负载。完整验证active位置父链、类型/无环/校区根后才verified，保留公共AREA挂在校区/建筑/楼层的已验收语义。候选池是能覆盖本位置祖先链的全部相关档案，不分页，不提前隐藏停用/不合格人员，不泄漏无关校区人员。
- 批量读取最终正常完成归属的效率及评价原始样本，先限定有效集合再按解析UTC/64位ID取各自最近20；返工读取当前最终result_submitted_at，Z与.000Z等价时间按ID排序。查询失败/时间异常阻断，不伪造零历史或60；实际0/1/2样本的冷启动仍只由领域算法处理。每次适配器load实测6次SELECT，无人员N+1，但历史扫描行数并非常数。
- 推荐在同一BEGIN读快照内重验身份、Recommend权限和状态，专用HTTP连接query_only；白名单返回Top3/其余全部合格候选、六分项/样本/理由/风险和多原因排除摘要，GET不写dispatch_records或日志。派单BEGIN IMMEDIATE内重新认证授权、查状态/version/唯一有效派单/int64溢出、重读资格容量并重算最终rank；非前三理由最多500Unicode字符且不能仅空白。原子写assignment、绑定assignment_id的完整最终快照、T-11状态/version及ORDER_ASSIGNED日志，提交前再次核验身份/授权；无外部HTTP/AI调用。
- 首轮MSVC新增测试出现auto声明类型冲突和Concurrency命名冲突，最小修复后完整Debug构建退出0，无编译/链接警告；CMake重配/生成成功，Drogon1.9.13#2、SQLite3.53.4#2、libsodium1.0.22#1等现有包均already installed，无依赖下载/升级。公共AREA过严验证及测试中的无用悬空引用在审查中修正。
- 新增Repository **39场景132断言**，Service/Assignment **13场景125断言**，直接SQLite多连接并发 **5场景22断言**；含真实唯一索引、最终排名变化、1/2样本、返工最终时间、UTC小数/闰日、最新20条及历史故障。新HTTP **86次记录请求、151次16张业务表全列指纹、3组同步竞争**，不累计重跑；正式程序与隔离四线程复用正式路由分别验证，后者两请求真实BEGIN尝试达到2后才放第三连接写锁。三类竞争均仅一胜，HTTP状态分别200/409、409/200、422/200；独立SQLite查验仅一份派单/快照/日志，容量败方状态/version未变。
- 真实HTTP覆盖401/403/404/409/422/500/503、严格JSON/字段/Unicode、64位ID、非Top3、技能撤销与停岗/容量最新事实、只读和故障回滚。assignment/dispatch_record/ORDER_ASSIGNED三者ID、实际actor/request_id关联一致；快照包含全部候选。插入派单/快照、更新状态、写日志失败均全表回滚；历史读取失败恢复后指纹不变；真实超时写锁返回503，放锁重试成功。日志/响应敏感值扫描通过，integrity_check=ok，foreign_key_check为空。
- 首轮HTTP业务断言完成但Node重复等待已信号终止子进程，导致CTest失败，未记为通过。修复清理逻辑后证据/PASS只在进程、端口、临时库清理后生成；重跑定向dispatch **7/7、退出0、29.55秒**。首轮残留单个临时夹具目录经绝对路径核对后安全删除，无用户业务文件删除。原D1/D2/D3断言不变；最终全量CTest **34/34、失败0、退出0、186.03秒**。ctest -N及起始HEAD注册名对比证实原30项无遗漏，新增4项；完整命令及最终HTTP证据路径见docs/W6-D4-acceptance-report.md。
- 最终修改5/新增16/删除0，共21文件；git diff --check退出0，新增文件独立no-index空白检查无诊断，全部UTF-8/末尾换行通过。no-index差异退出1与空白错误已区分；LF→CRLF仅Git提示。暂存区为空，无无关未跟踪文件，构建/证据JSON/数据库/WAL/SHM仍忽略。未修改冻结文档、Schema、seed、既有纯算法、状态机、认证/授权规则、AI或前端；不git add/commit/push/merge。
- 未运行新前端/浏览器、独立W2冒烟、额外AI-D3脚本、真实DeepSeek、跨机器或生产规模压测；全量旧CTest不冒充这些未运行项目。候选历史全量扫描可能增长锁持有时间，需在不截断事实的前提下后续评估；撤销与派单按SQLite写锁顺序线性化，不宣称能看到未提交的撤销。等待人工和独立源码审查，不进入W6-D5。
- **CampusFix W6-D4 真实推荐API与事务派单实现及自动化验证已完成，等待人工和独立源码审查；未进入W6-D5。**

## 2026-10-31 W6-D5：管理员智能调度面板与真实代理联调

- 起始分支feature/w6-dispatch，HEAD=`a086eab55fff2580be8802bf0d432a7e02f0ba27`；实际fetch成功，HEAD与origin/feature/w6-dispatch一致，工作区/暂存区干净。按AGENTS优先级阅读冻结调度需求、API、权限/决策、D4报告及现有前端/后端实现，没有重发明评分、资格或事务规则。
- 新增AdminDispatchView、候选卡/六项分数/确认对话框、dispatch API及本地状态模块；将菜单占位替换为真实/admin/dispatch与/admin/dispatch/:id，AdminOrders对合法读到的待派工单提供导航。ElDialog显式局部注册，复用现有Element Plus/客户端/认证/路由，无新依赖。仅在有BUSINESS_READ时读列表/详情，DISPATCH-only保留ID/深链接入口，不借范围摘要扩权，不读取其他区域人员资料。
- 所有候选排名、分项、总分、样本和风险来自API-17，不重算、不重排、不自动选择。空覆盖池明确说明仅该工单覆盖范围内无人员，多原因次数不当成人数。校验DTO及JS安全整数，不截断64位ID；确认POST仅发送worker_id、GET实际order_version和可选reason，非前三必填最多500Unicode字符且非空白。generation清理切换工单、权限/范围变化、退出/卸载后的晚到请求；submitting禁重复，409清旧快照，422区分资格失效和可修改字段。
- POST超时/断网/5xx/异常回执进入unknown_result，禁止重发；只在有读取能力时核实详情，即使PENDING_ACCEPT也不冒称本次POST成功，无读取权限保持未知。200真实回执后清除旧推荐，有读取权限时刷新核实状态/负责人，未修改本地状态假装成功。
- 新增仅测试C++DispatchUiFixture及CMake构建目标，隔离迁移/seed、随机Argon2id密码和虚构账号供真实登录；不改backend/src或生产CLI/HTTP/Schema。MSVC Debug完整构建退出0、无编译/链接警告，现有依赖already installed，无下载升级。新增35/35前端单元/安全测试通过，含实际SFC setup/watch执行与模拟状态机测试；既有auth9、orders16、admin23、security10、AI12、draft19、handoff30均实际通过，旧119/119+新35=154/154，不含HTTP6项。首次npm.ps1受执行策略阻止，使用现有npm.cmd成功，未修改系统策略或升级全局工具。
- 独立脚本准备随机端口Drogon+真实Vite代理，真实HTTP **6/6、退出0，35次前端模块请求、10次独立16表全列指纹、4条派单关联**：Top1、非Top3有原因、冲突、最新停岗422、空候选/空覆盖池、跨范围404、无权限403及退出后旧Token401；回执与SQLiteassignment/dispatch_record/ORDER_ASSIGNED的ID及request_id一致，最终PENDING_ACCEPT/version4，integrity_check=ok、foreign_key_check为空。真实POST提交后在客户端丢回执是明确测试注入，不冒充真实断网或假后端：两次均保持未知、不重放，其中DISPATCH-only未调用列表/详情。安全证据在忽略的backend/build/Testing/w6d5-frontend-evidence.json，不记录Token/密码，不累计重跑。
- 最终后端全量CTest **34/34、失败0、退出0、196.32秒**，包括D1～D4及旧认证/数据库/HTTP/假模型回归；按起始HEAD静态及foreach展开注册名核对原34项无遗漏，没有新增/删除CTest。Vite build退出0，JS约775.89kB/gzip252.62kB，>500kB chunk警告作为非阻塞项保留，未提高阈值掩盖。
- **真实浏览器未验证**：按computer-use技能尝试，原生管道不存在，浏览器host unavailable、清单为空；没有页面点击、桌面/390px视觉、Console或截图证据，不能将Node/SFC/HTTP标成人工UI通过。为浏览器保留的隔离待机因环境不可用被中断（退出1），核实自建服务停止后精确删除其临时库/凭据；随后普通模式完整HTTP重跑退出0，确认进程、随机端口、临时数据库和凭据自动清理。没有删除用户业务文件。
- 修改8/新增12/删除0，共20文件，暂存区为空、无无关未跟踪文件；git diff --check退出0，12新增文件独立no-index空白检查无诊断，全部20文件UTF-8及末尾换行通过，产物忽略范围已核对。LF→CRLF仅Git提示，非空白错误。冻结设计、Schema、seed、后端生产源码/算法、AI和依赖版本未变；不git add/commit/push/merge。完整文件清单、命令、真实证据、风险及人工验收步骤见docs/W6-D5-acceptance-report.md。
- W6-D1～D4为已提交基线，D5代码与自动化完成，但真实浏览器验收及独立源码审查待补；W6不得宣称最终验收通过。未运行跨机器/生产压测、真实DeepSeek、独立admin:http/AI-D3全链路及独立W2冒烟，未进入W7或合并main。
- **CampusFix W6-D5 管理员智能调度面板与端到端验收未完成（代码与自动化已完成，真实浏览器验收待补），等待独立源码审查；W6功能分支尚未合并main。**

## 2026-10-31 W7-D1：维修员接单、拒单与当前任务工作台

- 用户本次明确确认 W6 已通过测试、源码审查及浏览器验收并合并main；此为用户提供的后续阶段确认，不改写上方工具失败和待补的历史，也不冒称 Codex 独立点击过浏览器。实际 git fetch 验证 origin/main 为 `9d7b78d0afcd6b16df75a2f6d324dc88c4686503`（PR #2合并），W6末提交f8aa95e是其祖先；起始工作区/暂存区干净，main快进后新建 `feature/w7-workflow`，没有覆盖本地修改。
- 依据 API-19/20、T-13/T-14 新增 WorkerTaskController/Service/Repository。单连接 BEGIN IMMEDIATE 内重新认证、AuthorizationService动作/归属/状态/版本校验；真实有效派单接单写 accepted_at，拒单写 ended_at/REFUSED，派单与工单版本递增、int64溢出保护，关联真实 assignment_id 的追加日志，CAS状态和最终身份复核，同事务提交/失败回滚。拒单的PENDING转派申请同事务SUPERSEDED、系统原因/解决时间、reviewed_by=NULL；不新增转派API。
- 只读列表最小补充 assigned 专用 `current_only=true`：当前有效派单属于认证维修员，计数/分页共享服务端过滤；省略/false保持原历史Assigned语义。报告说明重复本人刚拒单只识别409，不赋历史负责人操作权。原报修人/dispatcher/有管理能力sysadmin不可冒充维修员；401/403/404/409/400/422与SQLITE_BUSY503安全处理。
- Vue `/worker/tasks` 替换占位为真实工作台，待接/进行中分页、真实编号/分类/位置/审核紧急度/状态/版本；接单二次确认、拒单必填1～500Unicode原因、ElDialog显式注册，POST不带身份字段、不自动重试。409清旧操作、401/403/404清未授权数据；网络/5xx/异常回执保持未知，通过授权GET只读核实、不当成本请求成功证明；generation阻止换用户/退出/卸载后的晚到数据。成功后重新读取列表，不伪造本地状态，与本人报修和历史记录分离。
- MSVC Debug完整构建退出0、未见新增编译/链接警告；CMake自动重生成，依赖均already installed，baseline、Drogon/SQLite/libsodium版本不变，无安装升级。新增CTest：worker_task_service **8场景77断言**，worker_task_concurrency **3场景12断言**，worker_task_http **8场景**。最终全量CTest **37/37、退出0、177.35秒**，原34项全部保留并实际回归通过，没有以删除失败用例获取通过。
- 新真实HTTP **80次请求、88次独立16表全列指纹检查**，真实API-18派单开始：拒单样本order5/assignment1/version5，负载1→0、REFUSED历史/快照保留、转派失效、拒单理由及请求ID写日志；接单样本order6/assignment2/version5，IN_PROGRESS/accepted_at一致，负载1→1且不增派单。参数边界、撤销/过期/停用、非本人/历史/管理角色拒绝、三组同步并发只一200另一409、7种SQL写入故障回滚、真实忙锁503均通过。integrity_check=ok、foreign_key_check为空。证据在忽略的backend/build/Testing/w7d1-http-evidence.json，临时账号/Token不打印不提交；不累计重跑数字。
- 新前端测试 **28/28**，加原154项，全量 **182/182、退出0**；包含真实SFC setup/watch/模板编译及模拟生命周期，不冒充浏览器。真实前端API/flow经Vite/Drogon/SQLite验证成功与服务器已提交后客户端注入丢回执的unknown_result、不重发；不是实际现场断网。npm.cmd run build退出0，JS785.76kB/gzip254.97kB，>500kB chunk警告保留。
- 独立在自有随机端口运行正式后端（数据库路径为空）和Vite，实际 scripts/smoke.ps1 直连/代理各两次200 JSON、不同request_id及首页HTML均通过、退出0；健康检查仍不依赖SQLite。自有测试服务停止、随机端口释放，临时数据库/凭据清理，未修改用户演示数据库。
- 保留首次失败与修复：C++测试缺少正确domain校验头；两条前端API夹具缺request_id；HTTP转派夹具列名误写expected_version后修正真实order_version；非法surrogate在JSON层应400而非字段层422。以上测试保留并复验，不修改Schema或既有安全校验来迁就夹具。
- **本轮真实浏览器尚未验证**：computer-use native pipe不可用、Browser Use host unavailable/清单空，无页面点击、Console、390px或截图；报告列出人工接单/拒单/隔离/故障/窄屏步骤，等待补验。保留SQLite单写者/客户端先超时未知结果、内存Token、离线撤销不确定、无完整限流、JS安全整数与跨机器/压测/物理故障未验证等风险。
- 修改10/新增15/删除0，共25文件；工作区未暂存，未修改冻结文档、Schema、seed、Policy、状态机、W6算法或AI。两份W1-D3交接文件未动，本轮无其他无关未跟踪文件；产物忽略核查通过。git diff --check及cached --check退出0，15项新增逐项独立空白检查无诊断，全部25项UTF-8严格解码/末尾换行通过；原34项CTest注册名核对无遗漏。完整范围见 `docs/W7-D1-acceptance-report.md`；无git add/commit/push/merge。
- **CampusFix W7-D1 接单/拒单及维修员任务工作台实现与自动化验证已完成，等待独立源码审查；未进入W7-D2。**真实浏览器人工验收仍待补。

## 2026-11-01 W7-D2：转派申请、审批、到场/进度及最小授权查询

- 起始feature/w7-workflow，实际git fetch后HEAD与origin/feature/w7-workflow同为 `023c24646f13af655cb4d3907a59d7cd7900218a`；W7-D1已提交，工作区/暂存区干净，无reset/stash/覆盖。先发现既有时间线不能可靠发现真实待审申请、DISPATCH-only sysadmin不能借机读取完整详情的缺口，暂停后由用户正式批准D-021/API-34。
- decisions新增已接受D-021，最小同步SRS第49接口索引、API-34完整查询/白名单/错误契约、FR-008及UI/API映射；保留既有决策与冻结历史。未改Schema、seed、原权限Policy/状态机、W7-D1后端、W6算法/权重、AI或依赖版本；无新增库。
- 先实现并独立实测API-34：mine只按服务端requested_by读取worker本人历史申请及公开审批结果；managed按AREA_SCOPE或sysadmin DISPATCH自身范围，仅真实PENDING/当前有效派单/合法状态，独立于BUSINESS_READ与INTERNAL_NOTE_READ。专用query_only连接，同一只读快照认证/计数/分页，参数绑定、重叠区域去重、稳定排序，order_version取当前工单，不取申请快照；跨区筛选与不存在统一空结果，不暴露联系方式/内部字段。初轮80个SQLite断言及3场景32HTTP/56指纹通过后才继续写接口；最终增补为89断言及3场景34HTTP/60指纹。
- API-21申请、API-22批准/拒绝、API-33到场/进度均单连接BEGIN IMMEDIATE重查身份、归属、范围、状态/版本，复用授权、状态机、事务和追加日志。申请/拒绝审批/到场/进度不改工单版本；批准结束原派单TRANSFERRED、派单version+1、工单回PENDING_DISPATCH并version+1，同事务写审批和日志，不自动创建新派单。保留旧assignment/dispatch_records；唯一PENDING申请及每派单唯一到场由原索引保护；int64溢出、旧申请/终态/失效/跨单均拒绝。
- 新增workflow_service **10场景114断言**、workflow_concurrency **8场景24断言**、transfer_query_service **89断言**；真实索引、各SQL阶段失败、事务内撤销授权均回滚16表；WAL临时库另一个连接在COUNT/items之间真实提交，本次只读快照仍一致、下次才可见。SQL故障与外部写入均明确测试注入，不声称覆盖断电等物理故障。
- 独立transfer_query_http **3场景、34请求、60次16表指纹**；workflow_http **10场景、156请求、156次16表指纹**（包含查询覆盖，不与前项/重跑累计）。真实Vite→Drogon API-18起步三链路：接单→到场/多进度只增加日志（其他15表指纹不变）；申请→API-34真实选项→拒绝，状态/版本/负载不变；申请后接单使当前版本4→5→API-34返回5→批准回待派版本6，原派单TRANSFERRED、真实累计负载5→4、快照保留，原负责人失去进度权，管理员可重新GET推荐但不自动派单。
- 两管理员approve/approve、approve/reject、reject/reject以及申请、到场、审批/拒单、审批/接单、审批/进度共8组独立连接竞争；由第三连接锁定并确认两个BEGIN尝试后释放。每申请仅一个最终审批结果；进度先追加再批准可合法串行双成功，批准先结束时原负责人403，不强行把所有并发都当版本冲突。HTTP忙锁503不留下数据；客户端丢弃真实已提交POST回执仍未知，仅GET核实不重放。负例中的跨区、历史、终态由虚构数据库夹具构造，不冒充W7-D3完工接口。
- Vue新增任务内转派/到场/进度及公开时间线、独立本人历史申请区、管理员真实审批队列。DISPATCH-only不会调用详情/内部时间线；必须选择API-34实际申请和当前版本再二次确认。422留草稿、409清旧操作、401/403/404清子组件及父页面数据、超时/5xx保持未知；身份/上下文generation与读序号分离，操作ticket避免旧POST释放新锁。到场当前页成功后禁用，重进页面不从旧公开日志臆测当前派单资格，服务端唯一索引仍保证不重复。
- MSVC Debug完整构建退出0，最终无新增编译/链接警告；CMake复用原依赖缓存。最终全量CTest **42/42、失败0、退出0、253.35秒**，原37项全部保留并执行；此前268.01秒通过的一轮不累计。原认证/权限/Schema/seed/工单/派单/本地替身AI等回归通过，不是本轮真实DeepSeek测试。
- 原前端182项保留，新增 **35/35**，全量 **217/217、退出0**；包含实际SFC setup/watch/模板编译和模拟生命周期，不冒充浏览器。npm.cmd run build退出0，JS806.82kB/gzip259.64kB，保留>500kB chunk非阻塞警告。独立正式后端（无数据库路径）+Vite随机端口执行既有W2 smoke.ps1退出0，直连/代理JSON与首页通过，健康检查仍独立于SQLite；自有进程/端口及临时库/凭据已清理，无用户数据库操作。
- 首次失败历史：新增C++测试缺头文件C3861、不同类型同auto声明C3538均最小修复；旧SFC测试夹具需补新增组件注入，原断言不删；新测试发现只读核实更改读取序号导致未知结果共享锁未设置，分离contextGeneration后原失败用例及全量通过。
- **真实浏览器/桌面/390px/Console仍未验证**：按computer-use技能检查，native pipe不存在、browser host unavailable、清单空；没有点击、截图或用户人工结果。报告给出人工补验步骤。保留SQLite单写者/5秒超时未知结果、无跨刷新幂等回执、内存Token、离线撤销不确定、限流与大chunk、跨机器/压测/物理故障未验证等风险。
- 完整范围修改15/新增25/删除0，共40文件，全部未暂存，无无关未跟踪项。两份W1-D3交接文件未改；构建/缓存/数据库/WAL/证据文件仍被忽略，无跟踪产物。完整清单、命令、HTTP与SQLite证据及最终Git空白/编码核查见 `docs/W7-D2-acceptance-report.md`。不git add/commit/push/merge。
- **CampusFix W7-D2 转派申请、管理员审批及维修进度事件实现与自动化验证已完成，等待独立源码审查；未进入W7-D3。**浏览器人工验收仍待补。

### W7-D3 — 维修结果提交与待验收流转（2026-11-01）

- 起始分支feature/w7-workflow、HEAD `f78c992c35e7d79e576ea64ff7f66caef1b2b7df`；fetch成功，与远端一致，工作区/暂存区干净。依据FR-009、API-23、T-19以及既有冻结决策实施，不改Schema/seed、冻结文档、权限/调度/AI语义，不实现API-24/25/27/28。
- 新增CompletionService/Repository，扩展既有WorkflowController及前端工作台。BEGIN IMMEDIATE内重认证、Action::Complete和当前有效已接单归属、状态/版本校验；保存完整结果、PENDING_VERIFY和工单version+1、派单result_submitted_at/version+1；同事务将该派单PENDING转派申请SUPERSEDED、公开系统原因/resolved_at、reviewed_by=NULL；追加真实actor/assignment/request_id/UTC及完整public_note的ORDER_COMPLETED，提交前复核权限与保存不变量。所有SQL参数绑定、条件更新/行数检查，int64溢出/无效时间拒绝。有效派单不结束、实时负载不释放、原派单/调度历史保留，rework_count和completed_at不改。
- 后端新增completion_service **7场景95断言**、completion_concurrency **5场景20断言**、completion_http；全量原42项均保留，最终CTest **45/45、失败0、退出0、279.45秒**。完整MSVC Debug构建退出0，无新增编译/链接警告；CMake复用既有Drogon1.9.13#2/SQLite3.53.4#2/libsodium1.0.22#1缓存，不升级/下载依赖。
- 真实Vite→Drogon→临时SQLite：**8场景、136HTTP、175次独立16表全内容指纹**，不与重跑累计。正常链路通过真实创建→提交→审核→派单→接单→到场/进度→complete；样本工单5版本5→6、PENDING_VERIFY，派单1仍有效、实际负载1→1；原报修人timeline读完整结果，其他报修人和无业务读取授权sysadmin为404；API-10结构不扩展。另工单6申请1在结果提交事务内SUPERSEDED、resolved_at等于最终提交时间，旧审批409、mine历史可见、managed移除。
- 两真实BEGIN先被第三连接锁阻塞再释放，覆盖complete/complete、批准转派、进度、申请（另Service覆盖拒绝审批）；仅一个完成日志，审批先结束派单时旧负责人403，进度/申请在完成前可合法串行成功，不伪称所有竞争必为409。完整回归后又实跑5场景20断言，包含第二个complete胜出，退出0。
- 四写入阶段HTTP SQL ABORT失败全部回滚16表；Service同时覆盖RAISE(IGNORE)零行修改/日志插入与事务末尾账号停用，禁止部分提交。真实SQLITE_BUSY503指纹不变。客户端丢弃真实已提交回执仍unknown_result，仅GET核实、不自动重放。临时库integrity_check=ok、foreign_key_check为空，自有进程/随机端口及临时凭据/数据库清理，未操作用户库。
- 合法重提兼容：保留旧ORDER_COMPLETED全文/时间/派单，更新最新摘要和result_submitted_at，complete不加返工次数。明确使用隔离SQL夹具模拟未来返工前置，不把它冒充API-24已实现或已做端到端验收。
- Vue沿用共享workflowGate，新增结果确认框、Unicode实时计数与不立即释放负载提醒；成功严格核验200统一结构/request_id/工单/版本/全文后重新GET公开时间线和PENDING_VERIFY只读任务（current_only=true）。422保留草稿，409清旧选择/父任务，401/403/404清未授权数据，网络/5xx/异常200未知且仅GET核实；跨任务互斥、身份/卸载generation与ticket防迟到覆盖/释放新锁。无客户端假写状态、无未实现的验收/评价按钮。
- 前端原217项保留，新增24，完整 **241/241、退出0**，含实际SFC生命周期/共享锁与编译后的OrderTimeline真实Vue SSR结果全文/HTML转义，不冒充浏览器。Node24.18.1、npm11.16.0，Vite生产构建退出0、JS809.13kB/gzip260.23kB；保留原>500kB chunk警告。通过实际Drogon/Vite运行既有W2 smoke.ps1退出0，直连/代理200JSON和首页通过。
- 失败历史保留：首次沙箱MSBuild FileTracker MSB4018/E_ACCESSDENIED，授权本机同工具链构建解决；新增测试C2678字符串/Json比较修复，首次测试程序缺失曾Not Run，后重建执行通过；新并发测试错误假定第一个complete胜出，修正为校验任一胜出且唯一完成日志，未删除场景或放宽业务断言。
- **真实浏览器桌面、390px、Console及页面交互未验证**。computer-use技能检查本机窗口枚举可用，但专用浏览器连接为空/host unavailable；未输入用户密码、未登录或截图，不虚构用户人工结论。完整人工补验步骤见 `docs/W7-D3-acceptance-report.md`。
- 结项修改11/新增8/删除0，共19文件全部未暂存；git diff --check、暂存区空白检查及全文件严格UTF-8/末尾换行、新增文件空白检查通过。保护基线无变化，构建/依赖/SQLite/WAL/证据均忽略，无无关未跟踪项；8848/5173无监听。保留内存Token、未知回执、离线撤销、限流/跨机器/物理故障/大chunk等既有风险。完整清单与真实HTTP/SQLite证据见验收报告。
- **CampusFix W7-D3 维修结果提交与待验收流转实现与自动化验证已完成，等待独立源码审查；未进入W7-D4。**未git add/commit/push/merge，等待人工及独立源码审查。

### W7-D4 — 原报修人验收与返工（2026-11-01）

- 起始分支feature/w7-workflow，HEAD `6972bcf4bc80b9cd96d3fd4f57f026119b8add90`；fetch成功，本地/远端一致，工作区和暂存区干净。仅实现FR-009/API-24/T-20/T-21，不开发W7-D5评价、关闭/归档，不改冻结Schema/seed、契约或W6调度/评分/容量口径。
- 新增VerificationService/Repository，现有WorkflowController复用严格JSON解析/统一JSON/request_id注册verify。主要角色reporter/worker且数据库reporter_id=有效Session用户才有资格；复用D-017及VerifyPass/Fail，不信任客户端身份/状态，不给当前/历史维修员或具有管理授权的sysadmin代验收能力。
- 单连接BEGIN IMMEDIATE内重认证、归属/状态/版本核查、真实有效已接单派单/结果/时间校验；通过条件更新COMPLETED/工单version+1/completed_at，原派单end_reason=COMPLETED、ended_at、version+1，实际负载-1；返工只IN_PROGRESS/工单version+1/rework_count+1，保留原派单整行、摘要/接单/结果时间和负载，不重新推荐或派单。日志真实actor/assignment/UTC/request_id，返工公开reason及白名单rework_count；行数、int64、时间顺序和提交前认证/数据库不变量复核失败均回滚。没有自动关闭、评价或调整评分。
- 新增verification_service **7场景100断言**、verification_concurrency **5场景15断言**，包括角色/归属、D-017、完整多轮返工重提、时间和历史、W6实际负载/历史效率样本、溢出/不一致数据、所有写入节点ABORT/IGNORE零行失败、事务末尾停用账号。原45项CTest全部保留，新增3项共48；首轮全量 **48/48、退出0、282.35秒**，随后加强每轮返工派单整行不变断言，最终全量复核结果见验收报告结项。Debug完整构建退出0，无新编译/链接警告，既有依赖缓存复用，无安装或升级。
- 真实Vite→Drogon→临时SQLite **8场景、289HTTP、216次独立连接16表全内容指纹**，不与重跑累计。全部正常前置经实际创建→提交→审核→派单→接单→维修提交，不直接UPDATE伪造API操作。样本工单5通过版本6→7、同派单结束COMPLETED、completed_at/ended_at一致、负载1→0；工单6实际A→返工→B→通过，版本6→7→8→9，返工次数1，负载1→1→1→0，A/B与原因/全部旧派单及调度历史保留，最终result_submitted_at来自B，现有W6适配器真实效率样本分钟数匹配最终提交。另worker02本人报修由worker01维修，worker01代验收403，worker02可返工并最终通过，符合D-017。
- 两个请求均实际尝试BEGIN后由第三连接释放锁，覆盖通过/通过、通过/返工、返工/返工、verify/complete及verify/进度；每单一条验收结果日志。前3组一个200/一个409；派单结束后worker失去写权限的403保持原语义。HTTP通过路径工单/派单/日志及返工路径工单/日志5节点故障500均全表回滚；真实BUSY503指纹不变；服务端已提交但客户端丢失回执仍未知，只GET核实、不重发。临时库integrity_check=ok、foreign_key_check为空，测试自有进程/随机端口与库/凭据清理，不操作用户演示库。
- 新增OrderVerifyPanel/verification API/verifyFlow。原报修人资格使用服务端reported分页（复用ownsReportedOrder加严格页/ID核验）；完整公开历史扫描、页数量/total/UTC+ID排序/去重及前后真实详情版本栅栏确认本轮最新ORDER_COMPLETED，不能用第一页旧结果或私有字段。上限50页/5000条，超限/错误/漂移失败关闭。完整公开返工事件计数用于严格验证回执次数，数据库仍权威。真正二次确认，500Unicode原因，提交锁定；422留草稿，409清旧上下文/父详情，401/403/404清资料，异常200/网络/5xx未知不重发；成功重读详情/时间线，账号/路由/卸载防迟到覆盖。返工后原WorkerTasks/完整API-23继续可用。
- 前端原241项全部保留，新增29，全量 **270/270、退出0**，含实际SFC、归属和100条边界/同时间戳、本轮结果/响应字段、未知与身份切换；不是浏览器E2E。Node24.18.1/npm11.16.0，Vite生产构建退出0，JS819.55kB/gzip263.35kB，保留原>500kB警告。真实HTTP进程内启动服务并执行既有W2 smoke.ps1退出0，直连/代理JSON及首页通过。
- 首次失败及修复保留：新测试曾将结束派单后的旧worker固定预期409、范围外管理员预期403，按原Policy真实403/404修正新用例，未改授权；并发返工测试漏reason出现422/422，补齐合法竞争输入，保留缺原因负例。未删除/放宽原断言。
- **真实浏览器桌面/390px/Network/Console未验证**。依computer-use技能检查连接返回apps/browsers空及host/native pipe不可用；没有登录/截图/用户反馈，不冒称人工通过。保留内存Token、未知回执、离线撤销、限流/压测/跨机/物理故障/大chunk等风险，新增有界历史超限关闭入口的限制已记录。
- 本轮预期修改6/新增11/删除0，共17文件，全部未暂存；完整源文件、HTTP/SQLite证据与最终Git、编码及全量结果见 `docs/W7-D4-acceptance-report.md`。不git add/commit/push/merge，不进入W7-D5，等待独立源码及人工验收。
- 最终结项：加强每轮派单整行不变断言后完整CTest **48/48、失败0、退出0、280.02秒**，前端最终 **270/270** 和构建通过；身份失效/时间线401/403/404同时清除旧退回原因、分页和操作提示。真实HTTP8/289/216、W2冒烟0、完整性ok/FK空；原45项CTest核对遗漏0。Git6修改/11新增/0删除全部未暂存、空白/UTF-8/换行及保护基线核查通过，8848/5173无监听。上述首次失败历史保留，不累计重跑数字。**CampusFix W7-D4 原报修人验收与返工实现与自动化验证已完成，等待独立源码审查；未进入W7-D5。**浏览器人工验收待补。
