"""
eca_tools.py — ECABridge proxy domain for RemoteMCP.

Registers the "eca" domain with 4 tools that proxy calls to UMCPECAProxy (C++),
which in turn delegates to FECACommandRegistry when WITH_ECA_BRIDGE=1.

When ECABridge is not available (WITH_ECA_BRIDGE=0), every tool returns a
structured error message — no exceptions, no crashes.

Design notes:
  - eca_call uses call_cpp_tools (FJsonObjectParameter bridge) because
    CallECACommand needs the SafeCallCPPFunction delegate path.
  - eca_status / eca_list / eca_categories call C++ methods that return
    FString directly (no delegate overhead needed for metadata queries).
  - eca_search is pure Python — filters the eca_list result client-side.
"""

from __future__ import annotations

import json
from typing import Any, Dict, List, Optional

from foundation.mcp_app import UnrealMCP
from foundation.utility import call_cpp_tools


def _has_eca_proxy() -> bool:
    """Check if UMCPECAProxy is available in the unreal module."""
    try:
        import unreal
        return hasattr(unreal, "MCPECAProxy")
    except ImportError:
        return False


def _call_eca_proxy_string(method_name: str, *args: Any) -> dict:
    """Call a UMCPECAProxy static method that returns FString (JSON).

    Used for ListECACommands / ListECACategories which return FString
    directly (not FJsonObjectParameter), so we don't need the delegate bridge.
    """
    try:
        import unreal
        proxy = unreal.MCPECAProxy
        method = getattr(proxy, method_name, None)
        if method is None:
            return {"success": False, "error": f"MCPECAProxy.{method_name} not found"}
        result_str = method(*args)
        return json.loads(result_str) if result_str else {}
    except Exception as e:
        return {"success": False, "error": f"MCPECAProxy.{method_name} failed: {e}"}


