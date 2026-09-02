# 第 6 编 02：SideCombat 与 WAO

> 本章所属层级：**玩法层**
>
> 本章目标：理解 Wheatear 的横板战斗是怎样把“输入、技能槽、动作配方、命中框、状态结算、结果跳转、调试记录”串成一条完整链路的。
>
> 先提醒一句：这一章的重点不是“我写了一个横板动作游戏”，而是“我如何把横板战斗拆成可以复用、可以调试、可以继续扩展的结构”。  
> `SideCombat` 是最贴近玩家体验的玩法层实例；`WAO` 是这套战斗背后的动作编排层。  
> 你可以把它们理解成：`SideCombat` 负责“这一场怎么打”，`WAO` 负责“一个动作怎么被统一描述和执行”。
>
> 本章会反复使用两条最典型的链路：
> 1. 玩家按 `J` 键，普通三段斩是怎么从按键变成一次可结算的动作。
> 2. 玩家按 `L` 键的 `Break Limit` 是怎么穿过资源、条件、命中和结果反馈的。

## 6.1 这一层在架构中的位置

`SideCombat` 不属于引擎最底层，也不属于核心平台层。
它属于**玩法层**，因为它处理的是战斗规则本身：

- 玩家怎么移动
- 什么时候能出招
- 角色怎么受击
- Boss 保护条怎么破
- 结果页什么时候跳
- 战斗奖励怎么结算

这些都是玩法语义，不是底层平台语义。

如果按我们前面约定的学习顺序来看，它是接在 `VN` 后面的一个更“硬”的玩法例子：

```text
VN / EventScript
    ↓
SideCombat / WAO
    ↓
TurnCombat / TacticalCombat / Arcade
```

`VN` 讲的是剧情怎么跑。  
`SideCombat` 讲的是战斗怎么打。  
两者都属于玩法层，但 `SideCombat` 更适合拿来讲“动作、状态、命中、结算”的完整工程链路。

## 6.2 本章先看哪些源码

建议按这个顺序看：

| 文件 | 作用 |
| --- | --- |
| `Projects/WheatearDemo/assets/scenes/SideCombatBeastPath.wt` | 这一场侧视战斗在场景里怎么配置 |
| `Wheatear/src/Wheatear/Modules/SideCombat/SideCombatComponents.h` | 战斗场景组件定义 |
| `Wheatear/src/Wheatear/Modules/SideCombat/SideCombatSystem.cpp` | 每帧更新总入口 |
| `Wheatear/src/Wheatear/Modules/SideCombat/SideCombatPlayerService.cpp` | 玩家输入和出招 |
| `Wheatear/src/Wheatear/Modules/SideCombat/SideCombatEnemyAIService.cpp` | 敌人 AI 出招 |
| `Wheatear/src/Wheatear/Modules/SideCombat/SideCombatActionService.cpp` | 动作开始、时序、配方接入 |
| `Wheatear/src/Wheatear/Modules/SideCombat/SideCombatHitboxService.cpp` | 命中框生成和碰撞检测 |
| `Wheatear/src/Wheatear/Modules/SideCombat/SideCombatHitResolutionService.cpp` | 命中结算、击退、受击状态 |
| `Wheatear/src/Wheatear/Modules/SideCombat/SideCombatPhysicsService.cpp` | 战斗体物理更新 |
| `Wheatear/src/Wheatear/Modules/SideCombat/SideCombatComboService.cpp` | 连击和资源条 |
| `Wheatear/src/Wheatear/Modules/SideCombat/SideCombatOutcomeService.cpp` | 胜负判定、掉落、跳结果页 |
| `Wheatear/src/Wheatear/Modules/SideCombat/SideCombatHudService.cpp` | 战斗 HUD |
| `Wheatear/src/Wheatear/Modules/SideCombat/SideCombatVisualService.cpp` | 角色视觉表现 |
| `Wheatear/src/Wheatear/Modules/SideCombat/SideCombatSkillRegistry.h` | 自定义技能扩展点 |
| `Wheatear/src/Wheatear/Modules/SideCombat/SideCombatTuningService.h` | 战斗调参、技能槽、道具槽 |
| `Wheatear/src/Wheatear/Gameplay/Action/ActionTypes.h` | WAO 的动作配方和运行时数据 |
| `Wheatear/src/Wheatear/Gameplay/Action/ActionDatabase.h/.cpp` | 动作表 |
| `Wheatear/src/Wheatear/Gameplay/Action/ActionAssetLoader.h/.cpp` | 动作表加载 |
| `Wheatear/src/Wheatear/Gameplay/Action/ActionResolver.h/.cpp` | 动作解析和调度 |
| `Wheatear/src/Wheatear/Gameplay/Action/ActionRunner.h/.cpp` | 动作执行 |
| `Wheatear/src/Wheatear/Gameplay/Action/ActionSignalRouter.h/.cpp` | 信号路由 |
| `Projects/WheatearDemo/assets/gameplay/actions/10_side_combat_actions.yaml` | 横板战斗动作配方 |
| `Projects/WheatearDemo/assets/events/sandbox_flow.wts` | 战斗结束后的事件流 |

