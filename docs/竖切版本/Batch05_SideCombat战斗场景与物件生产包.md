# Batch05 SideCombat 战斗场景与物件生产包

更新时间：2026-07-31

本包用于把正式横板战斗场景从当前单张占位背景升级为分层战斗房间，并补齐世界内道具、可破坏物、掉落物和通用遮罩素材。目标是让黑林兽道看起来像“能打、能走位、能读纵深”的高清像素风横板动作场景。

## 1. 本批硬性规则

- 战斗背景按 2K 显示优先制作：主背景和可视层默认 `3840x2160`，运行时可以缩放到 1080p 或 1440p，不能靠 1080p 图硬放大到 2K。
- 分层背景必须保留横板战斗可读性：玩家能看出地面、前后纵深、危险区域和可站位范围。
- 除明确写着“不透明”的完整背景层外，所有透明物件、前景层、魔光层、掉落物、遮罩都必须是 **RGBA PNG 真透明**，无用区域 alpha 为 0，边缘干净，无白底、灰底、黑底、绿底、棋盘格底、白色 matte、半透明残底或脏边。
- 世界内掉落物和可破坏物动画先生成单帧源文件，再拼接成 sprite strip；每个源帧固定 canvas，掉落物、碎片和发光边缘完整在本帧内，不能裁切。
- 可破坏物和掉落物同样按该 clip 最大扩散帧定 cell：第一帧可能很小，破碎峰值或发光峰值可能很大，但同一个 clip 内所有源帧必须统一 canvas。若碎片、光环、旋转边缘放不下，只扩大这个 clip 的 cell，不缩小物件、不裁切、不跨格。
- 物件碰撞、拾取触发范围和遮挡范围独立于图片 canvas；例如破箱动画可以是 `512x512`，实际碰撞仍然可以是 `160x120`。
- 可破坏物和掉落物默认 `pivot: center`，`pivotX = cellWidth / 2`，`pivotY = cellHeight / 2`，`renderOffset = [-pivotX, -pivotY]`。如果后续裁掉透明边打 atlas，必须提供每帧 `sourceRect` 和 `offsetFromPivot`。
- UI 图标不在本批做最终 atlas，掉落 UI 图标在 Batch07 统一处理；本批只做世界内可拾取物。
- 背景和物件不允许出现文字、Logo、可识别商标、参考游戏图案或商业作品元素。
- 本批全部游玩素材采用高清像素风：参考 DNF 类横板城镇/地下城的像素密度、清晰轮廓、明暗分层和可走位地面阅读性，但不能复制任何原作建筑、NPC、地图块、UI 或文字。

## 2. 场景视角与比例

正式 SideCombat 不是纯平台跳跃，而是带一点俯视纵深的横板房间。参考图里的重点是“侧向房间 + 斜向地面砖/路径 + 人物脚底站位清楚”，不是照抄城镇美术。

- 镜头：横向 2D，略微俯视地面，类似可在前后小幅移动的动作房间。
- 地面：必须有可读的地面平面和透视线索，但不要画成明显棋盘格。
- 前景：前景草叶、树根和碎石可以压暗边缘，但不得遮住角色主要战斗区。
- 中景：树根、岩石、发光植物提供黑林识别度。
- 远景：树影、魔雾和天空建立层次，不抢角色轮廓。

## 3. 本批资产清单

### 3.1 黑林兽道分层背景

