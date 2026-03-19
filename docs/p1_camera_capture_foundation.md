# P1 Camera And Capture Foundation Notes

## Purpose

This note records the constructive editor-side fixes that were validated while
driving `Gym-01` through `Gym-03`.

The goal is not to document every failed experiment. The goal is to preserve
the fixes that clearly improved:

- capture correctness
- camera reuse safety
- viewport evidence reliability
- baseline Gym automation quality

## Main Fixes

### 1. Camera-anchored viewport capture

`capture_viewport` now accepts an optional `camera_name`.

When a camera name is supplied, the level viewport is aligned to that camera
before the screenshot is taken.

Why this matters:
- Gym evidence should not depend on whichever viewport happened to be active
- a known camera makes before/after capture deterministic enough for reporting

### 2. Safer capture-camera reuse

`ensure_capture_camera` no longer relies on forcing the same actor object name
at spawn time.

Instead:
- the tool looks up existing actors by name or label
- it updates the camera transform if the camera already exists
- it labels newly created cameras consistently for later lookup

Why this matters:
- repeated automation runs should not crash the editor because a camera name
  collides at spawn time
- idempotent tool behavior is critical for Gym reruns

### 3. Viewport refresh after scene mutation

The editor now forces a viewport refresh after key mutation paths such as:

- actor transform updates
- static-mesh actor spawning
- capture-camera setup
- directional light changes
- skylight changes
- fog changes
- screenshot preparation

Why this matters:
- without a refresh, screenshots can capture stale frames even when the scene
  mutation already succeeded logically

### 4. Lighting rig support was made more baseline-friendly

The initial lighting foundation was extended so the minimal rig can include:

- directional light
- skylight
- exponential height fog
- sky atmosphere

Additional details:
- the directional light is marked as an atmosphere sunlight
- skylight recapture is triggered after skylight changes

Why this matters:
- baseline lighting/readability passes need a more consistent visual rig than
  ad hoc default map lighting

### 5. Actor lookup semantics were improved

Some editor-side helpers now treat actor label matching as a valid lookup path,
not only raw object names.

Why this matters:
- human-readable camera and Gym object names are easier to reuse across runs
- editor workflows often reason about labels rather than internal object names

### 6. Static mesh spawning is safer under repeated runs

`spawn_static_mesh_actor` no longer forces the Unreal object name at spawn time.

Why this matters:
- repeated Gym runs can otherwise hit fatal same-name collisions inside the
  current level package
- using label-oriented lookup semantics is safer for baseline automation than
  forcing object-name uniqueness at spawn time

## Practical Validation Impact

These fixes directly supported:

- `Gym-01` baseline infrastructure validation
- `Gym-02` valid before/after space-readability evidence
- `Gym-03` valid before/after gameplay-feedback evidence

In practice, they improved three things:

1. evidence can now be trusted more often
2. reruns are less fragile
3. Gym baselines no longer depend as heavily on ad hoc viewport luck

## Remaining Boundary

These fixes improve capture and baseline Gym stability, but they do not remove
all lifecycle complexity.

Map-changing operations should still be treated as:

- `session-disrupting`
- reconnect-oriented

This note therefore describes a validated capture/tooling improvement layer,
not a complete lifecycle redesign.
