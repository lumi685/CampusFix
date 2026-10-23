# CampusFix REST API 契约

> **版本**：V1.0（W1-D5 P0 冻结基线）
>
> **范围**：13 项 P0 的 HTTP 契约；本文是设计，不表示接口或测试已经实现。
> **依据**：`docs/SRS.md` 第 45～56 章、`docs/requirements-trace.md`、`docs/flows.md`、`docs/roles.md`、`docs/database-design.md`。

## 1. 通用协议

### 1.1 请求与认证

- Base URL：`/api`；JSON 和错误消息使用 UTF-8；时间使用 UTC ISO-8601。
- 除登录外均使用 `Authorization: Bearer <session-token>`。服务端只保存 Token 哈希，退出或过期后返回 401。
- `reporter_id`、`operator_id`、角色、目标状态、当前负责人和得分均由服务端推断或计算；即使客户端传入也必须忽略或拒绝。
- 状态写操作的请求体必须包含正整数 `expected_version`。成功后响应返回递增后的 `version`。
- `POST` 按钮在请求进行时禁用。服务端仍以状态、版本、唯一约束和事务处理重复请求；本稿不引入额外幂等键功能。
- 分页参数：`page>=1`，`1<=page_size<=100`；默认 `page=1&page_size=20`。

### 1.2 统一响应

成功：

```json
{"code":0,"message":"success","data":{},"request_id":"req_01J..."}
```

分页的 `data` 固定为：

```json
{"items":[],"page":1,"page_size":20,"total":0}
```

失败：

```json
{"code":"ORDER_STATUS_CONFLICT","message":"工单状态已变化，请刷新后重试","data":null,"request_id":"req_01J..."}
```

### 1.3 HTTP 与业务错误码

| HTTP | 业务码 | 语义 |
|---:|---|---|
| 400 | `MALFORMED_REQUEST` | JSON/查询参数无法解析 |
| 401 | `AUTH_REQUIRED`、`AUTH_INVALID`、`AUTH_EXPIRED` | 未登录、会话无效或过期 |
| 403 | `ACTION_FORBIDDEN` | 已登录但角色没有该动作权限 |
| 404 | `RESOURCE_NOT_FOUND` | 资源不存在，或为避免泄露而隐藏越权资源 |
| 409 | `ORDER_STATUS_CONFLICT` | 当前状态不允许该动作 |
| 409 | `ORDER_VERSION_CONFLICT` | `expected_version` 已过期 |
| 409 | `ACTIVE_ASSIGNMENT_CONFLICT` | 已存在有效派单；重复/并发派单 |
| 409 | `WORKER_CAPACITY_CONFLICT` | 派单事务中容量已被并发请求占用 |
| 409 | `DUPLICATE_OPERATION` | 重复提交、待处理转派或重复评价 |
| 422 | `VALIDATION_ERROR` | 字段缺失、长度/范围/格式不合法 |
| 422 | `WORKER_INELIGIBLE` | 技能、资质、在岗、区域或容量硬约束不满足 |
| 413 | `FILE_TOO_LARGE` | 单文件超过 5 MiB |
| 415 | `UNSUPPORTED_FILE_TYPE` | 扩展名或实际 MIME 不允许 |
| 429 | `RATE_LIMITED` | 关键接口请求过快 |
| 500 | `INTERNAL_ERROR` | 服务异常；只返回 `request_id`，不暴露堆栈 |
| 503 | `DATABASE_BUSY` | SQLite 短时繁忙且有限重试失败 |

权限判断顺序为认证 → 动作权限 → 资源范围 → 业务前置 → 版本/并发 → 事务 → 响应过滤。已知 ID 的跨工单读取默认用 404 隐藏资源存在性；对当前用户明确可见资源上的无动作权限使用 403。

## 2. 响应字段白名单

### 2.1 工单字段分层

| 受众 | 可返回字段 |
|---|---|
| 列表通用 | `id, order_no, title, category{id,code,name}, location{id,code,name}, requested_urgency, approved_urgency, status, version, created_at, submitted_at, updated_at` |
| 原报修人 | 列表通用 + `description, expected_visit_at, contact_name, contact_phone, current_assignment{worker_id,worker_name}, attachments`；只返回公开时间线 |
| 当前维修员 | 列表通用 + 完成任务所需的 `description, expected_visit_at, contact_name, contact_phone, report attachments, current_assignment`；不返回内部审核/调度字段 |
| 历史维修员 | 仅本人历史任务所需的最小公开字段和本人参与记录；不再返回联系电话或新内部数据 |
| 授权管理人员 | 通用详情 + 联系字段（确有业务需要时）+ `review, dispatch, transfer` 管理视图；仍不返回密码、Token、文件路径 |

### 2.2 时间线字段白名单

普通报修人和维修员只收到：

```json
{"id":91,"action":"ORDER_RETURNED","from_status":"PENDING_REVIEW","to_status":"PENDING_SUPPLEMENT","public_note":"请补充具体楼层","actor":{"id":8,"display_name":"调度员A"},"created_at":"2026-10-19T08:30:00.000Z"}
```

区域内 dispatcher，或同时具备业务范围与 `INTERNAL_NOTE_READ` 明确授权的 sysadmin，事件才可额外返回：

```json
{"internal_note":"疑似重复报修，已电话核验","management_detail":{"assignment_id":501,"worker_id":18,"assign_reason":"同楼值班","candidate_rank":4}}
```

管理扩展仅可按事件返回适用字段：审核类 `review_action/category_id/approved_urgency`；派单类 `assignment_id/worker_id/assign_reason/candidate_rank`；转派类 `transfer_request_id/decision`；返工类 `rework_count`。禁止返回 `password_hash`、`token_hash`、`stored_path`、原始 `metadata_json`、其他用户私人联系方式、未授权完整候选评分。后端必须构造角色专用 DTO，不能先发完整对象再让前端隐藏。

## 3. P0 端点总表