| 资源 ID | 正式输出文件 | 规格 | 背景 | 说明 |
| --- | --- | --- | --- | --- |
| SIDE_BG_BLACK_FOREST_SKY | `WheatearEditor/assets/vertical_slice/side_combat/backgrounds/black_forest_sky_4k.png` | 3840x2160 | 不透明 | 远景天空、树冠剪影、魔雾 |
| SIDE_BG_BLACK_FOREST_MID | `WheatearEditor/assets/vertical_slice/side_combat/backgrounds/black_forest_mid_4k.png` | 3840x2160 | 透明或不透明 | 中景树林、岩石、发光植物 |
| SIDE_BG_BLACK_FOREST_FLOOR | `WheatearEditor/assets/vertical_slice/side_combat/backgrounds/black_forest_floor_4k.png` | 3840x2160 | 不透明 | 可战斗地面、纵深透视线、根系 |
| SIDE_BG_BLACK_FOREST_FRONT | `WheatearEditor/assets/vertical_slice/side_combat/backgrounds/black_forest_front_4k.png` | 3840x2160 | 透明 PNG | 前景草叶、树根、碎枝 |
| SIDE_BG_BLACK_FOREST_LIGHTS | `WheatearEditor/assets/vertical_slice/side_combat/backgrounds/black_forest_magic_lights_4k.png` | 3840x2160 | 透明 PNG | 可叠加魔光、粒子和辉光层 |
| SIDE_BG_BLACK_FOREST_PREVIEW | `WheatearEditor/assets/vertical_slice/side_combat/backgrounds/bg_black_forest_stage_hd.png` | 3840x2160 | 不透明 | 合成预览版，用于未接分层前替换旧背景 |

### 3.2 战斗房间装饰与障碍物

| 资源 ID | 正式输出文件 | 规格 | 说明 |
| --- | --- | --- | --- |
| PROP_ROOT_ARCH | `WheatearEditor/assets/vertical_slice/side_combat/props/root_arch.png` | 768x576，透明 PNG | 树根拱形装饰，可做中景或前景 |
| PROP_MAGIC_CRYSTAL | `WheatearEditor/assets/vertical_slice/side_combat/props/magic_crystal_cluster.png` | 512x512，透明 PNG | 发光水晶簇 |
| PROP_BROKEN_LOG | `WheatearEditor/assets/vertical_slice/side_combat/props/broken_log.png` | 768x384，透明 PNG | 倒木、可遮挡边缘 |
| PROP_STONE_PILLAR | `WheatearEditor/assets/vertical_slice/side_combat/props/stone_pillar.png` | 512x768，透明 PNG | 石柱/前景遮挡候选 |
| PROP_FOREST_CRATE | `WheatearEditor/assets/vertical_slice/side_combat/props/forest_supply_crate.png` | 384x384，透明 PNG | 可破坏补给箱候选 |
| PROP_RUNE_MARKER | `WheatearEditor/assets/vertical_slice/side_combat/props/rune_ground_marker.png` | 512x256，透明 PNG | 地面魔纹提示，可做教程标记 |

### 3.3 可破坏物 strip

| 资源 ID | 正式输出文件 | Cell | 帧数 | FPS | 输出尺寸 | 说明 |
| --- | --- | --- | ---: | ---: | --- | --- |
| BREAK_CRATE | `WheatearEditor/assets/vertical_slice/side_combat/props/forest_supply_crate_break_strip.png` | 384x384 | 8 | 18 | 3072x384 | 补给箱破碎，碎片完整在格内 |
| BREAK_CRYSTAL | `WheatearEditor/assets/vertical_slice/side_combat/props/magic_crystal_break_strip.png` | 512x512 | 10 | 20 | 5120x512 | 水晶破碎，发光碎片不串格 |

### 3.4 世界内掉落物 strip

| 掉落 | 正式输出文件 | Cell | 帧数 | FPS | 输出尺寸 | 说明 |
| --- | --- | --- | ---: | ---: | --- | --- |
| 魔核碎片 | `WheatearEditor/assets/vertical_slice/side_combat/pickups/pickup_magic_core_strip.png` | 192x192 | 8 | 12 | 1536x192 | 悬浮闪光循环 |
| 兽筋 | `WheatearEditor/assets/vertical_slice/side_combat/pickups/pickup_beast_sinew_strip.png` | 192x192 | 6 | 10 | 1152x192 | 轻微浮动 |
| 熊爪 | `WheatearEditor/assets/vertical_slice/side_combat/pickups/pickup_bear_claw_strip.png` | 192x192 | 6 | 10 | 1152x192 | 轮廓锐利但不血腥 |
| 金币/通用货币 | `WheatearEditor/assets/vertical_slice/side_combat/pickups/pickup_coin_strip.png` | 192x192 | 8 | 12 | 1536x192 | 旋转闪光 |
| 装备箱 | `WheatearEditor/assets/vertical_slice/side_combat/pickups/pickup_equipment_box_strip.png` | 256x256 | 8 | 12 | 2048x256 | 稀有掉落发光 |

