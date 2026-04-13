"""
日志压缩器：将 UE 原始日志压缩为高信噪比的摘要。

设计原则：
1. 黑名单过滤已知噪音（默认保留未知前缀，安全优先）
2. 相同前缀的重复日志合并计数
3. 有效信息提取为结构化摘要
4. 原始日志保留在 raw 字段中，agent 需要时可以取

使用方式：
    from foundation.log_compressor import compress_logs
    result = compress_logs(raw_logs)
    # result = {
    #     "summary": "2 actors spawned, map saved, 1 compile error",
    #     "details": ["LogEditor: spawn BP_Spawner x2", ...],
    #     "errors": ["LogBlueprint: Error: ..."],
    #     "raw_line_count": 30,
    #     "compressed_line_count": 5,
    #     "compression_ratio": 0.83
    # }
"""

import re
from collections import Counter
from typing import List, Dict, Any, Optional


# ─── 黑名单：确定是噪音的日志前缀 ───
# 只过滤经过 Z2 项目实测确认的噪音，未知前缀默认保留
NOISE_PATTERNS = [
    # 异步加载（高频，零信息量）
    r'\[LogStreaming\] FlushAsyncLoading',
    # 资产链接版本警告（大量重复）
    r'\[LogLinker\] \[AssetLog\].*Asset has been saved with empty engine version',
    # UObject 哈希表压缩（内部 GC 信息）
    r'\[LogUObjectHash\] Compacting FUObjectHashTables',
    # 缩略图生成（保存时的内部流程）
    r'\[None\] OBJ SavePackage: Generating thumbnails',
    r'\[None\] OBJ SavePackage: Finished generating thumbnails',
    r'\[None\] OBJ SavePackage:\s+Rendered thumbnail',
    # 文件操作细节（保存流程的中间步骤）
    r'\[LogFileHelpers\] All files are already saved',
    r'\[LogFileHelpers\] InternalPromptForCheckoutAndSave took',
    r'\[LogFileHelpers\] InternalPromptForCheckoutAndSave started',
    # 保存包移动（内部文件操作）
    r'\[LogSavePackage\] Moving output files',
    r'\[LogSavePackage\] Moving \'',
    # Cmd 回显（命令本身的回显，不是结果）
    r'\[Cmd\] OBJ SAVEPACKAGE',
    # 文件删除重试（临时文件操作）
    r'\[LogFileManager\] DeleteFile was unable to delete',
    r'\[LogFileManager\] DeleteFile recovered',
    # ActorFactory 加载（内部工厂初始化）
    r'\[LogActorFactory\] Loading ActorFactory Class',
    # HAL 空行
    r'\[LogHAL\]\s*$',
    # 动画压缩（资产加载时的内部处理）
    r'\[LogAnimationCompression\] Building compressed animation data',
    r'\[LogAnimationCompression\] Storing compressed animation data',
]

# 编译为正则
_NOISE_REGEXES = [re.compile(p) for p in NOISE_PATTERNS]


# ─── 有效信息提取规则 ───
# 顺序重要：error/warning/ensure 必须在 python_output 之前匹配
EXTRACT_PATTERNS_ORDERED = [
    ('error', re.compile(r'(?:Error|Fatal)[:!]\s*(.+)', re.IGNORECASE)),
    ('warning', re.compile(r'Warning[:!]\s*(.+)', re.IGNORECASE)),
    ('ensure', re.compile(r'Ensure condition failed')),
    ('play_mode', re.compile(r'currently in a play mode')),
    ('actor_spawn', re.compile(r'\[LogEditor\] Attempting to add actor of class \'(\w+)\'')),
    ('blueprint_compile', re.compile(r'\[LogBlueprint\] Compiling Blueprint \'([^\']+)\'')),
    ('source_control', re.compile(r'\[LogSourceControl\] Attempting \'(p4 \w+) ([^\']+)\'')),
    ('save_package', re.compile(r'\[LogFileHelpers\] Saving Package: (.+)')),
    ('python_output', re.compile(r'\[LogPython\] (.+)')),
]


def _is_noise(line: str) -> bool:
    """检查一行日志是否是已知噪音"""
    stripped = line.strip()
    if not stripped:
        return True
    for regex in _NOISE_REGEXES:
        if regex.search(stripped):
            return True
    return False


def _extract_info(line: str) -> Optional[Dict[str, str]]:
    """从一行日志中提取结构化信息"""
    stripped = line.strip()
    for key, regex in EXTRACT_PATTERNS_ORDERED:
        match = regex.search(stripped)
        if match:
            return {'type': key, 'value': match.group(1) if match.groups() else stripped}
    return None


