# 竖切 Boss 与材料副本配置

合并黑熊丈夫 Boss 参数表与黑林兽道副本配置表，便于一起维护第二章战斗内容。

---

## 来源：竖切Boss与材料副本配置.md

## 《Wheatear 异世界项目》黑熊丈夫 Boss 参数表 v0.3

### 0. 文档定位

本文档负责竖切 Boss `BOSS_CH02_BearHusband` 的战斗参数、阶段、招式、AI、掉落、评分和实现检查。

设计目标：

- 作为正式横板战斗的第一只 Boss。
- 教玩家读招、闪避、一段跳、上挑、跳斩续空、火球补 hit。
- 普通玩家不需要断限追击即可通关。
- 熟练玩家可以通过短空连和完美闪避更快击败。
- 不在第二章正式教学 Boss 保护条，保护机制只做隐藏防无限连。

### 1. 基础信息

| 字段 | 值 |
| --- | --- |
| `BossId` | `BOSS_CH02_BearHusband` |
| 显示名 | 黑熊丈夫 |
| 所属章节 | 第二章：真青梅送魔剑与真正的新手教程 |
| 所属副本 | `CH02_MAIN_BearAwakening` |
| 首次场景 | `SCN_CH02_Boss` 或教程同场景 R5 |
| Boss 类型 | 新手教程 Boss |
| 推荐主角等级 | Lv 4-6 |
| 推荐魔剑等级 | Lv 1-2 |
| 推荐技能 | 三段斩、裂空挑斩、跳斩、火球术、闪避、一段跳 |
| 是否需要断限 | 否 |
| 是否显示保护条 | 否，除非开发调试打开 `showBossProtectionHud` |
| 是否可复战 | 竖切可先不做，后续作为 Boss 回放 |

### 2. 推荐玩家基准

用于调参的默认玩家属性：

| 属性 | 建议值 |
| --- | --- |
| Level | 5 |
| HP | 420 |
| MP | 80 |
| SP | 110 |
| ATK | 38 |
| MATK | 34 |
| DEF | 20 |
| MDEF | 14 |
| 已解锁技能 | 三段斩、裂空挑斩、基础跳斩、火球术、闪避、一段跳 |
| 装备 | 旅人护衣 + 旧护符 |

目标战斗时长：

| 玩家类型 | 目标时长 |
| --- | --- |
| 新手玩家 | 120-160 秒 |
| 普通玩家 | 90-120 秒 |
| 熟练玩家 | 55-85 秒 |

### 3. Boss 属性

| 属性 | 普通难度建议 |
| --- | --- |
| HP | 3600 |
| ATK | 44 |
| MATK | 0 |
| DEF | 22 |
| MDEF | 12 |
| Poise | 85 |
| BreakGauge | 100 |
| HiddenProtectionMax | 100 |
| LaunchWeight | 95 |
| MoveSpeed | 3.4 |
| TurnSpeed | 中 |
| ContactDamage | 18 |

说明：

- `HiddenProtectionMax` 只用于防止玩家在教程 Boss 身上无限控死，默认不显示 UI。
- Boss 只有在破防、冲撞撞空、地震后疲劳等窗口可被明显浮空。
- 平时可以被打出硬直，但不应该被轻松高浮空。

### 4. 阶段设计

| 阶段 | HP | 行为主题 | 新增机制 |
| --- | --- | --- | --- |
| P1 | 100%-60% | 基础爪击和短冲撞 | 教闪避冲撞后反击 |
| P2 | 60%-20% | 地面震波和低跳扑击 | 教一段跳躲地面攻击 |
| P3 | 20%-0% | 攻击频率提高但破绽更大 | 教别慌，抓疲劳窗口输出 |

阶段切换演出：

- P2：Boss 后退咆哮，前爪拍地，地面出现裂纹。
- P3：Boss 身上魔光增强，动作更急躁，冲撞后疲劳时间增加。

### 5. 招式参数

#### 5.1 ClawSwipe：爪击

