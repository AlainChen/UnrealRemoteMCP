# Compatibility Refactor Roadmap

This note describes a compatibility-first refactor path for a Remote MCP plugin.

It is written for this fork, but the structure is intentionally general:

- preserve what already works
- add a stronger editor-side foundation
- reduce brittle scripting paths
- improve team-scale deployment over time

## Current Usage Pattern

The plugin currently works as a hybrid:

- Unreal C++ plugin layer
- Unreal Python bridge and tool registration
- MCP server bootstrapped from inside the editor

This is already good enough for experimentation and some automation, but repeated usage exposes several pain points:

- high-level workflows still fall back to long Python scripts
- editor state is easy to push into unstable territory
- map and scene lifecycle are under-tooled
- lighting, capture, and post-process automation are fragile
- tool return shapes are not consistent enough for robust orchestration

## Refactor Goal

The goal is not to rewrite everything at once.

The goal is to move from:

- flexible but fragile scripting

to:

- structured, stable, contract-based editor capabilities

while preserving enough compatibility that current workflows can keep operating.

## AI Interaction Modes

The plugin should support more than one style of AI usage.

For planning purposes, use these five modes:

1. advisory / read-only
2. sandbox prototyping
3. restricted co-building
4. workflow-node automation
5. high-privilege maintenance

These modes matter because they imply different expectations:

- read-only analysis prioritizes safe state inspection
- sandbox prototyping prioritizes isolated creation and evidence capture
- restricted co-building prioritizes bounded write operations and predictable contracts
- workflow-node automation prioritizes health, reconnect, and artifact collection
- high-privilege maintenance prioritizes explicit risk handling and policy controls

The current refactor should optimize first for:

- advisory / read-only
- workflow-node automation
- sandbox prototyping

The later phases should progressively improve support for:

- restricted co-building
- eventually high-privilege maintenance

## What Must Stay Compatible

The following should keep working during the transition:

- plugin startup inside Unreal Editor
- editor subsystem lifecycle
- current MCP endpoint behavior where possible
- Python-registered tools that existing users depend on
- domain dispatch concepts

The transition should be additive first, not destructive first.

## What Should Change First

### 1. Standardize tool contracts

New or upgraded tools should move toward a consistent result shape:

```json
{
  "ok": true,
  "data": {},
  "warnings": [],
  "error_code": null,
  "message": "..."
}
```

This should happen before large orchestration changes.

### 2. Add structured P0 tools

The most important first tools are:

- map lifecycle
- minimal scene construction
- capture

These have the highest leverage because they unblock stable automation without requiring broad redesign.

### 3. Reduce Python from primary logic to bridge logic

Python should gradually stop carrying:

- long workflow scripts
- multi-step orchestration
- heavy object/property guessing

Instead, Python should increasingly do:

- tool registration
- parameter translation
- bridge calls
- compatibility shims

### 4. Prepare for a thinner plugin-side orchestration model

Long term, heavier orchestration should not live in the plugin.

That includes:

- session management
- retries
- task pipelines
- artifact assembly
- benchmark and gym sequencing

Those can later live in an outer control layer while this plugin focuses on editor-native capabilities.

## P0 Roadmap

P0 should cover only the minimum set required to make the current structured tools:

- predictable for agents
- safe enough for repeated use
- recoverable after session-disrupting operations

In practice, that means P0 is not just "add new tools". It also includes the first layer of contract and health semantics around those tools.

### P0-A: Map lifecycle

- `create_blank_map`
- `create_map_from_template`
- `load_map`
- `save_current_map`
- `save_map_as`

These tools should be treated as two separate compatibility tiers:

- `save_current_map`
  - safe to call inside an existing MCP session
- `save_map_as`
  - should fail cleanly when the current map has no persistent filename
  - should avoid forcing an immediate reload of the saved copy inside the same live session
- `load_map`, `create_blank_map`, `create_map_from_template`
  - session-disrupting
  - callers should expect the current MCP session to terminate and reconnect before continuing

### P0-B: Scene and testbed construction

- `spawn_static_mesh_actor`
- `find_actors_by_prefix`
- `delete_actors_by_prefix`
- `reset_testbed`
- `ensure_capture_camera`

### P0-C: Evidence capture

- `set_editor_camera`
- `capture_viewport`
- `capture_before_after`

The evidence-capture batch is intentionally lower risk than map lifecycle and should become the preferred baseline path for agent-driven showcase automation before revisiting seamless map transitions.

These three areas form the minimum automation foundation.

### P0-D: Agent-friendly contract baseline

