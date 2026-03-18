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

### P0-A: Map lifecycle

- `create_blank_map`
- `create_map_from_template`
- `load_map`
- `save_current_map`
- `save_map_as`

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

These three areas form the minimum automation foundation.

## P1 Roadmap

- lighting rig and readability presets
- post process wrappers
- structured error taxonomy
- step logging
- checkpoints or rollback-friendly save points

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