| 字段 | 值 |
| --- | --- |
| `AttackId` | `BH_ClawSwipe` |
| 阶段 | P1-P3 |
| 前摇 | 0.45s |
| 有效帧 | 0.18s |
| 后摇 | 0.55s |
| 伤害 | 42 |
| 削韧 | 18 |
| 击退 | 小 |
| 可闪避 | 是 |
| 可防御 | 后续剑盾加入后可防，竖切无 |
| 破绽 | 后摇可接三段斩或上挑 |

用途：

- 基础近战压力。
- 给玩家练习不要站在 Boss 正面。

#### 5.2 DoubleClaw：双爪连击

| 字段 | 值 |
| --- | --- |
| `AttackId` | `BH_DoubleClaw` |
| 阶段 | P2-P3 |
| 前摇 | 0.50s |
| 两段间隔 | 0.28s |
| 每段伤害 | 36 |
| 后摇 | 0.75s |
| 削韧 | 15 x2 |
| 破绽 | 第二段结束后 0.75s |

用途：

- 教玩家不要被第一段骗出反击。
- 第二段结束后给明确输出窗口。

#### 5.3 ShortCharge：短冲撞

| 字段 | 值 |
| --- | --- |
| `AttackId` | `BH_ShortCharge` |
| 阶段 | P1-P3 |
| 预警 | 低头刨地，0.75s |
| 冲刺时长 | 0.65s |
| 伤害 | 68 |
| 击退 | 中 |
| 撞墙 / 撞空后疲劳 | P1 1.20s，P2 1.40s，P3 1.65s |
| 可跳过 | 可，但推荐闪避 |
| 可打断 | 不可 |
| 破绽 | 疲劳期间 BreakDamageRate x1.5 |

用途：

- 第一核心教学招。
- 玩家闪避后可以接 `三段斩 -> 裂空挑斩 -> 跳斩 -> 火球术`。

#### 5.4 LowLeapSmash：低跳扑击

| 字段 | 值 |
| --- | --- |
| `AttackId` | `BH_LowLeapSmash` |
| 阶段 | P2-P3 |
| 前摇 | 0.65s |
| 跳跃时长 | 0.55s |
| 落地范围 | 半径 1.7m |
| 伤害 | 74 |
| 后摇 | 0.85s |
| 可躲避 | 闪避或向外移动 |
| 可反击 | 落地后可火球或冲刺斩 |

用途：

- 让玩家识别空中威胁。
- 不要求空战处理，只要求躲开后反击。

#### 5.5 GroundShock：地面震波

| 字段 | 值 |
| --- | --- |
| `AttackId` | `BH_GroundShock` |
| 阶段 | P2-P3 |
| 前摇 | 0.90s |
| 震波速度 | 慢 |
| 伤害 | 58 |
| 异常 | 短硬直 0.35s |
| 躲避方式 | 一段跳 |
| 后摇 | 1.00s |
| 破绽 | 后摇期间可用火球、冲刺斩或上挑 |

用途：

- 第二核心教学招。
- 明确告诉玩家：没有二段跳也能躲，关键是起跳时机。

#### 5.6 Roar：咆哮压制

| 字段 | 值 |
| --- | --- |
| `AttackId` | `BH_Roar` |
| 阶段 | P2-P3 |
| 前摇 | 0.50s |
| 范围 | 前方扇形 |
| 伤害 | 0 |
| 效果 | 轻微推开，打断贪刀 |
| 冷却 | 12s |
| 破绽 | 无明显破绽 |

用途：

- 防止玩家一直贴身无脑输出。
- 不造成伤害，避免新手挫败。

#### 5.7 FrenzySwipe：狂乱挥击

| 字段 | 值 |
| --- | --- |
| `AttackId` | `BH_FrenzySwipe` |
| 阶段 | P3 |
| 前摇 | 0.55s |
| 连击数 | 3 |
| 每段伤害 | 34 |
| 后摇 | 1.10s |
| 破绽 | 后摇明显，可短空连 |

用途：

- P3 压迫感。
- 让玩家在 Boss 急躁时反而抓大破绽。

### 6. AI 行为权重

#### P1 行为权重

