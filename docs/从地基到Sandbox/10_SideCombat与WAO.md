# Part 10 · SideCombat 与 WAO：数据表战斗、动作编排与 HUD

> 目标：理解横板战斗模块的分层结构——数据表驱动、运行时服务集、
> WAO 动作编排、AI 与 HUD。结尾用"一次 basic 攻击"的完整链路
> 把本 Part 与 Part 2/6/7 全部串起来。

## 10.1 数据表驱动：一切参数都是资产

战斗的**所有参数**都在数据表里（`assets/vertical_slice/data/side_combat_tuning.yaml`），
不是硬编码：

```yaml
# 玩家手感（移动/跳跃/连段窗口）
player:
  moveSpeed: 5.55
  maxJumps: 1
  jumpImpulse: 8.8
  gravity: 23.0
  basicChainWindow: 0.76
  launcherChainWindow: 0.86
  # ...
combat:                    # 通用战斗规则
  comboDropDelay: 1.20
  hitInvulnerableTime: 0.035
  defenseBase: 100.0
# 敌人 AI（Boss 是 bearBoss 节点）
enemy:
  bearBoss: { moveSpeed: 3.75, aggroRange: 14.0, attackRange: 1.72,
              attackInterval: 0.82, chargeDistance: 2.15, ... }
# 角色动画 clip（Part 6 的运行时生成帧就来自这里）
visuals:
  playerAnimations:
    idle: { atlas: { sheet: ".../protag_idle_sheet.png", cellWidth: 512,
                     cellHeight: 512, columns: 8, startFrame: 0 },
            frameCount: 8, frameRate: 8.0, loop: true }
    basic1: { atlas: { sheet: ".../protag_basic1_sheet.png", cellWidth: 512,
                       cellHeight: 512, columns: 4, startFrame: 0 },
              frameCount: 4, frameRate: 14.0, loop: false }
    # ...
skills:                    # 技能显示/输入/招式绑定/解锁章节
  basic_attack: { displayName: "三段斩", input: "J",
                  attackIds: [basic1, basic2, basic3], unlockChapter: 2 }
```

`SideCombatTuningService` 加载这些表（500ms 热重载，Part 4 降频表），
各服务只读表、不写死参数。**调手感 = 改 YAML，不重编**——
这是整个模块的第一原则。

## 10.2 服务分层：系统薄，服务厚

`SideCombatSystem`（系统本体）每帧只做编排，具体能力拆成十几个
**服务**（`Modules/SideCombat/`）：

| 服务 | 职责 |
| --- | --- |
| `SideCombatLifecycleService` | 战斗开始/结束、实体重置、玩家属性与 Boss 参数应用 |
| `SideCombatPlayerService` | 玩家移动/跳跃/输入解析/技能起手/空中动作 |
| `SideCombatActionService` | 动作请求（读 WAO recipe：时长/命中帧/取消窗/音效） |
| `SideCombatComboService` | 连击窗口与链式、空中命中奖励、魔剑槽增长 |
| `SideCombatEnemyAIService` | 敌人行为状态机（接近/走位/Boss 阶段） |
| `SideCombatHitboxService` | 攻击判定盒生成/帧贴图/空间重叠检测 |
| `SideCombatHitResolutionService` | 命中结算（伤害/击退/无敌帧/保护条） |
| `SideCombatPhysicsService` | 横板纵深移动/空中高度/重力/落地 |
| `SideCombatPickupService` | 掉落物创建/吸附/拾取入库 |
| `SideCombatOutcomeService` | 死亡处理/胜利判定/结算与场景流转 |
| `SideCombatVisualService` | 动画/特效/音效播放、受击颜色反馈 |
| `SideCombatFeedbackService` | 镜头反馈/受击闪白/hit pause/震屏 |
| `SideCombatHudService` | HUD 状态同步（血条/技能图标/连击/奖励文字） |
| `SideCombatTargetService` | 玩法内目标选择（最近存活敌人） |
| `SideCombatTuningService` | 调参 YAML 加载/默认值/500ms 热重载/章节 profile |
| `SideCombatResultService` | 结算评级/经验/结果摘要 |

**为什么拆服务**：系统保持"每帧调度"的薄壳，每个服务是
可独立测试/独立调参的纯逻辑单元。这也是"引擎 vs 玩法"的
边界实践——Part 2/3 的纪律在玩法层延续。

## 10.3 输入 → 动作：HUD 按钮与键盘共用一条链路

Part 2 的实例里 `SC_SkillIcon_J` 按钮带
`OnClickFunction: "side:basic"`。这条链路是**输入统一**的范例：

```
键盘按键  →  InputBindingService::IsActionPressed("side.basic")
HUD 按钮  →  CommandBus 收到 "side:basic"
          →  SideCombatSystem::OnUpdateRuntime drain 命令
          →  InputBindingService::InjectActionPress("side.basic")
          →  SideCombatPlayerService 看到同一个动作
```

`SideCombatSystem.cpp` 的开头就是这段（`DrainGameplayCommands("side:")` →
`InjectActionPress`）——**UI 点击和键盘按下对玩法是完全相同的输入**
（Part 7 的"命令注入"在这里落地）。连招窗口、冷却、消耗、
动画选择全部只看动作名。

