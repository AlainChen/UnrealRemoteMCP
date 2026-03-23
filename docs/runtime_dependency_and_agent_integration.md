# Runtime Dependency And Agent Integration

This note describes how this fork should be used in real projects so that:

- upstream syncs remain usable
- project-side plugin installs do not fail at runtime
- external agents have a predictable contract for interacting with the plugin

It is intentionally practical rather than architectural.

## The Three Python Layers

This fork currently touches three separate Python environments:

1. local development Python
   - used for repo development, tests, helper scripts, and future outer runners
   - can be managed by tools such as `uv`

2. UnrealMCPHub embedded or external Python
   - used by the outer control plane and validation tools
   - separate from Unreal Editor runtime Python

3. Unreal Editor runtime Python
   - provided by Unreal's `PythonScriptPlugin`
   - runs `Content/Python/init_mcp.py`
   - is the actual runtime used by `RemoteMCP`

The most important rule is:

- `uv` or a healthy local Python setup does **not** automatically make the plugin runtime healthy inside Unreal Editor

## Current Reality

At the moment, the most fragile part of project deployment is not C++ compilation.

It is the Unreal-side Python runtime dependency layer.

Typical failure mode:

- the plugin code syncs correctly
- the editor compiles and launches
- `init_mcp.py` runs
- Unreal Python fails with `ModuleNotFoundError`, such as missing `mcp`

This means:

- code sync is not the same as runtime readiness
- a project needs both plugin code and plugin runtime dependencies

## Recommended Deployment Model

For now, prefer a project-level plugin install with vendored runtime dependencies.

Recommended structure:

- `Plugins/RemoteMCP`
  - plugin source and resources
- `Plugins/RemoteMCP/Content/Python/Lib/site-packages`
  - vendored runtime dependencies required by Unreal Python

This model is preferred because it:

- keeps the runtime self-contained per project
- avoids depending on machine `PATH`
- makes benchmark and validation results more reproducible
- is easier to reason about than "hope the machine already has the right packages"

## Recommended Runtime Dependency Flow

When syncing this fork into a project, treat setup as two separate steps.

### Step 1: Sync plugin code

Update the plugin code from the fork:

- C++ source
- Python bridge and tools
- plugin metadata and docs

### Step 2: Sync plugin runtime dependencies

Ensure Unreal runtime dependencies exist inside:

- `Plugins/RemoteMCP/Content/Python/Lib/site-packages`

The current practical expectation is:

- if `init_mcp.py` imports `mcp`, `httpx`, or related packages, they must be importable from Unreal Python

Today, the safest approach is:

- vendor the required runtime packages into the plugin
- treat those packages as part of project deployment

## What To Avoid

Do not assume the following are enough on their own:

- local `uv` environment is healthy
- local system `python` is healthy
- Hub works
- plugin compiles

None of those guarantee that Unreal runtime Python can import what the plugin needs.

## Agent-Safe Usage Rules

External agents should treat this plugin as a structured editor capability layer, not as an unrestricted script executor.

Recommended rules for agents:

1. prefer structured tools over `run_python_script`
2. call health tools first
   - `ping`
   - `get_editor_state`
   - `get_current_level`
3. treat map-changing tools as session-disrupting
   - reconnect after those operations
4. use `risk_tier`, `session_disrupted`, and `reconnect_required`
   - do not rediscover tool behavior ad hoc
5. treat runtime import failures as deployment problems, not as ordinary tool failures

## Minimum Project Integration Checklist

Before a project is considered ready for agent use, confirm:

- the plugin compiles in the target project
- Unreal Editor starts with the plugin enabled
- `init_mcp.py` runs without `ModuleNotFoundError`
- `ping` succeeds
- `get_editor_state` succeeds
- one structured `level` tool succeeds
- one capture or evidence tool succeeds

If any of these fail, the project is not truly ready for agent workflows yet.

## Short-Term TODO

The fork should still formalize the runtime side further:

- define the authoritative runtime dependency set for Unreal Python
- define how those dependencies are refreshed after upstream sync
- separate clearly:
  - development dependencies
  - Unreal runtime dependencies
- provide a repeatable project-side runtime sync/install step

## Long-Term Direction

Long term, this should evolve toward:

- a clearer runtime boundary between:
  - local development Python
  - Hub-side execution Python
  - Unreal runtime Python
- fewer hidden runtime assumptions
- easier team-wide deployment
- a more durable outer runner/client that relies on tool contracts rather than local shell glue