def compress_logs(raw_logs: List[str]) -> Dict[str, Any]:
    """
    压缩 UE 日志。
    
    Args:
        raw_logs: 原始日志行列表（来自 LogCaptureScope.get_logs()）
    
    Returns:
        {
            "summary": str,          # 一行摘要
            "details": [str],        # 有效信息（去重合并后）
            "errors": [str],         # 错误和警告（全部保留）
            "filtered_lines": [str], # 过滤后保留的原始行
            "raw_line_count": int,
            "compressed_line_count": int,
            "compression_ratio": float
        }
    """
    if not raw_logs:
        return {
            'summary': '(no logs)',
            'details': [],
            'errors': [],
            'filtered_lines': [],
            'raw_line_count': 0,
            'compressed_line_count': 0,
            'compression_ratio': 0.0,
        }

    # 1. 过滤噪音
    filtered = []
    noise_count = 0
    for line in raw_logs:
        if _is_noise(line):
            noise_count += 1
        else:
            filtered.append(line.strip())

    # 2. 提取结构化信息
    extractions = []
    errors = []
    for line in filtered:
        info = _extract_info(line)
        if info:
            extractions.append(info)
            if info['type'] in ('error', 'warning', 'ensure', 'play_mode'):
                errors.append(line)

    # 3. 合并计数
    spawn_counter = Counter()
    compile_list = []
    p4_ops = []
    saves = []
    other_details = []

    for ext in extractions:
        if ext['type'] == 'actor_spawn':
            spawn_counter[ext['value']] += 1
        elif ext['type'] == 'blueprint_compile':
            compile_list.append(ext['value'].split('/')[-1].split('.')[0])
        elif ext['type'] == 'source_control':
            p4_ops.append(ext['value'])
        elif ext['type'] == 'save_package':
            saves.append(ext['value'].split('/')[-1])
        elif ext['type'] == 'python_output':
            other_details.append(ext['value'])
        elif ext['type'] not in ('error', 'warning', 'ensure'):
            other_details.append(ext['value'])

    # 4. 生成摘要
    summary_parts = []
    if spawn_counter:
        for cls, count in spawn_counter.most_common():
            summary_parts.append(f'{count}x {cls} spawned')
    if compile_list:
        summary_parts.append(f'compiled: {", ".join(compile_list)}')
    if saves:
        summary_parts.append(f'saved: {", ".join(saves)}')
    if p4_ops:
        summary_parts.append(f'p4: {", ".join(p4_ops[:3])}')
    if errors:
        summary_parts.append(f'{len(errors)} error(s)')

    summary = '; '.join(summary_parts) if summary_parts else '(no significant events)'

    # 5. 生成 details
    details = []
    for cls, count in spawn_counter.most_common():
        details.append(f'spawn {cls} x{count}')
    for bp in compile_list:
        details.append(f'compile {bp}')
    for op in p4_ops:
        details.append(f'{op}')
    for s in saves:
        details.append(f'save {s}')
    for d in other_details[:10]:  # 限制其他细节数量
        details.append(d)

    compressed_count = len(details) + len(errors)
    raw_count = len(raw_logs)

    return {
        'summary': summary,
        'details': details,
        'errors': errors,
        'filtered_lines': filtered,
        'raw_line_count': raw_count,
        'compressed_line_count': compressed_count,
        'compression_ratio': round(1.0 - compressed_count / max(raw_count, 1), 2),
    }


def format_compressed_logs(compressed: Dict[str, Any], include_filtered: bool = False) -> str:
    """
    将压缩结果格式化为字符串，用于返回给 agent。
    
    Args:
        compressed: compress_logs() 的返回值
        include_filtered: 是否包含过滤后的原始行（默认不包含）
    """
    parts = []
    parts.append(f'[log summary] {compressed["summary"]}')
    
    if compressed['errors']:
        for e in compressed['errors']:
            parts.append(f'[error] {e}')
    
    if compressed['details']:
        for d in compressed['details'][:15]:  # 最多 15 条 detail
            parts.append(f'[detail] {d}')
    
    if include_filtered and compressed['filtered_lines']:
        parts.append(f'[filtered lines ({len(compressed["filtered_lines"])})]')
        for line in compressed['filtered_lines']:
            parts.append(f'  {line}')
    
    parts.append(f'[stats] {compressed["raw_line_count"]} raw → {compressed["compressed_line_count"]} compressed ({compressed["compression_ratio"]:.0%} reduction)')
    
    return '\n'.join(parts)