## 10.4 WAO：动作编排层

WAO（Wheatear Action Orchestration）是**跨玩法的动作语义层**，
位于 `Gameplay/Action/`（不归属任何战斗模块）：

```
ActionIntent（意图）→ ActionRecipe（配方）→ RuleResolver（规则）
        → EffectBundle（效果束）→ EffectLedger（账本）→ ActionSignalRouter（信号）
```

### 10.4.1 四个核心类型（`ActionTypes.h`）

```cpp
struct ActionIntent      // 谁、用什么动作、目标是谁
{
    UUID Actor; std::string ActionId; UUID ExplicitTarget;
    glm::vec2 WorldPoint; std::string InputId; std::string Source;
};

struct ActionRecipe      // 一个动作的完整定义（资产！）
{
    std::string Id, DisplayName, IconPath, AnimationId, SoundPath, EffectPath;
    float Cooldown, Duration, Startup, Recovery, HitTime;
    float CancelStart, CancelEnd;          // 取消窗口（连招基础）
    std::vector<std::string> Tags;         // 分类标签
    std::vector<std::string> Signals;      // 播放期信号
};
```

配方是**YAML 资产**（`assets/gameplay/actions/10_side_combat_actions.yaml`）：

```yaml
- id: side.basic1
  displayName: Basic Slash I
  icon: side.skill.icon.basic
  animation: side_basic1
  sound: assets/vertical_slice/side_combat/audio/swing_light.wav
  effect: side.vfx.basic_slash
  cooldown: 0.19
  duration: 0.31
  startup: 0.06          # 前摇
  recovery: 0.10         # 后摇
  hitTime: 0.06          # 命中帧
  cancelStart: 0.12      # 取消窗口开始（可接 basic2）
  cancelEnd: 0.26
  tags: [Gameplay.SideCombat, Gameplay.Combat, Attack.Basic, Combo.Ground]
  signals: [side.action.start, side.action.hit]
```

### 10.4.2 与 GAS 的对比（面试高频）

- GAS（UE）核心是 AttributeSet + GameplayEffect + 网络预测，
  面向大型 3A 与多人。
- WAO 聚焦**单机 2D 多玩法复用**：意图来源（输入/AI/脚本）统一、
  配方数据化（连招窗口/取消/前摇后摇）、效果账本可追踪
  （`ActionDebugHistory` 记录每次结算，编辑器可查）。
- 横板、回合制、战棋、弹幕共享同一套 Recipe/Effect 结构，
  各自模块只写"自己怎么产生意图、怎么消费效果"。

## 10.5 AI：敌人行为

`SideCombatEnemyAIService` 是轻量状态机（数据表驱动参数）：
`Idle → Chase → Attack → Recover`，距离/冷却/连招窗口从 tuning 读。
AI 产生动作也是 `ActionIntent`——**AI 与玩家共用动作入口**
（Part 7 的注入思想在 AI 侧同款）。

## 10.6 特效与 HUD

- **VFX**：`side.vfx.basic_slash` 这类效果 id 由 `ActionSignalRouter`
  路由——命中信号 → 刀光/粒子/命中停顿由 `SideCombatFeedbackService`
  消费（VFX 素材 Part 13 讲）。
- **HUD**：`SideCombatHudService` 每帧把战斗状态（血量/冷却/连击数）
  写进场景里的 HUD 实体（Part 2 的 UIImage + SubRect 体系）——
  HUD 是"数据驱动渲染"的日常形态。

## 10.7 实例：一次 basic 攻击的完整链路

玩家按下 J 键（或点技能图标）：

```
① 输入：IsActionPressed("side.basic") / 按钮 → CommandBus → InjectActionPress
② 动作服务：查配方 side.basic1（冷却 0.19 通过？）
      → 连招窗口内？→ 播放动画 side_basic1（Part 6 的 sheet 帧）
      → 音效 swing_light.wav、VFX side.vfx.basic_slash
③ 时间线：startup 0.06s → hitTime 0.06s 生成攻击判定盒
      → SideCombatHitboxService 与敌人碰撞 → HitResolution 结算
      （伤害 = 攻击 × 技能倍率，应用无敌帧/击退）
      → 信号 side.action.hit → Feedback（hit pause + 闪白 + 镜头）
④ 连招：cancelStart 0.12s 后玩家可再按 → basic2（Combo.Ground 链）
⑤ 账本：ActionDebugHistory 记录本次意图/配方/结算 → 调试器可查
```

这一条链路串起了 Part 2（HUD 渲染）、Part 6（sheet 动画帧）、
Part 7（动作层/注入）、Part 8（判定与物理）。

## 10.8 当前状态

- ✅ 数据表驱动（tuning YAML，500ms 热重载）
- ✅ 系统薄 + 服务厚（16 个服务）
- ✅ 输入统一（键盘/HUD 按钮/AI → 同一动作入口）
- ✅ WAO 动作编排（Intent/Recipe/Resolver/Effect/Ledger/Signal）
- ✅ AI 状态机、VFX 信号路由、HUD 数据驱动

下一步：**Part 11 其他玩法**——回合制/战棋/弹幕如何复用这套架构。
