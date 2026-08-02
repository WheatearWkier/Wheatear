# Batch03 SideCombat 主角动作 Sheet 生产包

更新时间：2026-07-31

本包用于把正式横板战斗 `SideCombat` 的主角动作从逐帧占位素材升级为可长期使用的高清像素风动画源帧，并在验收后拼接成横向 sprite strip。目标是先把玩家角色的动作轮廓、节奏和打击姿态统一，再继续做小怪、Boss、场景和 VFX。

风格参考 `docs/参考分析/美术参考/战斗界面的风格像这张图，要明亮爽快的二次元风格，来自游戏艾尔之光.png` 和用户补充参考图里的“DNF 类横板动作像素密度、清晰黑边/色块、带纵深横板房间、技能栏强反馈”气质；不要复刻原图角色、界面、地图、文字、技能图标或任何商业可识别元素。比例参考 DNF 类横板动作角色，不是马里奥式平台跳跃，也不是低清 16-bit 小人。

## 1. 本批原则

- AI 生成阶段必须先交付单帧源文件：`WheatearEditor/assets/vertical_slice/source_frames/protag_<action>/protag_<action>_000.png`、`001.png` 等。
- 每个单帧源文件都是固定 canvas，尺寸等于该动作的 cell profile；不要让 AI 直接生成整张横向 strip。
- 正式 `_strip.png` 只作为源帧验收通过后的拼接结果，放入 `WheatearEditor/assets/vertical_slice/side_combat/sheets/`。
- 每张最终 sheet 只放一个动作，横向一行排列，不混入其他动作。
- 每个动作使用固定 cell profile，不能在同一张 sheet 内出现大小不同的子格。默认从 `512x512` 起步，大幅挥剑动作使用更宽或更高的动作专用 cell。
- 所有 sheet 必须是 **RGBA PNG 真透明**：无用区域 alpha 为 0，禁止白底、灰底、黑底、绿底、棋盘格底、白色 matte、半透明残底和脏边。
- 帧间角色比例、脸、发型、服装、剑的主结构必须一致，只允许按动作发生自然形变。
- 角色是主角的异世界初期战斗形态：现实校服受损后混入黑银魔剑士元素，不是全套重甲，不是完全换人。
- 技能 VFX 会在 Batch06 单独生产。本批可以保留轻微贴身剑光和施法光，但不要把大范围剑气、冲刺拖尾、魔法阵或爆光当作角色动作主体。
- 旧运行时帧序列 `protag_*_01.png` 等可作为动作意图参考；正式生产以本批 `source_frames` 为主，正式接入以拼接后的 `_strip.png` 为准。

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
no text, no letters, no numbers, no logo, no watermark, no signature, no copyright character, no recognizable franchise design, no UI, no background, no floor shadow baked into the sheet, no visible cell borders, no inconsistent cell size, no inconsistent character scale, no cropped body, no cropped sword, no body part crossing into neighboring cells, no changing costume between frames, no changing face identity, no white background, no gray background, no checkerboard background, no opaque background rectangle, no white matte, no dirty alpha edge, no chibi mascot proportions, no low-resolution tiny pixel character
```

## 4. 规格总纲

- 源帧画布：每个 `source_frames/<clip>/<clip>_<frame3>.png` 的尺寸必须等于该 clip 的 `cellWidth x cellHeight`。
- 最终 strip：宽度必须等于 `cellWidth * frameCount`，高度必须等于 `cellHeight`；同一张 sheet 内 cell 尺寸绝对一致，且必须由已验收源帧拼接得到。
- 同一 clip 的 `cellWidth/cellHeight` 取该 clip 全部帧里最大的身体 + 魔剑本体 + 披风/腿部动作包围盒，再加安全边距。定好 profile 后，该 clip 从第 0 帧到最后一帧都使用同一 canvas；不能站姿帧小、挥剑峰值帧大，也不能为了让某帧塞进小格而缩小角色。
- 透明：必须是真 RGBA 透明 PNG，所有无用像素 alpha 为 0，不能用白底、绿底、黑底、灰底或棋盘格底替代。
- 朝向：默认角色面向右侧，适合当前横板战斗从左向右攻击。
- 站位：脚底落在同一 baseline，跳跃和被击飞动作允许脚离开 baseline，但角色中心和缩放仍要稳定。
- 轮廓：大剑/魔剑动作要清楚，攻击帧要有可读 silhouette，不要被光效糊成一团。
- 节奏：每个动作需要有起手、动作峰值、收招或恢复帧，不能只是同一姿势轻微晃动。
- 碰撞：图片 canvas 或 strip 尺寸不等于碰撞体。移动碰撞盒、受击框和攻击框由引擎/YAML 独立定义，不能因为某个动作 cell 更宽就扩大角色行走碰撞。

### 4.1 三层动作包围盒规则

魔剑是长武器，不能靠提示词祈祷它不被切掉。每个动作都按三层来设计：

| 层级 | 作用 | 规则 |
| --- | --- | --- |
| `bodyBox` | 主角身体本体范围 | 身体高度、头身比例、脸部大小、脚底 baseline 和 pivot 稳定，不随 cell 变大而缩小 |
| `weaponActionEnvelope` | 剑尖、腿、披风、前倾身体能扫到的最大范围 | 按动作峰值预留透明边，宽挥剑/前冲动作可使用更大 cell |
| `detachedVfx` | 剑气、冲击波、魔法阵、拖尾、爆光 | 拆到 Batch06 VFX sheet，用挂点和 offset 叠到角色动作上 |

处理顺序：

1. 先判断是否只是武器本体或身体姿态超出。是的话扩大该动作的 cell profile。
2. 如果超出的是剑气、冲刺残影、魔法阵、爆光或冲击波，拆到 VFX，角色 sheet 只保留身体和武器本体。
3. 不允许裁切，也不允许让剑尖、腿、披风或头发延伸到隔壁 cell。
4. 不允许为了塞进 cell 而让某一帧角色突然缩小；不同 cell profile 的动作也必须通过 `bodyBox`、`pivot`、`baselineY` 和 `renderOffset` 对齐。

### 4.2 主角动作 cell profile

| Profile | Cell | Baseline | 用途 |
| --- | --- | ---: | --- |
| `body_512` | 512x512 | 430 | 待机、跑步、跳跃、下落、落地、施法、受击、起身 |
| `slash_640` | 640x512 | 430 | 普攻 1/2、空中斩、支援呼应，给魔剑横向挥动留空间 |
| `slash_heavy_768` | 768x512 | 430 | 第三段收尾斩、前倾幅度更大的横向攻击 |
| `vertical_640` | 640x640 | 548 | 上挑、明显竖向剑路 |
| `dash_768` | 768x512 | 430 | 空中追击、断限追击的角色本体和短距离冲刺姿态 |
| `floor_1024` | 1024x512 | 430 | 倒地、死亡、站立到躺下等横向占宽动作，默认给魔剑和身体横向展开留空间 |

无论 cell 多大，主角常态身体高度都以 `body_512` 为锚点，建议占 `512` 高度的 68%-78%。大 cell 只是给剑、前倾姿态和倒地占宽留空间，不能把人物整体画小。

### 4.2.1 上劈 / 小跳攻击规则

上劈、挑斩、小跳挥剑和空中追击仍然遵守“同一 clip 固定 cell”。例如 `launcher` 前几帧是抬手蓄力，后几帧主角脚离地、剑向上劈，这个 clip 整体使用 `vertical_640` 或更高 profile；抬手帧会有上方透明空白，跳起峰值帧也必须完整落在同一个 `640x640` canvas 内。

规则：

- `pivotX/pivotY/baselineY` 在整个 clip 内不变；离地帧允许脚画在 baseline 上方，落地/恢复帧回到 baseline。
- 如果剑尖、头发或脚在跳起峰值贴近画布边，扩大整个 clip 到 `vertical_768` 或动作专用 profile，不改某几帧 cell。
- 如果这是纯视觉小跳，actor world origin 可以仍留在地面 pivot，画面里人物身体上移即可。
- 如果这是会影响玩法高度的上升/浮空，由状态机、物理速度、`rootMotionY` 或动画事件移动 actor；图片 canvas 不承担碰撞和位移逻辑。
- 攻击 hitbox/hurtbox 可以按帧变化，例如上劈命中帧把 hitbox 放到剑路上方；移动 collider 不因 `vertical_640` 变高而自动变大。

### 4.3 倒地与死亡动作拆分策略

倒地/死亡不污染其它动作的 cell。`idle`、`run`、`jump`、普通攻击仍然使用自己的 profile；只有从站立到躺下、躺地保持、死亡倒下这类横向占宽动作使用 `floor_1024`。如果 1024 仍放不下，扩大该动作到 `floor_1280`，不缩小人物、不裁切剑、不把身体或剑尖挤到画布边。

工程上不要把 `run_loop` 做成“第一帧在跑、最后一帧倒地”。跑步、待机这类 locomotion clip 必须能无缝循环；被击倒是状态机切换到 `hit_react` / `knockdown_fall` / `down_loop` 的结果。如果确实需要表现“奔跑中被打倒”这个专门过渡，就单独建立 `protag_run_to_knockdown_strip` 或复用 `protag_knockdown_strip` 的前几帧做起始姿态；该 transition clip 按最大倒地包围盒使用 `floor_1024` 或更大 profile，不能和 `protag_run_strip` 混在一起。

推荐拆分：

| Clip | Profile | 用途 |
| --- | --- | --- |
| `protag_knockdown_strip` | `floor_1024` | 被击倒过程：站立/空中姿态到落地躺下 |
| `protag_run_to_knockdown_strip` | `floor_1024`，仅在需要专门表现奔跑中倒地时制作 | 从奔跑姿态被打断到摔倒的过渡，不是跑步循环 |
| `protag_down_loop_strip` | `floor_1024` 或裁切帧+offset | 已经躺地的短循环或静止保持 |
| `protag_recover_strip` | `body_512`，必要时前 1-2 帧可用 `floor_1024` 单独拆 `recover_start` | 起身回到站姿 |
| `protag_dead_strip` | `floor_1024` | 死亡倒下过程 |
| `protag_dead_pose.png` | `floor_1024` 静态单图或裁切帧+offset | 最终躺地保持帧 |

图片 canvas 只服务渲染和对齐，不参与碰撞。倒地时可以切换横向 hurtbox，移动碰撞可以缩小或禁用；攻击判定仍由 hitbox/VFX 事件独立控制。

### 4.4 Pivot / Offset 对齐规则

主角所有动作都以 actor world origin 作为角色脚底/身体根部 pivot。AI 生成源帧时必须在提示词和 `frame_plan.yaml` 里写明 cell 分辨率、pivot 像素坐标和 baseline；这些辅助线是“隐形参考”，不能画进 PNG。

坐标约定：

- 图片原点在左上角，x 向右，y 向下。
- 固定 canvas 源帧中，`pivotX = cellWidth / 2`，`pivotY = baselineY`。
- 渲染偏移为 `renderOffset = [-pivotX, -pivotY]`，也就是把图片 pivot 对齐到角色世界坐标。
- 同一个 clip 内不允许每帧不同 canvas 或不同 pivot；不同 clip profile 可以有不同 cell，但必须通过 pivot/baseline/renderOffset 对齐。
- 标准交付不需要每帧 offset，因为每帧都是固定 canvas。只有验收后为了图集压缩裁掉透明边时，才允许每帧实际图片不同尺寸，并且必须额外提供 `sourceRect` 和 `offsetFromPivot`。

AI 逐帧提示词必须补充：

```text
invisible pivot at pixel ([PIVOT_X], [PIVOT_Y]), invisible baseline y=[BASELINE], align the character root and feet to this pivot/baseline, do not draw any guide line
```

### 4.5 Visual Scale 视觉尺度锁定

主角大小不由 cell 决定，而由 `scaleReference` 决定。`body_512`、`slash_640`、`floor_1024` 只是给动作预留不同透明空间；主角身体、脸、头发量、肩宽、腿长和魔剑本体长度必须看起来是同一个人。

规范描述：

- 直立/奔跑/攻击：以 `standingBodyHeightPx`、`headHeightPx`、`shoulderWidthPx` 检查比例。
- 倒地/死亡/躺地：外接框会从“窄高”变成“宽矮”，这没问题；要检查 `proneBodyLongAxisPx` 是否接近站立身体高度。
- 示例：直立身体视觉约 `100x300`，倒下后可以约 `300x100`；这表示同一个身体横过来了。不能因为 `floor_1024` 很宽，就画成 `420x140` 的巨人，也不能缩成 `220x70` 的小人。
- 动作峰值允许轻微透视、前倾和拉伸，但头部大小、脸部面积、手脚大小和魔剑长度不能漂。
- 验收容差：常规帧关键身体尺度不超过 `5%`，强攻击峰值不超过 `8%`，连续帧不能逐渐变大或变小。

AI 逐帧提示词必须补充：

```text
keep anatomical scale locked to scaleReference: same head size, shoulder width, limb length and magic sword blade length as the reference idle frame; if the character is lying down, the body long axis should match the standing body height, not become a larger or smaller character
```

### 4.6 动作连续性与力量感

主角是横板格斗动作角色，每个攻击 clip 都必须是一段有重量的动作弧，而不是几张互不相干的姿势图。尤其 `basic1/basic2/basic3/launcher/air_chase/break_limit`，必须能看出身体发力、魔剑挥动轨迹、命中峰值和收招惯性。

硬性规则：

- 每个攻击动作必须包含 `startup/anticipation`、`windup`、`activeImpact`、`followThrough`、`recovery`，帧数少的动作也不能省掉动作逻辑。
- 装备、魔剑、握持手、服装破损、头发和脸必须跨帧继承；如果第 0 帧有魔剑，第 1 帧不能突然没剑。
- 角色发力要从身体开始：脚步踩地、重心下压、腰肩旋转、手臂带剑，不能只有剑角度变化。
- 命中峰值要有强 line of action，能一眼看出攻击方向和力量点。
- 收招帧要保留惯性：衣摆、头发、魔剑角度继续运动，身体逐渐回到可操作姿态。
- 少量贴身 smear 可以留在角色源帧里，但大剑气、冲击波、爆光仍拆到 Batch06。

AI 逐帧提示词必须补充：

```text
continue the same powerful side-scrolling fighting animation sequence from the previous frame; preserve the magic sword, right-hand grip, costume details and facing direction; clear anticipation, planted foot, hip-and-shoulder rotation, strong impact frame and follow-through inertia; not disconnected poses
```

## 5. 本批资产清单

| 动作 ID | 源帧目录 | 最终输出文件 | Profile | 帧数 | FPS | 输出尺寸 | 说明 |
| --- | --- | --- | --- | ---: | ---: | --- | --- |
| idle | `source_frames/protag_idle/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_idle_strip.png` | body_512 | 8 | 12 | 4096x512 | 呼吸待机，魔剑微光 |
| run | `source_frames/protag_run/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_run_strip.png` | body_512 | 10 | 18 | 5120x512 | 横向奔跑，脚步清楚 |
| jump_start | `source_frames/protag_jump_start/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_jump_start_strip.png` | body_512 | 4 | 18 | 2048x512 | 起跳发力 |
| jump_loop | `source_frames/protag_jump_loop/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_jump_loop_strip.png` | body_512 | 4 | 12 | 2048x512 | 空中滞留 |
| fall | `source_frames/protag_fall/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_fall_strip.png` | body_512 | 4 | 12 | 2048x512 | 下落姿态 |
| land | `source_frames/protag_land/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_land_strip.png` | body_512 | 4 | 18 | 2048x512 | 落地缓冲 |
| basic1 | `source_frames/protag_basic1/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_basic1_strip.png` | slash_640 | 7 | 24 | 4480x512 | 第一段平砍 |
| basic2 | `source_frames/protag_basic2/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_basic2_strip.png` | slash_640 | 7 | 24 | 4480x512 | 第二段反手斩 |
| basic3 | `source_frames/protag_basic3/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_basic3_strip.png` | slash_heavy_768 | 9 | 24 | 6912x512 | 第三段收尾斩 |
| air_basic | `source_frames/protag_air_basic/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_air_basic_strip.png` | slash_640 | 7 | 24 | 4480x512 | 空中跳斩 |
| launcher | `source_frames/protag_launcher/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_launcher_strip.png` | vertical_640 | 9 | 24 | 5760x640 | S+J 裂空上挑 |
| air_chase | `source_frames/protag_air_chase/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_air_chase_strip.png` | dash_768 | 8 | 24 | 6144x512 | 断限后空中追击 |
| magic_bolt | `source_frames/protag_magic_bolt/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_magic_bolt_strip.png` | body_512 | 9 | 20 | 4608x512 | 魔法弹施法 |
| ally_support | `source_frames/protag_ally_support/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_ally_support_strip.png` | slash_640 | 8 | 18 | 5120x512 | 借队友力量支援 |
| break_limit | `source_frames/protag_break_limit/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_break_limit_strip.png` | dash_768 | 12 | 24 | 9216x512 | 断限追击，角色本体冲刺姿态 |
| hurt | `source_frames/protag_hurt/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_hurt_strip.png` | body_512 | 5 | 18 | 2560x512 | 受击 |
| launched | `source_frames/protag_launched/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_launched_strip.png` | body_512 | 4 | 12 | 2048x512 | 被击飞 |
| knockdown | `source_frames/protag_knockdown/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_knockdown_strip.png` | floor_1024 | 5 | 12 | 5120x512 | 倒地 |
| recover | `source_frames/protag_recover/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_recover_strip.png` | body_512 | 6 | 16 | 3072x512 | 起身 |
| dead | `source_frames/protag_dead/` | `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_dead_strip.png` | floor_1024 | 8 | 12 | 8192x512 | 战败 |

## 6. 主角通用 Prompt

每个动作先逐帧生成，不要直接让 AI 输出横向 strip。将 `[ACTION REQUEST]`、`[FRAME INDEX]`、`[FRAME COUNT]`、`[CELL WIDTH]`、`[CELL HEIGHT]`、`[PIVOT_X]`、`[PIVOT_Y]`、`[BASELINE]` 和 `[POSE FOR THIS FRAME]` 替换为对应动作要求。生成时如果工具支持参考图，应把同角色 neutral、上一帧和动作关键姿势草图作为参考输入。

```text
Use case: stylized-concept
Asset type: single animation frame for a 2D side-scrolling action game character
Primary request: original high-resolution anime pixel-art side-view magic swordsman protagonist single frame, [ACTION REQUEST], frame [FRAME INDEX] of [FRAME COUNT], [POSE FOR THIS FRAME]
Subject: young male protagonist, damaged modern school uniform mixed with restrained black-silver fantasy magic swordsman details, dark hair, readable heroic silhouette, one-handed magic sword with subtle cyan glow
Style/medium: high-resolution pixel-art anime action game sprite, DNF-like side-view proportions and pixel density, crisp pixel clusters, clean outline, readable silhouette, smooth high-frame action
Composition/framing: fixed [CELL WIDTH]x[CELL HEIGHT] RGBA transparent canvas, unused pixels alpha 0, one frame only, character faces right, invisible pivot at pixel ([PIVOT_X], [PIVOT_Y]), feet/root aligned to invisible baseline y around [BASELINE] where applicable, pivot is bottom center, do not draw pivot or baseline guide lines
Frame integrity: full body, head, hands, feet, sword blade, sword tip, coat tails and hair tips stay inside this single canvas with transparent padding; no cropped sword or body; same anatomical scale as scaleReference
Animation requirements: this is one numbered frame from a planned animation, preserve the same body box, costume, face identity, equipment, magic sword, hand grip, pivot and baseline as the other frames; clear anticipation, active impact, follow-through and recovery; powerful grounded side-scrolling fighting motion; do not draw neighboring frames; do not create a horizontal sprite sheet
Lighting/mood: energetic fantasy combat, readable over dark forest background and bright UI effects
Color palette: dark school uniform base, black and silver fantasy accents, restrained cyan magic glow, warm skin tones
Constraints: RGBA transparent PNG, unused pixels alpha 0, no text, no UI, no background, no floor, no logo, no watermark, no copied franchise character, no white matte
Avoid: chibi proportions, platformer mascot style, heavy armor redesign, changing age or face, oversized effects hiding the character, cropped sword or body, blurry non-pixel-art rendering, dirty alpha edge, horizontal contact sheet, multiple poses in one image
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
protag_basic2_strip.png: second grounded combo slash, reverse cut with stronger body rotation, follows naturally after basic1, grounded weight and clear follow-through
protag_basic3_strip.png: third grounded finisher slash, wider finishing arc, stronger step forward, heavy impact pose and recovery pose readable
protag_air_basic_strip.png: aerial slash while suspended, compact jump attack, slight forward momentum, can imply light launcher continuation
protag_launcher_strip.png: upward launcher slash, crouch then rising diagonal cut, sword points up during active frames, strong vertical energy
protag_air_chase_strip.png: airborne pursuit attack after break limit, fast forward dash in air, aggressive slash posture, controlled momentum
protag_magic_bolt_strip.png: magic bolt casting animation, gathers cyan magic at sword tip then release pose, projectile itself not included
protag_ally_support_strip.png: support call animation, protagonist braces and channels ally power through sword, no second character body in the sheet
protag_break_limit_strip.png: dramatic break limit chase body animation, fast dash strike and readable character silhouette; magic circle fragments and long cyan surge are separate Batch06 VFX, only tiny attached glow remains here
protag_hurt_strip.png: short hurt reaction, torso recoils, face tense, sword arm pulled back, returns toward controllable posture
protag_launched_strip.png: being launched upward or backward, body lifted from ground, limbs and coat trailing, no floor contact
protag_knockdown_strip.png: fall to ground and downed pose, readable non-graphic defeat state, body stays within cell
protag_recover_strip.png: recover from knockdown, hand supports body, rises back into guarded stance
protag_dead_strip.png: final defeat animation, collapses and remains down, non-graphic, no blood, no gore
```

## 8. sheet_params.yaml

推荐随 PNG 一起交付：

```yaml
defaults:
  productionMode: source_frames_first
  coordinateSystem: image_top_left_x_right_y_down
  actorWorldOrigin: pivot
  rowOrigin: top
  transparent: true
  requireRgba: true
  unusedPixelsAlpha: 0
  noCrossCellBleed: true
  noVisibleGrid: true
  bodyScaleLock: true
  bodyBoxReference: body_512
  sourceFramePattern: source_frames/{clip}/{clip}_{frame:03}.png
  assembleStripAfterValidation: true
  framePlanRequired: true
  requirePivotMetadata: true
  frameOffsetMode: fixed_canvas_no_per_frame_offset
  continuityPlanRequired: true
  actionPowerPlanRequired: true
  collisionIndependentFromSpriteCanvas: true

