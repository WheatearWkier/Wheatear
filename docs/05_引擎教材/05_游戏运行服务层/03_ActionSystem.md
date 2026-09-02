# 第 5 编 03：ActionSystem 与 WAO 动作编排

> 本章所属层级：**游戏运行服务层**
>
> 本章目标：理解 Wheatear 如何把“动作意图、动作配方、玩法规则、效果结算、信号表现、调试记录”串成一条可读、可查、可扩展的链路。
>
> 先提醒一句：旧版架构文档里常写成 `RuleResolver`、`SignalRouter`、`EffectBundle` 这一套概念；当前源码里更接近的名字是 `ActionResolverRegistry`、`ActionOrchestrator`、`ActionSignalRouter`、`EffectLedger`。本章以**现有源码命名**为准，但会保留“规则解析 / 信号路由 / 效果账本”这几个概念，方便你和面试官对话。
>
> 本章会反复使用三个例子：
> 1. 横板动作 `side.basic1` 为什么能从“按键”变成“可记录的一次攻击”。
> 2. 回合制 `turn.white_vow` 为什么能从同一套动作数据里读出治疗、状态和音效。
> 3. 弹幕动作 `arcade.gun` 为什么可以把“发射”当成一个动作，而不是散在各处的特例。

## 5.1 这一层在架构中的位置

如果把前两章的内容连起来看：

```text
CommandBus
    ↓
EventScript
    ↓
ActionSystem / WAO
    ↓
Gameplay modules
```

`CommandBus` 解决的是“这条命令该交给谁”。  
`EventScript` 解决的是“什么时候发什么命令”。  
`ActionSystem` 解决的是“一个动作从意图到结算，到底经历了什么”。

所以它不属于最底层的平台层，也不属于最上层的玩法层。  
它是**游戏运行服务层**里最像“中枢神经”的一块：既要读数据，也要组织执行，还要留下记录。

对一个简单 2D 游戏来说，这层的价值特别直接：

- 你不会把“按钮点击、消耗资源、播放音效、命中反馈、状态变化”全塞到一个脚本里。
- 你也不会为了一个普攻就发明一套过重的通用战斗引擎。
- 你会得到一条足够统一、又不至于太重的动作链。

## 5.2 本章要读的源码

建议按这个顺序看：

- `Wheatear/src/Wheatear/Gameplay/Action/ActionTypes.h`
- `Wheatear/src/Wheatear/Gameplay/Action/ActionTypes.cpp`
- `Wheatear/src/Wheatear/Gameplay/Action/ActionRecipeQueries.h`
- `Wheatear/src/Wheatear/Gameplay/Action/ActionRecipeQueries.cpp`
- `Wheatear/src/Wheatear/Gameplay/Action/ActionDatabase.h`
- `Wheatear/src/Wheatear/Gameplay/Action/ActionDatabase.cpp`
- `Wheatear/src/Wheatear/Gameplay/Action/ActionAssetLoader.h`
- `Wheatear/src/Wheatear/Gameplay/Action/ActionAssetLoader.cpp`
- `Wheatear/src/Wheatear/Gameplay/Action/ActionRunner.h`
- `Wheatear/src/Wheatear/Gameplay/Action/ActionRunner.cpp`
- `Wheatear/src/Wheatear/Gameplay/Action/ActionResolver.h`
- `Wheatear/src/Wheatear/Gameplay/Action/ActionResolver.cpp`
- `Wheatear/src/Wheatear/Gameplay/Action/ActionSignalRouter.h`
- `Wheatear/src/Wheatear/Gameplay/Action/ActionSignalRouter.cpp`
- `Wheatear/src/Wheatear/Gameplay/Action/EffectRegistry.h`
- `Wheatear/src/Wheatear/Gameplay/Action/EffectRegistry.cpp`
- `Wheatear/src/Wheatear/Gameplay/Action/EffectFormula.h`
- `Wheatear/src/Wheatear/Gameplay/Action/EffectFormula.cpp`
- `Wheatear/src/Wheatear/Gameplay/Action/StateRegistry.h`
- `Wheatear/src/Wheatear/Gameplay/Action/StateRegistry.cpp`
- `Wheatear/src/Wheatear/Gameplay/Action/ActionDebugHistory.h`
- `Wheatear/src/Wheatear/Gameplay/Action/ActionDebugHistory.cpp`
- `Wheatear/src/Wheatear/Modules/ModuleBootstrap.cpp`
- `Wheatear/src/Wheatear/Modules/SideCombat/SideCombatActionService.cpp`
- `Wheatear/src/Wheatear/Modules/SideCombat/SideCombatHitResolutionService.cpp`
- `Wheatear/src/Wheatear/Modules/TurnCombat/TurnCombatSkillService.cpp`
- `Wheatear/src/Wheatear/Modules/TurnCombat/TurnCombatActionService.cpp`
- `Wheatear/src/Wheatear/Modules/TacticalCombat/TacticalCombatSkillService.cpp`
- `Wheatear/src/Wheatear/Modules/TacticalCombat/TacticalCombatActionService.cpp`
- `Wheatear/src/Wheatear/Modules/ArcadeCombat/ArcadeCombatActionResolver.cpp`
- `WheatearEditor/src/Panels/WAOActionEditorPanel.cpp`
- `WheatearEditor/src/Panels/WAOActionEditorPanel_Sandbox.cpp`
- `Projects/WheatearDemo/assets/gameplay/actions/action_sets.yaml`
- `Projects/WheatearDemo/assets/gameplay/actions/00_arcade_actions.yaml`
- `Projects/WheatearDemo/assets/gameplay/actions/10_side_combat_actions.yaml`
- `Projects/WheatearDemo/assets/gameplay/actions/20_turn_combat_actions.yaml`
- `Projects/WheatearDemo/assets/gameplay/actions/30_tactical_combat_actions.yaml`

