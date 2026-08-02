# Batch02 VN UI 高清皮肤生产包

更新时间：2026-07-31

本包承接 `Batch01A_VN风格小样生产包.md` 和 `Batch01B_VN当前可见表情与场景生产包.md`。Batch01 已经锁定 VN 背景、立绘和正式命名规则；本批开始替换 VN 对话层 UI，让序章、主菜单后的剧情播放和选择项不再像临时调试界面。

目标参考 `docs/参考分析/美术参考/VN的UI界面.png` 的信息层级：底部半透明对话框、左下小头像/说话者区域、底部一排命令图标、左上角 BGM 提示条。只参考布局层级和完成度，不复刻原图图案、角色、文字、Logo、装饰纹样或商业可识别元素。VN UI 使用 2x 高清母版并九宫格接入，避免 2K 屏幕上把小图硬拉导致失真。

## 1. 本批原则

- 所有输出文件使用正式 `snake_case` 命名，放入 `WheatearEditor/assets/vertical_slice/ui/atlases/`。
- UI 素材必须是 **RGBA PNG 真透明**，无用区域 alpha 为 0；面板类素材保留可九宫格拉伸的边框和干净中心区域。
- 对话框、名牌、BGM 提示条和选择项这类承载文字的面板，采用成熟工程做法：PNG 主要提供边框、角饰、高光和少量边缘纹理；文字下方的半透明深色底由引擎绘制。最终 UI 不能显示成透明洞，但不把大面积文字底烘死在 PNG 里。
- UI 上不绘制任何文字、字母、数字、Logo 或水印。游戏文字由引擎字体渲染。
- 风格统一为“现代日常进入幻想后的 VN 皮肤”：深色玻璃、柔和玫瑰金边线、少量青蓝魔法高光。
- 不做一整张 UI 截图，本批只交付可被工程拆分使用的面板、按钮底和图标 atlas。
- VN 命令图标属于规则 UI atlas，可以直接生成 `vn_command_icons_256.png`；它不按动画单帧流程生产，但每个 cell 必须固定 256x256、居中、视觉尺寸一致、无串格。
- 不能使用旧 demo 路径或短名，后续工程引用必须指向本批正式路径。

## 2. 本批资产

| 资源 ID | 正式输出文件 | 规格 | 用途 |
| --- | --- | --- | --- |
| UI_VN_TEXTBOX | `WheatearEditor/assets/vertical_slice/ui/atlases/vn_textbox_panel.png` | 3200x640，透明 PNG | 底部 VN 对话框 2x 母版，可九宫格 |
| UI_VN_NAMEPLATE | `WheatearEditor/assets/vertical_slice/ui/atlases/vn_nameplate.png` | 720x192，透明 PNG | 角色名牌底板 2x 母版 |
| UI_VN_COMMAND_ICONS | `WheatearEditor/assets/vertical_slice/ui/atlases/vn_command_icons_256.png` | 2048x512，透明 PNG | VN 命令图标 atlas，cell 256 |
| UI_VN_BGM_NOTICE | `WheatearEditor/assets/vertical_slice/ui/atlases/vn_bgm_notice_panel.png` | 1120x192，透明 PNG | 左上角 BGM 提示条底板 2x 母版 |
| UI_VN_CHOICE_PANEL | `WheatearEditor/assets/vertical_slice/ui/atlases/vn_choice_panel.png` | 1800x240，透明 PNG | 选择项按钮底板 2x 母版 |

## 3. 图集切片说明

`vn_command_icons_256.png` 使用 `256x256` cell，`8 columns x 2 rows`；运行时可缩到 `128x128` 或 `64x64`：

| 列 | 图标语义 | 第 1 行 | 第 2 行 |
| ---: | --- | --- | --- |
| 0 | save | normal | hover/active |
| 1 | load | normal | hover/active |
| 2 | quick save | normal | hover/active |
| 3 | quick load | normal | hover/active |
| 4 | system | normal | hover/active |
| 5 | history | normal | hover/active |
| 6 | auto | normal | hover/active |
| 7 | skip | normal | hover/active |

