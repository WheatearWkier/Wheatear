# Batch04 SideCombat 敌人与 Boss 动作 Sheet 生产包

更新时间：2026-07-31

本包承接 `Batch03_SideCombat主角动作Sheet生产包.md`，用于把正式横板战斗里的低阶小怪、扩展小怪和黑熊丈夫 Boss 从当前占位帧升级为可长期使用的高清像素风透明动画源帧，并在验收后拼接成 sprite strip。目标是让空连、受击、击飞、Boss 破绽和死亡状态都能清楚播放。

## 1. 本批硬性规则

- AI 生成阶段必须先交付单帧源文件：`source_frames/<enemy_or_boss_action>/<clip>_000.png`、`001.png` 等；不要直接生成整张横向 strip。
- 每个源帧都是固定 canvas，尺寸等于该动作 profile；最终 `_strip.png` 只作为源帧验收通过后的拼接结果。
- 每张最终 sheet 只放一个角色的一个动作，横向一行排列，不混入其它动作或效果。
- 每个子格必须是完全相同的固定 cell，不能出现某一帧人物大、某一帧人物小的情况。
- 每帧角色、武器、爪子、尾巴、披毛、身体阴影都必须完整落在本 cell 内，禁止任何部位跨到相邻 cell。
- 如果冲刺、爪击、Boss 扫击会超出 cell，不要裁切，也不要让它进入隔壁格；必须扩大整张 sheet 的 cell，或把剑气/冲击波拆到 Batch06 VFX sheet。
- 同一个动作 clip 内必须统一 cell 长宽；不同动作 clip 可以使用不同 profile。不要让 idle/run 被死亡倒地的宽度污染，也不要让死亡/跳扑为了塞进常态格而缩小或裁掉。
- 凡是第一帧收缩、最后一帧横向展开，或前后姿态高度差很大的动作，都必须先按该动作最大包围盒定 profile，再逐帧生成。
- `cellWidth/cellHeight` 不是看某一帧站姿决定，而是看该 clip 所有帧里最宽、最高、最容易越界的姿态决定。定好后，该 clip 的每个源帧和最终 sheet 的每个子 cell 都必须使用这一个最大规格；不能同一动作里前几帧窄、后几帧宽。
- `run/walk/charge_loop` 等循环动作不能把最后几帧做成倒地或死亡。敌人正在移动时被击倒，由状态机从移动循环切到 `hit`、`launched`、`fall`、`dead/collapse` 等 transition clip；只有专门制作 `run_to_dead`、`charge_crash` 这类过渡动作时，才允许第一帧像移动、最后一帧倒地，并且该过渡 clip 必须按最大倒地/撞墙包围盒使用宽 profile。
- 同一动作内缩放锁死：角色静止站立帧的身体高度波动不超过 3%，Boss 动作峰值只允许姿态变化，不允许突然缩放。
- 所有源帧和最终 sheet 必须是 **RGBA PNG 真透明**，无用区域 alpha 为 0；禁止白底、灰底、黑底、绿底、棋盘格底、白色 matte、半透明残底、脏 alpha、网格线、帧号、文字、Logo 或水印。
- 战斗游玩部分统一为高清像素风：参考 DNF 类横板动作的像素密度、清晰黑边和动作可读性，但不能复刻 DNF 或任何商业作品的怪物、NPC、地图或 UI。

## 2. Cell 与安全框

`cellWidth x cellHeight` 是渲染画布和拼接网格，不是碰撞体尺寸。敌人移动碰撞、受击框、攻击框和 Boss 阶段判定由 `side_combat_tuning.yaml` 或后续 hitbox/hurtbox 数据独立定义；不能因为 Boss 死亡或爪击 cell 很宽，就把移动碰撞盒也做宽。

### 2.1 小怪通用 cell

低阶小怪常态动作使用 `512x384` cell。

```yaml
cellWidth: 512
cellHeight: 384
pivot: bottom_center
baselineY: 306
safePadding: [56, 42, 56, 42]
unusedPixelsAlpha: 0
```

说明：

- `safePadding` 表示左、上、右、下都要留透明缓冲。爪子和尾巴也必须在安全框内。
- 小怪常态身体缩放锁定，不允许某一帧因为跳扑或受击就整体变小；必要时扩大 cell，不缩放角色。
- 跳扑、被击飞、死亡动作可以脚离开 baseline，但角色中心和视觉缩放仍要稳定。
- 跳扑、上跃、被挑空这类纵向变化仍然要求同一 clip 固定 cell；用 `small_enemy_air` 或更高 profile 包住最高姿态。离地帧脚可以在 baseline 上方，但 `pivotX/pivotY/baselineY` 不变；真正的空中位移由状态机、速度或动画事件控制，图片 canvas 不承担碰撞高度。
- 小怪实际身体不要塞满 cell，常态高度建议占 cell 高度的 58%-72%。

小怪遇到大动作时用动作专用 profile：

| profile | cell | baseline | 用途 |
| --- | --- | ---: | --- |
| `small_enemy` | 512x384 | 306 | idle/run/hit/land 等常态地面动作 |
| `small_enemy_wide` | 768x384 | 306 | claw_attack、throw、leap、dead/collapse 等横向伸展动作 |
| `small_enemy_air` | 512x512 | 390 | launched/fall 等竖向伸展动作 |