这章最适合边看源码边看，因为 `WAO` 的概念本来就是“代码里已经长出来了”。

## 5.3 先给结论：ActionSystem 不是一个类

`ActionSystem` 只是教材标题。  
源码里没有一个巨大的 `ActionSystem` 类把所有事情包掉。

它实际是一组协作模块：

| 子层 | 主要职责 |
| --- | --- |
| 动作数据层 | `ActionRecipe`、`ActionDatabase`、`ActionAssetLoader`、`ActionRecipeQueries` |
| 动作执行层 | `ActionRunner`、`ActionRuntime`、`ActionExecutionResult` |
| 规则分流层 | `ActionResolverRegistry`、`ActionOrchestrator`、`ActionSignalRouter` |
| 扩展层 | `EffectRegistry`、`EffectFormula`、`StateRegistry` |
| 调试层 | `EffectLedger`、`ActionDebugHistory`、`WAOActionEditorPanel` |

这几个层合起来，才是你简历里说的 `WAO`。

你可以把它理解成一句话：

> `ActionIntent` 负责“我想做什么”，  
> `ActionRecipe` 负责“这个动作长什么样”，  
> `ActionResolver` / `ActionRunner` 负责“怎么执行”，  
> `EffectLedger` 负责“到底发生了什么”，  
> `ActionSignalRouter` 负责“把表现和反馈分出去”。

这就是它的骨架。

## 5.4 当前实现边界：统一什么，不统一什么

WAO 现在已经统一了这些事：

- 动作 id、显示名、描述、图标、音效、特效路径
- 冷却、前摇、后摇、命中帧、取消窗口、移动倍率
- 资源消耗、标签、条件、参数
- 效果列表、状态列表、表现信号
- 运行记录、调试账本、沙盒验证

WAO 还没有也不应该强行统一这些事：

- 横板里的 hitbox 扫描、浮空、纵深移动、保护条
- 回合制里的行动顺序、目标菜单、回合推进
- 战棋里的格子、路径、占位、范围规则
- 弹幕里的投射物飞行、碰撞、掩体阻挡

这点很重要。  
因为很多人一看到“动作系统”就会想把所有玩法规则都抽上来，最后把公共层做成一个四不像。

Wheatear 的做法更克制：

- 公共层负责**动作语义**。
- 玩法层负责**规则差异**。

这也是它适合简单 2D 游戏的地方。  
你不需要做成 UE GAS 那样的大系统，但也不必退回到“每个功能都各写各的”。

## 5.5 第一组核心对象：意图、配方、执行上下文

### 5.5.1 `ActionIntent`

`ActionIntent` 表示“谁想做什么动作”。

它的字段很少：

- `Actor`：动作发起者
- `ActionId`：动作 id
- `ExplicitTarget`：显式目标
- `WorldPoint`：世界坐标点
- `InputId`：输入来源
- `Source`：来源标签

这几个字段能覆盖很多来源：

- 玩家按键
- AI
- 事件脚本
- 编辑器沙盒
- 动画事件

举个简单例子：

```text
Actor = player
ActionId = side.basic1
ExplicitTarget = boss
Source = SideCombat.Player
```

你会发现，玩家按键并不直接等于“伤害已经发生”。  
它只是一个意图。

这一步的意义非常大：  
输入层只管提交意图，后面的规则、资源、时机、对象都可以换。

### 5.5.2 `ActionRecipe`

`ActionRecipe` 是动作配方，负责描述“这个动作是什么样子”。

它大致分成五类信息：

