# Batch06 SideCombat 技能 VFX 与命中特效 Sheet 生产包

更新时间：2026-07-31

本包承接 Batch03 主角动作、Batch04 敌人与 Boss 动作、Batch05 战斗场景，用于生产独立的横板战斗高清像素风 VFX 源帧，并在验收后拼接成 VFX sheet。目标是把剑气、魔法弹、命中火花、Boss 冲击波、落地烟尘和掉落吸附光从角色 sheet 中拆出来，避免长剑动作、冲击波或大光效导致角色帧串格和裁切。

## 1. 本批核心原则

- 角色动作 sheet 只放角色身体、武器本体和少量贴身光；大范围剑气、冲击波、爆光、魔法阵全部放在本批 VFX。
- VFX 用来增强动作力道，不能替代身体发力。剑气、命中火花、冲击波必须跟角色或 Boss 的 `activeImpact` 帧对齐，强化攻击方向、停顿感和跟随惯性；不能用大光团遮住软弱或不连贯的动作。
- AI 生成阶段必须先交付单帧源文件：`source_frames/<vfx_clip>/<vfx_clip>_000.png`、`001.png` 等；不要直接生成整张横向 VFX strip。
- VFX 可以大，但每个源帧 canvas 仍然必须固定大小；光效、粒子、拖尾、冲击波边缘必须完整在本帧内。
- 不同 VFX 根据实际包围盒选择 cell，不强行统一成一个尺寸。长横向剑气用宽 cell，竖向上挑用方形或高 cell。
- 每个 VFX clip 必须记录 `pivotX/pivotY` 和运行时挂点 offset。默认中心 pivot 为 `cellWidth / 2, cellHeight / 2`；地面冲击波、落地烟尘这类可以使用 `ground_center` pivot，但必须在 `vfx_params.yaml` 写清楚。裁切图集必须附带每帧 `sourceRect` 和 `offsetFromPivot`。
- VFX 默认 **RGBA PNG 真透明**，无用区域 alpha 为 0；按 `additive-like` 观感绘制，但不要烘焙纯黑底、白底、灰底、棋盘格底、白色 matte 或半透明残底。
- 每张最终 sheet 只放一种 VFX，一行横向排列，不混合轻重命中或多个技能；最终 sheet 必须由验收通过的源帧拼接得到。
- 禁止出现文字、符号、法阵字母、Logo、水印、帧号、网格线。
- VFX 统一为高清像素风：边缘是清楚的像素化光块和粒子，不是模糊的矢量光刷或低清截图。

## 2. Cell 与边界规则

VFX 的正确做法是“按最大扩散帧定 cell”。不要按第一帧小火花定 cell，也不要让最后一帧残影跨到隔壁格。

```yaml
frameIntegrity:
  noCrossCellBleed: true
  safePaddingMinimum: 32
  alphaEdge: clean
  sameCellEveryFrame: true
  unusedPixelsAlpha: 0
  noWhiteMatte: true
```

具体规则：

- 横向剑气：剑气最宽那一帧左右至少留 48px 透明边。
- 竖向上挑：最高那一帧顶部至少留 48px 透明边。
- 爆炸/命中火花：最大爆发半径外至少留 40px 透明边。
- 冲击波：如果 1024px 宽还不够，宁可用 `1536x512` cell，也不要裁切。
- 魔法阵：圆环必须完整，不得切边；如果要出画面，应在引擎里缩放/裁剪，而不是素材本身断掉。

## 3. 本批资产清单

