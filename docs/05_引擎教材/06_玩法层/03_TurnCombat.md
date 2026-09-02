# 第 6 编 03：TurnCombat

> 本章所属层级：**玩法层**
>
> 本章目标：理解 Wheatear 里的回合制战斗是怎样把“命令输入、回合队列、技能配方、目标选择、状态结算、UI 展示、视觉反馈、调参和结果跳转”串成一条完整链路的。
>
> 先提醒一句：这一章不是在讲“我写了一个回合制小游戏”，而是在讲“我如何把回合制战斗拆成可以复用、可以调试、可以继续扩展的结构”。  
> `TurnCombatSystem` 负责把它接进每帧主循环，`TurnCombatFlowService` 负责回合推进，`TurnCombatCommandService` 负责把 `turn:` 命令变成战斗操作，`TurnCombatActionService` 负责真正结算，`TurnCombatUIService` 和 `TurnCombatVisualService` 负责把结果画出来。
>
> 本章会反复使用两条最典型的链路：
> 1. 玩家在战斗 UI 里点“技能”或“道具”，为什么最后会变成一次具体的攻击、治疗或防御。
> 2. 敌人 AI 为什么能自己出手，而且还能按速度、回合数和目标规则稳定跑完一整场战斗。

## 6.1 这一层在架构中的位置

`TurnCombat` 属于**玩法层**，不是核心平台层，也不是引擎服务层。
它处理的是回合制战斗规则本身：

- 一回合谁先动
- 玩家什么时候能输入
- 敌人什么时候自动行动
- 技能怎么选
- 目标怎么选
- 命中后怎么结算
- 胜负后怎么跳场景

这些都是玩法语义，不是底层语义。

如果按我们前面约定的学习顺序看，它正好是接在 `VN` 和 `SideCombat` 之后的另一种战斗样本：

```text
VN / EventScript
    ↓
SideCombat / WAO
    ↓
TurnCombat
    ↓
TacticalCombat / Arcade / Progression
```

`VN` 讲的是剧情怎么跑。  
`SideCombat` 讲的是即时战斗怎么打。  
`TurnCombat` 讲的是“回合制怎么被工程化地拆开”。

它特别适合用来讲一件事：  
**玩法层不是一个大函数，而是一组有职责边界的服务。**

## 6.2 本章先看哪些源码

建议按这个顺序看：

| 文件 | 作用 |
| --- | --- |
| `Projects/WheatearDemo/assets/scenes/TurnCombat.wt` | 示例战斗场景，先看它怎么绑实体 |
| `Projects/WheatearDemo/assets/gameplay/actions/20_turn_combat_actions.yaml` | 回合制技能的动作配方 |
| `Wheatear/src/Wheatear/Modules/TurnCombat/TurnCombatComponents.h` | 战斗场景组件、角色组件、回合状态 |
| `Wheatear/src/Wheatear/Modules/TurnCombat/TurnCombatSystem.cpp` | 每帧入口，命令收口和回合更新 |
| `Wheatear/src/Wheatear/Modules/TurnCombat/TurnCombatFlowService.cpp` | 回合推进主轴 |
| `Wheatear/src/Wheatear/Modules/TurnCombat/TurnCombatCommandService.cpp` | `turn:` 命令解析 |
| `Wheatear/src/Wheatear/Modules/TurnCombat/TurnCombatActionService.cpp` | 技能开始、执行、结算 |
| `Wheatear/src/Wheatear/Modules/TurnCombat/TurnCombatSkillService.cpp` | 技能库、伤害/治疗/状态公式 |
| `Wheatear/src/Wheatear/Modules/TurnCombat/TurnCombatTargetService.cpp` | 回合队列和目标选择 |
| `Wheatear/src/Wheatear/Modules/TurnCombat/TurnCombatUIService.cpp` | 战斗 UI |
| `Wheatear/src/Wheatear/Modules/TurnCombat/TurnCombatVisualService.cpp` | 角色动画和动作特效 |
| `Wheatear/src/Wheatear/Modules/TurnCombat/TurnCombatTuningService.h/.cpp` | 回合节奏和公式调参 |

