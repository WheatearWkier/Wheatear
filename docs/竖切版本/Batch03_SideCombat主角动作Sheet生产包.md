# Batch03 SideCombat 主角动作 Sheet 生产包

更新时间：2026-07-30

本包用于把正式横板战斗 `SideCombat` 的主角动作从逐帧占位素材升级为可长期使用的横向 sprite strip。目标是先把玩家角色的动作轮廓、节奏和打击姿态统一，再继续做小怪、Boss、场景和 VFX。

风格参考 `docs/参考分析/美术参考/战斗界面的风格像这张图，要明亮爽快的二次元风格，来自游戏艾尔之光.png` 的“明亮爽快、二次元横板动作、技能栏强反馈”气质；不要复刻原图角色、界面、地图、文字、技能图标或任何商业可识别元素。比例参考 DNF 类横板动作角色，不是马里奥式平台跳跃，不是像素小人。

## 1. 本批原则

- 所有输出文件使用正式 `_strip.png` 命名，放入 `WheatearEditor/assets/vertical_slice/side_combat/sheets/`。
- 每张 sheet 只放一个动作，横向一行排列，不混入其他动作。
- 每格 cell 固定 `384x384`，透明背景，脚底 baseline 约 `y=320`，pivot 为底部中心。
- 帧间角色比例、脸、发型、服装、剑的主结构必须一致，只允许按动作发生自然形变。
- 角色是主角的异世界初期战斗形态：现实校服受损后混入黑银魔剑士元素，不是全套重甲，不是完全换人。
- 技能 VFX 会在 Batch06 单独生产。本批可以保留轻微剑光和施法光，但不要把大范围特效当作角色动作主体。
- 旧运行时帧序列 `protag_*_01.png` 等可作为动作意图参考，但正式交付以本批 `_strip.png` 为准。

## 2. 输入锚点

如果生成工具支持参考图或编辑模式，请优先使用：

| 用途 | 锚点文件 |
| --- | --- |
| 主角脸和校服气质 | `WheatearEditor/assets/vertical_slice/vn/portraits/protag_school_neutral.png` |
| 主角警觉/战斗表情 | `WheatearEditor/assets/vertical_slice/vn/portraits/protag_school_alert.png` |
| 当前横板主角占位 | `WheatearEditor/assets/vertical_slice/side_combat/characters/protag_magic_swordsman.png` |

如果参考图和动作 sheet 生成发生冲突，以横板侧视动作可读性优先，但不要把主角改成无关角色。

## 3. 通用负面要求

所有提示词追加：

```text
no text, no letters, no numbers, no logo, no watermark, no signature, no copyright character, no recognizable franchise design, no UI, no background, no floor shadow baked into the sheet, no visible cell borders, no inconsistent cell size, no cropped body, no changing costume between frames, no changing face identity, no pixel-art style, no chibi mascot proportions
```

## 4. 规格总纲

- 画布：每张 strip 高度固定 `384`，宽度为 `frameCount * 384`。
- 透明：必须是真透明 PNG，不能用白底、绿底、黑底替代。
- 朝向：默认角色面向右侧，适合当前横板战斗从左向右攻击。
- 站位：脚底落在同一 baseline，跳跃和被击飞动作允许脚离开 baseline，但角色中心和缩放仍要稳定。
- 轮廓：大剑/魔剑动作要清楚，攻击帧要有可读 silhouette，不要被光效糊成一团。
- 节奏：每个动作需要有起手、动作峰值、收招或恢复帧，不能只是同一姿势轻微晃动。

## 5. 本批资产清单

| 动作 ID | 正式输出文件 | 帧数 | FPS | 输出尺寸 | 说明 |
| --- | --- | ---: | ---: | --- | --- |
| idle | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_idle_strip.png` | 8 | 12 | 3072x384 | 呼吸待机，魔剑微光 |
| run | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_run_strip.png` | 10 | 18 | 3840x384 | 横向奔跑，脚步清楚 |
| jump_start | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_jump_start_strip.png` | 4 | 18 | 1536x384 | 起跳发力 |
| jump_loop | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_jump_loop_strip.png` | 4 | 12 | 1536x384 | 空中滞留 |
| fall | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_fall_strip.png` | 4 | 12 | 1536x384 | 下落姿态 |
| land | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_land_strip.png` | 4 | 18 | 1536x384 | 落地缓冲 |
| basic1 | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_basic1_strip.png` | 7 | 24 | 2688x384 | 第一段平砍 |
| basic2 | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_basic2_strip.png` | 7 | 24 | 2688x384 | 第二段反手斩 |
| basic3 | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_basic3_strip.png` | 9 | 24 | 3456x384 | 第三段收尾斩 |
| air_basic | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_air_basic_strip.png` | 7 | 24 | 2688x384 | 空中跳斩 |
| launcher | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_launcher_strip.png` | 9 | 24 | 3456x384 | S+J 裂空上挑 |
| air_chase | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_air_chase_strip.png` | 8 | 24 | 3072x384 | 断限后空中追击 |
| magic_bolt | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_magic_bolt_strip.png` | 9 | 20 | 3456x384 | 魔法弹施法 |
| ally_support | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_ally_support_strip.png` | 8 | 18 | 3072x384 | 借队友力量支援 |
| break_limit | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_break_limit_strip.png` | 12 | 24 | 4608x384 | 断限追击，魔法阵碎裂感 |
| hurt | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_hurt_strip.png` | 5 | 18 | 1920x384 | 受击 |
| launched | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_launched_strip.png` | 4 | 12 | 1536x384 | 被击飞 |
| knockdown | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_knockdown_strip.png` | 5 | 12 | 1920x384 | 倒地 |
| recover | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_recover_strip.png` | 6 | 16 | 2304x384 | 起身 |
| dead | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_dead_strip.png` | 8 | 12 | 3072x384 | 战败 |

