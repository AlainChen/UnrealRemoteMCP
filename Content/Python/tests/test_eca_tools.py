"""
eca_tools.py 单元测试

测试策略:
  - Mock unreal 模块（测试不在 UE 内运行）
  - Mock MCPECAProxy C++ 方法的返回值
  - 验证 4 个 domain tool 的输入输出契约
  - 验证 ECA 不可用时的降级行为

运行:
  cd Content/Python
  python -m pytest tests/test_eca_tools.py -v
  # 或从项目根目录:
  uv run python -m pytest Content/Python/tests/test_eca_tools.py -v
"""

import json
import sys
import os
from unittest.mock import MagicMock, patch
from typing import Any, Dict

# ── Setup: mock unreal before any import that touches it ────────────

# Create a mock unreal module so imports don't fail outside UE
_mock_unreal = MagicMock()
sys.modules["unreal"] = _mock_unreal

# Add Content/Python to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from tools.eca_tools import (
    _has_eca_proxy,
    _call_eca_proxy_string,
    register_eca_tools,
)


# ── Fixtures / Helpers ──────────────────────────────────────────────

SAMPLE_COMMANDS = [
    {
        "name": "get_actors_in_level",
        "description": "Get all actors in the current level",
        "category": "Actor",
        "parameters": [
            {"name": "class_filter", "type": "string", "description": "Filter by class", "required": False}
        ],
    },
    {
        "name": "create_actor",
        "description": "Create a new actor in the level",
        "category": "Actor",
        "parameters": [
            {"name": "actor_type", "type": "string", "description": "Type of actor", "required": True}
        ],
    },
    {
        "name": "create_blueprint",
        "description": "Create a new Blueprint asset",
        "category": "Blueprint",
        "parameters": [
            {"name": "blueprint_name", "type": "string", "description": "Name", "required": True}
        ],
    },
    {
        "name": "mesh_boolean",
        "description": "CSG boolean operation on meshes",
        "category": "Mesh",
        "parameters": [],
    },
]

SAMPLE_CATEGORIES = ["Actor", "Blueprint", "Mesh"]

SAMPLE_LIST_RESPONSE = json.dumps({"commands": SAMPLE_COMMANDS, "count": len(SAMPLE_COMMANDS)})
SAMPLE_CATEGORIES_RESPONSE = json.dumps({"categories": SAMPLE_CATEGORIES, "count": len(SAMPLE_CATEGORIES)})
SAMPLE_FILTERED_RESPONSE = json.dumps({
    "commands": [c for c in SAMPLE_COMMANDS if c["category"] == "Actor"],
    "count": 2,
    "category_filter": "Actor",
})

UNAVAILABLE_RESPONSE = json.dumps({
    "success": False,
    "error": "ECABridge is not available (ListECACommands). This engine build does not include the ECABridge plugin (WITH_ECA_BRIDGE=0).",
})


def _make_mock_mcp() -> MagicMock:
    """Create a mock UnrealMCP that captures domain_tool registrations."""
    mcp = MagicMock()
    mcp._registered_tools: Dict[str, Any] = {}

    def mock_domain_tool(domain: str, **kwargs):
        def decorator(fn):
            mcp._registered_tools[fn.__name__] = fn
            return fn
        return decorator

    mcp.domain_tool = mock_domain_tool
    mcp.set_domain_description = MagicMock()
    return mcp


def _setup_eca_available():
    """Configure mock unreal with MCPECAProxy that returns sample data."""
    proxy = MagicMock()
    proxy.is_eca_available.return_value = True
    proxy.list_eca_commands.side_effect = lambda cat: (
        SAMPLE_FILTERED_RESPONSE if cat == "Actor" else SAMPLE_LIST_RESPONSE
    )
    proxy.list_eca_categories.return_value = SAMPLE_CATEGORIES_RESPONSE
    proxy.call_eca_command = MagicMock()  # Used via call_cpp_tools
    _mock_unreal.MCPECAProxy = proxy
    return proxy


def _setup_eca_unavailable():
    """Remove MCPECAProxy from mock unreal."""
    if hasattr(_mock_unreal, "MCPECAProxy"):
        del _mock_unreal.MCPECAProxy


# ── Tests: _has_eca_proxy ───────────────────────────────────────────

class TestHasEcaProxy:
    def test_available(self):
        _setup_eca_available()
        assert _has_eca_proxy() is True

    def test_unavailable(self):
        _setup_eca_unavailable()
        assert _has_eca_proxy() is False


# ── Tests: _call_eca_proxy_string ───────────────────────────────────

class TestCallEcaProxyString:
    def test_returns_parsed_json(self):
        _setup_eca_available()
        result = _call_eca_proxy_string("list_eca_categories")
        assert result["categories"] == SAMPLE_CATEGORIES
        assert result["count"] == 3

    def test_method_not_found(self):
        _setup_eca_available()
        del _mock_unreal.MCPECAProxy.nonexistent_method
        result = _call_eca_proxy_string("nonexistent_method")
        assert result["success"] is False
        assert "not found" in result["error"]


# ── Tests: eca_status ───────────────────────────────────────────────