- 身份：`Id`、`DisplayName`、`Description`
- 表现：`IconPath`、`AnimationId`、`SoundPath`、`EffectPath`
- 时序：`Cooldown`、`Duration`、`Startup`、`Recovery`、`HitTime`、`CancelStart`、`CancelEnd`、`MovementScale`
- 规则：`Tags`、`RequiredStates`、`BlockedStates`、`RequiredTags`、`BlockedTags`、`ResourceCost`、`Params`
- 结果：`Effects`、`Signals``

它是数据驱动的核心。

例如 `side.break_limit` 这种动作，真正对横板有用的不是名字，而是这些字段：

- 冷却是多少
- 需要什么资源
- 命中点在哪
- 能不能取消
- 会不会发出 `side.break_limit.flash`

### 5.5.3 `EffectSpec`

`EffectSpec` 是动作里的单条效果描述。

当前源码里已经有这些效果类型：

- `Damage`
- `Heal`
- `ModifyAttribute`
- `AddState`
- `RemoveState`
- `StartCooldown`
- `ConsumeResource`
- `Launch`
- `HitStun`
- `EmitSignal`

另外还留了两个扩展入口：

- `CustomType`
- `Formula`

这两个入口很值钱，因为它说明 WAO 不是死板枚举，而是允许继续长的。

### 5.5.4 `ActionRuntime`

`ActionRuntime` 是动作运行时快照。

它里面放的是会变的东西：

- `Attributes`
- `States`
- `Tags`
- `Cooldowns`
- `Resources`

这和 `ActionRecipe` 正好相反：

- `Recipe` 是静态设计。
- `Runtime` 是运行时状态。

### 5.5.5 `ActionResolveContext` 和 `ActionResolveResult`

`ActionResolveContext` 是给 resolver 用的上下文，里面有：

- `SceneContext`
- `Intent`
- `Runtime`
- `TransientPayload`
- `Detail`

`TransientPayload` 很关键。  
它允许你把某个玩法自己的临时数据塞进来，比如弹幕里的发射参数、横板里的命中预览、编辑器里的沙盒上下文。

`ActionResolveResult` 则负责返回：

- 有没有处理
- 成不成功
- 写了哪些效果
- 留了什么账本
- 描述信息是什么

### 5.5.6 `ActionSignalContext`

`ActionSignalContext` 是给表现信号用的。

它和 intent 很像，但更偏“表现链路”：

- `Intent`
- `ActionId`
- `SignalId`
- `Source`
- `Detail`
- `TransientPayload`

它和 `CommandBus` 不是一回事。  
`CommandBus` 处理的是“命令分发”，`ActionSignalRouter` 处理的是“动作表现信号”。

## 5.6 动作数据怎么进来：`ActionDatabase` 与 `ActionAssetLoader`

### 5.6.1 `ActionDatabase`

`ActionDatabase` 是运行时动作表。

公开 API 很简单：

- `Register`
- `Find`
- `Has`
- `All`
- `Clear`
- `Revision`

它的设计目标有两个：

1. 查找快。
2. 供编辑器和玩法模块热刷新。

`Revision()` 很重要。  
回合制和战棋的 skill library 会拿它做缓存失效判断：只要数据库版本变了，就重建技能列表。

### 5.6.2 `ActionRecipeQueries`

这是很实用的一层辅助查询。

它提供的不是“新概念”，而是“把 recipe 当数据表查”的常用函数：

- `ComposeActionId`
- `FindRecipeOrWarn`
- `RecipesWithPrefix`
- `FirstEffect`
- `ParamString`
- `ParamFloat`
- `ParamInt`
- `ParamBool`
- `PrimaryEffectValue`
- `ResourceCost`
- `HasTag`

这层的价值是：  
玩法服务不用自己重复写字符串查表、类型转换、前缀过滤。

比如回合制和战棋的技能库，都会从 `RecipesWithPrefix("turn.")`、`RecipesWithPrefix("tactical.")` 里扫出配方，再把字符串参数变成真正可用的技能定义。

### 5.6.3 `ActionAssetLoader`

`ActionAssetLoader` 负责把 YAML 读进来。

它支持：

- `LoadFile`
- `LoadDirectory`
- `LoadManifest`
- `ReloadDirectory`
- `ReloadManifest`

当前项目里，动作集合清单是：

- `Projects/WheatearDemo/assets/gameplay/actions/action_sets.yaml`

里面列了四套：

- `00_arcade_actions.yaml`
- `10_side_combat_actions.yaml`
- `20_turn_combat_actions.yaml`
- `30_tactical_combat_actions.yaml`

这就是为什么你会在玩法层看到“一个模块一套动作表”。

### 5.6.4 YAML 里能写什么

当前 loader 能识别的字段大体包括：

- 基础字段：`id`、`displayName`、`description`
- 资源字段：`icon`、`animation`、`sound`、`effect`
- 时序字段：`cooldown`、`duration`、`startup`、`recovery`、`hitTime`、`cancelStart`、`cancelEnd`、`movementScale`
- 规则字段：`tags`、`requiredStates`、`blockedStates`、`requiredTags`、`blockedTags`
- 成本字段：`resourceCost`
- 参数字段：`params`
- 效果字段：`effects`
- 信号字段：`signals`

效果项还能写：

- `type`
- `attribute`
- `state`
- `signal`
- `value`
- `turns`
- `seconds`
- `durationPolicy`
- `customType`
- `formula`

这就是为什么 `turn.white_vow`、`side.break_limit`、`tactical.aether_lance` 能共用一套配方格式。

### 5.6.5 热重载

`Wheatear/src/Wheatear/Modules/GameplayModuleRuntime.h` 里有一个很直白的热重载函数 `UpdateActionHotReload()`。

它做的事很简单：

1. 每 500ms 检查一次。
2. 看 `action_sets.yaml` 的修改时间。
3. 变了就 `ReloadManifest`。
4. 没有 manifest 就退回 `ReloadDirectory`。

这对教学型引擎非常友好。  
因为你可以改一条动作数据，立刻看效果，不用重启整个程序。

## 5.7 动作怎么被执行：`ActionRunner`

如果说 `ActionDatabase` 管数据，`ActionRunner` 管的就是“把数据真正跑起来”。

### 5.7.1 `ActionRunner` 的运行时快照

`ActionRunner.h` 里有一个 `ActionRuntime`：

- `Attributes`
- `States`
- `Tags`
- `Cooldowns`
- `Resources`

这说明 `ActionRunner` 是一个**轻量运行器**，它并不直接操作场景里的复杂组件。

这点很关键：

- 它适合沙盒测试。
- 它适合局部执行。
- 它适合 fallback。
- 它不替代横板、回合制、战棋各自的复杂规则服务。

### 5.7.2 `CanAfford` 与 `CanActivate`

`CanAfford` 检查资源是否够。

`CanActivate` 还会继续检查：

- 冷却是否结束
- 资源是否足够
- 必须存在的状态有没有
- 禁止存在的状态有没有
- 必须存在的标签有没有
- 禁止存在的标签有没有

这就是最基础的动作门禁。

简单说：  
`Recipe` 告诉你“这个动作应该能做什么”，  
`CanActivate` 告诉你“现在能不能做”。

### 5.7.3 `Execute` 的顺序

`ActionRunner::Execute()` 的顺序大致是：

1. 记录当前动作意图。
2. 检查能不能激活。
3. 先扣资源。
4. 再挂冷却。
5. 再逐条应用 `recipe.Effects`。
6. 再按 `recipe.Signals` 发信号。
7. 把结果写进 `ActionDebugHistory`。

这条顺序很重要。  
因为它决定了“动作事实”和“表现信号”是怎么分开的。

### 5.7.4 当前 `EffectType` 的实际行为

`ActionRunner` 目前对这些效果做了真正的 runtime 操作：

- `Damage`：扣生命
- `Heal`：加生命
- `ModifyAttribute`：改属性
- `AddState`：挂状态
- `RemoveState`：移状态
- `StartCooldown`：开始冷却
- `ConsumeResource`：扣资源
- `EmitSignal`：发信号

而这些项在 `ActionRunner` 里更多是记录：

- `Launch`
- `HitStun`

这点别看小，它说明了当前架构的边界：

> 真正的物理位移、命中反应、控制状态，仍然应该由具体玩法服务来处理，不应该硬塞进一个通用 runner 里。

### 5.7.5 `ActionRunner` 适合什么时候用

最适合的地方有三个：

- 编辑器沙盒
- 单元式验证
- 一些不依赖场景复杂对象的轻量动作执行

在 `WAOActionEditorPanel::RunSandbox()` 里就能看到：

1. 先造一个 synthetic runtime。
2. 填好生命、魔力、资源。
3. 调 `WAO::Execute()`。
4. 看前后属性差和账本。

这非常适合教材和面试演示。

## 5.8 动作怎么被分流：`ActionResolverRegistry` 与 `ActionOrchestrator`

如果说 `ActionRunner` 是“怎么执行”，那么 `ActionOrchestrator` 更像“先找谁来执行”。

### 5.8.1 `ActionResolverRegistry`

它提供的 API 也很清楚：

- `RegisterResolver`
- `Resolve`
- `ClearResolvers`

它按**动作 id 前缀**路由。

比如：

- `arcade.` 走弹幕的 resolver
- `side.`、`turn.`、`tactical.` 现在多是预览 resolver

这里要记住一个细节：  
它是**前缀匹配**，不是复杂的规则系统。

### 5.8.2 `ActionOrchestrator`

`ActionOrchestrator::Execute()` 做的第一件事不是立刻执行，而是先去 `ActionDatabase` 找 recipe。

`ExecuteWithRecipe()` 则按这个顺序：

1. 先尝试 resolver。
2. 如果 resolver 接了，就走 resolver。
3. 如果没接但有 runtime，就 fallback 到 `ActionRunner`。
4. 如果两者都没有，就报失败。

这很好理解：

- `ActionRunner` 负责轻量通用执行。
- `ActionResolver` 负责玩法专属执行。

### 5.8.3 当前代码里谁在用它

当前比较明确的情况是：

- `ArcadeCombatActionResolver` 注册了 `arcade.` resolver。
- `ModuleBootstrap` 给 `side.`、`turn.`、`tactical.` 注册了预览 resolver。
- `SideCombatActionService` 和 `ArcadeCombatPlayerService` 会直接调用 `ActionOrchestrator::ExecuteWithRecipe()`。

而回合制、战棋的主结算链路目前仍由各自的 action service 负责，只是数据来自 WAO。

这不是缺陷，而是刻意的分层。

### 5.8.4 `TransientPayload` 的作用

这个字段是 resolver 体系里最灵活的一块。

例如弹幕发射时，`ArcadeCombatSignalHandlers::ProjectileSpawnPayload` 会被塞进去。  
resolver 不用硬依赖某个 gameplay 组件类型，只要能拿到这包临时数据就行。

这比在公共层里写一堆 `dynamic_cast` 强多了。

## 5.9 动作信号怎么出去：`ActionSignalRouter`

`ActionSignalRouter` 是 WAO 的表现信号出口。

### 5.9.1 它解决什么问题

动作执行之后，经常不是“只改一个数值”这么简单，而是还要：

- 播音效
- 出特效
- 刷 UI
- 发镜头震动
- 触发弹幕生成

如果这些都写死在执行器里，系统会变得很脏。

所以 WAO 把它们抽成了“信号”。

### 5.9.2 这个路由器怎么工作

它的 API 只有三个：

- `RegisterHandler`
- `Emit`
- `ClearHandlers`

它按 signal id 的前缀匹配 handler。

例如：

- `arcade.projectile.spawn`
- `side.action.hit`
- `turn.skill.vfx`

### 5.9.3 同步触发和临时载荷

`ActionSignalContext` 里有一个 `TransientPayload`，注释写得很清楚：只在同步 `Emit` 期间有效。

这意味着：

- 它适合立即消费。
- 不适合长时间保存。
- 不适合跨线程乱传。

弹幕玩法就是典型用法：

1. 动作 resolver 发出 `arcade.projectile.spawn`。
2. `ArcadeCombatSignalHandlers` 收到。
3. 立即调用 `ArcadeCombatProjectileService::CreateProjectile()`。

### 5.9.4 为什么它不是 `CommandBus`

因为职责不同。

- `CommandBus` 处理的是“一个命令该去哪里”。
- `ActionSignalRouter` 处理的是“动作结果该怎么表现”。

前者偏流程入口。  
后者偏表现出口。

## 5.10 动作怎么扩展：`EffectRegistry` 与 `EffectFormula`

### 5.10.1 `EffectRegistry`

`EffectRegistry` 允许你注册自定义效果语义。

它的核心是：

- `Register`
- `Run`
- `Has`
- `DisplayName`
- `AllIds`

注册时你给它：

- 一个 id
- 一个显示名
- 一个 handler

这意味着 WAO 不是只能改枚举，还能继续长新语义。

当前 `ModuleBootstrap.cpp` 里已经注册了几个示例：

- `lifesteal`
- `mana_leech`
- `full_heal`

### 5.10.2 自定义效果在什么地方用

最直观的地方是 SideCombat 的道具使用链路。

`SideCombatPlayerService` 里有一段逻辑：

1. 先看 `CustomType`
2. 再看 `Formula`
3. 再走普通 `EffectType` 分支

这代表动作效果不是只能靠一个 `switch`。

### 5.10.3 `EffectFormula`

`EffectFormula` 是一套很实用的表达式求值器。

它支持：

- 数字
- 变量
- 括号
- `+ - * / %`
- 比较
- 逻辑
- `min / max / clamp / abs / round / floor / ceil / if`

变量名可以是点号风格的键，比如：

- `source.attack`
- `target.health`
- `controller.mana`

这特别适合做 2D 游戏里的简单公式。

例如：

```text
min(target.max_health, target.health + 50)
```

或者：

```text
source.attack * 1.2 + 8
```

这类表达式足够给简单 2D 游戏提供灵活度，又不会像完整脚本语言那样过重。

## 5.11 状态怎么统一：`StateRegistry`

### 5.11.1 内置状态

`StateRegistry` 里当前内置的状态很少，但够用：

- `guard`
- `regen`
- `burn`
- `def_down`
- `stun`

这几个状态正好覆盖了 turn / tactical 里最常见的公共状态。

### 5.11.2 它提供的能力

主要接口有：

- `MakeState`
- `HasState`
- `ApplyState`
- `RemoveState`
- `FormatStates`
- `CalculateDefenseMultiplier`
- `CalculateDamageTakenMultiplier`
- `TickTurnStates`

它的意义是把“状态”从散乱字段里收束出来。

### 5.11.3 哪些玩法在用

回合制和战棋都已经在用 `WAO::RuntimeState`：

- `TurnCombatSkillService`
- `TacticalCombatSkillService`
- `TurnCombatActionService`
- `TacticalCombatActionService`

这意味着：

> 这两个玩法的 `guard`、`regen`、`burn`、`def_down`、`stun` 不是各自重写一遍，而是共用同一套状态定义和 tick 逻辑。

这是 WAO 真正落地的价值之一。

### 5.11.4 和横板状态的区别

横板也有自己的运行状态，比如 `SuperArmor`、`HitStun`、`Launched`、`Broken`。

但那一套更多是横板手感和关卡交互，不是公共状态系统。  
所以它仍然留在 SideCombat 自己的组件和服务里。

这很正常。  
公共层只收最常见、最值得复用的那一部分。

## 5.12 账本怎么记：`EffectLedger` 与 `ActionDebugHistory`

### 5.12.1 `EffectLedger`

`EffectLedger` 是 WAO 里最有“工程味”的一个对象。

它记录的不是“感觉上发生了什么”，而是“这次动作实际提交了哪些事实”。

它有三个基础动作：

- `BeginAction`
- `Record`
- `Clear`

每条 `EffectLedgerEntry` 会写：

- `ActionId`
- `Type`
- `Source`
- `Target`
- `Detail`
- `Value`
- `Applied`

这比单纯 `bool` 或 `int` 好太多了。  
因为你可以把整次动作回放成一串事实。

### 5.12.2 `ActionDebugHistory`

它是调试历史。

API：

- `Record`
- `RecordSimple`
- `Recent`
- `Clear`

它有一个很重要的实现细节：

- **Debug 版**才会真正保存深拷贝。
- **Release 版**直接跳过，避免热路径开销。

这非常适合做工具型引擎。

### 5.12.3 编辑器怎么用它

`WAOActionEditorPanel` 会把最新的运行账本显示出来。

所以你在编辑器里点一次“Run in Sandbox”，就能直接看到：

- 哪些资源被消耗了
- 哪些效果被应用了
- 哪些信号发出去了
- 有没有某个效果失败

这就是它适合拿来面试展示的原因。

## 5.13 真实链路一：`side.basic1` 怎么从按键到结果

这是最适合讲给初学者听的一条链。

### 5.13.1 起点：玩家输入

横板里玩家按攻击键后，`SideCombatPlayerService` 不会直接在输入层里改血条。  
它会先找对应的动作配方。

大致过程是：

```text
按键
  ↓
