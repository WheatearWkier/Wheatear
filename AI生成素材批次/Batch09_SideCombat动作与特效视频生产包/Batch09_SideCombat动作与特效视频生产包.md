# Batch09 SideCombat 动作与特效视频生产包

更新时间：2026-08-03

本批用于替代新动作和新 VFX 的“逐帧单图生成”流程。生成阶段以角色或特效参考图为锚点，先生成一段固定机位的视频，再抽取视频帧，处理为 RGBA 源帧，最后拼接成运行时 sprite strip 或 VFX sheet。

本批只改变 AI 生成阶段的工作方式，不改变运行时的固定 cell、pivot/baseline、视觉尺度、碰撞独立配置、VFX 拆分和最终 PNG 验收规则。Batch03、Batch04、Batch06 已有单帧资产可以继续使用；从本批开始制作的新动作和新特效，优先采用本批的视频流程。

## 1. 核心原则

### 1.1 视频优先，帧序列后处理

不要再把每一帧当成一张独立插画分别生成。每个动作或特效先生成一个完整视频 clip：

1. 准备一张或多张参考图，锁定角色身份、服装、武器、体型、视角和风格。
2. 生成一段只有一个动作意图的视频，不混入第二个动作、镜头切换或场景转场。
3. 观看视频，确认动作弧、角色稳定性和画布完整性。
4. 按目标帧数和动作节奏抽取关键帧，去除首尾多余停顿，统一到固定 cell canvas。
5. 处理背景和 alpha，输出 `source_frames/<clip>/` 下的 RGBA PNG。
6. 逐帧验收后，再拼接为 `_strip.png` 或 VFX sheet。

视频不是运行时资源。视频、抽帧前图片和中间处理文件都只放在批次归档目录；运行时仍只引用正式 PNG 和参数文件。

### 1.2 只限制真正影响接入的内容

本批不再要求 AI 在生成阶段直接输出透明 PNG，也不要求 AI 逐帧理解 pivot 像素坐标。AI 阶段只需要稳定地生成一段完整、连续、固定机位的视频。

以下规则仍然是硬性规则：

- 摄像机视角、距离和构图在整个视频中保持不变。
- 角色、武器和特效完整留在视频画布内，任何一帧都不能裁切。
- 角色比例、脸、服装、装备、武器和朝向保持连续。
- 远程 projectile、魔法弹、冲击波、大范围剑气和大魔法阵单独生成。
- 同一批次使用统一视频规格分档、统一时间节奏分档、统一导出命名和统一验收方式。
- 最终运行时帧仍必须是固定 cell、真透明 RGBA、无跨格串位。

## 2. 视频母版规格

本批不再一刀切使用 `1920x1080`。人物、普通小怪和紧凑特效用更小的方形或近方形母版；只有大 Boss、大范围冲击波、全屏叠加和场景类视频才使用 16:9 大画布。

原则是：使用“能完整容纳动作最大姿态的最小足够视频母版”。母版越大，生成工具越容易把主体缩小、漂移或补出多余背景；人物/怪物动作优先用中小画布，把质量集中在主体上。

```yaml
videoMasterDefaults:
  audio: false
  defaultGeneratedClipSeconds: 4.0
  camera: locked_side_view
  background: chroma_key_green
  safeMarginPercent: 8
  maxExtractedFramesPerClip: 10
  sourceFrameRateRequirement: not_important_extract_keyframes
  previewPlaybackFps: 10-12
  effectiveActionWindow: trim_after_generation
  nonLoopActionRule: action_once_then_hold_final_pose
  loopActionRule: stable_loop_cycles_across_clip
  keyBackground:
    color: "#00FF00"
    name: pure_chroma_green
    flat: true
    noGradient: true
    noShadow: true
    noFloorLine: true
    noContactShadow: true

videoCanvasProfiles:
  character_square:
    canvas: [1024, 1024]
    useFor: protagonist, humanoid characters, small enemies, idle/run/hit/basic attacks
  character_wide:
    canvas: [1280, 720]
    useFor: long weapon arcs, knockdown/dead, leap, dash, wide monster attacks
  boss_large:
    canvas: [1280, 720]
    useFor: normal boss actions and wide boss attacks
  compact_vfx:
    canvas: [768, 768]
    useFor: cast sparks, hit sparks, guard flash, pickup glow, small magic impact
  projectile_wide:
    canvas: [768, 432]
    useFor: magic bolts, stones, short horizontal projectiles
  large_vfx_wide:
    canvas: [1280, 720]
    useFor: slash waves, dash trails, shockwaves, large dust
  large_vfx_square:
    canvas: [1024, 1024]
    useFor: magic circles, radial bursts, vertical launcher effects
  full_screen_overlay_or_scene:
    canvas: [1920, 1080]
    useFor: screen flash, break-limit overlay, backgrounds, UI-scene composite previews
```

如果生成工具只能提供某些固定比例，按最接近的分档选择：人物/小怪优先 `1:1`，不支持时使用 `1280x720` 并把主体居中；横向 projectile 和冲击波使用 16:9；魔法阵和径向爆发使用 1:1。后处理时先统一到该 clip 记录的 `masterCanvas`，再抽帧，不需要全部缩放到 `1920x1080`。

### 2.1 绿幕背景规则

