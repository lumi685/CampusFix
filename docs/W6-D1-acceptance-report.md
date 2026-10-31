# W6-D1：智能调度核心模型与策略接口验收报告

记录日期：2026-10-31。阶段状态：实现与自动化验证完成，等待人工与独立源码审查；未进入 W6-D2。

## 1. 基线与实施范围

- 起始分支：`feature/w6-dispatch`。
- 起始 HEAD：`7ed3a33cab601c45c797ac65039611ccc2235e25`。
- 开始前实际检查 branch/status/log：分支、HEAD 符合用户指定值，工作区及暂存区为空。没有清理或覆盖原有修改。
- 对应 FR-006/007/008/013、D-006/D-013：本日仅建立后续调度所需的纯 C++17 类型、策略接口及结构校验，不宣称这些 FR 已全部实现。
- 已依照 AGENTS.md 阅读 SRS 第14、44～56章及相关计划、需求、数据库、API-17/18、决策、流程、权限与现有模型/测试。实际没有 `docs/dispatch-design.md`，未创建新的冻结设计来替代 SRS。
- 不修改冻结文档、Schema、seed、状态机、REST 接口、认证/授权、AI 或前端行为。新增库没有接入生产 Controller/Service，不进行真实派单。

## 2. 实际文件清单

以下路径相对 `G:\A_Study\CppProject\CampusFix`：

| 类型 | 文件 | 用途 |
| --- | --- | --- |
| 新增 | `backend/src/dispatch/DispatchModels.h` | 工单/人员输入、原始历史、分项分数、候选/排除结果 |
| 新增 | `backend/src/dispatch/DispatchModels.cpp` | 关键输入及结果结构校验 |
| 新增 | `backend/src/dispatch/DispatchStrategy.h` | 可替换的抽象策略接口 |
| 新增 | `backend/src/dispatch/DispatchStrategy.cpp` | 公共调用入口、输入/输出结构保护 |
| 新增 | `backend/tests/unit/DispatchDomainTest.cpp` | 测试专用策略及独立单元测试 |
| 修改 | `backend/CMakeLists.txt` | 独立静态库、单测目标和 CTest 注册 |
| 新增 | `docs/W6-D1-acceptance-report.md` | 本报告 |
| 修改 | `docs/progress-log.md` | 追加本日实际执行记录 |

合计修改2、新增6、删除0，共8个文件。原 `dispatch/.gitkeep` 保留。没有自动暂存或提交。

## 3. 模型与现有业务模型关系

调度命名空间为 `campusfix::dispatch`；业务 ID 使用 `std::int64_t`，不缩窄为32位。输入是后续由服务端组装的临时快照，不是另一套持久化实体或数据库 DTO。

| 类型/字段 | 权威来源与关系 | 今日边界/后续映射 |
| --- | --- | --- |
| `DispatchWorkOrder.id/version/status/categoryId/locationId` | `work_orders`；与现有 `WorkOrder` ID类型一致 | 不修改 `WorkOrder`；审核可调整分类，必须读审核后的实际 `category_id` |
| `approvedUrgency` | `work_orders.approved_urgency` | 现有 `WorkOrder` 读模型未提供此字段；未来显式查询，不能用申报紧急程度替代 |
| `requiredSkill`、`categoryRequiresCertificate` | 复用 `SkillOption`；分类必需技能/资质及技能资质配置 | 后续联查 `fault_categories`、`skills`，本日不实现资质硬过滤 |
| `DispatchWorker.record` | 直接组合现有 `WorkerRecord`，其中技能直接复用 `WorkerSkill` | 不重复定义权威容量、技能、人员有效性、在岗状态、区域、负载和紧急值班字段 |
| `accountStatus/accountRole` | `users.status/role`，现有 `WorkerRecord` 不含这两项 | 后续联查用户；不能由存在人员档案推断账号有效且角色正确 |
| `LocationAncestry` | `locations` 的祖先、校区、建筑信息；自身节点ID在工单/人员记录中 | 仅校验正数/不重复/不自指及元数据一致性，不证明数据库层级或授权；缺失不是授权 |
| `record.activeTasks` | 有效 `assignments`（`ended_at IS NULL`）实时计数 | 与现有 WorkerRepository 负载口径一致；未来派单事务仍须重新检查 |
| `DispatchHistory.efficiency` | 冻结数据库设计6.5：有效完成派单、接单时间、最后维修提交事件和目标时长 | 仅原始样本：派单ID、目标分钟数、实际分钟数；不读取数据库或计算效率分 |
| `DispatchHistory.rating` | 冻结6.5：最终维修人员所属的有效评价 | 仅评价ID及1～5星原始值；不提前计算评价分 |