如果某只敌人的尾巴、投掷道具或死亡倒地仍然贴边，只扩大该动作 profile，例如 `small_enemy_896` 或 `small_enemy_floor_896`，不缩小怪物、不裁切、不跨格。

### 2.2 Boss 通用 cell

黑熊丈夫 Boss 常态动作使用 `1024x768` cell。

```yaml
cellWidth: 1024
cellHeight: 768
pivot: bottom_center
baselineY: 626
safePadding: [112, 72, 112, 72]
unusedPixelsAlpha: 0
```

说明：

- Boss 常态身体高度建议占 cell 高度的 62%-76%，不要把爪子顶到边缘。
- `shockwave_cast` 只表现 Boss 蓄力、砸地和释放姿态；真正地面冲击波放到 Batch06 的 `vfx_boss_bear_shockwave_strip.png`。
- `charge_loop` 只表现 Boss 身体冲锋循环，不把长距离拖尾画满整格。
- Boss 跳砸、抬爪到高处、被挑空或破防弹起时，按该 clip 的最高头部/爪尖/背部姿态选择更高或更宽 profile；不能某一帧临时加高 cell，也不能把 Boss 缩小。
- 如果 Boss 爪击峰值仍然贴边，不允许裁切或缩小 Boss，应扩大该 Boss 动作 profile 并同步改 `sheet_params.yaml`。

Boss 横向大动作使用动作专用 profile：

| profile | cell | baseline | 用途 |
| --- | --- | ---: | --- |
| `boss_bear_husband` | 1024x768 | 626 | idle/walk/charge_loop/roar/hit/launched/fall/break_stun |
| `boss_bear_husband_wide` | 1280x768 | 626 | claw_attack、shockwave_cast 的砸地峰值、dead/collapse |

如果死亡倒下或爪击峰值 1280 仍不够，扩大该 clip 到 `boss_bear_husband_1536`。不要把 Boss 缩小到常态格，也不要把爪尖、头或身体压到画布边。

### 2.3 Pivot / Offset 对齐规则

敌人和 Boss 的 actor world origin 默认也是脚底/身体根部 pivot。不同敌人可以有自己的 profile，但同一个敌人的所有动作都必须能通过 pivot/baseline/renderOffset 对齐到同一个世界原点。

坐标约定：

- 图片原点在左上角，x 向右，y 向下。
- 固定 canvas 源帧中，`pivotX = cellWidth / 2`，`pivotY = baselineY`。
- 渲染偏移为 `renderOffset = [-pivotX, -pivotY]`。
- 同一个 clip 内所有源帧必须相同 `cellWidth/cellHeight/pivotX/pivotY/baselineY/renderOffset`。
- 不同 clip 可以换 `small_enemy_wide`、`small_enemy_air` 或 `boss_bear_husband_wide`，但必须继续把身体根部对齐到对应 pivot；不能因为画布变宽就让怪物身体横向漂移。
- 如果后续为了 atlas 压缩裁掉透明边，必须给每帧 `sourceRect` 和 `offsetFromPivot`；没有 offset metadata 的裁切帧禁止替换正式动画。

AI 逐帧提示词必须补充：

```text
invisible pivot at pixel ([PIVOT_X], [PIVOT_Y]), invisible baseline y=[BASELINE], align the creature root or boss feet to this pivot/baseline, do not draw any guide line
```

### 2.4 Visual Scale 视觉尺度锁定

敌人/Boss 的 cell 只是透明包装盒，怪物体型由 `scaleReference` 锁定。小怪换到 `small_enemy_wide` 或 `small_enemy_air` 时，身体、头/核心、爪子、尾巴和投掷道具的尺度不能突然变大或变小；Boss 换到 `boss_bear_husband_wide` 时也是同理。

规范描述：

- 人形或半人形小怪用 `standingBodyHeightPx`、`headOrCoreSizePx`、`chestWidthPx` 检查比例。
- 四足/兽形小怪用 `bodyLongAxisPx`、`headOrCoreSizePx`、`chestWidthPx` 检查比例，跳扑拉长时允许姿态变宽，但身体长轴不能变成另一只怪。
- Boss 直立或行走时看 `standingBodyHeightPx`，死亡倒下时看 `proneBodyLongAxisPx`，横扫时看头部、肩背和爪掌尺度。
- 示例：小怪 idle 身体长轴约 `250px`，跳扑时外接框可能更宽，但身体长轴仍应在约 `235-270px`；如果跳扑峰值变成 `340px` 的大怪，或死亡帧缩成 `180px`，不合格。
- 验收容差：小怪关键尺度不超过 `6%`，Boss 不超过 `5%`，攻击峰值或跳扑峰值最多 `8%`。

AI 逐帧提示词必须补充：

```text
keep anatomical scale locked to scaleReference: same head/core size, torso or body long-axis length, limb thickness and claw size as the reference idle frame; wide or airborne cells give transparent room, not a larger or smaller creature
```

### 2.5 动作连续性与力量感

敌人和 Boss 的动作也必须符合横板格斗的力道要求。小怪攻击要能看出扑击、抓挠、投掷的身体参与；Boss 动作要更重，必须有压低重心、蓄力、巨大惯性和清楚的破绽/恢复。

硬性规则：

