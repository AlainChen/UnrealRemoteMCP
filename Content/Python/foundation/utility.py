import json
from typing import Any, Callable, Optional

from mcp.types import TextContent
import pydantic_core
import unreal


class UnrealDelegateProxy:
    def __init__(self, delegate: Callable):
        self.delegate = delegate

    def call(self, in_parameter : unreal.JsonObjectParameter) -> unreal.JsonObjectParameter:
        return self.delegate(in_parameter)
    


def combine_code_path(root: str, plugin_name: str, relative_path: str) -> str:
    """Combine the code path from the specified root, plugin name and relative path.
    The path should be relative to the Project Source directory.
    Arguments:
        root: The root path of the project. It should be "Project" or "Engine".
        plugin_name: Optional, The name of the plugin. if is none, it will be the project name.
        relative_path: The relative path to the file from the Project Source directory.
    """
    source_path : str = "Source"
    base_path : Optional[str] = None
    if plugin_name is None or plugin_name is "":
        base_path = unreal.MCPPythonBridge.plugin_directory(plugin_name)
        base_path = unreal.Paths.combine(base_path, source_path) # type: ignore
    elif root == "Project":
        base_path= unreal.Paths.game_source_dir()
    elif root == "Engine":
        base_path= unreal.Paths.engine_source_dir()
    else:
        raise ValueError("Invalid root path. It should be 'Project' or 'Plugin'.")
    path = root + "/" + relative_path
    return path

def to_unreal_json(data: dict) -> unreal.JsonObjectParameter:
    string_data = json.dumps(data)
    return unreal.MCPJsonUtils.make_json_object(string_data)

def str_to_unreal_json(string_data: str) -> unreal.JsonObjectParameter:
    return unreal.MCPJsonUtils.make_json_object(string_data)

def parameter_to_string(json_obj: unreal.JsonObjectParameter) -> str:
    return unreal.MCPJsonUtils.json_object_to_string(json_obj)

def to_py_json(json_obj: unreal.JsonObjectParameter) -> dict:
    return json.loads(parameter_to_string(json_obj))

def call_cpp_tools(function : Callable, params: dict) -> dict:
    # json_params = to_unreal_json(params)
    # return to_py_json(function(json_params))
    str_ret = safe_call_cpp_tools(function, params)
    return json.loads(str_ret)

def safe_call_cpp_tools(function : Callable, params: dict) -> str:
    json_params = json.dumps(params)
    closure  = UnrealDelegateProxy(function)
    delegate = unreal.MCPCommandDelegate()
    delegate.bind_callable(closure.call)
    return unreal.MCPPythonBridge.safe_call_cpp_function(delegate,json_params) # type: ignore
    

def like_str_parameter(params:dict | str, name:str, default_value:Any) -> Any:
    if isinstance(params, dict):
        return params.get(name, default_value)
    elif isinstance(params, str):
        return params
    else:
        raise ValueError("Invalid params type. It should be a dictionary or a string.")
def to_json_value(data : dict | list | str | int | float | bool | None) -> dict:
    """Convert a Python data structure to a JSON-serializable value."""
    if data is TextContent :
        return data.to_dict()
    return json.loads(json.dumps(data))


def attach_logs_to_result(result: Any, logs: list[str]|str) -> Any:
    """Attach logs to the result in a dictionary."""
    
    if isinstance(result, dict):
        result['logs'] = str(logs)
        return result
    if isinstance(result, list) and len(result) > 0:
       text_content_ = result[0]
       if isinstance(text_content_, TextContent):
            text_content_.text = pydantic_core.to_json({
                    "result": text_content_.text,
                    "logs": str(logs)
                }).decode()
            return [text_content_]
    return result


def normalize_agent_result(
    raw: dict,
    *,
    default_message: str = "",
    default_risk_tier: str = "safe",
    default_session_disrupted: bool = False,
    default_reconnect_required: bool = False,
) -> dict:
    """Normalize C++ tool output into a more agent-friendly envelope.

    The current bridge still carries legacy fields such as `success` / `error`.
    This helper preserves those fields while adding a stable outer shape that is
    easier for external agents to reason about.
    """
    if raw is None:
        raw = {}

    ok = bool(raw.get("success", "error" not in raw))
    error_message = str(raw.get("error", "")).strip()
    message = error_message or str(raw.get("message", "")).strip() or default_message
    session_disrupted = bool(raw.get("session_disrupted", default_session_disrupted))
    reconnect_required = bool(raw.get("reconnect_required", default_reconnect_required or session_disrupted))
    risk_tier = str(raw.get("risk_tier", default_risk_tier))
    recommended_client_action = raw.get(
        "recommended_client_action",
        "reconnect" if reconnect_required else "continue",
    )

    reserved = {
        "success",
        "error",
        "message",
        "risk_tier",
        "session_disrupted",
        "reconnect_required",
        "warnings",
        "error_code",
        "ok",
        "data",
        "recommended_client_action",
    }
    data = {k: v for k, v in raw.items() if k not in reserved}

    error_code = raw.get("error_code") or infer_error_code(raw, message, reconnect_required)

    return {
        "ok": ok,
        "data": data,
        "warnings": list(raw.get("warnings", [])) if isinstance(raw.get("warnings", []), list) else [str(raw.get("warnings"))],
        "error_code": error_code,
        "message": message,
        "risk_tier": risk_tier,
        "session_disrupted": session_disrupted,
        "reconnect_required": reconnect_required,
        "recommended_client_action": recommended_client_action,
        # Keep legacy fields for compatibility while the fork transitions.
        **raw,
    }


def make_health_result(
    *,
    ok: bool,
    data: dict,
    message: str,
    warnings: list[str] | None = None,
    error_code: str | None = None,
    recommended_client_action: str = "continue",
) -> dict:
    return {
        "ok": ok,
        "data": data,
        "warnings": warnings or [],
        "error_code": error_code,
        "message": message,
        "risk_tier": "safe",
        "session_disrupted": False,
        "reconnect_required": False,
        "recommended_client_action": recommended_client_action,
    }


def infer_error_code(raw: dict, message: str, reconnect_required: bool) -> str | None:
    if not message:
        return "session_disrupted" if reconnect_required else None

    lowered = message.lower()
    if reconnect_required and ("restart" in lowered or "reconnect" in lowered):
        return "session_disrupted"
    if "missing '" in lowered or "must be provided" in lowered:
        return "invalid_arguments"
    if "already exists" in lowered:
        if "map" in lowered:
            return "map_already_exists"
        return "already_exists"
    if "not found" in lowered:
        if "map" in lowered or "template map" in lowered:
            return "map_not_found"
        return "not_found"
    if "current map path is empty" in lowered or "/temp/untitled" in lowered:
        return "map_unsaved"
    if "failed to get editor world" in lowered or "editor world" in lowered:
        return "editor_state_error"
    if reconnect_required:
        return "session_disrupted"
    return None
       

