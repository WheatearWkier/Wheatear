# 竖切美术资源 Sheet 生产清单

更新时间：2026-07-31

本文档用于给 Gemini Nano Banana 分批生成正式替换素材。目标是从现在的临时单张 PNG，逐步升级到“商业游戏项目可维护”的图集和横向帧序列资源。

参考资料位于：

```text
docs/参考分析/美术参考/
```

这些参考只用于理解方向：VN 要高清漂亮；VN 之外的角色、敌人、VFX、场景等实际游玩素材改为高清像素风，参考 DNF 类横板动作游戏的像素密度、清晰轮廓、横板房间层次和打击阅读性；战斗 HUD、据点和系统 UI 使用高清二次元游戏 UI，只带轻微像素质感和硬朗边缘，不做低清粗颗粒像素 UI。不要复刻商业作品角色、Logo、文字、地图、具体 UI 图案或可识别元素。

## 1. 总规则

### 1.1 风格分层

| 模块 | 风格 | 规格重点 |
| --- | --- | --- |
| VN | 高清二次元 Galgame | 1920x1080 背景，透明半身立绘，精细表情 |
| 正式横板战斗 SideCombat | 高清像素风横板动作，比例和阅读性参考 DNF 类横板 | 先生成单帧透明 pixel-art PNG，验收后拼接成 sprite strip |
| 系统 UI | 高清二次元幻想 RPG UI，轻微像素质感，布局沿用当前 demo | 图标化、少文字、可做 atlas，悬浮 tooltip 承载说明 |
| 假玩法 ArcadeCombat | 像素风 | 小体积单帧源图，验收后拼 strip；图标可做 atlas |
| 假玩法 TurnCombat | 像素风 + JRPG 回合制 UI | 角色小人、技能特效先单帧后拼接，命令图标可做 atlas |

### 1.2 通用负面要求

所有提示词都追加：

```text
no text, no letters, no numbers, no logo, no watermark, no signature, no UI letters, no copyright character, no recognizable franchise design, no white background, no gray background, no checkerboard background, no opaque background rectangle, no white matte, no dirty alpha edge, no cropped body, no cropped weapon, no body part crossing into neighboring cells, no blurry edge, no inconsistent frame size, no inconsistent character scale, no changing costume between frames
```

### 1.3 PNG 真透明硬性规则

