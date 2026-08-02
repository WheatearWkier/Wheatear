# WAO 当前落地状态

更新日期：2026-08-01

## 当前目标

WAO（Wheatear Action Orchestration）目前作为 Wheatear 的统一动作语义层使用。
它不替代每种玩法自己的状态机，也不把横板浮空、弹幕投射物、战棋格子、回合指令硬塞进同一个万能类。

当前边界是：

- WAO 负责：动作 id、显示名、图标、冷却、资源消耗、动作时序、命中帧、取消窗口、动作标签、效果预览、音效/动画/VFX 信号。
- 玩法服务负责：每种玩法自己的输入、AI、目标选择、物理/格子/回合规则、实际命中判定、最终玩法循环。

## 已接入内容

### 公共核心

已落地在 `Wheatear/src/Wheatear/Gameplay/Action/`：

- `ActionTypes`：动作意图、动作配方、效果描述、属性表、运行状态、效果账本。
- `ActionDatabase`：运行时动作配方注册与查询。
- `ActionRunner`：轻量动作执行器，可处理资源、冷却、状态、属性变化和效果账本。
- `StateRegistry`：公共状态注册、叠加、移除、回合 tick、伤害倍率和防御倍率计算。

### 回合制 / 战棋

回合制和战棋已把重复的状态效果逻辑迁移为 `WAO::RuntimeState`。
防御、再生、燃烧、破甲、眩晕等效果现在走公共 `StateRegistry`。

这一步解决的是“Buff/Debuff 规则重复”的问题。

### 弹幕假玩法

弹幕玩法已新增 `ArcadeCombatActionCatalog`。

已纳入 WAO 的动作：

- `arcade.gun`
- `arcade.cannon`
- `arcade.katana`
- `arcade.boss_bullet`

现在玩家武器发射会从 WAO recipe 读取冷却和主伤害值。
Boss 发弹也拥有 WAO recipe，伤害从 recipe 读取；AI 发弹节奏仍优先使用 Boss 组件里的 `ShootInterval`，方便场景内直接调参。

弹幕飞行、碰撞、掩体阻挡、近战 slash 生命周期仍由 `ArcadeCombatProjectileService` 管理。

### 横板格斗

横板格斗已新增 `SideCombatActionCatalog`。

已纳入 WAO 的动作：

- `side.basic1`
- `side.basic2`
- `side.basic3`
- `side.air_basic`
- `side.launcher`
- `side.air_chase`
- `side.magic_bolt`
- `side.ally_support`
- `side.break_limit`
- `side.enemy_claw`
- `side.bear_charge`
- `side.bear_shockwave`

玩家和敌人每次启动动作时，会根据 `side_combat_tuning.yaml` 生成对应 WAO recipe，并注册到 `ActionDatabase`。

`SideCombatActionService` 现在会读取 recipe 的：

- 动作持续时间
- 命中帧时间
- 取消窗口
- 移动倍率
- 音效路径
- 运行态 recipe id

冷却和魔剑槽消耗也通过 recipe 回填。
横板真实命中、浮空、断限、Boss 保护条、hitbox 扫描、纵深移动仍保留在横板玩法服务中。

## 设计理由

这轮没有把四种玩法硬塞成同一个“大 GAS 组件”，原因是玩法骨架差异很大：

- 横板格斗核心是动作帧、hitbox、浮空、断限和保护条。
- 弹幕玩法核心是投射物密度、飞行、遮挡和 Boss 行为。
- 回合制核心是指令、回合顺序、目标选择和状态 tick。
- 战棋核心是格子、路径、占位、行动次数和范围。

公共部分抽到 WAO；差异部分留在玩法服务。
这样以后加新玩法时，可以复用 ActionRecipe / Effect / State / Ledger / Signal 的思想，同时保留玩法自己的手感和规则。

## 编辑器调试工具

已新增 `WAO Action Debugger` 编辑器面板。

打开方式：

```text
View -> WAO Action Debugger
```

当前面板能力：