| 行为 | 权重 | 条件 |
| --- | --- | --- |
| ClawSwipe | 45 | 玩家近距离 |
| ShortCharge | 35 | 玩家中距离 |
| Backstep | 10 | 玩家连续近身攻击超过 3 秒 |
| IdleThreat | 10 | 调整节奏 |

#### P2 行为权重

| 行为 | 权重 | 条件 |
| --- | --- | --- |
| ClawSwipe | 25 | 近距离 |
| DoubleClaw | 20 | 近距离且玩家未闪避 |
| ShortCharge | 25 | 中距离 |
| GroundShock | 20 | 玩家远离或频繁贴地 |
| LowLeapSmash | 10 | 玩家长时间远离 |

#### P3 行为权重

| 行为 | 权重 | 条件 |
| --- | --- | --- |
| FrenzySwipe | 25 | 近距离 |
| ShortCharge | 30 | 中距离 |
| GroundShock | 20 | 玩家贴地或远离 |
| LowLeapSmash | 15 | 玩家远离 |
| Roar | 10 | 玩家连续输出过久 |

AI 限制：

- 同一高伤技能不能连续使用超过 2 次。
- `GroundShock` 后至少间隔 7 秒。
- P3 可以更频繁攻击，但每次大动作后的破绽也更明显。
- 玩家血量低于 25% 时，不连续使用两个高伤技能，给新手喘息。

### 7. 破防与浮空规则

#### 7.1 BreakGauge

| 内容 | 值 |
| --- | --- |
| BreakGauge | 100 |
| 普攻削减 | 每段 4 / 5 / 7 |
| 裂空挑斩削减 | 14 |
| 跳斩削减 | 每段 5 |
| 火球术削减 | 10 |
| 冲撞疲劳期间削减倍率 | x1.5 |
| 破防持续 | 4.0s |

破防后：

- Boss 可被明显浮空。
- 上挑浮空高度提高。
- 跳斩命中滞空生效。
- 火球可用于补 hit。
- 破防结束后 Boss 强制落地并后退。

#### 7.2 隐藏保护

| 内容 | 值 |
| --- | --- |
| HiddenProtectionMax | 100 |
| 三段斩保护增长 | +4 / +5 / +8 |
| 跳斩保护增长 | +7 / hit |
| 裂空挑斩保护增长 | +12 |
| 空中追斩保护增长 | +9 |
| 火球保护增长 | +5 |
| 真青梅支援保护增长 | +10 |
| 保护满槽脱离时间 | 1.15s |
| 中立保护衰减 | 16 / s |

规则：

- 第二章不显示保护条。
- 保护到上限后 Boss 落地并后退，不触发断限教学。
- 导师可以用台词暗示“别贪太久，落地后重新来”。
- 开发调试可在 `side_combat_tuning.yaml` 中打开 `showBossProtectionHud` 和 `showCombatStateHud` 检查保护条与状态机。
- 第二章的 `L` 测试权限来自 `progression.profiles.CH02_MAIN_BearAwakening.debugSkills`，不是全局正式开放。

### 8. 推荐连招

普通玩家路线：

```text
闪避冲撞 -> 三段斩 -> 裂空挑斩 -> 跳斩 -> 落地 -> 火球术
```

更稳路线：

```text
等待地面震波 -> 一段跳躲避 -> 落地火球 -> 三段斩 -> 后撤
```

熟练玩家路线：

```text
完美闪避 -> 三段斩 1-2 -> 裂空挑斩 -> 跳跃 -> 跳斩 -> 火球术 -> 落地取消 -> 疾风冲刺斩
```

设计要求：

- 普通路线必须能打赢。
- 熟练路线只缩短时间和提高评分。
- 不要求断限追击。

### 9. 教学触发

| Trigger | 条件 | 导师提示 |
| --- | --- | --- |
| `Boss_FirstChargeWindup` | 第一次短冲撞前摇 | 它低头时别站正面，闪开。 |
| `Boss_FirstChargeMissed` | 玩家成功躲开冲撞 | 现在，上挑。 |
| `Boss_FirstGroundShock` | 第一次地震前摇 | 震波贴地，跳起来。 |
| `Boss_PlayerGreedyPunished` | 玩家连续近身被咆哮推开 | 贪刀会被逼退，等破绽。 |
| `Boss_FirstBreak` | 首次破防 | 这才是输出窗口。把它打到空中。 |
| `Boss_Phase3Start` | P3 开始 | 它急了，破绽会更大。稳住。 |