除明确写着“不透明 PNG”的背景整图外，所有角色、战斗 sprite、VFX、UI 面板、图标、掉落物、遮罩、前景层都必须是 **RGBA PNG 真透明**。

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
```

验收方式：把素材分别叠在纯黑、纯白、亮粉和透明棋盘背景上看。只要出现白底、灰底、浅色方块、白色毛边、黑边、脏 alpha、半透明残底，直接判定不合格并重生，不再后期抠图。

### 1.4 动画生产流程：单帧源文件优先

`*_strip.png` 是运行时和导入阶段使用的打包结果，不是 AI 生图阶段的首选目标。角色、敌人、Boss、VFX、掉落物、破碎物、回合制小人和假玩法动画都按下面顺序生产：

1. 先按动作拆出 `source_frames/<clip_name>/<clip_name>_000.png`、`001.png` 等单帧 PNG。
2. 每一帧都是固定画布，尺寸等于该动作的 `cellWidth x cellHeight`，并且必须是 RGBA 真透明。
3. 每帧单独验收：主体完整、没有裁切、没有白底或脏 alpha、pivot/baseline 对齐、人物或特效比例不漂。
4. 全部单帧通过后，再由脚本或工具横向拼成 `sheets/*_strip.png` 或 `vfx_sheets/*_strip.png`。
5. 拼接后的 strip 只做导入和运行时使用；如果 strip 出问题，优先回到源帧修正，不要直接让 AI 重画整条横图。

同一个动画 clip 的 `cellWidth/cellHeight` 必须按该 clip 全部帧中的最大动作包围盒决定：先规划起手、蓄力、命中峰值、跟随、倒地等每一帧，找出身体、武器本体、爪子、尾巴、披风和必要贴身光效会占用的最大宽高，再加安全边距定 profile。定好后，这个 clip 的每一帧都使用同一 canvas；第一帧很窄或站着时也保留透明空白，不能某几帧用小 cell、某几帧用大 cell。不同 clip 可以使用不同 profile，不把死亡、横扫、躺下的最大宽度污染到 idle/run。

上劈、挑斩、小跳挥剑、跳扑、被击飞这类纵向变化也按同一原则处理：这个 clip 选用能容纳最高跳起帧和最高武器轨迹的纵向 profile，例如 `vertical_640`、`small_enemy_air` 或必要时新增 `vertical_768`。pivot/baseline 仍固定；角色脚在离地帧可以画在 baseline 上方，落地/收招再回到 baseline。不要因为某一帧跳起就改变该帧 canvas，也不要把人物缩小。真正会影响玩法位置的上升、位移、击飞高度由状态机、物理速度或 `rootMotion`/事件数据控制；图片只负责视觉姿态。

工程上动画 clip 按状态语义拆分，不按“角色从上一秒到下一秒发生了什么”随意混合。`idle_loop`、`run_loop`、`walk_loop` 是可循环 locomotion；`hit_react`、`knockdown_fall`、`dead_fall` 是一次性 transition；`down_loop`、`dead_pose` 是保持姿态；`recover` 是回到站姿的 transition。如果角色正在走路时被打倒，运行时由状态机从 `run_loop` 切到 `hit_react` 或 `knockdown_fall`，而不是让 `run_loop` 的第一帧走路、最后一帧躺下。只有这个动作本身被定义为“走路中绊倒/被击倒过渡”时，才允许第一帧类似走路姿态、最后一帧倒下；这时整个 transition clip 仍按最大倒地包围盒定 cell，例如 `floor_1024`。

例外：UI 图标 atlas、按钮状态 atlas、系统节点 atlas、VN 命令图标这类规则网格素材可以直接生成整张 atlas，因为每个 cell 的内容独立、尺寸统一、不会出现人物动作跨格。即便如此，atlas 仍要检查固定 cell、透明 alpha 和视觉尺寸一致。

单帧生成提示词必须使用这类表述：

```text
single animation frame PNG, fixed [CELL_WIDTH]x[CELL_HEIGHT] transparent canvas, frame [FRAME_INDEX] of [FRAME_COUNT], invisible pivot at pixel ([PIVOT_X], [PIVOT_Y]), same pivot and baseline as the frame plan, full body/weapon/effect contained inside this canvas, do not draw pivot or baseline guide lines, do not create a horizontal sprite sheet, do not include neighboring frames
```

源帧参数建议随图交付：

```yaml
sourceFrameRule:
  generateAsIndividualFrames: true
  assembleStripAfterValidation: true
  sourceFramePattern: source_frames/[clip_name]/[clip_name]_[frame3].png
  stripOutput: sheets/[clip_name]_strip.png
  framePlanRequired: true
  scaleReferenceRequired: true
  continuityPlanRequired: true
  actionPowerPlanRequired: true
  verticalEnvelopeRequiredForJumpingOrLauncherClips: true
  rootMotionEventsAllowed: true
  validateBeforePacking: true
```

### 1.5 Pivot / Offset 对齐元数据

为了避免播放时“抖一下”“突然瞬移”或不同动作切换错位，每个动画 clip 必须随图交付 pivot 和 offset 元数据。AI 生图提示词里也要写清楚 cell 分辨率和 pivot 像素坐标，但不要让 AI 画出辅助线。

坐标约定：

- 图片坐标原点在左上角，x 向右，y 向下。
- 角色、敌人和 Boss 的运行时世界原点默认是脚底/身体根部 pivot。
- 固定 canvas 标准流程里，`pivotX = cellWidth / 2`，`pivotY = baselineY`。
- 渲染时图片左上角相对 actor world origin 的偏移为 `renderOffset = [-pivotX, -pivotY]`。
- 同一个 clip 内每帧必须相同 `cellWidth/cellHeight/pivotX/pivotY/baselineY/renderOffset`。
- 不同 clip 可以不同 cell，但必须用 metadata 对齐到同一个 actor world origin。

源帧提示词必须带这类信息：

```text
fixed [CELL_WIDTH]x[CELL_HEIGHT] RGBA transparent canvas, invisible pivot at pixel ([PIVOT_X], [PIVOT_Y]), invisible baseline y=[BASELINE_Y], align the character root/feet to that pivot and baseline, do not draw guide lines
```

标准交付不要求每帧不同 offset，因为源帧本身已经是固定 canvas。只有在“验收后裁掉透明边并打进 atlas”的高级流程中，才允许每帧图像实际尺寸不同；这时必须给出 `sourceRect` 和 `offsetFromPivot`，否则禁止替换运行时素材。

```yaml
coordinateSystem:
  imageOrigin: top_left
  xAxis: right
  yAxis: down
  actorWorldOrigin: pivot

clipExample:
  profile: slash_640
  cellWidth: 640
  cellHeight: 512
  pivotX: 320
  pivotY: 430
  baselineY: 430
  renderOffset: [-320, -430]
  frameOffsetMode: fixed_canvas_no_per_frame_offset

trimmedAtlasOnly:
  allowedAfterSourceFrameValidation: true
  requiredPerFrameMetadata:
    - sourceRect
    - offsetFromPivot
    - originalCellWidth
    - originalCellHeight
    - pivotX
    - pivotY
```

### 1.6 Visual Scale 视觉尺度锁定

`cellWidth x cellHeight` 只是透明包装盒，不代表人物大小。角色、敌人和 Boss 的视觉比例必须用一组 `scaleReference` 锁定，不能因为某个 clip 的 cell 变大就把角色画大，也不能因为某个 clip 的 cell 较小就把角色缩小。

规范描述：

- 站立、跑步、攻击这类直立或半直立动作，用 `standingBodyHeightPx`、`headHeightPx`、`shoulderWidthPx` 检查比例。
- 倒地、死亡、趴伏这类横向动作，不看“外接框高度”是否等于站立高度，而看身体长轴 `proneBodyLongAxisPx` 是否接近站立身高。
- 例如主角直立身体约 `100x300`，倒下后可以变成约 `300x100`，这代表同一个身体旋转/躺下；如果倒下变成 `420x140` 或 `220x70`，就是人物比例漂移。
- 同一个角色跨 clip 的 `headHeightPx`、脸部大小、肩宽、手脚大小、武器本体长度都应保持一致，允许动作透视和 squash/stretch 带来小幅变化。
- 推荐容差：主角和 Boss 关键身体尺度不超过 `5%`，小怪不超过 `6%`，强动作峰值最多 `8%`，但不能连续多帧漂移。

```yaml
scaleReference:
  protag:
    referenceClip: protag_idle
    standingBodyHeightPx: 360
    headHeightPx: 48
    shoulderWidthPx: 82
    proneBodyLongAxisPx: [340, 385]
    swordBladeLengthPx: [230, 270]
    tolerancePercent: 5
    actionPeakTolerancePercent: 8
    rule: "larger cell gives transparent room, not larger character"
```

AI 逐帧提示词必须带这类信息：

```text
keep anatomical scale locked to scaleReference: same head size, shoulder width, limb length and weapon length as the reference idle frame; if the character is lying down, the body long axis should match the standing body height, not become a larger or smaller character
```

验收时不要只看当前帧 alpha 外接框的宽高。直立帧看身高，躺姿看身体长轴，空中/前倾动作看头部和躯干尺度；只要脸、头、肩、手脚或武器明显忽大忽小，直接重生该源帧。

### 1.7 Frame Continuity 动作连续性

动画 clip 必须是一段逻辑连续的动作，不是几张随机姿势图。生成前必须先写 `frame_plan.yaml`，并为每个动作记录 `continuityState`，让 AI 和后续验收都知道哪些状态必须跨帧继承。

硬性规则：

- 每个 clip 必须有清楚的动作弧：`anticipation/startup -> active/impact -> followThrough -> recovery/loopReady`。
- 第 N 帧必须能从第 N-1 帧自然过渡，不能突然换姿势、换手、换武器方向、换装备状态或换朝向。
- 持续存在的物件必须跨帧一致：武器、装备、背包、投掷前手里的石头、衣摆、尾巴、爪子、Boss 魔纹都不能无故消失、复制、换边或换形。
- 只有 `continuityState.intentionalChanges` 明确写到的变化才允许发生，比如投石动作在 release 帧之后手里的石头消失，因为 projectile 已经交给独立飞行物。
- 逐帧生成时优先提供上一帧和关键帧作为参考；如果 AI 工具不支持参考图，prompt 也必须写明上一帧状态和本帧变化。

```yaml
frameContinuity:
  clip: protag_basic1
  actionArc: [startup, windup, active_slash, follow_through, recovery]
  persistentState:
    facing: right
    weapon: magic_sword
    weaponHand: right
    costume: damaged_school_uniform_black_silver
    equipmentVisible: true
    bodyRootContinuity: true
  intentionalChanges:
    - "sword angle rotates through the slash arc"
    - "coat tails follow body rotation"
  forbiddenChanges:
    - "weapon disappears"
    - "equipment appears only on some frames"
    - "hand grip swaps sides without a turn frame"
    - "pose jumps from idle directly to recovery"
```

AI 逐帧提示词必须带这类信息：

```text
continue the same animation sequence from the previous frame and toward the next frame; preserve all persistent equipment, weapon grip, costume pieces, face identity and facing direction; this frame must be a logical in-between or key pose, not a random new pose
```

验收时把源帧快速轮播或叠 onion-skin 检查：如果第一帧有剑/装备，第二帧突然没了；如果手臂、脚、头发、尾巴、魔纹无故跳位；如果动作弧看不出起手、命中、跟随和收招，直接判定该 clip 不合格。

### 1.8 Action Power 力量感与打击感

横板格斗动作必须有力气感。角色不能像纸片一样滑过去，也不能只是每帧换一个帅姿势；动作要能看出身体发力、重心转移、武器弧线、命中峰值和收招重量。

硬性规则：

- 攻击动作必须包含明确的蓄力/预备帧、发力帧、命中峰值帧、跟随帧和恢复帧。
- 起手要有重心变化：下压、扭腰、踏步、肩膀后拉或爪子蓄势，不能从 idle 直接跳到挥击峰值。
- 命中峰值必须有清楚的 line of action：身体、肩、手、武器或爪子的方向形成一条有冲击力的动作线。
- 跟随帧必须保留惯性：衣摆、头发、尾巴、爪子、武器角度继续运动；不能命中后一帧立刻静止。
- 力量来自身体姿态和时序，VFX 只增强反馈。不能用一团光效遮住软绵绵的身体动作。
- 允许 1-2 帧 weapon smear、残影或强姿态拉伸，但主体解剖尺度仍受 `scaleReference` 约束，大型拖尾和爆光拆到 VFX。

```yaml
actionPower:
  clip: protag_basic1
  powerLevel: light_to_medium
  keyFrames:
    startup: [0, 1]
    windup: [2]
    activeImpact: [3]
    followThrough: [4, 5]
    recovery: [6]
  forceCues:
    - planted_front_foot
    - hip_and_shoulder_rotation
    - sword_arc_reads_left_to_right
    - coat_and_hair_lag_after_body
  forbidden:
    - "flat pose with only sword angle changing"
    - "weapon disappears between frames"
    - "active frame has no body weight shift"
    - "VFX hides the body mechanics"
```

AI 逐帧提示词必须带这类信息：

```text
make the action feel powerful and weighty: clear anticipation, planted foot or grounded body weight, strong line of action, readable impact frame, follow-through inertia in hair/coat/weapon; do not make this a set of disconnected poses
```

验收时快速播放源帧：如果动作没有蓄力、挥击没有身体参与、命中峰值不清楚、收招没有惯性，或只靠光效假装有打击感，该 clip 不合格。

### 1.9 Sprite Strip 拼接规格

横板战斗、回合制、假玩法动画最终仍可交付横向长条图，但只作为源帧验收后的拼接结果：

```text
single horizontal sprite strip, RGBA transparent PNG, unused pixels alpha 0, exactly N frames, equal fixed-size cells, one action only, character feet aligned to the same baseline where applicable, same camera angle, same scale, no grid lines, no frame numbers, no white matte, no cropped body, no cropped weapon, no cross-cell bleed
```

每张 sheet 旁边必须记录一份参数，后续导入 Wheatear 的 Sprite Sheet Picker：

```yaml
cellWidth: [profile_cell_width]
cellHeight: [profile_cell_height]
frameCount: 8
frameRate: 18
rowOrigin: top
pivot: bottom_center
pivotX: [profile_cell_width / 2]
pivotY: [profile_baseline_y]
baselineY: [profile_baseline_y]
renderOffset: [-pivotX, -pivotY]
safePadding: [48, 48, 48, 48]
noCrossCellBleed: true
unusedPixelsAlpha: 0
sourceFrames: source_frames/[clip_name]/[clip_name]_[frame3].png
assembledFromSourceFrames: true
```

每个子 cell 都必须能独立保存成一张完整单帧图：人物、武器、爪子、尾巴、披风、投掷物、碎片、特效边缘都只能存在于本 cell 内，不能伸到前一帧或后一帧。遇到长剑、前倾冲刺、Boss 爪击、冲击波这类会超出 cell 的动作，处理顺序是：

1. 优先把大范围剑气、冲击波、拖尾、爆光拆到独立 VFX sheet。
2. 保持角色本体缩放不变，扩大整张 sheet 的 cell 尺寸并同步更新 `sheet_params.yaml`。
3. 不允许为了塞进 cell 而让某一帧角色突然缩小，也不允许裁切后让缺失部分跑到相邻 cell。

### 1.10 2K 与九宫格规则

- VN UI、战斗 HUD、系统 UI 都按 2K 屏幕优先准备母版；能缩小显示，不用低分辨率图硬放大。
- 战斗 HUD、据点和系统 UI 只做轻微像素质感：允许清晰硬边、少量点阵纹理、像素化高光边缘；禁止做成低分辨率粗颗粒、可爱农场式或文字承载区发糊的 UI。
- 大面板、对话框、按钮、tooltip 必须九宫格友好，角饰和边框不随整体拉伸变形。
- 图标 atlas 建议用 `256x256` cell 母版，运行时缩到 `128x128` 或 `64x64`。
- 面板中心区域必须干净，中文文字、数值、tooltip 由引擎渲染，不烘焙进图片。
- 承载文字的面板采用成熟工程分层：PNG 只提供框架、角饰、高光和少量边缘纹理，中心透明或近透明；文字底由引擎额外绘制半透明 `UIPanelComponent` / 无贴图 `UIImageComponent` fill，颜色和 alpha 可运行时调整。不要通过修改 PNG 透明通道来控制阅读底。
- 推荐渲染顺序：场景或页面背景 -> engine text backdrop fill -> panel frame PNG -> engine text / icon / number。

### 1.11 文件命名

正式替换素材统一放入下列目录。收到素材后我会按这些路径接入工程：

```text
WheatearEditor/assets/vertical_slice/source_art/
WheatearEditor/assets/vertical_slice/source_frames/
WheatearEditor/assets/vertical_slice/vn/
WheatearEditor/assets/vertical_slice/side_combat/sheets/
WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/
WheatearEditor/assets/vertical_slice/ui/atlases/
WheatearEditor/assets/vertical_slice/arcade_combat/sheets/
WheatearEditor/assets/vertical_slice/turn_combat/sheets/
```

正式资源命名使用 `snake_case`，文件名必须表达模块、对象、状态/动作和规格，不再沿用早期 demo 的短名或含糊名。工程里的 `.wt` 场景、`.wts` 脚本、VN 脚本、调参 YAML 和打包依赖都要迁移到这些正式文件名。

命名规则：

- VN 背景使用 `bg_<world_or_area>_<location>_<time_or_mood>.png`，例如 `bg_modern_schoolroad_morning.png`。
- VN 立绘使用 `<character_id>_<costume_or_identity>_<expression>.png`，例如 `protag_school_neutral.png`、`mentor_magic_swordswoman_serious.png`。
- 横板动作和特效使用 `<subject>_<action>_strip.png`，例如 `protag_basic1_strip.png`、`vfx_magic_impact_strip.png`。
- 动画源帧使用 `source_frames/<clip_name>/<clip_name>_<frame3>.png`，例如 `source_frames/protag_run/protag_run_003.png`；strip 只由这些源帧拼接得到。
- 回合制和假玩法像素动画使用 `px_<subject>_<action>_strip.png`。
- UI 图集使用 `<domain>_<content>_<cell_or_size>.png` 或 `<domain>_<content>_atlas.png`，例如 `battle_skill_icons_256.png`。
- 旧名只允许出现在“迁移映射/历史备注”里，不作为新素材交付名。

### 1.12 已有资源的迁移规则

| 情况 | 处理方式 | 示例 |
| --- | --- | --- |
| 场景里已经引用的单张图 | 重命名到正式规范名，并同步改 `.wt` / `.vn` / `.wts` 引用 | `school_road.png` -> `bg_modern_schoolroad_morning.png` |
| 现有逐帧动画序列 | 先记录旧序列映射，再迁移为正式源帧和拼接后的 `_strip` | `player_basic1_1.png` -> `source_frames/protag_basic1/protag_basic1_000.png` -> `protag_basic1_strip.png` |
| 新增正式横向 sheet | 先交付 `source_frames`，验收后拼 `_strip`，旁边提供参数 | `source_frames/protag_basic1/` -> `protag_basic1_strip.png` + `sheet_params.yaml` |
| 新增 UI 图集 | 使用 `_atlas` 或规格后缀 | `battle_skill_icons_256.png` |
| 新增临时占位资源 | 放在对应模块目录，禁止散落到根目录 | `side_combat/props/root_arch.png` |

正式迁移节奏是：先生成正式命名资源；再批量更新场景、脚本、调参表和生成脚本中的资源引用；最后重新打包并检查 `content.wtpack` 只依赖正式路径。旧 demo 文件名不再作为长期兼容目标，必要时只保留在迁移表里，确认引用清零后删除。

## 2. 生成顺序

优先级按“先让一条完整体验变漂亮，再扩展细节”排列。

| 批次 | 内容 | 用途 |
| --- | --- | --- |
| Batch 01 | VN 核心立绘和背景 | 序章观感立刻提升 |
| Batch 02 | VN 对话框、菜单按钮、BGM 提示条 UI | 解决文字框和按钮临时感 |
| Batch 03 | 横板主角动作 sheet | 正式战斗手感和打击表现核心 |
| Batch 04 | 横板小怪和黑熊 Boss sheet | 浮空、受击、Boss 战观感 |
| Batch 05 | 横板战斗场景、地面、道具、掉落物 | 让正式战斗房间从“能打”变成“像游戏场景” |
| Batch 06 | 横板技能 VFX sheet、命中火花、落地烟尘 | 打击感 |
| Batch 07 | 横板战斗 HUD、技能图标、掉落图标 atlas | 战斗 UI 商业化 |
| Batch 08 | 据点/技能树/装备/结算/存档/设置 UI atlas | 系统完整性美化 |
| Batch 09 | TurnCombat 像素角色、特效、命令 UI | 回合制假玩法完整替换 |
| Batch 10 | ArcadeCombat 像素素材 | 第一段假玩法完整替换 |
| Batch 11 | TacticalCombat 像素战旗素材 | 战旗假玩法和后续战棋项目能力验证 |
| Batch 12 | 扩展角色和后续章节资源 | 白魔法、护卫、黑魔法、王后天使等正式内容 |

## 3. Batch 01：VN 核心高清资源

### 3.1 VN 背景

尺寸：`1920x1080`，不透明 PNG，16:9，无角色，无文字。

| 资源 ID | 文件名 | 说明 |
| --- | --- | --- |
| BG_MODERN_SCHOOLROAD_MORNING | `vn/backgrounds/bg_modern_schoolroad_morning.png` | 现实上学路，清晨柔光 |
| BG_MODERN_SCHOOLROAD_UNEASE | `vn/backgrounds/bg_modern_schoolroad_unease.png` | 同一路段，气氛转不安 |
| BG_OTHERWORLD_FOREST_WAKE | `vn/backgrounds/bg_otherworld_forest_wake.png` | 异世界醒来黑林 |
| BG_FOREST_AFTER_BEAR | `vn/backgrounds/bg_forest_after_bear.png` | 黑熊战后，碎枝和魔光 |
| BG_FOREST_CAMP_NIGHT | `vn/backgrounds/bg_forest_camp_night.png` | 据点营地 |
| BG_CHAPTER3_ROAD | `vn/backgrounds/bg_chapter3_road.png` | 第三章预告，边境道路 |

通用 Prompt：

```text
original anime visual novel background, [SCENE DESCRIPTION], polished galgame background art, clean composition, readable depth, soft cinematic lighting, high detail but not cluttered, 1920x1080, 16:9, no characters, no text
```

### 3.2 VN 立绘

尺寸：`1024x1536` 或 `1200x1800`，RGBA 透明 PNG，半身立绘，三分之二正面。每个角色保持同一画布、同一头身比例、同一站位。无用区域 alpha 必须为 0，不能带白底、灰底、半透明残底或白色毛边。

| 角色 | 文件名前缀 | 表情 |
| --- | --- | --- |
| 主角现实校服 | `vn/portraits/protag_school_*.png` | neutral, tired, alert, shocked, determined |
| 主角异世界初期 | `vn/portraits/protag_isekai_*.png` | confused, panic, exhausted, nervous, battle_determined |
| 现实青梅/假青梅 | `vn/portraits/aoba_*.png` | neutral, smile, worried, blush, empty, pain |
| 真青梅/导师魔剑士 | `vn/portraits/mentor_*.png` | neutral, serious, command, calm, hidden_tender, slight_smile |
| 白魔法队友 | `vn/portraits/white_mage_*.png` | neutral, smile, worried, heal_focus |
| 剑盾护卫队友 | `vn/portraits/guardian_*.png` | neutral, stern, protect, shy |
| 黑魔法队友 | `vn/portraits/black_mage_*.png` | neutral, smug, cold, casting |
| 王后/天使转生 | `vn/portraits/queen_angel_*.png` | neutral, elegant, sorrow, revelation, gentle |

通用 Prompt：

```text
original anime visual novel half-body character portrait, [CHARACTER DESCRIPTION], RGBA transparent background with unused pixels alpha 0, clean cel shading, beautiful polished galgame style, expressive eyes, consistent costume, front three-quarter view, high resolution, no text, no white matte, no dirty alpha edge
```

如果要做“眨眼/呼吸/头发轻动”的 VN 表情动画，每个表情额外生成横向 4 帧 strip：

```text
single horizontal visual novel portrait animation strip, RGBA transparent background with unused pixels alpha 0, exactly 4 frames, same character and expression, subtle breathing and blinking only, equal cell size 1024x1536, no camera movement, no text, no white matte, no dirty alpha edge
```

## 4. Batch 02：VN UI 高清皮肤

目标：参考 `VN的UI界面.png` 的信息层级：底部半透明对话框、左下小头像、底部一排图标按钮、BGM 提示条。不要复刻原图图案。VN UI 使用 2x 高清母版和九宫格接入，避免在 2K 屏幕上把小图硬拉糊。

| 资源 ID | 文件名 | 尺寸 | 说明 |
| --- | --- | --- | --- |
| UI_VN_TEXTBOX | `ui/atlases/vn_textbox_panel.png` | 3200x640 | 对话框 2x 母版，可九宫格 |
| UI_VN_NAMEPLATE | `ui/atlases/vn_nameplate.png` | 720x192 | 角色名牌 2x 母版 |
| UI_VN_BUTTON_ATLAS | `ui/atlases/vn_command_icons_256.png` | 2048x512 | save/load/qsave/qload/system/history/auto/skip 图标，cell 256 |
| UI_VN_BGM_NOTICE | `ui/atlases/vn_bgm_notice_panel.png` | 1120x192 | 左上角 BGM 提示条 2x 母版 |
| UI_VN_CHOICE_PANEL | `ui/atlases/vn_choice_panel.png` | 1800x240 | 选择项按钮底 2x 母版 |

Prompt：

```text
original anime fantasy visual novel UI asset, elegant rose-gold and dark glass frame layer, clean decorative edges, transparent or near-transparent center for engine-drawn text backdrop, no text, no icons unless requested, game-ready RGBA PNG, unused outside pixels alpha 0, soft highlight, readable over bright and dark backgrounds after engine fill is applied
```

## 5. Batch 03：正式横板主角动作 Sheet

风格：高清像素风横板动作。比例、横向房间阅读性和像素密度参考 DNF 类横板角色，不是马里奥平台跳跃，也不是低清小人。角色在有纵深的横板房间中战斗，动作轮廓要适合空中连击。

主角 cell：默认 `512x512`，RGBA 真透明背景，脚底 baseline 约 `y=430`，pivot 为底部中心。AI 先逐帧生成固定 canvas 单帧源图，验收后再横向拼接为 strip。角色身体和魔剑完整留在本帧 canvas 内；大范围剑气、冲刺拖尾和魔法阵拆到 Batch06 VFX，不烘焙在角色 sheet 里。

倒地、死亡、站立到躺下这类动作默认使用 `floor_1024`，也就是 `1024x512` 固定 canvas。第一帧站着时允许左右有大量透明空白，最后一帧躺下时身体和魔剑必须完整放进同一 canvas。如果 `1024x512` 仍不够，只扩大这个动作 profile，例如 `floor_1280`，不缩小角色、不裁切剑、不让身体进入相邻帧。`idle/run/basic` 等动作不跟着放大。

图片 canvas 不等于碰撞体。移动 collider、hurtbox、hitbox、VFX collider 都由引擎或 YAML 独立配置；躺地时可以切换横向 hurtbox 或禁用移动碰撞。

| 动作 ID | 文件名 | 帧数 | FPS | 说明 |
| --- | --- | ---: | ---: | --- |
| idle | `side_combat/sheets/protag_idle_strip.png` | 8 | 12 | 呼吸待机，魔剑微光 |
| run | `side_combat/sheets/protag_run_strip.png` | 10 | 18 | 横向奔跑，脚步清楚 |
| jump_start | `side_combat/sheets/protag_jump_start_strip.png` | 4 | 18 | 起跳发力 |
| jump_loop | `side_combat/sheets/protag_jump_loop_strip.png` | 4 | 12 | 空中滞留 |
| fall | `side_combat/sheets/protag_fall_strip.png` | 4 | 12 | 下落姿态 |
| land | `side_combat/sheets/protag_land_strip.png` | 4 | 18 | 落地缓冲 |
| basic1 | `side_combat/sheets/protag_basic1_strip.png` | 7 | 24 | 第一段平砍 |
| basic2 | `side_combat/sheets/protag_basic2_strip.png` | 7 | 24 | 第二段反手斩 |
| basic3 | `side_combat/sheets/protag_basic3_strip.png` | 9 | 24 | 第三段收尾斩 |
| air_basic | `side_combat/sheets/protag_air_basic_strip.png` | 7 | 24 | 空中跳斩，能轻微续浮空 |
| launcher | `side_combat/sheets/protag_launcher_strip.png` | 9 | 24 | S+J 裂空上挑 |
| air_chase | `side_combat/sheets/protag_air_chase_strip.png` | 8 | 24 | 断限后空中追击 |
| magic_bolt | `side_combat/sheets/protag_magic_bolt_strip.png` | 9 | 20 | 魔法弹施法 |
| ally_support | `side_combat/sheets/protag_ally_support_strip.png` | 8 | 18 | 借队友力量支援 |
| break_limit | `side_combat/sheets/protag_break_limit_strip.png` | 12 | 24 | 断限追击，魔法阵碎裂 |
| hurt | `side_combat/sheets/protag_hurt_strip.png` | 5 | 18 | 受击 |
| launched | `side_combat/sheets/protag_launched_strip.png` | 4 | 12 | 被击飞 |
| knockdown | `side_combat/sheets/protag_knockdown_strip.png` | 5 | 12 | 倒地 |
| recover | `side_combat/sheets/protag_recover_strip.png` | 6 | 16 | 起身 |
| dead | `side_combat/sheets/protag_dead_strip.png` | 8 | 12 | 战败 |

主角通用 Prompt：

```text
original high-resolution anime pixel-art side-scrolling action game single animation frame, young male magic swordsman, damaged modern school uniform mixed with fantasy black-silver magic sword, DNF-like side-view readability and pixel density, crisp pixel clusters, clean silhouette, fixed [CELL] transparent canvas, invisible pivot at pixel ([PIVOT_X], [PIVOT_Y]), anatomical scale locked to scaleReference, frame continuity and action power locked to frame_plan, RGBA transparent background with unused pixels alpha 0, frame [FRAME_INDEX] of [N], feet aligned to same baseline, same character scale as frame plan, no cropped body, no cropped sword, no text, do not draw guide lines, do not create a horizontal sprite sheet
```

## 6. Batch 04：正式横板敌人与 Boss Sheet

敌人/Boss 和主角同样采用“单帧源文件优先 + 动作 profile 分层”。同一个 clip 内 cell 长宽固定；不同 clip 可以使用不同 profile。不要让 idle/run 使用倒地或横扫的最大宽度，也不要让跳扑、爪击、投掷、被挑空、死亡倒下为了塞进常态格而缩小或裁掉。

推荐 profile：

| profile | cell | 用途 |
| --- | --- | --- |
| `small_enemy` | 512x384 | 小怪 idle/run/hit/land 等常态动作 |
| `small_enemy_wide` | 768x384 | 小怪 claw_attack/throw/leap/dead/collapse 等横向展开动作 |
| `small_enemy_air` | 512x512 | 小怪 launched/fall 等竖向展开动作 |
| `boss_bear_husband` | 1024x768 | Boss 常态、移动、短硬直、破防动作 |
| `boss_bear_husband_wide` | 1280x768 | Boss 横扫、砸地峰值、死亡倒下等横向大动作 |

如果这些 profile 仍不够，只扩大当前动作，例如 `small_enemy_896` 或 `boss_bear_husband_1536`；不能缩小、裁切或跨格。敌人移动 collider、hurtbox、hitbox、Boss 攻击判定仍然独立于图片 canvas。

### 6.1 爪兽小怪

cell：按 Batch04 profile 选择，RGBA 真透明背景。小怪常态高度建议占 `small_enemy` 高度的 `58%-72%`，所有爪子、尾巴、耳朵、毛发尖端和光边都留在本帧 canvas 内。

| 动作 | 文件名 | Profile | 帧数 | FPS |
| --- | --- | --- | ---: | ---: |
| idle | `side_combat/sheets/en_claw_beast_idle_strip.png` | `small_enemy` | 8 | 12 |
| run | `side_combat/sheets/en_claw_beast_run_strip.png` | `small_enemy` | 8 | 18 |
| claw_attack | `side_combat/sheets/en_claw_beast_claw_attack_strip.png` | `small_enemy_wide` | 8 | 20 |
| hit | `side_combat/sheets/en_claw_beast_hit_strip.png` | `small_enemy` | 5 | 18 |
| launched | `side_combat/sheets/en_claw_beast_launched_strip.png` | `small_enemy_air` | 4 | 12 |
| fall | `side_combat/sheets/en_claw_beast_fall_strip.png` | `small_enemy_air` | 4 | 12 |
| dead | `side_combat/sheets/en_claw_beast_dead_strip.png` | `small_enemy_wide` | 8 | 12 |

Prompt：

```text
original high-resolution anime pixel-art side-scrolling monster single animation frame, small agile wolf-like claw beast, dark fur, cyan magic eyes, readable silhouette, bright DNF-like action game pixel style, fixed transparent canvas from Batch04 current clip profile, invisible pivot pixel and baseline from sheet_params metadata, anatomical scale locked to scaleReference, frame continuity and action power locked to frame_plan, RGBA transparent background with unused pixels alpha 0, frame [FRAME_INDEX] of [N], same scale as frame plan, full body and claws contained inside canvas, no cropped claws, no text, do not draw guide lines, do not create a horizontal sprite sheet
```

### 6.2 黑熊丈夫 Boss

cell：按 Batch04 profile 选择，RGBA 真透明背景。Boss 比主角大约 1.8 到 2.2 倍，但不要塞满画布；攻击动作允许姿态伸展，但爪子、头、背、脚掌和贴身光必须完整留在本帧 canvas 内。横扫、砸地峰值、死亡倒下默认使用 `boss_bear_husband_wide`。大面积冲击波拆到 Batch06。

| 动作 | 文件名 | Profile | 帧数 | FPS |
| --- | --- | --- | ---: | ---: |
| idle | `side_combat/sheets/boss_bear_husband_idle_strip.png` | `boss_bear_husband` | 10 | 10 |
| walk | `side_combat/sheets/boss_bear_husband_walk_strip.png` | `boss_bear_husband` | 10 | 14 |
| claw_attack | `side_combat/sheets/boss_bear_husband_claw_attack_strip.png` | `boss_bear_husband_wide` | 10 | 18 |
| charge_windup | `side_combat/sheets/boss_bear_husband_charge_windup_strip.png` | `boss_bear_husband` | 8 | 14 |
| charge_loop | `side_combat/sheets/boss_bear_husband_charge_loop_strip.png` | `boss_bear_husband` | 6 | 18 |
| shockwave_cast | `side_combat/sheets/boss_bear_husband_shockwave_cast_strip.png` | `boss_bear_husband_wide` | 12 | 18 |
| roar | `side_combat/sheets/boss_bear_husband_roar_strip.png` | `boss_bear_husband` | 8 | 12 |
| hit | `side_combat/sheets/boss_bear_husband_hit_strip.png` | `boss_bear_husband` | 5 | 16 |
| launched | `side_combat/sheets/boss_bear_husband_launched_strip.png` | `boss_bear_husband` | 4 | 10 |
| fall | `side_combat/sheets/boss_bear_husband_fall_strip.png` | `boss_bear_husband` | 5 | 10 |
| break_stun | `side_combat/sheets/boss_bear_husband_break_stun_strip.png` | `boss_bear_husband` | 8 | 12 |
| dead | `side_combat/sheets/boss_bear_husband_dead_strip.png` | `boss_bear_husband_wide` | 10 | 10 |

Prompt：

```text
original high-resolution anime pixel-art side-scrolling boss single animation frame, massive corrupted black bear husband, blue magical veins, huge claws, powerful readable silhouette, bright DNF-like action game pixel style, fixed transparent canvas from Batch04 current clip profile, invisible pivot pixel and baseline from sheet_params metadata, anatomical scale locked to scaleReference, frame continuity and heavy action power locked to frame_plan, RGBA transparent background with unused pixels alpha 0, frame [FRAME_INDEX] of [N], consistent ground baseline, same boss scale as frame plan, full body and claws contained inside canvas, no cropped claws, no text, do not draw guide lines, do not create a horizontal sprite sheet
```

## 7. Batch 05：横板战斗场景与战斗物件

正式横板战斗视角不是平台跳跃，而是“带一点俯视纵深的横板房间”。参考方向是明亮爽快的二次元动作游戏场景：画面有强烈层次、清晰战斗平面、地面透视线能帮助玩家判断前后走位。

### 7.1 黑林兽道正式战斗场景

尺寸建议：

- 主背景：`3840x2160`，用于宽屏和镜头轻微移动。
- 分层背景：每层 `3840x1080` 或 `3840x1440`。
- 地面/可行走区域：单独导出，便于后续做碰撞、透视线和替换。

| 资源 ID | 文件名 | 尺寸 | 说明 |
| --- | --- | --- | --- |
| SIDE_BG_BLACK_FOREST_SKY | `side_combat/backgrounds/black_forest_sky.png` | 3840x1080 | 远景天空、树影、魔雾 |
| SIDE_BG_BLACK_FOREST_MID | `side_combat/backgrounds/black_forest_mid.png` | 3840x1080 | 中景树林、岩石、发光植物 |
| SIDE_BG_BLACK_FOREST_FLOOR | `side_combat/backgrounds/black_forest_floor.png` | 3840x1080 | 可战斗地面，有纵深透视线 |
| SIDE_BG_BLACK_FOREST_FRONT | `side_combat/backgrounds/black_forest_front.png` | 3840x1080 | 前景草叶、树根，透明 PNG |
| SIDE_BG_BLACK_FOREST_LIGHTS | `side_combat/backgrounds/black_forest_magic_lights.png` | 3840x1080 | 可叠加魔光层，透明 PNG |

Prompt：

```text
original high-resolution pixel-art 2D side-scrolling action game battle stage, bright and stylish fantasy black forest path, DNF-like side-scrolling town/dungeon readability without copying any map, slight top-down perspective, clear horizontal combat room, readable floor plane with subtle perspective guide lines, layered parallax background, glowing cyan magical plants, dark trees but vibrant action-game lighting, crisp pixel-art background, no characters, no text
```

### 7.2 战斗房间装饰与障碍物

这些素材用于让场景不像空地，也能给后续关卡设计做复用。透明 PNG，独立物件。

| 资源 ID | 文件名 | 尺寸 | 说明 |
| --- | --- | --- | --- |
| PROP_ROOT_ARCH | `side_combat/props/root_arch.png` | 512x384 | 树根拱形装饰 |
| PROP_MAGIC_CRYSTAL | `side_combat/props/magic_crystal_cluster.png` | 384x384 | 发光水晶 |
| PROP_BROKEN_LOG | `side_combat/props/broken_log.png` | 512x256 | 倒木 |
| PROP_STONE_PILLAR | `side_combat/props/stone_pillar.png` | 384x512 | 石柱/可遮挡前景 |
| PROP_CRATE_FOREST | `side_combat/props/forest_supply_crate.png` | 256x256 | 补给箱，可破坏候选 |

Prompt：

```text
original high-resolution pixel-art fantasy action game stage prop, [PROP DESCRIPTION], black forest magic theme, clean silhouette, RGBA transparent background with unused pixels alpha 0, consistent lighting with bright cyan magical accents, game-ready 2D prop, no white matte, no dirty alpha edge, no text
```

### 7.3 掉落物与可拾取素材

掉落物需要“世界内小 sprite”和“UI 图标”两套。世界内 sprite 用于战斗场景地上掉落，先单帧源图后拼接 strip；UI 图标用于结算、背包和 tooltip，可以保留 atlas。

| 掉落 | 世界内文件 | UI 图标文件 | 说明 |
| --- | --- | --- | --- |
| 魔核碎片 | `side_combat/pickups/pickup_magic_core_strip.png` | `ui/atlases/battle_drop_icons_256.png` | 8 帧闪光循环 |
| 兽筋 | `side_combat/pickups/pickup_beast_sinew_strip.png` | `ui/atlases/battle_drop_icons_256.png` | 6 帧轻微浮动 |
| 熊爪 | `side_combat/pickups/pickup_bear_claw_strip.png` | `ui/atlases/battle_drop_icons_256.png` | 6 帧闪烁 |
| 金币/通用货币 | `side_combat/pickups/pickup_coin_strip.png` | `ui/atlases/battle_drop_icons_256.png` | 8 帧旋转 |
| 装备箱 | `side_combat/pickups/pickup_equipment_box_strip.png` | `ui/atlases/battle_drop_icons_256.png` | 8 帧发光 |

Prompt：

```text
original high-resolution pixel-art fantasy action RPG pickup item single animation frame, [ITEM DESCRIPTION], fixed [CELL] transparent canvas, RGBA transparent background with unused pixels alpha 0, frame [FRAME_INDEX] of [N], subtle floating and glowing animation, item and glow fully contained inside this canvas, same scale as frame plan, no white matte, readable at small size, no text, do not create a horizontal sprite sheet
```

### 7.4 战斗场景通用遮罩与反馈素材

| 资源 ID | 文件名 | 尺寸 | 说明 |
| --- | --- | --- | --- |
| SHADOW_BLOB_SOFT | `side_combat/ui/blob_shadow_soft.png` | 256x128 | 角色/敌人地面阴影 |
| LANE_GUIDE_SUBTLE | `side_combat/ui/lane_guide_subtle.png` | 1920x256 | 调试/可选纵深提示 |
| HIT_SCREEN_FLASH | `side_combat/ui/hit_screen_flash.png` | 1920x1080 | 命中白闪，透明 |
| BREAK_LIMIT_OVERLAY | `side_combat/ui/break_limit_overlay.png` | 1920x1080 | 断限瞬间叠加特效 |

Prompt：

```text
original high-resolution pixel-art action game overlay effect, [EFFECT DESCRIPTION], RGBA transparent background with unused pixels alpha 0, clean high contrast, designed for fast combat feedback, no white matte, no dirty alpha edge, no text
```

## 8. Batch 06：横板战斗 VFX Sheet

VFX 先做单帧源图，验收后再拼成独立 sheet，便于和角色动画分离，也便于后续用 `SpriteAnimatorComponent` 和动画事件控制。

| VFX | 文件名 | Cell | 帧数 | FPS | 用途 |
| --- | --- | --- | ---: | ---: | --- |
| basic_slash | `side_combat/vfx_sheets/vfx_basic_slash_strip.png` | 768x384 | 8 | 30 | 三段斩横向剑气 |
| basic_slash_heavy | `side_combat/vfx_sheets/vfx_basic_slash_heavy_strip.png` | 896x448 | 10 | 30 | 第三段收尾重剑气 |
| launcher_slash | `side_combat/vfx_sheets/vfx_launcher_slash_strip.png` | 768x768 | 10 | 30 | 上挑竖向剑气 |
| air_slash | `side_combat/vfx_sheets/vfx_air_slash_strip.png` | 768x512 | 8 | 30 | 空中跳斩 |
| air_chase_trail | `side_combat/vfx_sheets/vfx_air_chase_trail_strip.png` | 896x512 | 10 | 30 | 空中追击拖尾 |
| break_limit_circle | `side_combat/vfx_sheets/vfx_break_limit_circle_strip.png` | 1024x1024 | 16 | 30 | 断限魔法阵碎裂 |
| break_limit_dash | `side_combat/vfx_sheets/vfx_break_limit_dash_strip.png` | 1280x640 | 12 | 30 | 断限冲刺残影 |
| magic_bolt_projectile | `side_combat/vfx_sheets/vfx_magic_bolt_projectile_strip.png` | 384x192 | 8 | 24 | 飞行魔法弹 |
| magic_cast_spark | `side_combat/vfx_sheets/vfx_magic_cast_spark_strip.png` | 512x512 | 8 | 24 | 剑尖蓄魔光 |
| magic_impact | `side_combat/vfx_sheets/vfx_magic_impact_strip.png` | 768x768 | 10 | 30 | 魔法命中爆裂 |
| hit_spark_light | `side_combat/vfx_sheets/vfx_hit_spark_light_strip.png` | 512x512 | 8 | 30 | 轻命中火花 |
| hit_spark_heavy | `side_combat/vfx_sheets/vfx_hit_spark_heavy_strip.png` | 768x768 | 10 | 30 | 重命中火花 |
| guard_flash | `side_combat/vfx_sheets/vfx_guard_flash_strip.png` | 640x640 | 8 | 24 | 防御/支援保护闪光 |
| boss_bear_charge_dust | `side_combat/vfx_sheets/vfx_boss_bear_charge_dust_strip.png` | 1024x384 | 10 | 24 | Boss 冲锋地面尘土 |
| boss_bear_shockwave | `side_combat/vfx_sheets/vfx_boss_bear_shockwave_strip.png` | 1536x512 | 12 | 24 | Boss 砸地冲击波 |
| landing_dust | `side_combat/vfx_sheets/vfx_landing_dust_strip.png` | 768x384 | 8 | 24 | 主角/Boss 落地烟尘 |
| pickup_glow | `side_combat/vfx_sheets/vfx_pickup_glow_strip.png` | 384x384 | 8 | 12 | 掉落物吸附光 |
| level_up_burst | `side_combat/vfx_sheets/vfx_level_up_burst_strip.png` | 1024x1024 | 14 | 24 | 战后升级/魔剑响应 |

VFX Prompt：

```text
original high-resolution pixel-art 2D action game visual effect single animation frame, [VFX DESCRIPTION], cyan blue magic and black-silver sword energy, fixed [CELL] transparent canvas, RGBA transparent background with unused pixels alpha 0, frame [FRAME_INDEX] of [N], crisp pixel-art additive-looking shapes, every particle and glow fully contained inside this canvas, no cropped effect, no white matte, no text, do not create a horizontal sprite sheet
```

## 9. Batch 07：横板战斗 HUD 与图标 Atlas

目标：参考明亮爽快二次元横板 HUD。不要学页游满屏红点，优先图标和 tooltip。

| Atlas | 文件名 | 规格 | 内容 |
| --- | --- | --- | --- |
| battle_skill_icons | `ui/atlases/battle_skill_icons_256.png` | 8x4, cell 256 | 主动技能、支援、断限、锁定灰图 |
| battle_item_icons | `ui/atlases/battle_item_icons_256.png` | 4x2, cell 256 | 1/2/3 道具栏占位、药水、卷轴 |
| battle_drop_icons | `ui/atlases/battle_drop_icons_256.png` | 6x2, cell 256 | 魔核、兽筋、兽爪、金币、装备箱 |
| battle_hud_panels | `ui/atlases/battle_hud_panels.png` | 4096x2048 | 血条框、Boss 条、技能栏底板、连击牌 |

Prompt：

```text
original high-resolution anime fantasy action RPG UI icon atlas with subtle pixel-art accents, bright polished DNF-like combat UI readability without copying any icon, sharp readable icons, crisp hard edges, cyan blue magic sword theme with warm gold accents, RGBA transparent background with unused pixels alpha 0, no text, no letters, no numbers, consistent icon perspective, 256x256 master cells, readable when downscaled to 128x128 and 64x64, not low-resolution chunky pixel UI, not Stardew Valley style
```

## 10. Batch 08：系统 UI Atlas

布局保持当前 demo：据点、技能树、装备、关系、支援、结算、存档、设置。但底板、按钮、节点、图标统一皮肤。

| Atlas | 文件名 | 规格 | 内容 |
| --- | --- | --- | --- |
| common_panels | `ui/atlases/common_panels_4k.png` | 4096x4096 | 大面板、二级面板、tooltip、弹窗、标题条 |
| common_buttons | `ui/atlases/common_buttons_256.png` | 2048x1024, cell 256 | 普通/hover/pressed/disabled 按钮，页码按钮 |
| system_icons | `ui/atlases/system_icons_256.png` | 8x8, cell 256 | 背包、技能树、装备、关系、保存、设置、返回 |
| skill_tree_nodes | `ui/atlases/skill_tree_nodes_256.png` | 8x4, cell 256 | 已学、可学、锁定、选中、核心节点 |
| equipment_icons | `ui/atlases/equipment_icons_256.png` | 8x4, cell 256 | 武器、防具、饰品、材料、强化石 |
| relationship_icons | `ui/atlases/relationship_icons_256.png` | 8x2, cell 256 | 白魔、护卫、黑魔、青梅、好感阶段 |
| result_badges | `ui/atlases/result_badges_256.png` | 8x2, cell 256 | S/A/B/C 评分、首通、连击、掉落 |

Prompt：

```text
original high-resolution anime fantasy RPG system UI atlas with subtle pixel-art accents, elegant dark glass panels with cyan magic sword glow and warm gold trims, crisp readable shapes, game-ready interface assets, RGBA transparent background where applicable with unused pixels alpha 0, no text, no logo, no white matte, consistent style, not low-resolution chunky pixel UI, not Stardew Valley style
```

## 11. Batch 09：TurnCombat 像素资源

回合制是假玩法之一，像素风即可，但要完整。cell：`96x96` 或 `128x128`，RGBA 真透明背景，无用区域 alpha 为 0。

| 角色/特效 | 文件名 | Cell | 帧数 |
| --- | --- | --- | ---: |
| protag_magic_swordsman_idle | `turn_combat/sheets/px_protag_magic_swordsman_idle_strip.png` | 128x128 | 4 |
| protag_magic_swordsman_attack | `turn_combat/sheets/px_protag_magic_swordsman_attack_strip.png` | 128x128 | 6 |
| white_mage_idle | `turn_combat/sheets/px_white_mage_idle_strip.png` | 128x128 | 4 |
| guardian_idle | `turn_combat/sheets/px_guardian_idle_strip.png` | 128x128 | 4 |
| en_bearling_idle | `turn_combat/sheets/px_en_bearling_idle_strip.png` | 128x128 | 4 |
| en_wolf_idle | `turn_combat/sheets/px_en_wolf_idle_strip.png` | 128x128 | 4 |
| en_apprentice_mage_idle | `turn_combat/sheets/px_en_apprentice_mage_idle_strip.png` | 128x128 | 4 |
| skill_slash | `turn_combat/sheets/px_vfx_slash_strip.png` | 128x128 | 6 |
| skill_heal | `turn_combat/sheets/px_vfx_heal_strip.png` | 128x128 | 8 |
| skill_dark | `turn_combat/sheets/px_vfx_dark_strip.png` | 128x128 | 8 |

Prompt：

```text
original pixel art JRPG turn-based battle single animation frame, [CHARACTER OR EFFECT], fixed [CELL] transparent canvas, RGBA transparent background with unused pixels alpha 0, frame [FRAME_INDEX] of [N], clean readable 32-bit pixel style, no cropped body or effect, no white matte, no text, do not create a horizontal sprite sheet
```

## 12. Batch 10：ArcadeCombat 像素资源

第一段假玩法要故意显得简单，但不能是方块。cell：`64x64` 或 `96x96`。

| 资源 | 文件名 | Cell | 帧数 |
| --- | --- | --- | ---: |
| fake_player_idle | `arcade_combat/sheets/px_fake_player_idle_strip.png` | 64x64 | 4 |
| fake_player_attack | `arcade_combat/sheets/px_fake_player_attack_strip.png` | 64x64 | 4 |
| fake_bear_idle | `arcade_combat/sheets/px_fake_bear_idle_strip.png` | 96x96 | 4 |
| fake_bear_attack | `arcade_combat/sheets/px_fake_bear_attack_strip.png` | 96x96 | 4 |
| fake_projectile | `arcade_combat/sheets/px_fake_projectile_strip.png` | 32x32 | 4 |
| fake_hit | `arcade_combat/sheets/px_fake_hit_strip.png` | 64x64 | 5 |
| fake_stage_tiles | `arcade_combat/px_fake_stage_tiles.png` | 512x512 | tile atlas |

Prompt：

```text
original cute pixel art fantasy mini-game single animation frame, intentionally simple but charming, [ASSET DESCRIPTION], fixed [CELL] transparent canvas, RGBA transparent background with unused pixels alpha 0, frame [FRAME_INDEX] of [N], no cropped body or effect, no white matte, no text, do not create a horizontal sprite sheet
```

## 13. Batch 11：TacticalCombat 像素战旗资源

战旗玩法是假玩法，但素材必须完整，不能只用纯色格子。当前工程路径：

```text
WheatearEditor/assets/vertical_slice/tactical_combat/
```

| 类型 | 命名 | 内容 |
| --- | --- | --- |
| 背景 | `backgrounds/bg_tactical_ruins_grid.png` | 战旗遗迹棋盘背景 |
| 棋格 | `tiles/tile_plain.png` 等 | 普通、移动、攻击、选中格 |
| 我方单位 | `characters/unit_<role>_<action>_<frame2>.png` | 主角、白魔、护卫的 idle/attack/hit/down |
| 敌方单位 | `characters/unit_en_<role>_<action>_<frame2>.png` | 枪兵、暗术师、兽兵 |
| 特效 | `effects/vfx_tac_<skill>_<frame2>.png` | 斩击、魔法、治疗、守备、暗术 |
| UI 图标 | `ui/icons/cmd_tac_<action>.png` | 攻击、魔法、治疗、守备、敌方技能 |
| UI 面板 | `ui/panels/panel_tactical_*.png` | 顶栏、状态栏、详情栏、命令栏、按钮底 |
| 音频 | `audio/tac_*.wav` | BGM、选择、移动、斩击、魔法、治疗、守备、受击、胜利 |

如果后续正式化，单位帧序列也要升级为横向 sheet：

```text
single pixel art tactical RPG animation frame, fixed 128x128 transparent canvas, RGBA transparent background with unused pixels alpha 0, frame [FRAME_INDEX] of 4, one action only, same unit scale, same baseline, no cropped body or effect, no white matte, no text, do not create a horizontal sprite sheet
```

## 14. Batch 12：扩展角色和后续章节

这些不是当前竖切第一优先，但应该先锁定规格，避免后面返工。

| 角色 | VN 立绘 | 横板支援/特效 |
| --- | --- | --- |
| 白魔法队友 | 1024x1536 表情差分 | heal_support strip，buff_aura strip |
| 剑盾护卫队友 | 1024x1536 表情差分 | guard_support strip，shield_impact strip |
| 黑魔法队友 | 1024x1536 表情差分 | dark_support strip，debuff_orb strip |
| 王后/天使转生 | 1024x1536 表情差分 | angel_blessing strip，revive_feather strip |
| 国师大魔法师 | 1024x1536 表情差分 | summon_circle strip，dark_army_gate strip |

## 15. 当前竖切全素材覆盖检查表

这一节按“玩家现在能看到/能玩到什么”倒推，不按美术类型倒推。凡当前竖切会展示的内容，都应该在正式素材计划里有对应资源。

### 15.1 VN、主菜单与剧情演出

| 位置 | 必要素材 | 命名策略 |
| --- | --- | --- |
| 主菜单 | 背景、标题 Logo、菜单按钮底、按钮图标、选中光效 | 背景和 UI 都迁移到正式名；场景引用同步更新。Logo 使用 `ui/atlases/title_logo_wheatear.png` |
| VN 对话 | 背景、立绘、表情差分、对话框、名牌、选择项按钮、历史记录面板 | 背景/立绘优先对齐 `vn/backgrounds/*` 和 `vn/portraits/*` |
| VN 控制栏 | 保存、读取、快存、快读、系统、历史、自动、跳过、文本速度图标 | 做成 `vn_command_icons_256.png`，旧 `icon_save.png` 等单图只作为迁移源，正式场景引用改到图集或规范单图 |
| BGM 提示 | 左上角 BGM 入场提示条、音符图标、淡入淡出遮罩 | 新增 `vn_bgm_notice_panel.png` 和 `icon_music_note.png` |
| 转场 | 黑幕淡入、白闪、章节标题底纹、路线确认提示 | 新增 `ui/atlases/transition_overlays.png` |

### 15.2 据点与系统页面

| 页面 | 必要素材 | 说明 |
| --- | --- | --- |
| 据点 Hub | 营地背景、主面板、功能入口图标、当前路线卡片、资源栏图标 | 迁移到正式 UI atlas / panel 命名；场景引用同步更新 |
| 副本选择 | 副本卡片、难度角标、奖励预览图标、路线确认按钮 | 后续章节复用，图标优先 atlas 化 |
| 技能树 | 大背景魔法阵、圆形节点、锁定/可学/已学/选中状态、曲线路径、分支徽记、右侧详情面板 | 迁移到 `skill_tree_nodes_256.png`、正式技能图标 atlas 或规范单图 |
| 装备 | 背包格子、已装备槽、装备图标、材料图标、强化按钮、对比箭头、悬浮 tooltip 面板 | 迁移到 `equipment_icons_256.png`、正式材料图标 atlas 或规范单图 |
| 好感度 | 角色头像、好感阶段徽章、支援技能等级图标、心形/羁绊条 | 和 VN 立绘保持同角色设计 |
| 支援技能 | 队友技能图标、冷却边框、已解锁/未解锁遮罩 | 后续可直接复用战斗 HUD 图标规范 |
| 存档/读取 | 存档槽缩略图框、章节标签、保存成功提示、分页按钮 | VN 内存档和战斗后存档使用同一套 UI |
| 设置 | 音量滑条、开关、分页/标签、重置按钮、确认弹窗 | 滑条需要 thumb/track/fill 三段素材 |
| 结算 | 评分徽章、掉落图标、经验条、材料列表、保存/继续按钮 | 掉落只用图标和 tooltip，避免长文字堆叠 |

### 15.3 正式横板战斗 SideCombat

| 类型 | 必要素材 | 正式路径/迁移策略 |
| --- | --- | --- |
| 战斗场景 | 黑林战斗房间、远中近景、地面透视线、前景遮挡、魔光层 | 当前运行时单张背景为 `side_combat/backgrounds/bg_black_forest_stage.png`；正式分层可扩展为 `bg_black_forest_*` |
| 场景物件 | 树根、石柱、水晶、倒木、补给箱、可破坏物 | 新增 `side_combat/props/*` |
| 主角动作 | 待机、跑、起跳、滞空、下落、落地、三段普攻、空中普攻、上挑、断限追击、魔法弹、支援、受击、击飞、倒地、起身、死亡 | 当前运行时帧序列为 `protag_*_{frame}.png`；正式 sheet 迁移到 `protag_*_strip.png` |
| 小怪动作 | 待机、跑、爪击、受击、浮空、下落、死亡 | 当前运行时帧序列为 `en_claw_beast_*_{frame}.png`；正式 sheet 迁移到 `en_claw_beast_*_strip.png` |
| Boss 动作 | 待机、行走、爪击、冲撞、震地、吼叫、受击、浮空、坠落、破防硬直、死亡 | 当前运行时帧序列为 `boss_bear_husband_*_{frame}.png`；正式 sheet 迁移到 `boss_bear_husband_*_strip.png` |
| 攻击特效 | 普攻剑气、上挑剑气、空中斩、魔法弹、魔法命中、断限魔法阵、命中火花、落地烟尘 | `side_combat_tuning.yaml` 的 `textureFramePattern` 同步迁移到 `vfx_sheets/vfx_*_strip.png` 或正式 VFX 帧序列 |
| 战斗 HUD | 生命条、Boss 条、技能栏底板、主动技能图标、冷却灰罩、倒计时遮罩、连击牌、断限槽、教程键位提示 | J/K 不进技能栏；主动技能和支援技能用图标+tooltip |
| 道具栏 | 1/2/3 道具槽、药水、专注药剂、爆裂道具、数量角标 | 迁移到正式 `battle_item_icons_256.png` 或规范单图，旧 `item_slot_*` 引用清零 |
| 掉落 | 世界内掉落 sprite、吸附光效、结算图标、背包图标 | 世界内掉落用 `pickup_*_strip.png`，UI 图标用 `battle_drop_icons_256.png` |
| 音画提示 | 屏幕震动遮罩、受击白闪、低血量边框、断限成功全屏叠加 | 新增 `side_combat/ui/*overlay*` |

### 15.4 回合制 TurnCombat

| 类型 | 必要素材 | 正式路径/迁移策略 |
| --- | --- | --- |
| 场景 | 遗迹战斗背景、站位影子、目标圈 | 当前运行时背景为 `turn_combat/backgrounds/bg_turn_ruins_arena.png`；UI 图标保持 `cmd_*` / `target_marker` 命名 |
| 我方角色 | 主角、白魔法、剑盾护卫的待机/攻击/受击/倒下 | 当前运行时帧序列使用 `protag_magic_swordsman_*_{frame2}.png`、`ally_*_{frame2}.png`；正式 sheet 迁移到 `px_*_strip.png` |
| 敌方角色 | 小熊、狼、见习魔法师的待机/攻击/受击/倒下 | 当前运行时帧序列使用 `en_bearling_*_{frame2}.png`、`en_wolf_*_{frame2}.png`、`en_apprentice_mage_*_{frame2}.png`；正式 sheet 迁移到 `px_*_strip.png` |
| 技能特效 | 斩击、魔剑、治疗、护盾、黑魔法、蓄力、爪击 | 迁移到 `px_vfx_*_strip.png` 或正式 VFX 图集 |
| UI | 行动顺序条、命令面板、技能图标、目标标记、消息框、HP/MP 条 | 迁移到正式 TurnCombat UI atlas / panel 命名 |
| 音频提示图标 | 技能音效触发提示、BGM 切换提示 | 可复用 VN 的 BGM 提示条 |

### 15.5 假玩法 ArcadeCombat

| 类型 | 必要素材 | 正式路径/迁移策略 |
| --- | --- | --- |
| 场景 | 像素竞技场背景、危险条、障碍物/掩体 | 迁移到 `arcade_combat/*` 规范目录，旧 `assets/vertical_slice/arcade_combat/*` 引用清零 |
| 角色 | 像素主角待机/攻击/受击、像素黑熊待机/攻击/受击 | 新增 `arcade_combat/sheets/*` |
| 攻击 | 像素投射物、爆炸、命中闪光 | 新增 strip，不再用纯色方块 |
| UI | 简单血条、假玩法吐槽框、胜利/失败提示 | 保持“假玩法”粗糙感，但要是完整游戏素材 |

### 15.6 假玩法 TacticalCombat

| 类型 | 必要素材 | 当前兼容路径/新增路径 |
| --- | --- | --- |
| 棋盘 | 遗迹战旗背景、普通格、移动格、攻击格、选中格 | `assets/vertical_slice/tactical_combat/backgrounds/` 与 `tiles/` |
| 单位 | 主角、白魔、护卫、枪兵、暗术师、兽兵的 idle/attack/hit/down | `characters/unit_*_<action>_<frame2>.png` |
| 技能特效 | 斩击、灵枪、治疗、守备、暗术 | `effects/vfx_tac_*_<frame2>.png` |
| 战旗 UI | 顶栏、状态栏、详情栏、命令栏、按钮底、选中标记 | `ui/panels/`、`ui/icons/`、`ui/marker_selected.png` |
| 音频 | BGM、选择、移动、攻击、魔法、治疗、守备、受击、胜利 | `audio/tac_*.wav` |

### 15.7 通用 UI 与工程兜底素材

| 类型 | 必要素材 | 说明 |
| --- | --- | --- |
| 通用面板 | 大面板、小面板、tooltip、弹窗、标题条、遮罩 | 所有系统页面复用同一套九宫格素材 |
| 通用按钮 | 普通、hover、pressed、disabled、选中态 | 禁止每个页面单独画一套不兼容按钮 |
| 通用图标 | 返回、关闭、确认、取消、保存、读取、设置、分页箭头、排序、锁定、提示 | 做成 `system_icons_256.png` |
| 拖动/分页 | 滑条轨道、滑块、页码按钮、左右箭头、滚动条 | 对应引擎通用 Scroll/Pager 组件 |
| 鼠标反馈 | hover 边框、选中框、拖拽影子、不可用遮罩 | 编辑器和运行时 UI 都可复用 |
| 缺失资源占位 | missing texture、missing icon、missing portrait | 统一红黑警示风格，便于调试 |

## 16. 交付给我的方式

每次给我一批素材时，建议按这个结构：

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
    protag_basic1_strip.png
    ...
  sheet_params.yaml
```

`sheet_params.yaml` 示例：

```yaml
protag_idle_strip:
  cellWidth: 512
  cellHeight: 512
  frameCount: 8
  frameRate: 12
  pivot: bottom_center
  pivotX: 256
  pivotY: 430
  baselineY: 430
  renderOffset: [-256, -430]
  frameOffsetMode: fixed_canvas_no_per_frame_offset
  sourceFrames: source_frames/protag_idle/protag_idle_{frame3}.png
  assembledFromSourceFrames: true
  notes: "idle loop, magic sword glow"
```

我收到后会按下面顺序处理：

1. 先把 `source_frames` 放入规范源帧目录并逐帧验收。
2. 验收通过后拼接 `sheets/*_strip.png` 或 `vfx_sheets/*_strip.png`。
3. 用 Sprite Sheet Picker 或导入脚本生成 Clip。
4. 更新 `side_combat_tuning.yaml` 或对应场景组件；碰撞框、hurtbox、hitbox 独立配置，不按图片外框推断。比如 `dead` 使用 `1024x512` 图片，也不代表移动碰撞盒是 `1024x512`。
5. 烟测动画播放、碰撞框时机、UI 显示和打包依赖。
6. 记录到竖切工程文档。