| VFX ID | 正式输出文件 | Cell | 帧数 | FPS | 输出尺寸 | 用途 |
| --- | --- | --- | ---: | ---: | --- | --- |
| basic_slash | `WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/vfx_basic_slash_strip.png` | 768x384 | 8 | 30 | 6144x384 | 三段斩横向剑气 |
| basic_slash_heavy | `WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/vfx_basic_slash_heavy_strip.png` | 896x448 | 10 | 30 | 8960x448 | 第三段收尾重剑气 |
| launcher_slash | `WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/vfx_launcher_slash_strip.png` | 768x768 | 10 | 30 | 7680x768 | 上挑竖向剑气 |
| air_slash | `WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/vfx_air_slash_strip.png` | 768x512 | 8 | 30 | 6144x512 | 空中跳斩弧线 |
| air_chase_trail | `WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/vfx_air_chase_trail_strip.png` | 896x512 | 10 | 30 | 8960x512 | 空中追击拖尾 |
| break_limit_circle | `WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/vfx_break_limit_circle_strip.png` | 1024x1024 | 16 | 30 | 16384x1024 | 断限魔法阵碎裂 |
| break_limit_dash | `WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/vfx_break_limit_dash_strip.png` | 1280x640 | 12 | 30 | 15360x640 | 断限冲刺残影 |
| magic_bolt_projectile | `WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/vfx_magic_bolt_projectile_strip.png` | 384x192 | 8 | 24 | 3072x192 | 飞行魔法弹循环 |
| magic_cast_spark | `WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/vfx_magic_cast_spark_strip.png` | 512x512 | 8 | 24 | 4096x512 | 剑尖蓄魔光 |
| magic_impact | `WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/vfx_magic_impact_strip.png` | 768x768 | 10 | 30 | 7680x768 | 魔法命中爆裂 |
| hit_spark_light | `WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/vfx_hit_spark_light_strip.png` | 512x512 | 8 | 30 | 4096x512 | 轻命中火花 |
| hit_spark_heavy | `WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/vfx_hit_spark_heavy_strip.png` | 768x768 | 10 | 30 | 7680x768 | 重命中火花 |
| guard_flash | `WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/vfx_guard_flash_strip.png` | 640x640 | 8 | 24 | 5120x640 | 防御/支援保护闪光 |
| boss_bear_charge_dust | `WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/vfx_boss_bear_charge_dust_strip.png` | 1024x384 | 10 | 24 | 10240x384 | Boss 冲锋地面尘土 |
| boss_bear_shockwave | `WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/vfx_boss_bear_shockwave_strip.png` | 1536x512 | 12 | 24 | 18432x512 | Boss 砸地冲击波 |
| landing_dust | `WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/vfx_landing_dust_strip.png` | 768x384 | 8 | 24 | 6144x384 | 主角/Boss 落地烟尘 |
| pickup_glow | `WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/vfx_pickup_glow_strip.png` | 384x384 | 8 | 12 | 3072x384 | 掉落吸附光 |
| level_up_burst | `WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/vfx_level_up_burst_strip.png` | 1024x1024 | 14 | 24 | 14336x1024 | 战后升级/魔剑响应 |

## 4. 通用负面要求

所有提示词追加：

```text
no text, no letters, no numbers, no logo, no watermark, no signature, no UI, no background, no black background, no white background, no gray background, no checkerboard background, no white matte, no opaque background rectangle, no dirty alpha edge, no visible cell borders, no frame numbers, no cropped effect, no effect crossing into neighboring cells, no inconsistent cell size, no muddy glow, no recognizable franchise magic circle, no readable runes, no blurry non-pixel-art glow
```

## 5. VFX 源帧 Prompt 模板

将 `[VFX DESCRIPTION]`、`[FRAME INDEX]`、`[FRAME COUNT]`、`[CELL WIDTH]`、`[CELL HEIGHT]`、`[POSE FOR THIS FRAME]` 替换为对应内容。每个 VFX 逐帧生成，不要直接输出横向 strip。

