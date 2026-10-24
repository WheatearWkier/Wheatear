# 原生 C++ 能力编排与发行包热更新设计

更新时间：2026-09-22

本文记录 Wheatear 后续升级为“原生 C++ 能力层 + 可组合行为层 + 发行包热更新”的总体路线。

目标不是重新引入 C# 或 Lua，而是在保留 C++ 性能、类型和调试能力的前提下，获得类似 Unity + Lua、UE C++ + Blueprint 的内容生产能力：

- 引擎底层能力仍然由 C++ 实现。
- 游戏规则由已有能力组合而成。
- 行为定义可以在编辑器中修改和编译。
- 正式打包后可以下载并加载新的行为资源。
- 不需要为了修改技能规则而重新编译整个引擎。

这是一份**后续设计文档**。当前源码中已经存在的能力与未来要新增的能力会分别标记，不能把规划内容当成已经完成的功能。

---

## 1. 核心结论

最适合 Wheatear 的方向是：

```text
原生 C++ 能力层
    ↓
WAO 动作配方
    ↓
WAO 行为图 / 行为字节码
    ↓
C++ Behavior VM
    ↓
EffectBundle / EffectLedger
    ↓
玩法服务、表现信号、存档和调试
```

这不是“用 C++ 源码当 Lua 脚本”，也不是在运行时编译新的 C++。

更准确的说法是：

> C++ 实现稳定的能力积木，行为图或行为脚本描述积木如何组合，C++ 运行时解释或执行组合结果。

这条路线和 UE Blueprint 的关系最接近：

```text
UE C++ 原生函数 / 组件
    ↓
Blueprint 节点
    ↓
Blueprint 编译结果
    ↓
Blueprint VM
```

Wheatear 的对应版本是：

```text
Wheatear C++ 服务 / Effect / Query
    ↓
WAO 行为节点
    ↓
行为图编译结果
    ↓
Wheatear Behavior VM
```

---

## 2. 为什么不直接引入 Lua

Lua 当然可以解决“规则灵活”和“发行后修改”的问题，但它会带来一层新的运行时边界：

```text
Lua
    ↓
Lua/C++ 桥接层
    ↓
C++ 玩法服务和 ECS
```

这层边界本身不一定不可接受，但会带来额外工程成本：

- C++ 类型需要映射成 Lua 类型。
- 实体、组件和资源的生命周期需要管理。
- 字符串查找、表创建、栈操作和桥接调用会增加开销。
- 调试需要同时理解 C++ 调用栈和 Lua 调用栈。
- 热更新脚本可能持有旧对象、旧函数或旧数据引用。
- 脚本可以绕过公共结算入口，导致战斗事实无法统一记录。

因此，Wheatear 不需要追求“所有逻辑都能写成任意脚本”。更适合的边界是：

```text
高频、底层、性能敏感逻辑：C++
可组合、低频、规则编排逻辑：行为图 / 行为字节码
长期可调的参数和动作内容：YAML / 数据资产
跨场景的低频流程：WTS
底层事件通信：EventBus
```

行为 VM 仍然会调用 C++ 原生能力，因此它不是零开销。但只要它主要用于动作流程、技能规则、条件分支和事件编排，就不应该进入碰撞扫描、物理积分、渲染批处理等每帧热点。

---

## 3. 当前源码基础

### 3.1 当前已经具备的 WAO 骨架

当前 WAO 已经形成了以下链路：

```text
ActionIntent
    ↓
ActionRecipe
    ↓
ActionResolverRegistry / ActionOrchestrator
    ↓
ActionRunner
    ↓
EffectBundle / EffectLedger
    ↓
ActionSignalRouter
```

当前源码中的职责如下：

| 现有模块 | 当前职责 |
| --- | --- |
| `ActionIntent` | 描述谁想执行哪个动作、目标是谁、来源是什么 |
| `ActionRecipe` | 保存动作 id、动画、音效、时序、冷却、资源、标签、效果和信号 |
| `ActionAssetLoader` | 从 YAML 加载动作配方 |
| `ActionDatabase` | 保存运行时动作配方，并支持按 id 查询 |
| `ActionOrchestrator` | 查找配方并进入 Resolver 或 ActionRunner |
| `ActionResolverRegistry` | 按动作前缀把动作交给玩法专属解析器 |
| `ActionRunner` | 检查资源、状态和冷却，应用通用效果 |
| `EffectBundle` | 保存一次动作产生的效果集合 |
| `EffectLedger` | 记录效果是否应用、对象和结果 |
| `ActionSignalRouter` | 把表现信号分发给音效、特效、UI 或玩法系统 |
| `ActionDebugHistory` | 保存动作调试记录 |

相关源码：

- `Wheatear/src/Wheatear/Gameplay/Action/ActionTypes.h`
- `Wheatear/src/Wheatear/Gameplay/Action/ActionAssetLoader.cpp`
- `Wheatear/src/Wheatear/Gameplay/Action/ActionDatabase.cpp`
- `Wheatear/src/Wheatear/Gameplay/Action/ActionResolver.cpp`
- `Wheatear/src/Wheatear/Gameplay/Action/ActionRunner.cpp`

### 3.2 当前 WTS 和 EventBus 的边界

`EventBus`、`CommandBus`、`WTS` 和 WAO 不是同一种机制。

