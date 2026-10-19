# W1-D1 Windows 本地操作手册

**任务目标**：创建/打开工程目录，验证需求与 AI 规范文件，然后做首次 Git 提交；**不写业务代码**。

## A. 放置文件

1. 将压缩包解压，得到 `CampusFix` 文件夹；请不要把 `CampusFix` 解压到同名 `CampusFix` 内导致双层目录。
2. 将 `CampusFix` 移动到自己选择的工作区，例如 `%USERPROFILE%\Projects\CampusFix`。
3. VS Code → 文件 → 打开文件夹 → 选 `CampusFix`，终端 → 新建终端，确保终端类型是 **PowerShell**。

## B. 检查当前路径与工具

```powershell
Get-Location
git --version
Get-ChildItem -Name
```

预期当前路径最后一级为 `CampusFix`；根目录下存在 `README.md`、`AGENTS.md`、`docs`、`backend`、`frontend`。

## C. 验证文档

```powershell
Test-Path .\README.md
Test-Path .\AGENTS.md
Test-Path .\docs\SRS.md
Test-Path .\docs\development-plan.md
Test-Path .\docs\decisions.md
Test-Path .\docs\flows.md
Select-String -Path .\docs\SRS.md -Pattern '^# 45\. 版本分级与范围冻结'
```

应依次返回六个 `True`；最后一条应找到第 45 章标题。注意 `.gitkeep` 是隐藏的空目录追踪占位文件。

## D. Git 初始化与首次提交

```powershell
git init -b main
git status --short
git add .
git status --short
git commit -m "docs: initialize CampusFix requirements baseline"
git log -1 --oneline
git status
```

预期：`git log` 包含这条提交，`git status` 显示 `working tree clean`。

首次提交若提示 `Author identity unknown`，只需配置自己的作者信息：

```powershell
git config --global user.name "你的显示名"
git config --global user.email "你的邮箱"
```

随后重试 `git commit ...`。GitHub 账号和推送是后续可做的步骤；今天先完成本地提交。

## E. 排除目录检查

```powershell
git check-ignore -v .\frontend\node_modules\test.txt
git check-ignore -v .\uploads\sample.png
git ls-files
```

前两条应指出 `.gitignore` 匹配规则；最后一条列出已提交的文档与 `.gitkeep`。

## F. 失败排查

- `git` 无法识别：尚未安装 Git for Windows，或安装后没有重新打开终端。下载官网：https://git-scm.com/downloads/win。
- `git init -b main` 报不支持：更新 Git，或用 `git init` 后执行 `git branch -M main`。
- 路径不对：VS Code 的“打开文件夹”必须指向包含 README 的那个 `CampusFix`，避免双层目录。
- `Test-Path docs/SRS.md` 返回 False：请重新解压完整压缩包，不要从空目录开始跳过需求文档。
- 不要执行 `cmake --build`、`npm run dev`：现阶段代码与依赖还不存在。

## G. 提交给 AI 检查时提供的信息

粘贴 `git status`、`git log -1 --oneline`、`Get-ChildItem -Name` 的结果即可。不要提交密码、Token 或真实个人联系方式。