The current structured tools are useful, but not yet consistent enough for robust multi-agent orchestration.

The minimum P0 contract layer should include:

- a common result envelope for newly added or upgraded tools
- explicit `risk_tier`
- explicit `session_disrupted`
- explicit `reconnect_required`
- a minimal health baseline for reconnect-aware clients

This is especially important for clients such as Codex, Cursor, Claude Code, or any outer orchestration service that cannot rely on implicit editor state.

### P0-E: Minimal health and reconnect semantics

P0 should also include the smallest health surface needed after session-disrupting operations:

- `ping`
- `get_editor_state`
- `get_current_level`

These tools do not need to be perfect at first. They need to be reliable enough to answer:

- is the server alive?
- is the editor available?
- which map is currently active?
- should the client reconnect or continue?

## P0.5 Roadmap

P0.5 is for improvements that are still close to the foundation layer, but are not strict blockers for today's validated workflows.

### P0.5-A: Core error codes

Introduce stable, small-scope error codes for the current P0 tools. Start with the highest-frequency paths only.

Recommended first error-code set:

- `map_not_found`
- `map_already_exists`
- `map_unsaved`
- `editor_state_error`
- `invalid_arguments`
- `session_disrupted`

Current status:

- the fork now infers the first core error codes at the Python contract layer
- validated examples include:
  - `map_unsaved`
  - `map_not_found`
  - `map_already_exists`
- schema-level missing-argument failures can still be raised by FastMCP /
  Pydantic before control reaches the wrapper layer

Decision:

- keep schema-level argument validation in FastMCP / Pydantic
- do not weaken tool signatures just to normalize every missing-argument case
- continue using plugin-side normalization for business and lifecycle errors
- if full cross-client error translation is needed later, prefer doing it in an
  outer orchestration layer rather than inside every plugin tool

### P0.5-B: Contract normalization for existing P0 tools

The current P0 tools still mix:

- `success`
- `error`
- action-specific payloads

P0.5 should normalize them toward a single shape such as:

```json
{
  "ok": true,
  "data": {},
  "warnings": [],
  "error_code": null,
  "message": "...",
  "risk_tier": "safe | editor-stateful | session-disrupting",
  "session_disrupted": false,
  "reconnect_required": false
}
```

This does not have to replace every legacy return path immediately, but new and upgraded P0 tools should converge on it.

Argument-validation note:

- schema-level parameter failures are currently allowed to remain outside this
  wrapper contract
- P0.5 should prioritize business-error normalization first
- missing-argument translation can be revisited later only if cross-client
  ergonomics clearly justify an outer translation layer

## P1 Roadmap

- lighting rig and readability presets
- post process wrappers
- structured error taxonomy beyond the core P0 set
- step logging
- checkpoints or rollback-friendly save points
- richer editor health and reconnect diagnostics

## P2 Roadmap

- mood and weather presets
- native gym/testbed creation helpers
- advanced capture sequences
- permission and policy layers

## Why This Path Helps Team Deployment

If the plugin needs to run across many teammates' machines, the biggest wins are not flashy features.

The biggest wins are:

- fewer hidden dependencies
- fewer fragile script chains
- more explicit health states
- more reproducible outputs
- clearer compatibility behavior between versions

That is why this roadmap prioritizes foundational editor-side tools over advanced effects or more dynamic scripting.

## Interaction With Outer Control Layers

This plugin should increasingly act as:

- the editor-native capability provider

and less as:

- the full workflow engine

That separation improves:

- maintainability
- deployment
- compatibility
- long-running automation stability

## Practical Migration Principle

Do not remove old paths before the new ones have proven stable.

Instead:

1. add stable structured tools
2. route new workflows to them
3. mark old script-heavy paths as legacy
4. gradually reduce their role

## Summary

The right refactor path for this fork is:

- preserve startup and basic compatibility
- build stable editor-side foundations
- thin the Python layer
- standardize tool contracts
- prepare for cleaner outer orchestration later

This produces a more modern and scalable system without forcing a risky full rewrite.

## Current Practical Backlog

Based on the current implementation and runtime validation, the next concrete work items should be:

### Still inside P0

- make the current P0 tools return more uniform agent-facing fields
- add minimal health tools
- document and enforce `session-disrupting` semantics consistently
- validate the current scene and capture tools through repeated chained use

### Immediately after P0

- add core error codes for map / scene / capture paths
- normalize tool result shapes across the current P0 set
- prepare the first lighting/readability preset layer on top of the validated P0 foundation

### Later

- revisit seamless map-transition support only after health, reconnect, and contract behavior are stable
