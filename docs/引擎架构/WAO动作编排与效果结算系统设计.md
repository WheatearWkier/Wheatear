# WAO 动作编排与效果结算系统设计

更新日期：2026-08-01

## 1. 设计定位

WAO 是 Wheatear Action Orchestration 的缩写，中文名为“动作编排与效果结算系统”。

它的目标不是复刻 UE GAS，而是为 Wheatear 这种单机、2D、多玩法、编辑器驱动的引擎提供一套统一的战斗动作管理框架。

WAO 关注的问题是：
- 一个动作从哪里来。
- 这个动作是否能被执行。
- 执行时消耗什么资源。
- 在什么时间点产生命中、效果、动画事件和表现信号。
- 最终对目标属性、状态、连击、奖励和 UI 造成了什么结果。
- 这些结果如何被记录、调试、回放和编辑器可视化。

核心一句话：

```text
动作不是直接改组件，而是先形成意图，再解析规则，生成效果包，最后通过效果账本提交。
```

## 2. 和 UE GAS 的关系

WAO 借鉴了 UE GAS 的思想，但不使用 UE GAS 的结构。

借鉴的思想：
- 能力激活前需要检查条件。
- 属性、资源、状态、效果应当有统一管理入口。
- 技能不应在每个玩法里硬编码一整套。
- Buff/Debuff 不应散落在不同系统中。
- 动画、特效、音效和 UI 反馈应当跟能力流程关联。
- 标签可以用于分类、筛选和条件判断。

不照搬的部分：
- 不做 `AbilitySystemComponent` 风格的巨型组件。
- 不做 UObject/反射对象树式能力类。
- 不做网络预测和复制。
- 不要求所有玩法服从同一套 TargetActor 模型。
- 不把标签当成核心状态机。
- 不把横板浮空、战棋格子、回合队列强行抽成同一种战斗规则。

WAO 的特色是：
- `ActionIntent` 统一输入来源。
- `ActionRecipe` 作为可编辑动作配方。
- `RuleResolver` 适配不同玩法规则。
- `EffectBundle` 承载一组待执行效果。
- `EffectLedger` 记录并提交确定性效果结果。
- `SignalRouter` 把动画、音频、VFX、UI、事件脚本串起来。

## 3. 是否会和其他引擎过于相似

结论：不会和某一个引擎过于相似，但会明显体现出你吸收了成熟引擎的设计思想。

WAO 的设计综合了几类成熟方案的优点：
- UE GAS 的能力、属性、效果、标签、表现 Cue 思想。
- Unity 数据资产式技能表的内容生产便利性。
- Godot 信号与状态机的流程组织思路。
- RPG Maker/传统 RPG 数据库的技能、状态、公式表思想。
- Cocos Creator / 2D 商业项目常见的轻量数据驱动战斗配置。

但 WAO 的核心组织方式是 Wheatear 自己的：

```text
ActionIntent -> ActionRecipe -> RuleResolver -> EffectBundle -> EffectLedger -> SignalRouter
```

这条管线不是常见引擎的标准结构。尤其是 `RuleResolver` 和 `EffectLedger` 是 WAO 区分其他方案的重点：前者让四种玩法保留自己的规则骨架，后者让战斗事实统一提交、记录和调试。

### 和 UE GAS 的区别

UE GAS 更像：

```text
Actor
└── AbilitySystemComponent
    ├── GameplayAbility
    ├── GameplayEffect
    ├── AttributeSet
    └── GameplayTag
```

WAO 更像：

```text
ActionIntent
    ↓
ActionRecipe
    ↓
RuleResolver
    ↓
EffectBundle
    ↓
EffectLedger
    ↓
SignalRouter
```

UE GAS 的重点是“角色拥有能力组件，能力对象驱动效果”。

WAO 的重点是“动作意图经过规则编排后形成效果事务，再统一提交和表现”。

### 和 Unity 常见技能系统的区别

Unity 常见实现通常是：

```text
ScriptableObject Skill
MonoBehaviour SkillCaster
Buff List
```

这种方案数据驱动很直观，但容易出现：
- 伤害在一个脚本里改。
- Buff 在另一个脚本里改。
- 冷却在 UI 或角色控制器里改。
- 动画事件直接调用某个具体对象方法。

