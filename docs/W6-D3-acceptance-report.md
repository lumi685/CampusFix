# W6-D3：WeightedScoreStrategy 加权评分验收报告

记录日期：2026-10-31。状态：实现与自动化验证完成，等待人工与独立源码审查。范围：FR-007的纯C++17评分、稳定排序与解释；不代表推荐REST或确认派单已实现。

## 1. 起始基线与同步证据

- 分支：`feature/w6-dispatch`。
- 起始HEAD：`e0cc7dfea6799559502241236334c5456adbd6a5`，`feat(dispatch): implement deterministic eligibility filtering`。
- 启动时助手在获准本机网络执行 `git fetch origin` 失败，退出128：`Recv failure: Connection was reset`；按要求暂停，没有编码、清理或改动文件。
- 用户随后亲自在PowerShell执行fetch成功，并提供HEAD/远端引用均为上述SHA的结果。继续执行时助手重新核对branch/status/cached/两个rev-parse：分支正确，工作区/暂存区干净，更新后的 `origin/feature/w6-dispatch` 与HEAD一致。**成功fetch来自用户提供的本机结果，不冒称助手fetch成功。**
- 已按AGENTS优先级阅读SRS第14/50章、W6计划、FR-007/TC-DSP-05～10、数据库6.5、API-17/18、D-006/013/016及现有模型/策略/过滤器/测试/CMake和D1/D2验收报告。没有修改冻结资料。

## 2. 实际文件范围

根目录为 `G:\A_Study\CppProject\CampusFix`。

| 状态 | 相对路径 | 用途 |
| --- | --- | --- |
| 新增 | `backend/src/dispatch/WeightedScoreStrategy.h` | final抽象策略实现及历史输入/精度契约 |
| 新增 | `backend/src/dispatch/WeightedScoreStrategy.cpp` | 真实过滤、六项评分、排序、解释 |
| 新增 | `backend/tests/unit/WeightedScoreStrategyTest.cpp` | 真实算法测试及六组可复现实验 |
| 修改 | `backend/CMakeLists.txt` | 纯库源文件及一个CTest目标 |
| 新增 | `docs/W6-D3-acceptance-report.md` | 本报告 |
| 修改 | `docs/progress-log.md` | 追加实际执行记录 |

最终范围修改2、新增4、删除0，共6文件。没有修改DispatchModels、DispatchStrategy、EligibilityFilter或D1/D2测试；没有复制领域模型、削弱既有校验或增加第三方依赖。

## 3. 策略调用与数据前提

`WeightedScoreStrategy final : public DispatchStrategy` 重写受保护的doRecommend，通过原公共recommend入口调用。执行顺序：

1. 公共入口验证工单、人员结构及重复ID。
2. 调用真实 `EligibilityFilter::filter`；不另建一套资格规则。
3. 保留excluded原始人员、原因数组及顺序；只处理eligible。
4. 以worker ID索引原输入，先检查所有eligible的efficiency/rating均已加载；任一nullopt抛 `std::invalid_argument`，整个调用不返回部分成功名单。
5. 计算六分项、总分、样本数及固定解释，再排序、连续排名。
6. 公共入口复用validateResult。整个实现只进行内存计算，const输入不变。

被排除人员的nullopt历史不触发冷启动或评分；它们无需为评分加载历史。非法NaN/无穷、重复样本或超过20条等结构错误仍由原公共校验拒绝，不能因先过滤而绕过结构安全检查。

## 4. 六项数学规则与区域边界

令 `Q(x)=round(clamp(x,0,100)*10)/10`，分数均为有限0～100数值。

| 分项 | 实现口径 |
| --- | --- |
| skill | 对正确requiredSkill的level使用 `level/5.0*100`，Lv1～5为20/40/60/80/100，不用其他技能替代 |
| area | 经D2过滤后，双方有相同可信buildingId为100；合法校区级覆盖为70；本日没有跨区30分候选 |
| load | `Q(100*(1-double(activeTasks)/capacity))`；满载/超载已经在过滤阶段排除 |
| efficiency | n<3为60；否则对每样本取 `clamp(100*targetCompletionMinutes/max(1,actualMinutes),0,100)`，**先求未经舍入的样本均值，再Q** |
| rating | n<3为60；否则对原星级×20求均值后Q |
| duty | 仅 `approvedUrgency==URGENT && emergencyDuty` 为100，其余0，HIGH不等同URGENT |

