# 第 6 编 05：Arcade

> 本章所属层级：**玩法层**
>
> 本章目标：理解 Wheatear 里的街机战斗是怎样把“输入、武器切换、Boss 行为、投射物、掩体、HUD、演出、调参和动作层”串成一条完整链路的。
>
> 先提醒一句：这里的 `Arcade` 不是“随便做了个射击 Demo”。  
> 它其实是一套完整的**街机动作玩法**：玩家移动、切武器、发射、Boss 登场、子弹和掩体碰撞、胜负过场、场景跳转，全都被拆成了独立服务。
>
> 如果说 `TacticalCombat` 讲的是格子战斗怎么工程化，`Arcade` 讲的就是：  
> **一个简单 2D 动作关卡，怎样用输入、动作配方和投射物系统做得既顺手又能调。**

## 6.1 这一层在架构里的位置

`Arcade` 仍然属于**玩法层**，不是核心平台层，也不是引擎服务层。
它处理的不是底层渲染或输入，而是街机动作玩法本身：

- 玩家怎么移动
- 武器怎么切换
- 攻击怎么发射
- Boss 怎么入场、跳跃和射击
- 子弹怎么飞、怎么撞、怎么消失
- 掩体怎么挡弹
- HUD 怎么显示
- 胜负后怎么切场景

它和 `TacticalCombat` 的区别很大：

- 战棋更偏状态机和格子规则
- 街机更偏实时输入、快速反馈和演出节奏

但它们也有共同点：

- 都是场景内玩法
- 都有自己的运行时状态
- 都有自己的调参文件
- 都能把结果接回更高层流程

你可以把这一层理解成：

```text
Input / HUD
    ↓
ArcadeCombatSystem
    ├─ Lifecycle
    ├─ Player
    ├─ Boss
    ├─ Projectile
    ├─ Presentation
    ├─ Outcome
    └─ HUD
```

所以它不是“一个射击脚本”，而是**一套街机战斗管线**。

## 6.2 本章先看哪些源码

建议按这个顺序看：

| 文件 | 作用 |
| --- | --- |
| `Wheatear/src/Wheatear/Modules/ArcadeCombat/ArcadeCombatComponents.h` | 关卡、单位、Boss、子弹、触发器组件 |
| `Wheatear/src/Wheatear/Modules/ArcadeCombat/ArcadeCombatSystem.cpp` | 每帧入口，接输入并推进各服务 |
| `Wheatear/src/Wheatear/Modules/ArcadeCombat/ArcadeCombatLifecycleService.cpp` | 开局重置、单位重置、Boss 初始姿态 |
| `Wheatear/src/Wheatear/Modules/ArcadeCombat/ArcadeCombatPlayerService.cpp` | 玩家移动、武器切换、发射 |
| `Wheatear/src/Wheatear/Modules/ArcadeCombat/ArcadeCombatBossService.cpp` | Boss 行为 |
| `Wheatear/src/Wheatear/Modules/ArcadeCombat/ArcadeCombatProjectileService.cpp` | 子弹生成、移动、碰撞、销毁 |
| `Wheatear/src/Wheatear/Modules/ArcadeCombat/ArcadeCombatPresentationService.cpp` | 开场淡入、触发器、Boss 登场演出 |
| `Wheatear/src/Wheatear/Modules/ArcadeCombat/ArcadeCombatOutcomeService.cpp` | 胜负过场、场景命令 |
| `Wheatear/src/Wheatear/Modules/ArcadeCombat/ArcadeCombatHudService.cpp` | HUD、武器栏、移动摇杆、攻击按钮 |
| `Wheatear/src/Wheatear/Modules/ArcadeCombat/ArcadeCombatTuningService.h/.cpp` | 街机调参、热重载、场景/数据主从 |
| `Wheatear/src/Wheatear/Modules/ArcadeCombat/ArcadeCombatActionResolver.cpp` | `arcade.` 动作如何接到投射物生成 |
| `Wheatear/src/Wheatear/Modules/ArcadeCombat/ArcadeCombatSignalHandlers.cpp` | 投射物生成信号的处理 |
| `WheatearEditor/src/Modules/ArcadeCombat/ArcadeCombatTuningEditorPanel.cpp` | 街机调参编辑器 |

如果你只想先抓住主链路，就先看：

`ArcadeCombatSystem` -> `ArcadeCombatPlayerService` / `ArcadeCombatBossService` -> `ArcadeCombatProjectileService` -> `ArcadeCombatOutcomeService`

## 6.3 `Arcade` 到底是什么