WAO 要求动作结果先进入 `EffectLedger`，再统一提交。这样更容易调试、测试、回放和做编辑器工具。

### 和 Godot 信号/状态机的区别

Godot 很擅长节点、信号和状态机。WAO 也会用信号思想，但 WAO 不把“谁收到信号谁自己改状态”作为核心。

WAO 的效果变化有账本：

```text
Signal 可以广播表现。
EffectLedger 才能改变战斗事实。
```

这能避免战斗状态被各种信号回调悄悄改乱。

### 和 RPG Maker / 数据库式技能表的区别

RPG Maker 类工具强调数据库技能、状态和公式。WAO 也重视数据表，但 WAO 还要支持：
- 横板动作命中帧。
- 浮空和断限。
- 战棋格子目标。
- 回合制行动。
- 动画事件和特效时序。
- 编辑器可视化动作流程。

所以 WAO 更像“动作编排管线”，不是单纯技能数据库。

### 和 Cocos Creator / 常见 2D 手游项目的区别

Cocos Creator 和很多 2D 商业项目常见做法是：

```text
配置表 SkillConfig
脚本 SkillController
BuffManager
特效/音效直接在技能脚本里播放
```

这种做法轻量、直接、产能高，很适合固定玩法项目。但缺点是项目一大之后容易出现：
- 技能、Buff、表现、UI 冷却各自一套逻辑。
- 横板、回合制、战棋很难共用规则。
- 调试时很难知道一次技能到底改了哪些战斗事实。
- 表现逻辑和结算逻辑容易互相穿插。

WAO 的优势是把结算和表现分开：

```text
EffectLedger 负责战斗事实。
SignalRouter 负责表现。
```

这样更适合 Wheatear 这种“引擎 + 多玩法模板”的目标，而不是只服务一个固定游戏。

WAO 的代价是前期架构成本更高，不能像简单脚本那样随手写一个技能就完事。它适合你现在这种希望长期维护、做编辑器、做多种玩法的引擎项目。

### 和 GameMaker / Construct 事件表式方案的区别

GameMaker 和 Construct 这类工具常见优势是事件表直观、上手快、非常适合 2D 小游戏。

典型方式是：

```text
On Key Pressed
  -> Create Hitbox
  -> Reduce HP
  -> Play Effect
```

这种方式的优点：
- 非常快。
- 非程序用户容易理解。
- 小项目迭代舒服。

缺点：
- 当技能、状态、装备、好感度、动画事件、连击、Boss 保护条都混在一起时，事件表会变得难以追踪。
- 很难保证伤害、状态、冷却的提交顺序。
- 很难形成跨玩法复用的底层框架。

WAO 不追求事件表式“马上能做”，而追求“动作管线可追踪、可调试、可长期扩展”。编辑器可以借鉴事件表的可视化体验，但底层仍然使用 `EffectLedger` 管理事实。

### 和 Unity DOTS / ECS 战斗框架的区别

Unity DOTS 或纯 ECS 风格通常会把能力执行拆成数据组件和系统批处理：

```text
AbilityRequestComponent
CooldownComponent
DamageEventBuffer
StatusEffectComponent
```

它的优势是性能、批处理和数据布局。WAO 不以大规模并行和极限性能为第一目标，而是以单机 2D 游戏的内容生产和调试体验为第一目标。

WAO 可以和 ECS 共存，但不会把设计重心放在“所有能力都是组件批处理”上。它更强调动作意图、配方、规则解析和账本提交。

### 和 Bevy / 现代 Rust ECS 玩法系统的区别

Bevy 一类现代 ECS 引擎通常强调插件、系统调度、事件和资源。WAO 也有类似“事件”和“系统调度”的味道，但 WAO 的抽象不是通用 ECS 框架，而是专门服务战斗动作。

WAO 的优势是语义更明确：
- Action 是动作。
- Recipe 是配方。
- Resolver 是玩法适配。
- Bundle 是效果集合。
- Ledger 是提交记录。

这比纯 ECS 事件流更适合拿来做编辑器面板和策划可视化。

### 和自研商业 2D ARPG 技能系统的关系

很多商业 2D ARPG 项目都会有：
- 技能配置。
- 命中帧。
- hitbox。
- Buff 系统。
- 特效表。
- 音效表。
- 冷却和消耗。

