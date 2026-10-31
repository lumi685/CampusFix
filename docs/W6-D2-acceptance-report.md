# W6-D2：EligibilityFilter 硬性资格筛选验收报告

日期：2026-10-31。状态：实现与自动化验证完成，等待人工与独立源码审查。范围：FR-006/FR-007 的纯C++硬筛选基础，不代表完整推荐或派单业务已完成。

## 1. 起始基线与权威规则

- 分支：`feature/w6-dispatch`。
- 起始HEAD：`da6ba5a0c603b040126af8f61b4e098c9fae86c3`，`feat(dispatch): establish pure C++ dispatch domain and strategy interface`。
- 先核对工作区/暂存区为空，再执行用户要求的fetch：受限环境首次失败，原始错误为 `Failed to connect to github.com:443 after 20 ms: Could not connect to server`。获准在本机网络执行同一 `git fetch origin` 后退出0；HEAD与更新后的 `origin/feature/w6-dispatch` 完全一致。没有reset、清理、覆盖或切换分支。
- 阅读AGENTS、SRS第14/44～56章、W6计划、FR-006/007及TC-DSP、数据库设计、API-14～18、D-006/012/013/016、001迁移、Worker模型/Repository、W6-D1模型/策略/单测/CMake及验收报告。
- SRS 50.2、数据库设计及现有Repository确立当前有效技能、区域与实时容量口径；没有发现BUSY可作为普通接单候选的更精确定义，本轮按任务要求仅接受AVAILABLE。排班为P1；跨区明确授权来源尚未落实，本日不开放例外。

## 2. 完整变更文件清单

均位于 `G:\A_Study\CppProject\CampusFix`：

| 状态 | 路径 | 内容 |
| --- | --- | --- |
| 新增 | `backend/src/dispatch/EligibilityFilter.h` | 无状态纯计算入口 |
| 新增 | `backend/src/dispatch/EligibilityFilter.cpp` | 真实硬过滤及稳定结果输出 |
| 新增 | `backend/tests/unit/EligibilityFilterTest.cpp` | 直接测试真实过滤器，无伪造过滤结果替身 |
| 修改 | `backend/src/dispatch/DispatchModels.h` | 最小输入完备性标记和两个排除原因 |
| 修改 | `backend/src/dispatch/DispatchModels.cpp` | 排除原因显式枚举校验 |
| 修改 | `backend/CMakeLists.txt` | 既有纯库加入实现、新增一个CTest |
| 新增 | `docs/W6-D2-acceptance-report.md` | 本报告 |
| 修改 | `docs/progress-log.md` | 追加本日记录，不覆盖W6-D1历史 |

合计修改4、新增4、删除0，共8文件。`DispatchStrategy.h/.cpp`、`DispatchDomainTest.cpp`没有修改；原 `.gitkeep` 保留。冻结SRS/API/Schema/seed、身份/授权、审核、状态机、AI和前端均不在修改范围。

## 3. 输入模型最小调整

继续组合 `WorkerRecord`、复用 `WorkerSkill/SkillOption`，不复制技能、负载、区域或工单业务字段。

- `DispatchWorker.skillsLoaded=false`：只有服务端成功完整读取有效 `worker_skills` 与有效 `skills` 后才设true。true且空vector表示确认没有有效技能；false无论vector是否为空，过滤器均抛 `std::invalid_argument`，不得伪报MissingSkill。
- `LocationAncestry.verified=false`：只有服务端核实完整有效位置链、各节点类型、校区/建筑归属后才设true；链按直接父节点到根校区排列。false涵盖未加载、不完整、停用链或读取失败，不等于已证实跨区。
- 两标记追加于原结构，默认失败关闭；不是数据库字段、HTTP参数或管理授权。原W6-D1通用输入校验仍允许构造不完整快照，既有测试策略保持兼容；真实EligibilityFilter额外要求完备性。
- 纯函数不能自己证明verified的真实性。当前测试使用可信的虚构服务端快照；生产适配器尚未实现，不得由客户端或管理界面DTO自行设置这些标记。

## 4. 筛选入口、顺序和排除原因

入口：`EligibilityFilter::filter(const DispatchWorkOrder&, const vector<DispatchWorker>&)`，返回现有 `DispatchResult`。