这套模块不是一个“玩家 + 敌人 + 子弹”的松散合集。
它更像一个**场景内的街机战斗执行器**：

```text
Scene
  └─ ArcadeCombatLevelComponent
        ├─ 竞技场边界
        ├─ 玩家 / Boss / HUD 绑定名
        ├─ Boss 入场与胜负命令
        ├─ 调参路径
        └─ 运行时状态

ArcadeCombatantComponent
  └─ 血量、移动速度、碰撞半径、活着与否

ArcadePlayerControllerComponent
  └─ 当前武器、自动瞄准、冷却

ArcadeBossComponent
  └─ Boss 入场、跳跃、射击节奏

ArcadeProjectileComponent / ArcadeCoverComponent / ArcadeTriggerComponent
  └─ 子弹、掩体、触发器
```

它的工程重点是：

1. 输入要快，反馈要快。
2. 武器和 Boss 行为要可调。
3. 子弹、掩体、触发器都要是场景实体。
4. 胜负后要稳定跳回剧情或别的流程。

你在面试里可以直接这么讲：

> Wheatear 的 Arcade 不是一个临时写的射击小样，而是一套场景驱动的动作玩法。  
> 输入、武器、Boss 行为、投射物生成、掩体碰撞和结果切换都被拆成独立服务，调参文件负责统一节奏和手感。

## 6.4 场景怎么配：`ArcadeCombatLevelComponent` 和相关组件

### 6.4.1 关卡组件

`ArcadeCombatLevelComponent` 描述的是一场街机战斗的壳子。

它的字段主要分几类：

#### 竞技场和 UI 绑定

- `ArenaMin`
- `ArenaMax`
- `PlayerEntityName`
- `BossEntityName`
- `FadeEntityName`
- `PausePanelEntityName`
- `MessageTextEntityName`
- `WeaponTextEntityName`
- `PlayerHealthBarEntityName`
- `PlayerHealthTextEntityName`
- `BossHealthBarEntityName`
- `BossHealthTextEntityName`

#### 节奏和结果

- `StartFadeDuration`
- `VictoryReturnDelay`
- `DefeatReturnDelay`
- `ResultSceneFadeDuration`
- `BossDefeatFadeDuration`
- `VictorySceneCommand`
- `DefeatSceneCommand`

#### 调参入口

- `TuningPath`

#### 运行时状态

- `RuntimeElapsed`
- `RuntimeFadeAlpha`
- `RuntimePaused`
- `RuntimeBossIntroStarted`
- `RuntimeBossIntroFinished`
- `RuntimeVictory`
- `RuntimeDefeat`
- `RuntimeResultTimer`
- `RuntimeResultCommandIssued`
- `RuntimeRequestedCommand`

### 6.4.2 单位组件

`ArcadeCombatantComponent` 保存的是最核心的战斗属性：

- `Team`
- `MaxHealth`
- `Health`
- `MoveSpeed`
- `CollisionRadius`
- `Invulnerable`
- `Alive`
- `ControlsLocked`

`ArcadePlayerControllerComponent` 是玩家控制状态：

- `CurrentWeapon`
- `AutoAim`
- `WeaponCooldown`

`ArcadeBossComponent` 是 Boss 的节奏控制：

- `Active`
- `IntroStartPosition`
- `FightPosition`
- `IntroDuration`
- `ShootInterval`
- `JumpInterval`
- `JumpDuration`
- 以及跳跃过程中的运行时参数

`ArcadeProjectileComponent` 则保存子弹本身：

- `Velocity`
- `Damage`
- `Lifetime`
- `Radius`
- `Team`
- `Heavy`
- `Melee`

`ArcadeCoverComponent` 和 `ArcadeTriggerComponent` 分别表示：

- 掩体
- 触发区

这说明一件事：  
街机玩法不是把所有东西写进一个控制器，而是把每一种交互都做成可以摆进场景的实体。

## 6.5 数据驱动的调参：`ArcadeCombatTuningService`

### 6.5.1 为什么这层也要单独调参

街机玩法最影响手感的东西通常不是“有没有功能”，而是：

- 玩家移速快不快
- Boss 出招频率高不高
- 子弹速度、寿命、半径合不合适
- 入场、胜负和淡入的节奏对不对

所以 Wheatear 也把街机的关键参数抽成了 YAML 调参文件：

```text
ArcadeCombatLevelComponent::TuningPath
```

默认值是：

```text
assets/sandbox/data/arcade_combat_tuning.yaml
```

