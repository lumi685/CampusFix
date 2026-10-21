# CampusFix —— 校园维修工单与智能调度系统

> 基于 C++17 / Drogon 与 Vue3 的前后端分离课程项目。W1 的 P0 设计基线已冻结；W2 目前只有可运行工程、健康检查及前后端连通，**尚未实现登录、工单、调度或数据库业务**。

## 1. 项目背景与目标

校园宿舍、教学楼和公共设施的报修通常面临记录分散、调度依赖人工、状态不可追踪的问题。CampusFix 计划实现从在线报修、审核、智能调度、人工确认，到维修处理、验收评价和归档统计的闭环。

**目标**：单人结合 AI 辅助，在约 10 周完成可演示、可测试、可复现启动的 Web 课程设计。时间计划是估计值而非保证。

## 2. 技术方案与范围

| 组件 | 当前选型 | W2 实现情况 |
|---|---|---|
| 后端 | C++17 + Drogon + CMake | 最小服务与 `GET /api/health` |
| 前端 | Vue 3 + Vite + JavaScript + Element Plus + Axios + ECharts | 欢迎页、开发代理、健康状态展示 |
| 数据库 | SQLite | 仅安装 C++ 开发依赖，尚未连接或建表 |
| 依赖管理 | vcpkg Manifest / npm lockfile | `backend/vcpkg.json`、`frontend/package-lock.json` |

角色规划为 `reporter` 报修人、`worker` 维修人员、`dispatcher` 调度管理员、`sysadmin` 系统管理员。业务主线为“提交报修 → 审核 → 推荐候选人 → 管理员确认派单 → 维修人员接单处理 → 报修人验收 → 评价 → 关闭归档”。后端是权限、数据范围和状态变化的最终裁判；当前页面中的角色导航只是占位，不具有业务权限功能。

- **P0 必须做**：四角色访问控制、报修与审核、技能和区域约束、候选推荐与人工派单、维修处理与验收、评价、基础附件与统计、操作历史、数据库与安全测试。
- **P1 有余力做**：站内通知、排班、超时提醒、策略切换、可配置权重、Docker、报表导出。
- **P2 本期不做**：大模型图片识别、全局路径规划、学校统一身份认证、短信与微信对接、自动验收。

详细范围以 `docs/SRS.md` 第 44～56 章及 W1 冻结设计为准，不能由 README 改写。

## 3. 已验证环境与首次准备

以下仅是在 **2026-10-21 的这台 Windows 机器** 上实际验证的组合，不代表其他机器已通过安装或部署测试：Visual Studio Community 2026 / MSVC 19.51、CMake 4.3.1-msvc1、vcpkg（`x64-windows`）、Drogon 1.9.13#2、SQLite C++ 包 3.53.4#2、Node.js 24.18.1、npm 11.16.0、Vite 8.3.5。其他机器请先确认对应工具链可用、Node 满足 `frontend/package.json` 所用 Vite 的要求。

1. 安装带 C++ 工作负载的 Visual Studio、CMake、Git、Node.js/npm，并准备官方 vcpkg。不要混用 MSVC 构建目录和 WSL/MinGW 工具链。
2. 后端依赖由 `backend/vcpkg.json` 的 `drogon`、`sqlite3` 及固定 `builtin-baseline` 声明。首次配置需要能访问官方 vcpkg registry；如果 GitHub 不可达，先修复网络，不要盲目升级基线或换未验证镜像。
3. 在项目根目录打开 Windows PowerShell。若 `cmake.exe` 不在 PATH，首次配置时向脚本传入其实际路径；若 `VCPKG_ROOT` 未设置，传入 vcpkg 根目录。脚本不会修改或覆盖环境变量：

```powershell
.\scripts\start-backend.ps1 -CMakePath "<cmake.exe 的完整路径>" -VcpkgRoot "<vcpkg 根目录>"
```

首次运行会以 Visual Studio 18 2026、x64、`x64-windows` 配置 `backend/build`，再编译 Debug 并前台启动。使用其他已安装 Visual Studio 版本时可传 `-Generator`，但其他组合尚未在本项目验证。如果已有有效的 `backend/build/CMakeCache.txt`，脚本优先复用其中的 CMake 路径和构建配置。构建缓存包含本机绝对路径，**不要复制到另一台机器复用**；新机器应从不含 `backend/build` 的仓库检出开始首次配置。

前端第一次安装（另开一个 PowerShell 终端）：

```powershell
Set-Location .\frontend
npm ci --cache .npm-cache
Set-Location ..
```