如果你只想先抓住主链路，就先看：

`TurnCombatSystem` -> `TurnCombatFlowService` -> `TurnCombatCommandService` -> `TurnCombatActionService` -> `TurnCombatSkillService` / `TurnCombatTargetService` -> `TurnCombatUIService` / `TurnCombatVisualService`

## 6.3 这一套回合制到底是什么

`TurnCombat` 不是一个“巨大的战斗类”，而是一组服务协作出来的回合制玩法。

你可以先把它理解成这样：

```text
Scene
  └─ TurnCombatLevelComponent
        ├─ 战斗场景名、UI 绑定名、胜负命令
        └─ 运行时回合状态

TurnCombatSystem
  ├─ 收 turn: 命令
  ├─ 推动回合流程
  └─ 刷战斗 UI

TurnCombatFlowService
  ├─ ResetLevel()
  ├─ BeginNextTurn()
  └─ UpdateLevel()

TurnCombatCommandService
  ├─ menu / skill / item / target / cancel
  └─ 把按钮选择变成行动请求

TurnCombatActionService
  ├─ BeginAction()
  └─ ApplySkill()

TurnCombatSkillService
  ├─ 从 WAO 动作表构建技能库
  ├─ 算伤害 / 治疗 / 状态
  └─ 提供敌人出招规则

TurnCombatTargetService
  ├─ 排队
  ├─ 找目标
  └─ 验证目标规则

TurnCombatUIService / TurnCombatVisualService
  └─ 把战斗结果画出来
```

所以它不是“一个战斗 UI”，而是一个完整的**回合制执行管线**。

你在面试里可以直接这么讲：

> Wheatear 的 TurnCombat 把回合制战斗拆成了流程、命令、动作、技能、目标、UI 和视觉七层。  
> 这样做的好处是，战斗规则、技能配方和调参文件都能独立演进，不会全挤在一个类里。

## 6.4 场景里怎么配置一场回合制战斗

回合制战斗也是从场景开始的。
`Projects/WheatearDemo/assets/scenes/TurnCombat.wt` 里有一个很典型的配置：

```yaml
Scene: TurnCombat
SavePolicy:
  CanSave: false
  CanLoad: false
  SaveDirectory: assets/saves
  AutoLoadSlot: 0
Entities:
  - TagComponent:
      Tag: TC_Controller
    TurnCombatLevelComponent:
      PlayOnStart: true
      LevelId: CH01_TURN_MagicSwordTrial
      FadeEntityName: TC_Fade
      MessageTextEntityName: TC_MessageText
      ActiveActorTextEntityName: TC_ActiveActorText
      TurnOrderTextEntityName: TC_TurnOrderText
      SkillDetailTextEntityName: TC_SkillDetailText
      CommandPanelEntityName: TC_CommandPanel
      TargetHintTextEntityName: TC_TargetHintText
      ActionFlashEntityName: TC_ActionFlash
      ActionEffectEntityName: TC_ActionEffect
      VictorySceneCommand: "event:turn_combat_victory"
      DefeatSceneCommand: "event:turn_combat_retry"
      StartFadeDuration: 0.55
      IntroDuration: 0.80
      ActionDuration: 0.72
      VictoryReturnDelay: 1.80
      DefeatReturnDelay: 1.40
```

这个配置已经把“战斗壳子”说得很清楚了：

- 背景、BGM、UI 画布都是场景实体
- 战斗控制器只是其中一个实体
- `TurnCombatLevelComponent` 负责绑定所有关键 UI 节点
- 胜负后不是直接退出，而是发 `event:...` 命令给后续流程

战斗角色也完全是场景实体：

- `TurnCombatantComponent` 里写团队、槽位、名字、数值、技能槽
- 还写了生命条、魔力条、状态文本、目标按钮、目标标记这些绑定名
- 动画可以走帧序列，也可以走图集

这说明一件事：  
**回合制战斗不是独立于场景存在的，它就是场景里的一个玩法层。**

## 6.5 `TurnCombatSystem`：每帧入口和命令收口