```text
EventBus
    负责 C++ 层事件分发和跨系统通信

CommandBus
    负责把短命令路由到具体系统

WTS
    负责低频流程：按钮、剧情、存档、读档、切场景、简单条件

WAO
    负责动作语义：动作意图、配方、资源、效果、结算和表现

行为图 / Behavior VM
    未来负责复杂的可组合规则和可挂起动作流程
```

当前 `.wts` 已经支持：

- `event`
- `wait`
- `if / endif`
- `scene:`
- `newgame:`
- `loadgame:`
- `progression:`
- `ui:`
- `event:`
- 经过 `CommandBus` 注册的玩法命令

它适合“什么时候做什么”，不适合每帧攻击状态、命中窗口、AI 或复杂技能分支。

当前 WTS 的文件变化重载已经在 `EventScriptSystem::LoadScript()` 中实现：缓存记录脚本文件的写入时间，发现变化后重新解析。这个机制可以作为未来行为资产热重载的参考，但不能直接解决发行包覆盖、字节码兼容和运行中行为实例迁移问题。

### 3.3 当前正式包的边界

当前打包运行时使用：

```text
content.wtpack
    ↓
运行时解压到 .wheatear_cache
    ↓
AssetPath::SetProjectRoot()
    ↓
按 assets/... 读取运行时资源
```

当前包格式版本为 `1`，运行时会读取一个 `content.wtpack`，解压到缓存目录，并使用包文件指纹避免重复解压。

当前还没有完整的：

- 多个补丁包叠加；
- 补丁版本清单；
- 补丁签名校验；
- 补丁回滚；
- 可更新资源白名单；
- 行为图字节码包；
- 运行中行为实例的版本策略。

因此，“当前 WTS/WAO 可以在开发目录热重载”不等于“正式打包游戏已经支持在线热更新”。后续实现需要在现有 `content.wtpack` 基础上增加覆盖层。

---

## 4. 目标架构：C++ 能力积木 + 行为组合层

### 4.1 三层模型

建议把未来系统分成三层：

```text
第一层：C++ Native Capability
    提供稳定、可测试、可复用的原生能力

第二层：Behavior Definition
    用 YAML、行为图或行为字节码组合能力

第三层：Gameplay Module
    提供玩法专属规则、实体查询和最终状态提交
```

#### 第一层：原生能力

原生能力是可以被多个行为复用的 C++ 操作，例如：

- 创建动作意图；
- 启动攻击；
- 打开格挡窗口；
- 创建 Hitbox；
- 查询当前状态；
- 查询资源和冷却；
- 查询目标；
- 判断是否命中；
- 判断是否处于某个时间窗口；
- 取消伤害；
- 施加伤害；
- 施加治疗；
- 增加或移除状态；
- 击退、浮空、硬直；
- 设置保护值；
- 触发反击动作；
- 消耗资源；
- 启动冷却；
- 发送表现信号；
- 写入 `EffectLedger`；
- 请求场景事件或低频流程。

这些能力应该由 C++ 直接实现，不应该让行为图直接访问任意 ECS 组件内存。

#### 第二层：行为组合

行为层负责组合原生能力：

- 顺序执行；
- 条件判断；
- 分支；
- 等待时间；
- 等待事件；
- 进入和退出阶段；
- 读写局部变量；
- 调用另一个动作；
- 生成效果；
- 触发信号；
- 失败回退；
- 超时处理；
- 行为实例结束或取消。

行为层不负责实现碰撞算法和物理规则，而是调用 C++ 提供的能力。

#### 第三层：玩法模块

玩法模块保留真正属于玩法的规则：

- 横板战斗的纵深、浮空、保护条和命中框；
- 回合制的行动顺序和目标规则；
- 战棋的格子、路径和占位；
- 弹幕玩法的投射物、遮挡和弹道；
- 玩法独有的 AI、胜负条件和结算。

WAO 行为层可以调用这些玩法服务，但不应该强行把所有玩法抽成同一套数据结构。

### 4.2 目标运行链

```text
UI / 输入 / AI / WTS / 动画事件
                ↓
          ActionIntent
                ↓
          ActionRecipe
                ↓
        Behavior Definition
       （行为图或行为字节码）
                ↓
          Behavior VM
                ↓
      Native Capability Registry
                ↓
       ActionContext / Transaction
                ↓
       EffectBundle / EffectLedger
                ↓
   Gameplay Service / Signal Router
```

这里的关键是：行为图不是直接修改组件，而是通过受控的 C++ 原生能力产生结构化结果。

---

## 5. 原生能力注册表

### 5.1 为什么需要注册表

如果行为图直接保存 C++ 函数指针、类名或字符串函数名，会产生几个问题：

- 版本更新后函数指针失效；
- 行为资产和具体 C++ ABI 绑定；
- 发行补丁无法保证兼容；
- 编辑器无法知道节点需要哪些输入和输出；
- 很难做行为图校验和错误提示。

因此建议增加一个稳定的能力注册表：

```text
Primitive ID
    ↓
能力元数据
    ↓
校验函数
    ↓
执行函数
```

能力的持久化身份应该是稳定 id 或稳定 opcode，而不是 C++ 函数名。

示例：

```text
combat.open_guard_window
combat.check_guard_window
combat.create_hitbox
combat.resolve_hit
combat.cancel_damage
combat.apply_hitstun
combat.apply_knockback
combat.start_action
combat.emit_signal
```

### 5.2 一个能力节点应具备什么信息

