# Batch01B VN 当前可见表情与场景生产包

更新时间：2026-07-30

本包承接 `Batch01A_VN风格小样生产包.md`。Batch01A 锁定了 VN 基础画风；本批补齐当前竖切马上会看到的 VN 表情差分和系统/预告背景，同时开始执行正式资源命名迁移。

## 1. 本批原则

- 本批开始不再按早期 demo 文件名交付。所有新素材都使用正式规范名。
- 工程里的 `.wt` 场景、`.vn` 脚本、`.wts` 事件脚本和打包依赖要同步改引用到正式名。
- 旧名只作为迁移映射，不能作为后续素材生产名或长期兼容名。
- 表情差分必须以 Batch01A 的 neutral 图为锚点，不重新设计角色。
- 青梅 `aoba_*` 和导师 `mentor_*` 是同一个人、同一年龄感，不是不同阶段。区别只来自不同打扮、装饰、妆发整理方式、站姿和身份呈现。
- 立绘差分只改表情和必要的细微姿态，画布、人物高度、头身比例、服装结构、发型大轮廓必须一致。

## 2. 旧名到正式名迁移表

当前工程若还引用旧名，必须迁移到右侧正式名：

| 旧 demo 名 | 正式规范名 |
| --- | --- |
| `vn/backgrounds/school_road.png` | `vn/backgrounds/bg_modern_schoolroad_morning.png` |
| `vn/backgrounds/isekai_forest.png` | `vn/backgrounds/bg_otherworld_forest_wake.png` |
| `vn/backgrounds/forest_camp.png` | `vn/backgrounds/bg_forest_camp_night.png` |
| `vn/backgrounds/chapter3_road.png` | `vn/backgrounds/bg_chapter3_road.png` |
| `vn/portraits/hero_neutral.png` | `vn/portraits/protag_school_neutral.png` |
| `vn/portraits/hero_happy.png` | `vn/portraits/protag_school_relieved.png` |
| `vn/portraits/hero_serious.png` | `vn/portraits/protag_school_alert.png` |
| `vn/portraits/hero_surprised.png` | `vn/portraits/protag_school_shocked.png` |
| `vn/portraits/hero_thinking.png` | `vn/portraits/protag_school_skeptical.png` |
| `vn/portraits/aoba_happy.png` | `vn/portraits/aoba_smile.png` |
| `vn/portraits/aoba_serious.png` | `vn/portraits/aoba_worried.png` |
| `vn/portraits/aoba_surprised.png` | `vn/portraits/aoba_surprised.png` |
| `vn/portraits/aoba_thinking.png` | `vn/portraits/aoba_hesitate.png` |
| `vn/portraits/mentor_happy.png` | `vn/portraits/mentor_slight_smile.png` |
| `vn/portraits/mentor_serious.png` | `vn/portraits/mentor_serious.png` |
| `vn/portraits/mentor_surprised.png` | `vn/portraits/mentor_alert.png` |
| `vn/portraits/mentor_thinking.png` | `vn/portraits/mentor_calm.png` |

迁移完成后，场景和脚本中不应再出现 `school_road.png`、`isekai_forest.png`、`forest_camp.png`、`chapter3_road.png`、`hero_*` 这些旧 demo 命名。

## 3. 输入锚点

如果生成工具支持参考图或编辑模式，请使用正式命名后的锚点：

| 角色/场景 | 锚点文件 |
| --- | --- |
| 主角校服 | `WheatearEditor/assets/vertical_slice/vn/portraits/protag_school_neutral.png` |
| 青梅日常打扮 | `WheatearEditor/assets/vertical_slice/vn/portraits/aoba_neutral.png` |
| 青梅导师/魔剑士打扮 | `WheatearEditor/assets/vertical_slice/vn/portraits/mentor_neutral.png` |
| 现实上学路画风 | `WheatearEditor/assets/vertical_slice/vn/backgrounds/bg_modern_schoolroad_morning.png` |
| 异世界黑林画风 | `WheatearEditor/assets/vertical_slice/vn/backgrounds/bg_otherworld_forest_wake.png` |

如果工程尚未完成重命名，可临时用旧文件作为参考图输入，但输出文件名和最终引用必须使用正式名。

## 4. 通用负面要求

所有提示词追加：

