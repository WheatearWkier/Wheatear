# Batch08 系统 UI 与据点界面 Atlas 生产包

更新时间：2026-07-31

本包用于替换据点、技能树、装备、关系、支援、结算、存读档和设置页面的系统 UI 皮肤。布局沿用当前 demo 的功能结构，但底板、按钮、图标、节点、槽位和徽章统一成正式高清二次元幻想 RPG UI，并带轻微像素质感。目标是让系统页不再像临时调试面板，同时在 2K 屏幕下保持清晰。

## 1. 本批硬性规则

- 所有需要透明的 UI 素材必须是 **RGBA PNG 真透明**，无用区域 alpha 必须为 0。
- 禁止白底、浅灰底、棋盘格底、黑底、白色 matte、半透明白边或任何需要后期抠图的残底。
- UI 上不绘制任何文字、字母、数字、键位、Logo 或水印。标题、按钮文案、数值、tooltip 全部由引擎字体渲染。
- Atlas 必须固定 cell；同一 atlas 内图标或节点视觉尺寸一致，不能有的撑满、有的很小。
- 面板、按钮、tooltip 必须九宫格友好，边框和角饰不能靠整图拉伸。
- 承载中文、数值、tooltip、存档信息或装备详情的系统面板采用“PNG 框架层 + 引擎绘制半透明文字底”的工程流程；PNG 不烘焙大面积文字底，中心保持透明或近透明，运行时用 fill 控制可读性。
- 2K 屏幕优先：面板和图标按高分辨率母版制作，运行时缩小显示，不用低分辨率图硬放大。
- 系统 UI 和战斗 HUD 使用同一套高清清晰 UI 语言：清晰硬边、稳定色块、强可读图标、轻微像素纹理和 restrained 装饰；不要复刻 DNF 或任何商业游戏 UI。
- 据点、装备、技能树、存档、设置这些需要长时间阅读和交互的页面不能做成低分辨率粗颗粒 UI，也不能像复古农场游戏 UI；它们要更接近 DNF 那种高清、硬朗、信息密度高但不糊的界面。
- 本批是规则 UI atlas/面板批次，可以直接生成整张 atlas 或面板；不按角色/VFX 动画的单帧源文件流程。前提是 atlas cell 规则、图标居中、透明边缘、状态行列和视觉尺寸全部稳定。

## 2. PNG 真透明要求

所有透明 PNG 必须满足：

```yaml
alphaRequirement:
  format: RGBA PNG
  unusedPixels: alpha_0
  noWhiteMatte: true
  noGrayMatte: true
  noCheckerboardBackground: true
  noOpaqueBackgroundRectangle: true
  cleanTransparentEdge: true
```

验收时用纯黑、纯白、亮粉和透明棋盘四种背景预览：任何白边、灰边、浅色方块、残留背景都不合格，直接重生。

## 3. 风格总纲

- 系统 UI：高清二次元幻想 RPG，深色玻璃、黑银魔剑纹理、青蓝魔法高光、少量温暖金色边线，并带轻微像素质感。
- 据点页：森林营地安全感，界面比战斗 HUD 更安静，但仍属于同一游戏。
- 技能树：魔剑核心、节点、连线、锁定状态清楚，可支持较大画布拖动。
- 装备页：槽位、背包、强化材料和详情面板要像 RPG 管理界面，耐看、可读、可重复操作。
- 结算页：首通、评分、掉落、经验和材料反馈要更有仪式感，但不要做成抽卡页面。
- 文字承载区的阅读底由引擎绘制：推荐深色玻璃 fill alpha 70%-85%，面板 PNG 中心 alpha 0%-15%，可以按场景亮度、弹窗层级和 hover 状态调整。

## 4. 本批资产清单

### 4.1 通用面板与按钮

