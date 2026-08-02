# Batch07 SideCombat 战斗 HUD 与图标 Atlas 生产包

更新时间：2026-07-31

本包用于替换正式横板战斗 HUD、技能图标、道具图标、掉落图标和 Boss 条等 UI 资源。目标是让战斗界面接近 DNF 类横板动作的高清清晰 UI：明亮爽快、图标语义清楚、带轻微像素质感但不低清，在 1080p 与 2K 显示下都不糊，同时保持当前工程的战斗信息结构。

## 1. 本批硬性规则

- 图标 atlas 必须固定 cell，所有图标在各自 cell 内保持同一视觉尺寸和同一透视角度。
- UI 资产不画任何文字、字母、数字、键位或中文，技能名、冷却数值、数量和说明由引擎文字渲染。
- 图标在 `128x128` 显示时可读，在 `64x64` 仍能看出语义；生成母版使用 `256x256` cell，接入时可缩放到 128 或 64。
- 面板类素材必须使用高分辨率母版，不靠 1080p 小图硬拉到 2K。
- 需要拉伸的面板必须九宫格友好：边框、角饰和高光放在边缘，中心区域干净。
- 承载中文、数值或 tooltip 的 HUD 面板采用“PNG 框架层 + 引擎绘制半透明文字底”的工程流程；PNG 不烘焙大面积文字底，中心保持透明或近透明，运行时用 fill 控制可读性。
- 所有透明 HUD、图标和面板必须是 **RGBA PNG 真透明**，无用区域 alpha 为 0。
- 透明边缘必须干净，无黑边、白边、灰边、半透明脏像素、白色 matte、棋盘格底、Logo、水印或参考游戏图案。
- 风格只参考 DNF 类横板动作 UI 的像素密度、边框重量、技能槽阅读性和高反馈状态，不复制任何原作图标、技能栏、血球、文字或界面布局。
- HUD 和可交互面板不是“全像素 UI”。允许轻微像素边、点阵纹理和硬朗色块，但必须保持高清、干净、现代，不能像低分辨率农场/复古像素游戏 UI。
- 本批是规则 UI atlas/面板批次，可以直接生成整张 atlas 或面板；不按角色/VFX 动画的单帧源文件流程。前提是 atlas cell 规则、图标居中、透明边缘和视觉尺寸全部稳定。

## 2. 2K 显示与 UI 缩放规则

- 图标母版：`256x256` cell。运行时显示到 128 或 64 时会更干净。
- 小按钮/技能槽底板：至少按 2x 尺寸输出，例如 128 显示用 256 母版。
- Boss 血条、技能栏底板、连击牌等大面板：按 2560x1440 UI 设计输出，避免 2K 屏幕放大失真。
- 九宫格素材只拉伸中心和边线，不拉伸角饰；如果一个面板要铺很宽，不要用一张带复杂纹样的小图整体缩放。
- 文字、数值和 tooltip 下方的阅读底由引擎绘制：推荐深色玻璃 fill alpha 70%-85%，面板 PNG 中心 alpha 0%-15%，便于根据战斗背景亮度调整。

## 3. 本批资产清单

### 3.1 图标 atlas

| Atlas | 正式输出文件 | 规格 | 内容 |
| --- | --- | --- | --- |
| battle_skill_icons | `WheatearEditor/assets/vertical_slice/ui/atlases/battle_skill_icons_256.png` | 8x4，cell 256，总 2048x1024 | 主动技能、支援、断限、锁定灰图 |
| battle_item_icons | `WheatearEditor/assets/vertical_slice/ui/atlases/battle_item_icons_256.png` | 4x2，cell 256，总 1024x512 | 治疗药、专注瓶、爆裂弹、空槽等 |
| battle_drop_icons | `WheatearEditor/assets/vertical_slice/ui/atlases/battle_drop_icons_256.png` | 6x2，cell 256，总 1536x512 | 魔核、兽筋、熊爪、金币、装备箱、稀有掉落 |
| battle_status_icons | `WheatearEditor/assets/vertical_slice/ui/atlases/battle_status_icons_256.png` | 8x2，cell 256，总 2048x512 | 攻击、防御、破防、浮空、冷却、支援、危险提示 |

### 3.2 HUD 面板与控件