### 3.5 战斗通用遮罩与反馈素材

| 资源 ID | 正式输出文件 | 规格 | 背景 | 说明 |
| --- | --- | --- | --- | --- |
| SHADOW_BLOB_SOFT | `WheatearEditor/assets/vertical_slice/side_combat/ui/blob_shadow_soft_hd.png` | 512x256 | 透明 PNG | 角色/敌人地面阴影，不带文字 |
| LANE_GUIDE_SUBTLE | `WheatearEditor/assets/vertical_slice/side_combat/ui/lane_guide_subtle.png` | 2560x320 | 透明 PNG | 可选纵深提示层 |
| HIT_SCREEN_FLASH | `WheatearEditor/assets/vertical_slice/side_combat/ui/hit_screen_flash.png` | 2560x1440 | 透明 PNG | 命中白闪叠加 |
| BREAK_LIMIT_OVERLAY | `WheatearEditor/assets/vertical_slice/side_combat/ui/break_limit_overlay.png` | 2560x1440 | 透明 PNG | 断限瞬间画面叠加 |
| BOSS_PHASE_VIGNETTE | `WheatearEditor/assets/vertical_slice/side_combat/ui/boss_phase_vignette.png` | 2560x1440 | 透明 PNG | Boss 阶段转换暗角 |

## 4. 通用负面要求

所有提示词追加：

```text
no text, no letters, no numbers, no logo, no watermark, no signature, no UI letters, no recognizable franchise design, no characters unless the asset is a pickup or prop, no readable signage, no screenshot composition, no blurry upscaled texture, no low-resolution source, no cropped prop, no visible cell borders, no frame numbers, no white background, no gray background, no checkerboard background, no opaque background rectangle, no white matte, no dirty alpha edge, no dirty transparent edge, no copied DNF map or prop
```

## 5. 背景 Prompt

### 5.1 分层背景通用模板

将 `[LAYER REQUEST]` 替换为具体层内容。

```text
Use case: stylized-concept
Asset type: 2D side-scrolling action game battle stage layer
Primary request: original high-resolution pixel-art fantasy black forest side-combat stage layer, [LAYER REQUEST]
Scene/backdrop: bright stylish magical black forest path, slight top-down side-scrolling room perspective, clear playable floor readability, layered parallax-friendly composition
Style/medium: polished high-resolution pixel-art action game background, DNF-like side-scrolling readability and pixel density without copying any map, crisp pixel clusters, clean shape language, not photo texture
Composition/framing: 3840x2160 16:9 canvas, horizontally readable, enough negative space for player, enemies, HUD and VFX, no characters
Lighting/mood: dark forest with vibrant readable action-game lighting, cyan magical plants and warm rim highlights, not muddy or horror-dark
Color palette: deep greens, blue-black shadows, cyan magic glow, restrained warm amber highlights
Resolution rule: draw as native 3840x2160 or higher source quality, no blurry upscale, no smeared texture
Constraints: no text, no logo, no watermark, no recognizable franchise design, no characters, no readable signage; transparent layers must be RGBA PNG with unused pixels alpha 0 and no white matte
Avoid: flat platformer tiles, over-dark unreadable scene, photorealism, clutter that hides combat silhouettes, blurry painted rendering
```

层内容替换：

