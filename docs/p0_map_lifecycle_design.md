# P0 Map Lifecycle Design

This document turns the broad refactor roadmap into the first concrete implementation target:

- map lifecycle

It focuses on a compatibility-friendly path that fits the current codebase.

## Why Start Here

Map lifecycle is the highest-leverage missing foundation because it sits under many unstable editor workflows:

- create a new test scene
- duplicate a baseline scene
- load a target map
- save editor changes safely
- branch a scene into a new asset path

Today these steps tend to fall back to long Python scripts or implicit editor state.

That makes automation brittle even when the rest of the MCP stack is healthy.

## Current Code Path

The current plugin already has a useful pattern:

- C++ editor-native tool implementation
  - `Source/RemoteMCP/Public/MCPTools/MCPEditorTools.h`
  - `Source/RemoteMCP/Private/MCPTools/MCPEditorTools.cpp`

- Python domain-tool wrapper layer
  - `Content/Python/tools/edit_tools.py`

- MCP registration
  - `Content/Python/tools/tool_register.py`

That means P0 map lifecycle does not need a new architecture.

It should follow the existing structure:

1. add editor-native handlers in `UMCPEditorTools`
2. expose thin Python wrappers in `edit_tools.py`
3. register them in the existing `level` domain

## P0 Scope

The first batch should stay small:

- `load_map`
- `save_current_map`
- `save_map_as`
- `create_blank_map`
- `create_map_from_template`

If implementation risk rises, the safest incremental order is:

1. `load_map`
2. `save_current_map`
3. `save_map_as`
4. `create_blank_map`
5. `create_map_from_template`

## File-Level Plan

### 1. C++ Public Header

File:

- `Source/RemoteMCP/Public/MCPTools/MCPEditorTools.h`

Add new static handlers:

- `HandleLoadMap`
- `HandleSaveCurrentMap`
- `HandleSaveMapAs`
- `HandleCreateBlankMap`
- `HandleCreateMapFromTemplate`

These should remain `BlueprintCallable` like the existing editor tools.

### 2. C++ Implementation

File:

- `Source/RemoteMCP/Private/MCPTools/MCPEditorTools.cpp`

Implement the five handlers using editor-safe APIs.

Recommended direction:

- loading maps:
  - use editor map load helpers instead of trying to emulate via Python
- saving current map:
  - save the active editor world / package
- save as:
  - duplicate or save the current map package to a target asset path
- blank map:
  - create a minimal world package safely
- from template:
  - duplicate an existing map asset to a new path, then optionally load it

### 3. Shared Result / Error Helpers

File:

- `Source/RemoteMCP/Private/MCPTools/UnrealMCPCommonUtils.cpp`

Potential additions:

- a helper for standardized success payloads
- a helper for map-path validation
- a helper for asset-path normalization
- a helper for structured map errors

This is not required to ship the first tool, but should be used as the response contract improves.

### 4. Python Wrappers

File:

- `Content/Python/tools/edit_tools.py`

Add thin wrappers under the `level` domain:

- `load_map`
- `save_current_map`
- `save_map_as`
- `create_blank_map`
- `create_map_from_template`

These should mirror the current style used by:

- `spawn_actor`
- `delete_actor`
- `focus_viewport`

That means:

- prepare a small params dict
- use `call_cpp_tools(...)`
- return structured results as Python dictionaries

### 5. Registration

File:

- `Content/Python/tools/tool_register.py`

No structural changes should be needed if the functions are added inside `register_edit_tool(...)`.

## Contract Shape

The current codebase uses mixed result conventions.

For compatibility, the first implementation should not try to rewrite every old tool.

Instead, new map lifecycle tools should move toward a more explicit shape.

Minimum recommended fields:

```json
{
  "success": true,
  "map_path": "/Game/Test/MyMap",
  "loaded": true
}
```

When possible, start nudging toward:

```json
{
  "ok": true,
  "data": {
    "map_path": "/Game/Test/MyMap",
    "loaded": true
  },
  "warnings": [],
  "error_code": null,
  "message": "Loaded map successfully."
}
```

But compatibility matters more than perfection in the first batch.

The main rule is:

- do not return only a bare success string

## Error Taxonomy

At minimum, these cases should be distinguished:

- `missing_parameter`
- `invalid_asset_path`
- `map_not_found`
- `editor_world_unavailable`
- `save_failed`
- `duplicate_failed`
- `template_not_found`

Even if the public payload is still simple, these distinctions should exist in implementation and logging.

## Asset Path Rules

To reduce future breakage, the tools should enforce a simple rule:

- all map paths must be Unreal asset paths, for example:
  - `/Game/__Gym/Maps/TestMap`

Do not accept:

- raw filesystem paths
- partial package names without clear normalization

If needed, add a small normalization helper:

- ensure leading `/Game/`
- reject invalid characters
- optionally strip `.umap` if provided

## Compatibility Rules

The implementation should preserve existing usage where possible:

- do not remove `run_python_script`
- do not rewrite old tools in the same change
- add new lifecycle tools alongside the old paths
- let higher-level workflows adopt them gradually

This keeps the migration incremental.

## Testing Strategy

First stage tests should not depend on complex showcase scenes.

They should verify:

- a map can be loaded by asset path
- the current map can be saved
- the current map can be saved as a new asset
- a blank map can be created
- a template map can be duplicated

The ideal testing layers are:

### Unit-ish validation

- path normalization
- parameter validation
- error classification helpers

### Editor integration checks

- create -> load -> save -> save as
- duplicate template -> load duplicated map

If full editor automation tests are expensive, document the manual validation recipe for the first pass.

## Risk Notes

### Lowest-risk tools

- `load_map`
- `save_current_map`

### Medium-risk

- `save_map_as`

### Highest-risk

- `create_blank_map`
- `create_map_from_template`

Why:

- map creation and duplication often touch package handling, asset registry behavior, and editor save semantics

So if implementation time is limited, shipping the first three tools first is still a very good P0 milestone.

## Success Criteria

P0 map lifecycle should be considered successful when:

- a client can load a map without using a raw Python script
- a client can save the active map safely
- a client can branch the current map into a new asset path
- the Python layer only acts as a thin wrapper
- no new workflow requires long lifecycle-oriented Python scripts by default

## Recommended Next Step After This

Once map lifecycle is in place, the next best follow-up is:

- minimal scene/testbed construction

because the combination of:

- map lifecycle
- scene construction

is enough to unlock much more stable baseline automation.