| ID | 方法与 URI | FR | 允许主体 | 状态变化 |
|---|---|---|---|---|
| API-01 | `POST /api/auth/login` | FR-001 | 匿名 | 无 |
| API-02 | `POST /api/auth/logout` | FR-001 | 已登录本人 | 无 |
| API-03 | `GET /api/auth/me` | FR-001、FR-002 | 已登录本人 | 无 |
| API-04 | `GET /api/locations` | FR-003 | 已登录 | 无 |
| API-05 | `GET /api/fault-categories` | FR-003 | 已登录 | 无 |
| API-06 | `POST /api/work-orders` | FR-003 | `reporter`、`worker` 本人 | 无 → `DRAFT` |
| API-07 | `PUT /api/work-orders/{id}` | FR-003、FR-005 | 原报修人 | 状态不变 |
| API-08 | `POST /api/work-orders/{id}/submit` | FR-003、FR-005 | 原报修人 | `DRAFT/PENDING_SUPPLEMENT → PENDING_REVIEW` |
| API-09 | `GET /api/work-orders` | FR-002、FR-004 | 按范围 | 无 |
| API-10 | `GET /api/work-orders/{id}` | FR-002、FR-004 | 按范围 | 无 |
| API-11 | `GET /api/work-orders/{id}/timeline` | FR-002、FR-004 | 按范围 | 无 |
| API-12 | `POST /api/work-orders/{id}/cancel` | FR-003、FR-005、FR-010 | 原报修人或授权管理人员 | T-07/T-08/T-09/T-12 |
| API-13 | `POST /api/work-orders/{id}/review` | FR-005 | 授权管理人员 | T-03/T-04/T-06 |
| API-14 | `GET /api/workers` | FR-006 | 授权管理人员；worker 只读本人 | 无 |
| API-15 | `GET /api/workers/{id}/workload` | FR-006 | 授权管理人员；worker 只读本人 | 无 |
| API-16 | `PUT /api/workers/{id}` | FR-006 | 授权管理人员 | 无工单状态变化 |
| API-17 | `GET /api/work-orders/{id}/dispatch-candidates` | FR-007 | 授权管理人员 | T-10，只读业务状态 |
| API-18 | `POST /api/work-orders/{id}/assign` | FR-008、FR-013 | 授权管理人员 | T-11 |
| API-19 | `POST /api/work-orders/{id}/accept` | FR-008 | 当前负责人 | T-13 |
| API-20 | `POST /api/work-orders/{id}/refuse` | FR-008 | 当前负责人 | T-14 |
| API-21 | `POST /api/work-orders/{id}/transfer-requests` | FR-008 | 当前负责人 | T-15/T-16，状态不变 |
| API-22 | `POST /api/work-orders/{id}/transfer-requests/{request_id}/decision` | FR-008、FR-013 | 授权管理人员 | 拒绝不变；批准 T-17/T-18 |
| API-23 | `POST /api/work-orders/{id}/complete` | FR-009 | 当前负责人 | T-19 |
| API-24 | `POST /api/work-orders/{id}/verify` | FR-009 | 原报修人 | T-20/T-21 |
| API-25 | `POST /api/work-orders/{id}/evaluations` | FR-010 | 原报修人 | T-22，状态不变 |
| API-26 | `GET /api/work-orders/{id}/evaluations` | FR-010 | 按工单范围 | 无 |
| API-27 | `POST /api/work-orders/{id}/close` | FR-010 | 授权管理人员 | T-23 |
| API-28 | `POST /api/files/upload` | FR-011 | 有修改权限的报修人/当前维修员 | 无 |
| API-29 | `GET /api/files/{id}` | FR-002、FR-011 | 按工单范围 | 无 |
| API-30 | `DELETE /api/files/{id}` | FR-011 | 上传者且处于可编辑阶段 | 无 |
| API-31 | `GET /api/dashboard/summary` | FR-012 | 授权管理人员 | 无 |
| API-32 | `GET /api/statistics/work-orders` | FR-012 | 授权管理人员 | 无 |
| API-33 | `POST /api/work-orders/{id}/progress-events` | FR-004、FR-009 | 当前负责人 | E-01/E-02，状态不变 |

位置和分类只读端点是创建工单的必要基础数据，不提供 P0 管理 CRUD。系统账号/授权后台属于 P1；P0 的显式授权由受控初始化数据建立。

## 4. 认证与基础数据

### API-01 登录

- **请求**：`POST /api/auth/login`；匿名；关联 FR-001，`TC-AUTH-01`、`TC-AUTH-02`。
- **请求体**：`username` 3～64 字符、`password` 1～128 字符，均必填。
- **成功 200**：`{"code":0,"message":"success","data":{"token":"cf_live_...","expires_at":"2026-10-20T08:00:00.000Z","user":{"id":12,"display_name":"李同学","role":"reporter"}},"request_id":"req_1"}`
- **失败示例 401**：`{"code":"AUTH_INVALID","message":"用户名或密码错误","data":null,"request_id":"req_1"}`；禁用账户同样使用安全提示，不泄露账号存在性。
- **DB/事务**：验证 `users.password_hash/status`，创建 `auth_sessions`（只存 Token 哈希）并更新 `last_login_at`；会话插入和登录时间更新同事务。

### API-02 退出

- **请求**：`POST /api/auth/logout`；任一已登录本人；FR-001，`TC-AUTH-03`；无请求体。
- **成功 200**：`{"code":0,"message":"success","data":{"revoked":true},"request_id":"req_2"}`。
- **失败示例 401**：`{"code":"AUTH_REQUIRED","message":"请先登录","data":null,"request_id":"req_2"}`。
- **DB/事务**：仅将当前 `auth_sessions.revoked_at` 条件更新为当前 UTC；不得撤销其他会话。重复使用已撤销 Token 按 401 处理。

### API-03 当前用户

- **请求**：`GET /api/auth/me`；已登录本人；FR-001、FR-002，`TC-AUTH-01`、`TC-AUTH-04`。
- **成功 200**：`{"code":0,"message":"success","data":{"id":12,"username":"reporter01","display_name":"李同学","role":"reporter","capabilities":[],"scope_location_ids":[]},"request_id":"req_3"}`。
- **失败示例 401**：`{"code":"AUTH_EXPIRED","message":"登录已过期","data":null,"request_id":"req_3"}`。
- **DB**：只读 `users`、有效会话与 `management_grants`；`worker` 可附 `worker_id`。不返回密码/Token 哈希。