```text
black_forest_sky_4k.png: distant sky and treetop silhouette layer, misty forest depth, no foreground floor
black_forest_mid_4k.png: middle forest layer with twisted trees, rocks, glowing cyan plants, transparent-friendly edges if possible
black_forest_floor_4k.png: playable ground layer, dirt path, roots, stones, subtle perspective guide lines for front-back movement, clear combat plane
black_forest_front_4k.png: transparent foreground foliage and root silhouettes, framing edges without covering central combat space
black_forest_magic_lights_4k.png: transparent overlay of cyan magical particles, small glow clusters and soft beams, no opaque background
bg_black_forest_stage_hd.png: composed final preview of all layers, full 3840x2160 background, no characters
```

## 6. 物件 Prompt

```text
Use case: stylized-concept
Asset type: 2D high-resolution pixel-art action game stage prop
Primary request: original fantasy black forest pixel-art prop, [PROP DESCRIPTION]
Style/medium: polished high-resolution pixel-art game prop, crisp pixel clusters, readable silhouette, consistent with bright black forest SideCombat stage
Composition/framing: RGBA transparent PNG, unused pixels alpha 0, [CANVAS SIZE], prop centered with 48px or more transparent padding, no crop, no background scene
Lighting/mood: cyan magical forest rim light with muted warm highlights, readable over dark forest ground
Constraints: transparent background, no white matte, no gray matte, no checkerboard background, no text, no logo, no watermark, no copied franchise object
Avoid: photoreal texture, dirty alpha edge, cropped object, overly tiny detail, blurry painted rendering
```

物件替换：

```text
root_arch.png: twisted tree-root arch, decorative black forest silhouette, not blocking gameplay if placed behind characters
magic_crystal_cluster.png: cyan glowing crystal cluster growing from mossy stones
broken_log.png: broken fallen log with moss and small cyan magic spores
stone_pillar.png: old cracked stone pillar with faint rune-like shapes but no readable letters
forest_supply_crate.png: simple fantasy supply crate made of wood and cloth straps, no text labels
rune_ground_marker.png: subtle circular ground marker made of abstract magic lines, no letters or symbols that look like text
```

## 7. 动画源帧 Prompt

### 7.1 可破坏物

```text
Use case: stylized-concept
Asset type: single animation frame for a 2D action game destructible prop
Primary request: original high-resolution pixel-art fantasy destructible prop animation single frame, [BREAK REQUEST], frame [FRAME INDEX] of [FRAME COUNT], [POSE FOR THIS FRAME]
Style/medium: polished pixel-art game prop animation, crisp fragments, RGBA transparent background with unused pixels alpha 0
Composition/framing: fixed [CELL WIDTH]x[CELL HEIGHT] RGBA transparent canvas, one frame only, prop centered on invisible pivot at pixel ([PIVOT_X], [PIVOT_Y]), no visible grid lines, do not draw pivot guides
Frame integrity: all fragments, glow edges and dust must remain inside this canvas with at least 32px transparent padding; object scale stays consistent with the frame plan; no cropped shards
Constraints: RGBA transparent PNG, unused pixels alpha 0, no text, no UI, no background, no logo, no watermark, no white matte
Avoid: cropped shards, inconsistent cell size, excessive smoke hiding the break, gore, dirty alpha edge, blurry painted rendering, horizontal sprite sheet, multiple frames in one image
```

替换文本：

```text
forest_supply_crate_break_strip.png: wooden fantasy supply crate breaking into contained fragments, no item icons inside
magic_crystal_break_strip.png: cyan magic crystal cluster shattering into contained glowing shards, no large explosion outside cell
```

### 7.2 掉落物