| 资源 ID | 正式输出文件 | 规格 | 背景 | 说明 |
| --- | --- | --- | --- | --- |
| COMMON_PANELS | `WheatearEditor/assets/vertical_slice/ui/atlases/common_panels_4k.png` | 4096x4096 | 透明 PNG | 大面板、二级面板、tooltip、弹窗、标题条 |
| COMMON_BUTTONS | `WheatearEditor/assets/vertical_slice/ui/atlases/common_buttons_256.png` | 2048x1024 | 透明 PNG | 8x4，cell 256，普通/hover/pressed/disabled 按钮 |
| COMMON_TABS | `WheatearEditor/assets/vertical_slice/ui/atlases/common_tabs_256.png` | 2048x512 | 透明 PNG | 8x2，cell 256，页签和分段控件 |
| COMMON_SLOTS | `WheatearEditor/assets/vertical_slice/ui/atlases/common_slots_256.png` | 2048x1024 | 透明 PNG | 8x4，cell 256，装备槽/材料槽/存档槽 |

### 4.2 系统入口与功能图标

| Atlas | 正式输出文件 | 规格 | 内容 |
| --- | --- | --- | --- |
| system_icons | `WheatearEditor/assets/vertical_slice/ui/atlases/system_icons_256.png` | 8x8，cell 256，总 2048x2048 | 副本、技能树、装备、关系、支援、保存、设置、返回等 |
| material_icons | `WheatearEditor/assets/vertical_slice/ui/atlases/material_icons_256.png` | 8x4，cell 256，总 2048x1024 | 魔核、兽筋、熊爪、铁片、强化石等 |
| equipment_icons | `WheatearEditor/assets/vertical_slice/ui/atlases/equipment_icons_256.png` | 8x4，cell 256，总 2048x1024 | 武器、防具、饰品、护符、鞋、特殊装备 |
| relationship_icons | `WheatearEditor/assets/vertical_slice/ui/atlases/relationship_icons_256.png` | 8x2，cell 256，总 2048x512 | 青梅/导师、白魔、护卫、黑魔、好感阶段 |

### 4.3 技能树与节点

| 资源 ID | 正式输出文件 | 规格 | 背景 | 说明 |
| --- | --- | --- | --- | --- |
| SKILL_TREE_NODES | `WheatearEditor/assets/vertical_slice/ui/atlases/skill_tree_nodes_256.png` | 2048x1024 | 透明 PNG | 8x4，cell 256，普通/可学/已学/锁定/核心/选中 |
| SKILL_TREE_LINES | `WheatearEditor/assets/vertical_slice/ui/atlases/skill_tree_line_styles.png` | 1024x512 | 透明 PNG | 连线端点、光点、箭头粒子，不含文字 |
| SKILL_TREE_CORE | `WheatearEditor/assets/vertical_slice/ui/atlases/skill_tree_core_emblem.png` | 512x512 | 透明 PNG | 魔剑核心节点徽记 |

### 4.4 结算、存档与设置

| 资源 ID | 正式输出文件 | 规格 | 背景 | 说明 |
| --- | --- | --- | --- | --- |
| RESULT_BADGES | `WheatearEditor/assets/vertical_slice/ui/atlases/result_badges_256.png` | 2048x512 | 透明 PNG | 8x2，cell 256，S/A/B/C 评分、首通、连击、掉落 |
| SAVE_SLOT_CARDS | `WheatearEditor/assets/vertical_slice/ui/atlases/save_slot_cards.png` | 2048x1024 | 透明 PNG | 存档槽卡片 normal/hover/selected/empty |
| SETTINGS_CONTROLS | `WheatearEditor/assets/vertical_slice/ui/atlases/settings_controls_256.png` | 2048x512 | 透明 PNG | 滑条轨道、手柄、checkbox、toggle，无文字 |
| NOTIFICATION_PANELS | `WheatearEditor/assets/vertical_slice/ui/atlases/notification_panels.png` | 1024x512 | 透明 PNG | toast、系统消息、解锁提示底板 |

## 5. Atlas 切片说明

### 5.1 common_buttons_256.png

`8 columns x 4 rows`，每格 `256x256`：

| 行 | 状态 |
| ---: | --- |
| 0 | normal |
| 1 | hover |
| 2 | pressed/selected |
| 3 | disabled/locked |

列可以做不同按钮宽高比例的底板缩略母版：短按钮、长按钮、图标按钮、分页按钮、返回按钮、确认按钮、危险按钮、空白备用。