### 6.5.2 哪些字段由调参文件主导

`IsFieldManagedByTuning()` 列出了被 YAML 主导的字段，包括：

- 开场和结果时序
- Boss 入场 / 射击 / 跳跃参数
- 玩家移动速度
- 自动瞄准开关

### 6.5.3 调参文件里有什么

`ArcadeCombatTuning` 大致分三块：

- `Level`
  - 开场淡入、胜负返回、结果页淡出
- `Boss`
  - 入场、射击、跳跃、子弹参数
- `Player`
  - 移速、自动瞄准、武器参数

### 6.5.4 武器为什么能单独调

`ArcadePlayerTuning::Weapons` 会分别给：

- `gun`
- `cannon`
- `katana`

提供各自的飞弹或斩击参数。

如果 YAML 里缺某个武器配置，还会回退到预填的经典默认值。

这对教材很重要，因为它说明：

**调参不是只调一个总表，而是可以针对每把武器单独调手感。**

### 6.5.5 编辑器怎么配

`ArcadeCombatTuningEditorPanel` 里通常会有这些页：

- Level
- Boss
- Player
- Scene Units
- Raw Preview

也就是说，它不是给你一堆裸 YAML 让你瞎改，而是已经有了结构化调参窗口。

## 6.6 `ArcadeCombatSystem`：每帧入口

街机模块和战棋最大的不同之一，就是它**不靠命令轮询来驱动整套输入**。

它更偏实时动作。

### 6.6.1 启动时做什么

`OnRuntimeStart()` 会：

- 重置触控输入
- 重置场景里的战斗单位、掩体和触发器
- 读取调参
- 把调参应用到关卡、Boss 和玩家
- 重置关卡运行时状态
- 重置 Boss 演出状态

### 6.6.2 每帧做什么

`OnUpdateRuntime()` 的核心流程可以概括成：

```text
采样输入
    ↓
更新开场淡入 / 触发器 / 暂停
    ↓
更新 Boss 入场
    ↓
更新 Boss 行为
    ↓
更新玩家行为
    ↓
更新子弹
    ↓
更新胜负过场
    ↓
更新 HUD
```

和战棋不同，街机这里是**实时推进**，不是阶段等待。

## 6.7 输入和 HUD：`ArcadeCombatHudService`

### 6.7.1 输入是怎么采样的

`ArcadeCombatSystem` 里会先采样输入，整理成一个 `PlayerInputState`。

它会读取：

- `move.left`
- `move.right`
- `move.up`
- `move.down`
- `arcade.attack`
- `arcade.weapon1`
- `arcade.weapon2`
- `arcade.weapon3`
- `game.pause`

同时还会融合 HUD 上的触控输入：

- 虚拟摇杆
- 攻击按钮
- 武器按钮

这很适合简单 2D 游戏，因为它既能吃键盘，也能吃触摸。

### 6.7.2 HUD 不只是显示，它也在采样触控

`ArcadeCombatHudService` 里有几个很关键的职责：

- 建消息文本
- 更新血条
- 更新武器栏
- 更新暂停面板
- 管理虚拟摇杆和攻击按钮
- 管理武器切换高亮

它会根据当前状态显示不同提示，例如：

- 还没触发 Boss 时怎么走
- Boss 入场中是否可操作
- 暂停中怎么继续
- 胜利或失败时怎么提示

### 6.7.3 武器选择怎么显示

HUD 里会把三种武器做成按钮或槽位。
当前武器会加选中框。

这也说明一件事：  
街机不是“输入打进逻辑里就完了”，它还要把状态很明确地反馈给玩家。

## 6.8 玩家怎么动和怎么打：`ArcadeCombatPlayerService`

### 6.8.1 武器切换

`UpdateWeaponSelection()` 会根据：

- 键盘输入
- 触控 HUD

去切换当前武器：

- 手枪
- 重炮
- 太刀

### 6.8.2 移动

`UpdatePlayer()` 会先检查玩家是否还活着。

如果活着且没被锁定，就根据输入方向移动角色，并把位置限制在竞技场边界内：

- 不能跑出左边界
- 不能跑出右边界
- 不能跑出上边界
- 不能跑出下边界

### 6.8.3 攻击

如果攻击键按住，而且冷却允许，就会发射武器。

这一段很关键，因为它把“手感”拆成了三层：

1. 输入层：按没按攻击键
2. 控制层：当前武器是什么
3. 动作层：对应的动作配方怎么播

### 6.8.4 动作配方怎么接进来

玩家的武器不是直接写死的特效。
它先映射成 WAO 动作配方：

