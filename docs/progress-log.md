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