所有角色、怪物、Boss 和 VFX 视频默认使用纯绿幕，方便后处理抠帧：

```text
solid pure chroma key green background, #00FF00, flat lighting,
no background texture, no gradient, no floor line, no contact shadow,
no cast shadow, no ambient scene, no checkerboard, no white/gray/black background
```

规则：

- 绿幕必须是整张画布唯一背景，不能出现森林、地面、天空、UI、网格、摄影棚墙角或透视地板。
- 角色脚下不画投影、不画地面接触阴影、不画绿色以外的底色块；落地烟尘、脚步火花等需要单独 VFX clip。
- 角色边缘不能有明显绿色溢色；如果生成工具总是给边缘染绿，换更远的绿色背景/更硬边缘提示或重生。
- VFX 如果本身就是绿色系，改用纯品红 `#FF00FF` 或纯蓝 `#0000FF` 作为 chroma key，并在 `batch09_video_params.yaml` 记录 `keyColor`。
- 最终 PNG 仍必须做 alpha 验收：只用绿幕方便抠图，不代表可以接受绿色残边。

### 2.2 时间与帧数分档

本批参考的是 DNF 类横板动作的“少帧强姿态 + 命中停顿 + 清楚收招”节奏，不追求 24fps 视频顺滑感。生成视频只是连续动作参考源；最终运行时通常只抽 `3-10` 帧，并用少量 hold 或事件停顿表现打击感。

默认源视频可以生成 `4` 秒，方便从中挑选最稳定的姿态和最干净的边缘。真正进入运行时的只是其中一个有效动作段；非循环动作只演一次，动作完成后保持最终姿态，不要在 4 秒里重复多次、换动作或重新设计角色。循环动作可以在 4 秒里稳定循环多次，后处理挑出一个完整循环。

| 类型 | 有效动作段 | 抽取帧数 | 运行时播放建议 | 说明 |
| --- | ---: | ---: | ---: | --- |
| idle loop | 1.0-1.2s | 6-8 | 6-8 fps | 呼吸、衣摆和武器微动，首尾可循环 |
| run / walk loop | 0.6-0.8s | 6-8 | 8-12 fps | 脚步接触清楚，循环优先 |
| light attack / basic1-2 | 0.35-0.55s | 5-7 | 10-14 fps | 起手短，命中帧可 hold 1 帧 |
| heavy attack / basic3 | 0.55-0.8s | 7-9 | 10-14 fps | 蓄力和收招更重，峰值姿态清楚 |
| launcher / air chase | 0.6-0.9s | 7-10 | 10-14 fps | 纵向变化清楚，避免过度顺滑 |
| hurt / hit react | 0.25-0.4s | 3-5 | 10-12 fps | 短促、明确，不拖泥带水 |
| knockdown / dead | 0.8-1.2s | 8-10 | 8-10 fps | 倒地过程和最终姿态可读 |
| Boss heavy attack | 0.8-1.2s | 8-10 | 8-12 fps | 更重，起手和破绽要明显 |
| compact VFX | 0.25-0.45s | 4-6 | 12-16 fps | 命中火花、施法火花、护盾闪光 |
| projectile loop | 0.4-0.6s | 4-6 | 8-12 fps | 魔法弹、投石等循环飞行物 |
| large VFX | 0.5-0.9s | 6-8 | 10-14 fps | 剑气、冲击波、烟尘、魔法阵 |

如果一个动作 10 帧以内仍然读不清，优先拆 clip 或拆 VFX，不要把单个 clip 扩到 16-24 帧。比如主角 `protag_magic_bolt` 保留 5-7 帧施法，飞行魔法弹另做 4-6 帧 projectile loop。

### 2.3 摄像机锁定规则

视频提示词必须明确：

```text
locked camera, fixed side-view angle, fixed focal length, fixed camera distance,
no pan, no tilt, no roll, no zoom, no dolly, no perspective change,
no reframing, no crop, no cut, no transition, no camera shake
```

允许角色自身前倾、跳起、倒地和冲刺；不允许摄像机跟着角色移动。角色的视觉位移由动作本身、运行时 root motion 或动画事件处理。

### 2.4 画布完整性

- 主体最宽、最高和最靠近边缘的姿态都必须留在画布内。
- 身体、头发、武器、披风、尾巴、爪子和贴身光效都不能碰到画布边缘。
- 默认四周至少保留 8% 的透明或干净背景安全区。
- 不为了塞进画布而缩小角色；如果动作确实超出母版，应先拆出远程 VFX，仍放不下时才扩大该 clip 的运行时 profile。
- 视频中不得出现自动裁切、智能重构图、主体突然缩小或主体从画面边缘进入。

## 3. 参考图使用规则

### 3.1 参考图优先级

同一 clip 最多使用三类参考图，按下面优先级处理：

1. **身份锚点**：锁定角色脸、发型、服装、颜色、装备和身体比例。
2. **动作锚点**：说明动作方向、起手、命中峰值和收招姿态。
3. **风格/VFX 锚点**：锁定像素密度、光效颜色、粒子形状和完成度。

如果参考图之间出现冲突，身份锚点优先于动作锚点，动作锚点优先于风格锚点。动作可以变，角色不能变成另一个人。

### 3.2 角色参考图

