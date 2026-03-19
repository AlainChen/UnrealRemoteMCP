# Agent Workflow Design Principles

## Purpose

这份文档解释的是：

- 为什么 `RemoteMCP` 的工具 contract 和分档模型
- 不只是插件内部实现细节
- 还会直接影响上层 agent skills 和 workflow 的设计方式

一句话：

`RemoteMCP` 的工具设计越清晰，  
上层的 Codex / Cursor / Claude / custom workflow 就越容易稳定复用。

## Core Principle

上层 agent 不应该靠猜测工具语义来工作。

它应该能够从工具本身得到足够多的信息，判断：

- 这是不是一个安全查询
- 这是不是一个正常的 editor mutation
- 这会不会打断当前会话
- 这是不是仍在实验阶段
- 失败后该继续、重连还是人工 review

## Why Tool Contracts Matter To Skills

一个 skill 真正依赖的不只是“有哪些工具”，还依赖：

- 风险语义
- 结果结构
- 可恢复性
- 成熟度

例如：

- 如果工具带 `session_disrupted=true`
  - skill 就应该停止连续调用
  - 先走 reconnect

- 如果工具带 `risk_tier=editor-stateful`
  - skill 就知道可以链式继续，但要保守验证

- 如果工具属于 `validated`
  - 它可以进入默认 baseline workflow

- 如果工具属于 `experimental`
  - skill 就不该默认选它作为主路径

## Five AI Usage Modes

建议所有上层 workflow 都默认考虑这 5 种模式：

1. `advisory / read-only`
2. `sandbox prototyping`
3. `restricted co-building`
4. `workflow-node automation`
5. `high-privilege maintenance`

这 5 种模式之所以重要，是因为同一个工具在不同模式下的可接受性不同。

例如：

- `load_map`
  - 在 `workflow-node automation` 中可以接受
  - 但必须明确是 `session-disrupting`

- `spawn_static_mesh_actor`
  - 适合 `sandbox prototyping`
  - 不应默认越权到 production map

- `post process wrapper`
  - 适合 gym/showcase baseline
  - 不应在未验证状态下直接进入高信任维护

## Recommended Skill Design Pattern

上层 skill 设计建议固定成三段：

1. `check`
   - health
   - editor state
   - tool availability
   - risk preflight

2. `act`
   - 只调用和当前 mode 匹配的工具
   - 优先用 validated structured tools
   - 避免默认退回长脚本

3. `verify`
   - 结果检查
   - evidence capture
   - risk / readiness note
   - reconnect if needed

## What A Good Tool Gives To A Workflow

一个好的结构化工具，不只是“完成操作”，还应该告诉 workflow：

- `risk_tier`
- `session_disrupted`
- `reconnect_required`
- `recommended_client_action`
- `error_code`

这样 workflow 才能做出统一决策，而不是每条流程都自己写一套特判。

## Why This Helps Team Expansion

当你以后要：

- 扩展自定义 skills
- 扩展 agent workflow
- 增加更多 Gym / benchmark
- 支持不同 AI client

你最不想做的事就是：
- 每个新 skill 再重新理解一遍工具边界
- 每个 agent 再重新踩一遍 editor lifecycle 的坑

因此，工具层的语义清晰度，本质上是在给后续所有 workflow 降低复杂度。

## Practical Rule

后续新增工具，至少要同时满足这两件事：

1. 有 editor-side implementation
2. 有清楚的 workflow-facing semantics

如果只有第 1 条，那它只是“能调用的功能”。
如果两条都有，它才是“能被 agent 稳定消费的能力”。