每个注册能力至少需要：

| 信息 | 用途 |
| --- | --- |
| 稳定 id | 行为资产引用，不随函数改名变化 |
| 版本号 | 判断行为资产能否在当前运行时执行 |
| 输入定义 | 编辑器显示端口，运行时校验参数 |
| 输出定义 | 后续节点使用结果 |
| 执行类别 | 查询、效果、控制流、等待或表现 |
| 线程约束 | 是否只能在主线程或玩法线程执行 |
| 是否允许热更新 | 对应能力发生变化时的兼容策略 |
| 调试名称 | 行为图和运行记录中显示 |
| 执行回调 | 由 C++ 实现的实际逻辑 |

建议将节点分成四类：

```text
Query
    只读取上下文，不改变战斗事实

Effect
    生成或提交结构化效果

Flow
    控制顺序、分支、等待和行为生命周期

Presentation
    发送音效、特效、UI 和摄像机信号
```

### 5.3 不允许能力节点随意改 ECS

能力节点不应该暴露类似下面的接口：

```text
WriteComponent(entity, arbitraryMemoryOffset, value)
```

这会让行为图绕过玩法规则、账本和调试系统。

更稳妥的接口是：

```text
ApplyDamage(target, value)
SetCombatState(target, Broken, duration)
ConsumeResource(actor, "magic_sword", value)
EmitSignal("side.break_limit.flash")
```

这些能力内部可以修改组件，但必须经过统一的验证和记录入口。

---

## 6. 行为图和行为字节码

### 6.1 编辑器中的行为图

编辑器中可以用节点表示：

```text
事件节点
    OnActionIntent
    OnHitReceived
    OnGuardStart
    OnTimer

条件节点
    CheckState
    CheckResource
    CheckCooldown
    CheckWindow
    CheckTarget

动作节点
    StartAttack
    OpenGuardWindow
    CreateHitbox
    ApplyEffect
    StartAction
    EmitSignal

流程节点
    Sequence
    Branch
    Wait
    WaitForEvent
    Cancel
    End
```

节点图应该是可读的，但运行时不建议每帧直接遍历编辑器节点对象。

### 6.2 编译为字节码

行为图保存后，编辑器应当执行：

```text
行为图
    ↓
节点连接校验
    ↓
输入输出类型校验
    ↓
能力 id 校验
    ↓
控制流整理
    ↓
行为字节码
```

字节码可以使用简单的指令形式：

```text
CHECK_STATE
CHECK_WINDOW
OPEN_GUARD_WINDOW
WAIT_EVENT
BRANCH_IF_FALSE
APPLY_EFFECT
START_ACTION
EMIT_SIGNAL
END
```

每条指令应尽量使用：

- 整数 opcode；
- 整数常量索引；
- 局部变量索引；
- 稳定能力 id；
- 预解析的资源引用；
- 调试信息索引。

不要在热路径中反复用字符串查找能力和组件。

### 6.3 行为实例必须有生命周期

行为图要支持挂起和恢复，不能只支持“一次调用立即结束”。

行为实例至少需要保存：

```text
BehaviorInstance
    BehaviorId
    BehaviorRevision
    ProgramCounter
    OwnerEntity
    TargetEntity
    LocalValues
    WaitingEvent
    WaitRemaining
    Cancelled
    Completed
```

这样才能表达：

```text
进入格挡
    ↓
等待 0.2 秒
    ↓
等待命中事件
    ↓
判断当前是否仍在弹反窗口
    ↓
执行反击
```

行为实例不能依赖旧的 C++ 函数指针。它只能保存稳定的行为 id、版本和数据。

### 6.4 一个 `.wtb` 可以包含多个行为入口

`.wtb` 不要求“一个动作对应一个文件”。更合理的定位是：`.wtb` 是一个可独立加载、编译和热更新的行为模块，模块内部可以包含多个命名行为入口。

例如横板战斗可以组织成：

```text
side_combat.wtb
├── side.basic_combo
├── side.guard
├── side.parry
├── side.break_limit
├── side.hit_reaction
└── common.open_guard_window
```

`ActionRecipe` 引用具体入口，而不是只引用文件：

```yaml
behaviorFile: assets/gameplay/behaviors/side_combat.wtb
behaviorEntry: side.break_limit
behaviorVersion: 3
```

运行时流程为：

```text
ActionRecipe
    ↓
加载 side_combat.wtb
    ↓
选择 side.break_limit 入口
    ↓
Behavior VM 从该入口开始执行
```

这和一个 Lua 文件定义多个函数的组织方式类似：

```text
Lua 文件：
    parry()
    break_limit()
    hit_reaction()

C++：
    调用 parry() 或 break_limit()
```

行为图模块也可以包含可复用的公共子图：

```text
common.open_guard_window
common.consume_resource
common.emit_action_signal
common.apply_hitstun
```

`side.guard`、`side.parry` 和 `side.break_limit` 可以共同调用这些子图。

推荐按玩法或系统拆分行为模块，而不是把整个游戏做成一个巨大的文件：

```text
common_combat.wtb
side_combat.wtb
turn_combat.wtb
tactical_combat.wtb
vn_flow.wtb
```

这样修改 `side.break_limit` 时，可以更新 `side_combat.wtb` 中的对应入口；其他入口仍然属于同一个模块并一起经过版本校验。只有当某个行为非常复杂、需要独立维护或独立发布时，才考虑拆成单独的 `.wtb` 文件。