## 6. 主角通用 Prompt

每个动作分别生成一次，将 `[ACTION REQUEST]`、`[FRAME COUNT]` 和 `[TOTAL WIDTH]` 替换为对应动作要求。

```text
Use case: stylized-concept
Asset type: 2D side-scrolling action game character sprite strip
Primary request: original anime side-view magic swordsman protagonist sprite strip, [ACTION REQUEST]
Subject: young male protagonist, damaged modern school uniform mixed with restrained black-silver fantasy magic swordsman details, dark hair, readable heroic silhouette, one-handed magic sword with subtle cyan glow
Style/medium: bright polished anime action game sprite, DNF-like side-view proportions, clean cel shading, crisp outline, smooth high-frame action, not pixel art
Composition/framing: transparent background, one horizontal row, exactly [FRAME COUNT] frames, equal 384x384 cells, total canvas [TOTAL WIDTH]x384, feet aligned to baseline y around 320 where applicable, character faces right
Animation requirements: clear anticipation, active pose, and recovery where relevant; consistent body scale and costume across every frame; no visible grid lines
Lighting/mood: energetic fantasy combat, readable over dark forest background and bright UI effects
Color palette: dark school uniform base, black and silver fantasy accents, restrained cyan magic glow, warm skin tones
Constraints: transparent PNG, no text, no UI, no background, no floor, no logo, no watermark, no copied franchise character
Avoid: chibi proportions, platformer mascot style, heavy armor redesign, changing age or face, oversized effects hiding the character, cropped sword or body
```

## 7. 动作替换文本

将下面每一行作为 `[ACTION REQUEST]`：

```text
protag_idle_strip.png: idle breathing loop, calm combat-ready stance, sword held low with subtle cyan pulse, hair and coat edge lightly moving
protag_run_strip.png: fast side-scrolling run cycle, forward lean, clear foot contacts, sword carried safely behind or beside the body
protag_jump_start_strip.png: jump anticipation and launch, knees bend then push upward, coat and hair react to upward force
protag_jump_loop_strip.png: airborne hover loop, body tucked slightly, sword balanced, readable mid-air control pose
protag_fall_strip.png: falling pose loop, body angled downward, coat and hair pulled upward by air, ready to land
protag_land_strip.png: landing recovery, feet touch down, knees compress, sword stabilizes, small motion but no separate dust cloud
protag_basic1_strip.png: first grounded slash, quick horizontal cut from ready stance, modest sword trail, clean startup active recovery
protag_basic2_strip.png: second grounded combo slash, reverse cut with stronger body rotation, follows naturally after basic1
protag_basic3_strip.png: third grounded finisher slash, wider finishing arc, stronger step forward, recovery pose readable
protag_air_basic_strip.png: aerial slash while suspended, compact jump attack, slight forward momentum, can imply light launcher continuation
protag_launcher_strip.png: upward launcher slash, crouch then rising diagonal cut, sword points up during active frames, strong vertical energy
protag_air_chase_strip.png: airborne pursuit attack after break limit, fast forward dash in air, aggressive slash posture, controlled momentum
protag_magic_bolt_strip.png: magic bolt casting animation, gathers cyan magic at sword tip then release pose, projectile itself not included
protag_ally_support_strip.png: support call animation, protagonist braces and channels ally power through sword, no second character body in the sheet
protag_break_limit_strip.png: dramatic break limit chase animation, magic circle fragments and cyan surge around the protagonist, fast dash strike, readable character silhouette
protag_hurt_strip.png: short hurt reaction, torso recoils, face tense, sword arm pulled back, returns toward controllable posture
protag_launched_strip.png: being launched upward or backward, body lifted from ground, limbs and coat trailing, no floor contact
protag_knockdown_strip.png: fall to ground and downed pose, readable non-graphic defeat state, body stays within cell
protag_recover_strip.png: recover from knockdown, hand supports body, rises back into guarded stance
protag_dead_strip.png: final defeat animation, collapses and remains down, non-graphic, no blood, no gore
```