`TurnCombatSystem` 是这一套的入口。

它只做两件事：

1. `OnRuntimeStart()` 时，找到所有 `TurnCombatLevelComponent`，如果 `PlayOnStart` 为真，就重置战斗。
2. `OnUpdateRuntime()` 时，先收 `turn:` 命令，再推进回合流程。

代码思路可以直接概括成这样：

```text
OnRuntimeStart()
  -> 找到 TurnCombatLevelComponent
  -> PlayOnStart ? ResetLevel() : 什么都不做

OnUpdateRuntime()
  -> CommandBus::DrainGameplayCommands("turn:")
  -> ProcessCommand(...)
  -> UpdateLevel(...)
```

这里最值得强调的是：  
`TurnCombat` 不直接去读键盘，不直接去读 ImGui，也不直接去碰按钮控件。
它只收 `turn:` 玩法命令。

这和 `VN`、`Progression` 的做法很像：  
**UI 只负责把选择变成命令，系统只负责消费命令。**

这就是为什么 `TurnCombatCommandService` 不是“按钮回调”，而是一个真正的命令解析器。

## 6.6 `TurnCombatFlowService`：回合推进的主轴

这一章最核心的代码就在 `TurnCombatFlowService`。

它有三个关键函数：

- `ResetLevel()`
- `BeginNextTurn()`
- `UpdateLevel()`

### 6.6.1 `ResetLevel()`：把战斗从头拉起来

`ResetLevel()` 会先读取调参文件，然后把战斗状态全部归零：

- `RuntimeElapsed`
- `RuntimeFadeAlpha`
- `RuntimePhase`
- `RuntimeRound`
- `RuntimeTurnIndex`
- `RuntimeActiveActor`
- `RuntimeCommandMenuPage`
- `RuntimeSelectedSkillId`
- `RuntimeActionActor / Target / SkillId`
- `RuntimeMessage`
- `RuntimeRequestedCommand`
- 各种计时器和标志位

然后它会对场景里的所有战斗单位做初始化：

- 血量和魔力回到合法范围
- 玩家阵营的单位可控
- `RuntimeAlive` 根据生命值决定
- `RuntimeGuarding`、`RuntimeStatusEffects` 清空
- `RuntimeAnimationClip`、`RuntimeAnimationTimer` 清空
- `CacheVisuals()` 和 `RestoreVisual()` 把视觉基线存起来再恢复

最后它会：

- 拉起遮罩 `FadeEntityName`
- 关闭动作闪光 `ActionFlashEntityName`
- 调 `BuildTurnQueue()`

也就是说，`ResetLevel()` 不是一个“清空数组”的函数，  
它是在把整场战斗重装成一个可运行状态。

### 6.6.2 `BeginNextTurn()`：谁该动，谁不能动

`BeginNextTurn()` 负责把下一位行动者找出来。

它的顺序大概是：

1. 先看玩家队伍有没有活人，没有就进 `Defeat`
2. 再看敌方有没有活人，没有就进 `Victory`
3. 如果回合队列空了，或者索引到头了，就重建队列并加一轮
4. 逐个取出队列中的单位
5. 跳过无效实体、死掉的单位、没数据的技能
6. 先 tick 状态效果
7. 再决定是玩家等待输入，还是敌人直接出招

这段逻辑有个很重要的特点：  
它不是按“一个函数调用一个函数”机械推进，而是按“回合规则”自然跳转。

敌人回合会直接走 AI：

- `ChooseEnemySkill()`
- `FindSkill()`
- `ChooseTargetForAI()`
- `BeginAction()`

玩家回合则进入：

- `AwaitCommand`
- 等待 `turn:` 命令

还有一个很值得记住的保险机制：  
如果一整轮里都找不到可以行动的人，它会退回到命令阶段，避免战斗卡死。

### 6.6.3 `UpdateLevel()`：把 Intro、Acting、Victory、Defeat 串起来

`UpdateLevel()` 是整场战斗每帧都在跑的节奏器。

它先处理淡入遮罩：