scaleReference:
  protag:
    referenceClip: protag_idle
    referenceProfile: body_512
    standingBodyHeightPx: 360
    headHeightPx: 48
    shoulderWidthPx: 82
    proneBodyLongAxisPx: [340, 385]
    swordBladeLengthPx: [230, 270]
    tolerancePercent: 5
    actionPeakTolerancePercent: 8
    rule: larger_cell_gives_transparent_room_not_larger_character

profiles:
  body_512:
    cellWidth: 512
    cellHeight: 512
    pivot: bottom_center
    pivotX: 256
    pivotY: 430
    baselineY: 430
    renderOffset: [-256, -430]
    safePadding: [64, 48, 64, 40]
    bodyBox: [150, 70, 220, 360]
  slash_640:
    cellWidth: 640
    cellHeight: 512
    pivot: bottom_center
    pivotX: 320
    pivotY: 430
    baselineY: 430
    renderOffset: [-320, -430]
    safePadding: [72, 48, 96, 40]
    bodyBox: [214, 70, 220, 360]
  slash_heavy_768:
    cellWidth: 768
    cellHeight: 512
    pivot: bottom_center
    pivotX: 384
    pivotY: 430
    baselineY: 430
    renderOffset: [-384, -430]
    safePadding: [96, 48, 120, 40]
    bodyBox: [278, 70, 220, 360]
  vertical_640:
    cellWidth: 640
    cellHeight: 640
    pivot: bottom_center
    pivotX: 320
    pivotY: 548
    baselineY: 548
    renderOffset: [-320, -548]
    safePadding: [72, 72, 96, 40]
    bodyBox: [214, 188, 220, 360]
    verticalEnvelope: true
    rootMotionY: optional_per_clip_event
  dash_768:
    cellWidth: 768
    cellHeight: 512
    pivot: bottom_center
    pivotX: 384
    pivotY: 430
    baselineY: 430
    renderOffset: [-384, -430]
    safePadding: [96, 48, 120, 40]
    bodyBox: [278, 70, 220, 360]
  floor_1024:
    cellWidth: 1024
    cellHeight: 512
    pivot: bottom_center
    pivotX: 512
    pivotY: 430
    baselineY: 430
    renderOffset: [-512, -430]
    safePadding: [128, 48, 160, 40]
    bodyBox: [406, 70, 220, 360]

