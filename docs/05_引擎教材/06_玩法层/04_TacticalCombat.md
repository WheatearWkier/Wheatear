# 第 6 编 04：TacticalCombat

> 本章所属层级：**玩法层**
>
> 本章目标：理解 Wheatear 里的战棋玩法是怎样把“棋盘、单位、命令、技能、AI、演出、调参、编辑器”串成一条完整链路的。
>
> 先提醒一句：这里的 `TacticalCombat` 不是“一个战棋 Demo 面板”，而是一套**完整的格子战斗玩法**。  
> 它把棋盘当成场景里的一个运行时系统，把单位当成场景实体，把技能和行动当成可调的动作，把 UI 当成命令入口，再用数据驱动的调参文件把这一切都接起来。
>
> 如果说 `VN` 讲的是剧情怎么跑，`TurnCombat` 讲的是回合制怎么跑，那么 `TacticalCombat` 讲的就是：  
> **一个简单 2D 游戏里的战棋战斗，如何被拆成清晰、可调、可复用的工程结构。**

## 6.1 这一层在架构里的位置

`TacticalCombat` 属于**玩法层**，不是核心平台层，也不是引擎服务层。
它负责的不是底层窗口、输入、渲染，也不是通用 ECS，而是战棋玩法本身：

- 棋盘格子怎么定义
- 单位怎么摆放
- 移动范围怎么算
- 攻击和技能怎么选
- 敌人 AI 怎么行动
- 伤害、治疗、状态怎么结算
- UI 怎么把这些选择展示出来
- 胜负后怎么退出到下一段流程

这层特别适合简单 2D 游戏。
因为很多 2D 战棋项目最后真正要跑的，不是复杂的战术模拟，而是：

- 一个清晰的格子地图
- 一组可控单位
- 一套可调的技能配方
- 一套稳定的回合或敌方行动节奏
- 一套能回到剧情或进度系统的胜负出口

你可以把这一层理解成：

```text
CommandBus
    ↓
TacticalCombatSystem
    ├─ Board
    ├─ Flow
    ├─ Command
    ├─ Skill
    ├─ Action
    ├─ AI
    ├─ UI
    └─ Visual
```

所以它不是“UI 页面”，而是**完整的战棋执行管线**。

## 6.2 本章先看哪些源码

建议按这个顺序看：

| 文件 | 作用 |
| --- | --- |
| `Wheatear/src/Wheatear/Modules/TacticalCombat/TacticalCombatComponents.h` | 战棋场景组件、单位组件、运行时状态 |
| `Wheatear/src/Wheatear/Modules/TacticalCombat/TacticalCombatSystem.cpp` | 每帧入口，收命令、推进流程 |
| `Wheatear/src/Wheatear/Modules/TacticalCombat/TacticalCombatBoardService.cpp` | 棋盘坐标、占位、范围、寻路式移动 |
| `Wheatear/src/Wheatear/Modules/TacticalCombat/TacticalCombatFlowService.cpp` | 回合与流程主轴 |
| `Wheatear/src/Wheatear/Modules/TacticalCombat/TacticalCombatCommandService.cpp` | `tactic:` 命令解析 |
| `Wheatear/src/Wheatear/Modules/TacticalCombat/TacticalCombatSkillService.cpp` | 技能从 WAO 动作表生成、伤害公式、状态效果 |
| `Wheatear/src/Wheatear/Modules/TacticalCombat/TacticalCombatActionService.cpp` | 行动开始、应用、收尾 |
| `Wheatear/src/Wheatear/Modules/TacticalCombat/TacticalCombatAIService.cpp` | 敌方自动行动 |
| `Wheatear/src/Wheatear/Modules/TacticalCombat/TacticalCombatUIService.cpp` | 命令面板、状态面板、格子高亮 |
| `Wheatear/src/Wheatear/Modules/TacticalCombat/TacticalCombatVisualService.cpp` | 单位动画、行动特效 |
| `Wheatear/src/Wheatear/Modules/TacticalCombat/TacticalCombatTuningService.h/.cpp` | YAML 调参、热重载、场景覆写 |
| `WheatearEditor/src/Modules/TacticalCombat/TacticalCombatTuningEditorPanel.cpp` | 战棋调参编辑器 |