### API-04 位置选项

- **请求**：`GET /api/locations?parent_id=1`；任一已登录用户；FR-003，`TC-ORDER-02`。
- **成功 200**：`{"code":0,"message":"success","data":{"items":[{"id":10,"code":"BLDG-A","name":"A栋","location_type":"BUILDING","parent_id":1}]},"request_id":"req_4"}`。
- **失败示例 422**：`{"code":"VALIDATION_ERROR","message":"parent_id 必须为正整数","data":{"field":"parent_id"},"request_id":"req_4"}`。
- **DB**：只读 `locations.active=1`；不返回停用位置供新工单选择。

### API-05 故障分类选项

- **请求**：`GET /api/fault-categories?parent_id=2`；任一已登录用户；FR-003，`TC-ORDER-02`。
- **成功 200**：`{"code":0,"message":"success","data":{"items":[{"id":21,"code":"ELECTRIC_SOCKET","name":"插座故障","default_priority":"NORMAL"}]},"request_id":"req_5"}`。
- **失败示例 401**：`{"code":"AUTH_REQUIRED","message":"请先登录","data":null,"request_id":"req_5"}`。
- **DB**：只读 `fault_categories.active=1`；不向普通用户返回内部技能/资质配置。

## 5. 工单创建、查询、取消与审核

### API-06 创建草稿

- **请求**：`POST /api/work-orders`；`reporter` 或 `worker` 以本人身份；FR-003，`TC-ORDER-01`、`TC-ORDER-02`、`TC-ORDER-03`、`RBAC-D3-11`。
- **请求体**：`category_id`、`location_id` 为正整数；`title` 1～120；`description` 1～2000；`requested_urgency` 为 `LOW/NORMAL/HIGH/URGENT`；`contact_name` 1～80；`contact_phone` 1～32；`expected_visit_at` 可选 UTC。禁止 `reporter_id/status/order_no`。
- **成功 201**：`{"code":0,"message":"success","data":{"id":101,"order_no":null,"status":"DRAFT","version":1},"request_id":"req_6"}`。
- **失败示例 422**：`{"code":"VALIDATION_ERROR","message":"故障位置无效或已停用","data":{"field":"location_id"},"request_id":"req_6"}`。
- **状态/DB**：无 → `DRAFT`；事务插入 `work_orders` 和创建事件 `status_logs`。服务端以认证用户写 `reporter_id`。P0 页面可直接随后调用 submit，不提供复杂草稿管理体验。

### API-07 编辑草稿或待补充工单

- **请求**：`PUT /api/work-orders/{id}`；仅原报修人；FR-003、FR-005，`TC-WF-02`。
- **请求体**：与 API-06 可编辑字段相同，全部传入；另含 `expected_version`。不接受状态、审核后紧急度或操作者。
- **成功 200**：`{"code":0,"message":"success","data":{"id":101,"status":"PENDING_SUPPLEMENT","version":4,"updated_at":"2026-10-19T09:00:00.000Z"},"request_id":"req_7"}`。
- **失败示例 409**：`{"code":"ORDER_STATUS_CONFLICT","message":"仅草稿或待补充工单可编辑","data":{"current_status":"PENDING_REVIEW"},"request_id":"req_7"}`。
- **状态/DB**：仅 `DRAFT`、`PENDING_SUPPLEMENT`，状态不变；条件更新 `work_orders/version` 并追加 `ORDER_CONTENT_UPDATED` 日志，待补充修改摘要公开可见。

### API-08 正式提交/补充后再提交

- **请求**：`POST /api/work-orders/{id}/submit`；原报修人；FR-003、FR-005，`TC-ORDER-01`、`TC-ORDER-03`、`WF-D3-01`、`WF-D3-02`。
- **请求体**：`{"expected_version":1}`；正式提交时重新校验所有必填字段。
- **成功 200**：`{"code":0,"message":"success","data":{"id":101,"order_no":"WX202610190001","status":"PENDING_REVIEW","version":2,"submitted_at":"2026-10-19T09:05:00.000Z"},"request_id":"req_8"}`。
- **失败示例 409**：`{"code":"ORDER_VERSION_CONFLICT","message":"工单已被修改，请刷新后重试","data":{"current_version":2},"request_id":"req_8"}`。
- **状态/DB**：`DRAFT` 或 `PENDING_SUPPLEMENT` → `PENDING_REVIEW`。事务内原子分配日期流水（首次提交）、条件更新工单、写日志；重复提交不产生新编号或日志。

### API-09 工单列表

- **请求**：`GET /api/work-orders?view=reported&page=1&page_size=20&status=PENDING_REVIEW&category_id=21`；全部角色按数据范围；FR-002、FR-004，`TC-ORDER-04`、`TC-ORDER-06`、`TC-SEC-02`、`TC-SEC-05`。`view` 为 `reported/assigned/managed`：reporter 只能用 reported，worker 可用 reported 或 assigned，管理人员只能用 managed；不匹配当前身份时返回 403。
- **成功 200**：`{"code":0,"message":"success","data":{"items":[{"id":101,"order_no":"WX202610190001","title":"宿舍插座无电","status":"PENDING_REVIEW","version":2,"category":{"id":21,"name":"插座故障"},"location":{"id":33,"name":"A栋301"},"created_at":"2026-10-19T09:00:00.000Z"}],"page":1,"page_size":20,"total":1},"request_id":"req_9"}`。
- **失败示例 403**：`{"code":"ACTION_FORBIDDEN","message":"无权查看该管理范围","data":null,"request_id":"req_9"}`。
- **DB**：只读。reported 按 `reporter_id`；assigned 按当前/受控历史派单；managed 按授权区域，sysadmin 还需显式 `BUSINESS_READ`。不同视图不隐式合并，避免维修任务混入本人报修列表。列表不返回联系方式、内部备注或完整描述。

### API-10 工单详情