def register_eca_tools(mcp: UnrealMCP) -> None:
    """Register the 'eca' domain — proxy to ECABridge 238+ commands."""

    mcp.set_domain_description(
        "eca",
        (
            "ECABridge 238+ C++ 原子命令，19 个分类。\n"
            "★ 蓝图逻辑实现 → 必须用 BlueprintLisp（lisp_to_blueprint），一次调用替代 5-10 次节点操作\n"
            "★ 发现命令 → eca_search(keyword) 或 eca_list(category)，不要猜测命令名\n"
            "分类: Actor(8) Asset(31) Blueprint(13) BlueprintLisp(4) BlueprintNode(24) "
            "Component(5) DataTable(4) Editor(14) MaterialNode(16) Mesh(40) "
            "Niagara(23) MVVM(11) WidgetTree(15) View(3) Save(4) Project(5) AI(4) Events(3)"
        ),
    )

    # ── eca_status ──────────────────────────────────────────────────

    @mcp.domain_tool("eca")
    def eca_status() -> Dict[str, Any]:
        """Check whether ECABridge is available. Call this first before using other eca tools.

        Returns:
            {
                "available": bool,
                "categories": ["Actor", "Blueprint", "BlueprintLisp", "Mesh", ...] or null,
                "command_count": int (238 when fully available),
                "message": str
            }
        """
        if not _has_eca_proxy():
            return {
                "available": False,
                "categories": None,
                "command_count": 0,
                "message": "MCPECAProxy not found — ECABridge may not be compiled into this build.",
            }

        try:
            import unreal
            available = unreal.MCPECAProxy.is_eca_available()
        except Exception as e:
            return {
                "available": False,
                "categories": None,
                "command_count": 0,
                "message": f"IsECAAvailable check failed: {e}",
            }

        if not available:
            return {
                "available": False,
                "categories": None,
                "command_count": 0,
                "message": "ECABridge compiled but no commands registered (plugin may be disabled).",
            }

        # Fetch summary
        cat_result = _call_eca_proxy_string("list_eca_categories")
        cmd_result = _call_eca_proxy_string("list_eca_commands", "")

        categories = cat_result.get("categories", [])
        command_count = cmd_result.get("count", 0)

        return {
            "available": True,
            "categories": categories,
            "command_count": command_count,
            "message": f"ECABridge active: {command_count} commands in {len(categories)} categories.",
        }

    # ── eca_list ────────────────────────────────────────────────────

    @mcp.domain_tool("eca")
    def eca_list(category: str = "") -> Dict[str, Any]:
        """List ECA commands, optionally filtered by category.

        Args:
            category: Filter by category name. Empty string returns all commands.
                      Common categories: Actor, Asset, Blueprint, BlueprintLisp,
                      BlueprintNode, Component, DataTable, Editor, MaterialNode,
                      Mesh, Niagara, MVVM, WidgetTree, View, Save, Project, AI, Events.

        Returns:
            {
                "commands": [{"name": ..., "description": ..., "category": ..., "parameters": [...]}, ...],
                "count": int,
                "category_filter": str or absent
            }
        """
        if not _has_eca_proxy():
            return {"success": False, "error": "MCPECAProxy not available."}

        return _call_eca_proxy_string("list_eca_commands", category)

    # ── eca_call ────────────────────────────────────────────────────

    @mcp.domain_tool("eca")
    def eca_call(command: str, arguments: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """Execute any ECA command by name.

        ★ PREFERRED for Blueprint logic: command="lisp_to_blueprint"
          (one call replaces 5-10 manual node operations)
        ★ Read Blueprint as Lisp: command="blueprint_to_lisp"
        ★ Syntax help: command="blueprint_lisp_help" (topic: forms/events/flow/expressions/math/arrays/examples/all)

        Discovery: use eca_search(keyword) or eca_list(category) first.
        Do NOT guess command names — always discover first.

        Args:
            command: ECA command name (e.g. "lisp_to_blueprint", "get_actors_in_level").
            arguments: Command-specific parameters as a dict. Defaults to {}.

        Returns:
            On success: {"success": true, "command": "...", "result": {...}}
            On error:   {"success": false, "command": "...", "error": "..."}
        """
        if not command:
            return {"success": False, "error": "Missing required argument: command"}

        if not _has_eca_proxy():
            return {"success": False, "error": "MCPECAProxy not available."}

        try:
            import unreal
            return call_cpp_tools(
                unreal.MCPECAProxy.call_eca_command,
                {"command": command, "arguments": arguments or {}},
            )
        except Exception as e:
            return {"success": False, "command": command, "error": str(e)}

    # ── eca_search ──────────────────────────────────────────────────

    @mcp.domain_tool("eca")
    def eca_search(keyword: str, category: str = "") -> Dict[str, Any]:
        """Search ECA commands by keyword in name and description.

        Use this when you don't know the exact command name.
        Example: eca_search("mesh boolean") → finds mesh_boolean command.

        Args:
            keyword: Search term (case-insensitive, matched against name and description).
            category: Optional category filter to narrow search scope.

        Returns:
            {
                "keyword": str,
                "matches": [{"name": ..., "description": ..., "category": ...}, ...],
                "count": int
            }
        """
        if not keyword:
            return {"keyword": "", "matches": [], "count": 0, "error": "keyword is required"}

        if not _has_eca_proxy():
            return {"keyword": keyword, "matches": [], "count": 0, "error": "MCPECAProxy not available."}

        # Fetch all (or category-filtered) commands, then filter client-side
        all_cmds = _call_eca_proxy_string("list_eca_commands", category)
        commands = all_cmds.get("commands", [])

        kw_lower = keyword.lower()
        matches = []
        for cmd in commands:
            name = cmd.get("name", "")
            desc = cmd.get("description", "")
            if kw_lower in name.lower() or kw_lower in desc.lower():
                matches.append({
                    "name": name,
                    "description": desc,
                    "category": cmd.get("category", ""),
                })

        return {
            "keyword": keyword,
            "category_filter": category or None,
            "matches": matches,
            "count": len(matches),
        }