角色动作视频至少使用一张稳定的角色参考图。优先使用：

```text
WheatearEditor/assets/vertical_slice/vn/portraits/protag_school_neutral.png
WheatearEditor/assets/vertical_slice/vn/portraits/protag_school_alert.png
WheatearEditor/assets/vertical_slice/side_combat/characters/protag_magic_swordsman.png
```

敌人和 Boss 使用各自的正式立绘、静态战斗图或已经验收的动作关键帧。已有单帧素材可以作为视频参考图，不要求把它们重新加工成整段动画。

### 3.3 参考图锁定内容

视频生成时明确要求保持：

- 同一角色身份和年龄感。
- 同一脸部结构、头发轮廓、服装和主要装备。
- 同一武器外形、握持手和主要颜色。
- 同一横板侧视角和朝向。
- 同一视觉尺度和画布内占比。

允许发生：

- 身体姿势、重心、手脚位置、衣摆和头发的自然运动。
- 动作需要的轻微 squash/stretch、前倾和惯性。
- 预先写在动作计划中的武器角度变化、投掷物释放和贴身光效变化。

禁止发生：

- 换脸、换发型、换服装、换武器或换朝向。
- 视频中途出现第二个角色、第二把武器或新的装备。
- 角色因为跳跃、攻击或倒地而突然变大或变小。

## 4. 动作视频生产规则

### 4.1 一个视频只表达一个 clip

一个视频只制作一个清晰的动作语义：

- `idle_loop`、`run_loop`、`jump_start`、`fall`、`land`
- `basic1`、`basic2`、`basic3`、`launcher`、`air_chase`
- `protag_magic_bolt`、`hurt`、`knockdown`、`recover`、`dead`
- 敌人的 `idle`、`run`、`attack`、`hit`、`launched`、`dead`
- Boss 的 `charge_windup`、`charge_loop`、`claw_attack`、`shockwave_cast`、`break_stun`、`dead`

不要在一个视频中要求“先跑步、再跳起、再攻击、再倒地”。如果运行时需要多个状态，分别生成多个视频 clip。

### 4.2 当前主角动作清单

本批主角动作清单继承 Batch03。视频文件名、抽帧目录名和最终 strip 名必须使用同一个 clip ID。

> 下面的历史候选清单保留用于动作设计参考。当前 Sandbox 的实际替换任务以 4.3.1 的运行时清单为准；运行时键名、帧数、FPS 和 loop 状态不要再从旧表推断。

| Clip ID | 视频源建议 | 继承 profile | 抽帧建议 | 说明 |
| --- | --- | --- | ---: | --- |
| `protag_idle` | `videos/characters/protag_idle.mp4` | `body_512` | 6-8 | 呼吸待机，魔剑微光 |
| `protag_run` | `videos/characters/protag_run.mp4` | `body_512` | 6-8 | 横向奔跑循环 |
| `protag_jump_start` | `videos/characters/protag_jump_start.mp4` | `body_512` | 3-4 | 起跳发力 |
| `protag_jump_loop` | `videos/characters/protag_jump_loop.mp4` | `body_512` | 4-6 | 空中滞留循环 |
| `protag_fall` | `videos/characters/protag_fall.mp4` | `body_512` | 4-6 | 下落姿态 |
| `protag_land` | `videos/characters/protag_land.mp4` | `body_512` | 3-4 | 落地缓冲 |
| `protag_basic1` | `videos/characters/protag_basic1.mp4` | `slash_640` | 5-7 | 第一段平砍 |
| `protag_basic2` | `videos/characters/protag_basic2.mp4` | `slash_640` | 5-7 | 第二段反手斩 |
| `protag_basic3` | `videos/characters/protag_basic3.mp4` | `slash_heavy_768` | 7-9 | 第三段收尾斩 |
| `protag_air_basic` | `videos/characters/protag_air_basic.mp4` | `slash_640` | 5-7 | 空中跳斩 |
| `protag_launcher` | `videos/characters/protag_launcher.mp4` | `body_512` | 2（取第 2/4 帧） | 裂空上挑 |
| `protag_air_chase` | `videos/characters/protag_air_chase.mp4` | `dash_768` | 7-10 | 空中追击 |
| `protag_magic_bolt` | `videos/characters/protag_magic_bolt.mp4` | `body_512` | 5-7 | 只生施法动作，飞行魔法弹另生 VFX |
| `protag_ally_support` | `videos/characters/protag_ally_support.mp4` | `slash_640` | 6-8 | 借队友力量支援 |
| `protag_break_limit` | `videos/characters/protag_break_limit.mp4` | `dash_768` | 8-10 | 断限追击角色本体，魔法阵/长拖尾另生 VFX |
| `protag_hurt` | `videos/characters/protag_hurt.mp4` | `body_512` | 3-5 | 受击 |
| `protag_launched` | `videos/characters/protag_launched.mp4` | `body_512` | 3-5 | 被击飞 |
| `protag_knockdown` | `videos/characters/protag_knockdown.mp4` | `floor_1024` | 5-8 | 倒地 |
| `protag_recover` | `videos/characters/protag_recover.mp4` | `body_512` | 5-6 | 起身 |
| `protag_dead` | `videos/characters/protag_dead.mp4` | `floor_1024` | 8-10 | 战败 |

