# P0 Validation Notes

This note tracks the intended validation path for the first two P0 batches.

It is intentionally lightweight and practical.

## Scope

Current implemented batches:

- health and reconnect baseline
  - `ping`
  - `get_editor_state`
  - `get_current_level`

- map lifecycle
  - `load_map`
  - `save_current_map`
  - `save_map_as`
  - `create_blank_map`
  - `create_map_from_template`

- minimal scene/testbed construction
  - `spawn_static_mesh_actor`
  - `find_actors_by_prefix`
  - `delete_actors_by_prefix`
  - `reset_testbed`
  - `ensure_capture_camera`

- evidence capture
  - `set_editor_camera`
  - `capture_viewport`
  - `capture_before_after`

## Validation Rule

Each tool should be validated in three layers where possible:

1. registration
2. execution
3. practical chained use

## Health Validation

### `ping`

- returns a stable lightweight health response
- can be used by external agents before attempting structured tool calls

### `get_editor_state`

- returns a minimal reconnect-oriented editor snapshot
- includes current level, actor count, and MCP port
- includes `session_ready`
- can be used by clients to decide whether they should continue or reconnect

### `get_current_level`

- returns the active editor level path/name
- remains valid as a post-reconnect sanity check
- includes `session_ready`

## Agent-Facing Contract Baseline

The current P0 wrappers now expose a minimum agent-facing contract:

- `ok`
- `data`
- `warnings`
- `error_code`
- `message`
- `risk_tier`
- `session_disrupted`
- `reconnect_required`
- `recommended_client_action`

Compatibility note:

- legacy fields such as `success`, action-specific payload fields, and `error`
  are still preserved for now
- the new fields should be treated as the preferred contract for reconnect-aware
  agents

## Batch 1 Validation

### `load_map`

- shows up in the `level` domain
- accepts a valid Unreal asset path
- actually switches the active editor level

### `save_current_map`

- shows up in the `level` domain
- succeeds on an already loaded writable map
- returns the active map path

### `save_map_as`

- shows up in the `level` domain
- duplicates the current map to a new asset path
- fails cleanly if the target path already exists

### `create_blank_map`

- creates a new map asset under a valid path
- loads the new map
- fails cleanly if the target already exists

### `create_map_from_template`

- creates a new map from a valid template asset
- loads the new map
- fails cleanly if the template path is invalid

## Batch 2 Validation

### `spawn_static_mesh_actor`

- creates a named actor
- applies mesh, transform, and scale
- fails cleanly for missing mesh or duplicate actor name

### `find_actors_by_prefix`

- returns actors matching the prefix
- includes count and actor details

### `delete_actors_by_prefix`

- deletes all matching actors
- returns deleted count and actor list

### `reset_testbed`

- behaves the same as prefix delete in the current version
- can be used as the reset step in a minimal testbed flow

### `ensure_capture_camera`

- creates a reusable camera actor if missing
- updates transform if the camera already exists
- fails cleanly if the name is already used by a non-camera actor

## Practical Chained Use

The first meaningful P0 chain to validate is:

1. `create_blank_map`
2. `spawn_static_mesh_actor`
3. `ensure_capture_camera`
4. `save_current_map`

The second chain is:

1. `load_map`
2. `find_actors_by_prefix`
3. `reset_testbed`
4. `save_map_as`

## Runtime Validation Snapshot

The first two P0 batches have now been runtime-validated inside Unreal 5.7 on this machine.

Confirmed working:

- `ping`
- `get_editor_state`
- `get_current_level`
- `create_blank_map`
- `spawn_static_mesh_actor`
- `ensure_capture_camera`
- `find_actors_by_prefix`
- `save_current_map`
- `load_map` for the original validation map
- `reset_testbed`
- `save_map_as`
- `set_editor_camera`
- `capture_viewport`
- `capture_before_after`

Confirmed contract behavior:

- health tools now return `recommended_client_action`
- health tools now expose `session_ready`
- scene/testbed and evidence-capture tools now return `risk_tier=editor-stateful`
- session-disrupting map tools return `recommended_client_action=reconnect`
- the first inferred core error codes are now validated:
  - `map_unsaved`
  - `map_not_found`
  - `map_already_exists`
- missing required parameters currently fail at the FastMCP / Pydantic schema
  layer before the wrapper can normalize them into `invalid_arguments`

Observed failure boundary:

- the original implementation could crash the editor after `save_map_as -> load_map(copy)`
- the current fork no longer treats seamless map transition as safe inside a live MCP session
- `create_blank_map` now completes without crashing the editor, but terminates the MCP session and requires reconnect
- `load_map` now completes without crashing the editor, but terminates the MCP session and requires reconnect
- `save_map_as` now fails cleanly on unnamed temporary maps such as `/Temp/Untitled_1` instead of crashing the editor
- seamless map-transition support is still blocked on a broader bridge-lifecycle redesign

Observed environment notes:

- the plugin quickstart settings on the validation project are correct:
  - `bEnable=True`
  - `bAutoStart=True`
  - `Port=8422`
- Live Coding must be disabled before rebuilding the plugin externally
- source-control checkout prompts can appear during map saves and should be disabled or accounted for during automated validation

Current status should be treated as:

- implemented
- runtime-validated for the health baseline
- runtime-validated for the first practical chain
- runtime-validated for most of the second chain
- runtime-validated for the P0 evidence-capture batch
- runtime-validated for session-disrupting `create_blank_map`
- runtime-validated for session-disrupting `load_map`
- still blocked on seamless map-transition support
- partially normalized for agent-facing contracts at the Python wrapper layer
- ready to use evidence capture as the preferred low-risk baseline path
