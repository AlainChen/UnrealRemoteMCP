# Branch: lab/lod-summary

> Harness Engineering 实践 — 让 Agent 自适应决定返回数据的精度，解决大世界项目 Actor 数量爆炸问题。

## 问题

Z2 项目使用 World Partition，单关卡 Actor 数量可达 **数千个**。
当 AI 调用 `get_actors_in_level` 时，返回全量数据会：
- 爆 token（一个 Actor 的 full 信息 ~200 tokens，1000 个 = 200K tokens）
- 淹没 AI 的上下文窗口，导致后续推理质量下降
- 大部分 StaticMesh/Light 对 gameplay 任务毫无意义

## 核心思路：LOD for LLM

借用虚幻引擎的 LOD（Level of Detail）概念——**离 AI 当前任务越远的信息，精度越低**：

```
Actor 数量    → 自动选择 detail_level
  ≤20        → standard (name + class + location + rotation + scale)
  21-100     → summary  (name + class only)
  >100       → overview (class 统计 + gameplay actor 空间排序)
```

### overview 模式的关键设计

省 token 的目的不是让 LLM 看到更少，而是**在同样的上下文窗口里看到更多有价值的东西**。

- `by_class` 统计：`{"StaticMeshActor": 340, "BP_Spawner": 5, "TriggerBox": 3}`
- `gameplay_actors` 保留坐标并按空间排序（X 轴，粗略的关卡进程顺序）
- 噪音过滤：StaticMesh/Landscape/Light/Fog/PostProcess 等环境 Actor 只计数不列出

## Changed Files (4 files, +637/-112)

| File | Change |
|------|--------|
| `Content/Python/foundation/lod.py` | **新增**: LOD 框架核心 — `auto_detail_level()`, `apply_lod()` |
| `Content/Python/tools/edit_tools.py` | 重写 `get_actors_in_level` — 加入自适应 LOD |
| `Content/Python/tools/edgraph_tools.py` | `edgraph_list_nodes` 加入 detail_level 参数 |
| `Content/Python/tests/test_lod.py` | **新增**: LOD 框架单元测试 |

## 与 ECA Integration 的关系

**零冲突。** 这个分支改的是 Python 层的数据返回策略，ECA 分支改的是 C++ 层的桥接。
两者可以独立 merge 到 master。

合并后，`eca_call("get_actors_in_level")` 返回的是 ECA 的原始全量数据，
而 `level/get_actors_in_level`（RemoteMCP 原生）返回的是 LOD 自适应数据。
AI 可以根据需要选择用哪个。

## Harness Engineering 背景

这是 Alain 的 "Harness Engineering" 方法论的实践：
- **L2 工具系统层**：工具自身具备智能（自适应返回精度）
- **L5 评估与观测层**：通过 `_meta.effective_level` 让 AI 知道当前精度
- **核心原则**：绝不能因为省 token 而牺牲 LLM 对关卡的空间感知能力