```text
Use case: stylized-concept
Asset type: single animation frame for a 2D action RPG pickup item
Primary request: original high-resolution pixel-art fantasy pickup item animation single frame, [PICKUP REQUEST], frame [FRAME INDEX] of [FRAME COUNT], [POSE FOR THIS FRAME]
Style/medium: polished pixel-art game pickup sprite, clean silhouette, readable at small size, RGBA transparent background with unused pixels alpha 0
Composition/framing: fixed [CELL WIDTH]x[CELL HEIGHT] RGBA transparent canvas, one frame only, item centered on invisible pivot at pixel ([PIVOT_X], [PIVOT_Y]), no visible grid lines, do not draw pivot guides
Frame integrity: item, glow, sparkle and motion arc must remain inside this canvas with at least 24px transparent padding; same item scale as the frame plan; no cropped glow
Animation requirements: subtle floating, pulsing or rotating loop, not a full explosion
Constraints: RGBA transparent PNG, unused pixels alpha 0, no text, no UI, no background, no logo, no watermark, no white matte
Avoid: cropped glow, inconsistent item size, unreadable tiny detail, commercial icon design, dirty alpha edge, blurry painted rendering, horizontal sprite sheet, multiple frames in one image
```

替换文本：

```text
pickup_magic_core_strip.png: small blue magic core crystal fragment, cyan glow pulse, useful as magic sword material
pickup_beast_sinew_strip.png: bundled beast sinew material, stylized dark red-violet fibers, subtle magic shine, non-gory
pickup_bear_claw_strip.png: dark bear claw material, clean stylized silhouette, cyan rim glow, non-gory
pickup_coin_strip.png: small fantasy coin or generic gold shard, rotating sparkle, no currency symbol or text
pickup_equipment_box_strip.png: small fantasy equipment chest or glowing item box, rare pickup glow, no text label
```

## 8. overlay_params.yaml 示例

```yaml
backgrounds:
  black_forest_sky_4k: { width: 3840, height: 2160, parallax: 0.25, opaque: true }
  black_forest_mid_4k: { width: 3840, height: 2160, parallax: 0.55, opaque: false }
  black_forest_floor_4k: { width: 3840, height: 2160, parallax: 1.0, opaque: true }
  black_forest_front_4k: { width: 3840, height: 2160, parallax: 1.15, opaque: false }
  black_forest_magic_lights_4k: { width: 3840, height: 2160, blend: additive_like, opaque: false }

transparentPngRule:
  requireRgba: true
  unusedPixelsAlpha: 0
  rejectWhiteMatte: true
  rejectOpaqueBackground: true
  rejectDirtyAlphaEdge: true

sourceFrameRule:
  productionMode: source_frames_first
  generateAsIndividualFrames: true
  assembleStripAfterValidation: true
  sourceFramePattern: source_frames/{clip}/{clip}_{frame:03}.png
  framePlanRequired: true
  requirePivotMetadata: true
  frameOffsetMode: fixed_canvas_no_per_frame_offset
  trimmedAtlasRequiresOffsetFromPivot: true

breakables:
  forest_supply_crate_break_strip: { cellWidth: 384, cellHeight: 384, frameCount: 8, frameRate: 18, pivot: center }
  magic_crystal_break_strip: { cellWidth: 512, cellHeight: 512, frameCount: 10, frameRate: 20, pivot: center }

pickups:
  pickup_magic_core_strip: { cellWidth: 192, cellHeight: 192, frameCount: 8, frameRate: 12, pivot: center }
  pickup_beast_sinew_strip: { cellWidth: 192, cellHeight: 192, frameCount: 6, frameRate: 10, pivot: center }
  pickup_bear_claw_strip: { cellWidth: 192, cellHeight: 192, frameCount: 6, frameRate: 10, pivot: center }
  pickup_coin_strip: { cellWidth: 192, cellHeight: 192, frameCount: 8, frameRate: 12, pivot: center }
  pickup_equipment_box_strip: { cellWidth: 256, cellHeight: 256, frameCount: 8, frameRate: 12, pivot: center }

overlays:
  hit_screen_flash: { width: 2560, height: 1440, blend: additive_like }
  break_limit_overlay: { width: 2560, height: 1440, blend: additive_like }
  boss_phase_vignette: { width: 2560, height: 1440, blend: alpha }
```

`frame_plan.yaml` 最小示例：

