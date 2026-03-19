# Tool Tiering Model

## Purpose

`RemoteMCP` 已经有了按 domain 分组的能力，例如：
- `level`
- `blueprint`
- `umg`

但对于长期自动化、多人部署和多种 AI client，这还不够。

还需要一套更明确的“工具分档模型”，帮助调用方回答：

- 这个工具适合什么使用场景？
- 这个工具风险高不高？
- 这个工具能不能链式调用？
- 这个工具是否可能中断当前会话？
- 这个工具当前是已验证、部分验证，还是已知高风险？

一句话：

`domain` 解决“这个工具是干什么的”，  
`tool tiering` 解决“这个工具该怎么安全地用”。

## Layers Of Classification

建议将每个工具至少放进下面 4 个维度中。

### 1. Domain

这是当前已经存在的分组：

- `level`
- `blueprint`
- `umg`
- other future domains

它表示：
- 功能域
- 工具大致服务的编辑器对象

### 2. Risk Tier

这是最关键的一层。

建议固定为：

- `safe`
  - 纯查询
  - 不改 editor 状态
  - 不影响当前会话稳定性

- `editor-stateful`
  - 会修改 editor state
  - 但不应中断当前会话
  - 适合小范围链式调用

- `session-disrupting`
  - 可能使当前 MCP session 失效
  - 调用方必须准备重连
  - 不能和普通原子工具按同一语义处理

### 3. Capability Tier

这是“能力成熟度/演进阶段”。

建议固定为：

- `P0`
  - 基础自动化底座
  - 必须优先稳定

- `P0.5`
  - 基础 contract / error / reconnect 强化

- `P1`
  - 高层可复用能力
  - 如 lighting rig / preset / readability wrapper

- `experimental`
  - 仍在探索
  - 不应默认进入关键工作流

### 4. Validation Status

这是“当前有没有被真实验证过”。

建议固定为：

- `validated`
  - 有真实运行验证
  - 可以进入默认 workflow

- `partial`
  - 已实现，但验证不足

- `known-risk`
  - 已知存在崩溃、竞态、强依赖 editor 状态等问题

## AI Usage Modes

工具还应该能映射到 AI 使用模式。

建议使用以下 5 种模式：

1. `advisory / read-only`
2. `sandbox prototyping`
3. `restricted co-building`
4. `workflow-node automation`
5. `high-privilege maintenance`

不是每个工具都要在返回里显式带这个字段，但至少在设计和文档上要考虑：
- 它主要服务哪几种模式
- 哪些模式不应该默认使用它

## Minimum Agent-Facing Semantics

对于新的结构化工具，建议最少都带这些语义：

- `risk_tier`
- `session_disrupted`
- `reconnect_required`
- `recommended_client_action`

推荐结果包：

```json
{
  "ok": true,
  "data": {},
  "warnings": [],
  "error_code": null,
  "message": "...",
  "risk_tier": "safe | editor-stateful | session-disrupting",
  "session_disrupted": false,
  "reconnect_required": false,
  "recommended_client_action": "continue | reconnect | retry_later | review"
}
```

## Recommended Defaults

### Query Tools

例如：
- `ping`
- `get_editor_state`
- `get_current_level`
- `find_actors_by_prefix`

建议默认：
- `risk_tier = safe`
  or `editor-stateful` when the query depends on live editor state
- `session_disrupted = false`
- `reconnect_required = false`
- `validation_status = validated` only after real runtime checks

### Scene Construction Tools

例如：
- `spawn_static_mesh_actor`
- `reset_testbed`
- `ensure_capture_camera`

建议默认：
- `risk_tier = editor-stateful`
- `session_disrupted = false`
- `reconnect_required = false`

### Map Lifecycle Tools

例如：
- `load_map`
- `create_blank_map`
- `create_map_from_template`
- `save_map_as`

当前建议默认：
- `risk_tier = session-disrupting`
- `session_disrupted = true`
- `reconnect_required = true`

`save_current_map` 则可以保持：
- `risk_tier = editor-stateful`

### P1 Lighting Tools

例如：
- `find_lighting_rig`
- `set_directional_light`
- `set_skylight`
- `set_exponential_height_fog`
- `apply_time_of_day_preset`

建议默认：
- `risk_tier = editor-stateful`
- `session_disrupted = false`
- `reconnect_required = false`

## How New Tools Should Use This Model

后续新工具建议按这个顺序设计：

1. 先确定 `domain`
2. 再确定 `risk_tier`
3. 再确定它属于 `P0 / P0.5 / P1 / experimental`
4. 再标记当前 `validation_status`
5. 最后决定默认 `recommended_client_action`

不要只写“功能能用”，不写它在自动化系统中的语义位置。

## Why This Matters

如果没有这套分档模型，agent 只能知道：
- 有这个工具

但不知道：
- 能不能批量跑
- 会不会打断会话
- 是不是只适合实验环境
- 出错后应该继续、重连、还是人工 review

所以从长期看，这套模型和 tool contract 一样重要。

## Current Direction

当前 fork 已经开始补这一层：

- `risk_tier`
- `session_disrupted`
- `reconnect_required`
- `recommended_client_action`

下一步应该继续：

- 在 README / roadmap 里明确这套模型
- 让 P1 新工具默认带上这些语义
- 后续再考虑 capability discovery / metadata export