先调用 `validateWorkOrder`，再对所有人员调用 `validateWorker`，检查skillsLoaded及重复worker ID。非法工单/人员/未加载技能使整个调用明确失败；不会返回部分推荐。待派前提为PENDING_DISPATCH、正数64位ID/版本、审核分类对应技能、合法approvedUrgency等，不能用客户端/AI提示替代。

排序只对const输入指针副本按数值worker ID升序进行。每位人员依次收集：

| 顺序 | 条件不满足 | 原因 |
| --- | --- | --- |
| 1 | 账号状态不是ACTIVE | `AccountInactive` |
| 2 | 账号主要角色不是worker | `AccountRoleMismatch`（新增） |
| 3 | 人员档案active=false | `WorkerInactive` |
| 4 | availability不是AVAILABLE | `Unavailable` |
| 5 | 已完整加载的技能中没有requiredSkill.id | `MissingSkill` |
| 6 | 类别或所需技能任一要求资质，而对应技能未持证 | `MissingCertificate` |
| 7 | 区域无法核实，或已确认不覆盖 | `LocationUnverified`（新增）或 `OutsideServiceArea` |
| 8 | activeTasks >= capacity | `AtCapacity` |

所有独立事实完整收集，不提前返回第一个失败；固定检查顺序、无重复、无私人信息。缺少对应技能时只报MissingSkill，不额外猜测该技能证书情况；两个资质要求同时成立也只报一次MissingCertificate。紧急度/值班标记不绕过任何约束，技能等级1～5不另设最低3级。

新原因追加于原枚举，旧原因数值未改变；`validateResult`改为显式switch允许已知原因，仍拒绝Unknown及非法枚举值。未加强通用校验的完整划分或评分语义，避免破坏D1测试策略。

## 5. 区域判定及未知数据

1. 工单和人员区域都需verified及campusId；复用原ID/重复/自指/成员检查，再检查根校区位于祖先链末端、建筑位于校区下方。声明verified也不能跳过这些结构检查。
2. 区域类型从可信的类型元数据确定：`serviceAreaId == campusId`为校区级，`serviceAreaId == buildingId`为楼宇级。不能确定是这两类则LocationUnverified，不把正数ID当成资格。
3. 校区级可覆盖同校区内建筑/楼层/房间，也可覆盖有完整校区链的校园公共区域或校区节点本身。
4. 楼宇级必须同校区且工单buildingId与serviceAreaId一致；同校区其他楼宇仍OutsideServiceArea。同楼宇不同楼层/房间可覆盖。楼宇级判定所需buildingId缺失时LocationUnverified。
5. 两侧完整且校区不同为OutsideServiceArea；同一建筑却指向不同校区是相互矛盾元数据，返回LocationUnverified，不猜测具体越区事实。
6. 同一位置ID也不能在元数据未核实时直接放行。没有跨区放宽字段、管理授权或应急例外。

## 6. 完整划分和评分边界

在通过结构/完备性检查的输入上，一次遍历每个唯一worker，恰好向eligible或excluded之一追加一项；不存在遗漏或两边重复。最后复用validateResult。每组按worker ID升序输出，仅为筛选展示顺序，不是W6-D3得分/负载排名。

候选workerId、activeTasks、capacity直接复制同一个输入WorkerRecord；score、rank、历史样本计数均保持nullopt。不合格结果类型没有score/rank。合格理由只说明通过硬约束；没有实际加权分数、冷启动60、历史公式、Top3或真实推荐排名。

## 7. 实测命令与结果

使用现有Visual Studio/MSVC、x64-windows、C++17及已有依赖，无新增vcpkg/npm依赖，无baseline/工具链切换：

```powershell
git fetch origin
git branch --show-current
git status --short
git log -1 --oneline
git rev-parse HEAD
git rev-parse origin/feature/w6-dispatch
$cmakeBin = 'D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
& "$cmakeBin\cmake.exe" --build backend/build --config Debug --parallel 4
& "$cmakeBin\ctest.exe" --test-dir backend/build -C Debug -R '^dispatch_(domain|eligibility)$' -V
& "$cmakeBin\ctest.exe" --test-dir backend/build -C Debug --output-on-failure
& "$cmakeBin\ctest.exe" --test-dir backend/build -C Debug -N
git diff --check
```