区域特别规则：楼宇级只服务本楼宇，得100；校区级服务所辖建筑、校区节点或无buildingId的公共区域统一记70。依据是冻结50.3的校区级匹配档位以及任务对合法校区覆盖的要求，没有精确楼宇依据就不能称同楼宇100。D2区域未核实、越楼宇/校区仍排除；评分没有扩大资格或编造距离/跨区授权。

效率例：目标60，实际30/60/120分钟得到100/100/50，均值83.3。实际0或小于1按1分钟处理。评价4/5/4星均值为86.7。两指标分别判断阈值，不合并样本数量；每个原始效率样本不先四舍五入。

## 5. 精度、总分复算与排序

冻结文档明确历史分保留1位小数，但没有强制负载/总分额外精度。结合API-17一位小数示例，本轮选择所有返回分项及总分统一为一位小数；数值直接保存为double，不依赖printf/字符串反解析。

总分使用**已返回分项**和原 `p0Weights={0.35,0.20,0.20,0.10,0.10,0.05}`：

`total=round_one_decimal(0.35*skill+0.20*area+0.20*load+0.10*efficiency+0.10*rating+0.05*duty)`。

实现上设 `c_i=round(10*component_i)`、`p_i=round(100*p0Weights[i])`，`W=sum(c_i*p_i)`，`totalTenths=(W+50)/100`（非负整数除法），`total=totalTenths/10.0`。百分比来自既有权重常量，不维护另一套可配置权重。W最大100000，不溢出int64。这样对十进制总分半入准确，例如80.35→80.4，而非被二进制乘加误差误判为80.3。

负载1/3→66.7、2/3→33.3；以这些返回值参与总分。历史仅在汇总阶段Q。总分已归一到十分位，严格按：总分降序→activeTasks升序→数值workerId升序。比较器没有epsilon，不以隐藏原始小数破同分，形成严格全序；rank为1开始连续整数。eligible保留全部人，第4名及之后不截断，Top3/其他候选HTTP拆分留待D4。

边界测试实际覆盖：80.34→80.3，80.35→80.4；87.00与87.01同为87.0时按负载/ID而非隐藏精度排序；完整分项可独立复算总分。未来API/UI必须沿用此口径，不得重新用未返回的负载小数或另一个隐藏总分排序。

## 6. 历史输入与未来数据库适配契约

- `nullopt`未加载，不能取60；已加载空vector是确认零样本，才可冷启动。
- 只有合格候选需两项历史均完整加载；样本数准确取vector长度。0/1/2分别采用60，3～20按均值。算法不自行截取、补齐、查询或编造历史。
- 未来D4适配器按冻结数据库6.5提供最近20条：效率为正常COMPLETED派单、accepted_at和最后result_submitted_at均非空，按ended_at/id倒序；时差按UTC计算，包含返工后的最终提交，target取类别目标时长。
- 评价只归属工单最后COMPLETED的维修人员，按评价created_at/id倒序取最近20；不能把历史转派人员或别人的评价拼入。
- 当前纯模型仅有必要原始样本ID/目标/耗时/星级，不能独立证明时间选取、归属或数据库真实性。这是未来适配器必须兑现的可信输入契约，不宣称今日已验证这些SQL联查。

## 7. 解释、风险与六组实际合成实验

每个合格候选输出：必需技能等级、Verified building/campus coverage、真实负载、是否有Urgent on-duty bonus、两项历史的冷启动或有效样本均值说明。两项各自n<3产生对应固定风险字符串 `Efficiency/Rating samples below 3; cold-start score 60`，原因中包含准确样本数；不赞称缺数据者“效率高/评价好”。不含displayName、workerNo、联系方式、凭证或内部备注。excluded不追加推荐理由/score/rank。

