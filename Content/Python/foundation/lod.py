"""
LOD (Level of Detail) 框架 — 工具返回值的渐进式细节控制

设计灵感：
- 游戏 LOD：距离决定面数
- HTTP Content Negotiation：客户端声明需要什么精度
- Lazy Evaluation：不到需要时不计算
- Progressive Disclosure：先展示最重要的，主动探索时再展示更多

使用方式：
    from foundation.lod import LOD, lod_filter

    # 定义字段的 LOD 级别
    SCHEMA = {
        "guid":             LOD.SUMMARY,
        "name":             LOD.SUMMARY,
        "class":            LOD.SUMMARY,
        "linked_pin_count": LOD.SUMMARY,
        "position":         LOD.STANDARD,
        "pin_count":        LOD.STANDARD,
        "pins":             LOD.FULL,
        "properties":       LOD.FULL,
        "default_values":   LOD.FULL,
    }

    # 按 LOD 级别裁剪
    filtered = lod_filter(raw_data, SCHEMA, level="summary")
"""

from typing import Any, Dict, List, Optional


class LOD:
    """LOD 级别常量"""
    SUMMARY = "summary"      # LOD 0: 最小信息集 (~200 tokens)
    STANDARD = "standard"    # LOD 1: 工作级信息 (~1000 tokens)
    FULL = "full"            # LOD 2: 完整原始数据 (~5000 tokens)

    # 级别包含关系：FULL 包含 STANDARD，STANDARD 包含 SUMMARY
    _HIERARCHY = {
        "summary": 0,
        "standard": 1,
        "full": 2,
    }

    @classmethod
    def includes(cls, requested: str, field_level: str) -> bool:
        """检查请求的级别是否包含该字段"""
        req = cls._HIERARCHY.get(requested, 0)
        fld = cls._HIERARCHY.get(field_level, 0)
        return req >= fld


def lod_filter(data: Dict[str, Any], schema: Dict[str, str], level: str = "summary") -> Dict[str, Any]:
    """
    按 LOD 级别裁剪单个字典。
    
    Args:
        data: 原始数据字典
        schema: {字段名: LOD 级别} 的映射
        level: 请求的 LOD 级别
    
    Returns:
        裁剪后的字典（只包含该级别及以下的字段）
    """
    if level == "full":
        return data  # full 模式返回全量，不裁剪

    result = {}
    for key, value in data.items():
        field_level = schema.get(key, LOD.SUMMARY)  # 未标注的字段默认 SUMMARY（安全优先）
        if LOD.includes(level, field_level):
            result[key] = value
    return result


def lod_filter_list(items: List[Dict[str, Any]], schema: Dict[str, str], level: str = "summary") -> List[Dict[str, Any]]:
    """
    按 LOD 级别裁剪字典列表。
    """
    if level == "full":
        return items
    return [lod_filter(item, schema, level) for item in items]


def lod_stats(original: Any, filtered: Any) -> Dict[str, Any]:
    """
    计算 LOD 裁剪的统计信息。
    
    Args:
        original: 裁剪前的数据（用于估算 token）
        filtered: 裁剪后的数据
    
    Returns:
        {"original_size": int, "filtered_size": int, "reduction": float}
    """
    import json
    orig_str = json.dumps(original, ensure_ascii=False, default=str)
    filt_str = json.dumps(filtered, ensure_ascii=False, default=str)
    orig_tokens = len(orig_str) // 4  # 粗略估算：4 字符 ≈ 1 token
    filt_tokens = len(filt_str) // 4
    return {
        "original_est_tokens": orig_tokens,
        "filtered_est_tokens": filt_tokens,
        "reduction": round(1.0 - filt_tokens / max(orig_tokens, 1), 2),
    }


# ─── 预定义的 LOD Schema ───

# EdGraph 节点
NODE_SCHEMA = {
    # SUMMARY: 节点身份 + 连接概况
    "guid": LOD.SUMMARY,
    "name": LOD.SUMMARY,
    "title": LOD.SUMMARY,
    "class": LOD.SUMMARY,
    
    # STANDARD: 位置 + pin 概况
    "pos_x": LOD.STANDARD,
    "pos_y": LOD.STANDARD,
    "comment": LOD.STANDARD,
    
    # FULL: 完整 pin 信息
    "pins": LOD.FULL,
    "properties": LOD.FULL,
}

# Actor 信息
ACTOR_SCHEMA = {
    # SUMMARY: 身份
    "name": LOD.SUMMARY,
    "label": LOD.SUMMARY,
    "class": LOD.SUMMARY,
    
    # STANDARD: 位置 + 基础属性
    "location": LOD.STANDARD,
    "rotation": LOD.STANDARD,
    "scale": LOD.STANDARD,
    
    # FULL: 组件 + 所有属性
    "components": LOD.FULL,
    "properties": LOD.FULL,
    "tags": LOD.FULL,
}

# EdGraph 连接
LINK_SCHEMA = {
    # SUMMARY: 连接关系
    "a": LOD.SUMMARY,
    "b": LOD.SUMMARY,
    
    # STANDARD: pin 类型信息
    "pin_type": LOD.STANDARD,
    
    # FULL: 完整 pin 路径
    "a_full_path": LOD.FULL,
    "b_full_path": LOD.FULL,
}