```text
Use case: stylized-concept
Asset type: single animation frame for a 2D high-resolution pixel-art action game visual effect
Primary request: original SideCombat high-resolution pixel-art visual effect single frame, [VFX DESCRIPTION], frame [FRAME INDEX] of [FRAME COUNT], [POSE FOR THIS FRAME]
Style/medium: bright polished pixel-art anime action game VFX, clean additive-looking pixel clusters, crisp alpha edge, not 3D render, RGBA transparent PNG
Composition/framing: fixed [CELL WIDTH]x[CELL HEIGHT] RGBA transparent canvas, unused pixels alpha 0, one frame only, centered or attached according to vfx_params, invisible VFX pivot at pixel ([PIVOT_X], [PIVOT_Y]), no visible grid lines, do not draw pivot guides
Frame integrity: every glow, spark, slash arc, magic ring, smoke puff and particle remains inside this canvas with generous transparent padding; no cropped edges; same visual scale as the frame plan
Animation requirements: this is one numbered frame from a planned VFX animation; clear anticipation or birth frame, strong readable peak frame aligned to the character activeImpact timing, fading recovery frames with directional follow-through; loop only when the VFX is explicitly a loop; do not draw neighboring frames; do not create a horizontal sprite sheet
Lighting/mood: cyan blue magic and black-silver sword energy with a few warm hit highlights, readable over dark forest and bright UI
Constraints: RGBA transparent PNG, unused pixels alpha 0, no text, no letters, no numbers, no logo, no watermark, no background, no readable symbols, no white matte
Avoid: overblown white blob, muddy glow, cut-off magic circle, commercial game VFX copy, too much detail that becomes noisy in motion, dirty alpha edge, blurry painted glow, horizontal contact sheet, multiple VFX frames in one image
```

## 6. VFX 描述替换

```text
vfx_basic_slash_strip.png: fast horizontal cyan sword slash arc, thin black-silver core, clean trailing fade, for basic combo hits
vfx_basic_slash_heavy_strip.png: wider heavier horizontal finishing slash, stronger cyan burst, warm impact spark at center, not a full explosion
vfx_launcher_slash_strip.png: rising vertical diagonal sword energy arc, clear upward motion, suitable for launcher uppercut
vfx_air_slash_strip.png: compact aerial crescent slash, forward-down arc, readable while character is airborne
vfx_air_chase_trail_strip.png: fast airborne chase trail, cyan afterimage streaks, suggests forward burst without drawing character body
vfx_break_limit_circle_strip.png: full circular magic ring forming then cracking into cyan fragments, no readable runes, circle never cut off by cell edge
vfx_break_limit_dash_strip.png: long horizontal dash impact trail, black-silver slash core and cyan particles, all contained inside 1280x640 cell
vfx_magic_bolt_projectile_strip.png: small flying cyan magic bolt loop, black-silver core, no cast pose, projectile only
vfx_magic_cast_spark_strip.png: sword-tip magic charge spark, compact circular energy gathering, used before projectile release
vfx_magic_impact_strip.png: cyan magic hit burst, radial sparks and fading ring, no screen-filling explosion
vfx_hit_spark_light_strip.png: light hit spark, short yellow-white core with cyan rim, compact impact feedback
vfx_hit_spark_heavy_strip.png: heavy hit spark, stronger burst, star-shaped impact and short shock ring, contained in cell
vfx_guard_flash_strip.png: protective guard flash, translucent shield shimmer, cyan edge, no letters or emblem
vfx_boss_bear_charge_dust_strip.png: low ground dust and claw scrape trail for boss charge, horizontal but contained
vfx_boss_bear_shockwave_strip.png: ground shockwave expanding left and right, cyan cracks and dust, wide 1536x512 cell, no cropped wave edge
vfx_landing_dust_strip.png: soft landing dust puff, ground-level smoke, not too opaque, works for player and boss landings
vfx_pickup_glow_strip.png: small pickup absorption glow, spiral spark rising into item, compact loop
vfx_level_up_burst_strip.png: celebratory cyan magic burst, sword resonance feel, radial particles and soft ring, no text or level number
```

## 7. vfx_params.yaml 示例