- 每个攻击 clip 必须有 `startup/anticipation -> windup -> activeImpact/release -> followThrough -> recovery`。
- 小怪不能只让爪子或石头移动，身体、肩背、尾巴或重心必须配合发力。
- 投石动作在 release 前石头必须持续可见，release 后石头从手中消失必须写在 `intentionalChanges`，飞行 projectile 不烘焙在怪物动作里。
- Boss 横扫、冲撞、砸地必须表现重量：肩背压低、前爪/脚掌承重、身体扭转、命中后惯性继续。
- VFX 只能增强打击反馈，不能遮掉 Boss 或小怪的身体发力。

AI 逐帧提示词必须补充：

```text
continue the same powerful side-scrolling fighting animation from previous frame; preserve creature design, limbs, claws, props and facing direction; show anticipation, grounded weight or airborne momentum, clear active impact/release frame and follow-through inertia; not disconnected poses
```

## 3. 通用负面要求

所有提示词追加：

```text
no text, no letters, no numbers, no logo, no watermark, no signature, no UI, no background, no floor, no grid lines, no visible cell borders, no frame numbers, no white background, no gray background, no checkerboard background, no opaque background rectangle, no white matte, no dirty alpha edge, no cropped body, no cropped weapon, no cropped claw, no body part crossing into neighboring cells, no inconsistent cell size, no inconsistent character scale, no changing creature design between frames, no copied franchise monster, no blurry non-pixel-art rendering
```

## 4. 输入锚点

如果生成工具支持参考图或编辑模式，优先使用：

| 用途 | 锚点文件 |
| --- | --- |
| 当前爪兽占位 | `WheatearEditor/assets/vertical_slice/side_combat/enemies/en_claw_beast_small.png` |
| 当前黑熊 Boss 占位 | `WheatearEditor/assets/vertical_slice/side_combat/enemies/boss_bear_husband.png` |
| 战斗场景光色 | `WheatearEditor/assets/vertical_slice/side_combat/backgrounds/bg_black_forest_stage.png` |

参考只用于体型、功能和战斗可读性，不要照抄占位的低完成度细节。

## 5. 本批资产清单

### 5.1 低阶爪兽 en_claw_beast

| 动作 ID | 正式输出文件 | 帧数 | FPS | 输出尺寸 | 说明 |
| --- | --- | ---: | ---: | --- | --- |
| idle | `WheatearEditor/assets/vertical_slice/side_combat/sheets/en_claw_beast_idle_strip.png` | 8 | 12 | 4096x384 | 低伏呼吸，眼睛微光 |
| run | `WheatearEditor/assets/vertical_slice/side_combat/sheets/en_claw_beast_run_strip.png` | 8 | 18 | 4096x384 | 快速四足奔跑 |
| claw_attack | `WheatearEditor/assets/vertical_slice/side_combat/sheets/en_claw_beast_claw_attack_strip.png` | 8 | 20 | 6144x384 | 前扑爪击，使用 `small_enemy_wide`，爪子不出格 |
| hit | `WheatearEditor/assets/vertical_slice/side_combat/sheets/en_claw_beast_hit_strip.png` | 5 | 18 | 2560x384 | 受击后仰 |
| launched | `WheatearEditor/assets/vertical_slice/side_combat/sheets/en_claw_beast_launched_strip.png` | 4 | 12 | 2048x512 | 被挑空，使用 `small_enemy_air` |
| fall | `WheatearEditor/assets/vertical_slice/side_combat/sheets/en_claw_beast_fall_strip.png` | 4 | 12 | 2048x512 | 下坠，使用 `small_enemy_air` |
| dead | `WheatearEditor/assets/vertical_slice/side_combat/sheets/en_claw_beast_dead_strip.png` | 8 | 12 | 6144x384 | 非血腥死亡消散，使用 `small_enemy_wide` |

### 5.2 投石魔物 en_forest_thrower

| 动作 ID | 正式输出文件 | 帧数 | FPS | 输出尺寸 | 说明 |
| --- | --- | ---: | ---: | --- | --- |
| idle | `WheatearEditor/assets/vertical_slice/side_combat/sheets/en_forest_thrower_idle_strip.png` | 8 | 12 | 4096x384 | 佝偻警戒，抱石袋 |
| run | `WheatearEditor/assets/vertical_slice/side_combat/sheets/en_forest_thrower_run_strip.png` | 8 | 16 | 4096x384 | 短步移动 |
| throw | `WheatearEditor/assets/vertical_slice/side_combat/sheets/en_forest_thrower_throw_strip.png` | 9 | 18 | 6912x384 | 举石、后摆、投出，使用 `small_enemy_wide`，石头本体不作为飞行 projectile |
| hit | `WheatearEditor/assets/vertical_slice/side_combat/sheets/en_forest_thrower_hit_strip.png` | 5 | 18 | 2560x384 | 受击 |
| launched | `WheatearEditor/assets/vertical_slice/side_combat/sheets/en_forest_thrower_launched_strip.png` | 4 | 12 | 2048x512 | 被击飞，使用 `small_enemy_air` |
| dead | `WheatearEditor/assets/vertical_slice/side_combat/sheets/en_forest_thrower_dead_strip.png` | 8 | 12 | 6144x384 | 倒地或魔雾消散，使用 `small_enemy_wide` |