WAO 和这类项目肯定会有相似点，因为这是动作游戏绕不开的基础能力。但 WAO 的差异在于它不是只为横板 ARPG 服务，而是要同时适配：
- 横板格斗。
- 回合制。
- 战棋。
- 类元气假玩法。
- 后续可能的弹幕玩法。

因此 WAO 的核心创新点不是“技能配置表”本身，而是：
- 用 `RuleResolver` 把不同玩法隔离开。
- 用 `EffectLedger` 统一提交战斗事实。
- 用 `SignalRouter` 统一表现。
- 用编辑器流程图呈现动作从意图到结算的全过程。

## 3.1 横向对比总结

| 对比对象 | 相似点 | WAO 的不同点 | WAO 的优点 | WAO 的代价 |
| --- | --- | --- | --- | --- |
| UE GAS | 都有能力、属性、效果、标签、表现反馈 | WAO 没有巨型 ASC，也不做网络复制；核心是 Action 管线和 Ledger | 更轻量，更适合单机 2D 和多玩法模板 | 少了 UE GAS 那种成熟网络能力和生态 |
| Unity ScriptableObject 技能系统 | 都重视数据化技能 | WAO 有 Resolver 和 Ledger，不让技能脚本直接乱改组件 | 更可调试，更适合跨玩法复用 | 实现成本比普通 SO 技能系统高 |
| Godot 信号/状态机 | 都有信号和状态流转思想 | WAO 用 Ledger 提交事实，Signal 只做表现 | 避免信号回调把战斗事实改乱 | 需要设计清晰的提交阶段 |
| RPG Maker 数据库 | 都有技能、状态、公式表 | WAO 支持动作时间线、命中帧、VFX/SFX、横板/战棋规则 | 表现力更强，能服务动作游戏 | 编辑器复杂度更高 |
| Cocos/手游技能配置 | 都有技能表、Buff、特效表 | WAO 把结算和表现分离，并支持多玩法 Resolver | 更工程化，更适合引擎化 | 前期不如直接脚本快 |
| GameMaker/Construct 事件表 | 都可做可视化流程 | WAO 底层不是事件表直接改状态，而是生成效果账本 | 长期维护和调试更稳 | 上手门槛更高 |
| ECS 战斗框架 | 都可用事件/组件处理战斗 | WAO 是战斗语义框架，不追求纯数据批处理 | 更适合编辑器和策划理解 | 极限性能不是第一优势 |

## 3.2 面试表达重点

如果被问“是不是像 UE GAS”，可以回答：

> 我确实学习了 UE GAS 的核心思想，包括能力激活、属性、效果、标签和表现反馈。但我的项目不是多人 3D 网络游戏，而是单机 2D、多玩法、编辑器驱动的自研引擎，所以我没有照搬 ASC 架构。
>
> 我把系统重新设计成 WAO：动作编排与效果结算系统。它以 ActionIntent 统一输入来源，以 ActionRecipe 做数据化动作配方，用 RuleResolver 适配不同玩法，再用 EffectLedger 统一提交和记录战斗事实。这样既吸收了 GAS 的思想，又能适合横板、回合制、战棋和弹幕这类差异很大的 2D 玩法。

如果被问“和 Unity 技能系统有什么区别”，可以回答：

> Unity 项目里常见的是 ScriptableObject 技能配置加 MonoBehaviour 执行器，这种方案很直观。但我的 WAO 更重视确定性结算和调试。技能不会直接修改角色组件，而是生成 EffectBundle，进入 EffectLedger 后统一提交。这样我可以清楚追踪一次动作造成的所有属性变化、状态变化和表现信号。

如果被问“创新点是什么”，可以回答：

> 创新点不是凭空发明一个没人见过的概念，而是针对自己的引擎目标重新组合成熟思想。我把动作游戏的命中帧、RPG 的属性状态、战棋的目标规则、事件脚本和动画事件整合进一条 Action 管线。RuleResolver 保证玩法差异不被过度抽象，EffectLedger 保证战斗事实可追踪，SignalRouter 保证表现层和结算层解耦。

## 4. 核心对象

### 4.1 ActionIntent