clips:
  protag_idle_strip: { frameCount: 8, frameRate: 12, profile: body_512 }
  protag_run_strip: { frameCount: 10, frameRate: 18, profile: body_512 }
  protag_jump_start_strip: { frameCount: 4, frameRate: 18, profile: body_512 }
  protag_jump_loop_strip: { frameCount: 4, frameRate: 12, profile: body_512 }
  protag_fall_strip: { frameCount: 4, frameRate: 12, profile: body_512 }
  protag_land_strip: { frameCount: 4, frameRate: 18, profile: body_512 }
  protag_basic1_strip: { frameCount: 7, frameRate: 24, profile: slash_640, detachedVfx: vfx_basic_slash_strip }
  protag_basic2_strip: { frameCount: 7, frameRate: 24, profile: slash_640, detachedVfx: vfx_basic_slash_strip }
  protag_basic3_strip: { frameCount: 9, frameRate: 24, profile: slash_heavy_768, detachedVfx: vfx_basic_slash_heavy_strip }
  protag_air_basic_strip: { frameCount: 7, frameRate: 24, profile: slash_640, detachedVfx: vfx_air_slash_strip }
  protag_launcher_strip: { frameCount: 9, frameRate: 24, profile: vertical_640, detachedVfx: vfx_launcher_slash_strip }
  protag_air_chase_strip: { frameCount: 8, frameRate: 24, profile: dash_768, detachedVfx: vfx_air_chase_trail_strip }
  protag_magic_bolt_strip: { frameCount: 9, frameRate: 20, profile: body_512, detachedVfx: vfx_magic_bolt_projectile_strip }
  protag_ally_support_strip: { frameCount: 8, frameRate: 18, profile: slash_640 }
  protag_break_limit_strip: { frameCount: 12, frameRate: 24, profile: dash_768, detachedVfx: [vfx_break_limit_circle_strip, vfx_break_limit_dash_strip] }
  protag_hurt_strip: { frameCount: 5, frameRate: 18, profile: body_512 }
  protag_launched_strip: { frameCount: 4, frameRate: 12, profile: body_512 }
  protag_knockdown_strip: { frameCount: 5, frameRate: 12, profile: floor_1024 }
  protag_recover_strip: { frameCount: 6, frameRate: 16, profile: body_512 }
  protag_dead_strip: { frameCount: 8, frameRate: 12, profile: floor_1024 }

