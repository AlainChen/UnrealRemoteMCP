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

## Current Limitation

This repository work has not yet been runtime-validated inside Unreal on this machine.

So current status should be treated as:

- implemented
- statically reviewed
- awaiting editor-side validation
