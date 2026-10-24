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