最终输出仍使用 Batch03 的正式文件名，例如 `protag_basic1_strip.png`。不要在视频文件名里加 `_video`、`_new`、`_final2` 之类临时后缀；版本信息放到归档目录或参数文件。

### 4.3 当前敌人和 Boss 动作清单

本批敌人和 Boss 动作清单继承 Batch04。

#### 低阶爪兽 `en_claw_beast`

| Clip ID | 视频源建议 | 继承 profile | 抽帧建议 | 说明 |
| --- | --- | --- | ---: | --- |
| `en_claw_beast_idle` | `videos/enemies_boss/en_claw_beast_idle.mp4` | `small_enemy` | 6-8 | 低伏呼吸 |
| `en_claw_beast_run` | `videos/enemies_boss/en_claw_beast_run.mp4` | `small_enemy` | 6-8 | 四足奔跑 |
| `en_claw_beast_claw_attack` | `videos/enemies_boss/en_claw_beast_claw_attack.mp4` | `small_enemy_wide` | 6-8 | 前扑爪击 |
| `en_claw_beast_hit` | `videos/enemies_boss/en_claw_beast_hit.mp4` | `small_enemy` | 3-5 | 受击后仰 |
| `en_claw_beast_launched` | `videos/enemies_boss/en_claw_beast_launched.mp4` | `small_enemy_air` | 3-4 | 被挑空 |
| `en_claw_beast_fall` | `videos/enemies_boss/en_claw_beast_fall.mp4` | `small_enemy_air` | 3-4 | 下坠 |
| `en_claw_beast_dead` | `videos/enemies_boss/en_claw_beast_dead.mp4` | `small_enemy_wide` | 6-8 | 非血腥死亡 |

#### 投石魔物 `en_forest_thrower`

| Clip ID | 视频源建议 | 继承 profile | 抽帧建议 | 说明 |
| --- | --- | --- | ---: | --- |
| `en_forest_thrower_idle` | `videos/enemies_boss/en_forest_thrower_idle.mp4` | `small_enemy` | 6-8 | 佝偻警戒 |
| `en_forest_thrower_run` | `videos/enemies_boss/en_forest_thrower_run.mp4` | `small_enemy` | 6-8 | 短步移动 |
| `en_forest_thrower_throw` | `videos/enemies_boss/en_forest_thrower_throw.mp4` | `small_enemy_wide` | 7-9 | 举石、后摆、投出；飞行石头另生 projectile |
| `en_forest_thrower_hit` | `videos/enemies_boss/en_forest_thrower_hit.mp4` | `small_enemy` | 3-5 | 受击 |
| `en_forest_thrower_launched` | `videos/enemies_boss/en_forest_thrower_launched.mp4` | `small_enemy_air` | 3-4 | 被击飞 |
| `en_forest_thrower_dead` | `videos/enemies_boss/en_forest_thrower_dead.mp4` | `small_enemy_wide` | 6-8 | 倒地或魔雾消散 |

#### 跳扑魔物 `en_pouncer`

| Clip ID | 视频源建议 | 继承 profile | 抽帧建议 | 说明 |
| --- | --- | --- | ---: | --- |
| `en_pouncer_idle` | `videos/enemies_boss/en_pouncer_idle.mp4` | `small_enemy` | 6-8 | 压低身体 |
| `en_pouncer_crouch` | `videos/enemies_boss/en_pouncer_crouch.mp4` | `small_enemy` | 4-5 | 跳扑前蓄力 |
| `en_pouncer_leap` | `videos/enemies_boss/en_pouncer_leap.mp4` | `small_enemy_wide` | 6-8 | 空中扑击 |
| `en_pouncer_land` | `videos/enemies_boss/en_pouncer_land.mp4` | `small_enemy` | 4-5 | 落地回收 |
| `en_pouncer_hit` | `videos/enemies_boss/en_pouncer_hit.mp4` | `small_enemy` | 3-5 | 受击 |
| `en_pouncer_dead` | `videos/enemies_boss/en_pouncer_dead.mp4` | `small_enemy_wide` | 6-8 | 非血腥失败 |

#### 黑熊丈夫 Boss `boss_bear_husband`

