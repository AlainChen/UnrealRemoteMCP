# Branch: feature/eca-integration

> ECABridge optional integration — RemoteMCP as the intelligent proxy layer for 238+ engine-native commands.

## What This Branch Does

When the engine contains Epic's experimental ECABridge plugin (~238 C++ commands covering
Actor/Blueprint/Mesh/Niagara/MVVM/Material/DataTable/View/etc.), RemoteMCP can now
**proxy all of them** through a single "eca" domain — with zero impact when ECABridge is absent.

```
AI Client → RemoteMCP (port 8422)
               ├── eca domain (NEW) → MCPECAProxy → FECACommandRegistry → 238 commands
               ├── level domain     → unchanged
               ├── blueprint domain → unchanged
               ├── edgraph domain   → unchanged (ECA has no generic EdGraph)
               ├── slate domain     → unchanged (finer-grained than ECA's WidgetTree)
               └── run_python_script → unchanged
```

## Changed Files (7 files, +961/-45)

### C++ Layer
| File | Change |
|------|--------|
| `Source/RemoteMCP/RemoteMCP.Build.cs` | ECABridge optional dependency via `Directory.Exists()` → `WITH_ECA_BRIDGE=0/1` |
| `RemoteMCP.uplugin` | ECABridge added as optional plugin dependency |
| `Source/RemoteMCP/Public/MCPTools/MCPECAProxy.h` | New: 4 UFUNCTION proxy methods |
| `Source/RemoteMCP/Private/MCPTools/MCPECAProxy.cpp` | New: conditional compilation, safe stubs when ECA absent |

### Python Layer
| File | Change |
|------|--------|
| `Content/Python/tools/eca_tools.py` | New: "eca" domain with 4 tools |
| `Content/Python/tools/tool_register.py` | +2 lines: import + register |
| `Content/Python/tests/test_eca_tools.py` | New: 23 unit tests |

## ECA Domain Tools

| Tool | Purpose |
|------|---------|
| `eca_status` | Is ECA available? How many commands/categories? |
| `eca_list(category)` | List commands with parameter schemas |
| `eca_call(command, arguments)` | Execute any ECA command |
| `eca_search(keyword)` | Search commands by name/description |

## Compatibility

- **WITH_ECA_BRIDGE=1** (engine has ECABridge): full proxy, 238 commands available
- **WITH_ECA_BRIDGE=0** (no ECABridge): all ECA methods return structured "not available" error, zero impact on existing functionality
- **All 6 existing domains unaffected** — verified via regression tests
- **Hot reload safe** — eca domain survives `mcp.reload`

## Verification

- Compiled: Z2 project, 47.6s, 0 errors
- Runtime: 238 commands, 19 categories, eca_call returns actors
- Regression: 8/8 old-path tests passed
- Unit tests: 23/23 passed