```text
no text, no logo, no watermark, no signature, no UI letters, no copyright character, no recognizable franchise design, no cropped body, no blurry edge, no inconsistent frame size, no changing costume between variants, no changing apparent age, no redesigning the character
```

## 5. 本批资产清单

### 5.1 背景

| 资产 | 正式输出文件 | 规格 | 说明 |
| --- | --- | --- | --- |
| 据点营地夜晚 | `WheatearEditor/assets/vertical_slice/vn/backgrounds/bg_forest_camp_night.png` | 1920x1080，不透明 PNG | 主菜单、Hub、系统页通用背景 |
| 第三章边境道路 | `WheatearEditor/assets/vertical_slice/vn/backgrounds/bg_chapter3_road.png` | 1920x1080，不透明 PNG | 第三章预告 |
| 上学路不安版 | `WheatearEditor/assets/vertical_slice/vn/backgrounds/bg_modern_schoolroad_unease.png` | 1920x1080，不透明 PNG | 同一路段气氛转不安 |
| 黑熊战后森林 | `WheatearEditor/assets/vertical_slice/vn/backgrounds/bg_forest_after_bear.png` | 1920x1080，不透明 PNG | 碎枝、战斗痕迹、魔光 |

### 5.2 表情差分

| 角色 | 正式输出文件 | 表情方向 | 锚点 |
| --- | --- | --- | --- |
| 主角 | `vn/portraits/protag_school_relieved.png` | 松一口气的轻微笑意，不要夸张 | `protag_school_neutral.png` |
| 主角 | `vn/portraits/protag_school_alert.png` | 警觉、进入战斗前的认真 | `protag_school_neutral.png` |
| 主角 | `vn/portraits/protag_school_shocked.png` | 被突发事件震住，眼睛睁大 | `protag_school_neutral.png` |
| 主角 | `vn/portraits/protag_school_skeptical.png` | 吐槽/思考，略微怀疑人生 | `protag_school_neutral.png` |
| 青梅 | `vn/portraits/aoba_smile.png` | 熟悉亲近的开朗笑容 | `aoba_neutral.png` |
| 青梅 | `vn/portraits/aoba_worried.png` | 察觉异常后的担忧认真 | `aoba_neutral.png` |
| 青梅 | `vn/portraits/aoba_surprised.png` | 穿越/突发危险时的惊讶 | `aoba_neutral.png` |
| 青梅 | `vn/portraits/aoba_hesitate.png` | 温柔犹豫、欲言又止 | `aoba_neutral.png` |
| 导师 | `vn/portraits/mentor_slight_smile.png` | 克制的浅笑，仍是导师打扮 | `mentor_neutral.png` |
| 导师 | `vn/portraits/mentor_serious.png` | 训导/战斗指挥时的冷静严肃 | `mentor_neutral.png` |
| 导师 | `vn/portraits/mentor_alert.png` | 轻微惊讶但不失控 | `mentor_neutral.png` |
| 导师 | `vn/portraits/mentor_calm.png` | 冷静分析、压低情绪 | `mentor_neutral.png` |

## 6. 背景生成提示词

### 6.1 bg_forest_camp_night.png

```text
Use case: stylized-concept
Asset type: visual novel background
Primary request: original anime visual novel background of a quiet otherworld forest camp at night
Scene/backdrop: small safe camp in a dark fantasy forest, simple tents, low campfire, wooden crates, hanging lanterns, mossy ground, faint cyan magical plants in the background, no characters
Style/medium: polished high-resolution galgame background art
Composition/framing: 1920x1080 16:9 landscape, readable depth, center-left campfire focal point, enough open space for VN portraits and UI overlays
Lighting/mood: warm campfire light balanced with cool cyan forest magic glow, safe but still mysterious
Color palette: deep forest greens, warm amber firelight, cyan magical accents, restrained dark glass-friendly contrast
Constraints: no characters, no monsters, no readable text, no logos, no watermark
Avoid: horror gore, overly dark unreadable scene, cluttered props blocking portraits, realistic photo texture
```

### 6.2 bg_chapter3_road.png