动作意图表示“某个来源想执行某个动作”。

来源可以是：
- 玩家输入。
- AI。
- 事件脚本。
- 编辑器调试按钮。
- 动画事件。
- 剧情触发。

建议字段：

```cpp
struct ActionIntent
{
    UUID Actor = 0;
    std::string ActionId;
    UUID ExplicitTarget = 0;
    glm::vec2 WorldPoint = {};
    std::string InputId;
    std::string Source;
};
```

设计原则：
- 输入层只提交意图，不直接施加效果。
- AI 和玩家输入走同一入口。
- 编辑器可以模拟提交意图，方便调试。

### 4.2 ActionRecipe

动作配方描述一个动作如何执行。

它可以来自 YAML、编辑器资源、内置 C++ 默认库，后续也可以由剧情脚本临时生成。

建议字段：

```yaml
id: side.launcher
displayName: 上挑
icon: assets/vertical_slice/side_combat/ui/icons/skill_launcher.png

activation:
  input: launcher
  cooldown: 0.38
  resource:
    magic_sword: 0.0
  blockedBy:
    - State.Stunned
    - State.Dead

targeting:
  rule: side.hitbox_forward
  team: enemy

timeline:
  animation: launcher
  duration: 0.42
  events:
    - time: 0.12
      emit: hit

effects:
  - type: damage
    formula: side.attack_scaled
    scale: 1.15
    flat: 10
  - type: launch
    velocity: [0.0, 8.4]
    hitStun: 0.34
  - type: protection_gain
    amount: 22

signals:
  onStart:
    - sfx: side_sword_start
  onHit:
    - vfx: vfx_launcher_slash
    - feedback: hit_pause_light
    - camera_shake: light
```

### 4.3 RuleResolver

规则解析器把通用动作配方落到具体玩法规则上。

公共 WAO 不直接知道“横板浮空怎么做”或“战棋格子怎么判定”。

每种玩法注册自己的 Resolver：
- `SideActionResolver`
- `TurnActionResolver`
- `TacticalActionResolver`
- `ArcadeActionResolver`

Resolver 负责：
- 判断目标是否合法。
- 计算具体命中范围。
- 处理玩法专属状态。
- 把配方转换成 `EffectBundle`。
- 决定动画事件帧如何触发效果。

例子：

```text
side.launcher
    ↓
SideActionResolver
    ↓
生成横板 hitbox、浮空、hit stun、保护条增长、空中连击收益
```

```text
tactical.aether_lance
    ↓
TacticalActionResolver
    ↓
检查格子距离、占位、阵营、行动次数，生成远程魔法伤害和破甲效果
```

### 4.4 EffectBundle

效果包是一组准备提交的效果。

常见效果类型：
- `Damage`
- `Heal`
- `ModifyAttribute`
- `AddState`
- `RemoveState`
- `AddTag`
- `RemoveTag`
- `StartCooldown`
- `ConsumeResource`
- `Launch`
- `HitStun`
- `ResetJump`
- `ResetAirActions`
- `ModifyProtection`
- `SpawnProjectile`
- `SpawnPickup`
- `EmitSignal`

一个动作可以生成多个效果。

例如断限追击：

```text
Damage
ModifyProtection(-BreakLimitProtectionReduce)
ResetJump
ResetAirActions
ResetAirHang
AddComboScore
EmitSignal(vfx_break_limit)
EmitSignal(hit_pause_heavy)
```

### 4.5 EffectLedger

效果账本是 WAO 的核心特色。

它记录：
- 谁发起动作。
- 对谁生效。
- 检查是否通过。
- 消耗了什么。
- 冷却是否启动。
- 造成了多少伤害。
- 施加了哪些状态。
- 触发了哪些表现信号。
- 是否被免疫、闪避、格挡、保护条中断。

提交流程：

```text
BuildLedger
    ↓
Validate
    ↓
Apply
    ↓
EmitSignals
    ↓
StoreDebugRecord
```

这样可以解决：
- 技能消耗了但没有命中。
- 状态重复叠加规则不清。
- UI 显示和真实数值不一致。
- 动画事件触发后不知道是谁改了血量。
- 回放和战斗调试困难。

### 4.6 AttributeStore

属性仓库提供统一属性读写入口。