图标必须用无文字符号表达，例如存档用小型水晶/书签符号，读取用打开的书或回旋箭头，系统用齿轮，历史用卷轴/时钟，自动用播放循环符号，跳过用双箭头。禁止出现 `SAVE`、`LOAD`、`AUTO`、`SKIP` 等文字。

## 4. 通用负面要求

所有提示词追加：

```text
no text, no letters, no numbers, no logo, no watermark, no signature, no brand mark, no copyright UI, no recognizable franchise design, no character art, no background scene, no screenshot composition, no blurry edge, no baked dialogue text, no visible cell guide lines, no white background, no gray background, no checkerboard background, no opaque background rectangle, no white matte, no dirty alpha edge
```

### 4.1 PNG 真透明与高清母版规则

```yaml
transparentPngRule:
  requireRgba: true
  unusedPixelsAlpha: 0
  rejectWhiteBackground: true
  rejectGrayBackground: true
  rejectCheckerboardBackground: true
  rejectOpaqueBackgroundRectangle: true
  rejectWhiteMatte: true
  rejectDirtyAlphaEdge: true

resolutionRule:
  targetUi: 2560x1440
  authoringScale: 2x
  nineSliceRequired: true

panelTextBackdropRule:
  outsidePanelAlpha: 0
  readableBackdropRequired: true
  defaultBackdropSource: engine_drawn_fill
  changesPngAlphaAtRuntime: false
  bakedPngFillAllowedOnlyForSmallControls: true
  recommendedBackdropAlpha: 0.70-0.85
  recommendedBackdropStyle: translucent_dark_glass
  pngTextAreaAlpha: 0.0-0.15
  borderAndCornerAlpha: 0.85-1.0
  noTransparentFinalTextArea: true
  noPureWhiteTextArea: true
  noOpaqueFullCanvasRectangle: true
  runtimeDrawOrder: [vn_background_or_scene, engine_text_backdrop_fill, panel_frame_png, engine_text]
  runtimePrimitive: ui_panel_or_ui_image_quad
```

验收时把每个 UI PNG 放到纯黑、纯白、亮粉和透明棋盘背景上检查；只要有白底、灰底、浅色方块、白色毛边、半透明残底或脏 alpha，直接重生。

面板类素材验收要额外检查：外轮廓之外 alpha 必须为 0；PNG 中心正文区不要烘焙大面积实底，允许透明或很轻的内阴影/纹理，建议 alpha 0%-15%。`ui_atlas_params.yaml` 必须写 `textBackdropSource: engine_drawn_fill`、颜色、alpha 和 textRect；接入后由引擎绘制半透明深色文字底，建议 alpha 70%-85%，放在亮背景和暗背景上都能读中文。文字本身不烘焙进图片。

工程实现规则：不要通过修改 PNG 透明通道来控制文字阅读底。正确做法是在同一 UI 层级里额外放一个 `UIPanelComponent` 或无贴图 `UIImageComponent`，矩形范围等于 `textRectInset` 推导出的文字区，颜色使用 `#10131D` 这类深色并设置 alpha 0.70-0.85；然后在它上面绘制 PNG 框架层，最后绘制 `UITextComponent`。后续如果背景太亮或太暗，只调这个 fill 的颜色/alpha，不重生或改写 PNG。

## 5. 风格总纲

- 视觉关键词：translucent dark glass, rose-gold trim, subtle cyan magic accent, clean galgame UI, elegant but readable。
- UI 不能过度厚重，放在明亮学校背景和深色黑林背景上都要能读。
- 边框装饰可以带少量花瓣、魔法回路、细线角饰，但不要复杂到抢文字。
- 对话框中心必须干净，默认让 PNG 中心保持透明或极轻微内阴影；真正的半透明深色文字底由引擎绘制，便于渲染中文正文、名字、历史记录和自动播放状态，也便于后续按背景亮度调 alpha。
- UI 资产边缘要干净，透明像素不能有脏边、黑边、白边、白色 matte、灰底、棋盘格底或半透明残底。

