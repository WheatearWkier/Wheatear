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

---

## 10.9 数据驱动扩展：效果、道具槽、技能槽（2026-08-16）

把"新效果类型"和"新技能"也推进到编辑器可做，分三层：

### 10.9.1 槽位表：道具槽与技能槽（`side_combat_tuning.yaml`）

```yaml
itemSlots:                      # 道具槽（表驱动轮询）
  - slot: 4                    # 加道具 = 加一行 + 输入绑定加动作
    actionId: side.item4
    kind: heal                 # heal / mana / attack_buff
    cooldown: 5.0
    recipeId: side.potion      # 可选：绑定 WAO 配方（见 10.9.2）
skillSlots:                    # 技能槽（表驱动触发）
  - slot: basic
    actionId: side.basic
    kind: basic                # basic/launcher/magic_bolt/dash/ally_support/break_limit/custom
    enabled: true
    customBehavior: berserk    # kind=custom 时使用的注册行为
```

- 玩家服务不再写死 `side.item1/2/3` 与六个技能动作——**遍历表轮询**，
  表里加一行 + 输入绑定加动作 = 新热键立即生效。
- `side:item:N` 命令路由同样查表映射到槽位的动作 ID。

### 10.9.2 效果扩展：公式表达式 + 自定义效果注册表

`EffectSpec` 新增两个数据字段（WAO 动作编辑器中直接编辑）：

```yaml
effects:
  - type: Damage
    value: 30
    formula: "min(target.max_health, target.health + 30)"  # 表达式覆盖 value
  - type: Damage
    customType: lifesteal      # 注册表效果
    value: 0.3                 # 吸血比例（handler 读 InValue）
```

- **表达式求值器**（`Gameplay/Action/EffectFormula`）：递归下降解析
  `+ - * / %`、比较、逻辑、`min/max/clamp/abs/round/floor/ceil/if`；
  变量为属性字典（`source.attack / target.health / controller.mana` 等）。
  求值结果覆盖 `Value`——"新效果"可以是纯数据（如吸血 = Damage +
  `Heal(formula: "source.attack * 0.3")`）。
- **效果注册表**（`Gameplay/Action/EffectRegistry`）：`Register("lifesteal", ...)`
  一次注册即全局可用，编辑器 Custom Effect 下拉自动列出。handler 只操作
  属性字典（不碰组件类型），由调用方写回——通用且安全。

### 10.9.3 技能行为注册表（`SideCombatSkillRegistry`）

`Register("berserk", "狂化", handler)` 一次注册后：
Skill Slots 面板 kind 选 `custom` + Behavior 下拉选它，即可绑键触发。
示例：`berserk`（扣 15% 血换 2 倍攻击 6 秒）。

**边界收敛到最小**：纯逻辑原语（物理、AI、状态机原语）仍要写一次 C++ handler
（≈10 行注册），但这是"注册一次、永久可在编辑器编排"，
不再是"改枚举 + 改所有 switch"。

---

## 10.10 架构边界与改进方向：以"弹反"为例（2026-08-16 讨论）

> 需求：敌方远程子弹飞来 → 玩家按键 → 子弹反向飞回（弹反）。
> 这是"纯编辑器能不能做"的试金石。

### 10.10.1 为什么纯编辑器做不了

弹反拆解后需要三样能力，当前架构只覆盖第一样：

| 需求 | 现有能力 | 差距 |
|---|---|---|
| 按键触发"弹反动作" | ✅ 技能槽表 + 输入绑定 | — |
| 弹反**窗口**（按键后 0.2s 内有效） | ❌ 自定义行为是一次性调用 | 需要**可挂起的每帧状态**（进入姿态 → 每帧检测 → 结束） |
| 找到来袭子弹 → 反向 → 换队 | ❌ 行为上下文只有玩家实体 | 需要**跨实体操作**：遍历场景子弹、改 `Velocity/Team` 组件 |

