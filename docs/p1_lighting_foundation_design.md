# P1 Lighting Foundation Design

## Purpose

`P1 lighting` 的目标不是做完整天气系统，也不是马上做高级 cinematic pipeline。

它的目标是：

- 在 `P0` 已完成的 map / scene / capture / health 基础上
- 增加第一批稳定、结构化、可复用的 lighting/readability 能力
- 让外部 agent 不再依赖长 Python 脚本去猜测光照和 atmosphere 属性

这批能力首先服务于：

- Gym-01 `Lighting Readability Modify`
- Gym-02 里的少量 supporting lighting
- 后续的 baseline showcase

## Why This Belongs In RemoteMCP

这些能力更适合放在插件侧，而不是 Hub 侧，原因有三点：

1. 它们是 editor-native capability  
   需要直接操作：
   - DirectionalLight
   - SkyLight
   - ExponentialHeightFog
   - SkyAtmosphere
   - Camera / viewport

2. 它们应该屏蔽底层对象和属性细节  
   如果放在 Hub 层，最终还是会退回：
   - 长 `ue_run_python`
   - 属性猜测
   - 反射式访问  
   这正是当前要减少的脆弱路径。

3. 它们需要和结果 contract / risk tier / error semantics 保持一致  
   这些更适合和现有 editor-side structured tools 一起生长。

一句话：

`lighting rig / preset` 是 editor-native foundation，不只是 orchestration concern。

## P1 Scope

第一批只做最有价值的基础能力，不追求大而全。

### P1-A: Core Lighting Setters

建议先补这几类工具：

- `set_directional_light`
- `set_skylight`
- `set_exponential_height_fog`

输入应该尽量保持高层、稳定、少字段，例如：
- intensity
- light_color
- temperature
- indirect_intensity
- volumetric_scattering_intensity
- fog_density
- fog_height_falloff

目标是避免 agent 直接碰杂乱、易变的 UE 属性名。

### P1-B: Lighting Rig Discovery / Ensure

建议补：

- `find_lighting_rig`
- `ensure_basic_lighting_rig`

第一版不一定要自动创建所有高级对象，但至少要能：
- 找到现有关键光源
- 在缺失时创建最小可用 rig

最小 rig：
- one `DirectionalLight`
- one `SkyLight`
- one `ExponentialHeightFog`

### P1-C: Preset Layer

在 core setters 之上，第一批 preset 建议只做少量可展示的 baseline：

- `neutral_day`
- `golden_hour`
- `cool_overcast`
- `night_focal`

这些 preset 的目标是：
- 让 Gym-01 不用手搓一堆参数
- 让 before/after 结果更一致
- 让后续 benchmark / showcase 更可复现

### P1-D: Readability Pass Wrapper

建议补一个轻量高层工具：

- `apply_lighting_readability_pass`

第一版只要求：
- 在不重建场景的前提下
- 针对 focal area 做一轮可见的 readability improve
- 返回“修改了哪些对象/参数”

这一步不需要一开始就极其智能，先保证结构化和可重复。

## Contract Expectations

继续沿用 `P0/P0.5` 的 agent-facing contract：

- `ok`
- `data`
- `warnings`
- `error_code`
- `message`
- `risk_tier`
- `session_disrupted`
- `reconnect_required`
- `recommended_client_action`

对于 lighting tools，默认建议：
- `risk_tier = editor-stateful`
- `session_disrupted = false`
- `reconnect_required = false`

除非某个操作真的触发 map/session 变化，否则不要乱抬风险级别。

## Error Expectations

第一版不追求完整 taxonomy，但建议优先支持：

- `not_found`
- `invalid_arguments`
- `editor_state_error`
- `unsupported_operation`

其中：
- schema-level 参数缺失，继续交给 FastMCP / Pydantic
- tool-level 业务错误，再走插件 wrapper 统一 envelope

## Validation Path

`P1 lighting` 的第一批验证应该尽量复用 `P0` 路径，而不是发明新链路。

推荐验证链：

1. `ping`
2. `get_editor_state`
3. `find_lighting_rig` or `ensure_basic_lighting_rig`
4. `apply_time_of_day_preset`
5. `set_editor_camera`
6. `capture_before_after`

这条链的意义是：
- 直接服务于 Gym
- 低风险
- 证据自然可留档

## Recommended Implementation Order

1. `find_lighting_rig`
2. `set_directional_light`
3. `set_skylight`
4. `set_exponential_height_fog`
5. `ensure_basic_lighting_rig`
6. `apply_time_of_day_preset`
7. `apply_lighting_readability_pass`

不要反过来。

先把原子能力补稳，再做 preset 和 readability wrapper。

## Out Of Scope For This First P1 Slice

当前不纳入第一轮：

- 完整天气系统
- 动态 day-night cycle
- 云层系统
- advanced post process lookdev
- 多机位 shot system
- cinematic sequencer integration

这些都更像后续 P1 扩展或 P2。

## Handoff To Hub

当 `P1 lighting` 第一批能力稳定后，Hub 侧应该同步：

- 更新 Gym-01 brief
- 更新 workflow 的 tool priority
- 更新 baseline evidence path
- 把 lighting preset 从“backlog”提升到“available capability”