| Clip ID | 视频源建议 | 继承 profile | 抽帧建议 | 说明 |
| --- | --- | --- | ---: | --- |
| `boss_bear_husband_idle` | `videos/enemies_boss/boss_bear_husband_idle.mp4` | `boss_bear_husband` | 8-10 | 重呼吸 |
| `boss_bear_husband_walk` | `videos/enemies_boss/boss_bear_husband_walk.mp4` | `boss_bear_husband` | 8-10 | 沉重移动 |
| `boss_bear_husband_claw_attack` | `videos/enemies_boss/boss_bear_husband_claw_attack.mp4` | `boss_bear_husband_wide` | 8-10 | 横扫爪击 |
| `boss_bear_husband_charge_windup` | `videos/enemies_boss/boss_bear_husband_charge_windup.mp4` | `boss_bear_husband` | 6-8 | 冲撞蓄力 |
| `boss_bear_husband_charge_loop` | `videos/enemies_boss/boss_bear_husband_charge_loop.mp4` | `boss_bear_husband` | 6-8 | 冲撞循环 |
| `boss_bear_husband_shockwave_cast` | `videos/enemies_boss/boss_bear_husband_shockwave_cast.mp4` | `boss_bear_husband_wide` | 8-10 | 砸地释放姿态，冲击波另生 VFX |
| `boss_bear_husband_roar` | `videos/enemies_boss/boss_bear_husband_roar.mp4` | `boss_bear_husband` | 6-8 | 咆哮和阶段切换 |
| `boss_bear_husband_hit` | `videos/enemies_boss/boss_bear_husband_hit.mp4` | `boss_bear_husband` | 3-5 | 受击硬直 |
| `boss_bear_husband_launched` | `videos/enemies_boss/boss_bear_husband_launched.mp4` | `boss_bear_husband` | 3-5 | 短促浮空 |
| `boss_bear_husband_fall` | `videos/enemies_boss/boss_bear_husband_fall.mp4` | `boss_bear_husband` | 4-5 | 下落 |
| `boss_bear_husband_break_stun` | `videos/enemies_boss/boss_bear_husband_break_stun.mp4` | `boss_bear_husband` | 6-8 | 破防眩晕 |
| `boss_bear_husband_dead` | `videos/enemies_boss/boss_bear_husband_dead.mp4` | `boss_bear_husband_wide` | 8-10 | 非血腥倒下 |

### 4.3.1 当前 Sandbox 运行时替换清单（2026-08-03）

这张表是下一轮生成和后续替换的唯一执行清单，来自当前
`WheatearEditor/assets/vertical_slice/data/side_combat_tuning.yaml`、
`10_side_combat_actions.yaml` 以及 Batch03/Batch04 的 `sheet_params.yaml`。

- `runtime key` 是引擎真正读取的动作名；不要把旧设计名直接当成运行时文件名。
- `frameCount`、`FPS`、`loop` 是当前默认值。收到新的 sheet 后，如果 JSON 或参数表给出的实际帧数不同，以当前 sheet 为准，再同步 YAML。
- `profile` 决定每帧固定 canvas、pivot 和 baseline。不要把 `renderScale` 烘焙进图片；它是当前运行时补偿参数。
- 角色本体只保留身体、武器本体和少量贴身光。剑气、冲击波、魔法弹、魔法阵、尘土和远距离拖尾继续单独做 VFX。

#### 主角

| runtime key | 建议输出 clip ID | Profile / 单帧尺寸 | 帧数 | FPS | loop | 当前状态 |
| --- | --- | --- | ---: | ---: | --- | --- |
| `idle` | `protag_idle` | `body_512` / 512x512 | 8 | 8 | true | 已由当前 ZIP 替换 |
| `run` | `protag_run` | `body_512` / 512x512 | 4 | 8 | true | 已由当前 ZIP 替换 |
| `jump` | `protag_jump` | `body_512` / 512x512 | 1 | 18 | false | 已由 `jump.zip` 替换 |
| `fall` | `protag_fall` | `body_512` / 512x512 | 1 | 12 | true | 已由当前 ZIP 替换 |
| `hit` | `protag_hit` | `body_512` / 512x512 | 4 | 14 | false | 已由当前 ZIP 替换 |
| `dead` | `protag_dead` | `floor_1024` / 1024x512 | 8 | 12 | false | 待生成 |
| `basic1` | `protag_basic1` | `body_512` / 512x512 | 4 | 14 | false | 已由当前 ZIP 替换 |
| `basic2` | `protag_basic2` | `body_512` / 512x512 | 4 | 12 | false | 已由当前 ZIP 替换 |
| `basic3` | `protag_basic3` | `body_512` / 512x512 | 4 | 10 | false | 已由当前 ZIP 替换 |
| `air_basic` | `protag_air_basic` | `body_512` / 512x512 | 4 | 14 | false | 已由当前 ZIP 替换 |
| `launcher` | `protag_launcher` | `body_512` / 512x512 | 2 | 10 | false | 已由 `launcher.zip` 第 2/4 帧替换 |
| `air_chase` | `protag_air_chase` | `dash_tall_768` / 768x640 | 8 | 24 | false | 待生成 |
| `magic_bolt` | `protag_magic_bolt` | `body_512` / 512x512 | 9 | 20 | false | 待生成 |
| `ally_support` | `protag_ally_support` | `slash_640` / 640x512 | 8 | 18 | false | 待生成 |
| `break_limit` | `protag_break_limit` | `dash_1024` / 1024x512 | 12 | 24 | false | 待生成 |

主角当前剩余 5 个运行时待生成动作：`dead`、`air_chase`、`magic_bolt`、
`ally_support`、`break_limit`。`jump_loop` 和 `land` 的 sheet 已更新为 1 帧，
但目前不是 Sandbox 的独立运行时引用；旧表里的 `launched`、`knockdown`、
`recover` 除非后续状态机新增这些键，否则暂不生成。

#### 爪兽小怪