- **请求**：`GET /api/work-orders/{id}`；按资源范围；FR-002、FR-004，`TC-ORDER-05`、`TC-ORDER-06`。
- **成功 200**：`{"code":0,"message":"success","data":{"id":101,"order_no":"WX202610190001","title":"宿舍插座无电","description":"两个插孔均无电","status":"PENDING_ACCEPT","version":5,"category":{"id":21,"name":"插座故障"},"location":{"id":33,"name":"A栋301"},"current_assignment":{"worker_id":18,"worker_name":"维修员A"},"attachments":[]},"request_id":"req_10"}`。
- **失败示例 404**：`{"code":"RESOURCE_NOT_FOUND","message":"工单不存在","data":null,"request_id":"req_10"}`。
- **DB**：只读 `work_orders`、当前有效 `assignments`、附件元数据及受众所需关联；严格使用第 2 节白名单。

### API-11 工单时间线

- **请求**：`GET /api/work-orders/{id}/timeline?page=1&page_size=50`；按工单范围；FR-002、FR-004、FR-010，`TC-ORDER-05`、`TC-ORDER-06`、`RBAC-D3-10`、`RBAC-D3-12`。
- **成功 200**：`{"code":0,"message":"success","data":{"items":[{"id":91,"action":"ORDER_SUBMITTED","from_status":"DRAFT","to_status":"PENDING_REVIEW","public_note":null,"actor":{"id":12,"display_name":"李同学"},"created_at":"2026-10-19T09:05:00.000Z"}],"page":1,"page_size":50,"total":1},"request_id":"req_11"}`。
- **失败示例 404**：`{"code":"RESOURCE_NOT_FOUND","message":"工单不存在","data":null,"request_id":"req_11"}`。
- **DB**：只读 `status_logs`；区域内 dispatcher 可获得管理字段白名单；sysadmin 仅在有 `INTERNAL_NOTE_READ` 且范围匹配时获得 `internal_note/management_detail` 白名单。

### API-12 取消工单

- **请求**：`POST /api/work-orders/{id}/cancel`；原报修人可取消 `DRAFT/PENDING_REVIEW/PENDING_SUPPLEMENT`；dispatcher 或具备 `CANCEL` 的 sysadmin 可在授权范围执行 T-07/T-08/T-09/T-12；FR-003、FR-005、FR-010，`WF-D3-03`。
- **请求体**：`expected_version` 必填；`reason` 在 `DRAFT` 可选，其他可取消状态为 1～500 字符必填。
- **成功 200**：`{"code":0,"message":"success","data":{"id":101,"status":"CANCELLED","version":3,"cancelled_at":"2026-10-19T09:10:00.000Z"},"request_id":"req_12"}`。
- **失败示例 422**：`{"code":"VALIDATION_ERROR","message":"已提交工单取消原因必填","data":{"field":"reason"},"request_id":"req_12"}`。
- **状态/DB**：允许状态 → `CANCELLED`；条件更新工单与追加日志同事务。`PENDING_ACCEPT` 及以后无 P0 取消路由；终态不可恢复。

### API-13 审核

- **请求**：`POST /api/work-orders/{id}/review`；授权区域 dispatcher，或有 `REVIEW` 能力与范围的 sysadmin；FR-005，`TC-WF-01`、`TC-WF-02`、`TC-WF-03`。
- **请求体**：`action` 为 `APPROVE/RETURN/REJECT`，`expected_version` 必填；APPROVE 需 `category_id, approved_urgency`，`internal_note` 可选；RETURN/REJECT 需 `reason` 1～500（作为可公开原因），`internal_note` 可选。
- **成功 200**：`{"code":0,"message":"success","data":{"id":101,"review_action":"APPROVE","status":"PENDING_DISPATCH","version":3},"request_id":"req_13"}`。
- **失败示例 409**：`{"code":"ORDER_STATUS_CONFLICT","message":"仅待审核工单可审核","data":{"current_status":"PENDING_DISPATCH"},"request_id":"req_13"}`。
- **状态/DB**：仅 `PENDING_REVIEW`；APPROVE → `PENDING_DISPATCH`，RETURN → `PENDING_SUPPLEMENT`，REJECT → `CANCELLED`。条件更新分类/紧急度/状态、时间字段与日志同事务；`internal_note` 不进入普通 DTO。

## 6. 维修人员与调度

### API-14 维修人员列表/本人档案

- **请求**：`GET /api/workers?page=1&page_size=20&availability=AVAILABLE&skill_id=3`；授权管理人员按区域，worker 只能用 `mine=true` 查本人；FR-006，`TC-WORKER-01`、`TC-WORKER-03`。
- **成功 200**：`{"code":0,"message":"success","data":{"items":[{"id":18,"worker_no":"W0018","display_name":"维修员A","service_area_id":10,"availability":"AVAILABLE","max_concurrent_tasks":3,"active_tasks":1,"skills":[{"skill_id":3,"level":5,"certified":true}]}],"page":1,"page_size":20,"total":1},"request_id":"req_14"}`。
- **失败示例 403**：`{"code":"ACTION_FORBIDDEN","message":"只能查看本人维修档案","data":null,"request_id":"req_14"}`。
- **DB**：只读 `workers/users/worker_skills`，实时聚合有效派单。worker 视图不返回其他人员或管理字段。

### API-15 实时负载

- **请求**：`GET /api/workers/{id}/workload`；授权管理人员按区域，worker 只读本人；FR-006，`TC-WORKER-03`。
- **成功 200**：`{"code":0,"message":"success","data":{"worker_id":18,"active_tasks":1,"max_concurrent_tasks":3,"remaining_capacity":2,"calculated_at":"2026-10-19T09:20:00.000Z"},"request_id":"req_15"}`。
- **失败示例 404**：`{"code":"RESOURCE_NOT_FOUND","message":"维修人员不存在","data":null,"request_id":"req_15"}`。
- **DB**：只读 `assignments WHERE ended_at IS NULL`；不读取前端缓存或冗余计数字段。

### API-16 更新维修档案