### 5.3 跳扑魔物 en_pouncer

| 动作 ID | 正式输出文件 | 帧数 | FPS | 输出尺寸 | 说明 |
| --- | --- | ---: | ---: | --- | --- |
| idle | `WheatearEditor/assets/vertical_slice/side_combat/sheets/en_pouncer_idle_strip.png` | 8 | 12 | 4096x384 | 压低身体，准备跳扑 |
| crouch | `WheatearEditor/assets/vertical_slice/side_combat/sheets/en_pouncer_crouch_strip.png` | 5 | 14 | 2560x384 | 跳扑前蓄力 |
| leap | `WheatearEditor/assets/vertical_slice/side_combat/sheets/en_pouncer_leap_strip.png` | 8 | 20 | 6144x384 | 空中扑击，使用 `small_enemy_wide`，身体完整在格内 |
| land | `WheatearEditor/assets/vertical_slice/side_combat/sheets/en_pouncer_land_strip.png` | 5 | 16 | 2560x384 | 落地回收 |
| hit | `WheatearEditor/assets/vertical_slice/side_combat/sheets/en_pouncer_hit_strip.png` | 5 | 18 | 2560x384 | 受击 |
| dead | `WheatearEditor/assets/vertical_slice/side_combat/sheets/en_pouncer_dead_strip.png` | 8 | 12 | 6144x384 | 非血腥失败，使用 `small_enemy_wide` |

### 5.4 黑熊丈夫 Boss boss_bear_husband

| 动作 ID | 正式输出文件 | 帧数 | FPS | 输出尺寸 | 说明 |
| --- | --- | ---: | ---: | --- | --- |
| idle | `WheatearEditor/assets/vertical_slice/side_combat/sheets/boss_bear_husband_idle_strip.png` | 10 | 10 | 10240x768 | 重呼吸，魔纹脉动 |
| walk | `WheatearEditor/assets/vertical_slice/side_combat/sheets/boss_bear_husband_walk_strip.png` | 10 | 14 | 10240x768 | 沉重移动 |
| claw_attack | `WheatearEditor/assets/vertical_slice/side_combat/sheets/boss_bear_husband_claw_attack_strip.png` | 10 | 18 | 12800x768 | 横扫爪击，使用 `boss_bear_husband_wide`，爪尖完整 |
| charge_windup | `WheatearEditor/assets/vertical_slice/side_combat/sheets/boss_bear_husband_charge_windup_strip.png` | 8 | 14 | 8192x768 | 压低身体蓄力 |
| charge_loop | `WheatearEditor/assets/vertical_slice/side_combat/sheets/boss_bear_husband_charge_loop_strip.png` | 6 | 18 | 6144x768 | 冲撞循环 |
| shockwave_cast | `WheatearEditor/assets/vertical_slice/side_combat/sheets/boss_bear_husband_shockwave_cast_strip.png` | 12 | 18 | 15360x768 | 砸地释放姿态，使用 `boss_bear_husband_wide`，冲击波另做 VFX |
| roar | `WheatearEditor/assets/vertical_slice/side_combat/sheets/boss_bear_husband_roar_strip.png` | 8 | 12 | 8192x768 | 咆哮和阶段切换 |
| hit | `WheatearEditor/assets/vertical_slice/side_combat/sheets/boss_bear_husband_hit_strip.png` | 5 | 16 | 5120x768 | 受击硬直 |
| launched | `WheatearEditor/assets/vertical_slice/side_combat/sheets/boss_bear_husband_launched_strip.png` | 4 | 10 | 4096x768 | 被打浮空，幅度克制 |
| fall | `WheatearEditor/assets/vertical_slice/side_combat/sheets/boss_bear_husband_fall_strip.png` | 5 | 10 | 5120x768 | 下落 |
| break_stun | `WheatearEditor/assets/vertical_slice/side_combat/sheets/boss_bear_husband_break_stun_strip.png` | 8 | 12 | 8192x768 | 破防眩晕，可被上挑追击 |
| dead | `WheatearEditor/assets/vertical_slice/side_combat/sheets/boss_bear_husband_dead_strip.png` | 10 | 10 | 12800x768 | 非血腥倒下，使用 `boss_bear_husband_wide`，魔光熄灭 |

## 6. 通用 Prompt 模板

### 6.1 小怪模板

将 `[CREATURE DESCRIPTION]`、`[ACTION REQUEST]`、`[FRAME INDEX]`、`[FRAME COUNT]`、`[CELL WIDTH]`、`[CELL HEIGHT]`、`[PIVOT_X]`、`[PIVOT_Y]`、`[BASELINE]`、`[POSE FOR THIS FRAME]` 替换为对应内容。每个动作逐帧生成，不要直接输出横向 strip。`[CELL WIDTH]x[CELL HEIGHT]` 必须来自 `sheet_params.yaml` 当前 clip 的 profile。