| runtime key | 建议输出 clip ID | Profile / 单帧尺寸 | 帧数 | FPS | loop | 当前状态 |
| --- | --- | --- | ---: | ---: | --- | --- |
| `idle` | `en_claw_beast_idle` | `small_enemy` / 512x384 | 4 | 7 | true | 待生成 |
| `run` | `en_claw_beast_run` | `small_enemy` / 512x384 | 5 | 11 | true | 待生成 |
| `hit` | `en_claw_beast_hit` | `small_enemy_wide` / 768x384 | 3 | 12 | false | 待生成 |
| `fall` | `en_claw_beast_fall` | `small_enemy_air_wide` / 768x512 | 3 | 9 | true | 待生成 |
| `dead` | `en_claw_beast_dead` | `small_enemy_wide` / 768x384 | 4 | 7 | false | 待生成 |
| `enemy_claw` | `en_claw_beast_attack` | `small_enemy_wide` / 768x384 | 4 | 14 | false | 待生成 |

#### 黑熊丈夫 Boss

| runtime key | 建议输出 clip ID | Profile / 单帧尺寸 | 帧数 | FPS | loop | 当前状态 |
| --- | --- | --- | ---: | ---: | --- | --- |
| `idle` | `boss_bear_husband_idle` | `boss_bear_husband` / 1024x768 | 4 | 6 | true | 待生成 |
| `run` | `boss_bear_husband_walk` | `boss_bear_husband` / 1024x768 | 5 | 8 | true | 待生成 |
| `hit` | `boss_bear_husband_hit` | `boss_bear_husband` / 1024x768 | 3 | 10 | false | 待生成 |
| `fall` | `boss_bear_husband_fall` | `boss_bear_husband` / 1024x768 | 3 | 8 | true | 待生成 |
| `dead` | `boss_bear_husband_dead` | `boss_bear_husband_wide` / 1280x768 | 4 | 7 | false | 待生成 |
| `enemy_claw` | `boss_bear_husband_attack` | `boss_bear_husband_wide` / 1280x768 | 4 | 12 | false | 待生成 |
| `bear_charge` | `boss_bear_husband_charge` | `boss_bear_husband` / 1024x768 | 4 | 12 | false | 待生成 |
| `bear_shockwave` | `boss_bear_husband_shockwave` | `boss_bear_husband_wide` / 1280x768 | 4 | 12 | false | 待生成 |

#### 交付格式

每个动作建议单独一个 ZIP，至少包含：

```text
spritesheet_1.png
spritesheet_1.json
```

`spritesheet_1.png` 必须是横向固定格 sprite sheet，格子从左到右、从第 0 帧开始；JSON 必须能读出每格的 `x/y/w/h` 和总帧数。不要 trim、不要跨格、不要把相邻帧拼成不等宽图片。若同时提供逐帧 PNG，文件名使用同一个 clip ID 和三位帧号。

实际替换时会按以下顺序处理：读取 sheet JSON/参数表 -> 裁成运行时单帧 PNG -> 更新 `side_combat_tuning.yaml` 的帧数和必要时 FPS -> 清理旧的多余帧 -> 重建 player package -> 做 Sandbox 启动检查。因此文档目标帧数和实际 sheet 帧数不一致时，以实际 sheet 为准。

### 4.4 动作弧

动作视频必须有可读的时间结构。动作长短可以按生成工具调整，但通常应包含：

```text
anticipation/startup -> windup -> active/release -> follow-through -> recovery
```

循环动作需要首尾可接；一次性动作需要有明确的开始和结束。动作力量来自身体重心、腰肩旋转、脚步、武器轨迹和惯性，不依赖一团光效遮盖角色。

### 4.5 角色本体与远程内容分层

角色动作视频中只保留：

- 角色身体。
- 角色手持的武器本体。
- 小范围贴身光、施法蓄力光或武器自带微光。

以下内容必须单独生成：

- 飞行魔法弹、火球、投石和其他 projectile。
- 离手后的剑气、冲击波、长距离拖尾和爆炸。
- 远离角色的魔法阵、地面裂纹、Boss 冲击波。
- 大范围命中爆光、屏幕级闪光和独立烟尘。

角色动作视频和远程 VFX 视频使用同一摄像机视角和同一方向约定，但可以使用不同视频母版分档。后处理时通过 `attach`、`offset` 或运行时事件组合，不把远程特效硬烘焙到角色帧里。

## 5. 特效视频生产规则

### 5.1 一个视频只制作一种特效

每个 VFX 视频只包含一种效果，例如：

- `vfx_magic_bolt_projectile`
- `vfx_basic_slash`
- `vfx_launcher_slash`
- `vfx_magic_impact`
- `vfx_boss_bear_shockwave`
- `vfx_landing_dust`
- `vfx_break_limit_circle`

不把魔法弹、命中爆炸、烟尘和角色一起生成。需要多层叠加时，分别生成多个 VFX clip。

### 5.2 特效参考图

VFX 可以使用一张参考图锁定形状和颜色，也可以使用角色动作关键帧作为位置参考。参考图只用于：

- 颜色体系。
- 光效边缘和粒子密度。
- 运动方向。
- 视觉中心和挂点关系。

不要把角色完整身体作为 VFX 视频主体。特效视频应能在不依赖角色图像的情况下单独抽帧和复用。

### 5.3 特效边界

- 最大爆发帧、最长拖尾帧和最宽冲击波帧都必须完整留在画布内。
- 圆环、魔法阵、冲击波两端和粒子末端不能被切掉。
- 不使用可读文字、字母、数字、符文或商业作品图案。
- 不用模糊的整块白光掩盖形状；光效要有清晰核心、外缘和消散过程。

## 6. 从视频到运行时帧