失败提示见 [竖切VN剧本](竖切VN剧本.md)。

### 10. 伤害调参

建议以玩家有效生命计算：

```text
PlayerEHP = PlayerHP * (1 + PlayerDEF / 100)
```

竖切默认：

```text
PlayerEHP = 420 * (1 + 20 / 100) = 504
```

目标承受次数：

| 招式 | 伤害 | 约可承受次数 |
| --- | --- | --- |
| ClawSwipe | 42 | 12 次 |
| DoubleClaw 全中 | 72 | 7 次 |
| ShortCharge | 68 | 7 次 |
| LowLeapSmash | 74 | 6-7 次 |
| GroundShock | 58 | 8 次 |
| FrenzySwipe 全中 | 102 | 4-5 次 |

调参原则：

- 新手被单招命中不应立刻崩盘。
- 连续站桩吃完整连击会失败。
- P3 高压但破绽变大，鼓励冷静反击。

### 11. 掉落表

#### 首通强制奖励

| ItemId | 数量 | 用途 |
| --- | --- | --- |
| `MAT-MAGIC-CORE-T0` | 2 | 魔剑 Lv2、火球术 |
| `MAT-BEAST-SINEW` | 3 | 跳斩强化、上挑强化 |
| `MAT-BEAST-CLAW` | 5 | 基础近战技能 |
| `EQ-T0-ACC-001` | 1 | 旧护符，基础 SP |

首通写入：

- `Flag_CH02_BossDefeated`
- `Flag_HubUnlocked`
- `Flag_CH02_Mat_BeastPathUnlocked`

#### 常规掉落

| 掉落 | 概率 | 数量 |
| --- | --- | --- |
| `MAT-BEAST-CLAW` | 100% | 3-6 |
| `MAT-BEAST-SINEW` | 100% | 2-4 |
| `MAT-MAGIC-CORE-T0` | 70% | 1-2 |
| `EQ-T0-ARM-002` 黑林皮甲 | 25% | 1 |
| `EQ-T0-ACC-002` 兽牙坠饰 | 18% | 1 |
| `EQ-T0-ACC-003` 初级魔晶戒 | 10% | 1 |

评分加成：

| 评分 | 追加 |
| --- | --- |
| B | 额外 `MAT-BEAST-CLAW` 1-2 |
| A | 额外装备抽取 1 次，低权重 |
| S | 额外装备抽取 1 次，高权重，`MAT-MAGIC-CORE-T0` +1 |

### 12. 评分规则

| 项目 | S | A | B | C |
| --- | --- | --- | --- | --- |
| 通关时间 | <= 80s | <= 115s | <= 160s | 超过 |
| 受击次数 | <= 3 | <= 6 | <= 10 | 超过 |
| 最大连击 | >= 20 | >= 14 | >= 8 | 低于 |
| 死亡次数 | 0 | 0 | <= 1 | 超过 |

竖切阶段不要求断限评分。

### 13. 动画和音效需求

动画：

- Idle。
- Walk。
- Turn。
- ClawSwipe。
- DoubleClaw。
- ChargeWindup。
- ChargeRun。
- ChargeRecover。
- LowLeap。
- GroundShockWindup。
- GroundShockImpact。
- Roar。
- Break。
- PhaseRoar。
- Death。

音效：

- 咆哮。
- 爪击挥空。
- 爪击命中。
- 冲撞起步。
- 冲撞撞地。
- 地震。
- 破防。
- 死亡。
- 魔核掉落。

视觉提示：

- 冲撞前低头刨地，脚下扬尘。
- 地震前双爪抬起，地面裂纹高亮。
- P3 身上魔光增强。
- 破防时头部低垂，身体短暂停顿。

### 14. 数据结构建议

#### BossDefinition