```text
Use case: stylized-concept
Asset type: single animation frame for a 2D side-scrolling action game enemy
Primary request: original high-resolution anime pixel-art fantasy side-view monster single frame, [ACTION REQUEST], frame [FRAME INDEX] of [FRAME COUNT], [POSE FOR THIS FRAME]
Subject: [CREATURE DESCRIPTION], readable enemy silhouette, designed for bright stylish side-scrolling combat in a dark magical forest
Style/medium: polished high-resolution pixel-art anime action game sprite, DNF-like side-view readability and pixel density, crisp pixel clusters, clean outline, not 3D render
Composition/framing: fixed [CELL WIDTH]x[CELL HEIGHT] RGBA transparent canvas, unused pixels alpha 0, one frame only, creature faces left toward the player by default, invisible pivot at pixel ([PIVOT_X], [PIVOT_Y]), root/feet aligned to invisible baseline y around [BASELINE] where applicable, pivot bottom center, do not draw pivot or baseline guide lines
Frame integrity: the whole creature, claws, tail, stones, ears, fur tips and shadow accents stay inside this canvas with profile safe padding; same anatomical scale as scaleReference; no cropped limbs
Animation requirements: this is one numbered frame from a planned animation; clear anticipation, active pose, impact/release and recovery where relevant; consistent creature design, limbs, props, pivot and visual scale across all frames; powerful side-scrolling fighting motion with body weight and follow-through; do not draw neighboring frames; do not create a horizontal sprite sheet
Lighting/mood: energetic fantasy combat, readable over dark forest background
Color palette: dark forest creature tones, cyan magical eye or marking accents, warm rim highlights
Constraints: RGBA transparent PNG, unused pixels alpha 0, no text, no UI, no background, no floor, no logo, no watermark, no copied franchise monster, no white matte
Avoid: cropped limbs, inconsistent frame size, creature scale changing between frames, effects covering the action, gore, dirty alpha edge, blurry painted rendering, horizontal contact sheet, multiple poses in one image
```

小怪描述替换：

```text
en_claw_beast: small agile wolf-like claw beast, dark mossy fur, cyan magic eyes, sharp foreclaws, early-game low beast enemy
en_forest_thrower: small hunched forest thrower monster, bark-like back, stone pouch, nervous but hostile, designed to throw rocks from mid range
en_pouncer: lean pouncing forest beast, long forelimbs, dark green-black fur, cyan markings, designed for fast leap attacks
```

### 6.2 Boss 模板

将 `[ACTION REQUEST]`、`[FRAME INDEX]`、`[FRAME COUNT]`、`[CELL WIDTH]`、`[CELL HEIGHT]`、`[PIVOT_X]`、`[PIVOT_Y]`、`[BASELINE]`、`[POSE FOR THIS FRAME]` 替换为对应内容。每个动作逐帧生成，不要直接输出横向 strip。`claw_attack`、`shockwave_cast`、`dead` 默认使用 `boss_bear_husband_wide`，除非 frame plan 明确证明常态 profile 放得下。

```text
Use case: stylized-concept
Asset type: single animation frame for a 2D side-scrolling action game boss
Primary request: original high-resolution anime pixel-art fantasy side-view boss single frame of the corrupted black bear husband, [ACTION REQUEST], frame [FRAME INDEX] of [FRAME COUNT], [POSE FOR THIS FRAME]
Subject: massive black bear husband corrupted by cyan magical veins, huge claws, wounded rage, powerful but readable silhouette, tutorial boss for magic-sword combat
Style/medium: polished high-resolution pixel-art anime action game boss sprite, bright DNF-like side-scrolling combat proportions and pixel density, crisp pixel clusters, clean outline, not 3D render
Composition/framing: fixed [CELL WIDTH]x[CELL HEIGHT] RGBA transparent canvas, unused pixels alpha 0, one frame only, invisible pivot at pixel ([PIVOT_X], [PIVOT_Y]), boss feet/root aligned to consistent invisible ground baseline at y around [BASELINE], boss faces left toward the player by default, pivot bottom center, do not draw pivot or baseline guide lines
Frame integrity: the full body, head, back, claws, paws, fur tips and magic veins must stay inside this canvas with profile safe padding; same boss anatomical scale as scaleReference; no cropped claw, paw, snout or body
Animation requirements: this is one numbered frame from a planned animation; clear anticipation, active impact and recovery where relevant; heavy grounded weight, shoulder/back rotation, readable attack timing and follow-through inertia; shockwaves, long trails and large impact rings should be separate VFX; do not draw neighboring frames; do not create a horizontal sprite sheet
Lighting/mood: dramatic but readable tutorial boss combat, cyan magical corruption balanced by warm highlights
Color palette: black bear fur, dark violet shadows, cyan magic veins, muted warm highlights
Constraints: RGBA transparent PNG, unused pixels alpha 0, no text, no UI, no background, no floor, no logo, no watermark, no copied franchise creature, no white matte
Avoid: cropped claws, cropped body, inconsistent scale, gore, excessive smoke hiding the pose, shockwave taking over the sprite, dirty alpha edge, blurry painted rendering, horizontal contact sheet, multiple poses in one image
```

## 7. 动作替换文本

