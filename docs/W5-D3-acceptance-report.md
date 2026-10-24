# W5-D3 人员档案与真实负载：自动化验收报告

日期：2026-10-24。最新状态：**端口/链接阻塞已解除，MSVC Debug构建、正式Worker HTTP、全量CTest及W2冒烟通过；等待用户最终功能、源码与Git范围验收，不进入W5-D4。** 下文保留首次阻塞及当时未执行项目的历史，并在第7～10节记录获准继续后的实际结果；历史“未验证”不代表最新测试状态。

## 1. 实际基线与范围

- main/HEAD：`2e690674cb6a9678032c0f0b1d9e10f2e8c8dc1b`，与本地origin/main对齐；开始时工作区及暂存为空，未fetch。
- 仅编写API-14 GET /api/workers、API-15 GET /api/workers/{id}/workload、API-16 PUT /api/workers/{id}及测试。没有创建/删除人员、账号变更、技能字典写接口、管理员Vue页面或W6调度。
- 复用AuthGuard、AuthService、AuthorizationService/Policy/Repository、SqliteConnection/Transaction及ResponseUtils，不修改Schema、seed、冻结文档、前端或W3认证权限代码。

## 2. 已写入的实现

- 列表SQL按dispatcher AREA_SCOPE、sysadmin WORKER_MANAGE自己的区域递归过滤，count和分页使用相同范围；重叠授权不重复，worker必须mine=true并绑定当前认证user_id。只序列化冻结字段，不返回账号/联系方式/授权数据。
- 实时负载严格统计assignments.ended_at IS NULL，不根据工单状态或缓存判断。历史超载时保留真实active_tasks，剩余容量置0，并由Controller记录包含request_id的告警，不增加协议字段。
- PUT在BEGIN IMMEDIATE内重验Session、旧区域权限、有效目标区域权限和实时容量，整体更新workers与worker_skills，不触碰用户、派单、工单和状态日志。无expected_version，使用事务串行化，不宣称人员乐观锁。
- 技能关系不物理删除；省略的旧关联停用，连续有效关联保留effective_at，重新激活使用当前UTC生效时间，updated_at反映当前更新。certified按实际布尔保存，绝不因requires_certificate自动设为true；空技能集合允许但不会留下有效旧技能。
- 规则说明：服务区域按SRS13.1的校区/楼宇解释，只接受有效CAMPUS/BUILDING及有效祖先；已有任务时停用、改可用状态、调整区域或技能不自动结束当前派单或修改工单。W6仍须做完整资格过滤，本轮未实现EligibilityFilter。

## 3. 首轮实际验证与未验证（历史记录，解除阻塞前）

| 检查 | 实际结果 | 退出码 |
| --- | --- | --- |
| CMake配置 | 复用原依赖缓存，配置/生成完成，无依赖版本变更 | 0 |
| MSVC Debug全工程构建 | 新源码及测试目标编译成功，正式exe链接LNK1168失败 | 1 |
| worker_service CTest | 1/1通过，1.17秒，60个SQLite/Service断言 | 0 |
| 新纯Policy测试 | 0，复用原Policy；本轮尚未全量回归 | — |
| 前端test:auth | 9/9 | 0 |
| 前端test:orders | 16/16 | 0 |
| 前端build | 成功，保留既有chunk>500kB警告，JS674.60kB/gzip223.17kB | 0 |
| 新Service临时库 | integrity_check=ok；foreign_key_check无违规 | 0 |
| 新正式Worker HTTP | **未执行，0次**；不以Service测试替代 | 未验证 |
| 全量CTest、W5-D1/2/W4回归 | 尚未执行本轮全量 | 未验证 |
| W2冒烟 | 未执行，不向原用户进程发送测试写请求 | 未验证 |

已运行命令：使用现有VS CMake执行`cmake --build backend/build --config Debug --parallel 4`；`ctest --test-dir backend/build -C Debug -R "^worker_service$" --output-on-failure`；frontend下npm.cmd run test:auth、test:orders、build。

Service60个断言覆盖区域/分页/筛选/本人读取、有效与历史任务、超载诊断、旧/新区域权限、技能停用/重新激活/空集合/证书、容量边界、非法输入、事务故障注入和双连接并发完整档案。使用独立临时库，结束后关闭连接并清理，没有修改用户人工库。还未获得正式HTTP请求、响应、request_id及HTTP数据库证据，不补造这些内容。

## 4. 首轮确切构建阻塞（历史记录）