历史两项分别采用 `optional<vector<...>>`：`nullopt` 是未加载，已加载空集合才是已核实零样本。最多20个样本的结构限制与冻结规则一致；本日没有查询、选择最近样本、冷启动60分、归一化或取整实现。原始耗时允许0，未来按冻结公式实施 `max(1, actual_minutes)`，不在模型中虚构处理结果。

默认工单ID/版本为0、状态及审核紧急度为空；默认人员无有效身份、现有 `active=false`；默认候选无 score/rank。默认对象不能直接通过公共策略调用。已知但不合格的真实状态（例如停用、休息、无技能、超载）仍可表示，交给后续硬过滤解释，不能把“结构合法”当成“可以派单”。

## 4. 策略与结果设计

`DispatchStrategy` 使用非虚公共入口 `recommend(order, workers) const`，内部调用受保护纯虚函数 `doRecommend(...) const`，具有虚析构函数。公共入口校验工单/人员结构、重复 worker ID 及返回结构，异常为 `std::invalid_argument`，不绑定 HTTP 错误码。

- 参数均为 const 引用；接口约定确定性、输入不变、无外部副作用。
- `DispatchCandidate` 与 `ExcludedWorker` 分开存放。排除项没有分数/排名；结果禁止同一 worker 同时出现在两组、重复ID、未知人员或无有效排除原因。
- 候选预留 worker ID、负载/容量、可选分项/总分、可选排名/样本数、理由与风险。缺失分数不冒充实际0分或60分。
- `p0Weights` 是不可配置常量，顺序为技能/区域/负载/效率/评价/值班，固定35/20/20/10/10/5。
- 结果结构校验检查分数有限且在0～100、排名/样本数边界等；**不验证真实资格、加权总分一致性、完整候选划分或生产排序语义**。这不是完整过滤器或评分器。
- W6-D3 后续才实现分数降序、同分低负载/低 worker ID，以及Top3与其他合格候选；本日没有输出真实推荐名单。

单测中的 `EchoFixtureStrategy` 明确按数值ID排序副本，并按构造参数生成合成排除结果；`RejectAllFixtureStrategy` 用于替换性验证。两者仅存在于单测文件，均非业务算法。分项测试中的任意数值只验证表示与范围，不能当成正确加权评分。抽象接口本身不能阻止未来恶意实现执行I/O，需继续源码审查。

## 5. 实际命令与结果

在项目根目录执行，使用已有 Visual Studio CMake，不安装新依赖、不升级 baseline：

```powershell
$cmakeBin = 'D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
& "$cmakeBin\cmake.exe" --build backend/build --config Debug --parallel 4
& "$cmakeBin\ctest.exe" --test-dir backend/build -C Debug -R '^dispatch_domain$' -V
& "$cmakeBin\ctest.exe" --test-dir backend/build -C Debug --output-on-failure
& "$cmakeBin\ctest.exe" --test-dir backend/build -C Debug -N
git diff --check
```

| 检查 | 本轮实际结果 |
| --- | --- |
| 配置复用/自动重新生成 | CMakeLists变更触发配置重新生成成功；既有依赖均 already installed，无下载/升级 |
| 首次受限环境构建 | 失败：MSB4018，FileTracker初始化 E_ACCESSDENIED；保留为环境失败历史，非源码编译错误 |
| 同命令授权本机 MSVC Debug 全量构建 | 退出0，所有目标构建成功；未见编译/链接警告 |
| 新增 `dispatch_domain` | 1/1通过，退出0；15个场景、104个断言；测试0.03秒、定向CTest总0.05秒 |
| 全量 CTest | **28/28通过，失败0，退出0，158.38秒** |
| 原测试保留 | 对照起始HEAD与当前注册列表：原27项均保留并本轮通过，新增1项 `dispatch_domain` |
| 纯库链接 | `campusfix_dispatch` 无第三方链接；测试可执行文件 dumpbin 仅见MSVC运行库及KERNEL32，无Drogon/SQLite/JSON/AI运行依赖 |
| Git空白检查 | `git diff --check`退出0；6个新增文件另外执行no-index空白检查，无空白诊断 |