如果你只想先抓主链路，就先看：

`TacticalCombatSystem` -> `TacticalCombatCommandService` -> `TacticalCombatFlowService` -> `TacticalCombatActionService` -> `TacticalCombatAIService`

## 6.3 `TacticalCombat` 到底是什么

这套模块不是“把一堆单位摆到格子上就完了”。
它更像一个**场景内战棋执行器**：

```text
Scene
  └─ TacticalCombatLevelComponent
        ├─ 棋盘尺寸、格子原点、格子大小
        ├─ UI 绑定名
        ├─ 胜负命令
        ├─ 调参路径
        └─ 运行时状态

TacticalUnitComponent
  └─ 每个单位的站位、数值、技能槽、动画、运行时状态

TacticalCombatSystem
  └─ 每帧推进战棋
```

这一层的工程重点是：

1. 棋盘是数据，不是硬编码。
2. 单位是场景实体，不是写死在逻辑里的对象。
3. 技能是动作配方，不是一个巨大的 if-else。
4. 命令从 UI 来，但状态变化由服务来。

你在面试里可以直接这么讲：

> Wheatear 的战棋模块不是单独的战斗面板，而是场景里的一个玩法系统。  
> 棋盘坐标、单位、命令、技能、AI、动画和 UI 都由独立服务协作完成，场景组件只保留可配置和可序列化的状态。

## 6.4 场景怎么配：`TacticalCombatLevelComponent` 和 `TacticalUnitComponent`

### 6.4.1 战棋关卡组件

`TacticalCombatLevelComponent` 描述的是“一场战棋战斗的壳子”。

它的字段大致可以分成几类：

#### 棋盘和节奏

- `GridWidth`
- `GridHeight`
- `BoardOrigin`
- `CellSize`
- `StartFadeDuration`
- `IntroDuration`
- `ActionDuration`
- `EnemyStepDuration`
- `VictoryReturnDelay`
- `DefeatReturnDelay`

#### UI 绑定

- `CellEntityPrefix`
- `UnitEntityPrefix`
- `FadeEntityName`
- `MessageTextEntityName`
- `PhaseTextEntityName`
- `DetailTextEntityName`
- `CommandPanelEntityName`
- `ActionEffectEntityName`

#### 胜负命令

- `VictorySceneCommand`
- `DefeatSceneCommand`

#### 调参入口

- `TuningPath`

#### 运行时状态

- `RuntimePhase`
- `RuntimeRound`
- `RuntimeEnemyCursor`
- `RuntimeSelectedUnit`
- `RuntimeCommandMenuPage`
- `RuntimeSelectedSkillId`
- `RuntimeActionActor`
- `RuntimeActionTarget`
- `RuntimeActionSkillId`
- `RuntimeMessage`
- `RuntimeRequestedCommand`
- 各种计时器和标志位

### 6.4.2 单位组件

`TacticalUnitComponent` 是战棋里最核心的实体数据。

它包含：

- 阵营：`Team`
- 格子位置：`GridX` / `GridY`
- 名字和职业：`DisplayName` / `ClassName`
- 基础数值：`MaxHealth` / `Attack` / `Magic` / `Defense`
- 移动和攻击范围：`MoveRange` / `AttackRange`
- 是否可控：`Controllable`
- 技能槽：`BasicSkillId` / `Skill1Id` / `Skill2Id`
- UI 绑定名：血条、状态文本、标记
- 动画帧配置：`IdleFrames` / `AttackFrames` / `HitFrames` / `DownFrames`
- 运行时状态：`RuntimeAlive` / `RuntimeHasActed` / `RuntimeMoved` / `RuntimeGuarding`
- 状态效果：`RuntimeStatusEffects`
- 视觉状态：`RuntimeVisualClip` / `RuntimeVisualTimer`