| 资源 ID | 正式输出文件 | 规格 | 背景 | 说明 |
| --- | --- | --- | --- | --- |
| HUD_PLAYER_STATUS | `WheatearEditor/assets/vertical_slice/ui/atlases/battle_player_status_panel.png` | 1024x256 | 透明 PNG | 玩家 HP/MP/魔剑状态底板，九宫格友好 |
| HUD_BOSS_BAR | `WheatearEditor/assets/vertical_slice/ui/atlases/battle_boss_bar_panel.png` | 2048x256 | 透明 PNG | Boss 血条和破防条底板 |
| HUD_SKILL_BAR | `WheatearEditor/assets/vertical_slice/ui/atlases/battle_skill_bar_panel.png` | 2048x384 | 透明 PNG | 底部技能栏底板 |
| HUD_SKILL_SLOT | `WheatearEditor/assets/vertical_slice/ui/atlases/battle_skill_slot_states.png` | 1024x256 | 透明 PNG | 4 状态技能槽，每格 256x256 横排 |
| HUD_ITEM_SLOT | `WheatearEditor/assets/vertical_slice/ui/atlases/battle_item_slot_states.png` | 1024x256 | 透明 PNG | 4 状态道具槽，每格 256x256 横排 |
| HUD_COMBO_BADGE | `WheatearEditor/assets/vertical_slice/ui/atlases/battle_combo_badge.png` | 768x256 | 透明 PNG | 连击牌底板，不含数字 |
| HUD_BREAK_LIMIT_GAUGE | `WheatearEditor/assets/vertical_slice/ui/atlases/battle_break_limit_gauge.png` | 1024x192 | 透明 PNG | 断限槽底板/边框 |
| HUD_TOOLTIP_PANEL | `WheatearEditor/assets/vertical_slice/ui/atlases/battle_tooltip_panel.png` | 768x384 | 透明 PNG | 技能/掉落 tooltip 底板，九宫格 |

## 4. battle_skill_icons_256.png 切片说明

`8 columns x 4 rows`，每格 `256x256`：

| 列 | 图标语义 | 行 0 normal | 行 1 hover/active | 行 2 disabled/locked | 行 3 cooldown/desaturated |
| ---: | --- | --- | --- | --- | --- |
| 0 | basic slash / 三段斩 | 可用 | 高亮 | 锁定 | 冷却 |
| 1 | launcher / 裂空挑斩 | 可用 | 高亮 | 锁定 | 冷却 |
| 2 | air slash / 空中追斩 | 可用 | 高亮 | 锁定 | 冷却 |
| 3 | magic bolt / 魔法弹 | 可用 | 高亮 | 锁定 | 冷却 |
| 4 | ally support / 支援 | 可用 | 高亮 | 锁定 | 冷却 |
| 5 | break limit / 断限 | 可用 | 高亮 | 锁定 | 冷却 |
| 6 | dodge / 闪避类提示 | 可用 | 高亮 | 锁定 | 冷却 |
| 7 | guard / 防御类提示 | 可用 | 高亮 | 锁定 | 冷却 |

图标只表现语义，不表现键位。比如裂空挑斩可以是向上剑气和小型上升箭头形构图，但不能写 `S+J`。

## 5. 通用负面要求

所有提示词追加：

```text
no text, no letters, no numbers, no key labels, no logo, no watermark, no signature, no screenshot composition, no character portrait, no background scene, no visible cell borders, no grid lines, no baked cooldown number, no copied franchise icon, no copied DNF UI, no white background, no gray background, no checkerboard background, no opaque background rectangle, no white matte, no dirty alpha edge, no blurry upscaled texture, no inconsistent icon scale, no low-resolution chunky pixel UI, no Stardew Valley style, no overly cute farm-game UI
```

## 6. 图标 atlas Prompt