根因：自定义行为注册表（10.9.3）与表达式求值器（10.9.2）都建立在
**数值字典模型**上（`source.attack` 这种属性表），而弹反是
**跨实体交互 + 持续状态 + 物理修改**的行为——属性字典表达不了
"遍历子弹并把某个子弹的组件字段取反"。

### 10.10.2 与商业引擎的区别

- **UE GAS**：`GameplayAbility` 是 C++/蓝图类——"弹反"在 UE 里同样要
  写一个 Ability（蓝图节点也是代码），但 GAS 提供**能力生命周期**
  （激活/结束/取消）+ 目标数据（`TargetData` 直接引用实体），
  所以跨实体交互是框架原语，不是插件。
- **Unity**：`StatusEffect`/`Skill` 都要 C# 类；可视化脚本（Playmaker/GraphView）
  提供节点图，但节点本身仍是代码。商业引擎的共识是：
  **行为语义 = 代码（一次），行为编排 = 数据（之后）**。
- **Wheatear 当前**：比商业引擎多数据化了两层（效果类型注册表 +
  数值表达式），但**缺少"可挂起的行为状态机"和"实体查询原语"**——
  这是与商业引擎在"行为系统"上的核心差距。

### 10.10.3 改进路线（若要做）

1. **短期（推荐）**：弹反 = 一次 C++ 注册（约 40 行）：
   `SideCombatSkillRegistry` 注册 `parry`（进入弹反窗口状态）
   + 一个每帧更新钩子（检测来袭弹幕 → 反向）。之后窗口时长、
   反弹倍率、按键全部进编辑器调参。**"做一次"要代码，"调一辈子"是编辑器**。
2. **中期**：行为状态机原语——`BehaviorComponent`（可挂起的每帧行为栈）
   + 实体查询原语（`find projectiles in radius`），让"跨实体交互"类
   行为也能数据编排。约数千行，需谨慎设计。
3. **远期**：可视化行为图（节点 = 注册原语，连线 = 数据）——即
   "技能蓝图"，工程量大且调试成本陡增，不建议近期做。

**结论**：弹反现在是"一次 C++ 注册 + 编辑器调参"；
纯编辑器化需要行为状态机系统，属于下一个架构里程碑。

---

## 10.11 敌人波次数据驱动（2026-08-16）

新关卡不再需要把敌人实体拖进视口。两层数据 + 一个运行时生成器：

### 10.11.1 波次表（场景组件 `SideCombatLevelComponent.WaveSpawns`）

```yaml
WaveSpawns:
  - Enabled: true
    WaveIndex: 0        # 波次（0-based，与 SideEnemyAIComponent.WaveIndex 一致）
    EnemyKind: 0        # 0=Grunt 1=Thrower 2=Pouncer（Boss 走场景 BossEntityName）
    Count: 2            # 数量
    SpawnMinX: -4.2     # 出生 X 范围（均布）
    SpawnMaxX: -2.4
    GroundYOffset: 0
    HpVariance: 0.08    # 每只 HP ±8% 抖动
```

Inspector 直接增删行（撤销栈）；表非空时运行时移除场景静态小兵（Boss 保留）
并按表生成敌人 + `{Tag}_Shadow` 影子实体，波次激活 / 动画桥接 / 掉落全走既有管线。

### 10.11.2 敌人种类模板（tuning `enemyTypes`）

每种 `SideEnemyKind` 一行：数值（HP/攻/防/速/碰撞）+ AI 参数（索敌/攻击距离/间隔）
+ 渲染与阴影缩放。Tuning 面板「敌人种类」页签编辑。**新种类仍需一次 C++**（枚举 + AI 分支），
之后即可纯数据消费——与 10.9/10.10 的边界判断一致。

### 10.11.3 场景迁移示例

`SideCombatBeastPath.wt` 删除 270 行静态敌人实体，改为 2 条波次记录
（波1×2 + 波2×3，HP 浮动 8%），运行日志验证 `spawned 5 enemy(ies)`。