---

## 7. 用弹反攻击说明完整组合方式

### 7.1 C++ 预先提供的能力

假设要实现“格挡成功后反弹攻击并触发反击”，C++ 先提供：

```text
combat.start_guard
combat.open_guard_window
combat.close_guard_window
combat.on_hit_received
combat.check_guard_window
combat.cancel_incoming_damage
combat.apply_hitstun
combat.apply_knockback
combat.start_action
combat.emit_signal
```

其中：

- Hitbox 扫描和命中检测仍由 C++ 每帧完成；
- `on_hit_received` 由命中结算服务产生；
- 行为图只消费结构化的命中事件；
- 最终伤害、状态和击退仍通过 C++ 受控能力提交。

### 7.2 弹反行为图

```text
OnActionIntent: side.guard
        ↓
CheckCanActivate
        ↓
StartGuard
        ↓
OpenGuardWindow(0.20s)
        ↓
WaitForEvent: combat.hit_received
        ↓
CheckGuardWindow
        ↓
        ├── 否 → ApplyGuardResult / 普通受击
        │
        └── 是
              ↓
        CancelIncomingDamage
              ↓
        ApplyHitstun(attacker)
              ↓
        ApplyKnockback(attacker)
              ↓
        StartAction(side.counter)
              ↓
        EmitSignal(side.parry.flash)
```

### 7.3 C++ 和行为图的边界

行为图负责：

- 什么时候进入格挡；
- 弹反窗口持续多久；
- 命中后检查哪个条件；
- 成功后执行哪些后续能力；
- 失败后走什么分支；
- 触发哪一个反击动作。

C++ 负责：

- 每帧更新行为实例；
- 查询实体和碰撞结果；
- 维护格挡窗口状态；
- 计算伤害；
- 修改生命、硬直、击退和状态；
- 生成 `EffectBundle`；
- 写入 `EffectLedger`；
- 发送表现信号。

如果 C++ 没有提供 `cancel_incoming_damage` 或 `apply_knockback`，行为图不能凭空创造这些能力。此时需要先扩展原生能力层。

这条边界保证了“新增规则可以只改行为图”，同时也承认“新增底层能力仍然需要改 C++”。

---

## 8. 和现有 WAO 的关系

### 8.1 `ActionRecipe` 继续保留

`ActionRecipe` 不应被行为图替代，而应承担动作资产和默认参数的职责：

```text
ActionRecipe
    动作身份
    动画和音效
    冷却和时序
    资源消耗
    标签和默认条件
    默认效果
    表现信号
    BehaviorId
```

后续可以为 `ActionRecipe` 增加：

```yaml
behaviorFile: assets/gameplay/behaviors/side_combat.wtb
behaviorEntry: side.break_limit
behaviorVersion: 3
```

也可以只在配方中保存稳定行为 id，由行为数据库维护文件到入口的映射：

```yaml
behaviorId: side.break_limit
```

`ActionRecipe` 负责“这个动作是什么以及使用哪个行为入口”，行为图负责“这个动作在运行时怎么组织”。

### 8.2 `ActionIntent` 作为行为入口

玩家输入、AI、WTS、编辑器沙盒和动画事件都可以产生 `ActionIntent`：

```text
UI / Input / AI / WTS / Editor
             ↓
        ActionIntent
             ↓
        ActionRecipe
             ↓
        Behavior VM
```

这样不需要让每一种来源单独调用一套技能逻辑。

### 8.3 `EffectBundle` 和 `EffectLedger` 继续作为事实边界

行为图可以提出效果，但不能直接把所有效果偷偷写入组件。

推荐使用：

```text
行为节点
    ↓
EffectBundle
    ↓
C++ EffectExecutor
    ↓
EffectLedger
    ↓
组件状态和表现信号
```

其中：

- `EffectBundle` 表示一次行为产生的效果集合；
- `EffectExecutor` 验证并应用效果；
- `EffectLedger` 记录成功、失败、来源、目标和数值；
- `ActionSignalRouter` 负责表现，不负责偷偷改变核心战斗事实。

这会让弹反、断限、普通攻击、治疗和状态效果都能被统一调试。

### 8.4 玩法 Resolver 的位置

`ActionResolverRegistry` 仍然保留，但职责逐渐从“直接写整段技能逻辑”转向：

- 为某个玩法注册原生能力；
- 为某个动作类型提供上下文；
- 将玩法专属命中结果转换成结构化事件；
- 对行为图请求做规则校验；
- 在行为图无法覆盖的复杂算法处调用 C++ 服务。

横板、回合制、战棋和弹幕仍然可以有各自的 Resolver。

---

## 9. WTS、EventBus 和行为图如何协作

### 9.1 EventBus

`EventBus` 继续作为 C++ 事件总线：

```text
窗口、输入、系统和模块
        ↓
EventBus
        ↓
C++ 事件处理器
```

它不应该被改造成通用的脚本运行时。

未来可以增加一类面向行为 VM 的结构化玩法事件，例如：

```text
ActionStarted
HitReceived
GuardWindowOpened
GuardWindowClosed
StateChanged
EffectApplied
```

行为 VM 消费这些事件时，建议使用专门的 `BehaviorEventQueue` 或经过过滤的事件入口，而不是让每个行为实例订阅全部 `EventBus` 事件。

### 9.2 WTS