SideCombatPlayerService
  ↓
ActionRecipe("side.basic1")
```

### 5.13.2 配方给了什么

`side.basic1` 这类动作配方会提供：

- `Duration`
- `HitTime`
- `CancelStart`
- `CancelEnd`
- `MovementScale`
- `SoundPath`
- `Signals`

于是 `SideCombatActionService::BeginPlayerAction()` 可以直接把这些值写进运行时控制器：

- 动作持续多久
- 什么时候出 hitbox
- 什么时候可以取消
- 移动倍率是多少

这就是数据驱动的最直观收益。

### 5.13.3 中间：开始动作

`SideCombatActionService` 还会调用 `ActionOrchestrator::ExecuteWithRecipe()`。

这里的用途更偏“动作起手的统一入口”和“调试记录”。

真正的横板命中、浮空、Boss 保护条，仍然在：

- `SideCombatHitboxService`
- `SideCombatHitResolutionService`
- `SideCombatPhysicsService`

这几个服务里。

### 5.13.4 结果：命中结算

当 hitbox 命中后，`SideCombatHitResolutionService` 会组一个 `EffectLedger`：

- `Damage`
- `HitStun`
- `Launch`
- `ModifyAttribute`
- `EmitSignal`
- `AddState`

然后交给 `ActionDebugHistory::Record()`。

这条链路的重点不是“谁扣了血”，而是：

> 一次攻击的全部事实，都可以被整理成账本。

### 5.13.5 这条链路的教材价值

这就是为什么我建议你在面试里这么讲：

> 横板动作的“起手”部分走 WAO 配方，负责统一时序、资源和表现；“命中”部分仍留在横板玩法服务里，负责实际 hitbox、浮空、保护条和状态控制。这样公共层和玩法层分工很清楚。

## 5.14 真实链路二：`turn.white_vow` 怎么从配方变成技能

### 5.14.1 技能库怎么来的

`TurnCombatSkillService::SkillLibrary()` 会从 `ActionDatabase::Revision()` 里看缓存是否过期。

如果过期，就重新从：

```text
RecipesWithPrefix("turn.")
```

构建技能库。

这说明 turn combat 不是把技能硬编码在一个大表里，而是直接从 WAO 动作数据里“长出来”。

### 5.14.2 `Params` 是怎么被用起来的

`TurnCombatSkillService` 会用这些查询函数：

- `ParamString`
- `ParamFloat`
- `ParamInt`
- `ParamBool`
- `PrimaryEffectValue`
- `ResourceCost`
- `HasTag`

然后把 recipe 转成更适合战斗流程的 `TurnSkillDefinition`。

比如：

- `category`
- `targetRule`
- `manaCost`
- `power`
- `healPower`
- `defensePierce`
- `statusEffect`
- `effectTurns`
- `effectPower`

这套流程很适合解释给面试官听：

> recipe 是数据层，skill definition 是玩法层的可执行定义。

### 5.14.3 `TurnCombatActionService` 做了什么

`TurnCombatActionService::ApplySkill()` 里会：

- 找到 recipe
- 生成 `ActionIntent`
- 扣魔力
- 按技能类型处理治疗 / 伤害 / 防御 / 等待
- 记录 `EffectLedger`
- 写入 `ActionDebugHistory`

你可以把它理解成：

> turn combat 仍然由玩法服务做实际结算，但 WAO 提供了统一数据、统一状态、统一账本格式。

### 5.14.4 `turn.white_vow` 的实际意义

它展示了一个很常见的 2D 游戏技能：

- 有动作名
- 有资源消耗
- 有治疗效果
- 有持续状态
- 有音效和特效

这些信息都可以从同一个 recipe 读出来。  
这就是数据化技能最实在的地方。

## 5.15 真实链路三：`tactical.aether_lance` 怎么和战棋规则拼起来

战棋和回合制有点像，但更强调格子与范围。

### 5.15.1 先从 recipe 读出战棋语义

`TacticalCombatSkillService` 会把 recipe 变成技能定义，重点读取：

- `category`
- `targetRule`
- `range`
- `power`
- `healPower`
- `defensePierce`
- `magic`
- `guard`
- `statusEffect`
- `effectTurns`
- `effectPower`

比如：

- `tactical.aether_lance`
- `tactical.white_pulse`

这类动作，一眼就能看出是远程、治疗还是防御。

### 5.15.2 战棋为什么需要 WAO

因为战棋里经常会重复这些东西：

- 攻击技能
- 远程技能
- 治疗技能
- 防御技能
- 持续状态

如果每个技能都各写一套，规则会碎。

WAO 的价值在于：

> 状态、效果、资源、描述、表现信号先统一，格子与移动规则再留给战棋模块。

### 5.15.3 结算还是在玩法层

`TacticalCombatActionService` 仍然负责：

- 选目标
- 算距离
- 判断范围
- 真正扣血 / 回血
- 写入 `EffectLedger`

所以战棋不是“交给 WAO 就完事”，而是“WAO 作为公共语义层，战棋作为规则层”。

## 5.16 真实链路四：`arcade.gun` 为什么最像“动作编排”

弹幕玩法很适合展示 resolver + signal 的组合。

### 5.16.1 入口

`ArcadeCombatPlayerService` 和 `ArcadeCombatBossService` 会先从 recipe 里读：

- 冷却
- 伤害
- 音效
- 标签

然后构造 `ActionResolveContext`。

### 5.16.2 直接交给 `ActionOrchestrator`

`arcade.` 前缀已经注册了真正的 resolver：

```text
ArcadeCombatActionResolver::RegisterResolver()
```

这个 resolver 会：

- 检查 payload
- 记录 ledger
- 播放音效
- 发出 `arcade.projectile.spawn`

### 5.16.3 `ActionSignalRouter` 接住了什么

`ArcadeCombatSignalHandlers` 会注册 `SpawnProjectileSignal` 的 handler。

于是信号到了之后，立刻调用：

- `ArcadeCombatProjectileService::CreateProjectile()`

这条链特别适合拿来解释“动作编排”四个字：

> 先是动作意图，接着是动作配方，再是规则分流，最后才是投射物创建。

### 5.16.4 为什么这个例子好讲

因为它一眼能看出：

- `ActionRecipe` 不是摆设
- `ActionSignalRouter` 不是摆设
- `TransientPayload` 不是摆设
- `ActionOrchestrator` 不是摆设

它们真的把“发射”变成了一条清晰链路。

## 5.17 真实链路五：WAO Action Editor 沙盒怎么验证动作

`WAOActionEditorPanel` 是很好的教材配套。

### 5.17.1 先验证，再保存

编辑器里会先做一轮校验：

- id 是否为空
- 资源路径是否存在
- 时序值是否合理
- `resourceCost` 是否负值
- `params` 是否可解析
- `effects` 是否有非法状态、属性、信号

这比“写完就信”要靠谱得多。

### 5.17.2 沙盒运行

`RunSandbox()` 会：

1. 构造一个假的 runtime
2. 给它填健康值、魔力、资源
3. 调 `WAO::Execute(intent, recipe, runtime)`
4. 展示属性前后差异
5. 展示 ledger

这非常适合新人理解“动作配方到底会改变什么”。

### 5.17.3 教材里为什么要讲这个

因为很多初学者看动作系统时，只会盯着运行时的复杂逻辑。  
但真正能让系统稳下来的，是**编辑器能不能验证数据**。

这也是为什么 WAO 很适合作为教材章节：

- 运行时
- 编辑器
- 数据表
- 调试账本

这四者是连在一起的。

## 5.18 WAO 还能统一什么，不能统一什么

### 5.18.1 能统一的

- 动作身份
- 动作描述
- 动作资源
- 冷却与时序
- 状态与资源消耗
- 效果和信号
- 调试记录
- 沙盒验证

### 5.18.2 不能硬统一的

- 横板的 hitbox 判定
- 战棋的格子范围
- 回合制的行动顺序
- 弹幕的飞行和碰撞
- Boss 的保护条
- 纵深、平台、地形

如果把这些也硬塞进 WAO，系统就会变成一个“什么都能管，什么都不好管”的大盒子。

## 5.19 和 Unity、UE、Godot、常见 2D 项目的对照

### 5.19.1 和 UE GAS 的关系

WAO 明显吸收了 GAS 的想法：

- 能力激活前要检查条件
- 属性、资源、状态、效果需要统一入口
- 动作不应该直接散改组件
- 表现应该和结算分开

但它没有照搬：

- 没有巨型 ASC
- 没有网络复制
- 没有强制 UObject 体系
- 没有把所有玩法硬塞进同一套能力结构

WAO 更像是“适合单机 2D 多玩法的轻量版动作语义层”。

### 5.19.2 和 Unity 常见做法

Unity 里很多项目会是：

- `ScriptableObject` 存技能数据
- `MonoBehaviour` 做执行
- `BuffManager`、`CooldownManager`、`EffectManager` 各管一块

这种做法直观，但项目一大就容易碎。

WAO 的区别是：

- 它把动作配方集中到统一数据层
- 它把结算放进账本
- 它把表现从执行里拆出去

### 5.19.3 和 Godot 的信号 / 状态机思路

Godot 很擅长信号和状态机。  
WAO 也有“信号”这层，但它更强调：

- `EffectLedger` 先记事实
- `ActionSignalRouter` 再发表现

不是“信号一来，谁都可以偷偷改状态”。

### 5.19.4 和常见 2D 小项目

常见 2D 小项目最容易写成：

```text
Input
  -> PlayerController
  -> CombatController
  -> Health
  -> UI