- **请求**：`PUT /api/workers/{id}`；dispatcher 按区域，或具备 `WORKER_MANAGE` 范围的 sysadmin；FR-006，`TC-WORKER-01`、`TC-WORKER-02`。
- **请求体**：`service_area_id`、`availability`、`max_concurrent_tasks`（1～100）、`emergency_duty`、`active`、`skills:[{skill_id,level:1..5,certified}]`；不允许改账号角色。
- **成功 200**：`{"code":0,"message":"success","data":{"id":18,"availability":"AVAILABLE","max_concurrent_tasks":3,"skills":[{"skill_id":3,"level":5,"certified":true}],"updated_at":"2026-10-19T09:30:00.000Z"},"request_id":"req_16"}`。
- **失败示例 422**：`{"code":"VALIDATION_ERROR","message":"技能熟练度必须为 1 到 5","data":{"field":"skills[0].level"},"request_id":"req_16"}`。
- **DB/事务**：校验区域/技能存在，在事务内更新 `workers` 并替换/更新 `worker_skills`。把容量降低到小于当前有效任务数时拒绝，避免制造已超载状态。

### API-17 推荐候选

- **请求**：`GET /api/work-orders/{id}/dispatch-candidates`；授权区域 dispatcher，或具备 `DISPATCH` 及相应范围的 sysadmin；FR-007，`TC-DSP-01～10`、`WF-D3-04`、`WF-D3-13`。sysadmin 若还需查看历史内部备注，另需 `INTERNAL_NOTE_READ`。
- **成功 200**：`{"code":0,"message":"success","data":{"order_id":101,"order_version":3,"strategy":"WEIGHTED_SCORE_V1","weights":{"skill":0.35,"area":0.20,"load":0.20,"efficiency":0.10,"rating":0.10,"duty":0.05},"top_candidates":[{"rank":1,"worker_id":18,"worker_name":"维修员A","total_score":80.3,"active_tasks":1,"max_concurrent_tasks":3,"score_detail":{"skill":100,"area":100,"load":66.7,"efficiency":60,"rating":60,"duty":0},"sample_counts":{"efficiency":2,"rating":1},"reason":"技能与区域匹配，仍有容量","risks":["历史样本不足"]}],"other_eligible_candidates":[{"rank":4,"worker_id":25,"worker_name":"维修员D","total_score":74.0,"active_tasks":0,"max_concurrent_tasks":2,"score_detail":{"skill":80,"area":70,"load":100,"efficiency":60,"rating":60,"duty":0},"sample_counts":{"efficiency":1,"rating":0},"reason":"满足全部硬约束","risks":["历史样本不足"]}],"excluded_summary":{"AT_CAPACITY":2}},"request_id":"req_17"}`。
- **候选 DTO**：两个候选数组均使用 `rank,worker_id,worker_name,total_score,active_tasks,max_concurrent_tasks,score_detail,sample_counts,reason,risks`。`other_eligible_candidates` 按同一排序规则返回第 4 名起的全部合格人员，P0 数据量无需另做分页。
- **无候选仍为 200**：`top_candidates=[]`、`other_eligible_candidates=[]` 且给出 `excluded_summary`；不是错误。
- **失败示例 409**：`{"code":"ORDER_STATUS_CONFLICT","message":"仅待派单工单可计算推荐","data":{"current_status":"PENDING_ACCEPT"},"request_id":"req_17"}`。
- **状态/DB**：`PENDING_DISPATCH → PENDING_DISPATCH`；评分与读取严格只读，不创建派单、不写 `dispatch_records` 或状态日志。效率采用最近 20 个正常完成派单及 `target_completion_minutes/accepted_at/result_submitted_at` 冻结公式，评价采用最近 20 条最终完成人员评价；各自样本少于 3 取 60。P0 使用固定权重，排班过滤不启用。

### API-18 确认派单

- **请求**：`POST /api/work-orders/{id}/assign`；dispatcher 或有 `DISPATCH` 范围的 sysadmin；FR-008、FR-013，`TC-WF-04`、`TC-WF-09`、`TC-CON-01～03`、`WF-D3-05`、`WF-D3-13`。
- **请求体**：`worker_id`、`expected_version` 必填；`reason` 最多 500 字符，人工选择非当前 top 3 合格人员时必填。不得传得分或目标状态。
- **成功 200**：`{"code":0,"message":"success","data":{"order_id":101,"assignment_id":501,"worker":{"id":18,"display_name":"维修员A"},"status":"PENDING_ACCEPT","version":4},"request_id":"req_18"}`。
- **失败示例 409**：`{"code":"ACTIVE_ASSIGNMENT_CONFLICT","message":"工单已被其他请求派单","data":{"current_status":"PENDING_ACCEPT"},"request_id":"req_18"}`。
- **失败示例 422**：`{"code":"WORKER_INELIGIBLE","message":"维修人员任务量已满","data":{"reason":"AT_CAPACITY"},"request_id":"req_18"}`。
- **状态/DB**：`PENDING_DISPATCH → PENDING_ACCEPT`；短 `BEGIN IMMEDIATE` 事务内重查状态/version、技能/资质/区域/可用性/实时容量并重新计算最终排名；校验非 Top 3 理由，插入 `assignments` 与绑定该派单的 `dispatch_records` 最终权重/候选/选中人员快照，条件更新工单并写日志。部分唯一索引或容量竞争统一映射为 409；绝不产生第二条有效派单或突破最后一个容量。

## 7. 接单、拒单、转派、维修与验收

### API-19 接单

- **请求**：`POST /api/work-orders/{id}/accept`；仅当前有效负责人；FR-008，`TC-WF-05`、`TC-WF-06`、`WF-D3-06`。
- **请求体**：`{"expected_version":4}`。
- **成功 200**：`{"code":0,"message":"success","data":{"order_id":101,"status":"IN_PROGRESS","version":5,"accepted_at":"2026-10-19T10:00:00.000Z"},"request_id":"req_19"}`。
- **失败示例 404**：`{"code":"RESOURCE_NOT_FOUND","message":"任务不存在","data":null,"request_id":"req_19"}`（非负责人不泄露任务）。
- **状态/DB**：`PENDING_ACCEPT → IN_PROGRESS`；条件更新当前 `assignments.accepted_at/version`、工单与状态日志同事务。