这就说明了一件事：  
战棋里的单位不是“纯逻辑对象”，它是**逻辑 + 视觉 + UI 绑定**一起挂在场景上的。

## 6.5 数据驱动的调参：`TacticalCombatTuningService`

### 6.5.1 为什么要单独有一层调参

战棋很容易变成“改一处数值，整场体验都变了”。
所以 Wheatear 把一部分关键数值抽成了 YAML 调参文件。

调参路径挂在：

```text
TacticalCombatLevelComponent::TuningPath
```

默认值是：

```text
assets/sandbox/data/tactical_combat_tuning.yaml
```

这一层的数据不是写死在代码里，而是由 `TacticalCombatTuningService` 热加载后再回写到场景组件。

### 6.5.2 哪些字段由 YAML 主导

`IsFieldManagedByTuning()` 明确列出了被调参文件管理的字段，包括：

- 关卡节奏
- 棋盘尺寸
- 棋盘原点
- 单格尺寸
- 格子颜色

也就是说，场景里的这些字段只是**兜底值**，真正跑的时候会优先读调参文件。

### 6.5.3 调参文件里有什么

`TacticalCombatTuning` 主要分三块：

- `Level`
  - 开场淡入、回合阶段时间、胜负返回延迟、棋盘大小、格子颜色
- `Formula`
  - 魔法防御倍率、最小伤害
- `Units`
  - 单位标签、阵营、槽位、格子、名字、职业、数值、技能、动画帧

### 6.5.4 为什么它适合教材

因为它很好地解释了这套引擎的一个原则：

**场景组件保留可视化兜底，数据文件负责统一调优。**

这样做的好处是：

- 改数值不用改逻辑
- 调节节奏不用重编译
- 同一套战棋可以快速试不同玩法

### 6.5.5 编辑器怎么配

`TacticalCombatTuningEditorPanel` 把调参文件分成了几个面板：

- Level
- Board
- Formula
- Scene Units
- Advanced
- Raw YAML

这说明战棋调参不是单纯改字符串，而是已经能通过结构化编辑器直接维护。

## 6.6 `TacticalCombatSystem`：每帧入口

`TacticalCombatSystem` 是这一套的总入口。

它只做两件事：

1. `OnRuntimeStart()` 时把调参和初始状态准备好。
2. `OnUpdateRuntime()` 时收命令、推进流程。

### 6.6.1 启动时做什么

`OnRuntimeStart()` 会：

- 读取当前关卡的调参
- 把调参应用到关卡和场景单位上
- 如果 `PlayOnStart` 为真，就重置战斗

这意味着战棋并不是纯靠场景初始值跑起来的，  
它会在运行时把 YAML 的配置再覆盖一遍。

### 6.6.2 每帧做什么

`OnUpdateRuntime()` 的主链路是：

```text
CommandBus::DrainGameplayCommands("tactic:")
    ↓
TacticalCombatCommandService::ProcessCommand(...)
    ↓
TacticalCombatFlowService::UpdateLevel(...)
```

它不会直接在系统层处理“点了哪个按钮”。
按钮只负责变成命令，命令再驱动状态机。

这和 `VN`、`Progression` 的设计是一致的。

## 6.7 棋盘怎么工作：`TacticalCombatBoardService`

棋盘服务是整章最基础的一层。

它负责把“格子世界”这件事讲清楚。

### 6.7.1 坐标和格子

棋盘里的格子不是魔法数。
它由这些字段计算出来：

- `GridWidth`
- `GridHeight`
- `BoardOrigin`
- `CellSize`

核心函数很直白：

- `CellTag(level, x, y)`
- `CellTopLeft(level, x, y)`
- `CellCenter(level, x, y)`
- `InBounds(level, x, y)`

### 6.7.2 单位收集和占位

`CollectUnits(scene)` 会把场景里的 `TacticalUnitComponent` 按团队和槽位排序收集出来。