### 6.1 推荐处理链

```text
video_sources/*.mp4
    -> normalize to selected masterCanvas
    -> remove extra lead-in and tail
    -> trim to effective action duration
    -> extract 3-10 key frames
    -> chroma key green background
    -> subject/effect segmentation and matte cleanup
    -> fit into fixed runtime profile
    -> validate at target scale
    -> assemble strip or VFX sheet
```

视频源必须使用绿幕或记录过的备用 key color。视频源不要求直接带 alpha；但最终交付的角色帧和 VFX 帧必须是 RGBA PNG，空白区域 alpha 为 0，不能把绿幕或备用底色带入运行时。

### 6.2 抽帧与时间

- 不要求视频源是 24fps；生成工具输出多少 fps 都可以，后处理只抽关键帧。
- 每个运行时 clip 默认 `targetFrameCount <= 10`；只有全屏 UI/VFX 预览或特殊 Boss 演出才考虑更多帧。
- 运行时 `frameRate` 通常在 `6-16 fps` 之间。攻击和 VFX 可以更快，idle/dead 可以更慢。
- 通过关键姿态抽帧得到目标帧数，不要求均匀抽取；起手、命中峰值、收招和循环接点比等间隔更重要。
- 命中峰值可以在运行时 hold 1 帧或由 hitstop 事件停顿；不要为了停顿感复制一堆几乎相同的 PNG。
- 删除重复帧、严重模糊帧、动作跳变帧和生成视频首尾的静止等待帧。
- 不为了凑帧数保留明显坏帧；宁可减少帧数或重新生成视频。

### 6.3 统一运行时规格

视频抽帧之后仍按原有运行时 profile 接入：

| 内容 | 统一要求 |
| --- | --- |
| 角色动作 | 同一 clip 固定 `cellWidth x cellHeight`、pivot、baseline 和视觉尺度 |
| 敌人/Boss 动作 | 同一 clip 固定 profile；大动作可换专用 profile，但不能缩放主体 |
| VFX | 同一 VFX clip 固定 cell、挂点、pivot 和方向 |
| 透明 | 最终 PNG 为 RGBA，空白 alpha=0，无白边、灰边、脏 matte |
| 拼接 | 只把验收通过的源帧横向拼接成一行，不混动作、不跨格 |
| 碰撞 | 图片画布不参与碰撞，hitbox/hurtbox/事件数据独立配置 |

视频分档统一，不代表所有运行时 cell 必须相同。运行时 cell 仍然按照动作最大包围盒决定；`body_512`、`slash_640`、`floor_1024` 等 profile 继续沿用 Batch03/04/06 的定义。

## 7. 批次参数文件

每个视频 clip 至少记录以下信息：

```yaml
clipId: protag_basic1
category: character_action
entity: protag
sourceVideo: videos/characters/protag_basic1.mp4
referenceImages:
  - references/protag_magic_swordsman.png
videoCanvasProfile: character_square
masterCanvas: [1024, 1024]
videoTimingProfile: light_attack
generatedClipSeconds: 4.0
effectiveActionSeconds: 0.45
targetFrameRate: 12
targetFrameCount: 7
keyColor: "#00FF00"
cameraPreset: locked_side_view_right
facing: right
profile: slash_640
preserveFullContent: true
safeMarginPercent: 8
alphaMethod: segmentation_then_matte_cleanup
detachedVfx:
  - vfx_basic_slash
```

远程 VFX 示例：

```yaml
clipId: vfx_magic_bolt_projectile
category: detached_vfx
sourceVideo: videos/vfx/vfx_magic_bolt_projectile.mp4
referenceImages:
  - references/vfx_magic_bolt_style.png
videoCanvasProfile: projectile_wide
masterCanvas: [768, 432]
videoTimingProfile: projectile_loop
generatedClipSeconds: 4.0
effectiveActionSeconds: 0.5
targetFrameRate: 10
targetFrameCount: 5
keyColor: "#00FF00"
cameraPreset: locked_side_view_right
profile: vfx_magic_bolt_projectile
attach: projectile_spawn
preserveFullContent: true
safeMarginPercent: 8
alphaMethod: segmentation_then_matte_cleanup
containsCharacter: false
```

## 8. Prompt 模板

### 8.1 角色动作视频

```text
Use the reference image as the identity and design anchor.
Create one continuous animation video of an original 2D side-scrolling action game character:
[CHARACTER DESCRIPTION], [ACTION DESCRIPTION].
Generate a 4 second source video on a pure green screen. Use DNF-like snappy
key-pose timing in the effective action segment: few strong readable poses,
clear impact timing, slight hold on the hit frame, not smooth 24fps cinematic
interpolation. For one-shot actions, perform the action once and then hold the
final pose; for loop actions, repeat a stable loop.

Keep the exact same face, hair, costume, equipment, weapon, body proportions,
side-view orientation and visual scale as the reference image.
The character faces [RIGHT OR LEFT].

Solid pure chroma key green background (#00FF00), flat and evenly lit,
no background texture, no gradient, no floor line, no contact shadow,
no cast shadow, no scene, no props, no UI.

Locked camera, fixed side-view angle, fixed focal length, fixed camera distance,
fixed composition, fixed background, no pan, no tilt, no roll, no zoom,
no dolly, no perspective change, no reframing, no crop, no cut, no transition,
no camera shake.

The entire body, hair, weapon, coat, tail and attached glow remain fully visible
inside the selected video canvas with at least 8 percent safe margin on every side.
Show a clear action arc: anticipation, active movement, impact or release when relevant,
follow-through and recovery. Preserve continuous motion between frames.

Only include the character body, held weapon and small attached effects.
Do not include remote projectiles, long sword waves, distant magic circles,
large explosions or screen-wide effects; those are separate VFX clips.

No text, no letters, no numbers, no logo, no watermark, no UI,
no second character, no extra weapon, no costume change, no cropped body,
no cropped weapon, no camera movement, no scene transition, no readable symbols.
```

