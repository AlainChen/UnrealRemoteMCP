# Blueprint Lisp DSL — AI 工作指南

> 基于 ECABridge `ECABlueprintLispCommands.cpp` (6652 行) 源码提炼
> 适用于通过 RemoteMCP eca domain 调用

## 快速开始

### 三个核心命令

| 命令 | 用途 | 调用方式 |
|------|------|---------|
| `lisp_to_blueprint` | **写蓝图逻辑（首选）** | `eca_call("lisp_to_blueprint", {"blueprint_path": "/Game/...", "code": "...", "auto_layout": true})` |
| `blueprint_to_lisp` | 读取现有蓝图为 Lisp | `eca_call("blueprint_to_lisp", {"blueprint_path": "/Game/..."})` |
| `blueprint_lisp_help` | 查看语法文档 | `eca_call("blueprint_lisp_help", {"topic": "all"})` |

### 最小闭环示例

```
# 1. 创建蓝图（用 blueprint domain）
dispatch_tool("blueprint", "create_blueprint",
  '{"name": "BP_Test", "package_path": "/Game/Test", "parent_class": "Actor"}')

# 2. 用 Lisp 写逻辑（用 eca domain）
dispatch_tool("eca", "eca_call", '{
  "command": "lisp_to_blueprint",
  "arguments": {
    "blueprint_path": "/Game/Test/BP_Test",
    "code": "(event BeginPlay (print \"Hello World!\"))",
    "auto_layout": true
  }
}')

# 3. 编译验证（用 blueprint domain）
dispatch_tool("blueprint", "compile_blueprint",
  '{"blueprint_path": "/Game/Test/BP_Test"}')
```

---

## 语法速查

### 顶层 Form（只有这些可以出现在最外层）

```lisp
(event EventName body...)           ; 事件处理器
(func FuncName body...)             ; 函数定义
(macro MacroName body...)           ; 宏定义（尚未完全支持）
(var VarName Type)                  ; 变量声明
(comment "text")                    ; 注释节点
(on-component CompName EventType body...)  ; 组件委托事件
(on-input ActionName :pressed body :released body)  ; 输入事件
```

### 事件

```lisp
; 标准事件（自动映射：BeginPlay→ReceiveBeginPlay, Tick→ReceiveTick, EndPlay→ReceiveEndPlay）
(event BeginPlay body...)
(event Tick :params ((DeltaTime Float)) body...)
(event EndPlay body...)

; 自定义事件
(event MyCustomEvent body...)

; 组件事件（注意：不要用 (event OnComponentBeginOverlap ...)，必须用 on-component）
(on-component BoxCollision BeginOverlap
  :params ((OtherActor Actor))
  (print "Overlap!"))

(on-component MeshComp Hit
  :params ((OtherActor Actor))
  body...)

; 输入事件
(on-input Jump :pressed (call self LaunchCharacter (vec 0 0 600)))
```

### 执行 Form（有 exec pin，控制执行流）

```lisp
(seq stmt1 stmt2 ...)               ; 顺序执行多条语句
(branch cond :true body :false body) ; 条件分支
(let var expr)                       ; 局部变量绑定（不创建蓝图变量，只是节点别名）
(let ((v1 e1) (v2 e2)) body...)     ; Common Lisp 风格多绑定
(set VarName expr)                   ; 设置蓝图变量
(call target func args...)           ; 调用对象方法
(call self func args...)             ; 调用自身方法
(print message)                      ; PrintString 简写
(log message)                        ; PrintWarning 简写
(delay seconds)                      ; 延迟节点
(return)                             ; 从函数返回
(return value)                       ; 带值返回
(comment "text")                     ; 注释节点

; 循环
(foreach item collection body...)    ; ForEach 循环
(for index start end body...)        ; 整数 ForLoop
(forloop index start end body...)    ; for 的别名

; 类型转换
(cast TargetType objExpr body...)    ; 动态转换，body 中用 _cast_result 引用结果

; Switch
(switch-int value :0 body0 :1 body1 :default body)

; 快捷命令
(spawn ActorClass Location)          ; SpawnActor
(destroy)                            ; DestroyActor（self）
(destroy target)                     ; DestroyActor（指定目标）
(get-component ComponentClass)       ; GetComponentByClass
(play-sound SoundAsset)              ; PlaySoundAtLocation
(play-sound SoundAsset Location)
(set-timer Duration bLoop)           ; SetTimer
(clear-timer Handle)                 ; ClearTimer

; 数组操作（有 exec pin）
(array-add array item)
(array-remove array index)
(array-clear array)
(array-shuffle array)
(array-reverse array)
```

### 纯表达式（无 exec pin，返回值）

```lisp
; 函数调用
(FuncName args...)                   ; 全局/库函数
(call target func args...)           ; 对象方法（纯函数版本）
(valid? obj)                         ; IsValid 检查
(IsValid obj)                        ; 同上

; 条件
(select cond true-val false-val)     ; 内联三元运算

; 组件/变量/结构体
(component Name)                     ; 按名称获取组件
(get VarName)                        ; 显式获取变量
(. struct field)                     ; 结构体字段访问，如 (. loc X)
(make Type :Field val ...)           ; 构造结构体，如 (make Rotator :Pitch 0 :Yaw 90 :Roll 0)

; 字面量构造
(vec x y z)                          ; FVector
(vec2 x y)                           ; FVector2D
(rot roll pitch yaw)                 ; FRotator
(asset "/Game/Path/To/Asset")        ; 资产引用

; Actor 查询
(get-actor-of-class ClassName)
(get-all-actors-of-class ClassName)

; 数学运算符
(+ a b) (- a b) (* a b) (/ a b) (% a b)
(< a b) (> a b) (<= a b) (>= a b) (= a b) (!= a b)
(and a b) (or a b) (not a)

; 数学函数
(sin x) (cos x) (tan x) (asin x) (acos x) (atan x) (atan2 y x)
(abs x) (sqrt x) (pow x y) (exp x) (log x)
(floor x) (ceil x) (round x) (sign x) (frac x)
(min a b) (max a b) (clamp x min max) (lerp a b t)
(random) (random-range min max) (random-int max) (random-int-range min max)

; 数组查询（纯表达式）
(array-length array)
(array-get array index)
(array-contains? array item)
(array-find array item)
```

