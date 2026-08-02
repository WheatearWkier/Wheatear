# Batch01A VN 风格小样生产包

更新时间：2026-07-31

本包用于先锁定 VN 高清美术方向，再批量扩展 Batch 01 的完整背景与立绘差分。目标不是一次性铺满所有素材，而是先得到 2 张背景和 3 张 neutral 立绘，用来验证画风、角色识别度、透明 PNG 后处理和工程接入路径。

本批所有资源都是静态单图，不参与动画 strip 拼接流程；立绘本身就必须作为单张 RGBA 透明 PNG 交付。

## 1. 本批资产

| 资产 | 输出文件 | 规格 | 备注 |
| --- | --- | --- | --- |
| 现实上学路清晨 | `WheatearEditor/assets/vertical_slice/vn/backgrounds/bg_modern_schoolroad_morning.png` | 1920x1080，不透明 PNG | 序章日常基调 |
| 异世界醒来黑林 | `WheatearEditor/assets/vertical_slice/vn/backgrounds/bg_otherworld_forest_wake.png` | 1920x1080，不透明 PNG | 转入幻想/危险基调 |
| 主角现实校服 neutral | `WheatearEditor/assets/vertical_slice/vn/portraits/protag_school_neutral.png` | 1024x1536，透明 PNG | 后续主角表情差分锚点 |
| 现实青梅 neutral | `WheatearEditor/assets/vertical_slice/vn/portraits/aoba_neutral.png` | 1024x1536，透明 PNG | 和导师为同一人，日常打扮 |
| 真青梅/导师 neutral | `WheatearEditor/assets/vertical_slice/vn/portraits/mentor_neutral.png` | 1024x1536，透明 PNG | 和青梅为同一人，导师/魔剑士打扮 |

## 2. 通用负面要求

所有提示词追加：

```text
no text, no logo, no watermark, no signature, no UI letters, no copyright character, no recognizable franchise design, no cropped body, no blurry edge, no inconsistent frame size, no changing costume between variants, no white background behind portraits, no gray background behind portraits, no checkerboard background, no white matte, no dirty alpha edge
```

### 2.1 立绘 PNG 真透明规则

背景图明确为不透明 PNG；三张立绘必须是 **RGBA PNG 真透明**：

```yaml
portraitTransparency:
  requireRgba: true
  unusedPixelsAlpha: 0
  rejectWhiteBackground: true
  rejectGrayBackground: true
  rejectCheckerboardBackground: true
  rejectWhiteMatte: true
  rejectDirtyAlphaEdge: true
```

验收时把立绘放到纯黑、纯白、亮粉和透明棋盘背景上检查；只要角色外缘出现白边、灰边、残底或半透明脏像素，直接重生，不再后期抠图。

## 3. 风格总纲

- VN 背景：高清二次元 galgame 背景，干净、漂亮、可读，细节丰富但不杂乱。
- VN 立绘：透明半身立绘，三分之二正面，干净赛璐璐上色，精细眼睛和表情。
- 参考图只取信息层级、气质和画面完成度，不复刻商业角色、Logo、文字、具体服装或可识别图案。
- 本批立绘作为后续表情差分锚点，必须保持画布、站位、头身比例、脸部结构和服装结构稳定。

## 4. 角色身份锁定

### 4.1 青梅与导师

青梅 `aoba_*` 和导师 `mentor_*` 是同一个人，不是不同阶段、不是不同年龄，只是不同打扮、装饰和身份呈现。区别要服务于剧情伪装/身份反差，但五官、年龄感和人物本体必须完全一致。

必须保持：

- 同一脸型和下巴轮廓。
- 同一眼型、瞳色倾向和温柔底色。
- 同一黑色中长发的发质、刘海结构和发尾走势。
- 同一可识别的核心气质：亲近、可靠、内心克制。
- 后续表情差分中，两者的五官比例不得漂移成不同角色。

允许并且需要区别：

- 青梅：现实日常、可爱、学生感、轻松亲近，服装和装饰简洁。
- 导师：同一个人换成导师/魔剑士打扮，更冷静、更克制，服装和装饰带战斗身份气质。
- 可通过衣服材质、领口/披肩/护具、发饰、腰饰、武器挂件、站姿和眼神压迫感区分。
- 区别要像“同一人换了服装、装饰和妆发整理方式”，不要像年龄变大，不要像成长阶段变化，也不要像两个无关角色。

## 5. 生成提示词

### 5.1 bg_modern_schoolroad_morning.png

```text
Use case: stylized-concept
Asset type: visual novel background
Primary request: original anime visual novel background of a modern Japanese-inspired school road in the early morning
Scene/backdrop: quiet residential street leading toward a school area, low walls, trees, utility poles, soft morning haze, no people
Style/medium: polished high-resolution galgame background art
Composition/framing: 1920x1080 16:9 landscape, clean readable depth, camera at human eye level, natural path leading into the scene
Lighting/mood: warm clear morning light, gentle and nostalgic, calm prologue atmosphere
Color palette: fresh sky blue, soft green foliage, warm pale sunlight, restrained everyday colors
Constraints: no characters, no vehicles as focal subjects, no text, no signage with readable letters, no logos, no watermark
Avoid: photo realism, cluttered street detail, gloomy horror mood
```

### 5.2 bg_otherworld_forest_wake.png