### 8.2 远程 VFX 视频

```text
Create one continuous animation video of one standalone 2D pixel-art action game VFX:
[VFX DESCRIPTION].
Generate a 4 second source video on a pure green screen. Keep the effective VFX
segment short and readable; the final extracted sequence must stay within 10 frames.

Use the reference image for the VFX shape, color, edge language and motion direction.
Solid pure chroma key green background (#00FF00), or a documented alternate key color
if the VFX itself is green. Flat key background only, no scene, no floor, no shadow.
Fixed selected video canvas, locked side-view camera, fixed scale,
fixed composition, no pan, no zoom, no perspective change, no reframing,
no crop, no camera shake, no cut and no transition.

The complete effect remains inside the frame with at least 8 percent safe margin.
Show a clear birth, build-up, readable peak and fade-out or loop.
Keep the visual center and direction stable for the specified runtime attach point.
Standalone effect only: no complete character body, no unrelated scene.

No text, no letters, no numbers, no readable runes, no logo, no watermark,
no black background, no white matte, no cropped effect, no effect crossing the frame edge,
no muddy glow, no blurry painted blob, no copied commercial game effect.
```

## 9. 推荐归档结构

```text
Batch09_SideCombat动作与特效视频生产包/
  references/
    characters/
    enemies_boss/
    vfx/
  videos/
    characters/
    enemies_boss/
    vfx/
  extracted_frames/
    <clip_name>/
  cleaned_source_frames/
    <clip_name>/
  previews/
    <clip_name>_video_preview.mp4
    <clip_name>_contact_sheet.png
    <clip_name>_checker_preview.png
  batch09_video_params.yaml
  README.md
```

运行时正式资源仍迁移到：

```text
WheatearEditor/assets/vertical_slice/source_frames/
WheatearEditor/assets/vertical_slice/side_combat/sheets/
WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/
```

`AI生成素材批次/` 只保存视频源、参考图、中间帧、最终交付副本和参数记录，不作为游戏运行时引用路径。

## 10. 交付顺序

推荐按下面顺序批量生成：

1. 先生成主角 `protag_idle`、`protag_run`、`protag_basic1`、`protag_basic2`、`protag_basic3`、`protag_launcher`、`protag_magic_bolt`、`protag_hurt`、`protag_knockdown`、`protag_recover`。
2. 再生成低阶小怪和 Boss 的 `idle`、`run/walk`、`attack`、`hit`、`launched/fall`、`dead`。
3. 单独生成 `basic_slash`、`launcher_slash`、`magic_bolt_projectile`、`magic_impact`、`landing_dust`、`boss_bear_shockwave` 等 VFX。
4. 抽取一组代表帧，先确认角色尺度、视角、画布和 alpha 处理，再批量处理同一角色的其他 clip。
5. 源帧全部通过后，按 Batch03/04/06 的 profile 和参数拼接 strip。

## 11. 验收标准

视频阶段不合格：

- 摄像机有移动、缩放、旋转、跟拍或视角变化。
- 主体进入或离开画面，身体、武器、爪子、尾巴或特效被裁掉。
- 角色脸、服装、武器、朝向或体型在视频中明显变化。
- 一个视频混入多个动作、多个角色、镜头切换或场景转场。
- 远程 projectile、冲击波或大范围魔法直接粘在角色动作中，无法独立拆出。
- 动作只有姿势变化，没有连续的起手、发力、峰值和收招。
- 画面出现文字、Logo、水印、可读符文或商业作品可识别元素。

抽帧阶段不合格：

- 抽帧后的角色或 VFX 没有统一到固定 runtime profile。
- alpha 中残留纯色背景、白边、灰边、黑边、半透明矩形或脏 matte。
- 相邻帧主体比例、pivot、baseline 或视觉中心明显漂移。
- VFX 峰值贴边、圆环断裂、拖尾被截断或效果跨入相邻 cell。
- 低分辨率游戏内播放时动作语义或效果方向不可读。

## 12. 与旧批次的关系

- Batch03、Batch04、Batch06 的单帧源文件和既有 strip 仍然有效，不需要为了本批重新生成。
- 新动作、新敌人、新 Boss 和新 VFX 默认采用本批视频优先流程。
- 如果某个视频始终无法保持角色身份或镜头稳定，可以退回旧的单帧源文件流程；这属于例外，不作为新批次默认方式。
- 无论采用视频还是单帧生成，最终运行时规则始终以《竖切美术资源生产规范》中的 RGBA、固定 cell、pivot/baseline、scaleReference 和 VFX 拆分规则为准。