```text
en_claw_beast_idle_strip.png: low crouched breathing idle loop, ears and fur tips subtly moving, cyan eyes pulsing
en_claw_beast_run_strip.png: fast four-legged run cycle, clear foot contacts, body low and agile
en_claw_beast_claw_attack_strip.png: forward claw swipe attack, small anticipation, active claw pose, recovery, claws fully inside each cell
en_claw_beast_hit_strip.png: quick hit reaction, body recoils but keeps readable silhouette
en_claw_beast_launched_strip.png: launched upward by sword uppercut, limbs trailing, no cell clipping
en_claw_beast_fall_strip.png: falling from air, body angled downward, ready to hit ground
en_claw_beast_dead_strip.png: non-graphic defeat, collapses then fades with tiny cyan particles contained in cell

en_forest_thrower_idle_strip.png: hunched idle with stone pouch, nervous scanning motion
en_forest_thrower_run_strip.png: short awkward run, clutching stones
en_forest_thrower_throw_strip.png: picks a stone, winds up, throws forward, projectile itself not included after release
en_forest_thrower_hit_strip.png: recoils and drops posture, no dropped items outside cell
en_forest_thrower_launched_strip.png: launched upward, pouch and limbs trailing but contained
en_forest_thrower_dead_strip.png: collapses or dissolves into forest magic dust, non-graphic

en_pouncer_idle_strip.png: low predatory idle, coiled spring posture
en_pouncer_crouch_strip.png: jump windup, legs compress, spine curls
en_pouncer_leap_strip.png: forward leap attack, body stretched but fully inside each small_enemy_wide cell
en_pouncer_land_strip.png: landing recovery from leap, claws touch ground then body recoils
en_pouncer_hit_strip.png: hit reaction while maintaining scale
en_pouncer_dead_strip.png: non-graphic defeat state

boss_bear_husband_idle_strip.png: heavy breathing idle, cyan veins pulsing, huge weight
boss_bear_husband_walk_strip.png: slow heavy walk cycle, paws impact with weight
boss_bear_husband_claw_attack_strip.png: massive horizontal claw swipe, anticipation, active swing, recovery, claws fully inside cell
boss_bear_husband_charge_windup_strip.png: lowers head and shoulders, paws dig in, preparing charge
boss_bear_husband_charge_loop_strip.png: charging run loop, strong forward momentum, no long trail baked beyond body
boss_bear_husband_shockwave_cast_strip.png: rears up then slams the ground, body animation only, shockwave graphic not included
boss_bear_husband_roar_strip.png: roar and phase anger, mouth open, magic veins flare, no text or symbols
boss_bear_husband_hit_strip.png: heavy hit stun, shoulders recoil
boss_bear_husband_launched_strip.png: brief boss-scale launch reaction, restrained vertical lift, readable for air-combo tutorial
boss_bear_husband_fall_strip.png: heavy fall from launch, body tips downward
boss_bear_husband_break_stun_strip.png: broken stance, dizzy magic vein flicker, clear player punish window
boss_bear_husband_dead_strip.png: non-graphic collapse and magic corruption fading
```

## 8. sheet_params.yaml 示例