`FindUnitAt(scene, x, y)` 用来查一个格子上有没有单位。

这两个函数构成了棋盘的最基础语义：

- 哪些单位存在
- 它们在哪个格子
- 哪个格子被占了

### 6.7.3 移动和目标是否合法

棋盘服务还负责判断：

- `CanMoveTo(...)`
- `IsValidTarget(...)`
- `AllPlayerUnitsDone(...)`
- `HasAliveTeam(...)`

它的核心思路是：

- 不能越界
- 不能撞占位
- 不能移动超过范围
- 目标必须满足阵营和距离条件

### 6.7.4 敌人怎么找目标

`FindNearestAliveEnemy(...)` 和 `StepUnitOneCellToward(...)` 是 AI 最实用的两个工具。

一个负责找最近的敌方单位，另一个负责往目标靠一格。

这里可以看出这套战棋的 AI 其实不是“复杂策略 AI”，而是更适合 2D 游戏的**稳定、可预期、好调试**的规则型 AI。

## 6.8 流程主轴：`TacticalCombatFlowService`

### 6.8.1 战棋的几个阶段

`TacticalCombatPhase` 大致是：

- `Intro`
- `PlayerTurn`
- `AwaitCommand`
- `Targeting`
- `Acting`
- `EnemyTurn`
- `Victory`
- `Defeat`

这就是战棋的主节奏。

### 6.8.2 `ResetLevel()` 做什么

`ResetLevel()` 不只是清一清变量。
它做的是把整场战斗装回一个可运行状态：

- 重新应用调参
- 清运行时阶段
- 清回合数和计时器
- 重置消息和命令请求
- 重置单位状态
- 恢复淡入和行动特效

对每个单位，它还会：

- 修正生命值
- 重置 `RuntimeAlive`
- 清 `RuntimeHasActed`
- 清 `RuntimeMoved`
- 清防御和状态效果
- 清视觉状态

### 6.8.3 `UpdateLevel()` 做什么

每帧更新时，流程大概是：

1. 处理开场淡入
2. 刷新单位视觉
3. 根据阶段推进：
   - `Intro`：等待开场
   - `Acting`：推动行动演出并在中点执行效果
   - `EnemyTurn`：按间隔逐步让敌人行动
   - `Victory` / `Defeat`：延迟后发出场景命令
4. 刷新战斗 UI

### 6.8.4 为什么这里是教材重点

因为它把“战棋不是一堆按钮，而是一台状态机”这件事说得很清楚。

阶段驱动的好处是：

- 调试容易
- 行为稳定
- 代码分工清晰
- 视觉和逻辑不会乱缠在一起

## 6.9 命令怎么进来：`TacticalCombatCommandService`

战棋的 UI 并不是直接改坐标。
它也是命令驱动的。

### 6.9.1 命令前缀

战棋接收的命令前缀是：

```text
tactic:
```

典型命令包括：

- `tactic:cell:x:y`
- `tactic:menu:move`
- `tactic:menu:attack`
- `tactic:menu:skills`
- `tactic:menu:items`
- `tactic:skill:slot0`
- `tactic:skill:slot1`
- `tactic:skill:slot2`
- `tactic:skill:wait`
- `tactic:skill:item0`
- `tactic:cancel`

### 6.9.2 先选单位，再选动作

在 `PlayerTurn` 阶段，点一个格子上的我方可控单位，会进入选中状态：

- 记录 `RuntimeSelectedUnit`
- 切到 `AwaitCommand`
- 提示玩家选择行动

### 6.9.3 `move` 和 `attack`

当菜单页是 `move` 时，点击格子会走：

- `CanMoveTo(...)`
- 更新单位坐标
- 标记本回合已移动

当菜单页是 `attack` 时，点击敌人会走：

- 找基础攻击对应的技能
- 判断目标是否合法
- `BeginAction(...)`

### 6.9.4 技能页和取消

`skills` 页会把当前单位的：

- 普通攻击
- 技能 1
- 技能 2
- 待机 / 防御