基础属性：
- `Health`
- `MaxHealth`
- `Mana`
- `MaxMana`
- `Attack`
- `Magic`
- `Defense`
- `Speed`

横板扩展：
- `MagicSwordGauge`
- `AirHeight`
- `Protection`
- `Poise`

战棋扩展：
- `MoveRange`
- `AttackRange`
- `ActionPoint`

规则：
- 公共属性走统一接口。
- 玩法专属属性可以注册扩展 key。
- UI 和公式不要直接到处手写字段名。

### 4.7 StateRegistry

状态注册表管理运行时状态。

状态包括：
- `State.Dead`
- `State.Stunned`
- `State.Guarding`
- `State.Burning`
- `State.Regeneration`
- `State.DefenseDown`
- `State.Invulnerable`
- `State.Launched`
- `State.SuperArmor`

注意：标签可以帮助查状态，但状态本身不能只是字符串。

状态应当有：
- Id
- DisplayName
- RemainingTime 或 RemainingTurns
- StackCount
- Power
- Harmful
- Source
- TickPolicy

### 4.8 SignalRouter

表现信号不直接改战斗事实。

它负责：
- 音效。
- BGM。
- VFX。
- 动画切换。
- UI 飘字。
- 冷却图标刷新。
- hit pause。
- 镜头震动。
- 事件脚本命令。

原则：

```text
EffectLedger 决定事实。
SignalRouter 决定表现。
```

## 5. 和现有四种玩法的关系

### 5.1 横板战斗

WAO 统一：
- 技能配方。
- 冷却。
- 魔剑槽消耗。
- 伤害公式。
- Buff/Debuff。
- 命中表现信号。

横板模块保留：
- 输入手感。
- 一段跳。
- 滞空。
- 浮空。
- 断限追击。
- Boss 保护条。
- hitbox 扫描。
- 纵深移动。

建议迁移顺序：
1. 把 `SideAttackTuning` 映射成 `ActionRecipe`。
2. 把冷却和魔剑槽消耗接入 WAO。
3. 把命中后伤害、hit stun、launch、保护条变化封装成 `EffectBundle`。
4. 保留 `SideCombatPhysicsService`、`SideCombatComboService`、`SideCombatHitboxService` 作为横板 Resolver 的下游服务。

### 5.2 回合制战斗

WAO 统一：
- 技能定义。
- MP 消耗。
- 状态效果。
- 伤害/治疗。
- 防御、再生、燃烧、破甲。

回合制模块保留：
- 回合顺序。
- 敌我行动。
- 指令菜单。
- 目标选择页面。

建议优先把 `TurnCombatSkillService` 中的状态效果逻辑迁到 WAO。

### 5.3 战棋战斗

WAO 统一：
- 技能定义。
- 状态效果。
- 伤害/治疗。
- 表现信号。

战棋模块保留：
- 格子。
- 移动范围。
- 占位。
- 行动次数。
- 敌方 AI 回合。

战棋的 `TacticalCombatBoardService` 不应该进入 WAO，但 WAO 可以调用它验证目标合法性。

### 5.4 类元气假玩法

WAO 统一：
- 武器动作配方。
- 投射物动作的消耗、冷却和表现信号。
- 伤害、回血、Buff。

类元气模块保留：
- 投射物飞行。
- Boss 跳跃。
- 遮挡物。
- 房间触发。

## 6. 编辑器设计

WAO 应当有独立编辑器面板：

```text
Window / Gameplay / WAO Action Editor
```

面板结构：

```text
左侧：Action 列表
中间：动作流程图
右侧：属性详情
底部：调试与预览
```

### 6.1 动作流程图

流程图节点：
- Intent
- Condition
- Cost
- Cooldown
- Targeting
- Timeline
- Effect
- Signal

示例：

```text
Input U
  ↓
CanActivate
  ↓
Consume Resource
  ↓
Play Animation
  ↓
Hit Frame
  ↓
Damage + Launch
  ↓
VFX + SFX + HitPause
```

### 6.2 效果编辑器

效果编辑应当尽量少让用户写文本。

控件：
- 效果类型下拉框。
- 属性名下拉框。
- 数值输入。
- 持续方式选择：即时、持续秒数、持续回合。
- 叠加规则选择。
- 是否有害。
- tooltip 预览。