logicBoxes:
  standingBody: { width: 64, height: 170, note: "movement collider; independent from sprite canvas" }
  crouchBody: { width: 72, height: 120 }
  downedHurtbox: { width: 180, height: 48 }
  attackHitbox: per_frame_event_data
```

`frame_plan.yaml` 最小示例：

```yaml
protag_run:
  profile: body_512
  frameCount: 10
  baselineY: 430
  pivot: bottom_center
  pivotX: 256
  pivotY: 430
  renderOffset: [-256, -430]
  frameOffsetMode: fixed_canvas_no_per_frame_offset
  scaleReference: protag
  frames:
    0: "left foot contact, torso leaning forward, sword carried behind body"
    1: "passing pose, rear foot lifting, coat trailing backward"
    2: "right foot contact, sword angle stable, head height matches frame 0"
    3: "passing pose mirrored, arms counter-swing, bodyBox unchanged"
    4: "stretch frame, forward momentum clear, no sword crop"
    5: "loop return toward left foot contact, same scale"
    6: "passing pose, coat and hair continue motion"
    7: "right foot contact, baseline stable"
    8: "recovery into loop, sword still fully inside canvas"
    9: "anticipates frame 0, same body height and pivot"

protag_basic1:
  profile: slash_640
  frameCount: 7
  baselineY: 430
  pivot: bottom_center
  pivotX: 320
  pivotY: 430
  renderOffset: [-320, -430]
  frameOffsetMode: fixed_canvas_no_per_frame_offset
  scaleReference: protag
  continuityState:
    facing: right
    weapon: magic_sword
    weaponHand: right
    costume: damaged_school_uniform_black_silver
    equipmentVisible: true
    forbiddenChanges: [weapon_disappears, grip_swaps_hand, costume_changes, pose_jumps_to_unrelated_key]
  actionPower:
    powerLevel: light_to_medium_grounded_slash
    keyFrames:
      startup: [0]
      windup: [1, 2]
      activeImpact: [3]
      followThrough: [4, 5]
      recovery: [6]
    forceCues: [front_foot_planted, torso_twist, shoulder_leads_sword, sword_arc_left_to_right, coat_lags_after_body]
  frames:
    0: "ready stance inherited from idle, right hand grips magic sword, front foot begins to plant, no equipment missing"
    1: "anticipation, knees and center of mass lower, sword pulls slightly back, shoulder rotates, face and costume unchanged"
    2: "windup peak, hip and shoulder loaded, sword tip still inside slash_640 cell, coat starts lagging"
    3: "active slash impact frame, strong horizontal line of action, planted foot, sword blade fully visible, body drives the hit"
    4: "follow-through, sword continues past target line, torso rotation carries momentum, hair and coat trail behind"
    5: "late follow-through, weight shifts forward, sword begins to settle, same head size and right-hand grip"
    6: "recovery into combo-ready pose, magic sword still visible, body returns toward baseline and can flow into basic2"