如果你只想先抓住主链路，就先看：

`SideCombatComponents` -> `SideCombatSystem` -> `SideCombatPlayerService` -> `SideCombatActionService` -> `ActionTypes / ActionDatabase / ActionResolver` -> `Hitbox / HitResolution` -> `OutcomeService`。

## 6.3 SideCombat 的总骨架

这套战斗不是一个“巨大的战斗类”，而是一组服务协作出来的。

你可以先记住这条总线：

```text
Scene
  └─ SideCombatLevelComponent
        ├─ 场景绑定
        ├─ 波次 / 地图 / 结果命令
        └─ 运行时状态

SideCombatSystem
  ├─ 读取输入
  ├─ 接收 side: 命令
  ├─ 驱动玩家 / 敌人 / 命中框 / 物理 / 结果
  └─ 刷新 HUD

WAO
  ├─ ActionRecipe
  ├─ ActionDatabase
  ├─ ActionResolver / ActionRunner
  └─ EffectLedger / ActionDebugHistory
```

`SideCombatSystem::OnRuntimeStart()` 做三件大事：

1. 读取当前 dungeon / profile。
2. 根据波次表生成敌人。
3. 重置战斗运行时。

`SideCombatSystem::OnUpdateRuntime()` 则按固定顺序推进一帧：

1. 收集 `side:` 命令。
2. 读取键盘和虚拟摇杆。
3. 形成 `PlayerInputState`。
4. 更新玩家。
5. 更新敌人 AI。
6. 更新命中框。
7. 更新物理和受击状态。
8. 更新胜负和掉落。
9. 更新结果转场。
10. 更新 HUD。

这条顺序很重要。  
它说明战斗不是“按键就立刻结算”，而是一个从输入到反馈再到结果的时序链。

## 6.4 场景里怎么配置一场侧视战斗

这章最好先从场景看起，因为 `SideCombat` 的很多行为都不是写死在代码里的，而是挂在场景组件上。

在 `SideCombatBeastPath.wt` 里，核心配置大致长这样：

```yaml
SideCombatLevelComponent:
  PlayOnStart: true
  LevelId: CH02_MAT_BeastPath
  TuningPath: assets/sandbox/data/side_combat_tuning.yaml
  WaveModeEnabled: true
  VictorySceneCommand: event:beast_path_victory
  DefeatSceneCommand: event:beast_path_retry
  PlayerEntityName: SC_Player
  BossEntityName: SC_Boss_BearHusband
  SkillHudSlots:
    - Key: J
      Command: side:basic
    - Key: K
      Command: side:launcher
    - Key: L
      Command: side:break_limit
  CombatItemHudSlots:
    - Key: "1"
      Command: side:item:1
```

这几行其实已经把整场战斗的骨架说明白了：

- `LevelId` 说明这是哪一关。
- `WaveModeEnabled` 说明敌人会按波次推进。
- `VictorySceneCommand` / `DefeatSceneCommand` 说明战斗结束后接什么流程。
- `PlayerEntityName` / `BossEntityName` 说明 runtime 要找哪个实体。
- `SkillHudSlots` 说明战斗按钮和输入命令怎么挂。
- `CombatItemHudSlots` 说明道具槽也可以数据驱动。

再补一个很重要的细节：  
如果当前进度里已经有 active dungeon profile，`SideCombatSystem::ApplyActiveDungeonProfile()` 还会把 `LevelId` 和胜负转场命令按进度覆盖掉。  
也就是说，场景里写的是“默认配置”，运行时可能会根据当前进度切到另一套 dungeon 语义。