### API-20 拒单

- **请求**：`POST /api/work-orders/{id}/refuse`；仅当前有效负责人；FR-008，`TC-WF-07`、`WF-D3-07`。
- **请求体**：`reason` 1～500、`expected_version` 必填。
- **成功 200**：`{"code":0,"message":"success","data":{"order_id":101,"status":"PENDING_DISPATCH","version":5,"assignment_ended":true},"request_id":"req_20"}`。
- **失败示例 422**：`{"code":"VALIDATION_ERROR","message":"拒单原因必填","data":{"field":"reason"},"request_id":"req_20"}`。
- **状态/DB**：`PENDING_ACCEPT → PENDING_DISPATCH`；同事务结束有效派单（`end_reason=REFUSED`）、将该派单仍为 PENDING 的转派申请标为 `SUPERSEDED`、条件更新工单并写日志，实时负载随之释放。

### API-21 申请转派

- **请求**：`POST /api/work-orders/{id}/transfer-requests`；仅当前有效负责人；FR-008，`TC-WF-08`、`WF-D3-08`、`WF-D3-09`。
- **请求体**：`reason` 1～500、`expected_version` 必填。
- **成功 201**：`{"code":0,"message":"success","data":{"transfer_request_id":701,"order_id":101,"status":"PENDING","order_status":"IN_PROGRESS"},"request_id":"req_21"}`。
- **失败示例 409**：`{"code":"DUPLICATE_OPERATION","message":"已有待处理转派申请","data":{"transfer_request_id":701},"request_id":"req_21"}`。
- **状态/DB**：仅 `PENDING_ACCEPT` 或 `IN_PROGRESS`，工单状态和负责人不变；事务插入 `transfer_requests` 和同状态日志事件。每个有效派单最多一个 PENDING 申请。

### API-22 审批转派

- **请求**：`POST /api/work-orders/{id}/transfer-requests/{request_id}/decision`；授权区域 dispatcher 或有 `DISPATCH` 范围的 sysadmin；FR-008、FR-013，`TC-WF-08`、`TC-WF-09`、`WF-D3-08`、`WF-D3-09`。
- **请求体**：`decision` 为 `APPROVE/REJECT`，`reason` 1～500，`expected_version` 必填。
- **成功 200（批准）**：`{"code":0,"message":"success","data":{"transfer_request_id":701,"decision":"APPROVE","order_status":"PENDING_DISPATCH","version":7,"assignment_ended":true},"request_id":"req_22"}`。
- **成功 200（拒绝）**：相同结构但 `decision=REJECT`、`order_status` 与 `version` 保持不变；追加审批事件。
- **失败示例 409**：`{"code":"ORDER_VERSION_CONFLICT","message":"任务已发生变化，请刷新后重试","data":{"current_version":7},"request_id":"req_22"}`。
- **状态/DB**：仅处理与当前有效派单相连的 PENDING 申请。拒绝只更新申请并写日志；批准则在同事务更新申请及 `resolved_at`、结束派单、`PENDING_ACCEPT/IN_PROGRESS → PENDING_DISPATCH`、递增版本并写日志。旧维修历史保留。没有 PENDING 申请时不得调用本接口实现管理员主动改派；该能力属于 P1。

### API-23 提交维修结果

- **请求**：`POST /api/work-orders/{id}/complete`；仅当前有效负责人；FR-009，`TC-WF-10`、`TC-WF-13`。
- **请求体**：`repair_summary` 1～2000、`expected_version` 必填；维修结果图片先经 API-28 上传并以关联元数据展示，无需信任附件 ID 数组改变归属。
- **成功 200**：`{"code":0,"message":"success","data":{"order_id":101,"status":"PENDING_VERIFY","version":8,"repair_summary":"更换损坏插座并测试正常"},"request_id":"req_23"}`。
- **失败示例 422**：`{"code":"VALIDATION_ERROR","message":"维修说明必填","data":{"field":"repair_summary"},"request_id":"req_23"}`。
- **状态/DB**：`IN_PROGRESS → PENDING_VERIFY`；同事务保存当前摘要、更新有效派单 `result_submitted_at`、条件更新状态/version、追加含结果快照的日志，并把该派单尚未处理的转派申请标记为 `SUPERSEDED`、写 `resolved_at` 与系统原因。有效派单继续保留，若验收不通过仍由原负责人返工；再次提交会更新最终结果时间。验收通过时才结束并释放负载。

### API-24 验收或返工

- **原报修人资格（D-017 勘误）**：主要角色为 reporter 或 worker，且服务端读取的 `work_orders.reporter_id` 等于认证用户 ID；当前维修负责人身份和管理授权均不能替代该归属。状态、版本和重复操作限制照常适用。

- **请求**：`POST /api/work-orders/{id}/verify`；仅原报修人；FR-009，`TC-WF-11～13`、`WF-D3-10`、`WF-D3-11`。
- **请求体**：`approved` 布尔、`expected_version`；`approved=false` 时 `reason` 1～500 必填，true 时原因可选。
- **成功 200**：`{"code":0,"message":"success","data":{"order_id":101,"approved":false,"status":"IN_PROGRESS","version":9,"rework_count":1},"request_id":"req_24"}`。
- **失败示例 422**：`{"code":"VALIDATION_ERROR","message":"验收不通过原因必填","data":{"field":"reason"},"request_id":"req_24"}`。
- **状态/DB**：仅 `PENDING_VERIFY`；通过 → `COMPLETED`、设 `completed_at` 并结束有效派单（`end_reason=COMPLETED`）以释放负载；不通过 → `IN_PROGRESS`、`rework_count+1`，保留有效负责人和旧维修结果。派单、工单更新与日志同事务。

## 8. 评价与关闭

### API-25 创建评价

- **原报修人资格（D-017 勘误）**：主要角色为 reporter 或 worker，且服务端读取的 `work_orders.reporter_id` 等于认证用户 ID；worker 只能评价自己报修的工单，管理人员不得代评价。状态、版本与每单一次限制照常适用。