```

生成时先写类似的 `frame_plan.yaml`，再按每个 `frames` 描述逐帧生成 PNG。不要用同一句 prompt 连续生成 10 张随机姿势。

## 9. 推荐交付结构

```text
Batch03_SideCombat_Protag/
  source_frames/
    protag_idle/protag_idle_000.png ... protag_idle_007.png
    protag_run/protag_run_000.png ... protag_run_009.png
    protag_basic1/protag_basic1_000.png ... protag_basic1_006.png
    ...
  sheets/
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

1. 先把源帧放入 `WheatearEditor/assets/vertical_slice/source_frames/protag_<action>/` 并逐帧验收。
2. 验收通过后，用脚本或工具按 `sheet_params.yaml` 拼接到 `WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_*_strip.png`。
3. 使用 Sprite Sheet Picker 或导入脚本按 `sheet_params.yaml` 中每个 clip 的 profile 切成 AnimationClip。
4. 将 `side_combat_tuning.yaml` 和场景里的主角动画引用迁移到正式 sheet/clip。
5. 保留旧逐帧序列只作为迁移对照，确认新 sheet 全部接入后删除旧运行时引用。
6. 重新打包 Sandbox，检查 `content.wtpack` 包含 `side_combat/sheets/protag_*_strip.png`。

