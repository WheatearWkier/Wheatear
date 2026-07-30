# Batch02 VN UI 高清皮肤生产包

更新时间：2026-07-30

本包承接 `Batch01A_VN风格小样生产包.md` 和 `Batch01B_VN当前可见表情与场景生产包.md`。Batch01 已经锁定 VN 背景、立绘和正式命名规则；本批开始替换 VN 对话层 UI，让序章、主菜单后的剧情播放和选择项不再像临时调试界面。

目标参考 `docs/参考分析/美术参考/VN的UI界面.png` 的信息层级：底部半透明对话框、左下小头像/说话者区域、底部一排命令图标、左上角 BGM 提示条。只参考布局层级和完成度，不复刻原图图案、角色、文字、Logo、装饰纹样或商业可识别元素。

## 1. 本批原则

- 所有输出文件使用正式 `snake_case` 命名，放入 `WheatearEditor/assets/vertical_slice/ui/atlases/`。
- UI 素材必须透明 PNG，面板类素材保留可九宫格拉伸的边框和干净中心区域。
- UI 上不绘制任何文字、字母、数字、Logo 或水印。游戏文字由引擎字体渲染。
- 风格统一为“现代日常进入幻想后的 VN 皮肤”：深色玻璃、柔和玫瑰金边线、少量青蓝魔法高光。
- 不做一整张 UI 截图，本批只交付可被工程拆分使用的面板、按钮底和图标 atlas。
- 不能使用旧 demo 路径或短名，后续工程引用必须指向本批正式路径。

## 2. 本批资产

| 资源 ID | 正式输出文件 | 规格 | 用途 |
| --- | --- | --- | --- |
| UI_VN_TEXTBOX | `WheatearEditor/assets/vertical_slice/ui/atlases/vn_textbox_panel.png` | 1600x320，透明 PNG | 底部 VN 对话框底板，可九宫格 |
| UI_VN_NAMEPLATE | `WheatearEditor/assets/vertical_slice/ui/atlases/vn_nameplate.png` | 360x96，透明 PNG | 角色名牌底板 |
| UI_VN_COMMAND_ICONS | `WheatearEditor/assets/vertical_slice/ui/atlases/vn_command_icons_128.png` | 1024x256，透明 PNG | VN 命令图标 atlas |
| UI_VN_BGM_NOTICE | `WheatearEditor/assets/vertical_slice/ui/atlases/vn_bgm_notice_panel.png` | 560x96，透明 PNG | 左上角 BGM 提示条底板 |
| UI_VN_CHOICE_PANEL | `WheatearEditor/assets/vertical_slice/ui/atlases/vn_choice_panel.png` | 900x120，透明 PNG | 选择项按钮底板 |

## 3. 图集切片说明