- 查看 `ActionDatabase` 中所有已注册 action recipe。
- 按 id、标签、名称、描述过滤。
- 查看动作基础信息：图标、动画、音效、VFX、冷却、动作时序、命中帧、取消窗口、移动倍率。
- 查看资源消耗，例如断限追击的 `magic_sword` 消耗。
- 查看效果预览，例如 Damage、Launch、AddState、Signal。
- 查看最近运行账本，即当前运行中触发过哪些 WAO action。
- 清空运行账本，方便重新测试一段动作流程。

运行账本由 `ActionDebugHistory` 提供。
它是轻量调试设施，不改变正式战斗结算，只记录最近的 action 事件和效果条目。

当前写入账本的来源：

- `ActionRunner::Execute`
- 弹幕玩家武器发射
- 弹幕 Boss 发弹
- 横板玩家动作启动
- 横板敌人动作启动

这意味着进入 Play 模式后，测试弹幕或横板战斗，再打开 WAO Action Debugger，就可以看到最近动作痕迹。

每次编辑器进入 Play 模式时会清空运行账本，确保本轮调试只显示当前试玩会话的动作记录。
Sandbox 启动时也会清空一次运行账本。

## SignalRouter 落地状态

已新增 `ActionSignalRouter`，作为 WAO 的表现信号路由层。

它解决的问题是：ActionRunner 不应该直接认识音效、VFX、UI、hit pause、震屏、事件脚本等具体系统。
WAO 只发出结构化信号，具体玩法或编辑器工具再注册 handler 响应。

当前已接入的信号来源：

- `ActionRunner` 执行 `EmitSignal` 效果时会发出 signal。
- 横板玩家/敌人启动 recipe 动作时，会发出 recipe 里的 signals。
- 弹幕玩家武器发射和 Boss 发弹时，会发出 recipe 里的 signals。

当前 `ActionSignalRouter` 仍然是轻量路由层，暂时没有强行接管现有音效/VFX。
这是有意保守处理：先建立统一表现信号出口，再逐步把音效、VFX、hit pause、震屏迁成 handler。
这样不会一次性打乱已有战斗手感。

## 下一步建议

1. 给 WAO 增加 `ActionResolver` 接口，让每种玩法把 recipe 转换为自己的命中/目标/结算逻辑。
2. 把横板命中后的伤害、浮空、保护条变化逐步生成 `EffectLedger`，让调试面板能看到“一次攻击到底发生了什么”。
3. 把弹幕投射物创建包装为 action signal handler，让武器图标、弹体、音效和冷却数据完全从同一个 recipe 出发。
4. 给 `ActionSignalRouter` 增加默认 handler 分类，例如 audio、vfx、feedback、ui、script。
5. 给每个玩法模块补一页“玩法模块边界说明”，新增玩法时按同一模板拆分 System、Service、Catalog、EditorPanel。

## 2026-08-01 三点落地更新

本轮把 WAO 从“代码里有 recipe 和账本”继续推进到“可被工程、编辑器、数据资产共同使用”的状态。

### 1. ActionResolver / ActionOrchestrator

新增 `ActionResolver` 和 `ActionOrchestrator`，位置：

```text
Wheatear/src/Wheatear/Gameplay/Action/ActionResolver.h
Wheatear/src/Wheatear/Gameplay/Action/ActionResolver.cpp
```

职责分界如下：

- `ActionDatabase` 只负责保存 action recipe。
- `ActionOrchestrator` 负责根据 `ActionIntent` 找 recipe，并把执行交给合适的 resolver。
- `ActionResolverRegistry` 按 action id 前缀注册玩法适配器，例如 `arcade.`、`side.`、`turn.`、`tactical.`。
- 没有玩法专属 resolver 但传入了 `ActionRuntime` 时，仍可回落到轻量 `ActionRunner`。

这样 WAO 不直接知道横板浮空、弹幕投射物、回合顺序、战旗格子这些玩法细节；它只统一“动作意图、动作配方、效果账本、表现信号”的入口。

### 2. 玩法接入状态

当前接入状态：