```text
Use case: stylized-concept
Asset type: visual novel background
Primary request: original anime visual novel background of a border road teasing chapter three
Scene/backdrop: wide fantasy border road leaving the black forest, distant ruined watchtower, low hills, pale morning mist, traces of magic on old stones, no characters
Style/medium: polished high-resolution galgame background art
Composition/framing: 1920x1080 16:9 landscape, cinematic road leading into distance, clear foreground/midground/background layers
Lighting/mood: hopeful but uncertain, cool morning light breaking through clouds, sense of journey continuing
Color palette: desaturated greens and stone grays with soft gold sunlight and subtle cyan magic traces
Constraints: no characters, no vehicles, no readable text, no logos, no watermark
Avoid: modern city elements, over-dramatic apocalypse, cluttered composition, realistic photo texture
```

### 6.3 bg_modern_schoolroad_unease.png

```text
Use case: stylized-concept
Asset type: visual novel background
Primary request: original anime visual novel background of the same modern school road from the morning scene, but with an uneasy atmosphere
Input images: Image 1: bg_modern_schoolroad_morning.png as layout and style anchor if available
Scene/backdrop: same quiet residential school road composition, no people, subtle suspicious empty feeling, slightly colder shadows, faint wind movement in trees
Style/medium: polished high-resolution galgame background art, consistent with the existing bg_modern_schoolroad_morning.png
Composition/framing: 1920x1080 16:9 landscape, preserve the general road angle and readable depth from the morning version
Lighting/mood: morning light turns tense and colder, a small sense that something is wrong without becoming horror
Color palette: muted sky blue, cooler green shadows, pale sunlight with slight gray cast
Constraints: no characters, no attacker silhouette, no readable text, no logos, no watermark
Avoid: night scene, blood, obvious horror, completely different location
```

### 6.4 bg_forest_after_bear.png

```text
Use case: stylized-concept
Asset type: visual novel background
Primary request: original anime visual novel background of the black forest after a bear battle
Input images: Image 1: bg_otherworld_forest_wake.png as style anchor if available
Scene/backdrop: same otherworld black forest mood, broken branches, torn earth, scattered leaves, claw marks on tree roots, fading cyan magic sparks, no characters and no visible corpse
Style/medium: polished high-resolution galgame background art, consistent with the existing bg_otherworld_forest_wake.png
Composition/framing: 1920x1080 16:9 landscape, readable battle aftermath in foreground while preserving space for VN portraits
Lighting/mood: tense aftermath, cool cyan magical residue, quiet after violence but not graphic
Color palette: dark forest greens, earth browns, cyan magic glow, small warm highlights
Constraints: no characters, no monsters, no gore, no readable text, no logos, no watermark
Avoid: horror gore, overly dark unreadable scene, changing the forest into a different biome
```

## 7. 表情差分生成提示词

### 7.1 主角 protag_school_* 差分通用 Prompt

每个文件分别生成一次，将 `[EXPRESSION REQUEST]` 替换为表情方向。

```text
Use case: identity-preserve
Asset type: visual novel half-body character portrait expression variant
Input images: Image 1: protag_school_neutral.png as the exact character anchor
Primary request: create a new transparent PNG expression variant of the same young male protagonist: [EXPRESSION REQUEST]
Subject: same teenage boy protagonist, same face, same hair, same school uniform, same canvas position, same apparent age, same body proportions
Style/medium: same polished galgame character portrait style as the anchor, clean cel shading, refined line art
Composition/framing: transparent background, same 1024x1536-style portrait canvas, half-body, front three-quarter view, centered consistent standing pose
Constraints: change only facial expression and tiny natural head/shoulder nuance; preserve outfit, hairstyle, pose scale, lighting direction, and silhouette; no text, no logo, no watermark
Avoid: redesigning the character, changing costume, changing age, changing camera angle, cropped body
```

主角表情替换：

```text
protag_school_relieved.png: a restrained relieved slight smile, still a little tired but warmer
protag_school_alert.png: alert and determined, brows focused, ready to fight
protag_school_shocked.png: shocked by sudden danger, eyes widened, mouth slightly open
protag_school_skeptical.png: skeptical thinking expression, subtle comedic doubt, not exaggerated
```

### 7.2 青梅 aoba_* 差分通用 Prompt

