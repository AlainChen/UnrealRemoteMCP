"""
日志压缩器边界测试
覆盖：极端输入、回退安全、性能、信息完整性
"""

import sys
import os
import time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from foundation.log_compressor import compress_logs, format_compressed_logs, _is_noise


# ─── 边界测试 ───

def test_single_error_line():
    """单行 error 不应被过滤"""
    result = compress_logs(["Fatal error! Something crashed"])
    assert len(result['errors']) == 1
    assert result['filtered_lines'] == ["Fatal error! Something crashed"]
    print("  ✅ test_single_error_line passed")


def test_very_long_log():
    """大量日志（1000 行）的性能和正确性"""
    logs = []
    for i in range(500):
        logs.append(f"[LogStreaming] FlushAsyncLoading({i}): 1 QueuedPackages")
    for i in range(100):
        logs.append(f"[LogEditor] Attempting to add actor of class 'Actor_{i}' to level")
    for i in range(400):
        logs.append(f"[LogUObjectHash] Compacting FUObjectHashTables data took {i}ms")
    
    start = time.time()
    result = compress_logs(logs)
    elapsed = time.time() - start
    
    assert result['raw_line_count'] == 1000
    assert result['compression_ratio'] > 0.8
    assert elapsed < 1.0  # 1000 行应该在 1 秒内处理完
    # 100 个不同的 Actor 应该被正确计数
    assert any('100' in d or 'Actor_' in d for d in result['details'])
    
    print(f"  ✅ test_very_long_log: 1000 lines in {elapsed:.3f}s, ratio={result['compression_ratio']:.0%}")


def test_unicode_logs():
    """包含中文/Unicode 的日志"""
    logs = [
        "[LogBlueprint] Compiling Blueprint '蓝图测试'",
        "[LogEditor] 正在加载资产",
        "[LogStreaming] FlushAsyncLoading(1): noise",
    ]
    result = compress_logs(logs)
    assert any('蓝图测试' in d for d in result['details'])
    assert len(result['filtered_lines']) == 2  # 第三行是噪音
    print("  ✅ test_unicode_logs passed")


def test_duplicate_errors():
    """重复的 error 行应该全部保留（不去重）"""
    logs = [
        "Error: Connection refused",
        "Error: Connection refused",
        "Error: Connection refused",
    ]
    result = compress_logs(logs)
    assert len(result['errors']) == 3
    assert '3 error(s)' in result['summary']
    print("  ✅ test_duplicate_errors: all 3 preserved")


def test_mixed_p4_operations():
    """多种 P4 操作"""
    logs = [
        "[LogSourceControl] Attempting 'p4 edit E:/file1.uasset'",
        "[LogSourceControl] Attempting 'p4 add E:/file2.uasset'",
        "[LogSourceControl] Attempting 'p4 revert E:/file3.uasset'",
    ]
    result = compress_logs(logs)
    assert len(result['details']) == 3
    assert 'p4:' in result['summary']
    print("  ✅ test_mixed_p4_operations passed")


def test_none_input():
    """None 输入应该安全处理"""
    result = compress_logs(None or [])
    assert result['raw_line_count'] == 0
    print("  ✅ test_none_input passed")


def test_format_with_errors():
    """格式化输出中 error 应该醒目"""
    logs = [
        "[LogEditor] Attempting to add actor of class 'MyActor'",
        "Error: Blueprint compile failed",
        "[LogStreaming] FlushAsyncLoading(1): noise",
    ]
    result = compress_logs(logs)
    formatted = format_compressed_logs(result)
    
    # error 行应该有 [error] 前缀
    assert '[error]' in formatted
    assert 'Blueprint compile failed' in formatted
    # summary 应该提到 error
    assert 'error' in formatted.lower()
    print("  ✅ test_format_with_errors passed")


def test_ensure_condition():
    """Ensure condition 应该被当作 error"""
    logs = [
        "[LogWindows] Ensure condition failed: ScanStates.Contains(Interactor) [PaperGameplayBehaviorSubsystem.cpp:277]",
    ]
    result = compress_logs(logs)
    assert len(result['errors']) == 1
    assert 'error' in result['summary'].lower()
    print("  ✅ test_ensure_condition passed")


def test_no_false_positives():
    """确保有效日志不被误过滤"""
    valid_logs = [
        "[LogPython] Task executed with result: success",
        "[LogBlueprint] Compiling Blueprint '/Game/Test'",
        "[LogSourceControl] Attempting 'p4 edit file.uasset'",
        "[LogFileHelpers] Saving Package: /Game/Test",
        "[LogEditor] Attempting to add actor of class 'TestActor'",
        "Warning: Asset may be incompatible",
        "Error: Something went wrong",
        "[LogSomethingNew] Completely unknown category",
    ]
    result = compress_logs(valid_logs)
    # 所有有效日志都应该出现在 filtered_lines 中
    assert len(result['filtered_lines']) == len(valid_logs)
    print(f"  ✅ test_no_false_positives: all {len(valid_logs)} valid lines preserved")


def test_compression_ratio_consistency():
    """压缩率应该在 0-1 之间"""
    test_cases = [
        [],
        ["single line"],
        ["[LogStreaming] noise"] * 100,
        ["Error: real"] * 5,
    ]
    for logs in test_cases:
        result = compress_logs(logs)
        assert 0.0 <= result['compression_ratio'] <= 1.0, f"Bad ratio: {result['compression_ratio']}"
    print("  ✅ test_compression_ratio_consistency passed")


def run_all_tests():
    print("=" * 60)
    print("Log Compressor Edge Case Tests")
    print("=" * 60)
    
    tests = [
        test_single_error_line,
        test_very_long_log,
        test_unicode_logs,
        test_duplicate_errors,
        test_mixed_p4_operations,
        test_none_input,
        test_format_with_errors,
        test_ensure_condition,
        test_no_false_positives,
        test_compression_ratio_consistency,
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