| 字段 | 值 |
| --- | --- |
| `BossId` | `BOSS_CH02_BearHusband` |
| `EnemyRank` | `TutorialBoss` |
| `StatsProfile` | `STATS_BH_Normal` |
| `PhaseProfile` | `PHASE_BH_3Phase` |
| `AIProfile` | `AI_BH_TutorialBoss` |
| `DropTable` | `DROP_BH_FirstClear` / `DROP_BH_Repeat` |
| `BreakProfile` | `BREAK_BH_Tutorial` |
| `ProtectionProfile` | `PROT_BH_Hidden` |
| `TutorialTriggers` | `TUT_BH_*` |

### 15. 实现检查

实现完成后检查：

- Boss 三阶段能按 HP 正确切换。
- 冲撞前摇清晰。
- 地震能被一段跳躲过。
- 冲撞撞空后有足够破绽。
- 破防期间可短空连。
- 不使用断限也能打赢。
- Boss 不会被无限浮空。
- 掉落写入背包。
- 首通写入据点解锁标记。
- 打包后 Boss 贴图、音效、掉落表都能加载。

---

## 来源：竖切Boss与材料副本配置.md

## 《Wheatear 异世界项目》黑林兽道副本配置表 v0.2

### 0. 文档定位

本文档负责竖切可重刷材料副本 `CH02_MAT_BeastPath` 的房间配置、敌人波次、掉落、评分、解锁和实现检查。

副本定位：

- 竖切第一个可重刷副本。
- 验证据点 UI 的副本选择、材料来源跳转、掉落结算、装备获取和回据点流程。
- 给玩家练习第二章学到的短空连：普攻、上挑、跳斩、火球补 hit。
- 不引入断限追击，不引入复杂随机地图。

### 1. DungeonDefinition

| 字段 | 值 |
| --- | --- |
| `DungeonId` | `CH02_MAT_BeastPath` |
| 显示名 | 黑林兽道 |
| 所属章节 | 第二章 |
| 类型 | 材料副本 |
| 入口 | `SCN_Hub_Prototype` |
| 场景 | `SCN_CH02_Mat_BeastPath` |
| 出口 | `SCN_Hub_Prototype` |
| 解锁条件 | `Flag_CH02_BossDefeated` + `Flag_CH02_Mat_BeastPathUnlocked` |
| 推荐等级 | Lv 4-8 |
| 推荐魔剑等级 | Lv 2 |
| 推荐技能 | 裂空挑斩、基础跳斩、火球术 |
| 首通时长 | 4-6 分钟 |
| 熟练重刷 | 3-4 分钟 |
| 可重刷 | 是 |
| 失败处理 | 返回据点，可保留 20% 经验，不给完整掉落 |

### 2. 副本目标

玩家目标：

- 清理黑林兽道上的魔物。
- 击败兽道尽头的精英兽。
- 获得兽爪、魔物筋腱、初级魔核和初级装备。

系统目标：

- 验证材料副本入口。
- 验证敌人死亡掉落。
- 验证副本结算奖励。
- 验证装备掉落进入背包。
- 验证材料来源跳转。
- 验证重刷不会重复给主线首通关键奖励。

### 3. 房间序列

竖切使用固定 5 房间结构。

```text
R0_Entry
-> R1_ClawBeastPractice
-> R2_ProjectileDodge
-> R3_MixedAirCombo
-> R4_EliteAlpha
-> R5_ResultExit
```

### 4. RoomDefinition

#### R0_Entry：兽道入口

| 字段 | 值 |
| --- | --- |
| `RoomId` | `BP_R0_Entry` |
| `RoomType` | Entry |
| `LayoutId` | `LAYOUT_BP_EntryShort` |
| `CameraMode` | HorizontalFollow |
| `MusicCue` | `BGM_BeastPath_Loop` |
| `ClearCondition` | 玩家走到右侧触发器 |
| `TeachingObjective` | 告诉玩家这是可重刷副本 |

入场文本：

| Speaker | Text |
| --- | --- |
| 系统 | 黑林兽道：可重刷材料副本。 |
| 魔剑士导师 | 这里的魔物弱一些，适合练剑，也适合收集魔剑需要的材料。 |

奖励：

- 无战斗奖励。

#### R1_ClawBeastPractice：爪兽练习