## 6. 生成提示词

### 6.1 vn_textbox_panel.png

```text
Use case: stylized-concept
Asset type: visual novel textbox UI panel
Primary request: original game-ready visual novel dialogue textbox frame layer, elegant rose-gold decorative trim and subtle cyan magic highlights, engine-drawn dark glass text backdrop will be added separately
Layout details: wide bottom dialogue box frame, soft rounded corners under 8px feel, clear nine-slice friendly border, clean transparent or near-transparent center area for rendered dialogue text and engine-drawn backdrop, small left portrait/name accent zone but no portrait drawn
Style/medium: polished anime galgame UI asset, modern fantasy interface, refined line accents, soft inner glow, transparent PNG
Composition/framing: 3200x640 RGBA transparent canvas, outside the panel unused pixels alpha 0, panel centered with 80px safe padding, center text area alpha 0 to 15 percent only, border thickness readable after scaling, no background scene
Lighting/mood: calm premium VN interface, readable over bright school backgrounds and dark forest backgrounds
Color palette: translucent charcoal glass, muted rose-gold edge, tiny cyan magical accents, soft white highlights
Constraints: transparent outside panel, no text, no letters, no logo, no character, no screenshot, no visible slicing guides, no white matte, no dirty alpha edge
Avoid: copying any existing VN UI, heavy baroque ornament, baked large dark rectangle in the center, pure white text area, opaque full-canvas rectangle, neon cyberpunk overload, blurry upscaled edge
```

### 6.2 vn_nameplate.png

```text
Use case: stylized-concept
Asset type: visual novel speaker nameplate UI panel
Primary request: original VN speaker nameplate base matching the textbox panel style
Layout details: compact horizontal nameplate frame, transparent or near-transparent center for engine-drawn text backdrop, rose-gold edge, small cyan magical accent notch on one side, enough empty area for rendered Chinese character name
Style/medium: polished anime galgame UI asset, modern fantasy interface, transparent PNG
Composition/framing: 720x192 RGBA transparent canvas, outside the nameplate unused pixels alpha 0, centered nameplate, center text area alpha 0 to 15 percent only, nine-slice friendly, no background scene
Lighting/mood: refined and readable, clearly belongs to the same UI family as vn_textbox_panel.png
Color palette: translucent charcoal, rose-gold trim, subtle cyan glow, soft highlight
Constraints: transparent background, no text, no letters, no logo, no character, no watermark, no white matte, no dirty alpha edge
Avoid: giant decorative badge, baked large dark rectangle in the center, pure white text area, sharp unreadable silhouette, copying reference ornaments, blurry upscaled edge
```

### 6.3 vn_command_icons_256.png

```text
Use case: stylized-concept
Asset type: visual novel command icon atlas
Primary request: original VN command icon atlas with symbolic icons only, matching translucent rose-gold and cyan fantasy UI style
Icon list: save, load, quick save, quick load, system settings, history log, auto play, skip fast-forward
Layout details: exactly 8 columns and 2 rows, each cell 256x256, total canvas 2048x512, first row normal state, second row hover or active state with brighter cyan glow, RGBA transparent background, unused pixels alpha 0, no visible grid lines
Style/medium: polished game UI icons, clean silhouette, readable at small size, no text labels
Composition/framing: each icon centered in its own 256x256 cell with consistent scale and 28px or more padding
Color palette: pale rose-gold line art, soft cyan active glow, translucent dark glass button hints
Constraints: transparent background, no text, no letters, no numbers, no logo, no watermark, no character art, no white matte, no dirty alpha edge
Avoid: unreadable tiny detail, commercial UI copies, random decorative symbols unrelated to the command, blurry upscaled icon
```