class TestEcaStatus:
    def setup_method(self):
        self.mcp = _make_mock_mcp()
        register_eca_tools(self.mcp)
        self.eca_status = self.mcp._registered_tools["eca_status"]

    def test_available(self):
        _setup_eca_available()
        result = self.eca_status()
        assert result["available"] is True
        assert result["command_count"] == 4
        assert len(result["categories"]) == 3
        assert "active" in result["message"].lower()

    def test_unavailable_no_proxy(self):
        _setup_eca_unavailable()
        result = self.eca_status()
        assert result["available"] is False
        assert result["command_count"] == 0
        assert result["categories"] is None

    def test_unavailable_no_commands(self):
        proxy = _setup_eca_available()
        proxy.is_eca_available.return_value = False
        result = self.eca_status()
        assert result["available"] is False
        assert "disabled" in result["message"].lower()


# ── Tests: eca_list ─────────────────────────────────────────────────

class TestEcaList:
    def setup_method(self):
        self.mcp = _make_mock_mcp()
        register_eca_tools(self.mcp)
        self.eca_list = self.mcp._registered_tools["eca_list"]

    def test_list_all(self):
        _setup_eca_available()
        result = self.eca_list()
        assert result["count"] == 4
        assert len(result["commands"]) == 4

    def test_list_filtered(self):
        _setup_eca_available()
        result = self.eca_list(category="Actor")
        assert result["count"] == 2
        assert result["category_filter"] == "Actor"

    def test_unavailable(self):
        _setup_eca_unavailable()
        result = self.eca_list()
        assert result.get("success") is False


# ── Tests: eca_call ─────────────────────────────────────────────────

class TestEcaCall:
    def setup_method(self):
        self.mcp = _make_mock_mcp()
        register_eca_tools(self.mcp)
        self.eca_call = self.mcp._registered_tools["eca_call"]

    def test_missing_command(self):
        _setup_eca_available()
        result = self.eca_call(command="")
        assert result["success"] is False
        assert "Missing" in result["error"]

    def test_unavailable(self):
        _setup_eca_unavailable()
        result = self.eca_call(command="get_actors_in_level")
        assert result["success"] is False
        assert "not available" in result["error"].lower()

    def test_call_delegates_to_cpp(self):
        """Verify eca_call passes correct params to call_cpp_tools."""
        _setup_eca_available()
        with patch("tools.eca_tools.call_cpp_tools") as mock_call:
            mock_call.return_value = {"success": True, "command": "get_actors_in_level", "result": {"actors": []}}
            result = self.eca_call(command="get_actors_in_level", arguments={"class_filter": "BP_"})

            mock_call.assert_called_once()
            args = mock_call.call_args
            # First arg is the C++ function reference
            # Second arg is the params dict
            params = args[0][1]
            assert params["command"] == "get_actors_in_level"
            assert params["arguments"]["class_filter"] == "BP_"
            assert result["success"] is True

    def test_call_with_no_arguments(self):
        """Verify arguments defaults to {} when None."""
        _setup_eca_available()
        with patch("tools.eca_tools.call_cpp_tools") as mock_call:
            mock_call.return_value = {"success": True, "command": "get_actors_in_level", "result": {}}
            self.eca_call(command="get_actors_in_level")

            params = mock_call.call_args[0][1]
            assert params["arguments"] == {}


# ── Tests: eca_search ───────────────────────────────────────────────

class TestEcaSearch:
    def setup_method(self):
        self.mcp = _make_mock_mcp()
        register_eca_tools(self.mcp)
        self.eca_search = self.mcp._registered_tools["eca_search"]

    def test_search_by_name(self):
        _setup_eca_available()
        result = self.eca_search(keyword="actor")
        assert result["count"] == 2  # get_actors_in_level + create_actor
        names = [m["name"] for m in result["matches"]]
        assert "get_actors_in_level" in names
        assert "create_actor" in names

    def test_search_by_description(self):
        _setup_eca_available()
        result = self.eca_search(keyword="boolean")
        assert result["count"] == 1
        assert result["matches"][0]["name"] == "mesh_boolean"

    def test_search_case_insensitive(self):
        _setup_eca_available()
        result = self.eca_search(keyword="BLUEPRINT")
        assert result["count"] >= 1

    def test_search_with_category_filter(self):
        _setup_eca_available()
        result = self.eca_search(keyword="create", category="Actor")
        # Only Actor commands searched, so only create_actor matches
        assert result["count"] == 1
        assert result["matches"][0]["name"] == "create_actor"
        assert result["category_filter"] == "Actor"

    def test_search_no_results(self):
        _setup_eca_available()
        result = self.eca_search(keyword="zzz_nonexistent_zzz")
        assert result["count"] == 0
        assert result["matches"] == []

    def test_search_empty_keyword(self):
        result = self.eca_search(keyword="")
        assert result["count"] == 0
        assert "required" in result.get("error", "")

    def test_search_unavailable(self):
        _setup_eca_unavailable()
        result = self.eca_search(keyword="actor")
        assert result["count"] == 0
        assert "not available" in result.get("error", "").lower()


# ── Tests: registration ────────────────────────────────────────────

class TestRegistration:
    def test_all_tools_registered(self):
        mcp = _make_mock_mcp()
        register_eca_tools(mcp)
        expected = {"eca_status", "eca_list", "eca_call", "eca_search"}
        assert set(mcp._registered_tools.keys()) == expected

    def test_domain_description_set(self):
        mcp = _make_mock_mcp()
        register_eca_tools(mcp)
        mcp.set_domain_description.assert_called_once()
        args = mcp.set_domain_description.call_args[0]
        assert args[0] == "eca"
        assert "238" in args[1]