这很适合简单 2D 游戏，因为你不需要为了每一场战斗都写一套专门的代码。
你只要改场景和调参，就能换出不同的战斗体验。

## 6.5 WAO 是什么

WAO 可以理解成一层**动作编排层**。

它的核心思想很像“把动作抽成可复用的数据和执行框架”，但又比完整的 AAA 战斗系统轻很多。

WAO 里最重要的四个概念是：

- `ActionIntent`：谁想做什么。
- `ActionRecipe`：这个动作长什么样。
- `ActionRuntime`：当前运行时有哪些资源、状态和冷却。
- `EffectLedger`：这次动作到底造成了什么结果。

你可以这样理解它们：

```text
玩家按键 / AI / 事件脚本
    -> ActionIntent
    -> ActionRecipe
    -> ActionRunner / ActionResolver
    -> EffectLedger / ActionDebugHistory
    -> 表现、状态、资源变化
```

### 6.5.1 `ActionIntent`

`ActionIntent` 描述的是“意图”，不是结果。

它会带上：

- 发起者 `Actor`
- 动作 id `ActionId`
- 显式目标 `ExplicitTarget`
- 世界坐标点 `WorldPoint`
- 输入来源 `InputId`
- 来源标签 `Source`

这意味着，玩家按键、AI、事件脚本、编辑器沙盒，都可以变成同一种动作输入。

### 6.5.2 `ActionRecipe`

`ActionRecipe` 是配方。

它把一个动作的这些东西放在一起：

- 名称、描述、图标、动画、音效
- 冷却、持续时间、前摇、后摇、命中时机
- 取消窗口、移动倍率
- 资源消耗、标签、条件
- 效果列表、信号列表

这就是 WAO 的重点：  
动作不再是散在不同服务里的零件，而是先被放进一份“可读的配方”里。

### 6.5.3 `ActionDatabase` 和 `ActionAssetLoader`

动作配方不是写死在 C++ 里的，而是从 YAML 读出来的。

`ActionAssetLoader` 会加载 `action_sets.yaml` 和动作目录；  
`ActionDatabase` 负责注册、查询和版本号。

这就是为什么你能在编辑器里改动作参数，而不用每次都改代码。

### 6.5.4 `ActionResolver` / `ActionRunner`

`ActionResolver` 负责把“这个动作能不能跑、该怎么路由”讲清楚。  
`ActionRunner` 负责真正执行配方里的效果。

两者合起来，就是 WAO 的执行内核。

### 6.5.5 `EffectLedger` 和 `ActionDebugHistory`

这两个东西是很实用的教材点。

- `EffectLedger` 记录这次动作的效果条目。
- `ActionDebugHistory` 保存这些条目，方便调试和回看。

这能帮你在面试里回答“你怎么知道一个动作到底做了什么”。

## 6.6 一个具体例子：`J` 键普通三段斩

这章最关键的部分，就是把一个按钮真正拆成源码链路。

### 6.6.1 场景里先绑定按键

`SideCombatBeastPath.wt` 里，技能槽会把按钮和命令绑定起来。

你可以把它理解成：

```yaml
SkillHudSlots:
  - Key: J
    Command: side:basic
  - Key: K
    Command: side:launcher
  - Key: L
    Command: side:break_limit
```

也就是说，玩家按 `J` 之后，系统最终接收到的是 `side:basic` 这个命令或等价输入状态。

### 6.6.2 玩家服务里选择具体的攻击段

`SideCombatSystem` 会先把输入收进来，然后交给 `SideCombatPlayerService`。

在 `SideCombatPlayerService` 里，普通攻击不是永远都一样的，而是会根据连段状态选择：

- `basic1`
- `basic2`
- `basic3`
- 空中连段则可能是 `air_basic`

也就是说，`J` 不是一个动作名，而是一个“动作入口”。

### 6.6.3 `BeginPlayerAction()` 统一开始动作

真正的动作开始，会走到 `SideCombatActionService::BeginPlayerAction()`。

这一步会做这些事：

1. 找到对应的 WAO 配方。
2. 读取配方里的持续时间、命中时机、取消窗口、移动倍率。
3. 写入玩家 runtime 动作状态。
4. 播放音效。
5. 交给 WAO 执行配方效果。