原始链接错误：

```text
LINK : fatal error LNK1168: 无法打开 G:\A_Study\CppProject\CampusFix\backend\build\Debug\campusfix_backend.exe 进行写入
```

本机只读进程检查确认：PID37648正在运行该完整路径的exe，启动于2026-10-24 14:11:35，并监听127.0.0.1:8848；5173由既有PID6600监听。它们不是本轮启动的进程，未擅自停止。受限环境CIM查询曾拒绝访问，随后经批准在本机只读查询确认，不把权限错误当作进程不存在。

需要用户在原终端停止服务，或明确授权停止以上指定PID后再继续链接与测试；不能使用旧的运行中exe声称新接口已经通过。当前端口仍由用户原进程占用，不宣称已释放。

## 5. 当前完整文件范围

新增12文件：

- G:/A_Study/CppProject/CampusFix/backend/src/models/Worker.h
- G:/A_Study/CppProject/CampusFix/backend/src/controllers/WorkerController.h
- G:/A_Study/CppProject/CampusFix/backend/src/controllers/WorkerController.cpp
- G:/A_Study/CppProject/CampusFix/backend/src/repositories/WorkerRepository.h
- G:/A_Study/CppProject/CampusFix/backend/src/repositories/WorkerRepository.cpp
- G:/A_Study/CppProject/CampusFix/backend/src/services/WorkerService.h
- G:/A_Study/CppProject/CampusFix/backend/src/services/WorkerService.cpp
- G:/A_Study/CppProject/CampusFix/backend/tests/integration/WorkerTestData.h
- G:/A_Study/CppProject/CampusFix/backend/tests/integration/WorkerServiceTest.cpp
- G:/A_Study/CppProject/CampusFix/backend/tests/integration/WorkerHttpFixture.cpp
- G:/A_Study/CppProject/CampusFix/backend/tests/integration/WorkerHttpTest.ps1
- G:/A_Study/CppProject/CampusFix/docs/W5-D3-acceptance-report.md

修改3文件：

- G:/A_Study/CppProject/CampusFix/backend/CMakeLists.txt（生产源文件/测试注册）
- G:/A_Study/CppProject/CampusFix/backend/src/main.cpp（仅挂接三个正式Worker路由）
- G:/A_Study/CppProject/CampusFix/docs/progress-log.md（本轮实际进度与阻塞）

删除0。暂存为空，不自动git add/commit/push。Git空白检查通过不等同完整功能验收；后续需继续核对失败输入、真实HTTP、整体回归、Git完整范围及构建产物忽略规则。

## 6. 首轮后续待完成与风险（历史记录）

先解除exe占用，再重新构建、执行Worker正式HTTP测试并修复真实失败，之后全量CTest、W2冒烟、SQLite完整性与Git检查；更新本报告为实际结果。没有管理员页面，不需要也不虚构浏览器CRUD截图。

尚未验证正式HTTP、全量回归、物理COMMIT失败、断电/磁盘满、高压或跨机器运行；Service串行化测试不是预期版本冲突测试。任何既有认证、离线退出、限流等风险仍保留。W5-D3当前未完成，不进入W5-D4。

## 7. 用户授权后的进程复查与重新构建

- 用户明确批准处理原PID37648和PID6600，但要求先核实PID、ExecutablePath、CommandLine及监听关系，身份不能确认不得终止，优先正常退出且禁止批量终止。本轮先进行了本机只读CIM/端口检查；受限查询不用于推断进程身份。
- 本机复查实际输出为`PID_NOT_FOUND=37648`、`PID_NOT_FOUND=6600`，8848/5173监听结果为空。两进程已不存在，无法再读取其ExecutablePath/CommandLine；不推测退出时间或原因，也未对这两个PID执行正常退出、Stop-Process或其他终止命令。没有停止其他既有Node、C++、Visual Studio或系统进程。
- 在端口空闲后复用现有CMake配置/依赖缓存重新构建：退出0，用时7.36秒，正式`backend/build/Debug/campusfix_backend.exe`成功链接，LNK1168不再出现；未见编译/链接警告。不重装依赖、不改变baseline、triplet、Drogon、SQLite或libsodium版本。
- 随后所有HTTP测试使用这次新链接的正式Drogon程序和独立临时SQLite库，不使用原用户服务冒充新接口。用户人工验收库、Schema和seed均未修改。

## 8. 本轮实际验证结果