- **请求**：`POST /api/work-orders/{id}/evaluations`；仅原报修人；FR-010，`TC-WF-14`、`TC-WF-15`、`WF-D3-12`、`WF-D3-14`。
- **请求体**：`score` 整数 1～5；`comment` 可选、最多 1000 字符；`expected_version` 必填但评价成功不递增工单版本。
- **成功 201**：`{"code":0,"message":"success","data":{"id":801,"order_id":101,"score":5,"comment":"处理及时","created_at":"2026-10-19T12:00:00.000Z"},"request_id":"req_25"}`。
- **失败示例 409**：`{"code":"DUPLICATE_OPERATION","message":"该工单已评价","data":null,"request_id":"req_25"}`；`CLOSED` 使用 `ORDER_STATUS_CONFLICT`。
- **状态/DB**：只允许 `COMPLETED`；事务内校验版本/身份，插入 `evaluations` 和 `EVALUATION_CREATED` 同状态日志。唯一约束保证一次评价；状态保持 `COMPLETED`。

### API-26 查询评价

- **请求**：`GET /api/work-orders/{id}/evaluations`；按工单详情权限；FR-010，`TC-WF-14`、`TC-WF-16`。
- **成功 200**：`{"code":0,"message":"success","data":{"evaluation":{"id":801,"score":5,"comment":"处理及时","created_at":"2026-10-19T12:00:00.000Z"}},"request_id":"req_26"}`；未评价时 `evaluation:null`。
- **失败示例 404**：`{"code":"RESOURCE_NOT_FOUND","message":"工单不存在","data":null,"request_id":"req_26"}`。
- **DB**：只读 `evaluations`，继承工单数据范围；不开放 P0 修改/删除。

### API-27 归档关闭

- **请求**：`POST /api/work-orders/{id}/close`；授权区域 dispatcher 或有 `CLOSE` 范围的 sysadmin；FR-010，`TC-WF-16`、`WF-D3-14`。
- **请求体**：`expected_version` 必填，`note` 可选、最多 500 字符。
- **成功 200**：`{"code":0,"message":"success","data":{"order_id":101,"status":"CLOSED","version":10,"closed_at":"2026-10-19T13:00:00.000Z"},"request_id":"req_27"}`。
- **失败示例 409**：`{"code":"ORDER_STATUS_CONFLICT","message":"仅已完成工单可关闭","data":{"current_status":"PENDING_VERIFY"},"request_id":"req_27"}`。
- **状态/DB**：`COMPLETED → CLOSED`；评价不是前置。有效派单应已在验收通过时结束；关闭事务条件更新工单并写日志，若仍发现有效派单则按一致性错误回滚而非静默修复。关闭后拒绝评价、编辑、派单、验收及恢复。

## 9. 附件

### API-28 上传图片

- **请求**：`POST /api/files/upload`，`multipart/form-data`；原报修人在 `DRAFT/PENDING_SUPPLEMENT` 上传 `purpose=REPORT`，当前负责人在 `IN_PROGRESS` 上传 `purpose=REPAIR_RESULT`；FR-011，`TC-FILE-01～03`。
- **字段**：`order_id` 正整数、`purpose` 枚举、`file` 必填；扩展名和实际 MIME 必须是 JPG/JPEG/PNG/WEBP，单张 ≤5 MiB。每单 `REPORT` 有效图片 ≤3，`REPAIR_RESULT` 有效图片也独立 ≤3。
- **成功 201**：`{"code":0,"message":"success","data":{"id":901,"order_id":101,"purpose":"REPORT","original_name":"socket.jpg","mime":"image/jpeg","size_bytes":245120,"download_url":"/api/files/901"},"request_id":"req_28"}`。
- **失败示例 415**：`{"code":"UNSUPPORTED_FILE_TYPE","message":"仅支持 JPG、JPEG、PNG、WEBP 图片","data":null,"request_id":"req_28"}`。
- **DB/文件**：先写隔离临时文件并验证真实内容；事务内重查工单权限/状态/数量并插入 `attachments`，再原子移动为随机路径；失败补偿，不在响应中返回 `stored_path`。

### API-29 下载图片

- **请求**：`GET /api/files/{id}`；继承关联工单详情权限；FR-002、FR-011，`TC-FILE-01`、`TC-FILE-04`、`RBAC-D3-09`。
- **成功 200**：返回图片字节，`Content-Type` 使用数据库中已验证 MIME，`Content-Disposition: inline; filename*=UTF-8''...`；不返回 JSON。
- **失败示例 404 JSON**：`{"code":"RESOURCE_NOT_FOUND","message":"附件不存在","data":null,"request_id":"req_29"}`（不存在、已删除、磁盘丢失或越权均不泄露路径）。
- **DB/文件**：先查附件及工单并鉴权，再由服务端安全路径读取；禁止用客户端路径拼接。

### API-30 删除未提交图片

- **请求**：`DELETE /api/files/{id}`；上传者且仍拥有相应编辑权；FR-011，`TC-FILE-02`、`TC-FILE-03`。
- **成功 200**：`{"code":0,"message":"success","data":{"id":901,"deleted":true},"request_id":"req_30"}`。
- **失败示例 409**：`{"code":"ORDER_STATUS_CONFLICT","message":"当前工单阶段不允许删除该附件","data":null,"request_id":"req_30"}`。
- **DB/文件**：REPORT 仅在 `DRAFT/PENDING_SUPPLEMENT` 删除，REPAIR_RESULT 仅在 `IN_PROGRESS` 且为当前负责人删除；先逻辑删除元数据，文件延迟清理。已进入历史的附件不允许普通删除。

## 10. 看板与基础统计

### API-31 看板摘要