### 6.3 调试面板

运行时调试显示：
- 当前 Actor 的属性。
- 当前状态列表。
- 当前冷却。
- 最近 20 条 EffectLedger。
- 最近触发的 Signal。
- 当前执行中的 Action。

这会是很好的引擎展示点。

## 7. 数据文件建议

建议新建：

```text
assets/gameplay/actions/
assets/gameplay/effects/
assets/gameplay/tags/
assets/gameplay/attributes/
```

动作文件示例：

```yaml
id: side.break_limit
displayName: 断限追击
tags:
  - Ability.Side.AirCombo
  - Ability.Side.BreakLimit

activation:
  cooldown: 3.0
  requiredState:
    - State.Airborne
  requiredResource:
    magic_sword: 1.0

targeting:
  rule: side.current_airborne_enemy

effects:
  - type: damage
    formula: side.attack_scaled
    scale: 0.72
    flat: 6
  - type: modify_protection
    value: -38
  - type: reset_air_actions
  - type: reset_jump
  - type: hit_stun
    duration: 0.26

signals:
  onCommit:
    - vfx: vfx_break_limit
    - sfx: side_break_limit
    - feedback: hit_pause_heavy
    - camera_shake: heavy
```

## 8. C++ 目录建议

建议目录：

```text
Wheatear/src/Wheatear/Gameplay/Action/
├── ActionIntent.h
├── ActionRecipe.h
├── ActionRuntime.h
├── ActionRunner.h
├── ActionResolver.h
├── EffectBundle.h
├── EffectLedger.h
├── AttributeStore.h
├── StateRegistry.h
├── SignalRouter.h
└── ActionDatabase.h
```

编辑器目录：

```text
WheatearEditor/src/Panels/WAO/
├── WAOActionEditorPanel.h
├── WAOActionEditorPanel.cpp
├── WAODebugPanel.h
└── WAODebugPanel.cpp
```

玩法适配目录：

```text
Wheatear/src/Wheatear/Modules/SideCombat/SideCombatActionResolver.*
Wheatear/src/Wheatear/Modules/TurnCombat/TurnCombatActionResolver.*
Wheatear/src/Wheatear/Modules/TacticalCombat/TacticalCombatActionResolver.*
Wheatear/src/Wheatear/Modules/ArcadeCombat/ArcadeCombatActionResolver.*
```

## 9. 实施路线

### 第一阶段：轻量核心

目标：先把重复的状态效果和属性操作抽出来。

内容：
- `ActionIntent`
- `AttributeStore`
- `RuntimeState`
- `EffectBundle`
- `EffectLedger`
- `GameplayEffect` 基础类型
- 回合制和战棋共享状态效果逻辑

不做：
- 完整编辑器流程图。
- 全量横板迁移。
- 复杂资源数据库。

当前已落地：
- `Wheatear/Gameplay/Action/ActionTypes.h/.cpp`
  - `ActionIntent`
  - `ActionRecipe`
  - `AttributeStore`
  - `RuntimeState`
  - `EffectSpec`
  - `EffectBundle`
  - `EffectLedger`
- `Wheatear/Gameplay/Action/StateRegistry.h/.cpp`
  - 内置状态：防御、再生、燃烧、破甲、眩晕。
  - 状态创建、叠加、移除、查询和格式化。
  - 回合 tick：再生回血、燃烧扣血、持续回合减少。
  - 防御倍率和受伤倍率计算。
- 回合制与战棋已经把原本重复的 `RuntimeStatusEffect` 迁移为 `WAO::RuntimeState`。
- `TurnCombatSkillService` 和 `TacticalCombatSkillService` 已经共用 WAO 的状态结算函数。

这代表 WAO 不再只是设计文档，而是已经进入运行时代码，并且被两个玩法实际调用。

### 第二阶段：动作配方

目标：把技能定义从硬编码服务迁到数据驱动配方。

内容：
- `ActionRecipe`
- `ActionDatabase`
- YAML 加载。
- 内置默认库兜底。
- Turn/Tactical 技能迁移。

### 第三阶段：玩法 Resolver

目标：每种玩法用自己的 Resolver 接入 WAO。