整理成按钮。

`cancel` 会根据当前状态返回：

- 退回根菜单
- 或退回单位选择

这就是一个很典型的教材点：

**UI 状态不是乱跳的，它是有明确层级的。**

## 6.10 技能从哪来：`TacticalCombatSkillService`

这一层非常值得讲，因为它直接把战棋和 WAO 动作层接起来了。

### 6.10.1 技能不是手写死的

战棋技能不是单独写一套技能表，而是从 WAO 动作数据库里按前缀读取：

```text
tactical.
```

`SkillLibrary()` 会读取：

- `WAO::RecipesWithPrefix("tactical.")`

然后把 `ActionRecipe` 转成 `TacticalSkillDefinition`。

这意味着：

- 同一套动作配方可以被不同玩法复用
- 战棋技能和其他系统的动作定义保持统一语言

### 6.10.2 一个技能定义里有什么

战棋技能会从动作配方里抽出：

- 名字、说明、图标、音效
- 效果帧或图集
- 类别
- 目标规则
- 范围
- 伤害 / 治疗 / 防御穿透
- 是否是魔法、是否是防御
- 附加状态和持续回合数

### 6.10.3 技能分类和目标规则

技能类别会区分：

- `Attack`
- `Guard`
- `Item`
- `Wait`
- `Skill`

目标规则会区分：

- 敌人
- 友军
- 自身

### 6.10.4 伤害和治疗怎么算

这部分很适合讲给初学者听。

战棋不是直接“减血”。
它会先算公式：

- 普通伤害看攻击力
- 魔法伤害看魔法力
- 目标防御会参与削减
- 还会考虑防御倍率、穿透和最小伤害

治疗则按魔法属性和治疗系数算。

### 6.10.5 状态效果怎么处理

状态效果和 WAO 的状态系统一致：

- `Guard`
- `Regeneration`
- `Burn`
- `DefenseDown`
- `Stun`

`TickStatusEffects()` 会在回合推进点处理状态回合消耗和掉血。

这部分讲清楚以后，面试官一般会很容易问你：

> 你为什么要从 WAO 动作表生成技能库？

你就可以回答：

> 因为我想让战棋技能、街机武器、其他玩法动作共用同一套动作表达，不再分散维护成多个平行系统。

## 6.11 行动怎么结算：`TacticalCombatActionService`

这一层是整章最“工程味”的地方。

### 6.11.1 `BeginAction()`

`BeginAction()` 会把战棋进入 `Acting` 阶段：

- 记录行动者
- 记录目标
- 记录技能 ID
- 重置行动计时器
- 切回根菜单
- 播放技能音效

### 6.11.2 `ApplyAction()`

`ApplyAction()` 负责真正结算：

- 找行动者和目标
- 找技能定义
- 生成 `WAO::ActionIntent`
- 建 `WAO::EffectLedger`
- 按技能类型执行

如果是防御类技能：

- 打开 `RuntimeGuarding`
- 记录状态效果

如果是治疗类技能：

- 调 `GameplayCombatService::ApplyHealing(...)`
- 记录治疗账目
- 回写状态效果

如果是伤害类技能：

- 调 `CalculateDamage(...)`
- 调 `GameplayCombatService::ApplyDamage(...)`
- 记录伤害账目
- 设置受击闪烁
- 死亡时标记 `RuntimeAlive = false`

### 6.11.3 为什么这里要有 `EffectLedger`

因为它让战斗不是“暗改数值”。

你可以把它理解成：

- 这次行动做了什么
- 目标是谁
- 造成了多少效果
- 有没有触发状态

这对于调试、面试讲解、甚至以后做战斗回放都很有帮助。

### 6.11.4 `EndAction()`

行动结束后会检查：

- 敌方是不是全倒了
- 我方是不是全倒了

如果胜利：

- 切到 `Victory`
- 触发胜利提示音

如果失败：

- 切到 `Defeat`

否则就继续回到下一单位或下一回合。