WTS 继续负责低频流程：

```text
按钮 / VN / 战斗结算
        ↓
event:name
        ↓
WTS
        ↓
scene / save / progression / action
```

后续可以增加一个明确的动作命令，例如：

```text
action:side.break_limit
```

它只负责创建或提交 `ActionIntent`，不负责写技能内部逻辑。

WTS 不需要承担：

- 每帧行为；
- 等待命中窗口；
- 复杂 AI；
- 物理和碰撞；
- 多实体战斗状态机。

### 9.3 三者最终分工

```text
EventBus
    底层 C++ 事件通信

WTS
    低频流程编排

WAO Recipe
    动作资产和默认参数

Behavior Graph / VM
    可组合的动作规则和可挂起行为

Gameplay C++
    高频模拟、玩法规则和最终状态提交
```

---

## 10. 打包后热更新方案

### 10.1 更新目标

发行后优先允许更新：

- WAO 动作配方；
- 行为图；
- 行为字节码；
- WTS 流程；
- VN 剧本；
- 战斗调参；
- 成长、装备、掉落等数据；
- 动画、音效、特效和 UI 资源；
- 行为图使用的非原生资源。

发行后暂不允许通过内容补丁更新：

- 引擎 exe；
- C++ 核心 DLL；
- C++ 玩法模块；
- 行为节点的原生实现；
- 新的物理算法；
- 新的渲染后端；
- 依赖 ABI 的原生对象布局。

如果新增一个行为节点本身，需要更新 C++，那仍然必须发布新的程序版本。

### 10.2 基础包和覆盖包

建议把当前单包结构扩展为：

```text
base/
    WheatearSandbox.exe
    content.wtpack

updates/
    manifest.json
    patch_0001.wtpatch
    patch_0002.wtpatch
```

补丁包使用和基础包一致的相对路径：

```text
assets/gameplay/actions/10_side_combat_actions.yaml
assets/gameplay/behaviors/side_combat.wtb
assets/events/sandbox_flow.wts
```

加载顺序：

```text
基础包
    ↓
patch_0001
    ↓
patch_0002
    ↓
最新有效版本
```

后加载的补丁覆盖同路径的旧文件。对于删除操作，清单中需要有明确的删除记录，不能只依赖文件是否存在。

### 10.3 更新清单

更新清单至少包含：

```json
{
  "product": "WheatearDemo",
  "baseVersion": "1.0.0",
  "contentVersion": "1.0.3",
  "minimumRuntimeVersion": "0.4.0",
  "patches": [
    {
      "id": "patch_0003",
      "path": "patch_0003.wtpatch",
      "sha256": "...",
      "size": 123456,
      "dependsOn": ["patch_0002"],
      "behaviorSchema": 2
    }
  ]
}
```

正式实现时还应增加签名字段。哈希用于完整性校验，签名用于确认补丁确实由发行方生成。

### 10.4 客户端更新流程

```text
启动或进入大厅
    ↓
读取本地 content manifest
    ↓
请求服务器最新 manifest
    ↓
检查产品、基础版本、运行时版本和依赖
    ↓
下载缺失补丁到临时目录
    ↓
校验大小、哈希和签名
    ↓
原子移动到 updates/active
    ↓
重建资源覆盖表
    ↓
按需要重新加载数据和行为程序
    ↓
记录当前活动版本
```

下载失败时不能破坏当前可运行版本。推荐使用：

```text
updates/staging/
updates/active/
updates/backup/
```

只有完整校验成功后，才把 `staging` 变成 `active`。

### 10.5 对现有 `content.wtpack` 的改造

当前运行时只读取一个包并解压到缓存。后续可以按下面的方式改造：

1. 保留 `content.wtpack` 作为基础包。
2. 增加更新清单扫描和补丁包列表。
3. 计算“基础包 + 所有有效补丁”的联合指纹。
4. 创建新的临时缓存目录。
5. 先解压基础包，再按顺序解压补丁包。
6. 对资源路径执行白名单检查。
7. 写入联合版本标记。
8. 原子替换当前运行时资源根目录。
9. 只清理不再使用的旧缓存。

注意：当前 `ExtractAssetPack()` 会先清空缓存根目录，再解压单个包。支持覆盖层后不能简单地对每个补丁重复调用“清空并解压”，否则前面包的内容会被删除。需要引入：

```text
ExtractBasePack()
ApplyPatchPack()
CommitRuntimeRoot()
```

而不是复用一个会清空目标目录的函数。

---

## 11. 热更新资源的重新加载策略

### 11.1 数据资源

数据资源可以按注册表粒度重新加载：

```text
ActionAssetLoader::ReloadManifest()
    ↓
ActionDatabase::Clear()
    ↓
重新加载动作配方
    ↓
ActionDatabase::Revision() 增加
    ↓
玩法模块刷新缓存
```

当前代码已经有 `ActionAssetLoader::ReloadManifest()` 和 `ActionDatabase::Revision()`，这为行为资产热更新提供了基础。

### 11.2 行为字节码

行为字节码可以使用：

```text
BehaviorDatabase
BehaviorProgram
BehaviorRevision
BehaviorInstance
```

重载时：

1. 读取新行为文件。
2. 校验字节码版本和能力依赖。
3. 编译或加载新 `BehaviorProgram`。
4. 原子替换 `BehaviorDatabase` 中的程序。
5. 新启动的行为实例使用新版本。