内容：
- `TurnActionResolver`
- `TacticalActionResolver`
- `SideActionResolver`
- `ArcadeActionResolver`

### 第四阶段：表现信号

目标：把音频、动画、特效、UI、hit pause、镜头震动统一走 Signal。

内容：
- `SignalRouter`
- 动画事件桥接。
- UI 飘字和冷却图标。
- Debug 面板显示最近信号。

### 第五阶段：编辑器

目标：让策划可以不写 C++ 地编辑动作。

内容：
- WAO Action Editor。
- Effect 编辑器。
- Action 流程图。
- 运行时 Ledger 调试器。
- Project Health 检查动作资源引用。

## 10. 面试表达

可以这样介绍：

> 我参考过 UE GAS 的能力激活、属性、效果、标签和表现 Cue 等思想，但没有直接复制它的 AbilitySystemComponent 架构。我的引擎是单机 2D、多玩法编辑器驱动，所以我设计了 WAO：动作编排与效果结算系统。
>
> WAO 把玩家输入、AI、事件脚本都抽象成 ActionIntent，通过数据化 ActionRecipe 描述动作，再交给玩法专属 RuleResolver 适配横板、回合制、战棋和弹幕规则。最终所有属性变化和状态效果先进入 EffectLedger，再统一提交和记录。这样我既能复用 GAS 的思想，又能让系统更适合自己的引擎：轻量、可编辑、可调试、确定性更强，也不会把不同玩法强行塞进一个巨型 AbilitySystemComponent。

核心亮点：
- 不是复刻 UE。
- 是从 UE GAS 提炼思想后重新设计。
- 更适合单机、多玩法、2D、编辑器驱动的 Wheatear。
- 有确定性账本，方便调试和回放。
- 有玩法 Resolver，避免公共层过度抽象。
- 有 ActionRecipe，方便内容生产。

## 2026-08-01 当前实现接口

WAO 目前已经有三层入口：

```text
ActionDatabase     保存 ActionRecipe
ActionOrchestrator 查 recipe 并调度 resolver
ActionResolver     由玩法模块把 recipe 转成真实规则
```

推荐新增玩法时按这个顺序接入：

1. 在玩法模块里建立 `*ActionCatalog`，把内置技能或武器映射成 `WAO::ActionRecipe`。
2. 把可配置的显示名、图标、音效、VFX、标签、效果预览、资源消耗写入 `assets/gameplay/actions/*.yaml`。
3. 如果该玩法需要把 recipe 转成运行时对象，例如投射物、格子技能、浮空 hitbox，就建立 `*ActionResolver` 并注册 action id 前缀。
4. 如果该玩法的真实结算已经由专属 `ActionService` 掌握，也可以先只把结果写入 `EffectLedger`，不要为了统一而破坏玩法规则。

### YAML Action 格式

示例：

```yaml
actions:
  - id: side.launcher
    displayName: Rising Cut
    description: Ground launcher that starts aerial combo.
    icon: assets/vertical_slice/side_combat/ui/icons/skill_launcher.png
    sound: assets/vertical_slice/side_combat/audio/side_launcher.wav
    effect: assets/vertical_slice/side_combat/effects/vfx_launcher_slash.png
    cooldown: 1.2
    startup: 0.10
    hitTime: 0.18
    recovery: 0.22
    tags: [Gameplay.SideCombat, Category.Launcher, Combo.AerialStarter]
    resourceCost:
      magic_sword: 1
    effects:
      - type: Damage
        attribute: attack
        value: 1.15
      - type: Launch
        value: 1.0
    signals: [side.action.hit, side.action.vfx, side.action.sfx]
```

当前 loader 支持字段：

- 基础：`id`、`displayName`、`description`
- 资源：`icon`、`animation`、`sound`、`effect`
- 时序：`cooldown`、`duration`、`startup`、`hitTime`、`recovery`、`cancelStart`、`cancelEnd`、`movementScale`
- 规则：`tags`、`requiredStates`、`blockedStates`、`requiredTags`、`blockedTags`
- 成本：`resourceCost`
- 效果：`effects`
- 表现：`signals`

这套格式的目的不是把所有玩法规则都写死在 YAML 里，而是让策划数据、编辑器调试、表现资产和效果账本先统一起来。