| 字段 | 值 |
| --- | --- |
| `RoomId` | `BP_R1_ClawBeastPractice` |
| `RoomType` | Combat |
| `LayoutId` | `LAYOUT_BP_FlatSmall` |
| `CameraMode` | CombatLockSoft |
| `ClearCondition` | 所有敌人死亡 |
| `TeachingObjective` | 三段斩、上挑、跳斩 |

波次：

| Wave | 敌人 | 数量 | 刷新位置 | 条件 |
| --- | --- | --- | --- | --- |
| 1 | `EN_T0_ClawBeast` | 3 | 前方地面 | 入场 0.5s |
| 2 | `EN_T0_ClawBeast` | 2 | 左右夹击 | Wave 1 全灭后 1s |

导师提示：

- 第一次进入：`小型魔物很容易被挑空。先用它们练稳定。`

#### R2_ProjectileDodge：投石躲避

| 字段 | 值 |
| --- | --- |
| `RoomId` | `BP_R2_ProjectileDodge` |
| `RoomType` | Combat |
| `LayoutId` | `LAYOUT_BP_SlightSlope` |
| `CameraMode` | CombatLockSoft |
| `ClearCondition` | 所有敌人死亡 |
| `TeachingObjective` | 一段跳躲远程，冲刺接近 |

波次：

| Wave | 敌人 | 数量 | 刷新位置 | 条件 |
| --- | --- | --- | --- | --- |
| 1 | `EN_T0_Thrower` | 2 | 高台 / 远端 | 入场 |
| 1 | `EN_T0_ClawBeast` | 2 | 地面近端 | 入场 |
| 2 | `EN_T0_Thrower` | 1 | 远端 | 玩家击杀任意投石敌人后 |

导师提示：

- 第一次投石：`投石轨迹很慢，一段跳就够。跳过石头后再追上去。`

#### R3_MixedAirCombo：混合短空连

| 字段 | 值 |
| --- | --- |
| `RoomId` | `BP_R3_MixedAirCombo` |
| `RoomType` | Combat |
| `LayoutId` | `LAYOUT_BP_WideFlat` |
| `CameraMode` | CombatLockSoft |
| `ClearCondition` | 所有敌人死亡 |
| `TeachingObjective` | 近战 + 远程混编，火球补 hit |

波次：

| Wave | 敌人 | 数量 | 刷新位置 | 条件 |
| --- | --- | --- | --- | --- |
| 1 | `EN_T0_ClawBeast` | 3 | 中央 | 入场 |
| 1 | `EN_T0_Thrower` | 1 | 右侧高台 | 入场 |
| 2 | `EN_T0_Pouncer` | 2 | 左右边缘 | Wave 1 剩余 1 只时 |

`EN_T0_Pouncer` 是竖切可选敌人。如果工程时间紧，可以用 `EN_T0_ClawBeast` 替代。

导师提示：

- `敌人分散时，先处理远程。火球可以补掉最后一点硬直。`

#### R4_EliteAlpha：兽道精英

| 字段 | 值 |
| --- | --- |
| `RoomId` | `BP_R4_EliteAlpha` |
| `RoomType` | Elite |
| `LayoutId` | `LAYOUT_BP_EliteArena` |
| `CameraMode` | CombatLockHard |
| `ClearCondition` | 精英死亡 |
| `TeachingObjective` | 短空连、闪避冲撞、破防窗口 |

波次：

| Wave | 敌人 | 数量 | 刷新位置 | 条件 |
| --- | --- | --- | --- | --- |
| 1 | `EL_T0_BeastPathAlpha` | 1 | 中央 | 入场 |
| 1 | `EN_T0_ClawBeast` | 2 | 左右 | 入场 2s 后 |
| 2 | `EN_T0_Thrower` | 1 | 远端 | 精英 HP 50% |

精英简表：

| 字段 | 值 |
| --- | --- |
| HP | 900 |
| ATK | 30 |
| DEF | 12 |
| BreakGauge | 60 |
| HiddenProtectionMax | 45 |
| 主要招式 | 爪击、短扑、后撤 |
| 破防窗口 | 短扑落空后 1.2s |

