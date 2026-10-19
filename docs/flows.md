# CampusFix 主要业务流程（W1-D1 草案）

> 对齐 `docs/SRS.md` 第 46 章；本文件只做文字梳理，W1-D3 再细化成正式状态机、泳道图及对应测试，不提前修改业务逻辑。

## 1. 标准流程

1. 报修人创建工单 `DRAFT`，完成输入校验后提交为 `PENDING_REVIEW`。
2. 调度管理员审核通过：`PENDING_REVIEW → PENDING_DISPATCH`。
3. 系统计算技能、区域、负载等匹配候选人，展示前三名与推荐理由（只推荐，不改工单状态）。
4. 调度管理员确认派单：`PENDING_DISPATCH → PENDING_ACCEPT`，持久化派单记录。
5. 被分配维修人员本人接单：`PENDING_ACCEPT → IN_PROGRESS`。
6. 维修人员提交处理说明：`IN_PROGRESS → PENDING_VERIFY`。
7. 原报修人验收：`PENDING_VERIFY → COMPLETED`；可评价一次。
8. 调度管理员归档：`COMPLETED → CLOSED`。

## 2. 退回补充

`PENDING_REVIEW → PENDING_SUPPLEMENT → PENDING_REVIEW`。退回必须说明原因；只允许原报修人补充；保留原编号及修改记录。

## 3. 拒单重派

`PENDING_ACCEPT → PENDING_DISPATCH`。维修人员必须填写拒单原因；结束原有效分配记录并释放该人员任务负载；管理员随后再次推荐和派单。

## 4. 验收失败返工

`PENDING_VERIFY → IN_PROGRESS`。原报修人写明未通过原因，原维修人员继续处理；必须保留本次验收与维修的历史记录。

## 5. 取消与异常

- `DRAFT`、`PENDING_REVIEW`、`PENDING_SUPPLEMENT`：报修人可按规则取消。
- `PENDING_DISPATCH`：调度管理员可附原因取消。
- `CLOSED` 和 `CANCELLED`：P0 不恢复、不修改主体数据。
- 无合格调度候选人：保持 `PENDING_DISPATCH` 并返回排除原因，不得强行派单。
- P0 不自动验收，不允许绕过资质或容量硬约束派单。

## 6. W1-D3 待细化点

- 每个按钮对应的操作者、必要请求字段和错误提示。
- 重复派单、重复接单和重复验收如何给出冲突响应。
- 当前状态与派单记录之间的事务一致性。
- 紧急工单人工审核后的队列排序，以及现场安全警示。