一个真实的 `side.basic1` 配方大概可以长成这样：

```yaml
- id: side.basic1
  displayName: Basic Slash I
  cooldown: 0.19
  duration: 0.31
  startup: 0.06
  recovery: 0.10
  hitTime: 0.06
  cancelStart: 0.12
  cancelEnd: 0.26
  movementScale: 0.58
  tags: [Gameplay.SideCombat, Gameplay.Combat, Attack.Basic, Combo.Ground]
  signals: [side.action.start, side.action.hit]
```

这份配方的意义非常直白：

- `duration` 决定这一招持续多久。
- `hitTime` 决定什么时候出命中判定。
- `cancelStart` / `cancelEnd` 决定能不能接下一招。
- `movementScale` 决定出招时位移速度。
- `signals` 为表现系统预留了信号。

所以你会发现，普通斩击不是“动画播放一下就算了”，而是一个完整的动作对象。

### 6.6.4 这条链路最终长什么样

把这条链路压缩成一句话就是：

```text
J 键
  -> side:basic
  -> CreatePlayerBasic()
  -> BeginPlayerAction()
  -> ActionRecipe side.basic1 / side.basic2 / side.basic3
  -> Hitbox / HitResolution
  -> Combo / Damage / State / HUD
```

这就是一条最适合放进教材里的完整例子。

## 6.7 另一个具体例子：`L` 键 Break Limit

普通斩击讲的是“基础动作链”，`Break Limit` 讲的是“条件更复杂的特殊动作”。

### 6.7.1 入口仍然是技能槽

`L` 键在场景里通常会绑定到：

```text
Command: side:break_limit
```

它不是普通攻击，而是一个带资源和条件检查的特殊动作。

### 6.7.2 先检查能不能放

`CreateBreakLimit()` 不是一按就触发，它会先检查：

- 这个技能是否解锁
- 冷却是否结束
- 魔剑条是否够
- 连击数是否满足
- Boss 是否处于超级霸体
- Boss 护盾是否还存在

只有这些条件都通过，才会真的开始动作。

### 6.7.3 配方也会参与决定行为

`side.break_limit` 的配方里有这些信息：

- `cooldown: 3.00`
- `duration: 0.32`
- `resourceCost: magic_sword: 1.5`
- `tags: [Attack.BreakLimit, Boss.Protection]`

这意味着：

- 资源消耗不是写死在代码里。
- 动作语义可以从数据中读取。
- 这个动作本身就带有“破霸体、清黄条”的意图。

### 6.7.4 这类动作更像什么

`Break Limit` 很像“动作系统里的大招入口”：

- 它会改变战斗节奏。
- 它会触发镜头和表现反馈。
- 它会改写 Boss 的防御状态。

所以它特别适合用来讲：  
WAO 不只是普通攻击的数据库，它还能支撑“有规则门槛的关键动作”。

## 6.8 敌人怎么共享同一套动作链

WAO 最好的地方之一，就是它不只服务玩家，也服务敌人。

`SideCombatEnemyAIService` 会根据距离、状态、冷却和目标，挑选动作并调用 `BeginEnemyAction()`。

敌人的动作也会走同样的链路：

```text
Enemy AI
  -> 选择 ActionRecipe
  -> BeginEnemyAction()
  -> WAO 执行
  -> Hitbox / HitResolution
  -> 状态变化 / 受击反馈
```

这样做的好处很明显：

- 玩家和敌人共享一套动作语义。
- 你不会在两个系统里各写一份“攻击时序”。
- 调参时只要看配方和技能槽就够了。

这也是 WAO 和“把战斗逻辑全写进 AI 脚本”最大的区别。

## 6.9 从命中框到结果页

动作开始之后，战斗还要继续往下走。

这条链路很适合拿来给初学者建立“结果是怎么出来的”这个概念。

### 6.9.1 命中框和物理

`SideCombatHitboxService` 会在合适的时机生成命中框。  
`SideCombatPhysicsService` 会更新战斗体的位置、受击、浮空和落地。  
`SideCombatHitResolutionService` 会把碰撞转换成实际受击结果。

这三步可以理解成：

```text
动作时序
  -> 生成 hitbox
  -> 物理检测
  -> 命中结算
```

### 6.9.2 连击和资源