```yaml
defaults:
  productionMode: source_frames_first
  coordinateSystem: image_top_left_x_right_y_down
  actorWorldOrigin: pivot
  rowOrigin: top
  transparent: true
  requireRgba: true
  unusedPixelsAlpha: 0
  noCrossCellBleed: true
  noVisibleGrid: true
  bodyScaleLock: true
  sourceFramePattern: source_frames/{clip}/{clip}_{frame:03}.png
  assembleStripAfterValidation: true
  framePlanRequired: true
  requirePivotMetadata: true
  frameOffsetMode: fixed_canvas_no_per_frame_offset
  continuityPlanRequired: true
  actionPowerPlanRequired: true
  collisionIndependentFromSpriteCanvas: true

scaleReference:
  small_enemy_default:
    referenceClip: idle
    bodyLongAxisPx: [235, 270]
    headOrCoreSizePx: [42, 62]
    chestWidthPx: [80, 120]
    tolerancePercent: 6
    actionPeakTolerancePercent: 8
    rule: larger_cell_gives_transparent_room_not_larger_creature
  forest_thrower:
    referenceClip: en_forest_thrower_idle
    standingBodyHeightPx: [230, 270]
    headOrCoreSizePx: [44, 62]
    chestWidthPx: [78, 115]
    carriedPropScaleLocked: true
    tolerancePercent: 6
  boss_bear_husband:
    referenceClip: boss_bear_husband_idle
    referenceProfile: boss_bear_husband
    standingBodyHeightPx: [500, 585]
    headHeightPx: [105, 140]
    shoulderBackWidthPx: [300, 390]
    proneBodyLongAxisPx: [520, 630]
    clawPawSizePx: [85, 130]
    tolerancePercent: 5
    actionPeakTolerancePercent: 8
    rule: boss_wide_cell_gives_claw_and_collapse_room_not_larger_boss

small_enemy:
  cellWidth: 512
  cellHeight: 384
  pivot: bottom_center
  pivotX: 256
  pivotY: 306
  baselineY: 306
  renderOffset: [-256, -306]
  safePadding: [56, 42, 56, 42]

small_enemy_wide:
  cellWidth: 768
  cellHeight: 384
  pivot: bottom_center
  pivotX: 384
  pivotY: 306
  baselineY: 306
  renderOffset: [-384, -306]
  safePadding: [96, 42, 112, 42]

small_enemy_air:
  cellWidth: 512
  cellHeight: 512
  pivot: bottom_center
  pivotX: 256
  pivotY: 390
  baselineY: 390
  renderOffset: [-256, -390]
  safePadding: [56, 58, 56, 64]

boss_bear_husband:
  cellWidth: 1024
  cellHeight: 768
  pivot: bottom_center
  pivotX: 512
  pivotY: 626
  baselineY: 626
  renderOffset: [-512, -626]
  safePadding: [112, 72, 112, 72]

boss_bear_husband_wide:
  cellWidth: 1280
  cellHeight: 768
  pivot: bottom_center
  pivotX: 640
  pivotY: 626
  baselineY: 626
  renderOffset: [-640, -626]
  safePadding: [160, 72, 176, 72]

clips:
  en_claw_beast_idle_strip: { frameCount: 8, frameRate: 12, profile: small_enemy }
  en_claw_beast_run_strip: { frameCount: 8, frameRate: 18, profile: small_enemy }
  en_claw_beast_claw_attack_strip: { frameCount: 8, frameRate: 20, profile: small_enemy_wide }
  en_claw_beast_hit_strip: { frameCount: 5, frameRate: 18, profile: small_enemy }
  en_claw_beast_launched_strip: { frameCount: 4, frameRate: 12, profile: small_enemy_air }
  en_claw_beast_fall_strip: { frameCount: 4, frameRate: 12, profile: small_enemy_air }
  en_claw_beast_dead_strip: { frameCount: 8, frameRate: 12, profile: small_enemy_wide }
  en_forest_thrower_idle_strip: { frameCount: 8, frameRate: 12, profile: small_enemy }
  en_forest_thrower_run_strip: { frameCount: 8, frameRate: 16, profile: small_enemy }
  en_forest_thrower_throw_strip: { frameCount: 9, frameRate: 18, profile: small_enemy_wide }
  en_forest_thrower_hit_strip: { frameCount: 5, frameRate: 18, profile: small_enemy }
  en_forest_thrower_launched_strip: { frameCount: 4, frameRate: 12, profile: small_enemy_air }
  en_forest_thrower_dead_strip: { frameCount: 8, frameRate: 12, profile: small_enemy_wide }
  en_pouncer_idle_strip: { frameCount: 8, frameRate: 12, profile: small_enemy }
  en_pouncer_crouch_strip: { frameCount: 5, frameRate: 14, profile: small_enemy }
  en_pouncer_leap_strip: { frameCount: 8, frameRate: 20, profile: small_enemy_wide }
  en_pouncer_land_strip: { frameCount: 5, frameRate: 16, profile: small_enemy }
  en_pouncer_hit_strip: { frameCount: 5, frameRate: 18, profile: small_enemy }
  en_pouncer_dead_strip: { frameCount: 8, frameRate: 12, profile: small_enemy_wide }
  boss_bear_husband_idle_strip: { frameCount: 10, frameRate: 10, profile: boss_bear_husband }
  boss_bear_husband_walk_strip: { frameCount: 10, frameRate: 14, profile: boss_bear_husband }
  boss_bear_husband_claw_attack_strip: { frameCount: 10, frameRate: 18, profile: boss_bear_husband_wide }
  boss_bear_husband_charge_windup_strip: { frameCount: 8, frameRate: 14, profile: boss_bear_husband }
  boss_bear_husband_charge_loop_strip: { frameCount: 6, frameRate: 18, profile: boss_bear_husband }
  boss_bear_husband_shockwave_cast_strip: { frameCount: 12, frameRate: 18, profile: boss_bear_husband_wide }
  boss_bear_husband_roar_strip: { frameCount: 8, frameRate: 12, profile: boss_bear_husband }
  boss_bear_husband_hit_strip: { frameCount: 5, frameRate: 16, profile: boss_bear_husband }
  boss_bear_husband_launched_strip: { frameCount: 4, frameRate: 10, profile: boss_bear_husband }
  boss_bear_husband_fall_strip: { frameCount: 5, frameRate: 10, profile: boss_bear_husband }
  boss_bear_husband_break_stun_strip: { frameCount: 8, frameRate: 12, profile: boss_bear_husband }
  boss_bear_husband_dead_strip: { frameCount: 10, frameRate: 10, profile: boss_bear_husband_wide }
```

`frame_plan.yaml` 最小示例：

```yaml
boss_bear_husband_claw_attack:
  profile: boss_bear_husband_wide
  frameCount: 10
  baselineY: 626
  pivot: bottom_center
  pivotX: 640
  pivotY: 626
  renderOffset: [-640, -626]
  frameOffsetMode: fixed_canvas_no_per_frame_offset
  scaleReference: boss_bear_husband
  continuityState:
    facing: left
    clawsVisible: true
    magicVeinsVisible: true
    forbiddenChanges: [claws_disappear, snout_shape_changes, body_scale_jumps, pose_jumps_to_unrelated_key]
  actionPower:
    powerLevel: heavy_boss_sweep
    keyFrames:
      startup: [0, 1]
      windup: [2]
      activeImpact: [3, 4]
      followThrough: [5, 6]
      recovery: [7, 8, 9]
    forceCues: [front_paw_planted, shoulder_back_rotation, head_counterweight, claw_arc_reads_wide, fur_and_magic_veins_follow_through]
  frames:
    0: "heavy idle-to-attack anticipation, shoulders rotate back, claws still inside safe padding"
    1: "windup deeper, head lowers, front paw pulls back"
    2: "attack starts, torso twists forward, body scale unchanged"
    3: "active swipe begins, claw arcs across front but stays inside 1280x768 canvas"
    4: "peak claw swipe, widest silhouette, no cropped claw tips"
    5: "follow-through, weight shifts forward, paws stay readable"
    6: "recovery, shoulders drop, magic veins dim slightly"
    7: "body returns toward baseline, no scale shrink"
    8: "guarded recovery, claws lower"
    9: "returns to loop-ready pose, same pivot and boss height"
```