## 8. sheet_params.yaml

推荐随 PNG 一起交付：

```yaml
protag_idle_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 8
  frameRate: 12
  pivot: bottom_center
  baselineY: 320

protag_run_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 10
  frameRate: 18
  pivot: bottom_center
  baselineY: 320

protag_jump_start_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 4
  frameRate: 18
  pivot: bottom_center
  baselineY: 320

protag_jump_loop_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 4
  frameRate: 12
  pivot: bottom_center
  baselineY: 320

protag_fall_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 4
  frameRate: 12
  pivot: bottom_center
  baselineY: 320

protag_land_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 4
  frameRate: 18
  pivot: bottom_center
  baselineY: 320

protag_basic1_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 7
  frameRate: 24
  pivot: bottom_center
  baselineY: 320

protag_basic2_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 7
  frameRate: 24
  pivot: bottom_center
  baselineY: 320

protag_basic3_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 9
  frameRate: 24
  pivot: bottom_center
  baselineY: 320

protag_air_basic_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 7
  frameRate: 24
  pivot: bottom_center
  baselineY: 320

protag_launcher_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 9
  frameRate: 24
  pivot: bottom_center
  baselineY: 320

protag_air_chase_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 8
  frameRate: 24
  pivot: bottom_center
  baselineY: 320

protag_magic_bolt_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 9
  frameRate: 20
  pivot: bottom_center
  baselineY: 320

protag_ally_support_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 8
  frameRate: 18
  pivot: bottom_center
  baselineY: 320

protag_break_limit_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 12
  frameRate: 24
  pivot: bottom_center
  baselineY: 320

protag_hurt_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 5
  frameRate: 18
  pivot: bottom_center
  baselineY: 320

protag_launched_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 4
  frameRate: 12
  pivot: bottom_center
  baselineY: 320

protag_knockdown_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 5
  frameRate: 12
  pivot: bottom_center
  baselineY: 320

protag_recover_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 6
  frameRate: 16
  pivot: bottom_center
  baselineY: 320

protag_dead_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 8
  frameRate: 12
  pivot: bottom_center
  baselineY: 320
```

## 9. 推荐交付结构

```text
Batch03_SideCombat_Protag/
  protag_idle_strip.png
  protag_run_strip.png
  protag_jump_start_strip.png
  protag_jump_loop_strip.png
  protag_fall_strip.png
  protag_land_strip.png
  protag_basic1_strip.png
  protag_basic2_strip.png
  protag_basic3_strip.png
  protag_air_basic_strip.png
  protag_launcher_strip.png
  protag_air_chase_strip.png
  protag_magic_bolt_strip.png
  protag_ally_support_strip.png
  protag_break_limit_strip.png
  protag_hurt_strip.png
  protag_launched_strip.png
  protag_knockdown_strip.png
  protag_recover_strip.png
  protag_dead_strip.png
  sheet_params.yaml
```

## 10. 接入动作

生成后接入当前 demo 的动作是：

1. 把 PNG 放入 `WheatearEditor/assets/vertical_slice/side_combat/sheets/`。
2. 使用 Sprite Sheet Picker 或导入脚本按 `384x384` cell 切成 AnimationClip。
3. 将 `side_combat_tuning.yaml` 和场景里的主角动画引用迁移到正式 sheet/clip。
4. 保留旧逐帧序列只作为迁移对照，确认新 sheet 全部接入后删除旧运行时引用。
5. 重新打包 Sandbox，检查 `content.wtpack` 包含 `side_combat/sheets/protag_*_strip.png`。

## 11. 验收标准

- 每张 PNG 尺寸必须等于 `384 * frameCount` by `384`。
- 透明背景真实有效，无白底、黑底、绿底、脏边或可见 cell 分隔线。
- 主角每个动作里脸、发型、校服破损结构、魔剑外形和颜色体系一致。
- 动作轮廓在 1x 和游戏内缩放后都能读，攻击动作有明确起手、命中峰值和收招。
- `run`、`jump_start`、`land`、`hurt`、`recover` 不能像静态立绘平移，必须有身体重心变化。
- `magic_bolt` 和 `break_limit` 可以有轻微角色附着光效，但不能替代 Batch06 的独立 VFX。
- 不出现 UI、背景、文字、Logo、参考游戏角色或商业可识别元素。