```text
Use case: stylized-concept
Asset type: visual novel background
Primary request: original anime visual novel background of waking up in an otherworldly black forest
Scene/backdrop: dark fantasy forest clearing, twisted trees, broken branches, mossy ground, faint cyan magical plants, distant mist, no characters
Style/medium: polished high-resolution galgame background art
Composition/framing: 1920x1080 16:9 landscape, readable depth, low eye-level view as if the viewer has just opened their eyes on the forest floor
Lighting/mood: mysterious but not muddy, cool cyan magical rim light, uneasy first-arrival atmosphere
Color palette: deep green-black forest tones balanced by cyan blue magic glow and small warm highlights
Constraints: no characters, no monsters, no text, no logos, no watermark
Avoid: horror gore, overly dark unreadable background, realistic photo texture
```

### 5.3 protag_school_neutral.png

```text
Use case: stylized-concept
Asset type: visual novel half-body character portrait
Primary request: original anime visual novel half-body portrait of the young male protagonist in his real-world school uniform, neutral expression
Subject: teenage boy protagonist, slim build, slightly tired but kind eyes, neat dark hair, modern school uniform with subtle loosened details, ordinary student before the fantasy incident
Style/medium: polished galgame character portrait, clean cel shading, expressive eyes, refined line art
Composition/framing: RGBA transparent background, unused pixels alpha 0, 1024x1536 portrait canvas, half-body, front three-quarter view, centered consistent standing pose, generous padding, no cropped body
Lighting/mood: soft neutral VN portrait lighting, calm but with a hint of inner tension
Color palette: dark school uniform, white shirt accents, subtle cool shadows
Constraints: transparent background, no text, no logo, no watermark, no white matte, no dirty alpha edge; keep costume simple enough to support later damaged/fantasy variants
Avoid: heroic armor, exaggerated action pose, childlike proportions, background residue requiring cutout
```

### 5.4 aoba_neutral.png

```text
Use case: stylized-concept
Asset type: visual novel half-body character portrait
Primary request: original anime visual novel half-body portrait of Aoba, the real-world childhood friend, neutral gentle expression
Subject: teenage girl childhood friend, black medium-length hair with soft straight bangs and slightly inward curling ends, warm brown eyes, approachable smile held back into a neutral expression, cute everyday student feeling
Identity lock: she is the same person as the mentor outfit version; preserve the same face shape, eye shape, hair texture, bangs structure, apparent age, and gentle reliable core personality
Style/medium: polished galgame character portrait, clean cel shading, expressive eyes, refined line art
Composition/framing: RGBA transparent background, unused pixels alpha 0, 1024x1536 portrait canvas, half-body, front three-quarter view, centered consistent standing pose, no cropped body
Lighting/mood: soft warm VN portrait lighting, familiar and comforting
Color palette: modest school or casual outfit colors, soft whites and muted warm accents, natural black hair
Constraints: transparent background, no text, no logo, no watermark, no white matte, no dirty alpha edge; outfit and accessories should feel intentionally simpler than the mentor form
Avoid: copying any reference character, fantasy armor, overly mature styling, background residue requiring cutout
```

### 5.5 mentor_neutral.png

```text
Use case: stylized-concept
Asset type: visual novel half-body character portrait
Primary request: original anime visual novel half-body portrait of Aoba wearing her mentor magic-swordswoman outfit, neutral calm expression
Subject: the exact same person as Aoba in a different outfit and accessories, same black medium-length hair foundation but styled more controlled, same warm brown eyes with a calmer sharper gaze, reliable mentor aura without changing her apparent age, fantasy magic swordswoman outfit with restrained black-silver fabric, subtle cyan magic ornament, small deliberate accessories that differ from her daily form
Identity lock: she is the same person and same apparent age as Aoba; preserve the same face shape, eye shape, hair texture, bangs structure, head-to-body proportion, skin softness, and gentle reliable core personality while changing only posture, styling, clothing, ornaments, and expression control
Style/medium: polished galgame character portrait, clean cel shading, expressive eyes, refined line art
Composition/framing: RGBA transparent background, unused pixels alpha 0, 1024x1536 portrait canvas, half-body, front three-quarter view, centered consistent standing pose, no cropped body
Lighting/mood: cool soft VN portrait lighting with a controlled heroic edge
Color palette: black and silver outfit, subtle cyan magical accents, warm skin tones, natural black hair
Constraints: transparent background, no text, no logo, no watermark, no white matte, no dirty alpha edge; differences from Aoba must come from clothing, ornaments, posture, expression control, and magic-swordswoman details, not from age or character redesign
Avoid: making her look older than Aoba, making her look like an unrelated character, implying a different life stage, overly revealing costume, heavy armor, copying any reference character, background residue requiring cutout
```

## 6. 后续验收标准

- 背景能直接用于 1920x1080 VN 场景，不含文字、Logo、人物或明显商业可识别元素。
- 立绘必须是真 RGBA 透明 PNG，无用区域 alpha 为 0；角色边缘干净，无彩边、白边、灰边、白色 matte 或半透明残底。
- `aoba_neutral.png` 和 `mentor_neutral.png` 一眼能看出是同一个人，但打扮、装饰和身份呈现明显不同。
- 三张立绘的画布、人物高度、站位、视角一致，方便后续在 VN 系统里切换。
- 如果小样通过，继续按同一锚点扩展 Batch 01 的表情差分和剩余背景。