```

短期很快，长期很乱。

WAO 的目标不是让你更“学术”，而是让你在项目变大以后还写得动。

## 5.20 这一层的好处

- 动作数据统一
- 玩法复用更自然
- 调试和面试表达更清楚
- 编辑器可以直接验证 recipe
- 同一种状态效果可以被多个玩法共享
- 新增玩法时不必从头写一套动作系统

对你这种“面试要讲清楚、项目还要继续长”的场景，这层非常合适。

## 5.21 这一层的坏处

- 前期设计成本更高
- 容易把简单问题做重
- 需要克制“什么都往 WAO 里塞”的冲动
- 需要维护数据、编辑器、运行时三者的一致性
- debug / release 行为要分清

所以 WAO 的正确心态不是“做一个万能动作框架”，而是：

> 在足够统一和不过度抽象之间，找到一个适合 2D 游戏项目的平衡点。

## 5.22 常见误区

### 5.22.1 误区一：WAO 就是战斗系统本体

不是。  
它是动作语义层，不是全部玩法规则的替代品。

### 5.22.2 误区二：有了 `ActionRunner` 就不用玩法服务了

也不是。  
`ActionRunner` 更像轻量执行器或沙盒执行器，不能替你处理横板 hitbox、战棋格子、投射物飞行这些规则差异。

### 5.22.3 误区三：`EffectRegistry` 可以把所有逻辑都塞成 data

不对。  
它只是给“新增效果语义”留了一个扩展口，不代表所有玩法规则都适合纯数据化。

### 5.22.4 误区四：`ActionSignalRouter` 等于事件总线

不等于。  
它只是动作语义里的表现出口，作用范围更窄。

### 5.22.5 误区五：`PreviewResolver` 就是实际结算

也不对。  
它当前主要用于预览、调试和统一语义展示，真正的玩法结算仍在模块内部。

## 5.23 面试怎么讲

如果面试官问“WAO 是什么”，可以这样答：

> WAO 是我给 Wheatear 做的动作编排与效果结算层。它参考了 UE GAS 的思想，但没有照搬 ASC 结构，而是更适合单机 2D、多玩法和编辑器驱动的场景。它把动作意图、动作配方、规则解析、效果账本和表现信号串成一条链，用来统一横板、回合制、战棋和弹幕里共通的动作语义。

如果面试官问“你为什么要做它”，可以这样答：

> 因为我希望简单 2D 游戏的动作系统既能数据化，又不会把玩法规则全抹平。WAO 让我可以统一资源、状态、效果和调试，而横板 hitbox、战棋格子、回合顺序这类差异仍留在各自玩法模块里，所以它既能复用，又不会过度抽象。

如果面试官问“和 GAS 的区别”，可以这样答：

> 我借鉴的是 GAS 的思想，不是它的结构。我的项目是单机 2D，所以没有做 ASC、网络复制和大型 UObject 树，而是做了更轻量的 ActionIntent、ActionRecipe、Resolver、EffectLedger 和 SignalRouter。这样更符合我项目的边界。

如果面试官问“你最能讲清楚的一条链路是什么”，可以直接讲：

> 以 `arcade.gun` 为例，玩家或 Boss 先形成 ActionIntent，ActionOrchestrator 根据 recipe 找到 `arcade.` resolver，resolver 记录账本并发出 `arcade.projectile.spawn` 信号，信号处理器再创建投射物。这样动作、表现和实际对象创建分层很清楚。

## 5.24 给初学者的阅读顺序

如果你是第一次看这章，建议按这个顺序：

1. 先看 `ActionTypes.h`
2. 再看 `ActionRecipeQueries.cpp`
3. 再看 `ActionDatabase.cpp`
4. 再看 `ActionAssetLoader.cpp`
5. 再看 `ActionRunner.cpp`
6. 再看 `ActionResolver.cpp`
7. 再看 `ActionSignalRouter.cpp`
8. 再看 `StateRegistry.cpp`
9. 再看 `EffectRegistry.cpp` 和 `EffectFormula.cpp`
10. 最后回到 `SideCombatActionService.cpp`、`TurnCombatActionService.cpp`、`TacticalCombatActionService.cpp`、`ArcadeCombatActionResolver.cpp`

如果你只想抓一条最清晰的教学线，就先看：

- `side.basic1`
- `turn.white_vow`
- `arcade.gun`

这三条链路足够把 WAO 看明白。

## 5.25 你可以马上做的三个练习

### 练习 1：读懂一条 recipe

随便挑一条 `side.` 或 `turn.` 动作，找出：

- 它的冷却
- 它的资源消耗
- 它的效果
- 它的信号
- 它的标签

### 练习 2：追一条执行链

从某个玩法入口开始，追到：

- `ActionIntent`
- `ActionRecipe`
- `ActionRunner` 或 `ActionOrchestrator`
- `EffectLedger`
- `ActionDebugHistory`

### 练习 3：改一条数据看变化

改一条 action YAML 的：

- `cooldown`
- `startup`
- `signals`
- `effects`

然后看热重载和编辑器沙盒如何反应。

## 5.26 本章小结

这一章你应该抓住四个关键词：

1. **意图**
2. **配方**
3. **账本**
4. **信号**

它们分别对应：

- `ActionIntent`
- `ActionRecipe`
- `EffectLedger`
- `ActionSignalRouter`

再加上两条支撑线：

- `ActionRunner` / `ActionOrchestrator`
- `EffectRegistry` / `StateRegistry`

就构成了 Wheatear 的 WAO。

如果说上一章 `EventScript` 解决的是“流程怎么编排”，  
这一章解决的就是“动作怎么被统一表达并被记录下来”。

## 5.27 下一章

下一章建议接着写 **SceneTransition**。

原因很简单：

- `CommandBus` 已经讲了命令怎么分发
- `EventScript` 已经讲了流程怎么串
- `ActionSystem` 已经讲了动作怎么表达
- 接下来就该讲“场景为什么切、怎么切、切到哪里去”

这条顺序会很自然。