```yaml
pickup_magic_core:
  cellWidth: 192
  cellHeight: 192
  frameCount: 8
  pivot: center
  pivotX: 96
  pivotY: 96
  renderOffset: [-96, -96]
  frames:
    0: "core centered, smallest cyan glow"
    1: "glow slightly brighter, core floats 2px upward"
    2: "sparkle appears near top-right but stays inside padding"
    3: "peak glow, no cropped halo"
    4: "glow begins dimming, core returns toward center"
    5: "small sparkle on lower-left, still inside canvas"
    6: "glow nearly normal, same item scale"
    7: "returns cleanly to frame 0"
```

生成时先写每个 pickup 或破碎动画的 `frame_plan.yaml`，再逐帧生成。世界内动画源帧可以很小，但仍必须真透明、居中、无裁切。

## 9. 推荐交付结构

```text
Batch05_SideCombat_Stage_Props/
  backgrounds/
    black_forest_sky_4k.png
    black_forest_mid_4k.png
    black_forest_floor_4k.png
    black_forest_front_4k.png
    black_forest_magic_lights_4k.png
    bg_black_forest_stage_hd.png
  props/
    root_arch.png
    magic_crystal_cluster.png
    broken_log.png
    stone_pillar.png
    forest_supply_crate.png
    rune_ground_marker.png
    forest_supply_crate_break_strip.png
    magic_crystal_break_strip.png
  source_frames/
    forest_supply_crate_break/forest_supply_crate_break_000.png ... forest_supply_crate_break_007.png
    magic_crystal_break/magic_crystal_break_000.png ... magic_crystal_break_009.png
    pickup_magic_core/pickup_magic_core_000.png ... pickup_magic_core_007.png
    pickup_beast_sinew/pickup_beast_sinew_000.png ... pickup_beast_sinew_005.png
    pickup_bear_claw/pickup_bear_claw_000.png ... pickup_bear_claw_005.png
    pickup_coin/pickup_coin_000.png ... pickup_coin_007.png
    pickup_equipment_box/pickup_equipment_box_000.png ... pickup_equipment_box_007.png
  pickups/
    pickup_magic_core_strip.png
    pickup_beast_sinew_strip.png
    pickup_bear_claw_strip.png
    pickup_coin_strip.png
    pickup_equipment_box_strip.png
  ui/
    blob_shadow_soft_hd.png
    lane_guide_subtle.png
    hit_screen_flash.png
    break_limit_overlay.png
    boss_phase_vignette.png
  stage_params.yaml
```

## 10. 接入动作

1. 背景先用 `bg_black_forest_stage_hd.png` 替换当前 `bg_black_forest_stage.png`，确认整体观感。
2. 分层接入时，把 sky/mid/floor/front/lights 拆到不同 Entity 或背景层系统，按 parallax 参数移动。
3. 物件 PNG 放入 `side_combat/props/`，场景中按装饰用途摆放，不参与碰撞前先只做视觉层。
4. 可破坏物和 pickup 动画先放入 `source_frames/` 逐帧验收，通过后拼接 strip；pickup strip 后续接入掉落系统，用 `sheet_params` 切 clip；UI 背包图标仍等 Batch07 atlas。
5. 重新打包 Sandbox，检查 `content.wtpack` 包含背景、props、pickups 和 overlay 素材。

## 11. 验收标准

- 3840x2160 背景在 2560x1440 显示时不糊、不像低分辨率放大。
- 地面平面清楚，角色、Boss 和小怪放上去后不会被背景吞掉。
- 透明前景和魔光层边缘干净，无黑边或白边。
- 所有透明素材放到黑、白、亮粉、透明棋盘背景上检查，不能出现白底、灰底、棋盘格底、白色 matte、半透明残底或脏 alpha。
- 可破坏物和掉落物源帧每一帧都完整，不裁切、不忽大忽小；拼成 strip 后逐格切开必须与源帧一致。
- 物件风格与 Batch04 敌人/Boss 和 Batch03 主角能放在同一画面里。
- 不出现文字、Logo、商业可识别元素、人物或不必要的 UI。