### 11.3 运行中的行为实例

第一版不要尝试把正在运行的行为实例强行迁移到新程序。推荐规则：

```text
已运行实例：继续使用启动时绑定的 BehaviorRevision
新启动实例：使用最新 BehaviorRevision
```

这样可以避免：

- 程序计数器指向不存在的节点；
- 局部变量布局发生变化；
- 等待事件名称变化；
- 旧状态和新行为语义不一致；
- 正在播放的动作突然跳转。

后续确实需要迁移时，再增加：

```text
行为迁移版本
迁移函数
局部变量兼容规则
安全迁移点
```

安全迁移点通常是动作完成、场景切换、回合结束或行为处于等待状态时。

### 11.4 资源对象的更新

动画、音效、纹理和特效资源如果已经被加载到内存，覆盖文件后不一定自动替换现有对象。

因此需要分级：

```text
配置类资源：可以立即重新解析
动作/行为程序：新实例生效
纹理/音效：新请求使用新资源，旧对象按引用计数释放
当前播放动画：通常播放完后再切换
场景对象：需要显式重建或重新加载场景
```

---

## 12. 版本兼容、安全和回滚

### 12.1 版本约束

行为资产不能只记录一个文件名，还应记录：

- 行为 schema 版本；
- 依赖的能力 id；
- 能力最低版本；
- 所需数据字段；
- 目标玩法模块；
- 是否允许在发行版执行。

加载流程：

```text
读取行为资产
    ↓
检查 schema
    ↓
检查 opcode / primitive
    ↓
检查参数类型
    ↓
检查依赖资源
    ↓
通过后放入 BehaviorDatabase
```

失败时应该拒绝加载该行为并报告清晰错误，而不是执行半个图。

### 12.2 安全边界

行为层只能访问注册能力：

- 不加载任意 DLL；
- 不执行任意机器码；
- 不直接访问文件系统；
- 不直接启动进程；
- 不直接使用任意 C++ 指针；
- 不绕过资源和实体权限；
- 不让补丁替换白名单之外的路径。

这也是“行为图/字节码”比“运行时编译 C++”更适合作为发行后热更新方案的原因。

### 12.3 回滚

每个补丁都需要可回滚：

```text
active_version
previous_version
patch_dependency_graph
```

启动时如果发现：

- 解压失败；
- 行为校验失败；
- 关键资源缺失；
- 运行时崩溃计数超过阈值；

则恢复到上一个已知可用版本。

不要直接覆盖基础包，也不要直接删除旧补丁。采用版本目录和活动指针更容易回滚。

---

## 13. 性能策略

### 13.1 不让行为 VM 进入高频热点

适合行为 VM 的逻辑：

- 技能条件；
- 动作阶段；
- 格挡窗口；
- 命中后的分支；
- 状态和效果组合；
- 低频 AI 决策；
- 剧情和任务流程；
- 玩法之间的规则编排。

继续放在 C++ 的逻辑：

- 每帧物理；
- 大量实体碰撞；
- Hitbox 扫描；
- 投射物批量更新；
- 渲染批处理；
- 大量数值循环；
- 资源解码；
- 并行计算热点。

### 13.2 减少行为调用开销

行为 VM 的执行应尽量：

- 编译成整数 opcode；
- 预绑定能力 id；
- 预解析资源引用；
- 使用连续指令数组；
- 避免每一步创建字符串；
- 避免每个节点分配临时容器；
- 将多个效果批量放入 `EffectBundle`；
- 把查询结果保存到局部变量；
- 对不需要等待的连续指令批量执行；
- 对纯数据动作使用 `ActionRunner` 的原生路径。

如果未来发现某条行为路径过慢，可以把它升级为一个新的 C++ 原生能力，而不是不断堆 VM 优化。

---

## 14. 编辑器升级路线

### 14.1 第一阶段：补强现有 WAO 编辑器

先把现有 `WAOActionEditorPanel` 做成稳定的数据编辑器：

- 配方 schema 校验；
- 效果类型校验；
- 资源引用校验；
- 动作 id 冲突检查；
- 配方版本和依赖信息；
- 热重载状态；
- 运行记录和失败原因；
- 生成行为图入口。

### 14.2 第二阶段：行为图编辑器

建议新增独立的行为图编辑器，而不是把所有节点塞进 `Event Script Editor`。

原因：

- WTS 是低频流程；
- 行为图有动作上下文、实体、局部变量和可挂起实例；
- 两者的节点语义和调试方式不同；
- 分开可以避免 WTS 逐渐变成一个没有边界的通用脚本语言。

建议新增资源扩展名：

```text
.wtb
```

含义可以是 Wheatear Behavior Graph。

编辑器保存：

```text
side_combat.wtb
    ├── side.guard
    ├── side.parry
    └── side.break_limit
```

运行时加载：

```text
side_combat.wtb
    ↓
校验 / 编译
    ↓
内存中的 BehaviorProgram
    ↓
Behavior VM
```

`.wtb` 文件内的每个命名入口都会被编译成可定位的行为程序。运行时不是执行整个文件，而是根据 `behaviorEntry` 或 `behaviorId` 从指定入口开始执行。

当前实现编译到内存中的 `BehaviorProgram`，**尚未生成 `.wtb.bin` 磁盘字节码**；`.wtb.bin` 属于后续可选优化，不要按已实现能力引用。

