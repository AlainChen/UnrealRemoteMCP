# P0 Validation Notes

This note tracks the intended validation path for the first two P0 batches.

It is intentionally lightweight and practical.

## Scope

Current implemented batches:

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

Observed failure boundary:

- after `save_map_as`, calling `load_map` on the duplicated map crashed the editor when the implementation still used deprecated `UEditorLevelLibrary` map lifecycle APIs
- migrating to `ULevelEditorSubsystem` did not eliminate the crash
- the practical boundary is now treated as a session-lifecycle problem rather than a single bad API call
- map-changing operations should be treated as `session-disrupting` until the bridge lifecycle is redesigned

Observed environment notes:

- the plugin quickstart settings on the validation project are correct:
  - `bEnable=True`
  - `bAutoStart=True`
  - `Port=8422`
- Live Coding must be disabled before rebuilding the plugin externally
- source-control checkout prompts can appear during map saves and should be disabled or accounted for during automated validation

Current status should be treated as:

- implemented
- runtime-validated for the first practical chain
- runtime-validated for most of the second chain
- runtime-validated for the P0 evidence-capture batch
- blocked on seamless map-transition support
- ready to use evidence capture as the preferred low-risk baseline path