- **请求**：`GET /api/dashboard/summary`；dispatcher 按区域，sysadmin 需 `STATS_VIEW` 范围；FR-012，`TC-STAT-01`、`TC-STAT-03`。
- **成功 200**：`{"code":0,"message":"success","data":{"status_counts":{"PENDING_REVIEW":4,"PENDING_DISPATCH":3,"PENDING_ACCEPT":2,"IN_PROGRESS":5,"PENDING_VERIFY":1,"COMPLETED":6},"worker_load":[{"worker_id":18,"worker_name":"维修员A","active_tasks":1,"max_concurrent_tasks":3}],"calculated_at":"2026-10-19T14:00:00.000Z"},"request_id":"req_31"}`。
- **失败示例 403**：`{"code":"ACTION_FORBIDDEN","message":"无权查看该区域统计","data":null,"request_id":"req_31"}`。
- **DB**：只读聚合，所有 WHERE 条件先应用授权位置范围；空数据返回零和空数组，不返回跨区总量。

### API-32 工单基础统计

- **请求**：`GET /api/statistics/work-orders?from=2026-10-01&to=2026-10-31&group_by=day`；dispatcher 按区域，sysadmin 需 `STATS_VIEW`；FR-012，`TC-STAT-01～03`。
- **约束**：日期必填且 `from<=to`，范围最多 366 天；`group_by=day/week/month`。
- **成功 200**：`{"code":0,"message":"success","data":{"range":{"from":"2026-10-01","to":"2026-10-31"},"series":[{"bucket":"2026-10-19","submitted":8,"currently_completed_or_closed":5}],"category_distribution":[{"category_id":21,"name":"插座故障","count":3}],"completion_rate":0.625,"definitions":{"cohort":"submitted_at 落在区间内的授权范围工单","completion_rate":"该队列当前为 COMPLETED 或 CLOSED 的数量/该队列总数"}},"request_id":"req_32"}`。
- **失败示例 422**：`{"code":"VALIDATION_ERROR","message":"统计日期范围不能超过 366 天","data":{"field":"to"},"request_id":"req_32"}`。
- **DB**：只读聚合、先做区域过滤；无数据返回空 series 与 `completion_rate:null`，避免除零。P0 不承诺实时高并发性能。

## 11. 维修过程事件

### API-33 到场/维修进度事件

- **请求**：`POST /api/work-orders/{id}/progress-events`；仅当前有效负责人且工单为 `IN_PROGRESS`；FR-004、FR-009，`TC-PROG-01～03`。
- **请求体**：`type` 为 `ARRIVED` 或 `PROGRESS_NOTE`，`expected_version` 必填但成功不递增工单版本；`ARRIVED` 的 `note` 可选且最多 500 字，`PROGRESS_NOTE` 的 `note` 必填且为 1～1000 字。
- **成功 201**：`{"code":0,"message":"success","data":{"id":950,"order_id":101,"assignment_id":501,"type":"ARRIVED","order_status":"IN_PROGRESS","order_version":6,"created_at":"2026-10-19T10:20:00.000Z"},"request_id":"req_33"}`。
- **失败示例 409**：`{"code":"DUPLICATE_OPERATION","message":"当前派单已记录到场","data":null,"request_id":"req_33"}`；非负责人返回隐藏资源的 404，非 `IN_PROGRESS` 返回 `ORDER_STATUS_CONFLICT`。
- **状态/DB**：E-01/E-02 均不改变工单状态或版本。在短事务中重查工单、版本和当前有效派单后，向 `status_logs` 插入关联 `assignment_id` 的 `WORKER_ARRIVED` 或 `WORKER_PROGRESS_NOTE`；部分唯一索引保证每个派单最多一次到场。失败不产生事件。

## 12. 状态—端点—事务核对

| W1-D3 动作 | API | 前置 → 结果 | 关键事务 |
|---|---|---|---|
| T-01 | API-06 | 无 → `DRAFT` | 工单 + 创建日志 |
| T-02/T-05 | API-08 | `DRAFT/PENDING_SUPPLEMENT → PENDING_REVIEW` | 流水号 + 工单 + 日志 |
| T-03/T-04/T-06 | API-13 | `PENDING_REVIEW → PENDING_DISPATCH/PENDING_SUPPLEMENT/CANCELLED` | 工单 + 日志 |
| T-07/T-08/T-09/T-12 | API-12 | 允许前期状态 → `CANCELLED` | 工单 + 日志 |
| T-10 | API-17 | `PENDING_DISPATCH` 不变 | 严格只读计算，不写快照 |
| T-11 | API-18 | `PENDING_DISPATCH → PENDING_ACCEPT` | 重算 + 最终快照 + 派单 + 工单 + 日志 |
| T-13/T-14 | API-19/API-20 | `PENDING_ACCEPT → IN_PROGRESS/PENDING_DISPATCH` | 派单 + 工单 + 日志 |
| T-15/T-16 | API-21 | 状态不变 | 转派申请 + 日志 |
| T-17/T-18 | API-22 | `PENDING_ACCEPT/IN_PROGRESS → PENDING_DISPATCH` | 申请 + 结束派单 + 工单 + 日志 |
| T-19 | API-23 | `IN_PROGRESS → PENDING_VERIFY` | 最终结果时间 + SUPERSEDED 申请 + 工单 + 日志 |
| T-20/T-21 | API-24 | `PENDING_VERIFY → COMPLETED/IN_PROGRESS` | 通过时结束派单；工单 + 返工计数 + 日志 |
| T-22 | API-25 | `COMPLETED` 不变 | 唯一评价 + 日志 |
| T-23 | API-27 | `COMPLETED → CLOSED` | 一致性检查 + 工单 + 日志 |
| E-01/E-02 | API-33 | `IN_PROGRESS` 不变 | 当前派单校验 + 追加事件日志 |

## 13. W1-D5 冻结结论

- API-01～API-33 是 P0 唯一接口编号基线；API-33 补齐到场/进度事件，不新增状态。
- 跨资源越权用 404 隐藏存在性；明确可见资源但无动作权限用 403。
- 推荐预览严格只读；非 Top 3 初次派单理由必填；管理员无申请主动改派为 P1。
- 单项评价、双用途各 3 张附件、验收释放负载、转派申请 `SUPERSEDED` 和内部字段白名单均已冻结。
- 本文只定义契约；所有接口和测试仍为待实现、待执行。