### 6.4 vn_bgm_notice_panel.png

```text
Use case: stylized-concept
Asset type: visual novel BGM notice UI panel
Primary request: original small BGM notification panel for top-left visual novel overlay, no text
Layout details: narrow ribbon frame, small icon socket on the left for a music note rendered by engine or icon, long clean transparent or near-transparent text area on the right for engine-drawn backdrop, rose-gold edge and subtle cyan pulse
Style/medium: polished anime VN UI, elegant dark glass, transparent PNG
Composition/framing: 1120x192 RGBA transparent canvas, outside the panel unused pixels alpha 0, panel centered, center text area alpha 0 to 15 percent only, readable against bright and dark backgrounds after engine backdrop is applied
Lighting/mood: gentle notification, premium but unobtrusive
Color palette: translucent charcoal, rose-gold trim, muted cyan glow, soft highlight
Constraints: transparent background, no text, no music-note letters, no logo, no character, no background scene, no white matte, no dirty alpha edge
Avoid: bright opaque banner, baked large dark rectangle in the center, pure white text area, large decorative badge, copying reference UI, blurry upscaled edge
```

### 6.5 vn_choice_panel.png

```text
Use case: stylized-concept
Asset type: visual novel choice button panel
Primary request: original VN choice option button base, elegant translucent fantasy UI, no text
Layout details: long horizontal choice frame, clean transparent or near-transparent center area for rendered Chinese choice text and engine-drawn backdrop, slightly brighter edge on hover-friendly border, subtle rose-gold corner ornaments and cyan focus line
Style/medium: polished anime galgame UI asset, transparent PNG, nine-slice friendly
Composition/framing: 1800x240 RGBA transparent canvas, outside the button unused pixels alpha 0, centered button, center text area alpha 0 to 15 percent only, consistent padding and clean silhouette
Lighting/mood: selectable but not loud, readable over VN backgrounds
Color palette: translucent charcoal glass, rose-gold trim, soft cyan selection highlight
Constraints: transparent background, no text, no letters, no logo, no character, no watermark, no white matte, no dirty alpha edge
Avoid: overly thick button, baked large dark rectangle in the center, pure white text area, mobile gacha banner style, sharp sci-fi panel unrelated to VN mood, blurry upscaled edge
```

## 7. 推荐交付结构

```text
Batch02_VN_UI/
  vn_textbox_panel.png
  vn_nameplate.png
  vn_command_icons_256.png
  vn_bgm_notice_panel.png
  vn_choice_panel.png
  ui_atlas_params.yaml
```

`ui_atlas_params.yaml` 示例：

