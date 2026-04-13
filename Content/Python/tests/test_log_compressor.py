"""
日志压缩器单元测试
使用 Z2 项目实际遇到的日志数据
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from foundation.log_compressor import compress_logs, format_compressed_logs, _is_noise


# ─── 测试数据：来自 Z2 项目实际操作的日志 ───

REAL_SPAWN_LOGS = [
    "[LogActorFactory] Loading ActorFactory Class /Script/Engine.LevelInstance ",
    "[LogEditor] Attempting to add actor of class 'BP_Spawner_Manny_C' to level at -1000.00,-200.00,0.00 ",
    "[LogEditor] Attempting to add actor of class 'BP_Spawner_Manny_C' to level at -600.00,-200.00,0.00 ",
    "[LogEditor] Attempting to add actor of class 'BP_Spawner_Manny_C' to level at -200.00,-200.00,0.00 ",
    "[LogStreaming] FlushAsyncLoading(647): 1 QueuedPackages, 0 AsyncPackages ",
    "[LogStreaming] FlushAsyncLoading(648): 1 QueuedPackages, 0 AsyncPackages ",
    "[LogUObjectHash] Compacting FUObjectHashTables data took   1.38ms ",
    "[None] OBJ SavePackage: Generating thumbnails for [0] asset(s) in package [/Game/Maps/UnitTest/L_CombatArena_Excuate] ([2] browsable assets)... ",
    "[None] OBJ SavePackage: Finished generating thumbnails for package [/Game/Maps/UnitTest/L_CombatArena_Excuate] ",
    "[Cmd] OBJ SAVEPACKAGE PACKAGE=\"/Game/Maps/UnitTest/L_CombatArena_Excuate\" FILE=\"../../../Projects/Z2Game/Content/Maps/UnitTest/L_CombatArena_Excuate.umap\" SILENT=true AUTOSAVING=false KEEPDIRTY=false ",
    "[LogSavePackage] Moving output files for package: /Game/Maps/UnitTest/L_CombatArena_Excuate ",
    "[LogSavePackage] Moving '../../../Projects/Z2Game/Saved/L_CombatArena_Excuate.tmp' to '../../../Projects/Z2Game/Content/Maps/UnitTest/L_CombatArena_Excuate.umap' ",
    "[LogFileHelpers] All files are already saved. ",
]

REAL_COMPILE_LOGS = [
    "[LogBlueprint] Compiling Blueprint '/Game/Developers/alain/BluePrints/BP_Execute_Trigger_TestModify.BP_Execute_Trigger_TestModify' ",
    "[LogFileHelpers] InternalPromptForCheckoutAndSave started... ",
    "[LogFileHelpers] Saving Package: /Game/Developers/alain/BluePrints/BP_Execute_Trigger_TestModify ",
    "[None] OBJ SavePackage: Generating thumbnails for [2] asset(s) in package [/Game/Developers/alain/BluePrints/BP_Execute_Trigger_TestModify] ([2] browsable assets)... ",
    "[None] OBJ SavePackage:     Rendered thumbnail for [Blueprint /Game/Developers/alain/BluePrints/BP_Execute_Trigger_TestModify.BP_Execute_Trigger_TestModify] ",
    "[None] OBJ SavePackage: Finished generating thumbnails for package [/Game/Developers/alain/BluePrints/BP_Execute_Trigger_TestModify] ",
    "[LogSavePackage] Moving output files for package: /Game/Developers/alain/BluePrints/BP_Execute_Trigger_TestModify ",
    "[LogSavePackage] Moving '../../../Projects/Z2Game/Saved/BP_Execute_Trigger_TestModify.tmp' to '../../../Projects/Z2Game/Content/Developers/alain/BluePrints/BP_Execute_Trigger_TestModify.uasset' ",
    "[LogFileHelpers] InternalPromptForCheckoutAndSave took 250.541 ms (total: 687.133 ms) ",
    "[LogSourceControl] Attempting 'p4 edit E:/YiningZ2/Projects/Z2Game/Content/Developers/alain/BluePrints/BP_Execute_Trigger_TestModify.uasset' ",
]

REAL_ERROR_LOGS = [
    "[LogUtils] The Editor is currently in a play mode. ",
    "[LogPython] Error: Cannot modify assets during PIE ",
]

REAL_MONTAGE_LOGS = [
    "[LogStreaming] FlushAsyncLoading(2311): 1 QueuedPackages, 0 AsyncPackages ",
    "[LogLinker] [AssetLog] E:\\YiningZ2\\Projects\\Z2Game\\Content\\Gameplay\\Animation\\AnimBlueprints\\Common\\ABP_DefaultLookAt_Template.uasset: Asset has been saved with empty engine version. The asset will be loaded but may be incompatible. ",
    "[LogLinker] [AssetLog] E:\\YiningZ2\\Projects\\Z2Game\\Content\\Gameplay\\Animation\\AnimBlueprints\\Common\\ABP_FootIK_Template.uasset: Asset has been saved with empty engine version. The asset will be loaded but may be incompatible. ",
    "[LogAnimationCompression] Building compressed animation data for AnimSequence /Game/Prototype/Demo/Character/CH_E_C30009/Animation/Sequence/Combat/Hit/AS_C30009_Dun0001_BeParry_BackHand_01 (Required Memory Estimate: 10.41 MB) ",
]

EMPTY_LOGS = []

ONLY_NOISE_LOGS = [
    "[LogStreaming] FlushAsyncLoading(100): 0 QueuedPackages, 0 AsyncPackages ",
    "[LogUObjectHash] Compacting FUObjectHashTables data took   0.5ms ",
    "",
    "  ",
]

# 包含未知前缀的日志（应该被保留）
UNKNOWN_PREFIX_LOGS = [
    "[LogSomethingNew] This is a new log category we haven't seen before ",
    "[LogEditor] Attempting to add actor of class 'MyActor_C' to level at 0,0,0 ",
    "[LogStreaming] FlushAsyncLoading(1): 1 QueuedPackages, 0 AsyncPackages ",
]


# ─── 测试用例 ───

def test_noise_filtering():
    """测试噪音过滤：已知噪音被过滤，未知前缀保留"""
    assert _is_noise("[LogStreaming] FlushAsyncLoading(647): 1 QueuedPackages") == True
    assert _is_noise("[LogUObjectHash] Compacting FUObjectHashTables data took 1.38ms") == True
    assert _is_noise("[None] OBJ SavePackage: Generating thumbnails for [0] asset(s)") == True
    assert _is_noise("[LogSavePackage] Moving output files for package: /Game/Maps/") == True
    assert _is_noise("[LogFileHelpers] All files are already saved.") == True
    assert _is_noise("") == True
    assert _is_noise("  ") == True
    
    # 有效信息不应被过滤
    assert _is_noise("[LogEditor] Attempting to add actor of class 'MyActor'") == False
    assert _is_noise("[LogBlueprint] Compiling Blueprint 'MyBP'") == False
    assert _is_noise("[LogPython] some output") == False
    assert _is_noise("[LogSourceControl] Attempting 'p4 edit file'") == False
    assert _is_noise("Error: something went wrong") == False
    assert _is_noise("[LogSomethingNew] unknown category") == False  # 未知前缀保留！
    
    print("  ✅ test_noise_filtering passed")


def test_spawn_logs_compression():
    """测试 spawn 场景的日志压缩"""
    result = compress_logs(REAL_SPAWN_LOGS)
    
    assert result['raw_line_count'] == 13
    assert result['compressed_line_count'] < 13
    assert result['compression_ratio'] > 0.3
    assert '3x BP_Spawner_Manny_C spawned' in result['summary']
    assert any('spawn BP_Spawner_Manny_C x3' in d for d in result['details'])
    assert len(result['errors']) == 0
    
    print(f"  ✅ test_spawn_logs: {result['raw_line_count']} → {result['compressed_line_count']} ({result['compression_ratio']:.0%} reduction)")


def test_compile_logs_compression():
    """测试编译场景的日志压缩"""
    result = compress_logs(REAL_COMPILE_LOGS)
    
    assert 'compiled: BP_Execute_Trigger_TestModify' in result['summary']
    assert 'p4:' in result['summary']
    assert any('compile' in d for d in result['details'])
    assert any('p4' in d for d in result['details'])
    assert result['compression_ratio'] > 0.3
    
    print(f"  ✅ test_compile_logs: {result['raw_line_count']} → {result['compressed_line_count']} ({result['compression_ratio']:.0%} reduction)")


def test_error_logs_preserved():
    """测试错误日志被完整保留"""
    result = compress_logs(REAL_ERROR_LOGS)
    
    assert len(result['errors']) >= 1
    assert any('PIE' in e for e in result['errors'])
    assert 'error' in result['summary'].lower()
    
    print(f"  ✅ test_error_logs: {len(result['errors'])} error(s) preserved")


def test_empty_logs():
    """测试空日志"""
    result = compress_logs(EMPTY_LOGS)
    
    assert result['raw_line_count'] == 0
    assert result['summary'] == '(no logs)'
    
    print("  ✅ test_empty_logs passed")


def test_only_noise():
    """测试全是噪音的日志"""
    result = compress_logs(ONLY_NOISE_LOGS)
    
    assert result['raw_line_count'] == 4
    assert len(result['filtered_lines']) == 0
    assert result['summary'] == '(no significant events)'
    
    print(f"  ✅ test_only_noise: {result['raw_line_count']} lines all filtered")


def test_unknown_prefix_preserved():
    """测试未知前缀的日志被保留（安全优先）"""
    result = compress_logs(UNKNOWN_PREFIX_LOGS)
    
    # 未知前缀应该出现在 filtered_lines 中
    assert any('LogSomethingNew' in line for line in result['filtered_lines'])
    # 已知噪音应该被过滤
    assert not any('FlushAsyncLoading' in line for line in result['filtered_lines'])
    
    print(f"  ✅ test_unknown_prefix: unknown prefix preserved, noise filtered")


def test_montage_logs():
    """测试 Montage 创建场景（大量 Linker 噪音）"""
    result = compress_logs(REAL_MONTAGE_LOGS)
    
    # Linker 和 Streaming 噪音应被过滤
    assert result['compression_ratio'] > 0.5
    # AnimationCompression 也是噪音
    assert not any('AnimationCompression' in line for line in result['filtered_lines'])
    
    print(f"  ✅ test_montage_logs: {result['raw_line_count']} → {result['compressed_line_count']} ({result['compression_ratio']:.0%} reduction)")


def test_format_output():
    """测试格式化输出"""
    result = compress_logs(REAL_SPAWN_LOGS)
    formatted = format_compressed_logs(result)
    
    assert '[log summary]' in formatted
    assert '[stats]' in formatted
    assert 'reduction' in formatted
    # 不应该包含原始行（默认 include_filtered=False）
    assert '[filtered lines' not in formatted
    
    # 带原始行
    formatted_full = format_compressed_logs(result, include_filtered=True)
    assert '[filtered lines' in formatted_full
    
    print(f"  ✅ test_format_output: formatted output looks good")
    print(f"     Preview:\n{formatted}")


def test_combined_real_scenario():
    """测试真实场景：spawn + compile + error 混合"""
    combined = REAL_SPAWN_LOGS + REAL_COMPILE_LOGS + REAL_ERROR_LOGS
    result = compress_logs(combined)
    
    assert result['raw_line_count'] == len(combined)
    assert 'spawned' in result['summary']
    assert 'compiled' in result['summary']
    assert 'error' in result['summary'].lower()
    assert len(result['errors']) >= 1
    assert result['compression_ratio'] > 0.5
    
    formatted = format_compressed_logs(result)
    print(f"  ✅ test_combined: {result['raw_line_count']} → {result['compressed_line_count']} ({result['compression_ratio']:.0%} reduction)")
    print(f"     Summary: {result['summary']}")


def run_all_tests():
    print("=" * 60)
    print("Log Compressor Unit Tests")
    print("=" * 60)
    
    tests = [
        test_noise_filtering,
        test_spawn_logs_compression,
        test_compile_logs_compression,
        test_error_logs_preserved,
        test_empty_logs,
        test_only_noise,
        test_unknown_prefix_preserved,
        test_montage_logs,
        test_format_output,
        test_combined_real_scenario,
    ]
    
    passed = 0
    failed = 0
    for test in tests:
        try:
            test()
            passed += 1
        except Exception as e:
            print(f"  ❌ {test.__name__} FAILED: {e}")
            failed += 1
    
    print(f"\n{'=' * 60}")
    print(f"Results: {passed} passed, {failed} failed, {passed + failed} total")
    print(f"{'=' * 60}")
    
    return failed == 0


if __name__ == '__main__':
    import sys
    if sys.platform == "win32":
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    success = run_all_tests()
    sys.exit(0 if success else 1)