```yaml
defaults:
  productionMode: source_frames_first
  coordinateSystem: image_top_left_x_right_y_down
  rowOrigin: top
  pivot: center
  transparent: true
  requireRgba: true
  unusedPixelsAlpha: 0
  blendHint: additive_like
  noCrossCellBleed: true
  noVisibleGrid: true
  noWhiteMatte: true
  sourceFramePattern: source_frames/{clip}/{clip}_{frame:03}.png
  assembleStripAfterValidation: true
  framePlanRequired: true
  requirePivotMetadata: true
  frameOffsetMode: fixed_canvas_no_per_frame_offset
  trimmedAtlasRequiresOffsetFromPivot: true

clips:
  vfx_basic_slash_strip: { cellWidth: 768, cellHeight: 384, frameCount: 8, frameRate: 30 }
  vfx_basic_slash_heavy_strip: { cellWidth: 896, cellHeight: 448, frameCount: 10, frameRate: 30 }
  vfx_launcher_slash_strip: { cellWidth: 768, cellHeight: 768, frameCount: 10, frameRate: 30 }
  vfx_air_slash_strip: { cellWidth: 768, cellHeight: 512, frameCount: 8, frameRate: 30 }
  vfx_air_chase_trail_strip: { cellWidth: 896, cellHeight: 512, frameCount: 10, frameRate: 30 }
  vfx_break_limit_circle_strip: { cellWidth: 1024, cellHeight: 1024, frameCount: 16, frameRate: 30 }
  vfx_break_limit_dash_strip: { cellWidth: 1280, cellHeight: 640, frameCount: 12, frameRate: 30 }
  vfx_magic_bolt_projectile_strip: { cellWidth: 384, cellHeight: 192, frameCount: 8, frameRate: 24, loop: true }
  vfx_magic_cast_spark_strip: { cellWidth: 512, cellHeight: 512, frameCount: 8, frameRate: 24 }
  vfx_magic_impact_strip: { cellWidth: 768, cellHeight: 768, frameCount: 10, frameRate: 30 }
  vfx_hit_spark_light_strip: { cellWidth: 512, cellHeight: 512, frameCount: 8, frameRate: 30 }
  vfx_hit_spark_heavy_strip: { cellWidth: 768, cellHeight: 768, frameCount: 10, frameRate: 30 }
  vfx_guard_flash_strip: { cellWidth: 640, cellHeight: 640, frameCount: 8, frameRate: 24 }
  vfx_boss_bear_charge_dust_strip: { cellWidth: 1024, cellHeight: 384, frameCount: 10, frameRate: 24 }
  vfx_boss_bear_shockwave_strip: { cellWidth: 1536, cellHeight: 512, frameCount: 12, frameRate: 24 }
  vfx_landing_dust_strip: { cellWidth: 768, cellHeight: 384, frameCount: 8, frameRate: 24 }
  vfx_pickup_glow_strip: { cellWidth: 384, cellHeight: 384, frameCount: 8, frameRate: 12, loop: true }
  vfx_level_up_burst_strip: { cellWidth: 1024, cellHeight: 1024, frameCount: 14, frameRate: 24 }

recommendedOffsets:
  vfx_basic_slash_strip: { pivotX: 384, pivotY: 192, attach: sword_mid, offset: [120, -30] }
  vfx_launcher_slash_strip: { pivotX: 384, pivotY: 384, attach: sword_tip, offset: [80, -170] }
  vfx_magic_cast_spark_strip: { pivotX: 256, pivotY: 256, attach: sword_tip, offset: [0, 0] }
  vfx_boss_bear_shockwave_strip: { pivotX: 768, pivotY: 384, attach: ground_center, offset: [0, 0] }
  vfx_landing_dust_strip: { pivotX: 384, pivotY: 292, attach: ground_center, offset: [0, 0] }
```

`frame_plan.yaml` 最小示例：