- `arcade.gun`
- `arcade.cannon`
- `arcade.katana`

然后通过 `WAO::ActionOrchestrator::ExecuteWithRecipe(...)` 去执行。

这时会带一个 `ProjectileSpawnPayload`，里面写：

- 场景上下文
- 子弹实体名
- 位置
- 速度
- 伤害
- 寿命
- 半径
- 阵营
- 颜色
- Heavy / Melee 标志

也就是说，**武器不是直接“发子弹”，而是“执行动作配方，动作再驱动子弹生成”。**

这个链路很适合教材和面试讲。

## 6.9 Boss 怎么跑：`ArcadeCombatBossService`

### 6.9.1 Boss 的基本行为

Boss 有三个核心节奏：

- 入场
- 跳跃
- 射击

它不是一堆随机逻辑，而是有固定时间间隔的。

### 6.9.2 入场怎么做

Boss 先有一个入场起点和战斗位置。

`ArcadeCombatPresentationService` 会先把 Boss 从场外拉进来，  
`ArcadeCombatBossService` 再在它 `Active` 后正式开始射击和跳跃。

### 6.9.3 跳跃怎么做

Boss 的跳跃不是瞬移，而是会基于时间和调参文件里的频率、幅度、弧高来算一个轨迹。

这就让 Boss 看起来更像一个有节奏的动作角色，而不是一个站桩炮台。

### 6.9.4 射击怎么做

Boss 射击会：

- 算朝向玩家的方向
- 取 Boss 子弹的调参
- 构造投射物 payload
- 通过动作层发出生成信号

如果你要讲“这个项目里动作层做了什么”，Boss 这一段就是很好的例子。

## 6.10 子弹和动作层怎么接：`ArcadeCombatProjectileService` / `ArcadeCombatActionResolver` / `ArcadeCombatSignalHandlers`

这三个文件要放在一起讲，才完整。

### 6.10.1 `ArcadeCombatActionResolver`

街机动作不是只会生成个视觉特效。
它先注册一个 resolver：

```text
WAO::ActionResolverRegistry::RegisterResolver("arcade.", ...)
```

这意味着所有 `arcade.` 前缀的动作，都能走街机专属的动作分发逻辑。

### 6.10.2 `ArcadeCombatSignalHandlers`

如果动作需要生成子弹，就会发出：

```text
arcade.projectile.spawn
```

然后由信号处理器把 payload 变成真正的场景实体。

### 6.10.3 `ArcadeCombatProjectileService`

子弹服务负责：

- 创建投射物实体
- 给它加 Transform 和 Sprite
- 写入速度、伤害、寿命、半径、阵营
- 每帧移动
- 到时间销毁
- 碰到掩体就挡住
- 碰到敌对单位就结算伤害

### 6.10.4 掩体为什么重要

掩体不是纯装饰。
它会真正参与子弹碰撞。

这让街机玩法变得更像一个可玩关卡，而不是只会直线互射的测试场。

## 6.11 开场、触发和结果：`ArcadeCombatPresentationService` / `ArcadeCombatOutcomeService`

### 6.11.1 开场淡入

`UpdateStartFade()` 会在开局逐渐减淡遮罩，形成关卡开始感。

### 6.11.2 Boss Intro 触发

`UpdateIntroTrigger()` 会检查玩家是否进入某个触发区。

触发后：

- Boss 开始入场
- 玩家控制被锁定
- 触发标记被置位

这就让 Boss 出场不是完全写死的，而是通过场景触发器控制。

### 6.11.3 Boss 入场演出

`UpdateBossIntro()` 会把 Boss 从起始位置平滑移动到战斗位置。

入场结束后：

- Boss `Active = true`
- 玩家控制解锁

### 6.11.4 胜负过场

`ArcadeCombatOutcomeService::UpdateResultTransition()` 会：

- 锁定玩家控制
- 胜利时淡出 Boss
- 把场景遮罩慢慢推上来
- 在延迟到点后发出 `VictorySceneCommand` 或 `DefeatSceneCommand`

这说明街机的结果不是“赢了就立刻切”。
它还有一个比较自然的过场时间。

## 6.12 一个完整例子：从开局到胜负

我们把整条链路连起来看。

### 6.12.1 关卡开始

场景启动后：

- `OnRuntimeStart()` 重置战斗
- 读调参
- Boss、玩家、掩体、触发器都被准备好
- 进入开场淡入

### 6.12.2 玩家移动并切武器

玩家用 WASD / 方向键移动，或者用虚拟摇杆。