### 5.2 system_icons_256.png

`8 columns x 8 rows`，每格 `256x256`。首批建议顺序：

```text
row0: dungeon, skill_tree, equipment, relationship, support, save, settings, back
row1: home, result, inventory, material, map, tutorial, history, next_story
row2: magic_sword, armor, accessory, charm, boots, special, lock, question
row3: hp, mp, exp, gold, chapter, objective, warning, success
row4-row7: reserved for later chapters and UI states
```

所有图标都不能写字，不能把中文或英文字母烘进图标。

## 6. 通用负面要求

所有提示词追加：

```text
no text, no letters, no numbers, no key labels, no logo, no watermark, no signature, no screenshot composition, no character full illustration, no background scene, no visible cell borders, no grid lines, no white background, no gray background, no checkerboard background, no white matte, no opaque background rectangle, no dirty alpha edge, no copied franchise UI, no copied DNF UI, no inconsistent icon scale, no blurry upscaled texture, no low-resolution chunky pixel UI, no Stardew Valley style, no overly cute farm-game UI
```

## 7. 通用面板 Prompt

```text
Use case: stylized-concept
Asset type: high-resolution fantasy RPG system UI panel atlas with subtle pixel accents
Primary request: original Wheatear vertical slice high-resolution system UI panel atlas, [PANEL SET REQUEST]
Style/medium: polished anime fantasy RPG interface with subtle pixel-art accents, dark translucent glass, black-silver magic sword metal trim, cyan magic glow accents, subtle warm gold edges, game-ready UI art
Composition/framing: [CANVAS SIZE] RGBA transparent PNG, panels and components separated with generous transparent padding, nine-slice friendly borders, clean transparent or near-transparent center areas for engine-rendered Chinese text and engine-drawn readable backdrop, no visible slice guides
Transparency rule: unused pixels must be fully transparent alpha 0, no white matte, no gray matte, no checkerboard background, no opaque rectangle behind the UI, clean alpha edge
Resolution rule: native high-resolution artwork for 2560x1440 UI, crisp edges when downscaled, no blurry upscale
Constraints: no text, no letters, no numbers, no logo, no watermark, no character art, no screenshot
Avoid: over-ornate borders, mobile gacha clutter, red notification dots, baked large dark rectangles in panel centers, flat gray rectangles, background residue requiring cutout, copied DNF UI, blurry painted rendering, low-resolution chunky pixel UI, Stardew Valley style, overly cute farm-game UI
```

面板替换：

```text
common_panels_4k.png: large panel, secondary panel, tooltip panel, modal panel, title bar, separator line, all style-matched and nine-slice friendly
common_buttons_256.png: button state atlas with normal, hover, pressed, disabled rows, no text or icon inside
common_tabs_256.png: tab and segmented control state atlas, normal and selected rows, no text
common_slots_256.png: item slot, equipment slot, material slot, save slot mini-frame, locked slot, selected slot, empty slot
save_slot_cards.png: larger save card backgrounds for normal, hover, selected and empty states, no text or screenshot thumbnail
settings_controls_256.png: slider track, slider handle, checkbox empty, checkbox checked, toggle off, toggle on, stepper buttons, no symbols or text
notification_panels.png: toast notification, unlock popup, system message panel, no text
```

## 8. 图标 Atlas Prompt

```text
Use case: stylized-concept
Asset type: high-resolution fantasy RPG system icon atlas with subtle pixel accents
Primary request: original Wheatear high-resolution system UI icon atlas, [ICON SET REQUEST]
Style/medium: polished anime fantasy RPG UI icons with subtle pixel-art accents, clean silhouettes, crisp hard edges, cyan magic sword glow, black-silver and warm gold accents, transparent PNG
Composition/framing: exact atlas layout [COLUMNS] columns x [ROWS] rows, each cell 256x256, total canvas [TOTAL WIDTH]x[TOTAL HEIGHT], each icon centered with consistent scale and 28px safe padding, no visible grid lines
Transparency rule: unused pixels alpha 0, no white matte, no background rectangle, no checkerboard, clean transparent edge around icons
Resolution rule: native 256x256 detail, readable when downscaled to 128x128 and 64x64
Constraints: no text, no letters, no numbers, no logo, no watermark, no character portrait unless explicitly requested as a small symbolic relationship icon
Avoid: inconsistent icon scale, copied commercial icon style, copied DNF UI, random decorations unrelated to function, background residue, blurry painted rendering, low-resolution chunky pixel UI, Stardew Valley style, overly cute farm-game UI
```