### 14.3 第三阶段：行为调试器

行为调试器需要显示：

- 当前行为 id 和 revision；
- 当前节点或 program counter；
- owner 和 target；
- 局部变量；
- 等待的事件；
- 等待剩余时间；
- 最近执行的能力；
- 产生的 `EffectBundle`；
- `EffectLedger` 结果；
- 失败原因；
- 进入和退出时间。

这会把“为什么这个技能没有触发”从盲猜变成可观察问题。

---

## 15. 建议新增的源码模块

以下是建议名称，不代表当前已经存在：

```text
Wheatear/src/Wheatear/Gameplay/Behavior/
    BehaviorTypes.h
    BehaviorProgram.h
    BehaviorDatabase.h
    BehaviorAssetLoader.h
    BehaviorCompiler.h
    BehaviorVM.h
    BehaviorInstance.h
    BehaviorPrimitiveRegistry.h
    BehaviorEventQueue.h
    BehaviorHotReload.h

Wheatear/src/Wheatear/Runtime/
    RuntimePatchManager.h
    RuntimePatchManager.cpp
    ContentMountTable.h
    ContentMountTable.cpp

WheatearEditor/src/Panels/
    BehaviorGraphPanel.h
    BehaviorGraphPanel.cpp
    BehaviorGraphCompilerPanel.cpp
```

建议职责：

| 模块 | 职责 |
| --- | --- |
| `BehaviorTypes` | 节点、端口、变量、事件和程序数据结构 |
| `BehaviorProgram` | 编译后的指令、常量和调试信息 |
| `BehaviorDatabase` | 行为程序按 id 和 revision 管理 |
| `BehaviorAssetLoader` | 加载 `.wtb` 或字节码文件 |
| `BehaviorCompiler` | 图结构校验和字节码编译 |
| `BehaviorVM` | 执行指令、推进实例和处理等待 |
| `BehaviorInstance` | 保存运行中的行为状态 |
| `BehaviorPrimitiveRegistry` | 注册和查找原生能力 |
| `BehaviorEventQueue` | 向行为实例投递筛选后的玩法事件 |
| `BehaviorHotReload` | 重新加载行为程序并管理 revision |
| `RuntimePatchManager` | 下载、校验、挂载和回滚补丁 |
| `ContentMountTable` | 维护基础包和补丁的资源覆盖顺序 |

---

## 16. 当前已落实的第一轮升级

截至 2026-09-23，下面这些部分已经进入源码并通过 Debug x64 编译和 Sandbox 启动回归：

### 15.1 战斗命中前置决策

- `SideCombatGuardService` 已提供格挡窗口、弹反窗口、伤害倍率和一次命中消费。
- 命中结算会先处理格挡/弹反，再决定伤害、硬直、击退、连击和事件。
- `combat.hit_received` 与 `combat.guard_resolved` 携带 `blocked`、`parried` 等结构化结果。
- `combat.open_guard_window`、`combat.close_guard_window` 和 `combat.is_guard_window_open` 已注册为行为能力。

### 15.2 Action 生命周期

- 增加 `ActionRuntimeStore`，按场景和实体持有冷却、资源、状态和属性运行时。
- `BeginAction` 与 `ExecuteBody` 分离，行为启动时只激活一次，行为内部执行配方不会重复扣资源或启动冷却。
- 行为实例绑定共享 Runtime，挂起、等待事件和等待子行为期间不会丢失状态。
- SideCombat 的普通动作在有场景和实体上下文时也进入统一 ActionRunner；预览 Resolver 只用于没有 Runtime 的编辑器或无实体场景。

### 15.3 Primitive 与行为资产校验

- Primitive 元数据包含输入、输出、类型、版本和线程约束。
- 编译阶段拒绝未知端口、缺少必需输入、字面量类型不匹配、未知输出和不兼容 schema。
- 执行阶段再次检查输入和输出，防止运行时能力实现写出未声明数据。
- 行为数据库和 Action 数据库采用“全部编译/解析成功后一次替换”的方式，失败时保留旧版本。

### 15.4 行为图编辑器

- `.wtb` 节点支持持久化 `position`。
- 编辑器支持输出 Pin 到输入 Pin 的连线、拖动节点、缩放、平移。
- 节点属性、连线、布局和操作类型支持撤销/重做，包含 `Ctrl+Z`、`Ctrl+Y`。
- `BehaviorVM::Active` 提供当前节点、等待事件、等待时间、最后 Primitive 和失败原因，编辑器可以查看并取消运行中的行为实例。

### 15.5 发行包覆盖层

- 补丁清单支持 `id`、`path`、`sha256`、`size`、`dependsOn`、`behaviorSchema` 和删除路径。
- 补丁路径、包内路径和删除路径都执行 `assets/...` 白名单检查。
- 依赖缺失、循环依赖、大小或 SHA-256 不匹配时拒绝整份活动清单。
- 基础包、补丁包和删除标记先落到 staging，再以目录 rename 提交到运行时缓存；失败不会留下半成品缓存。
- 清单支持 `signatureRequired`、`signatureAlgorithm`、`publicKey`/`publicKeyFile` 和 `signature`。
- 签名算法固定为 `ECDSA-P256-SHA256`；签名载荷使用 `WHEATEAR_PATCH_MANIFEST_V1` 规范化格式，覆盖产品、版本、补丁顺序、哈希、依赖、行为 schema 和删除记录。
- `updates/staging/<version>` 会先完整校验，再通过 `ActivateStagedPatchSet()` 切换到 `updates/active`；旧活动版本保留在 `updates/backup`，`active_version` 和 `previous_version` 记录当前/上一个版本。
- 激活过程中写入 `activation.pending`，启动时会修复被进程中断的目录交换；`launch_state.txt` 连续两次未达到健康场景后，启动流程会自动调用回滚。
- 未声明签名的清单仍允许开发环境使用；声明签名或 `signatureRequired: true` 后，公钥缺失、格式错误或验签失败都会拒绝整份清单。