| 检查 | 实际结果 | 退出码 |
| --- | --- | --- |
| MSVC Debug全工程构建 | 成功，无编译/链接警告 | 0 |
| Worker定向CTest | worker_service、worker_http共2/2通过，12.63秒 | 0 |
| 最终全量CTest | **22/22通过，65.02秒**；Worker Service 1.18秒，Worker HTTP 11.49秒 | 0 |
| Worker SQLite/Service测试 | **60个断言**；独立真实SQLite连接/事务/故障注入，不冒充HTTP | 0 |
| Worker正式HTTP测试 | **107次真实HTTP请求**，含预期401/403/404/422及注入故障500 | 0 |
| W5-D1审核回归 | Service 283个断言，正式HTTP149次请求，已计入22项CTest | 0 |
| W5-D2补充回归 | Service 60个断言，正式HTTP74次请求，已计入22项CTest | 0 |
| 既有W3/W4回归 | 认证、Guard/Policy、迁移/seed、创建/提交/列表/详情/时间线及报修集成测试全部通过 | 0 |
| SQLite完整性 | Worker Service/HTTP临时库`integrity_check=ok`，`foreign_key_check`无违规 | 0 |
| W2真实后端/Vite冒烟 | 直连与代理各2次HTTP200 JSON、code=0、status=ok、service正确且request_id不同；首页200 HTML | 0 |
| 最终端口清理 | 测试进程关闭后8848、5173监听数为0 | — |

新增纯Policy测试0；原Policy测试在全量CTest中通过。定向执行与全量执行重复跑同一组Worker测试，107次是最终单次Worker HTTP测试的请求数，不将两轮相加冒充更多独立用例。

前端认证9/9、工单16/16及生产build成功沿用本阶段上一轮实际结果，**本轮没有重新执行前端测试/build**，没有改动前端代码。既有JS大chunk警告（674.60kB/gzip223.17kB）保留，不写成无警告。没有管理员Vue页面，不声称完成浏览器人员管理验收或提供截图。

实际执行命令（从项目根目录；CMake/CTest使用本机现有VS工具，不要求写入脚本）：

```powershell
& "D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build backend/build --config Debug --parallel 4
& "D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir backend/build -C Debug -R "^worker_" --output-on-failure
& "D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir backend/build -C Debug --output-on-failure
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./scripts/smoke.ps1
git diff --check
```

冒烟前分别启动新后端（未配置数据库，验证health独立）及Vite，监听127.0.0.1:8848与5173，使用隐藏测试进程与独立日志。冒烟实际输出摘要：

```text
PASS backend #1 request_id=req_844e3012-7880-47c8-b18f-6a8f4d56d594
PASS backend #2 request_id=req_dc86648e-7b8e-4eba-9ddd-ae48bb40be19
PASS proxy #1 request_id=req_99733c44-d020-430f-99c5-596e0660f233
PASS proxy #2 request_id=req_222d6ef8-158b-4a4d-b4c2-8b0767b0dc05
PASS frontend homepage HTTP 200 HTML
PASS all W2-D5 smoke checks
SMOKE_EXIT=0 BACKEND_PID=25572 VITE_PID=35336
FINAL_LISTENERS=0
```

这些是本轮自行启动的测试PID，只按其PID清理；它们不同于用户批准复查但已消失的37648/6600。临时环境变量在finally中恢复，未持久覆盖用户环境变量。HTTP夹具另使用随机空闲端口并在finally清理自己的进程及临时库。

## 9. API与数据库实际证据

完整本轮Worker回执及SQL前后快照保存在被Git忽略的`backend/build/Testing/w5d3-evidence.json`；CTest输出在`backend/build/Testing/Temporary/LastTest.log`。均无原始Token或固定密码。以下ID仅属于本轮临时测试数据，不与用户人工库相同ID混用。

### API-14：真实授权列表

`GET /api/workers`，A区域dispatcher：HTTP200、code=0、page=1、page_size=20、total=3，items的ID为1/2/3；request_id为`req_26f2f342-38e6-4661-b3c3-a744dc49c163`。B区域dispatcher同样HTTP200、total=3，只含4/5/6。worker的`?mine=true`仅返回本人ID1，未加mine拒绝403；reporter拒绝403。列表项严格只有id、worker_no、display_name、service_area_id、availability、max_concurrent_tasks、active_tasks、skills；skills仅skill_id/level/certified。