```text
Use case: stylized-concept
Asset type: high-resolution fantasy action RPG UI icon atlas with subtle pixel accents
Primary request: original SideCombat high-resolution icon atlas, [ICON SET REQUEST]
Style/medium: bright polished anime action game UI icons with subtle pixel-art accents, sharp readable silhouettes, crisp hard edges, cyan magic sword theme with warm gold accents, transparent PNG
Composition/framing: exact atlas layout [COLUMNS] columns x [ROWS] rows, each cell 256x256, total canvas [TOTAL WIDTH]x[TOTAL HEIGHT], each icon centered in its own cell with consistent 28px safe padding, no visible grid lines
Cell consistency: all icons use the same visual scale, same perspective and same lighting; no icon touches cell edge; no icon crosses into neighboring cells
Resolution rule: native 256x256 icon details, readable when downscaled to 128x128 and 64x64, no blurry upscale
Transparency rule: RGBA transparent PNG, unused pixels alpha 0, no white matte, no opaque background rectangle, clean alpha edge
Constraints: transparent background, no text, no letters, no numbers, no key labels, no logo, no watermark, no character portraits
Avoid: random decorative symbols, over-detailed icons that become unreadable, copied commercial icon style, blurry painted rendering, dirty alpha edge, low-resolution chunky pixel UI, Stardew Valley style, overly cute farm-game UI
```

图标集替换：

```text
battle_skill_icons_256.png: magic sword combat skill icons: slash combo, upward launcher, aerial slash, cyan magic bolt, ally support, break limit, dodge, guard; four state rows normal, hover active, locked disabled, cooldown desaturated
battle_item_icons_256.png: combat item icons: heal potion, focus vial, burst bomb, empty slot, antidote placeholder, defense charm, magic shard, locked item slot; two rows normal and disabled
battle_drop_icons_256.png: material and loot icons: magic core crystal, beast sinew bundle, bear claw, coin shard, equipment box, rare magic fragment; two rows normal and highlighted
battle_status_icons_256.png: status icons: attack up, defense up, broken guard, airborne, cooldown, support ready, danger warning, low health; two rows normal and active
```

## 7. HUD 面板 Prompt

```text
Use case: stylized-concept
Asset type: high-resolution fantasy action RPG HUD panel asset with subtle pixel accents
Primary request: original SideCombat high-resolution HUD panel, [HUD REQUEST]
Style/medium: bright polished anime action game UI with subtle pixel-art accents, dark translucent glass base, black-silver magic sword trim, cyan glow accents, small warm gold highlights, transparent PNG
Composition/framing: [CANVAS SIZE] RGBA transparent canvas, unused pixels alpha 0, panel centered with generous padding, nine-slice friendly where applicable, clean transparent or near-transparent center areas for engine-rendered Chinese text, numeric values and engine-drawn readable backdrop
Resolution rule: designed for 2560x1440 UI use, crisp native edges, no blurry upscaling
Transparency rule: no white matte, no gray matte, no checkerboard background, no opaque background rectangle, clean alpha edge
Constraints: transparent background, no text, no letters, no numbers, no logo, no watermark, no character art, no screenshot
Avoid: page-game clutter, red notification dots, ornate decorations that block readability, baked large dark rectangle in the center, flat gray rectangle, copied DNF UI, dirty alpha edge, low-resolution chunky pixel UI, Stardew Valley style, overly cute farm-game UI
```

HUD 替换：

```text
battle_player_status_panel.png: top-left or lower-left player HP MP and magic sword status frame, readable dark glass, no numbers
battle_boss_bar_panel.png: wide boss HP and break guard frame, dramatic but clean, no boss name text
battle_skill_bar_panel.png: bottom skill bar base with sockets for skill slots, no key labels
battle_skill_slot_states.png: four horizontal 256x256 slot frames: normal, hover, pressed, disabled, no icon inside
battle_item_slot_states.png: four horizontal 256x256 item slot frames: normal, hover, pressed, disabled, no item inside
battle_combo_badge.png: compact combo badge frame, energetic cyan/gold trim, no combo number text
battle_break_limit_gauge.png: horizontal break limit gauge frame, magic sword energy edge, empty center for fill bar
battle_tooltip_panel.png: tooltip panel background, dark glass, clean readable center, nine-slice friendly
```

## 8. ui_atlas_params.yaml 示例