以下数据由真实策略在单测中实际运行，均为**虚构内存快照**。公共工单：ID9000000001、v2、待派单、技能8、房间44→楼层33→楼22→校区11，通常NORMAL。默认人员ACTIVE/worker/AVAILABLE/档案有效、已加载有效技能和verified位置；两项历史均已加载为空，故60/60并产生两项冷启动风险。不是实际数据库记录或真实维修效率。

| 实验 | 输入差异（ID：所需等级、服务区域、负载、值班） |
| --- | --- |
| ordinary | 1：Lv5、楼22、1/3、否；2：Lv4、楼22、0/3、否；3：Lv5、校区11、0/3、否 |
| urgent | 工单URGENT；1：Lv5、楼22、0/3、否；2同上但值班true |
| skill-mismatch | 1仅有其他技能9 Lv5；2为默认合格人员 |
| at-capacity | 1为3/3；2为默认合格人员 |
| ties | 20：Lv5、楼22、1/3；10：Lv5、楼22、2/6；5：Lv5、楼22、1/3，均非值班 |
| all-excluded | 1无技能；2为3/3；3档案active=false |

分项向量固定顺序为skill/area/load/efficiency/rating/duty，以下为定向CTest实际输出：

| 实验 | worker | 分项 | 总分 / rank 或排除原因 |
| --- | ---: | --- | --- |
| ordinary | 3 | 100/70/100/60/60/0 | 81.0 / 1 |
| ordinary | 1 | 100/100/66.7/60/60/0 | 80.3 / 2 |
| ordinary | 2 | 80/100/100/60/60/0 | 80.0 / 3 |
| urgent | 2 | 100/100/100/60/60/100 | 92.0 / 1 |
| urgent | 1 | 100/100/100/60/60/0 | 87.0 / 2 |
| skill-mismatch | 2 | 100/100/100/60/60/0 | 87.0 / 1 |
| skill-mismatch | 1 | 不评分 | MissingSkill |
| at-capacity | 2 | 100/100/100/60/60/0 | 87.0 / 1 |
| at-capacity | 1 | 不评分 | AtCapacity |
| ties | 5、20、10 | 各100/100/66.7/60/60/0 | 各80.3，rank分别1/2/3 |
| all-excluded | 1、2、3 | 无合格候选 | 分别MissingSkill、AtCapacity、WorkerInactive |

理由直接来自这些分项：ordinary的3仅称校区覆盖；urgent只有2称值班加分；每人的Current load和Required skill level与输入一致；每位已评分人员均明确零样本冷启动。全部具体固定字符串在CTest stdout中实际输出，不引用模型回答。

单测另做**测试专用**最小任务量/ID对照，不新增生产P1策略：ordinary为2→3→1（加权为3→1→2）；urgent为1→2（加权为2→1）；技能失败/满载均为2；ties均5→20→10；all-excluded均为空。这里只说明排序侧重点不同，不证明优于人工、全局最优或实际校园效率提升。

## 8. 构建、测试与失败历史

实际命令在项目根目录执行：

```powershell
$cmakeBin = 'D:\CSwWorkApp\Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
& "$cmakeBin\cmake.exe" --build backend/build --config Debug --parallel 4
& "$cmakeBin\ctest.exe" --test-dir backend/build -C Debug -R '^dispatch_(domain|eligibility|weighted_score)$' -V
& "$cmakeBin\ctest.exe" --test-dir backend/build -C Debug --output-on-failure
& "$cmakeBin\ctest.exe" --test-dir backend/build -C Debug -N
git diff --check
```