| 项目 | 本轮结果 |
| --- | --- |
| CMake配置/生成 | 既有缓存自动重新生成成功，依赖均already installed，无下载或升级 |
| 完整MSVC Debug | 本机授权环境构建成功；最终补显式utility头后再次完整构建成功，未见编译/链接警告 |
| 定向两项CTest | 2/2通过，退出0，总0.15秒；原dispatch_domain为15场景/104断言，新dispatch_eligibility为41场景/1019断言 |
| CTest注册 | 实际CTest -N为29；对照起始HEAD原28个名字全部保留，仅新增dispatch_eligibility |
| 全量CTest | **29/29通过、失败0、退出0、158.07秒**；包括原28项，最终重编译后的两项调度测试均通过 |
| 纯模块依赖 | dumpbin仅见MSVC运行库及KERNEL32，无Drogon/SQLite/WinHTTP/AI运行依赖 |
| Git检查 | git diff --check退出0；4个新增文件另外执行no-index空白检查，无空白诊断，UTF-8及末尾换行检查通过 |

新41场景覆盖任务列出的30类要求，并额外验证可信加载标记、同建筑跨校区矛盾、链顺序、校园公共区域及空候选解释。1019个断言包含六人混合输入全部720种排列，不等于1019个独立业务场景。混合输入精确核对2个合格、技能失败、区域失败、满容量和多重失败；所有ID和原因数组、输入不变、64位大ID均直接检验实际过滤器输出。

本轮构建与定向/全量测试均无编译/业务断言失败；没有删除或放宽失败用例。fetch的受限环境失败及本机成功记录保留，不记为算法失败。最终单测原始输出保存在被忽略的 `backend/build/Testing/Temporary/LastTest.log`，其中原单测15/104、新过滤器41/1019，未累计重复执行数字。

## 8. 无副作用、未验证项与后续前提

- 新过滤器只用C++标准库及现有纯数据模型，无数据库连接、HTTP/网络、全局可变状态、业务文件访问或业务表写入，没有接入生产服务器。只有单测计数器/控制台输出存在于测试程序。
- 没有新Controller/Repository/DispatchService、assign事务、WeightedScoreStrategy、前端调度页面或P1策略；没有调整权重或状态机。
- 全量既有回归包含隔离数据库与本地HTTP/假模型测试，不能冒充新调度HTTP、真实DeepSeek或人工浏览器验收。今天未运行前端测试/build、独立W2冒烟、额外AI-D3集成脚本、真实浏览器/模型、跨机器或性能压测。
- W6-D3需在本过滤器后实现冻结评分/冷启动及同分规则，不能复活excluded；历史nullopt仍是未知，不自动视作零样本或60分。没有未完成评分用例被标记通过。
- W6-D4数据适配器需从数据库核实审核分类/紧急度、账号/人员有效性、完整有效技能、有效位置祖先与类型及实时负载；成功后显式设置完备标记。读取失败须报错或关闭资格，不能伪造空数据。现有管理Repository未包含所有调度输入，不能直接当作完整调度快照。
- 位置标记是可信服务端输入契约，不是纯函数独立验证数据库的证明；位置停用和链完整性须由未来适配器核实。
- 确认派单必须在短写事务内重读状态/版本/资格/实时容量并重新运行规则；本日快照通过不保证稍后仍有空位。跨区来源未确定，保持关闭，不借用管理授权。

## 9. 最终Git交付状态

最终实际范围为修改4、新增4、删除0，共8文件，与第2节一致；暂存区为空，无无关未跟踪文件。HEAD仍为起始da6ba5a，与本次fetch后的远端一致。

`git diff --check`退出0；4个新增文件的 `git -c core.autocrlf=false diff --no-index --check -- NUL <file>` 没有空白诊断（退出1仅表示相对空文件有差异）。UTF-8解码和末尾换行检查通过。Git提示模型文件下次可能LF转CRLF，不是编译警告或空白错误。

构建库/可执行文件、CMake缓存、CTest日志及数据库/WAL路径仍被Git忽略；本轮没有加入数据或凭据。未自动git add、commit、push或merge。当前无阻塞本日纯过滤器的冲突；数据适配、历史映射及跨区来源仍按第8节留待后续确认，不擅自新增规则。

**CampusFix W6-D2 EligibilityFilter 实现与自动化验证已完成，等待人工与源码审查；未进入W6-D3。**