- `StartFadeDuration` 决定遮罩消退速度
- 遮罩 alpha 通过 `UIRuntimeTools::SetImageAlpha()` 写回到 `FadeEntityName`

然后按状态机推进：

- `Intro`：等 `IntroDuration` 到了，再进入下一回合
- `AwaitCommand`：等待玩家发命令
- `AwaitTarget`：等待玩家点目标
- `Acting`：等到 `ActionDuration * 0.42f` 时真正执行技能
- `Victory` / `Defeat`：隐藏命令面板，延迟发结果命令

这里最像教材的一点是：  
**伤害不是在“按下按钮”的那一刻结算的，而是在动作时间轴的一个固定点上结算的。**

这让“动画”和“逻辑”可以对齐，而不是谁先谁后乱掉。

## 6.7 `TurnCombatCommandService`：玩家命令怎么变成战斗操作

这一层负责把 `turn:` 命令翻译成战斗行为。

它先用 `GameplayTextService::SplitCommand()` 把字符串拆开，再按语义处理：

- `turn:menu:skills`
- `turn:menu:items`
- `turn:cancel`
- `turn:wait`
- `turn:guard`
- `turn:item:potion`
- `turn:skill:slot1`
- `turn:target:TC_Boss`

### 6.7.1 命令不是直接执行的

这里最重要的不是命令长什么样，而是命令怎么被解释。

比如：

- `menu` 只负责切 UI 菜单页
- `cancel` 负责退回上一层
- `wait` / `guard` 是直接可执行动作
- `skill` / `item` 先解析成具体技能 ID，再决定要不要选目标
- `target` 最后才真的进入 `BeginAction()`

`TurnCombatCommandService` 还会先检查：

- 当前行动者是不是玩家
- 当前阶段是不是 `AwaitCommand` 或 `AwaitTarget`
- 魔力够不够
- 目标合不合法

所以它不是单纯的按钮路由，而是一道完整的战斗输入校验关。

### 6.7.2 一些很有味道的小细节

命令服务里有两个很实用的反馈：

- `turn.audio.select`
- `turn.audio.denied`

这意味着：

- 点对了，会有轻反馈
- 选错了、蓝不够、目标无效，会有拒绝音效和提示文本

这类细节很适合在面试里说，因为它体现了你不只是“能跑通”，还考虑了交互手感。

### 6.7.3 技能槽和道具槽怎么映射

`ResolvePlayerSkillId()` 会把槽位和技能 ID 对上：

- `basic` / `slot0` -> 主技能
- `slot1` / `slot2` / `slot3` -> 角色自己的技能槽
- `item0` / `potion` -> 道具技能

也就是说，玩家看到的是 UI 按钮，  
系统内部拿到的是具体的技能 ID。

这一步很像一个“薄适配层”：

```text
UI 文案
  -> 命令文本
  -> 技能 ID
  -> 回合动作
```

## 6.8 `TurnCombatActionService` 和 `TurnCombatSkillService`

这一节讲的是：  
**战斗动作到底是怎么从“技能名称”变成“真正的数值结算”的。**

### 6.8.1 `TurnCombatSkillService`：从 WAO 动作表构建技能库

这套回合制技能不是硬编码在 C++ 里，而是从动作表里读出来的。

`TurnCombatSkillService::SkillLibrary()` 会扫描所有 `turn.*` 的 WAO 动作配方，然后把它们转换成 `TurnSkillDefinition`。

比如这些技能都来自 `assets/gameplay/actions/20_turn_combat_actions.yaml`：

- `turn.slash` -> `魔剑斩`
- `turn.aether_edge` -> `灵素剑锋`
- `turn.white_vow` -> `白誓治愈`
- `turn.shield_oath` -> `守誓之盾`
- `turn.focus_wait` -> `冥想`
- `turn.claw` / `turn.wild_pounce` / `turn.dark_orb` -> 敌人技能

动作表里会提供这些关键数据：

- 图标
- 音效
- 特效
- 分类
- 目标规则
- 魔力消耗
- 伤害系数
- 治疗系数
- 防御穿透
- 状态效果
- 特效帧数和帧率