现有版本仍为 Drogon 1.9.13#2、SQLite 3.53.4#2、libsodium 1.0.22#1，triplet为x64-windows。没有修改manifest/lock或工具链。整个后端CMake仍会查找原有依赖，独立测试目标不链接它们，不宣称整个项目可在未安装依赖的机器直接配置。

单测15个场景实际覆盖：安全默认、现有类型兼容、关键ID/状态/审核紧急度、人员输入边界、技能边界、位置元数据结构、未知/零历史区分、历史非法值、多态替换、显式确定性夹具排序及重复/排列调用、完整输入快照不变、空集合、结果分区结构、分数表示/固定权重、64位大ID。

原27项回归包括AI本地假模型HTTP、认证/授权、数据库迁移/seed、工单/目录/审核/人员Service与HTTP。它们使用既有隔离测试夹具，不是新调度接口测试，也不证明真实DeepSeek或人工浏览器回归。

## 6. 无副作用核验与未实现边界

- 调度生产源码只使用C++标准库及既有纯数据头文件；没有HTTP/SQLite调用、网络、文件访问、Controller路由、业务表写入或状态转换。测试专用策略没有接入正式服务器。
- 本轮没有读取/修改用户业务数据库，没有创建真实派单。全量既有CTest会在自身隔离夹具内执行原有数据库/HTTP测试，不能把它们说成完全不产生测试数据。
- 未实现：完整硬过滤、评分公式/冷启动、历史查询适配器、Top3、生产稳定排序、API-17/18、派单事务、前端调度界面、P1动态策略/排班。
- 未执行：前端测试/build、独立W2冒烟、AI-D3额外68次HTTP/37快照脚本、Vite代理人工回归、真实DeepSeek、浏览器、跨机器部署或性能压测。本日纯C++范围不机械重跑这些流程，不借用用户基线数字冒充本轮结果。
- 无冻结接口/Schema/seed/状态机/AI/认证语义变更；未删除失败用例获得通过。

## 7. W6后续输入映射与风险

1. W6-D2及后续适配器需从数据库建立一致的新鲜快照：审核分类/紧急度、账号状态/角色、有效技能与资质、人员区域/层级及实时负载。管理界面的DTO不能直接冒充完整调度输入。
2. 现有 WorkerRepository 过滤有效人员技能/技能项，但缺账号状态、完整区域层级和历史统计；本日复用类型，不直接复用不完整结果来做推荐。
3. 跨区域支持若依赖“明确授权”，当前没有核实到独立人员跨区授权字段/映射。不能把管理员AREA_SCOPE/DISPATCH授权偷换成维修人员跨区资格，也不能因缺数据而放宽硬约束；后续落地该分支前需确认可靠来源。本日不新增Schema或授权规则。
4. 历史数据可能尚未形成；W6-D3需按冻结6.5实现最近20有效样本、返工最终提交归属、样本阈值及可复算公式。未加载不能冒充已核实零样本，本日不生成实际60分。
5. 空位置元数据只表示未知，不授予区域资格；结构校验不能证明传入祖先确实来自数据库。未来适配器与硬过滤需验证来源，不能接收客户端自报的技能/层级/负载。
6. 候选模型仅为基础，后续策略必须先硬过滤再评分，并测试无候选解释、同分排序及不得靠高分复活排除人员；确认派单仍需独立事务重新检验容量、状态与版本。

这些是后续映射与算法的明确前置工作，不以临时猜测修改冻结需求；本日纯模型/接口无阻塞。

## 8. Git交付状态

工作区仅上表8个文件；暂存区为空，无无关未跟踪文件。构建/测试产物仍被忽略，未将数据库、WAL、可执行文件或缓存纳入文件范围。最终Git状态与空白检查已实际核对；没有执行git add、commit、push或merge。

**CampusFix W6-D1 实现与自动化验证已完成，等待人工与源码审查；未进入W6-D2。**