`SideCombatComboService` 会维护连击数和资源条。

这意味着普通攻击、空中追击、Break Limit 这些动作不是孤立的，它们彼此之间会有节奏关系。

### 6.9.3 胜负和结果转场

`SideCombatOutcomeService` 会检查：

- 敌人是不是都死了
- 是否已经胜利或失败
- 是否需要发掉落
- 是否需要记录副本结果

如果胜利，它会：

- `GameProgress::RecordDungeonClear(...)`
- `GameProgress::RecordLastDungeonResult(...)`
- 让玩家控制锁定
- 经过一段延迟后，发出结果场景命令

它真正发出的命令不是直接“硬切场景”，而是走：

```text
GameplayFlowService::TryIssueDelayedCommand()
```

这一步很重要，因为它会先给淡出和反馈留时间。

### 6.9.4 一个完整的胜利链

如果在 `SideCombatBeastPath` 里打完 Boss，链路大概是：

```text
战斗胜利
  -> SideCombatOutcomeService
  -> event:beast_path_victory
  -> sandbox_flow.wts
  -> progression:set_flag...
  -> scene:assets/scenes/Result.wt
```

这就是为什么这章要放在“玩法层”里讲：  
因为战斗不只是打起来，它还会把结果交给进度和事件流。

## 6.10 技能槽、道具槽和自定义行为

`SideCombatTuningService` 是这章很关键的配置中心。

它里面不光有基础战斗参数，还有：

- `SkillSlots`
- `CombatItemHudSlots`
- `SideUnlockProfile`
- `WaveSpawns`

### 6.10.1 技能槽

技能槽会决定：

- 哪个按键触发哪个动作入口
- 这个动作属于什么类型
- 需不需要解锁
- 能不能做自定义行为

例如：

- `Basic`
- `Launcher`
- `MagicBolt`
- `Dash`
- `AllySupport`
- `BreakLimit`
- `Custom`

`Custom` 的意义很大。
它说明你不用每加一个特殊技能都改枚举或写一堆 switch。
你可以把行为注册到 `SideCombatSkillRegistry`，然后在数据里选择它。

### 6.10.2 道具槽

道具槽也和技能槽一样，是数据驱动的。

你可以在场景里让：

- `1` 对应回血
- `2` 对应回蓝
- `3` 对应攻击增益

这对简单 2D 游戏特别实用，因为战斗系统能随着内容量增长而扩展，不用把 UI 和逻辑重新拆一遍。

### 6.10.3 波次表

`WaveModeEnabled` 和 `WaveSpawns` 说明敌人生成也能数据驱动。

这意味着你不必完全手摆敌人，也可以让运行时按波次表生成关卡内容。

## 6.11 和 Unity / UE / GAS 的对照

面试里很容易被问到这一题：

“你这个和别的引擎怎么比？”

你可以这样讲。

### Unity

Unity 里通常会把这类东西拆成：

- `Input System`
- `Animator`
- `Collider`
- `ScriptableObject`
- 自己写的状态机或第三方战斗框架

它的风格是模块化，但很多细节要靠项目自己拼。

Wheatear 的做法更像是把这条链直接写进源码，让你能看清楚从输入到结果的每一步。

### Unreal Engine / GAS

UE 的 `GAS` 思路和 WAO 很像：

- 都重视动作的意图和配方
- 都会把效果和结算独立出来
- 都希望技能不是散在各处的硬编码

但 Wheatear 的 WAO 更轻：

- 更适合 2D 和中小型项目
- 更适合源码学习
- 不追求 GAS 那种完整的网络预测和超大体系

你可以把它理解成：  
WAO 学的是 GAS 的思想，但实现上更克制、更贴合这个项目本身。

### Godot

Godot 常见做法是：

- 场景树组织战斗对象
- 脚本管理行为
- 资源文件管理数据

Wheatear 和它的相似点在于：  
都很适合把战斗做成“场景 + 数据 + 运行时”的组合。

不同点在于：  
Wheatear 强调的是 C++ 源码层的链路可读性。

## 6.12 这一层的好处和坏处

### 好处

- 战斗动作可以数据驱动
- 玩家、敌人、AI 能共享同一套动作语义
- 技能槽、道具槽、波次表都能扩展
- 结果页和进度系统能无缝接上
- 对 2D 项目来说够用，而且不会太重
- 面试时很容易讲清楚“我为什么这么拆”