可以把一条技能理解成这样：

```yaml
- id: turn.aether_edge
  displayName: 灵素剑锋
  category: Skill
  targetRule: EnemySingle
  manaCost: 8
  power: 1.45
  magic: true
  statusEffect: DefenseDown
  effectTurns: 2
```

`TurnCombatSkillService` 再把这些信息变成 C++ 里的技能定义。

### 6.8.2 `BeginAction()`：先进入动作态，再进结算

`TurnCombatActionService::BeginAction()` 做的事情很明确：

- 把 `RuntimePhase` 设成 `Acting`
- 清动作计时器
- 记录动作发起者、目标、技能 ID
- 清掉当前选中的技能和菜单页
- 隐藏命令面板
- 给 UI 写一条提示：`某某准备使用某某`

也就是说，`BeginAction()` 只是把战斗推到“动作播放中”这个状态，  
真正的伤害、治疗和状态变化要等到 `ApplySkill()`。

### 6.8.3 `ApplySkill()`：真正的结算发生在这里

`ApplySkill()` 是这一章最关键的函数之一。

它会按这个顺序处理：

1. 通过实体 ID 找回 actor 和 target
2. 通过 `skillId` 找到对应的技能定义
3. 通过 `WAO::ComposeActionId("turn", skill.Id)` 找到动作配方
4. 建立 `WAO::ActionIntent`
5. 开始记录 `EffectLedger`
6. 扣魔力
7. 特判 `focus_wait`
8. 特判 `guard`
9. 普通技能则按目标规则收集目标
10. 对每个目标执行治疗、伤害、状态附加
11. 记录调试历史

几个最有教学价值的特殊分支：

- `focus_wait`：回一点魔力，作为“等待/冥想”动作
- `guard`：进入防御状态，回一点魔力，同时应用防御状态
- `skill->HealPower > 0`：走治疗结算
- `skill->Power > 0`：走伤害结算

结算时还会把结果写进 `WAO::EffectLedger` 和 `WAO::ActionDebugHistory`，  
这样后面的调试面板能看到“这一招到底怎么结的账”。

### 6.8.4 公式不是硬写死的

`TurnCombatSkillService` 里真正重要的是这几个通用概念：

- `CalculateDamage()`
- `CalculateHeal()`
- `GetDefenseMultiplier()`
- `GetDamageTakenMultiplier()`
- `ApplyStatusEffect()`
- `TickStatusEffects()`

伤害公式里会读：

- 攻击力还是魔力
- 防御穿透
- 状态效果
- 调参文件里的防御倍率

治疗公式则更简单，基本就是魔力乘系数再加一点基底值。

你可以把这章的设计理解成：

```text
动作表负责“技能是什么”
战斗服务负责“这招怎么结算”
WAO 负责“结算日志怎么记”
```

## 6.9 `TurnCombatTargetService`、`TurnCombatUIService`、`TurnCombatVisualService`

这一节讲“战斗怎么被看见”。

### 6.9.1 `TurnCombatTargetService`：队列、目标和合法性

`TurnCombatTargetService` 做三件事：

1. 收集战斗单位
2. 排回合队列
3. 找合法目标

`CollectCombatants()` 会拿所有 `TurnCombatantComponent`，然后按团队、槽位、速度排序。

`BuildTurnQueue()` 会按速度从高到低排队，如果速度一样，就按槽位。
这就是回合制里“谁先手”的基础。

`ResolveTargets()` 则负责把目标规则翻译成真实实体：

- `Self`
- `EnemySingle`
- `EnemyAll`
- `AllySingle`
- `AllyAll`

`FindTurnTarget()` 还支持通过按钮名、标记名、实体名找对象，所以 UI 按钮并不需要直接知道战斗逻辑。

### 6.9.2 `TurnCombatUIService`：战斗 UI 不是写死的

`TurnCombatUIService` 负责把“状态”写回到界面。

它主要更新这些东西：

- 当前行动者文本
- 回合顺序文本
- 每个单位的血条和魔力条
- 状态文本
- 命令面板
- 目标提示
- 技能说明