## 6.12 敌人怎么行动：`TacticalCombatAIService`

这部分很适合讲“战棋 AI 其实并不玄”。

### 6.12.1 敌人列表怎么来

AI 会先从场景里收集所有单位，然后筛出：

- `Team == Enemy`
- `RuntimeAlive == true`

### 6.12.2 回合结束后怎么重置

当敌人都行动完以后：

- 回合数加一
- 切回 `PlayerTurn`
- 清 `RuntimeSelectedUnit`
- 清命令页
- 清技能选择
- 给玩家提示“第 N 回合，我方行动”

同时它还会：

- 对所有单位 tick 状态效果
- 把 `RuntimeHasActed` / `RuntimeMoved` / `RuntimeGuarding` 清掉

### 6.12.3 敌人是怎么决定“打还是走”的

逻辑很直接：

1. 找最近的存活敌方单位
2. 看当前技能范围够不够
3. 不够就往目标靠一格
4. 够就 `BeginAction(...)`

这类 AI 的好处是：

- 稳定
- 易调
- 容易解释
- 很适合 2D 游戏的教学和 Demo

## 6.13 UI 和演出怎么接：`TacticalCombatUIService` 和 `TacticalCombatVisualService`

### 6.13.1 状态面板

`UpdateStatusPanel()` 会做这些事：

- 刷单位血条
- 显示单位状态文本
- 标出防御、已行动、倒下、状态效果
- 显示当前阶段和回合数
- 显示本帧消息

### 6.13.2 命令面板

`UpdateCommandPanel()` 会根据当前页生成按钮：

- 根菜单：移动、攻击、技能、道具
- 技能菜单：普通攻击、技能 1、技能 2、待机 / 防御
- 道具菜单：恢复药水、返回

按钮本质上都只是命令：

- `tactic:menu:move`
- `tactic:skill:slot1`
- `tactic:cancel`

所以这里还是那句老话：  
**UI 负责发命令，系统负责消费命令。**

### 6.13.3 格子高亮

`UpdateTiles()` 会把格子染成不同颜色：

- 默认格
- 选中格
- 移动格
- 攻击格
- 技能目标格

这就是战棋“看得懂”的关键。

### 6.13.4 单位视觉

`TacticalCombatVisualService` 会根据单位当前状态选择视觉片段：

- `idle`
- `attack`
- `hit`
- `down`

然后按：

- 帧序列
- 或图集帧

去更新单位图像。

它还会做：

- 受击闪烁
- 已行动变灰
- 倒下变暗
- 选中标记显示

### 6.13.5 行动特效

`UpdateActionEffect()` 会在目标格上显示技能效果图。

这一块非常适合做教材里的“攻击动画从哪来”的例子，因为它把：

- 技能定义
- 视觉资源
- 目标位置
- 动画帧率

串成了一条完整链。

## 6.14 一个完整例子：从点单位到一回合结束

我们拿一个最直观的例子走一遍。

### 6.14.1 进入战斗

场景启动后：

- `OnRuntimeStart()` 读取调参
- 重置战斗
- 单位状态回到初始值
- 进入开场淡入

### 6.14.2 选择单位

玩家点一个我方单位：

```text
tactic:cell:2:3
```

系统检查这个格子上是否有可控单位、是否还没行动。
如果可以，就进入 `AwaitCommand`。

### 6.14.3 选择移动

玩家点移动按钮：

```text
tactic:menu:move
```

然后点一个蓝色格子：

```text
tactic:cell:4:3
```

系统判断：

- 是否在范围内
- 是否空格

通过后更新格子坐标，并标记该单位已移动。

### 6.14.4 选择攻击或技能

如果点攻击：

```text
tactic:menu:attack
```

再点红色范围内的敌人，就直接 `BeginAction(...)`。

如果点技能：

```text
tactic:menu:skills
tactic:skill:slot1
```

如果技能需要选目标，系统会进入 `Targeting`。
如果是自身技能，就直接开始行动。

### 6.14.5 行动演出

