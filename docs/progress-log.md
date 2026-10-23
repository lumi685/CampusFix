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