### 字面量

```lisp
42, 3.14, -5        ; 数字
"hello world"        ; 字符串（支持 \n \t \\ \"）
true, false          ; 布尔
nil                  ; 空值
self                 ; 自身引用
; 这是注释          ; 分号到行尾
```

### 函数定义

```lisp
; 简单函数
(func SayHello
  (print "Hello!"))

; 带参数的函数
(func ApplyDamage
  :inputs ((Target Actor) (Amount Float))
  (call Target TakeDamage Amount))

; 带返回值的函数
(func CalculateSpeed
  :inputs ((Distance Float) (Time Float))
  :outputs ((Speed Float))
  (return (/ Distance Time)))

; 支持的参数类型:
; Boolean/Bool, Integer/Int, Float, Double, String, Name, Text,
; Vector, Rotator, Transform, Actor, Object
```

---

## 常见模式

### 1. BeginPlay 初始化

```lisp
(event BeginPlay
  (seq
    (set IsActive true)
    (set Health 100)
    (print "Actor spawned!")))
```

### 2. Tick 每帧更新

```lisp
(event Tick
  :params ((DeltaTime Float))
  (let speed (* DeltaTime 100))
  (call self AddActorWorldOffset (vec speed 0 0)))
```

### 3. 碰撞/重叠响应

```lisp
(on-component TriggerBox BeginOverlap
  :params ((OtherActor Actor))
  (cast Character OtherActor
    (seq
      (call _cast_result AddHealth 25)
      (PlaySound2D (asset "/Game/Sounds/S_Pickup"))
      (destroy))))
```

### 4. 分支 + 条件

```lisp
(event BeginPlay
  (branch (> Health 0)
    :true (print "Alive!")
    :false (print "Dead!")))
```

### 5. 循环处理

```lisp
; ForEach
(event BeginPlay
  (let enemies (get-all-actors-of-class EnemyClass))
  (foreach enemy enemies
    (call enemy SetTarget self)))

; ForLoop
(event BeginPlay
  (for i 0 10
    (print i)))
```

### 6. 资产引用

```lisp
(event BeginPlay
  (call MeshComp SetMaterial 0
    (asset "/Game/Materials/M_Glowing")))
```

### 7. 结构体操作

```lisp
(event BeginPlay
  (let loc (call self GetActorLocation))
  (print (. loc X))                          ; 获取 X 分量
  (let newRot (make Rotator :Pitch 0 :Yaw 90 :Roll 0))
  (call self SetActorRotation newRot))
```

---

## 常见坑

### 1. on-component vs event
**错误**: `(event OnComponentBeginOverlap ...)`
**正确**: `(on-component BoxCollision BeginOverlap ...)`
组件事件必须用 `on-component` form，指定组件名和事件类型。

### 2. let 不创建蓝图变量
`(let var expr)` 只是创建一个节点别名（追踪 node GUID + pin name），不会创建蓝图变量。
要设置真正的蓝图变量，用 `(set VarName expr)`。

### 3. cast 结果引用
`(cast Type obj body)` 后，在 body 中用 `_cast_result` 引用转换结果：
```lisp
(cast Character OtherActor
  (call _cast_result GetHealth))   ; ← _cast_result 是 cast 的输出
```

### 4. auto_layout 参数
`lisp_to_blueprint` 的 `auto_layout` 参数默认为 **false**。建议显式设为 true：
```json
{"blueprint_path": "...", "code": "...", "auto_layout": true}
```

### 5. 函数搜索顺序
调用函数时，引擎按以下顺序搜索：
KismetSystemLibrary → GameplayStatics → AActor → APawn → ACharacter
→ 组件类 → GeometryScript 类 → Blueprint 父类 → GeneratedClass
也会自动尝试 `K2_` 前缀变体。

### 6. 数值类型自动转换
连接 float pin 到 int pin 时，会自动插入 FTrunc/FFloor 转换节点。

---

## 工作流推荐

### 新建蓝图逻辑

```
1. create_blueprint (blueprint domain)
2. lisp_to_blueprint (eca domain, auto_layout=true)
3. compile_blueprint (blueprint domain)
4. 验证：在编辑器中打开蓝图查看节点图
```

### 修改现有蓝图

```
1. blueprint_to_lisp → 读取现有逻辑为 Lisp
2. 理解结构，修改 Lisp 代码
3. lisp_to_blueprint (clear_existing=true) → 重写
4. compile_blueprint → 验证
```

### 调试 Lisp 代码

```
1. parse_blueprint_lisp → 验证语法是否正确（不实际生成节点）
2. blueprint_lisp_help (topic="forms") → 查看特定语法
3. lisp_to_blueprint → 实际生成，检查 warnings 数组
```