导师提示：

- `它比普通魔物硬，但规律一样。让它扑空，再把它挑起来。`

#### R5_ResultExit：奖励与出口

| 字段 | 值 |
| --- | --- |
| `RoomId` | `BP_R5_ResultExit` |
| `RoomType` | Reward |
| `LayoutId` | `LAYOUT_BP_ExitCamp` |
| `CameraMode` | Free |
| `ClearCondition` | 玩家确认结算 |
| `TeachingObjective` | 回据点强化 |

出口文本：

| Speaker | Text |
| --- | --- |
| 系统 | 副本完成。获得材料、装备和评分奖励。 |
| 魔剑士导师 | 材料够了就回去整理。强大不是只靠打一场硬仗，是每一场之后都变得更稳。 |

### 5. EnemyDefinition 竖切补充

#### EN_T0_ClawBeast

| 字段 | 值 |
| --- | --- |
| 名称 | 低阶爪兽 |
| HP | 180 |
| ATK | 18 |
| DEF | 5 |
| 行为 | 接近、爪击、短后撤 |
| 浮空 | 容易 |
| 掉落 | 兽爪、骨片 |

#### EN_T0_Thrower

| 字段 | 值 |
| --- | --- |
| 名称 | 投石魔物 |
| HP | 140 |
| ATK | 16 |
| DEF | 4 |
| 行为 | 保持距离、低弧线投石 |
| 浮空 | 容易 |
| 掉落 | 石片、兽骨粉、铁片低概率 |

#### EN_T0_Pouncer

| 字段 | 值 |
| --- | --- |
| 名称 | 跳扑魔物 |
| HP | 220 |
| ATK | 22 |
| DEF | 6 |
| 行为 | 后撤、跳扑 |
| 浮空 | 中等 |
| 掉落 | 魔物筋腱、兽爪 |

#### EL_T0_BeastPathAlpha

| 字段 | 值 |
| --- | --- |
| 名称 | 黑林精英兽 |
| HP | 900 |
| ATK | 30 |
| DEF | 12 |
| 行为 | 爪击、短扑、咆哮、疲劳 |
| 浮空 | 破防时可明显浮空 |
| 掉落 | 初级魔核、魔物筋腱、黑林装备 |

### 6. EncounterGroup

| EncounterId | 敌人组合 | DifficultyBudget | ComboLesson | 推荐应对 |
| --- | --- | --- | --- | --- |
| `ENC_BP_R1_BASIC` | 3 爪兽 + 2 爪兽 | 1.0 | 上挑和跳斩 | ME-02、空中 J |
| `ENC_BP_R2_RANGE` | 2 投石 + 2 爪兽 + 1 投石 | 1.2 | 一段跳躲远程 | MO-01、ME-04 |
| `ENC_BP_R3_MIXED` | 3 爪兽 + 1 投石 + 2 跳扑 | 1.45 | 混编优先级、火球补 hit | MA-01 |
| `ENC_BP_R4_ELITE` | 1 精英 + 2 爪兽 + 1 投石 | 1.9 | 精英破防窗口 | 闪避、ME-02、基础跳斩 |

### 7. 掉落表

#### 小怪掉落

| DropTableId | 来源 | 掉落 |
| --- | --- | --- |
| `DROP_T0_ClawBeast` | 低阶爪兽 | 兽爪 1-2，骨片 0-1 |
| `DROP_T0_Thrower` | 投石魔物 | 石片 1-2，铁片 15%，兽骨粉 0-1 |
| `DROP_T0_Pouncer` | 跳扑魔物 | 兽爪 1-2，魔物筋腱 35% |

#### 精英掉落

| DropTableId | 来源 | 掉落 |
| --- | --- | --- |
| `DROP_BP_Alpha` | 黑林精英兽 | 魔物筋腱 2-3，初级魔核 1，装备抽取 35% |

#### 副本结算

| 评分 | 材料 | 装备 |
| --- | --- | --- |
| C | 兽爪 2，铁片 1 | 无 |
| B | 兽爪 3，魔物筋腱 1，铁片 1 | 普通装备 15% |
| A | 兽爪 4，魔物筋腱 2，初级魔核 1 | 优良装备 25% |
| S | 兽爪 5，魔物筋腱 2，初级魔核 1-2 | 稀有装备 20%，优良装备 35% |