```yaml
productionMode: direct_ui_atlas_or_panel

transparentPngRule:
  requireRgba: true
  unusedPixelsAlpha: 0
  rejectWhiteMatte: true
  rejectOpaqueBackground: true

engineTextBackdrop:
  enabledForTextAndNumberPanels: true
  source: engine_drawn_fill
  changesPngAlphaAtRuntime: false
  color: "#10131D"
  alpha: 0.70-0.85
  pngCenterAlpha: 0.0-0.15
  drawOrder: [battle_scene_or_hud_parent, engine_text_backdrop_fill, hud_panel_frame_png, engine_text_or_numbers]
  primitive: ui_panel_or_ui_image_quad
  rectSource: textRectInset_or_widget_inner_rect
  runtimeAdjustable: true

iconAtlases:
  battle_skill_icons_256:
    width: 2048
    height: 1024
    cellWidth: 256
    cellHeight: 256
    columns: 8
    rows: 4
    rowOrder: [normal, hover_active, locked_disabled, cooldown]
    columnOrder: [basic_slash, launcher, air_slash, magic_bolt, ally_support, break_limit, dodge, guard]
  battle_item_icons_256:
    width: 1024
    height: 512
    cellWidth: 256
    cellHeight: 256
    columns: 4
    rows: 2
  battle_drop_icons_256:
    width: 1536
    height: 512
    cellWidth: 256
    cellHeight: 256
    columns: 6
    rows: 2
  battle_status_icons_256:
    width: 2048
    height: 512
    cellWidth: 256
    cellHeight: 256
    columns: 8
    rows: 2

nineSlicePanels:
  battle_player_status_panel: { width: 1024, height: 256, suggestedBorder: [64, 64, 48, 48], textBackdropSource: engine_drawn_fill }
  battle_boss_bar_panel: { width: 2048, height: 256, suggestedBorder: [80, 80, 48, 48], textBackdropSource: engine_drawn_fill }
  battle_skill_bar_panel: { width: 2048, height: 384, suggestedBorder: [72, 72, 64, 64], textBackdropSource: engine_drawn_fill }
  battle_tooltip_panel: { width: 768, height: 384, suggestedBorder: [56, 56, 56, 56], textBackdropSource: engine_drawn_fill, textRectInset: [72, 64, 72, 64] }
```

## 9. 推荐交付结构

```text
Batch07_SideCombat_HUD_Icons/
  battle_skill_icons_256.png
  battle_item_icons_256.png
  battle_drop_icons_256.png
  battle_status_icons_256.png
  battle_player_status_panel.png
  battle_boss_bar_panel.png
  battle_skill_bar_panel.png
  battle_skill_slot_states.png
  battle_item_slot_states.png
  battle_combo_badge.png
  battle_break_limit_gauge.png
  battle_tooltip_panel.png
  ui_atlas_params.yaml
```

## 10. 接入动作

1. 把 PNG 放入 `WheatearEditor/assets/vertical_slice/ui/atlases/`。
2. 图标 atlas 按 `256x256` cell 建立语义映射，运行时可显示为 128 或 64。
3. 将当前 `side_combat/ui/icon_skill_*.png`、`items/icon_item_*.png`、`icon_drop_*.png` 逐步迁移到 atlas cell。
4. 面板按九宫格或等效方式接入，避免整体拉伸导致边框和角饰变形；承载文字/数值的区域按 `engineTextBackdrop` 增加一层可调 `UIPanelComponent` 或无贴图 `UIImageComponent`，绘制半透明底，不改 PNG 文件的透明通道。
5. 重新打包 Sandbox，检查 `content.wtpack` 包含本批 HUD 与图标 atlas。

## 11. 验收标准

- atlas 尺寸精确，cell 数量、行列顺序和文档一致。
- 每个图标在 cell 中大小一致、居中一致、边缘留白一致。
- UI atlas 是允许直接整图生成的例外；如果任一 cell 图标跨格、贴边、比例漂移、带残底或状态行不对齐，整张 atlas 不合格。
- 图标不含文字、键位、数字、Logo 或商业可识别元素。
- downscale 到 128x128 和 64x64 后语义仍清楚。
- 面板放大到 2K UI 尺寸后不糊、不拉伸角饰、不污染中文文字区域；PNG 中心默认透明或近透明，运行时文字/数值底由引擎 fill 提供。
- 透明 PNG 必须是真 RGBA，空白 alpha 为 0；放到黑、白、亮粉、透明棋盘背景上检查，不能出现黑边、白边、灰底、白色 matte、棋盘格底和脏 alpha。
- HUD 和交互面板只能有轻微像素质感，必须保持高清清晰；不接受低分辨率粗颗粒、复古农场感或把文字区域做糊的 UI。