按 `1/2/3` 可以切武器：

- 手枪
- 重炮
- 太刀

### 6.12.3 触发 Boss 入场

玩家跑进触发区后：

- Boss 开始入场演出
- 玩家暂时不能动
- Boss 进入战斗位置

### 6.12.4 开火

玩家按攻击键后：

- 当前武器对应的 WAO 动作配方被执行
- 动作信号发出子弹生成请求
- 子弹实体被创建
- 子弹开始移动

### 6.12.5 子弹碰撞

子弹飞行时会：

- 命中掩体
- 或命中敌方战斗单位

命中后立即处理伤害并销毁子弹。

### 6.12.6 Boss 反击

Boss 会按自己的节奏：

- 跳跃
- 射击
- 继续压迫玩家

### 6.12.7 胜负与跳转

当 Boss 被击破：

- 进入胜利状态
- 淡出 Boss
- 过一小会儿跳转到胜利场景命令

如果玩家被击倒：

- 进入失败状态
- 过一小会儿跳回重试或剧情命令

这就是街机玩法的完整闭环。

## 6.13 和 Unity / UE 的对照

### 6.13.1 和 Unity 的对照

Unity 里这类玩法通常会拆成：

- PlayerController
- BossController
- Projectile Prefab
- HUD Canvas
- Trigger Collider
- ScriptableObject 配表

Wheatear 的做法更偏服务化：

- 场景组件放状态
- 玩家、Boss、子弹、HUD 各走独立服务
- 动作层统一控制 projectile spawn
- 调参通过 YAML 热重载

### 6.13.2 和 UE 的对照

UE 里常见的思路是：

- Character / Actor
- Projectile Actor
- UMG HUD
- DataAsset / DataTable
- GameMode / GameState

Wheatear 的差异在于：

- 它把动作配方和投射物生成的桥接做得更显式
- 它对场景实体和 UI 绑定更直接
- 它更适合一个轻量、清楚、好讲的 2D 项目

## 6.14 好处、坏处和常见误区

### 6.14.1 好处

- 输入响应快
- HUD 和触控支持完整
- 武器、Boss 和子弹都能热调
- 动作层和投射物生成解耦
- 胜负过场和剧情跳转衔接清楚

### 6.14.2 坏处

- 服务类多，初学者会觉得“文件很多”
- 如果控制锁定和状态切换没处理好，手感会发飘
- 如果调参和场景字段的主从关系不清楚，就会出现“改了没效果”
- 子弹、掩体、Boss 行为都在实时更新，调试时要更注意顺序

### 6.14.3 常见误区

1. 以为街机玩法就是直接在 `Update()` 里写逻辑。
2. 让输入和 HUD 直接改战斗状态，不走控制层。
3. 把子弹生成和动作执行混成一团。
4. 不区分调参文件和场景兜底值。
5. 忘了在胜负时锁住玩家控制。

## 6.15 面试时可以怎么讲

如果面试官问你“Arcade 是怎么设计的”，你可以这样说：

> 我把 Arcade 做成了一个场景内的街机动作玩法。  
> `ArcadeCombatSystem` 每帧采样输入并推进各个服务，`ArcadeCombatPlayerService` 负责移动、切武器和发射，`ArcadeCombatBossService` 负责 Boss 行为，`ArcadeCombatProjectileService` 负责子弹和掩体碰撞，`ArcadeCombatPresentationService` 负责 Boss 登场和开场演出，`ArcadeCombatOutcomeService` 负责胜负过场。  
> 玩家武器和 Boss 子弹都通过 WAO 动作配方和动作解析器接进来，子弹生成则由信号处理器落到场景实体上。调参文件负责把手感和节奏从代码里剥离出来。

如果对方继续追问，你可以接着说：

- 为什么要有 `arcade.` 动作前缀
- 为什么要把子弹生成拆成信号
- 为什么 HUD 还要处理触控
- 为什么要让 Boss 出场先锁控制
- 为什么调参文件里要单独拆武器表

## 6.16 小结

这一章最重要的一句话是：

**Arcade 不是一个临时写出来的射击小样，而是一套把输入、动作、子弹、Boss、HUD 和过场统一起来的街机动作系统。**

如果你理解了这一层，后面再看：

- `TacticalCombat`
- `Progression`
- `VN`
- `TurnCombat`

你会发现它们虽然玩法不同，但组织方式都很一致：

- 场景里放状态
- 服务层推流程
- 命令或输入只负责入口
- 数据表和调参文件负责可调