### 坏处

- 配置项一多，调参会变复杂
- 动作配方、技能槽、战斗调参三者如果边界不清，容易重复定义
- 规则一多，特例会变多，代码会朝“多模块协作”方向长大
- 自定义行为如果滥用，后期可能又回到“到处有小分支”的局面

所以这层的原则不是“越抽象越好”，而是“在可理解的前提下尽量复用”。

## 6.13 常见误区

- 把 `side:basic` 直接当成“动作本体”
- 把 `SkillSlots` 和 `ActionRecipe` 混成一件事
- 让输入、动作时序、命中结算写在同一层
- 以为 `Break Limit` 只是一个按钮，其实它是一个带条件的大动作
- 忽略 `EffectLedger` 和 `ActionDebugHistory`，导致出问题时说不清动作到底做了什么
- 把结果转场写成硬切场景，没留淡出和延迟
- 让玩家和敌人各写一套攻击逻辑，失去 WAO 的复用价值

## 6.14 面试时怎么讲

你可以准备三层说法。

### 6.14.1 先讲定位

“SideCombat 是我项目里的横板战斗玩法层，WAO 是动作编排层。我把动作拆成意图、配方、执行和结算四层，这样玩家和敌人都能复用一套动作语义。”

### 6.14.2 再讲链路

“玩家按 `J` 键后，输入先变成 `side:basic`，再由玩家服务选择具体连段，动作服务从动作库里取出 `side.basic1` 这类配方，最后由命中框、物理和结果服务完成结算。”

### 6.14.3 最后讲亮点

“我没有把战斗写成一堆硬编码 if/else，而是把技能槽、动作配方、命中结算和结果转场拆开了。这样一来，新增技能或者新增敌人行为时，大部分时候只需要改数据和少量扩展点。”

## 6.15 初学者建议怎么读

如果你是第一次看这章，建议按这个顺序：

1. 先看 `SideCombatBeastPath.wt`，知道一场战斗场景里绑了什么。
2. 再看 `SideCombatComponents.h`，知道 runtime 状态和场景参数有哪些。
3. 再看 `SideCombatSystem.cpp`，知道每帧更新的总顺序。
4. 再看 `SideCombatPlayerService.cpp`，知道按键怎么变成动作。
5. 再看 `SideCombatActionService.cpp` 和 `ActionTypes.h`，知道动作怎么落到 WAO。
6. 再看 `Hitbox / HitResolution / Outcome`，知道结果怎么出来。
7. 最后看 `SideCombatTuningService.h`、`SideCombatSkillRegistry.h` 和动作 YAML，理解数据驱动和扩展点。

如果你想先抓住“结果是怎么出来的”，就只盯这三条：

- `J` 键普通斩击
- `L` 键 Break Limit
- 战斗胜利后结果页怎么跳

## 6.16 小练习

1. 在 `SkillSlots` 里加一个新的输入键，但先不绑定新行为，只让它落到一个已有动作上。
2. 在 `10_side_combat_actions.yaml` 里加一条新动作，调一调 `duration`、`hitTime`、`cancelStart`。
3. 给一个技能槽改成 `Custom`，用 `SideCombatSkillRegistry` 注册一个最简单的自定义行为。
4. 观察 `ActionDebugHistory`，看动作执行后有没有留下足够的记录。
5. 把胜利后的 `VictorySceneCommand` 改成另一个事件，确认结果流是不是走对了。

## 6.17 本章总结

`SideCombat` 讲的是“横板战斗这一层怎么组织”；  
`WAO` 讲的是“动作怎么统一编排、执行和记录”。

两者合在一起，你就能看到这套引擎对 2D 战斗的真实想法：

- 不把输入和结果硬绑在一起
- 不把动作和规则写成一坨
- 不把战斗、进度、结果页切成互相不认识的孤岛

它们是一个完整链路：

> 输入 -> 动作槽 -> 动作配方 -> 动作执行 -> 命中结算 -> 结果反馈 -> 进度/转场

这就是这章最该记住的东西。

## 6.18 下一章

下一章建议接着看 `03_TurnCombat.md`。  
因为 `SideCombat` 已经把 WAO 的核心链路讲清楚了，接下来就可以看：**同一套动作思路，怎么迁移到回合制战斗里**。