```text
Use case: identity-preserve
Asset type: visual novel half-body character portrait expression variant
Input images: Image 1: aoba_neutral.png as the exact character anchor
Primary request: create a new transparent PNG expression variant of the same Aoba in daily childhood-friend outfit: [EXPRESSION REQUEST]
Subject: same teenage girl Aoba, same face shape, same warm brown eyes, same black medium-length hair, same bangs, same daily outfit, same apparent age, same body proportions
Identity lock: this is the same person as mentor_neutral.png; keep apparent age and facial structure identical, only daily outfit and accessories differ from the mentor version
Style/medium: same polished galgame character portrait style as the anchor, clean cel shading, expressive eyes
Composition/framing: transparent background, same 1024x1536-style portrait canvas, half-body, front three-quarter view, centered consistent standing pose
Constraints: change only facial expression and tiny natural head/shoulder nuance; preserve outfit, hairstyle, pose scale, lighting direction, and silhouette; no text, no logo, no watermark
Avoid: redesigning her, making her look older or younger, changing costume, copying reference characters, cropped body
```

青梅表情替换：

```text
aoba_smile.png: bright familiar smile, warm and close, daily childhood-friend charm
aoba_worried.png: worried and serious after noticing something wrong, brows slightly tense
aoba_surprised.png: startled by sudden danger or another world, eyes widened but still recognizable
aoba_hesitate.png: gentle hesitation, conflicted but kind, as if hiding something
```

### 7.3 导师 mentor_* 差分通用 Prompt

```text
Use case: identity-preserve
Asset type: visual novel half-body character portrait expression variant
Input images: Image 1: mentor_neutral.png as the exact character anchor; Image 2: aoba_neutral.png only for same-person facial identity check if available
Primary request: create a new transparent PNG expression variant of Aoba wearing her mentor magic-swordswoman outfit: [EXPRESSION REQUEST]
Subject: the exact same person as Aoba, same apparent age, same face shape, same warm brown eyes, same black medium-length hair foundation, same mentor outfit and ornaments, same controlled posture
Identity lock: do not make her older, do not imply a different life stage, do not redesign her face; differences from daily Aoba are only outfit, accessories, styling, posture, and expression control
Style/medium: same polished galgame character portrait style as the anchor, clean cel shading, refined line art
Composition/framing: transparent background, same 1024x1536-style portrait canvas, half-body, front three-quarter view, centered consistent standing pose
Constraints: change only facial expression and tiny natural head/shoulder nuance; preserve outfit, ornaments, hairstyle, pose scale, lighting direction, and silhouette; no text, no logo, no watermark
Avoid: making her look older than Aoba, changing costume, heavy armor redesign, unrelated character face, cropped body
```

导师表情替换：

```text
mentor_slight_smile.png: very slight restrained smile, calm and reliable, not playful
mentor_serious.png: composed command expression, focused eyes, battle instructor aura
mentor_alert.png: subtle controlled surprise, widened eyes but still calm
mentor_calm.png: quiet analytical expression, emotionally restrained, looking ahead
```

## 8. 推荐交付结构

```text
Batch01B_VN_CurrentVisible/
  bg_forest_camp_night.png
  bg_chapter3_road.png
  bg_modern_schoolroad_unease.png
  bg_forest_after_bear.png
  protag_school_relieved.png
  protag_school_alert.png
  protag_school_shocked.png
  protag_school_skeptical.png
  aoba_smile.png
  aoba_worried.png
  aoba_surprised.png
  aoba_hesitate.png
  mentor_slight_smile.png
  mentor_serious.png
  mentor_alert.png
  mentor_calm.png
```

生成后接入当前 demo 的动作不是“覆盖旧名”，而是：

1. 把 PNG 放入正式规范路径。
2. 批量更新 `.wt`、`.vn`、`.wts` 和生成脚本里的旧资源引用。
3. 重新打包 Sandbox。
4. 搜索旧名确认引用清零。

## 9. 验收标准

- 背景无人物、无文字、无 Logo，能承载 VN 对话框和立绘。
- `bg_forest_camp_night.png` 要适合主菜单和据点/系统页面长期复用，画面不能过满。
- `bg_chapter3_road.png` 要有继续旅行的预告感，但不要喧宾夺主。
- 表情差分和 neutral 图叠放时，脸部位置、身体高度、服装边缘和画布位置基本一致。
- 青梅和导师必须是同一人同年龄感：只有打扮、装饰、气质控制不同，不能生成成不同角色。
- 所有立绘必须为透明 PNG；如果生成工具不能直接输出透明，请先用纯色平面背景生成，再抠成透明，确保边缘无明显色边。