装备池：

| ItemId | 名称 | 权重 |
| --- | --- | --- |
| `EQ-T0-ARM-001` | 旅人护衣 | 30 |
| `EQ-T0-ARM-002` | 黑林皮甲 | 28 |
| `EQ-T0-ACC-001` | 旧护符 | 25 |
| `EQ-T0-ACC-002` | 兽牙坠饰 | 14 |
| `EQ-T0-ACC-003` | 初级魔晶戒 | 3 |

保底：

- 每次通关至少获得 `MAT-BEAST-CLAW` 3。
- 每 3 次通关至少获得 `MAT-MAGIC-CORE-T0` 1。
- 每 5 次通关至少获得 1 件优良及以上装备。

### 8. 评分规则

| 项目 | S | A | B | C |
| --- | --- | --- | --- | --- |
| 通关时间 | <= 210s | <= 300s | <= 420s | 超过 |
| 受击次数 | <= 5 | <= 9 | <= 14 | 超过 |
| 最大连击 | >= 18 | >= 12 | >= 6 | 低于 |
| 死亡次数 | 0 | 0 | <= 1 | 超过 |

加分项：

- 所有房间清场。
- 至少一次 `上挑 -> 跳斩 -> 火球术`。
- 击败精英时没有死亡。

竖切不统计断限。

### 9. 副本解锁与返回

解锁：

```text
Flag_CH02_BossDefeated == true
Flag_CH02_Mat_BeastPathUnlocked == true
```

进入：

```text
HubAction_SelectDungeon(CH02_MAT_BeastPath)
-> LoadScene(SCN_CH02_Mat_BeastPath)
```

通关返回：

```text
DungeonResultPayload
-> AddExp
-> AddMaterials
-> AddEquipment
-> UpdateBestRank
-> ReturnToHub(SCN_Hub_Prototype)
```

失败返回：

```text
FailurePayload
-> AddExp(20%)
-> NoEquipmentReward
-> ReturnToHub(SCN_Hub_Prototype)
```

### 10. 材料来源联动

魔剑技能树缺材料时应能跳到本副本。

| 材料 | 来源说明 |
| --- | --- |
| 兽爪 | 黑林兽道小怪大量掉落 |
| 魔物筋腱 | 跳扑魔物和黑林精英兽掉落 |
| 初级魔核 | 黑林精英兽和 A/S 评价结算 |
| 铁片 | 投石魔物和结算奖励 |

技能树显示示例：

```text
火球术：缺少 初级魔核 1/2
来源：黑林兽道、黑熊丈夫
```

### 11. PackageAssetTags

| 标签 | 资源 |
| --- | --- |
| `scene.beast_path` | 副本场景和布局 |
| `bg.black_forest` | 黑林背景 |
| `enemy.claw_beast` | 低阶爪兽 Sprite |
| `enemy.thrower` | 投石魔物 Sprite |
| `enemy.pouncer` | 跳扑魔物 Sprite |
| `enemy.beast_alpha` | 黑林精英兽 Sprite |
| `ui.loot_icons.t0` | 初级材料和装备图标 |
| `audio.beast_path` | 副本 BGM 和敌人音效 |
| `fx.t0_combat` | 命中、掉落、魔核吸收特效 |

打包检查：

- 只收集本副本引用资源。
- 不打包后续章节资源。
- 副本掉落装备图标必须进入 `.wtpack`。

### 12. 实现检查

实现完成后检查：

- 据点能显示黑林兽道。
- 未击败黑熊丈夫前黑林兽道锁定。
- 进入副本后能按顺序清理 5 个房间。
- 投石可被一段跳稳定躲过。
- 精英兽可被短空连击败。
- 每次通关都能获得材料。
- A/S 评价能获得更好奖励。
- 装备掉落能进入背包。
- 返回据点后材料数量刷新。
- 技能树能显示材料来源为黑林兽道。
- 打包后副本资源完整。