- 弹幕假玩法：`ArcadeCombatActionResolver` 负责把 `arcade.*` action 转成 projectile spawn signal，玩家武器和 Boss 发弹都已经走 orchestrator。
- 横板格斗：玩家/敌人动作起手时走 orchestrator，recipe 的图标、音效、动作元数据、冷却、资源消耗和信号统一从 WAO 读取；真实命中、浮空、断限、Boss 保护条仍保留在横板专属服务。
- 回合制：技能仍由 `TurnCombatActionService` 按回合规则实际结算，但所有技能都有 `turn.*` recipe，可进入 WAO 数据库、调试面板和效果账本。
- 战旗：技能仍由 `TacticalCombatActionService` 按格子规则实际结算，但所有技能都有 `tactical.*` recipe，可进入 WAO 数据库、调试面板和效果账本。

这是刻意设计的边界：公共层统一语义和调试，玩法层保留手感和规则。

### 3. 数据资产化

新增 YAML action 数据目录：

```text
WheatearEditor/assets/gameplay/actions/
```

当前文件：

```text
00_arcade_actions.yaml
10_side_combat_actions.yaml
20_turn_combat_actions.yaml
30_tactical_combat_actions.yaml
```

引擎启动模块时会调用：

```cpp
WAO::ActionAssetLoader::LoadDirectory("assets/gameplay/actions");
```

加载顺序上先注册 C++ 默认 recipe，再加载 YAML，所以 YAML 可以覆盖显示名、图标、音效、VFX、标签、效果预览、资源消耗和表现信号。横板格斗的手感数值仍以 `side_combat_tuning.yaml` 为主，action YAML 主要承担动作资产元数据和 WAO 调试语义。

## 2026-08-01 调试器生产力更新

`WAO Action Debugger` 已从单纯列表升级为按玩法模块组织的调试入口：

- 左侧 action 列表支持按 `arcade`、`side`、`turn`、`tactical` 模块折叠分组。
- 仍然保留 id、标签、名称、描述过滤，方便快速定位某个 recipe。
- Recipe 页新增 Authoring 区域，显示该 action 对应的 YAML、调参表、图标、音效和特效资源。
- 每个资源路径提供 `Open Folder` 和 `Copy Path`，缺失资源会直接标红 `Missing`。

这一步的意义是：WAO 不只是运行时战斗能力，也开始成为内容排错工具。以后策划或程序看到某个技能图标、音效、VFX 不对，可以从 action debugger 直接追到对应数据资产，而不是在代码和资源目录里盲找。
## 2026-08-02 Action Recipe 编辑落地

`WAO Action Debugger` 现在不只是查看器，也具备第一阶段 authoring 能力：

- Recipe 页新增 `Edit Recipe`。
- 可编辑显示名、描述、图标、动画、音效、VFX、冷却、前摇、命中帧、后摇、取消窗口、移动倍率。
- 可编辑 `tags`、`signals` 和 `resourceCost`，其中资源消耗格式为 `mana=12, sword=1`。
- 保存时写回 `assets/gameplay/actions/*.yaml`，并把当前 recipe 重新注册到 `ActionDatabase`。

这一步的定位是“策划/程序都能快速修一条 action 的常用参数”。它不是最终形态的完整技能编辑器：`Effects` 表暂时仍以查看为主，因为伤害、浮空、状态、信号等效果的行级编辑需要更严格的类型化 UI，后续应单独做成 Effect Editor。

## 2026-08-02 用户设置与输入绑定

用户偏好已经从 `GameProgress` 进度存档里拆出，落到 `UserSettings`：

```text
Wheatear/src/Wheatear/Core/UserSettings.h
Wheatear/src/Wheatear/Core/InputBindingService.h
assets/saves/user_settings.wtsettings
```

设计边界：

- `UserSettings` 保存玩家/开发者偏好：音量、全屏、屏幕震动、文字速度、按键绑定。
- `GameProgress` 保存游戏进度：章节、装备、技能、材料、好感度、地城结果。
- `InputBindingService` 是玩法读取输入的唯一中间层，实时玩法不再直接写死键位。

当前已经接入：

- 横板格斗：移动、跳跃、普攻、魔法、支援、断限、暂停。
- 类元气假玩法：移动、攻击、武器 1/2/3、暂停。
- VN：推进、自动、历史、保存、读取。

后续编辑器可以在这层之上做“改键面板”和“玩法默认键位模板”，不会再侵入各个玩法系统。