图标集替换：

```text
system_icons_256.png: dungeon, skill tree, equipment, relationship, support, save, settings, back, home, result, inventory, material, map, tutorial, history, next story, magic sword, armor, accessory, charm, boots, special, lock, question, hp, mp, exp, gold, chapter, objective, warning, success
material_icons_256.png: magic core crystal, beast sinew, bear claw, iron fragment, enhancement stone, forest herb, rare shard, empty material placeholder; multiple rows for normal and highlighted states
equipment_icons_256.png: traveler armor, black forest leather armor, beast tooth pendant, novice magic ring, wind boots, old ward charm, training blade, angel feather, plus generic weapon armor accessory slot symbols
relationship_icons_256.png: Aoba mentor same-person symbolic icon, white mage, guardian, black mage, affinity heart stages, locked companion, support slot, relationship event marker
skill_tree_nodes_256.png: skill node frames and icons for core, melee, launcher, air, magic, support, mobility, break limit; states normal, learnable, learned, locked
result_badges_256.png: result grade badges S A B C represented by abstract rank medals with no letters, first clear badge, combo badge, loot badge, experience badge, material badge
```

## 9. 技能树专用 Prompt

```text
Use case: stylized-concept
Asset type: high-resolution fantasy RPG skill tree UI asset with subtle pixel accents
Primary request: original high-resolution magic sword skill tree node and connector assets, [SKILL TREE REQUEST]
Style/medium: polished anime fantasy RPG system UI with subtle pixel-art accents, black-silver magic sword motif, cyan energy lines, subtle warm gold highlight, transparent PNG
Composition/framing: RGBA transparent PNG, fixed 256x256 cells for node atlas or specified canvas for connector asset, each node centered with consistent scale, no visible grid lines
Transparency rule: unused pixels alpha 0, no white matte, no opaque background, clean glow edge
Constraints: no text, no letters, no numbers, no logo, no watermark, no character art
Avoid: unreadable tiny symbols, copied skill tree UI, copied DNF UI, excessive glow covering node state, blurry painted rendering, low-resolution chunky pixel UI, Stardew Valley style, overly cute farm-game UI
```

替换：

```text
skill_tree_nodes_256.png: node frames and symbolic icons for magic sword core, slash, launcher, air chase, magic bolt, support, mobility, break limit; states normal, learnable, learned, locked
skill_tree_line_styles.png: small connector line caps, glowing dots, arrow particles, branch highlights, no full background
skill_tree_core_emblem.png: standalone magic sword core emblem, centered, complete circular shape, no cropped glow
```

## 10. ui_atlas_params.yaml 示例