仍未完成的发行层工作主要是服务器下载器、断点续传，以及把产品/基础版本/最低运行时版本接入正式的版本策略。当前本地 staging、签名验签、依赖排序、原子激活、删除、跨版本备份回滚和崩溃后自动回退已经具备实现路径。

---

## 17. 分阶段实施顺序

### 阶段 0：整理现有边界

- 明确 WTS、CommandBus、WAO、玩法服务的职责。
- 清理直接从 UI 修改战斗事实的路径。
- 保证动作输入都能形成 `ActionIntent`。
- 保证效果进入 `EffectBundle` 或 `EffectLedger`。
- 将现有特殊技能逻辑记录成原生能力候选。

### 阶段 1：原生能力注册

- 建立 `BehaviorPrimitiveRegistry`。
- 把已有 `ActionRunner`、Effect、State、Signal 能力注册成稳定 id。
- 为能力增加输入输出和版本信息。
- 为横板补充格挡窗口、命中事件、取消伤害、击退和状态转换能力。

### 阶段 2：最小行为运行时

- 先不做复杂图编辑器。
- 支持顺序、条件、等待和调用动作。
- 使用文本或简单 YAML 描述行为。
- 先验证 `side.break_limit` 或弹反流程。
- 行为实例支持等待和取消。

### 阶段 3：行为图和编译器

- 实现 `.wtb` 行为图资产。
- 节点端口校验。
- 编译成稳定 opcode。
- 写出调试信息。
- 编辑器可以从图生成运行程序。

### 阶段 4：运行时热重载

- 开发目录检测文件变化。
- 重新加载行为程序。
- 新行为实例使用新 revision。
- 旧实例继续使用旧 revision。
- 增加失败回退和错误提示。

### 阶段 5：发行包覆盖层

- 扩展 `content.wtpack` 为基础包 + 补丁包。
- 增加清单、版本、依赖、哈希和签名。
- 增加 `RuntimePatchManager`。
- 增加临时目录、原子激活和回滚。
- 只允许更新白名单路径。

### 阶段 6：生产验证

- 修改技能条件并发布补丁。
- 修改弹反窗口并发布补丁。
- 修改断限后的反击流程并发布补丁。
- 验证正在运行的动作不会被破坏。
- 验证补丁失败可以回滚。
- 验证旧客户端拒绝不兼容行为版本。

---

## 18. 典型更新例子

### 17.1 只改数值

修改：

```yaml
cooldown: 3.00
hitTime: 0.04
```

只需要更新动作配方，客户端重新加载 `ActionDatabase`。

### 17.2 修改弹反窗口

修改行为图：

```text
OpenGuardWindow(0.20)
```

变为：

```text
OpenGuardWindow(0.28)
```

不需要修改 C++，行为编译器生成新程序，发行包只更新行为文件。

### 17.3 修改弹反成功后的规则

旧图：

```text
取消伤害
击退攻击者
```

新图：

```text
取消伤害
击退攻击者
施加短暂硬直
启动反击动作
触发镜头特效
```

只要这些能力已经注册在 C++ 中，就可以通过行为图更新。

### 17.4 增加新的底层能力

如果要增加：

```text
TimeRewind
特殊物理约束
新的渲染后处理
```

而当前 C++ 没有这些原生能力，就必须发布新的程序版本。行为热更新不能创造不存在的 C++ 能力。

---

## 19. 最终定位

这套方案可以这样概括：

```text
Wheatear
    = 原生 C++ 引擎能力
    + WAO 动作数据
    + 行为图 / 字节码组合
    + C++ Behavior VM
    + 基础包 / 补丁包覆盖
```

它和 Unity + Lua 的区别：

- 不需要 Lua/C# 桥接层；
- 不执行任意脚本源码；
- 行为只能调用注册过的能力；
- 高性能逻辑仍在 C++；
- 热更新主要更新数据、行为图和字节码。

它和 UE Blueprint 的相似点：

- C++ 提供底层节点；
- 可视化图组合节点；
- 图可以编译成运行时程序；
- 发行后可以通过内容补丁更新编译结果。

它和 UE Blueprint 的不同点：

- Wheatear 不需要照搬 UObject、反射和 Blueprint 类体系；
- 当前目标是单机 2D、多玩法和可调试动作系统；
- `EffectLedger` 继续作为战斗事实边界；
- WTS、WAO 和 Behavior VM 保持不同职责；
- 补丁系统需要结合现有 `content.wtpack` 自行实现。

最终建议坚持下面这句话：

> **C++ 写能力，WAO 写动作资产，行为图写规则组合，C++ VM 负责执行，补丁包负责发行后更新。**

这比“把所有技能改成 Lua”更符合 Wheatear 当前的架构，也能保留引擎底层的原生性能和长期可维护性。