| 项目 | 实际结果 |
| --- | --- |
| 完整MSVC Debug | 退出0，成功；CMake自动重新配置/生成成功，未见编译/链接警告 |
| 依赖 | 原依赖均already installed，无下载/升级，无manifest/npm依赖变化 |
| 定向CTest | 3/3通过，退出0，0.50秒 |
| dispatch_domain | 15场景、104断言，原源码不变 |
| dispatch_eligibility | 41场景、1019断言，原源码不变 |
| dispatch_weighted_score | **57场景、1149断言通过**，直接运行真实WeightedScoreStrategy与EligibilityFilter |
| 注册 | CTest -N实查30项；与起始HEAD对照原29项完整保留，仅新增dispatch_weighted_score |
| 全量CTest | **30/30通过、失败0、退出0、156.83秒**，包括原29项 |
| 链接核对 | 新测试dumpbin仅见MSVC运行库与KERNEL32，无Drogon/SQLite/WinHTTP/AI库依赖 |
| Git空白/编码/范围 | git diff --check退出0；4个新增文件no-index空白检查无诊断；全部6文件UTF-8及末尾换行检查通过 |

57场景覆盖任务45类要求，并包括六实验、历史未加载、各自冷启动、近似同分、十进制半入、20条边界/超限、NaN/无穷/重复、极端有限输入、全部720种人员排列、完整结果重复一致、64位ID及输入成功/失败不变。1149断言包含720排列比较，不能表述为1149独立业务场景。不累计重复运行数；不会把未实现API-17/18测试列为通过。

本轮构建、定向和全量测试没有源码/断言失败；没有删减失败用例。仅保留第1节真实网络阻塞及用户补齐同步的历史，未借测试成功改写fetch失败。最终测试原始日志位于被忽略的 `backend/build/Testing/Temporary/LastTest.log`，含六组实验输入/输出和57/1149结论；不是另行编造的数据库或HTTP证据。

## 9. 边界、未验证事项与W6-D4前提

- 新代码只依赖C++标准库和既有纯模型，无Drogon/HTTP、SQLite查询、WinHTTP、AI、文件或业务写入、全局可变状态。测试程序的stdout/计数器仅用于单测；未接入生产Controller。
- 未新增API-17/18、数据适配器、派单、SQL/Schema/seed、前端面板或P1策略/动态权重。未改身份/授权、审核、工单状态机、AI-D1～D3、模型或D2硬过滤。
- 原全量CTest包含隔离HTTP/SQLite/假模型回归；不是本日算法访问业务数据库或真实DeepSeek。未运行前端/build、独立W2冒烟、额外AI-D3脚本、浏览器、真实校园数据、跨机器或性能/长期压测，不借用历史结果声称本轮通过。
- D4需建立真实输入适配器，核实审核字段、账号/技能、verified区域链、实时容量及上述历史查询契约。失效数据不能伪造为空或冷启动。
- D4只读推荐API须先做管理动作/区域授权，使用当前评分口径并拆分Top3/其他候选；不是让用户提交角色、评分、负载或验证标记。
- 确认派单必须在短BEGIN IMMEDIATE事务中重查状态/version/有效技能资质/区域/实时容量，重算过滤和评分，原子保存最终快照、派单、状态及日志，处理唯一派单和容量竞争；本日快照通过不保证之后仍可派单。
- 跨区来源仍未知，30分分支未启用。量化精度及校区70分已在头文件/本报告/测试明确；未来UI不能隐藏不同排序口径。算法未被证明现实最优。

## 10. 最终Git状态

工作区仅第2节6文件（修改2/新增4/删除0），暂存区为空，没有无关未跟踪文件。HEAD仍为e0cc7df；已核对模型/过滤器/D1/D2测试及冻结设计、Schema/seed、原业务代码无差异。

`git diff --check`退出0；4个新文件另执行 `git -c core.autocrlf=false diff --no-index --check -- NUL <file>`，无空白诊断（退出1只表示相对空文件有差异）；全部6文件严格UTF-8解码及末尾换行通过。构建库/可执行文件、CMake缓存、CTest日志、数据库/WAL路径仍被忽略，不纳入提交范围。没有自动git add、commit、push或merge。

**CampusFix W6-D3 WeightedScoreStrategy 实现与自动化验证已完成，等待人工与独立源码审查；未进入W6-D4。**
