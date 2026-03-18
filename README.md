# UnrealRemoteMCP

An Unreal Editor plugin that exposes Unreal Engine editor capabilities through MCP.

This fork keeps the original project usable, but reframes it as the editor-side foundation for more stable automation, benchmark, and capability-gym workflows.

## Project Relationship

There are two related projects in this workflow:

- `UnrealRemoteMCP`
  - Runs inside Unreal Editor
  - Owns editor-native capabilities
  - Starts the MCP server
  - Executes editor tools on the game thread

- `UnrealMCPHub`
  - Runs outside Unreal Editor
  - Owns orchestration, preflight, benchmark-lite, artifacts, and workflow-level policy
  - Can compile, launch, discover, monitor, and proxy Unreal instances

In short:

- `RemoteMCP` is the editor-side capability layer
- `Hub` is the outer control plane

## Fork Intent

This fork is not trying to replace the upstream project overnight.

The current goal is to evolve the plugin in a compatibility-friendly way so it becomes:

- more stable for long-running automation
- easier to extend with structured tools
- better suited for team-wide deployment
- less dependent on long `run_python_script` chains

The long-term direction is:

- keep Unreal-native capabilities close to the editor
- reduce Python responsibility over time
- expose more structured, contract-based editor tools
- support a more modern outer orchestration layer later if needed

## Compatibility Strategy

This fork is intended to stay compatible with the original workflow where practical.

That means:

- existing Python and MCP startup flow should keep working
- existing clients that talk to the plugin should not break unnecessarily
- new functionality should be added incrementally behind stable tool contracts
- high-risk script-heavy paths should be replaced gradually rather than deleted all at once

The preferred evolution path is:

1. keep the current plugin boot flow
2. add structured editor-side tools for high-frequency operations
3. standardize result and error contracts
4. reduce reliance on long editor Python scripts
5. move heavier orchestration concerns out of the plugin later

## Why This Fork Exists

The original project already provides a strong base:

- editor subsystem lifecycle
- MCP server startup inside Unreal Editor
- game-thread tool execution
- domain-based tool dispatch
- a mix of C++ editor tools and Python-registered tools

The current pain points are mostly about stability and structure:

- too much high-level work still falls back to `run_python_script`
- map lifecycle tooling is weak
- scene/testbed construction is not yet structured enough
- lighting, capture, and post-process flows are still brittle
- tool result contracts are not yet consistent enough for robust automation

One important current limitation is that map-changing operations are not yet seamless inside a single MCP session. Until the bridge lifecycle is redesigned, callers should treat those tools as session-disrupting and reconnect before issuing follow-up requests.

## P0 Foundation Priorities

The first priority is not "more tools everywhere".

It is to add a small, stable foundation layer for automation.

### P0

- map lifecycle
  - `create_blank_map`
  - `create_map_from_template`
  - `load_map`
  - `save_current_map`
  - `save_map_as`

  Compatibility note:
  - `save_current_map` is expected to remain safe inside an active session
  - the other map lifecycle tools should currently be treated as session-disrupting

- scene and testbed construction
  - `spawn_static_mesh_actor`
  - `find_actors_by_prefix`
  - `delete_actors_by_prefix`
  - `reset_testbed`
  - `ensure_capture_camera`

- evidence capture
  - `set_editor_camera`
  - `capture_viewport`
  - `capture_before_after`

### P1

- lighting rig and presets
- post process wrappers
- structured error taxonomy
- task step logging and checkpoints

## Deployment Vision

The long-term goal is to make this plugin easy to deploy and run across many teammates' machines.

That requires:

- fewer local one-off scripts
- more structured editor-side capabilities
- clearer version and compatibility boundaries
- easier setup and health checks
- less hidden editor state dependence

This fork is still in the foundation phase, but the intended direction is team-scale operability rather than one-off experimentation.

## Repository Structure

- `Source/RemoteMCP`
  - Unreal plugin C++ code
- `Content/Python`
  - Python bridge, MCP server bootstrap, and Python-side tools
- `docs/`
  - reference notes and refactor planning

## Current Architecture

Today the plugin is a hybrid system:

- C++ plugin layer
  - module startup
  - editor subsystem
  - bridge objects
  - some editor-native tools

- Python MCP layer
  - FastMCP server
  - tool registration
  - domain dispatch
  - game-thread scheduling helpers

This fork currently preserves that architecture, but aims to shift the balance toward:

- more C++ structured tool foundations
- thinner Python bridge logic
- more stable contracts for external orchestration

## Recommended Reading

For the generic refactor direction and capability-gap framing, see the companion notes in the related `UnrealMCPHub` fork:

- `docs/unreal-ai-playbook/remote-mcp-generic-roadmap.md`
- `docs/unreal-ai-playbook/gym/hub-vs-remotemcp-boundary.zh-CN.md`
- `docs/unreal-ai-playbook/gym/gym-tooling-backlog.zh-CN.md`

For this fork's first concrete implementation target, see:

- `docs/compatibility_refactor_roadmap.md`
- `docs/p0_map_lifecycle_design.md`

## Upstream

This repository is a fork of the original `blackplume233/UnrealRemoteMCP`.

The intent is:

- keep upstream improvements easy to merge where possible
- keep fork-specific foundation work isolated and understandable
- contribute back small, generally useful changes when they are mature enough

## License

Please refer to the upstream repository and local license files for licensing details.