命令面板的默认结构是五个槽位：

- 攻击
- 防守
- 技能
- 道具
- 取消

如果进入技能页，就显示该角色的技能槽；  
如果进入道具页，就显示道具和返回；  
如果进入选目标阶段，就只显示返回和高亮目标。

换句话说：  
**战斗 UI 不是静态菜单，而是一个跟着回合状态机变化的面板。**

### 6.9.3 `TurnCombatVisualService`：攻击、受击、死亡、动作闪光

`TurnCombatVisualService` 会把战斗状态直接写到实体视觉上。

它先做两件很基础的事：

- `CacheVisuals()`：缓存角色原始位移和缩放
- `RestoreVisual()`：把它们恢复回去

然后它会根据状态决定用哪个动画：

- `idle`
- `attack`
- `hit`
- `down`

动画既可以来自帧序列，也可以来自图集。
如果 `TextureAtlasFrameSpec` 有效，就先走图集；否则就走普通帧路径。

还有几个很明显的视觉反馈：

- `RuntimeHitFlashTimer` 在受击时高亮角色
- `RuntimeAlive` 变成 false 时，角色会变暗
- `Acting` 状态下，出手角色会有轻微位移和缩放脉冲
- 动作特效实体会跟着目标移动，并在动作时间轴中同步出现

你可以把这一层记成一句话：

```text
逻辑告诉它“谁在出手”，视觉层就把“谁在出手”画成动作。
```

## 6.10 `TurnCombatTuningService`：节奏和公式怎么调

这一层把回合节奏从场景里拆出来，放到了 `assets/sandbox/data/turn_combat_tuning.yaml`。

它管的主要是两类数值：

- 回合节奏：`StartFadeDuration`、`IntroDuration`、`ActionDuration`、`VictoryReturnDelay`、`DefeatReturnDelay`
- 公式参数：例如防御倍率、最小伤害等

它的特点是：

- 按路径缓存
- 通过文件修改时间热重载
- 场景组件字段只作为 fallback

这意味着：

- 你可以在场景里给出一套默认值
- 也可以把整场战斗的节奏移到全局调参文件里
- 调试时改 YAML，不用频繁改 C++

这就是为什么 `TurnCombatLevelComponent` 里会有一部分字段看起来像“可写”，但真正运行时可能被 tuning 覆盖。

如果你在编辑器里看到某些字段不能直接改，那通常就是被 `IsFieldManagedByTuning()` 视为调参项了。

## 6.11 一个完整例子：从打开战斗场景到胜负跳转

把这一章串成一个完整例子，大概是这样：

```text
打开 Projects/WheatearDemo/assets/scenes/TurnCombat.wt
  -> 场景里有 FlowController、TC_Controller、TC_Hero、TC_WhiteMage、敌方单位等
  -> TurnCombatSystem::OnRuntimeStart()
  -> ResetLevel()
  -> BuildTurnQueue()
  -> Intro 结束
  -> BeginNextTurn()
  -> 如果轮到玩家，进入 AwaitCommand
  -> UI 显示“攻击 / 防守 / 技能 / 道具 / 取消”
  -> 玩家点“技能”
  -> turn:menu:skills
  -> 玩家选“灵素剑锋”
  -> turn:skill:aether_edge
  -> 进入 AwaitTarget
  -> 玩家点目标
  -> turn:target:TC_Boss
  -> BeginAction()
  -> Acting
  -> 约在 ActionDuration * 0.42f 时真正 ApplySkill()
  -> 伤害、治疗、状态和音效全部结算
  -> 继续 BeginNextTurn()
  -> 如果敌人全倒下，进入 Victory
  -> 延迟发 `event:turn_combat_victory`
  -> 交给后续 EventScript / SceneTransition 去跳下一段
```

这个例子很适合面试时讲，因为它把“看起来很简单的回合制 UI”拆成了完整链路。

## 6.12 和 Unity、UE、Godot 怎么对照

### Unity

Unity 里通常会对应到：

- `GameManager` 或 `BattleManager`
- 一组 `ScriptableObject` 技能表
- 角色 UI prefab
- 一个回合状态机