`npm ci` 使用已提交的锁文件。这里显式使用项目本地、被 Git 忽略的缓存，避免本机默认 npm 缓存目录权限问题；不会安装全局 npm 工具。

## 4. 两个终端启动与验证

两个终端均从 **项目根目录** 执行；脚本内部以自身所在目录定位仓库，不依赖当前工作目录。

```powershell
# 终端 A：后端，若首次配置请按上一节传入 CMake/vcpkg 路径
.\scripts\start-backend.ps1

# 终端 B：前端
.\scripts\start-frontend.ps1
```

脚本会检查 8848/5173 端口占用，服务在各自终端前台运行。后端只监听 `127.0.0.1:8848`；Vite 页面为 `http://127.0.0.1:5173/`，其 `/api` 开发代理转发到后端。两个健康检查地址：

- 直连：`http://127.0.0.1:8848/api/health`
- 经 Vite 代理：`http://127.0.0.1:5173/api/health`

服务均启动后，在第三个 PowerShell 终端执行：

```powershell
.\scripts\smoke.ps1
```

脚本验证直连与代理的 HTTP 200、JSON 响应结构及重复请求 ID，并检查前端首页。任一断言或连接失败返回非零退出码。它只读健康接口，不连接数据库或修改业务数据。

如需单独执行构建：

```powershell
& "<cmake.exe 的完整路径>" --build backend/build --config Debug --target campusfix_backend --parallel 4
Set-Location .\frontend
npm run dev
npm run build
Set-Location ..
```

在各服务所在终端按 `Ctrl+C` 停止。若终端关闭后端口仍被占用，先用 `netstat -ano -p tcp` 找到监听 PID，再用 `Get-CimInstance Win32_Process -Filter "ProcessId=<PID>"` 核对程序路径或命令行，**只对已确认属于本项目的 PID** 执行 `Stop-Process -Id <PID>`；不要批量结束所有 Node 或其他服务进程。

## 5. 目录与 Git 忽略

`backend/` 存放 CMake 与 Drogon 源码；`frontend/` 存放 Vue/Vite 源码；`docs/` 存放冻结设计和进度记录；`scripts/` 存放上述 PowerShell 操作入口。`.gitignore` 排除 `backend/build/`、`vcpkg_installed/`、`frontend/node_modules/`、`frontend/dist/`、`frontend/.npm-cache/`、本地 `.env`、数据库、日志和真实附件。`backend/vcpkg.json` 与 `frontend/package-lock.json` 应提交，以便复现依赖选择。不要提交真实个人信息、Token、密钥、数据库或上传图片。

## 6. 常见故障

| 现象 | 先检查 |
|---|---|
| GitHub / vcpkg registry 连接失败 | 先用 `git ls-remote https://github.com/microsoft/vcpkg.git HEAD` 检查官方仓库可达性；核对代理/网络，不反复下载或擅改 baseline。 |
| vcpkg 报依赖不存在 | 核查实际 toolchain、vcpkg 根目录、registry 与 `backend/vcpkg.json` 的 baseline；不要仅凭最终一句错误判断版本缺失。 |
| CMake 或 MSVC 路径无效 | 向后端脚本传 `-CMakePath` / `-VcpkgRoot`；旧 `CMakeCache.txt` 可能记录另一台机器的绝对路径。 |
| 8848 / 5173 已占用 | 用 `netstat -ano -p tcp` 确认 PID 与进程身份；只停止确认属于本项目的进程，或先关闭已有终端。 |
| npm 缓存 `EPERM` | 使用 `npm ci --cache .npm-cache`；不要直接修改系统级 npm 配置。 |
| 页面显示 Vite 代理 HTTP 502 | 先访问后端直连健康接口，确认 Drogon 在 `127.0.0.1:8848` 运行；再检查 Vite 代理配置及端口。 |
| 受限环境中 MSBuild FileTracker `E_ACCESSDENIED` | 在有权限的 Windows 本机终端重试相同构建命令；不要以禁用警告或更换编译器掩盖问题。 |

## 7. 文档入口

- [需求基线](docs/SRS.md)
- [系统架构](docs/architecture.md)
- [开发计划](docs/development-plan.md)
- [决策日志](docs/decisions.md)
- [业务流程](docs/flows.md)
- [API 契约](docs/api-contract.md)
- [进度记录](docs/progress-log.md)

参考：[Drogon 官方仓库](https://github.com/drogonframework/drogon)、[Vue 官方快速开始](https://vuejs.org/guide/quick-start.html)、[Git 文档](https://git-scm.com/docs/git-init)。