## 11. 验收标准

- 每张源帧 PNG 尺寸必须等于该 profile 的 `cellWidth x cellHeight`；每张最终 strip PNG 尺寸必须等于 `cellWidth * frameCount` by `cellHeight`，不得多一列、少一列或混入第二行。
- 透明背景必须是真 RGBA：无用区域 alpha 为 0；放到黑、白、亮粉、透明棋盘背景上都不能有白底、灰底、黑底、绿底、脏边、白色 matte 或可见 cell 分隔线。
- 每张源帧单独显示都必须是完整角色：不缺头、不缺手、不缺腿、不缺披风、不缺剑尖；把 strip 按 cell 网格切开后必须与源帧一致。
- 同一动作内主角 bodyBox 和视觉尺寸稳定；不同 profile 的动作切换时也必须通过 pivot/baseline/offset 对齐，不能像人物突然缩放或瞬移。
- 跨动作的主角视觉尺度必须符合 `scaleReference`：直立帧看站立身高/头部/肩宽，倒地帧看身体长轴；不能因为 cell 变宽就把人物画大，也不能因为躺姿变矮就把人物缩小。
- 攻击动作必须有力量感：起手蓄力、脚步/重心、腰肩旋转、命中峰值和收招惯性清楚；不能只是不同姿势拼在一起，也不能靠 VFX 遮住没有发力的身体动作。
- 图片尺寸不参与碰撞判定；行走碰撞盒、hurtbox、hitbox 必须在引擎或 YAML 中独立配置。
- 主角每个动作里脸、发型、校服破损结构、魔剑外形和颜色体系一致。
- 动作轮廓在 1x 和游戏内缩放后都能读，攻击动作有明确起手、命中峰值和收招；像素块边缘清楚，不是模糊的赛璐璐插画缩小图。
- `run`、`jump_start`、`land`、`hurt`、`recover` 不能像静态立绘平移，必须有身体重心变化。
- `magic_bolt` 和 `break_limit` 可以有轻微角色附着光效，但不能替代 Batch06 的独立 VFX；剑气、长拖尾和魔法阵必须用独立 VFX sheet。
- 不出现 UI、背景、文字、Logo、参考游戏角色或商业可识别元素。