测试实际验证了范围内分页/total、空集、状态/技能筛选、重叠授权去重、sysadmin必须有自己的WORKER_MANAGE范围、AREA_SCOPE不等价于该能力。未返回其他账号/联系方式/授权字段。

### API-15：真实负载

`GET /api/workers/1/workload`，HTTP200响应：

```json
{"code":0,"message":"success","data":{"worker_id":1,"active_tasks":2,"max_concurrent_tasks":3,"remaining_capacity":1,"calculated_at":"2026-10-24T07:21:29.753Z"},"request_id":"req_054ff753-3566-4388-85a6-5a47401930fe"}
```

Service及HTTP分别验证0任务、多条有效任务与历史已结束任务；只统计ended_at IS NULL，包括未接单的有效派单。Service另验证不能因工单状态为COMPLETED就忽略仍未结束的异常派单。超载夹具返回active_tasks=2、capacity=1、remaining=0，并验证正式日志存在`Worker overload worker_id=1`告警；不静默隐瞒真实负载。

### API-16：真实更新及持久化

`PUT /api/workers/1`请求（经A区域dispatcher授权）：

```json
{"service_area_id":3,"availability":"AVAILABLE","max_concurrent_tasks":2,"emergency_duty":true,"active":true,"skills":[{"skill_id":1,"level":4,"certified":false}]}
```

HTTP200响应：

```json
{"code":0,"message":"success","data":{"id":1,"availability":"AVAILABLE","max_concurrent_tasks":2,"skills":[{"skill_id":1,"level":4,"certified":false}],"updated_at":"2026-10-24T07:21:36.631Z"},"request_id":"req_b52faec0-8886-4c61-b284-fd943a290fb1"}
```

独立SQL查询确认worker1的service_area_id=3（BUILDING）、capacity=2、active=true、emergency_duty=true；account_role=worker、account_status=ACTIVE保持原值。skill1有效、level4、certified=false，即使其技能要求证书也没有自动改为true；省略的skill2关联保留但active=false。重新激活后的effective_at为`2026-10-24T07:21:30.998Z`，后续连续有效更新仍保留此值，updated_at为`2026-10-24T07:21:36.631Z`。

更新后GET负载仍为active_tasks=2，capacity=2、remaining=0；request_id为`req_1178c71b-8bc9-4b19-8c03-df25f2827fde`。容量等于现有任务数允许，小于任务数422且无数据库副作用；未创建、结束或移动任何既有派单。

失败与一致性证据：

- HTTP验证缺失/无效/过期/撤销Session401、禁用用户旧Session401、无动作权限403、隐藏的越界/不存在资源404、撤销授权立即失效；旧区域与目标区域权限均需满足。
- HTTP验证字段缺失/多余/类型/枚举/容量/熟练度/重复技能/无效技能或区域/停用引用等422，畸形JSON400；GET及每个预期失败前后比较全库指纹，确认无写副作用。
- HTTP分别注入档案写入失败与技能写入失败，预期500且整体回滚。Service另有双真实SQLite连接竞争更新，最终档案完整对应某一请求，不出现混合字段；无expected_version，不宣称409乐观锁冲突。
- 最终HTTP夹具独立核验`integrity_check=ok; foreign_key_check empty; orders/assignments/audit unchanged`。测试结束关闭连接并清理临时库，运行时回执继续留在被忽略的build目录。

## 10. 最终Git检查与剩余风险

预期范围仍为第5节的**新增12、修改3、删除0，共15文件**；其中本轮解除阻塞后仅更新本报告和progress-log，未增加新的业务代码修改。暂存区为空，未执行git add、commit或push。冻结设计、001迁移、seed、vcpkg依赖清单、W3权限及前端均未改变；无无关未跟踪文件，两份原W1-D3交接文件本轮没有修改或加入范围。

`git diff --check`退出0；新增未跟踪文件另逐一用no-index进行空白检查（有新文件差异时退出1并非空白失败），不使用git add -N。数据库/WAL/SHM、构建产物、运行时证据、node_modules/dist继续由Git忽略。Git的LF→CRLF提示不是编译警告或diff空白错误。

尚未验证物理COMMIT失败、断电/磁盘满、高负载或跨机器首次部署。保留既有内存Token刷新需重登、离线退出不保证服务端撤销及完整登录限流尚未完成等风险。本轮不包含W6调度资格过滤，也没有人员管理前端；不伪造浏览器证据。**W5-D3自动化验证完成，无当前构建/测试阻塞，等待用户最终验收，停止于W5-D3。**