```yaml
productionMode: direct_ui_atlas_or_panel

transparentPngRule:
  requireRgba: true
  unusedPixelsAlpha: 0
  rejectWhiteMatte: true
  rejectOpaqueBackground: true

engineTextBackdrop:
  enabledForTextPanels: true
  source: engine_drawn_fill
  changesPngAlphaAtRuntime: false
  color: "#10131D"
  alpha: 0.70-0.85
  pngCenterAlpha: 0.0-0.15
  drawOrder: [system_page_or_hub_scene, engine_text_backdrop_fill, panel_frame_png, engine_text_or_values]
  primitive: ui_panel_or_ui_image_quad
  rectSource: textRectInset_or_widget_inner_rect
  runtimeAdjustable: true

atlases:
  common_buttons_256: { width: 2048, height: 1024, cellWidth: 256, cellHeight: 256, columns: 8, rows: 4 }
  common_tabs_256: { width: 2048, height: 512, cellWidth: 256, cellHeight: 256, columns: 8, rows: 2 }
  common_slots_256: { width: 2048, height: 1024, cellWidth: 256, cellHeight: 256, columns: 8, rows: 4 }
  system_icons_256: { width: 2048, height: 2048, cellWidth: 256, cellHeight: 256, columns: 8, rows: 8 }
  material_icons_256: { width: 2048, height: 1024, cellWidth: 256, cellHeight: 256, columns: 8, rows: 4 }
  equipment_icons_256: { width: 2048, height: 1024, cellWidth: 256, cellHeight: 256, columns: 8, rows: 4 }
  relationship_icons_256: { width: 2048, height: 512, cellWidth: 256, cellHeight: 256, columns: 8, rows: 2 }
  skill_tree_nodes_256: { width: 2048, height: 1024, cellWidth: 256, cellHeight: 256, columns: 8, rows: 4 }
  result_badges_256: { width: 2048, height: 512, cellWidth: 256, cellHeight: 256, columns: 8, rows: 2 }

nineSlice:
  common_panels_4k:
    width: 4096
    height: 4096
    textBackdropSource: engine_drawn_fill
    note: contains multiple separated panel parts; each part needs its own border metadata after slicing
  battle_like_tooltip: { suggestedBorder: [56, 56, 56, 56], textBackdropSource: engine_drawn_fill }
  large_system_panel: { suggestedBorder: [96, 96, 96, 96], textBackdropSource: engine_drawn_fill }
  button: { suggestedBorder: [40, 40, 36, 36] }
```

## 11. 推荐交付结构

```text
Batch08_System_UI_Atlas/
  common_panels_4k.png
  common_buttons_256.png
  common_tabs_256.png
  common_slots_256.png
  system_icons_256.png
  material_icons_256.png
  equipment_icons_256.png
  relationship_icons_256.png
  skill_tree_nodes_256.png
  skill_tree_line_styles.png
  skill_tree_core_emblem.png
  result_badges_256.png
  save_slot_cards.png
  settings_controls_256.png
  notification_panels.png
  ui_atlas_params.yaml
```

## 12. 接入动作

1. 把 PNG 放入 `WheatearEditor/assets/vertical_slice/ui/atlases/`。
2. 建立 atlas cell 语义映射，逐步替换当前 `assets/vertical_slice/ui/icons/*.png` 的散图引用。
3. 面板和按钮按九宫格或等效方式接入，禁止整体拉伸造成边框变形。
4. `ProgressionSystem` 仍负责中文文本、数值、tooltip 内容，UI 图片只提供框架层和图标；承载文字的底色按 `engineTextBackdrop` 由引擎绘制，做成独立可调 UI fill 层，不改 PNG 文件透明通道。
5. 重新打包 Sandbox，检查 `content.wtpack` 包含本批 UI atlas。

## 13. 验收标准

- 所有透明素材必须是真 RGBA PNG，空白 alpha 为 0；出现白底、灰底、棋盘格底或白色残边即不合格。
- 用黑、白、亮粉、透明棋盘四种底色检查，边缘都必须干净。
- atlas 尺寸精确，cell 大小、行列数和文档一致。
- 同一 atlas 内图标/按钮/槽位视觉尺寸一致，不能有的贴边、有的大量空白。
- UI atlas 是允许直接整图生成的例外；如果任一 cell 图标/按钮/槽位跨格、贴边、比例漂移、状态行不对齐或带残底，整张 atlas 不合格。
- 面板中心区域干净，默认透明或近透明，2K 显示下不糊，九宫格拉伸后角饰不变形；运行时文字底由引擎 fill 提供，不能让中文直接压在复杂背景上。
- 所有 UI 资产不含文字、数字、键位、Logo、水印或商业可识别 UI 图案。
- UI 只能有轻微像素质感，边缘清晰稳定，不是模糊插画缩小图，也不是低分辨率粗颗粒 UI；图标缩到 128x128 和 64x64 后仍能识别。
- 风格与 VN UI、战斗 HUD、黑林场景和魔剑主题统一，不像外部素材拼贴。