```yaml
vfx_boss_bear_shockwave:
  cellWidth: 1536
  cellHeight: 512
  frameCount: 12
  pivot: center
  pivotX: 768
  pivotY: 384
  attach: ground_center
  offset: [0, 0]
  frames:
    0: "tiny ground crack birth at center, mostly transparent canvas"
    1: "cyan crack begins expanding left and right"
    2: "dust lifts from ground, shockwave width about 35 percent of canvas"
    3: "shockwave width about 55 percent, edges still far from canvas border"
    4: "main impact ridge expands, strongest cyan core"
    5: "peak width, still leaves at least 48px transparent padding at left and right"
    6: "shockwave starts fading, dust breaks into pixel clusters"
    7: "outer cracks dim, center glow softens"
    8: "particles separate upward, no opaque smoke block"
    9: "mostly fading dust and tiny cyan sparks"
    10: "residual ground glow, transparent edge clean"
    11: "almost gone, loop/end frame clean"
```

生成时先写每个 VFX 的 `frame_plan.yaml`，再逐帧生成。峰值帧如果贴边，扩大 cell，不要裁切或缩小整个特效。

## 8. 推荐交付结构

```text
Batch06_SideCombat_VFX/
  source_frames/
    vfx_basic_slash/vfx_basic_slash_000.png ... vfx_basic_slash_007.png
    vfx_break_limit_circle/vfx_break_limit_circle_000.png ... vfx_break_limit_circle_015.png
    vfx_boss_bear_shockwave/vfx_boss_bear_shockwave_000.png ... vfx_boss_bear_shockwave_011.png
    ...
  vfx_sheets/
    vfx_basic_slash_strip.png
    vfx_basic_slash_heavy_strip.png
    vfx_launcher_slash_strip.png
    vfx_air_slash_strip.png
    vfx_air_chase_trail_strip.png
    vfx_break_limit_circle_strip.png
    vfx_break_limit_dash_strip.png
    vfx_magic_bolt_projectile_strip.png
    vfx_magic_cast_spark_strip.png
    vfx_magic_impact_strip.png
    vfx_hit_spark_light_strip.png
    vfx_hit_spark_heavy_strip.png
    vfx_guard_flash_strip.png
    vfx_boss_bear_charge_dust_strip.png
    vfx_boss_bear_shockwave_strip.png
    vfx_landing_dust_strip.png
    vfx_pickup_glow_strip.png
    vfx_level_up_burst_strip.png
  vfx_params.yaml
```

## 9. 接入动作

1. 先把源帧放入 `WheatearEditor/assets/vertical_slice/source_frames/<vfx_clip>/` 并逐帧验收。
2. 验收通过后，用脚本或工具按 `vfx_params.yaml` 拼接到 `WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/vfx_*_strip.png`。
3. 按 `vfx_params.yaml` 切 clip，设置推荐 blend 模式或当前渲染器等效透明混合。
4. 在 `side_combat_tuning.yaml` 的技能事件、命中事件、Boss 事件中引用对应 VFX clip。
5. 主角和 Boss 角色 sheet 中如果已有大范围剑气/冲击波，接入本批后应逐步从角色动画里移除。
6. 重新打包 Sandbox，检查 `content.wtpack` 包含 `side_combat/vfx_sheets/vfx_*_strip.png`。

## 10. 验收标准

- 每张 VFX 源帧 PNG 尺寸必须等于对应 `cellWidth x cellHeight`；每张最终 strip PNG 尺寸必须等于 `cellWidth * frameCount` by `cellHeight`。
- 每张源帧都是完整特效，不缺圆环、不缺剑气边、不缺冲击波末端；拼接后的 strip 逐格切开必须与源帧一致。
- 最大爆发帧也必须留透明安全边，不贴边。
- VFX 缩到游戏内尺寸后，出生、峰值、消散过程清楚。
- 透明边缘干净，没有黑底、白底、脏边和硬裁切。
- 放到黑、白、亮粉、透明棋盘背景上检查，不能出现白底、灰底、棋盘格底、白色 matte、半透明残底或脏 alpha。
- 魔法阵不能出现可读文字或商业作品可识别图案。
- VFX 不把角色身体画进去，除非是抽象残影，不出现主角/敌人完整人物。