`vn_command_icons_128.png` 使用 `128x128` cell，`8 columns x 2 rows`：

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
no text, no letters, no numbers, no logo, no watermark, no signature, no brand mark, no copyright UI, no recognizable franchise design, no character art, no background scene, no screenshot composition, no blurry edge, no baked dialogue text, no visible cell guide lines
```

## 5. 风格总纲

- 视觉关键词：translucent dark glass, rose-gold trim, subtle cyan magic accent, clean galgame UI, elegant but readable。
- UI 不能过度厚重，放在明亮学校背景和深色黑林背景上都要能读。
- 边框装饰可以带少量花瓣、魔法回路、细线角饰，但不要复杂到抢文字。
- 对话框中心必须干净，便于渲染中文正文、名字、历史记录和自动播放状态。
- UI 资产边缘要干净，透明像素不能有脏边或黑边。

## 6. 生成提示词

### 6.1 vn_textbox_panel.png

```text
Use case: stylized-concept
Asset type: visual novel textbox UI panel
Primary request: original game-ready visual novel dialogue textbox panel, elegant translucent dark glass with rose-gold decorative trim and subtle cyan magic highlights
Layout details: wide bottom dialogue box base, soft rounded corners under 8px feel, clear nine-slice friendly border, clean empty center area for rendered dialogue text, small left portrait/name accent zone but no portrait drawn
Style/medium: polished anime galgame UI asset, modern fantasy interface, refined line accents, soft inner glow, transparent PNG
Composition/framing: 1600x320 transparent canvas, panel centered with 40px safe padding, border thickness readable after scaling, no background scene
Lighting/mood: calm premium VN interface, readable over bright school backgrounds and dark forest backgrounds
Color palette: translucent charcoal glass, muted rose-gold edge, tiny cyan magical accents, soft white highlights
Constraints: transparent background, no text, no letters, no logo, no character, no screenshot, no visible slicing guides
Avoid: copying any existing VN UI, heavy baroque ornament, opaque black rectangle, neon cyberpunk overload
```

### 6.2 vn_nameplate.png

```text
Use case: stylized-concept
Asset type: visual novel speaker nameplate UI panel
Primary request: original VN speaker nameplate base matching the textbox panel style
Layout details: compact horizontal nameplate, translucent dark glass center, rose-gold edge, small cyan magical accent notch on one side, enough empty area for rendered Chinese character name
Style/medium: polished anime galgame UI asset, modern fantasy interface, transparent PNG
Composition/framing: 360x96 transparent canvas, centered nameplate, nine-slice friendly, no background scene
Lighting/mood: refined and readable, clearly belongs to the same UI family as vn_textbox_panel.png
Color palette: translucent charcoal, rose-gold trim, subtle cyan glow, soft highlight
Constraints: transparent background, no text, no letters, no logo, no character, no watermark
Avoid: giant decorative badge, sharp unreadable silhouette, copying reference ornaments
```

### 6.3 vn_command_icons_128.png

```text
Use case: stylized-concept
Asset type: visual novel command icon atlas
Primary request: original VN command icon atlas with symbolic icons only, matching translucent rose-gold and cyan fantasy UI style
Icon list: save, load, quick save, quick load, system settings, history log, auto play, skip fast-forward
Layout details: exactly 8 columns and 2 rows, each cell 128x128, total canvas 1024x256, first row normal state, second row hover or active state with brighter cyan glow, transparent background, no visible grid lines
Style/medium: polished game UI icons, clean silhouette, readable at small size, no text labels
Composition/framing: each icon centered in its own 128x128 cell with consistent scale and padding
Color palette: pale rose-gold line art, soft cyan active glow, translucent dark glass button hints
Constraints: transparent background, no text, no letters, no numbers, no logo, no watermark, no character art
Avoid: unreadable tiny detail, commercial UI copies, random decorative symbols unrelated to the command
```

### 6.4 vn_bgm_notice_panel.png

```text
Use case: stylized-concept
Asset type: visual novel BGM notice UI panel
Primary request: original small BGM notification panel for top-left visual novel overlay, no text
Layout details: narrow translucent ribbon panel, small icon socket on the left for a music note rendered by engine or icon, long clean text area on the right, rose-gold edge and subtle cyan pulse
Style/medium: polished anime VN UI, elegant dark glass, transparent PNG
Composition/framing: 560x96 transparent canvas, panel centered, readable against bright and dark backgrounds
Lighting/mood: gentle notification, premium but unobtrusive
Color palette: translucent charcoal, rose-gold trim, muted cyan glow, soft highlight
Constraints: transparent background, no text, no music-note letters, no logo, no character, no background scene
Avoid: bright opaque banner, large decorative badge, copying reference UI
```

### 6.5 vn_choice_panel.png

```text
Use case: stylized-concept
Asset type: visual novel choice button panel
Primary request: original VN choice option button base, elegant translucent fantasy UI, no text
Layout details: long horizontal button panel, clean center area for rendered Chinese choice text, slightly brighter edge on hover-friendly border, subtle rose-gold corner ornaments and cyan focus line
Style/medium: polished anime galgame UI asset, transparent PNG, nine-slice friendly
Composition/framing: 900x120 transparent canvas, centered button, consistent padding and clean silhouette
Lighting/mood: selectable but not loud, readable over VN backgrounds
Color palette: translucent charcoal glass, rose-gold trim, soft cyan selection highlight
Constraints: transparent background, no text, no letters, no logo, no character, no watermark
Avoid: overly thick button, mobile gacha banner style, sharp sci-fi panel unrelated to VN mood
```

## 7. 推荐交付结构

```text
Batch02_VN_UI/
  vn_textbox_panel.png
  vn_nameplate.png
  vn_command_icons_128.png
  vn_bgm_notice_panel.png
  vn_choice_panel.png
  ui_atlas_params.yaml
```

`ui_atlas_params.yaml` 示例：

```yaml
vn_textbox_panel:
  width: 1600
  height: 320
  type: nine_slice_panel
  transparent: true
  suggestedBorder: [48, 48, 48, 48]

vn_nameplate:
  width: 360
  height: 96
  type: nine_slice_panel
  transparent: true
  suggestedBorder: [28, 28, 24, 24]

vn_command_icons_128:
  width: 1024
  height: 256
  cellWidth: 128
  cellHeight: 128
  columns: 8
  rows: 2
  row0: normal
  row1: hover_active
  order: [save, load, quick_save, quick_load, system, history, auto, skip]

vn_bgm_notice_panel:
  width: 560
  height: 96
  type: nine_slice_panel
  transparent: true
  suggestedBorder: [28, 28, 24, 24]

vn_choice_panel:
  width: 900
  height: 120
  type: nine_slice_panel
  transparent: true
  suggestedBorder: [36, 36, 28, 28]
```

## 8. 接入动作

生成后接入当前 demo 的动作是：

1. 把 PNG 放入 `WheatearEditor/assets/vertical_slice/ui/atlases/`。
2. 在 VN UI 渲染或对应 `.wt` UI 图片组件中改引用到正式路径。
3. 图标 atlas 按 `128x128` cell 切片，建立命令语义到 atlas cell 的映射。
4. 对话框、名牌、选择项按九宫格或等效拉伸方式接入，避免拉伸边框装饰。
5. 重新打包 Sandbox，检查 `content.wtpack` 包含本批 UI 资产。

## 9. 验收标准

- 所有文件尺寸精确，透明背景真实有效。
- 面板中心区域干净，不影响中文正文、名字和选项文字可读性。
- 命令图标无文字、无字母、无数字，缩到 32px 仍能辨认语义。
- `vn_command_icons_128.png` 必须是 1024x256，8 列 2 行，每格 128x128。
- 风格和 Batch01 背景/立绘能放在同一画面里，不像外部素材拼贴。
- 不出现参考图里的商业 Logo、视频水印、章节文字、角色图案或可识别 UI 纹样。