```yaml
productionMode: direct_ui_atlas_or_panel

engineTextBackdropImplementation:
  changesPngAlphaAtRuntime: false
  drawOrder: [vn_background_or_scene, engine_text_backdrop_fill, panel_frame_png, engine_text]
  primitive: ui_panel_or_ui_image_quad
  color: "#10131D"
  alpha: 0.70-0.85
  rectSource: textRectInset
  runtimeAdjustable: true

vn_textbox_panel:
  width: 3200
  height: 640
  type: nine_slice_panel
  transparent: true
  requireRgba: true
  unusedPixelsAlpha: 0
  outsidePanelAlpha: 0
  pngTextAreaAlpha: 0.0-0.15
  textBackdropSource: engine_drawn_fill
  textBackdropColor: "#10131D"
  textBackdropAlpha: 0.70-0.85
  textRectInset: [160, 112, 160, 96]
  readableTextBackdrop: true
  suggestedBorder: [96, 96, 96, 96]

vn_nameplate:
  width: 720
  height: 192
  type: nine_slice_panel
  transparent: true
  requireRgba: true
  unusedPixelsAlpha: 0
  outsidePanelAlpha: 0
  pngTextAreaAlpha: 0.0-0.15
  textBackdropSource: engine_drawn_fill
  textBackdropColor: "#10131D"
  textBackdropAlpha: 0.70-0.85
  textRectInset: [72, 48, 72, 48]
  readableTextBackdrop: true
  suggestedBorder: [56, 56, 48, 48]

vn_command_icons_256:
  width: 2048
  height: 512
  cellWidth: 256
  cellHeight: 256
  columns: 8
  rows: 2
  transparent: true
  requireRgba: true
  unusedPixelsAlpha: 0
  row0: normal
  row1: hover_active
  order: [save, load, quick_save, quick_load, system, history, auto, skip]

vn_bgm_notice_panel:
  width: 1120
  height: 192
  type: nine_slice_panel
  transparent: true
  requireRgba: true
  unusedPixelsAlpha: 0
  outsidePanelAlpha: 0
  pngTextAreaAlpha: 0.0-0.15
  textBackdropSource: engine_drawn_fill
  textBackdropColor: "#10131D"
  textBackdropAlpha: 0.70-0.85
  textRectInset: [96, 44, 72, 44]
  readableTextBackdrop: true
  suggestedBorder: [56, 56, 48, 48]

vn_choice_panel:
  width: 1800
  height: 240
  type: nine_slice_panel
  transparent: true
  requireRgba: true
  unusedPixelsAlpha: 0
  outsidePanelAlpha: 0
  pngTextAreaAlpha: 0.0-0.15
  textBackdropSource: engine_drawn_fill
  textBackdropColor: "#10131D"
  textBackdropAlpha: 0.70-0.85
  textRectInset: [96, 56, 96, 56]
  readableTextBackdrop: true
  suggestedBorder: [72, 72, 56, 56]
```

## 8. 接入动作

生成后接入当前 demo 的动作是：

1. 把 PNG 放入 `WheatearEditor/assets/vertical_slice/ui/atlases/`。
2. 在 VN UI 渲染或对应 `.wt` UI 图片组件中改引用到正式路径。
3. 图标 atlas 按 `256x256` cell 切片，运行时可缩放显示为 `128x128` 或 `64x64`，建立命令语义到 atlas cell 的映射。
4. 对话框、名牌、BGM 提示条、选择项按九宫格或等效拉伸方式接入，避免拉伸边框装饰；同时按 `ui_atlas_params.yaml` 的 `textRectInset/textBackdropColor/textBackdropAlpha` 增加一层可调 `UIPanelComponent` 或无贴图 `UIImageComponent`，由引擎在文字下方绘制半透明底。
5. 重新打包 Sandbox，检查 `content.wtpack` 包含本批 UI 资产。

## 9. 验收标准

- 所有文件尺寸精确，透明背景真实有效；无用区域 alpha 必须为 0。
- 透明 PNG 放到黑、白、亮粉、透明棋盘背景上检查，不能出现白底、灰底、棋盘格底、白色 matte、半透明残底或脏 alpha。
- 面板外部无用区域 alpha 必须为 0；PNG 中心文字区域默认透明或近透明，不能烘焙大面积深色实底、纯白/浅灰实底或脏底。最终运行时必须由引擎绘制可读半透明深色文字底，不影响中文正文、名字和选项文字可读性。
- 命令图标无文字、无字母、无数字，缩到 32px 仍能辨认语义。
- `vn_command_icons_256.png` 必须是 2048x512，8 列 2 行，每格 256x256。
- `vn_command_icons_256.png` 是 UI atlas 例外，可以直接整图生成；但任一图标贴边、大小漂移、跨入邻格或透明残底都必须重生。
- `vn_textbox_panel.png` 等面板按 2x 母版生产并通过九宫格缩放，2K 显示下不糊、不拉伸角饰。
- 风格和 Batch01 背景/立绘能放在同一画面里，不像外部素材拼贴。
- 不出现参考图里的商业 Logo、视频水印、章节文字、角色图案或可识别 UI 纹样。
