"""
LOD 框架单元测试
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from foundation.lod import LOD, lod_filter, lod_filter_list, lod_stats, NODE_SCHEMA, ACTOR_SCHEMA


# ─── 模拟数据：来自 edgraph_list_nodes 的真实结构 ───

SAMPLE_NODE_FULL = {
    "guid": "98B05F0C49C951890A7221911B4D27AE",
    "name": "K2Node_Event_0",
    "title": "Event BeginPlay",
    "class": "K2Node_Event",
    "pos_x": -200,
    "pos_y": 0,
    "comment": "",
    "pins": [
        {"name": "then", "direction": "output", "type": "exec", "linked_to_count": 1, "default_value": ""},
        {"name": "OutputDelegate", "direction": "output", "type": "delegate", "linked_to_count": 0, "default_value": ""},
    ],
    "properties": {"EventReference": "ReceiveBeginPlay", "bOverrideFunction": True},
}

SAMPLE_NODE_2 = {
    "guid": "7829E9164002ABCD",
    "name": "K2Node_CallFunction_24",
    "title": "Motion Match",
    "class": "K2Node_CallFunction",
    "pos_x": 100,
    "pos_y": 50,
    "comment": "",
    "pins": [
        {"name": "execute", "direction": "input", "type": "exec", "linked_to_count": 1},
        {"name": "AnimInstance", "direction": "input", "type": "object", "linked_to_count": 1},
        {"name": "AssetsToSearch", "direction": "input", "type": "array", "linked_to_count": 1},
        {"name": "Result", "direction": "output", "type": "struct", "linked_to_count": 1},
    ],
    "properties": {"FunctionReference": "MotionMatch"},
}

SAMPLE_ACTOR = {
    "name": "BP_Spawner_Manny_C_0",
    "label": "Spawner_BackStab_01",
    "class": "BP_Spawner_Manny_C",
    "location": {"x": -1000, "y": -200, "z": 0},
    "rotation": {"pitch": 0, "yaw": 0, "roll": 0},
    "scale": {"x": 1, "y": 1, "z": 1},
    "components": [
        {"name": "DefaultSceneRoot", "class": "SceneComponent"},
        {"name": "VisualizeStaticMesh", "class": "StaticMeshComponent"},
    ],
    "properties": {"Count": 1, "bAutoSpawn": True},
    "tags": [],
}


# ─── 测试用例 ───

def test_lod_includes():
    """LOD 级别包含关系"""
    assert LOD.includes("summary", "summary") == True
    assert LOD.includes("summary", "standard") == False
    assert LOD.includes("summary", "full") == False
    assert LOD.includes("standard", "summary") == True
    assert LOD.includes("standard", "standard") == True
    assert LOD.includes("standard", "full") == False
    assert LOD.includes("full", "summary") == True
    assert LOD.includes("full", "standard") == True
    assert LOD.includes("full", "full") == True
    print("  ✅ test_lod_includes passed")


def test_node_summary():
    """节点 summary 模式：只返回身份信息"""
    result = lod_filter(SAMPLE_NODE_FULL, NODE_SCHEMA, "summary")
    assert "guid" in result
    assert "name" in result
    assert "title" in result
    assert "class" in result
    assert "pins" not in result
    assert "properties" not in result
    assert "pos_x" not in result
    print(f"  ✅ test_node_summary: {len(result)} fields (from {len(SAMPLE_NODE_FULL)})")


def test_node_standard():
    """节点 standard 模式：身份 + 位置"""
    result = lod_filter(SAMPLE_NODE_FULL, NODE_SCHEMA, "standard")
    assert "guid" in result
    assert "pos_x" in result
    assert "pos_y" in result
    assert "pins" not in result
    assert "properties" not in result
    print(f"  ✅ test_node_standard: {len(result)} fields")


def test_node_full():
    """节点 full 模式：返回全量"""
    result = lod_filter(SAMPLE_NODE_FULL, NODE_SCHEMA, "full")
    assert result == SAMPLE_NODE_FULL
    print(f"  ✅ test_node_full: {len(result)} fields (unchanged)")


def test_actor_summary():
    """Actor summary：只返回身份"""
    result = lod_filter(SAMPLE_ACTOR, ACTOR_SCHEMA, "summary")
    assert "name" in result
    assert "label" in result
    assert "class" in result
    assert "location" not in result
    assert "components" not in result
    assert "properties" not in result
    print(f"  ✅ test_actor_summary: {len(result)} fields (from {len(SAMPLE_ACTOR)})")


def test_actor_standard():
    """Actor standard：身份 + 位置"""
    result = lod_filter(SAMPLE_ACTOR, ACTOR_SCHEMA, "standard")
    assert "label" in result
    assert "location" in result
    assert "components" not in result
    print(f"  ✅ test_actor_standard: {len(result)} fields")


def test_filter_list():
    """批量裁剪"""
    nodes = [SAMPLE_NODE_FULL, SAMPLE_NODE_2]
    result = lod_filter_list(nodes, NODE_SCHEMA, "summary")
    assert len(result) == 2
    assert "pins" not in result[0]
    assert "pins" not in result[1]
    print(f"  ✅ test_filter_list: {len(result)} items filtered")


def test_unknown_fields_preserved():
    """未标注的字段默认 SUMMARY（安全优先）"""
    data = {"guid": "abc", "unknown_field": "should be kept"}
    result = lod_filter(data, NODE_SCHEMA, "summary")
    assert "unknown_field" in result  # 未标注 → 默认 SUMMARY → 保留
    print("  ✅ test_unknown_fields_preserved passed")


def test_token_reduction():
    """验证 token 节省效果"""
    nodes = [SAMPLE_NODE_FULL, SAMPLE_NODE_2]
    
    summary = lod_filter_list(nodes, NODE_SCHEMA, "summary")
    stats = lod_stats(nodes, summary)
    
    assert stats["reduction"] > 0.3  # 至少 30% 节省
    print(f"  ✅ test_token_reduction: {stats['original_est_tokens']} → {stats['filtered_est_tokens']} tokens ({stats['reduction']:.0%} reduction)")


def test_empty_data():
    """空数据"""
    assert lod_filter({}, NODE_SCHEMA, "summary") == {}
    assert lod_filter_list([], NODE_SCHEMA, "summary") == []
    print("  ✅ test_empty_data passed")


def test_realistic_graph():
    """模拟真实场景：12 个节点的 EventGraph"""
    nodes = []
    for i in range(12):
        nodes.append({
            "guid": f"GUID_{i:04d}",
            "name": f"K2Node_{i}",
            "title": f"Node {i}",
            "class": "K2Node_CallFunction",
            "pos_x": i * 200,
            "pos_y": 0,
            "comment": "",
            "pins": [{"name": f"pin_{j}", "direction": "input", "type": "exec"} for j in range(4)],
            "properties": {"FunctionReference": f"Function_{i}", "bPure": False},
        })
    
    summary = lod_filter_list(nodes, NODE_SCHEMA, "summary")
    standard = lod_filter_list(nodes, NODE_SCHEMA, "standard")
    
    stats_s = lod_stats(nodes, summary)
    stats_st = lod_stats(nodes, standard)
    
    assert stats_s["reduction"] > 0.5  # summary 至少省 50%
    assert stats_st["reduction"] > 0.2  # standard 至少省 20%
    assert stats_st["reduction"] < stats_s["reduction"]  # standard 比 summary 大
    
    print(f"  ✅ test_realistic_graph (12 nodes):")
    print(f"     summary:  {stats_s['original_est_tokens']} → {stats_s['filtered_est_tokens']} ({stats_s['reduction']:.0%})")
    print(f"     standard: {stats_st['original_est_tokens']} → {stats_st['filtered_est_tokens']} ({stats_st['reduction']:.0%})")


def run_all_tests():
    print("=" * 60)
    print("LOD Framework Unit Tests")
    print("=" * 60)
    
    tests = [
        test_lod_includes,
        test_node_summary,
        test_node_standard,
        test_node_full,
        test_actor_summary,
        test_actor_standard,
        test_filter_list,
        test_unknown_fields_preserved,
        test_token_reduction,
        test_empty_data,
        test_realistic_graph,
    ]
    
    passed = 0
    failed = 0
    for test in tests:
        try:
            test()
            passed += 1
        except Exception as e:
            print(f"  ❌ {test.__name__} FAILED: {e}")
            import traceback
            traceback.print_exc()
            failed += 1
    
    print(f"\n{'=' * 60}")
    print(f"Results: {passed} passed, {failed} failed, {passed + failed} total")
    print(f"{'=' * 60}")
    return failed == 0


if __name__ == '__main__':
    if sys.platform == "win32":
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    success = run_all_tests()
    sys.exit(0 if success else 1)