Wheatear 的思路更偏服务化：  
回合流程、命令、技能、目标、UI、视觉都拆开了，不把所有东西都塞进一个 manager。

### Unreal

UE 里可以类比成：

- Widget + 战斗逻辑蓝图
- 数据资产 / DataTable
- 回合控制器
- 动作效果和状态系统

Wheatear 这里的 `WAO` 有点像把“动作资产 + 结算日志”从战斗逻辑里独立了出来。

### Godot

Godot 里通常会是：

- 节点树里的战斗控制节点
- 资源文件里的技能定义
- 信号驱动的命令流

三者思路都一样：  
**战斗不是一个脚本文件，而是“状态机 + 数据 + UI + 视觉”的组合。**

## 6.13 这一层的优点、坏处和常见误区

### 优点

- 回合流程清楚，玩家输入和自动出手都很稳定
- 技能、目标、状态和表现是分开的
- 技能表可数据驱动，动作可热更新
- 结算日志和调试历史好追
- UI、视觉和逻辑没有揉成一团

### 坏处

- 命令和状态很多，初学者容易跟丢
- 需要维护一整套 UI 绑定名
- 场景、技能表、调参文件、动作表之间有耦合
- 角色数据、动作表和视觉资源一旦缺失，问题会很分散

### 常见误区

- 把 `TurnCombatSystem` 当成一个“大战斗类”
- 以为玩家点按钮就是直接执行伤害
- 忘了 `AwaitCommand` 和 `AwaitTarget` 是两个不同阶段
- 以为回合顺序只看速度，不看队列重建和存活状态
- 以为技能只是图标和名字，忽略 WAO 动作配方
- 忘了敌人 AI 也走同一套技能库
- 忘了 `turn:` 命令是先被命令总线收口，再进入战斗系统

## 6.14 面试时怎么讲

如果面试官问你：“Wheatear 的回合制战斗是怎么做的？”

你可以这样答：

```text
TurnCombat 是一套分层的回合制系统。
TurnCombatSystem 负责收 turn: 命令并驱动更新，
TurnCombatFlowService 负责回合推进和胜负判断，
TurnCombatCommandService 负责把 UI 选择翻译成命令，
TurnCombatActionService 负责真正的动作结算，
TurnCombatSkillService 从 WAO 动作表里构建技能库，
TurnCombatTargetService 负责排队和目标规则，
UI 和视觉服务只负责把结果画出来。
```

如果面试官继续问：“为什么要这么拆？”

你可以补一句：

```text
因为回合制特别容易把流程、UI、数值和动作混在一起。
拆开以后，技能表能热更新，回合节奏能调，目标规则能改，UI 也能单独换。
```

如果继续追问：“一招是怎么结算的？”

你就说：

```text
命令先把玩家选中的技能变成技能 ID，
`BeginAction()` 进入动作态，
到动作时间轴的固定点再 `ApplySkill()`，
然后按目标规则计算伤害、治疗和状态，最后记录到 WAO 的结算日志里。
```

## 6.15 小结

这一章其实只讲了一件事：

```text
回合制战斗不是一个按钮面板，
而是一整套由状态机、命令、动作、技能、目标、UI 和视觉组成的玩法系统。
```

Wheatear 在这一层做了几件很关键的事：

- 用 `TurnCombatSystem` 把它接进主循环
- 用 `TurnCombatFlowService` 把回合流程拆清楚
- 用 `TurnCombatCommandService` 把 UI 选择变成命令
- 用 `TurnCombatActionService` / `TurnCombatSkillService` 把动作和结算拆开
- 用 `TurnCombatTargetService` / `TurnCombatUIService` / `TurnCombatVisualService` 把目标、界面和视觉接上

把这一章看懂以后，你后面再看：

- `Progression`
- `SceneTransition`
- `GameProgress`
- 以及后面的战术战斗和街机模式

就会顺很多。  
因为你会知道：

```text
玩法层最重要的，不是“我有没有一个系统”，
而是“我的系统能不能把命令、数值、界面和表现稳定串起来”。
```