进入 `Acting` 后：

- 中点时 `ApplyAction()`
- 结束时 `EndAction()`
- 结果写进单位状态
- UI 和视觉立刻更新

### 6.14.6 敌方回合

当所有我方单位都行动完：

- 切到 `EnemyTurn`
- 敌人按顺序逐步行动
- 不够范围就往前走一格
- 够范围就出手

### 6.14.7 胜负后跳转

如果敌人死光：

- 进入 `Victory`
- 延迟后发 `VictorySceneCommand`

如果我方死光：

- 进入 `Defeat`
- 延迟后发 `DefeatSceneCommand`

这条链路讲完，战棋模块就不再是“按钮很多”，而是“状态机很清楚”。

## 6.15 和 Unity / UE 的对照

### 6.15.1 和 Unity 的对照

Unity 里你可能会把战棋拆成：

- MonoBehaviour
- ScriptableObject
- Tilemap / Grid
- UI 控制器
- 回合管理器
- 技能表
- AI 脚本

Wheatear 的做法更偏服务化：

- 场景组件存状态
- 棋盘服务管格子
- 流程服务管阶段
- 技能服务从 WAO 动作表取配方
- 行动服务统一结算

### 6.15.2 和 UE 的对照

UE 里常见的思路是：

- Actor / Component 管单位
- DataTable / DataAsset 管技能和数值
- UMG 管 UI
- GameMode / GameState 管流程

Wheatear 的差异在于：

- 它把“技能”和“动作配方”靠得更近
- 它把 AI、UI、演出都拆成很小的服务
- 它更适合一个轻量的 2D 教学项目

## 6.16 好处、坏处和常见误区

### 6.16.1 好处

- 棋盘、单位、技能、AI 职责清楚
- 命令驱动，调试简单
- WAO 动作表可以复用到别的玩法
- 调参文件能直接改节奏和布局
- 视觉和逻辑分离，方便迭代

### 6.16.2 坏处

- 文件很多，初学者容易迷路
- 组件字段一多，命名必须统一
- 状态机阶段多了以后，必须小心别把流程写乱
- 如果调参和场景字段的主从关系不清楚，就会出现“改了没生效”的错觉

### 6.16.3 常见误区

1. 把战棋写成一个巨大的 `Update()`。
2. 让 UI 直接修改单位数值。
3. 把技能表和动作表混成两套完全不同的体系。
4. 忘了区分场景兜底值和 YAML 主导值。
5. 让 AI 直接写死在单位组件里，不单独抽服务。

## 6.17 面试时可以怎么讲

如果面试官问你“战棋是怎么设计的”，你可以这样说：

> 我把战棋做成了一个场景内的状态机。  
> 场景组件负责棋盘和单位配置，`TacticalCombatSystem` 负责每帧入口，`TacticalCombatBoardService` 负责格子和目标规则，`TacticalCombatSkillService` 从 WAO 动作表生成技能库，`TacticalCombatActionService` 统一结算，`TacticalCombatAIService` 负责敌方行动，UI 和视觉服务只负责把状态画出来。  
> 另外我还把棋盘布局、回合时序、单位参数和颜色做成了可热重载的 YAML 调参，这样战棋的节奏和体验可以不改代码直接调。

如果对方继续问，你可以顺着讲：

- 为什么要用 `tactic:` 命令
- 为什么技能从 WAO 动作表生成
- 为什么要把格子高亮和单位动画分开
- 为什么要有 YAML 主导的调参层

## 6.18 小结

这一章最重要的一句话是：

**TacticalCombat 不是一个大脚本，而是一套把棋盘、单位、技能、AI、UI 和调参串起来的战棋执行系统。**

如果你理解了这层，后面再看：

- `Arcade`
- `Progression`
- `VN`
- `TurnCombat`

你会发现它们虽然玩法不同，但组织方式其实很像：

- 都有自己的状态
- 都有自己的命令入口
- 都有自己的页面或演出服务
- 都能把结果接回更高层的流程里