生成时先写每个动作的 `frame_plan.yaml`，再逐帧生成。不要用同一句 prompt 让 AI 自己猜完整动作序列。

## 9. 推荐交付结构

```text
Batch04_SideCombat_Enemies_Boss/
  source_frames/
    en_claw_beast_idle/en_claw_beast_idle_000.png ... en_claw_beast_idle_007.png
    en_claw_beast_run/en_claw_beast_run_000.png ... en_claw_beast_run_007.png
    en_forest_thrower_throw/en_forest_thrower_throw_000.png ... en_forest_thrower_throw_008.png
    boss_bear_husband_claw_attack/boss_bear_husband_claw_attack_000.png ... boss_bear_husband_claw_attack_009.png
    ...
  sheets/
    en_claw_beast_idle_strip.png
    en_claw_beast_run_strip.png
    en_claw_beast_claw_attack_strip.png
    en_claw_beast_hit_strip.png
    en_claw_beast_launched_strip.png
    en_claw_beast_fall_strip.png
    en_claw_beast_dead_strip.png
    en_forest_thrower_idle_strip.png
    en_forest_thrower_run_strip.png
    en_forest_thrower_throw_strip.png
    en_forest_thrower_hit_strip.png
    en_forest_thrower_launched_strip.png
    en_forest_thrower_dead_strip.png
    en_pouncer_idle_strip.png
    en_pouncer_crouch_strip.png
    en_pouncer_leap_strip.png
    en_pouncer_land_strip.png
    en_pouncer_hit_strip.png
    en_pouncer_dead_strip.png
    boss_bear_husband_idle_strip.png
    boss_bear_husband_walk_strip.png
    boss_bear_husband_claw_attack_strip.png
    boss_bear_husband_charge_windup_strip.png
    boss_bear_husband_charge_loop_strip.png
    boss_bear_husband_shockwave_cast_strip.png
    boss_bear_husband_roar_strip.png
    boss_bear_husband_hit_strip.png
    boss_bear_husband_launched_strip.png
    boss_bear_husband_fall_strip.png
    boss_bear_husband_break_stun_strip.png
    boss_bear_husband_dead_strip.png
  sheet_params.yaml
```

## 10. 接入动作

1. 先把源帧放入 `WheatearEditor/assets/vertical_slice/source_frames/<clip>/` 并逐帧验收。
2. 验收通过后，用脚本或工具按 `sheet_params.yaml` 拼接到 `WheatearEditor/assets/vertical_slice/side_combat/sheets/*_strip.png`。
3. 用 Sprite Sheet Picker 或导入脚本按 `sheet_params.yaml` 切成 AnimationClip。
4. 将 `side_combat_tuning.yaml` 里敌人与 Boss 的旧逐帧 `pattern` 迁移到正式 strip/clip。
5. Boss `shockwave_cast` 只触发施法姿态；真正冲击波引用 Batch06 的 `vfx_boss_bear_shockwave_strip.png`。
6. 重新打包 Sandbox，检查 `content.wtpack` 包含 `side_combat/sheets/en_*_strip.png` 和 `boss_bear_husband_*_strip.png`。

## 11. 验收标准

- 每张源帧 PNG 尺寸必须等于该 profile 的 `cellWidth x cellHeight`；每张最终 strip PNG 尺寸必须等于 `cellWidth * frameCount` by `cellHeight`。
- 透明背景必须是真 RGBA：无用区域 alpha 为 0；放到黑、白、亮粉、透明棋盘背景上都不能有白底、灰底、黑底、绿底、脏边、白色 matte 或可见 cell 分隔线。
- 每张源帧单独显示都必须是完整角色，不缺爪、不缺腿、不缺头、不缺尾巴；把 strip 按 cell 网格切开后必须与源帧一致。
- 同一动作内敌人或 Boss 的视觉尺寸稳定，不能有忽大忽小。
- 跨动作的敌人/Boss 视觉尺度必须符合 `scaleReference`：直立看站立身高或身体长轴，倒地/跳扑看身体长轴，Boss 横扫看头、肩背、爪掌尺度；不能因为 cell 变宽就放大怪物，也不能因为倒地变矮就缩小怪物。
- Boss 大动作不能把爪子、头或身体切到画布边缘。
- 任意敌人动作如果第一帧紧凑、最后一帧展开或倒地，必须使用动作专用 profile；不允许把展开帧缩小到常态格。
- 图片尺寸不参与碰撞判定；移动碰撞盒、hurtbox、hitbox 和 Boss 攻击判定必须独立配置。
- `shockwave_cast` 不包含大面积冲击波；大面积 VFX 必须走 Batch06。
- 缩到游戏内显示尺寸后，idle/run/attack/hit/launched/dead 的动作语义仍能一眼分清；像素块边缘清楚，不像模糊插画缩小图。
- 不出现血腥、商业可识别怪物、UI、文字、Logo、水印或背景。
