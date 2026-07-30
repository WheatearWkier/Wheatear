# 竖切美术资源 Sheet 生产清单

更新时间：2026-07-30

本文档用于给 Gemini Nano Banana 分批生成正式替换素材。目标是从现在的临时单张 PNG，逐步升级到“商业游戏项目可维护”的图集和横向帧序列资源。

参考资料位于：

```text
docs/参考分析/美术参考/
```

这些参考只用于理解方向：VN 要高清漂亮，战斗要明亮爽快，系统 UI 布局保持现有功能结构但统一皮肤。不要复刻商业作品角色、Logo、文字、具体 UI 图案或可识别元素。

## 1. 总规则

### 1.1 风格分层

| 模块 | 风格 | 规格重点 |
| --- | --- | --- |
| VN | 高清二次元 Galgame | 1920x1080 背景，透明半身立绘，精细表情 |
| 正式横板战斗 SideCombat | 明亮爽快二次元横板动作，比例参考 DNF 类横板 | 大尺寸透明 sprite strip，清晰轮廓，高帧率关键动作 |
| 系统 UI | 二次元幻想 RPG，布局沿用当前 demo | 图标化、少文字、可做 atlas，悬浮 tooltip 承载说明 |
| 假玩法 ArcadeCombat | 像素风 | 小体积 sprite strip 和图标，占位但完整 |
| 假玩法 TurnCombat | 像素风 + JRPG 回合制 UI | 角色小人、技能特效、命令图标 |

### 1.2 通用负面要求

所有提示词都追加：

```text
no text, no logo, no watermark, no signature, no UI letters, no copyright character, no recognizable franchise design, no cropped body, no blurry edge, no inconsistent frame size, no changing costume between frames
```

### 1.3 Sprite Strip 硬性规格

横板战斗、回合制、假玩法动画都优先给横向长条图：

```text
single horizontal sprite strip, transparent background, exactly N frames, equal cell size, one action only, character feet aligned to the same baseline, same camera angle, same scale, no grid lines, no frame numbers
```

每张 sheet 旁边最好人工记录一份参数，后续导入 Wheatear 的 Sprite Sheet Picker：

```yaml
cellWidth: 384
cellHeight: 384
frameCount: 8
frameRate: 18
rowOrigin: top
pivot: bottom_center
baselineY: 320
```

### 1.4 文件命名

正式替换素材统一放入下列目录。收到素材后我会按这些路径接入工程：

```text
WheatearEditor/assets/vertical_slice/source_art/
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
- 回合制和假玩法像素动画使用 `px_<subject>_<action>_strip.png`。
- UI 图集使用 `<domain>_<content>_<cell_or_size>.png` 或 `<domain>_<content>_atlas.png`，例如 `battle_skill_icons_128.png`。
- 旧名只允许出现在“迁移映射/历史备注”里，不作为新素材交付名。

### 1.5 已有资源的迁移规则

| 情况 | 处理方式 | 示例 |
| --- | --- | --- |
| 场景里已经引用的单张图 | 重命名到正式规范名，并同步改 `.wt` / `.vn` / `.wts` 引用 | `school_road.png` -> `bg_modern_schoolroad_morning.png` |
| 现有逐帧动画序列 | 先记录旧序列映射，再迁移为正式 `_strip` 或正式帧序列名 | `player_basic1_1.png` -> `protag_basic1_strip.png` |
| 新增正式横向 sheet | 使用 `_strip` 后缀，旁边提供参数 | `protag_basic1_strip.png` + `sheet_params.yaml` |
| 新增 UI 图集 | 使用 `_atlas` 或规格后缀 | `battle_skill_icons_128.png` |
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

尺寸：`1024x1536` 或 `1200x1800`，透明 PNG，半身立绘，三分之二正面。每个角色保持同一画布、同一头身比例、同一站位。

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
original anime visual novel half-body character portrait, [CHARACTER DESCRIPTION], transparent background, clean cel shading, beautiful polished galgame style, expressive eyes, consistent costume, front three-quarter view, high resolution, no text
```

如果要做“眨眼/呼吸/头发轻动”的 VN 表情动画，每个表情额外生成横向 4 帧 strip：

```text
single horizontal visual novel portrait animation strip, transparent background, exactly 4 frames, same character and expression, subtle breathing and blinking only, equal cell size 1024x1536, no camera movement, no text
```

## 4. Batch 02：VN UI 高清皮肤

目标：参考 `VN的UI界面.png` 的信息层级：底部半透明对话框、左下小头像、底部一排图标按钮、BGM 提示条。不要复刻原图图案。

| 资源 ID | 文件名 | 尺寸 | 说明 |
| --- | --- | --- | --- |
| UI_VN_TEXTBOX | `ui/atlases/vn_textbox_panel.png` | 1600x320 | 对话框底板，可九宫格 |
| UI_VN_NAMEPLATE | `ui/atlases/vn_nameplate.png` | 360x96 | 角色名牌 |
| UI_VN_BUTTON_ATLAS | `ui/atlases/vn_command_icons_128.png` | 1024x256 | save/load/qsave/qload/system/history/auto/skip 图标 |
| UI_VN_BGM_NOTICE | `ui/atlases/vn_bgm_notice_panel.png` | 560x96 | 左上角 BGM 提示条 |
| UI_VN_CHOICE_PANEL | `ui/atlases/vn_choice_panel.png` | 900x120 | 选择项按钮底 |

Prompt：

```text
original anime fantasy visual novel UI asset, elegant translucent rose-gold and dark glass panel, clean decorative edges, no text, no icons unless requested, game-ready PNG, soft highlight, readable over bright and dark backgrounds
```

## 5. Batch 03：正式横板主角动作 Sheet

风格：明亮爽快二次元横板动作。比例参考 DNF 类横板角色，不是马里奥平台跳跃。角色在有纵深的横板房间中战斗，动作轮廓要适合空中连击。

主角 cell：`384x384`，透明背景，脚底 baseline 约 `y=320`，pivot 为底部中心。横向 strip，每张 sheet 只放一个动作。

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
original anime 2D side-scrolling action game sprite strip, young male magic swordsman, damaged modern school uniform mixed with fantasy black-silver magic sword, bright stylish anime action style, readable DNF-like side-view proportions, transparent background, exactly [N] frames, equal 384x384 cells, one horizontal row, feet aligned to same baseline, clean silhouette, smooth high-frame action, no text
```

## 6. Batch 04：正式横板敌人与 Boss Sheet

### 6.1 爪兽小怪

cell：`320x256`，透明背景。

| 动作 | 文件名 | 帧数 | FPS |
| --- | --- | ---: | ---: |
| idle | `side_combat/sheets/en_claw_beast_idle_strip.png` | 8 | 12 |
| run | `side_combat/sheets/en_claw_beast_run_strip.png` | 8 | 18 |
| claw_attack | `side_combat/sheets/en_claw_beast_attack_strip.png` | 8 | 18 |
| hit | `side_combat/sheets/en_claw_beast_hit_strip.png` | 5 | 18 |
| launched | `side_combat/sheets/en_claw_beast_launched_strip.png` | 4 | 12 |
| fall | `side_combat/sheets/en_claw_beast_fall_strip.png` | 4 | 12 |
| dead | `side_combat/sheets/en_claw_beast_dead_strip.png` | 8 | 12 |

Prompt：

```text
original anime fantasy side-scrolling monster sprite strip, small agile wolf-like claw beast, dark fur, cyan magic eyes, readable silhouette, bright polished action game style, transparent background, exactly [N] frames, equal 320x256 cells, one horizontal row, same scale, no text
```

### 6.2 黑熊丈夫 Boss

cell：`640x448`，透明背景。Boss 比主角大约 1.8 到 2.2 倍，但不要塞满画布，攻击动作允许爪子和魔光延展。

| 动作 | 文件名 | 帧数 | FPS |
| --- | --- | ---: | ---: |
| idle | `side_combat/sheets/boss_bear_husband_idle_strip.png` | 10 | 10 |
| walk | `side_combat/sheets/boss_bear_husband_walk_strip.png` | 10 | 14 |
| claw_attack | `side_combat/sheets/boss_bear_husband_claw_attack_strip.png` | 10 | 18 |
| charge_windup | `side_combat/sheets/boss_bear_husband_charge_windup_strip.png` | 8 | 14 |
| charge_loop | `side_combat/sheets/boss_bear_husband_charge_loop_strip.png` | 6 | 18 |
| shockwave | `side_combat/sheets/boss_bear_husband_shockwave_strip.png` | 12 | 18 |
| roar | `side_combat/sheets/boss_bear_husband_roar_strip.png` | 8 | 12 |
| hit | `side_combat/sheets/boss_bear_husband_hit_strip.png` | 5 | 16 |
| launched | `side_combat/sheets/boss_bear_husband_launched_strip.png` | 4 | 10 |
| fall | `side_combat/sheets/boss_bear_husband_fall_strip.png` | 5 | 10 |
| break_stun | `side_combat/sheets/boss_bear_husband_break_stun_strip.png` | 8 | 12 |
| dead | `side_combat/sheets/boss_bear_husband_dead_strip.png` | 10 | 10 |

Prompt：

```text
original anime fantasy side-scrolling boss sprite strip, massive corrupted black bear husband, blue magical veins, huge claws, powerful readable silhouette, bright polished action game style, transparent background, exactly [N] frames, equal 640x448 cells, one horizontal row, consistent ground baseline, no text
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
original anime 2D side-scrolling action game battle stage, bright and stylish fantasy black forest path, slight top-down perspective, clear horizontal combat room, readable floor plane with subtle perspective guide lines, layered parallax background, glowing cyan magical plants, dark trees but vibrant action-game lighting, polished anime game background, no characters, no text
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
original anime fantasy action game stage prop, [PROP DESCRIPTION], black forest magic theme, clean silhouette, transparent background, consistent lighting with bright cyan magical accents, game-ready 2D prop, no text
```

### 7.3 掉落物与可拾取素材

掉落物需要“世界内小 sprite”和“UI 图标”两套。世界内 sprite 用于战斗场景地上掉落，UI 图标用于结算、背包和 tooltip。

| 掉落 | 世界内文件 | UI 图标文件 | 说明 |
| --- | --- | --- | --- |
| 魔核碎片 | `side_combat/pickups/pickup_magic_core_strip.png` | `ui/atlases/battle_drop_icons_128.png` | 8 帧闪光循环 |
| 兽筋 | `side_combat/pickups/pickup_beast_sinew_strip.png` | `ui/atlases/battle_drop_icons_128.png` | 6 帧轻微浮动 |
| 熊爪 | `side_combat/pickups/pickup_bear_claw_strip.png` | `ui/atlases/battle_drop_icons_128.png` | 6 帧闪烁 |
| 金币/通用货币 | `side_combat/pickups/pickup_coin_strip.png` | `ui/atlases/battle_drop_icons_128.png` | 8 帧旋转 |
| 装备箱 | `side_combat/pickups/pickup_equipment_box_strip.png` | `ui/atlases/battle_drop_icons_128.png` | 8 帧发光 |

Prompt：

```text
original anime fantasy action RPG pickup item sprite strip, [ITEM DESCRIPTION], transparent background, exactly [N] frames, equal 128x128 cells, one horizontal row, subtle floating and glowing animation, readable at small size, no text
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
original anime action game overlay effect, [EFFECT DESCRIPTION], transparent background, clean high contrast, designed for fast combat feedback, no text
```

## 8. Batch 06：横板战斗 VFX Sheet

VFX 单独 sheet，便于和角色动画分离，也便于后续用 `SpriteAnimatorComponent` 和动画事件控制。

| VFX | 文件名 | Cell | 帧数 | FPS | 用途 |
| --- | --- | --- | ---: | ---: | --- |
| basic_slash | `side_combat/vfx_sheets/vfx_basic_slash_strip.png` | 512x256 | 8 | 30 | 三段斩通用剑气 |
| launcher_slash | `side_combat/vfx_sheets/vfx_launcher_slash_strip.png` | 512x512 | 10 | 30 | 上挑竖向剑气 |
| air_slash | `side_combat/vfx_sheets/vfx_air_slash_strip.png` | 512x384 | 8 | 30 | 空中跳斩 |
| break_limit | `side_combat/vfx_sheets/vfx_break_limit_strip.png` | 640x640 | 16 | 30 | 断限追击，魔法阵碎裂 |
| magic_bolt | `side_combat/vfx_sheets/vfx_magic_bolt_strip.png` | 256x128 | 8 | 24 | 飞行魔法弹 |
| magic_impact | `side_combat/vfx_sheets/vfx_magic_impact_strip.png` | 512x512 | 10 | 30 | 魔法命中爆裂 |
| hit_spark_light | `side_combat/vfx_sheets/vfx_hit_spark_light_strip.png` | 384x384 | 8 | 30 | 轻命中火花 |
| hit_spark_heavy | `side_combat/vfx_sheets/vfx_hit_spark_heavy_strip.png` | 512x512 | 10 | 30 | 重命中火花 |
| landing_dust | `side_combat/vfx_sheets/vfx_landing_dust_strip.png` | 512x256 | 8 | 24 | 落地烟尘 |
| pickup_glow | `side_combat/vfx_sheets/vfx_pickup_glow_strip.png` | 256x256 | 8 | 12 | 掉落物吸附光 |

VFX Prompt：

```text
original anime 2D action game visual effect sprite strip, [VFX DESCRIPTION], cyan blue magic and black-silver sword energy, transparent background, exactly [N] frames, equal [CELL] cells, one horizontal row, clean additive-looking shapes, no text
```

## 9. Batch 07：横板战斗 HUD 与图标 Atlas

目标：参考明亮爽快二次元横板 HUD。不要学页游满屏红点，优先图标和 tooltip。

| Atlas | 文件名 | 规格 | 内容 |
| --- | --- | --- | --- |
| battle_skill_icons | `ui/atlases/battle_skill_icons_128.png` | 8x4, cell 128 | 主动技能、支援、断限、锁定灰图 |
| battle_item_icons | `ui/atlases/battle_item_icons_128.png` | 4x2, cell 128 | 1/2/3 道具栏占位、药水、卷轴 |
| battle_drop_icons | `ui/atlases/battle_drop_icons_128.png` | 6x2, cell 128 | 魔核、兽筋、兽爪、金币、装备箱 |
| battle_hud_panels | `ui/atlases/battle_hud_panels.png` | 2048x1024 | 血条框、Boss 条、技能栏底板、连击牌 |

Prompt：

```text
original anime fantasy action RPG UI icon atlas, bright polished game UI, sharp readable icons, cyan blue magic sword theme with warm gold accents, transparent background, no text, no letters, no numbers, consistent icon perspective, 128x128 cells
```

## 10. Batch 08：系统 UI Atlas

布局保持当前 demo：据点、技能树、装备、关系、支援、结算、存档、设置。但底板、按钮、节点、图标统一皮肤。

| Atlas | 文件名 | 规格 | 内容 |
| --- | --- | --- | --- |
| common_panels | `ui/atlases/common_panels.png` | 2048x2048 | 大面板、二级面板、tooltip、弹窗、标题条 |
| common_buttons | `ui/atlases/common_buttons.png` | 2048x1024 | 普通/hover/pressed/disabled 按钮，页码按钮 |
| system_icons | `ui/atlases/system_icons_128.png` | 8x8, cell 128 | 背包、技能树、装备、关系、保存、设置、返回 |
| skill_tree_nodes | `ui/atlases/skill_tree_nodes_192.png` | 4x4, cell 192 | 已学、可学、锁定、选中、核心节点 |
| equipment_icons | `ui/atlases/equipment_icons_128.png` | 8x4, cell 128 | 武器、防具、饰品、材料、强化石 |
| relationship_icons | `ui/atlases/relationship_icons_128.png` | 8x2, cell 128 | 白魔、护卫、黑魔、青梅、好感阶段 |
| result_badges | `ui/atlases/result_badges_192.png` | 4x2, cell 192 | S/A/B/C 评分、首通、连击、掉落 |

Prompt：

```text
original anime fantasy RPG system UI atlas, elegant dark glass panels with cyan magic sword glow and warm gold trims, clean readable shapes, game-ready interface assets, transparent background where possible, no text, no logo, consistent style
```

## 11. Batch 09：TurnCombat 像素资源

回合制是假玩法之一，像素风即可，但要完整。cell：`96x96` 或 `128x128`，透明背景。

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
original pixel art JRPG turn-based battle sprite strip, [CHARACTER OR EFFECT], transparent background, exactly [N] frames, equal [CELL] cells, one horizontal row, clean readable 32-bit pixel style, no text
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
original cute pixel art fantasy mini-game sprite strip, intentionally simple but charming, [ASSET DESCRIPTION], transparent background, exactly [N] frames, equal [CELL] cells, one horizontal row, no text
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
single horizontal pixel art tactical RPG sprite strip, transparent background, exactly 4 frames, equal 128x128 cells, one action only, same unit scale, same baseline, no text
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
| VN 控制栏 | 保存、读取、快存、快读、系统、历史、自动、跳过、文本速度图标 | 做成 `vn_command_icons_128.png`，旧 `icon_save.png` 等单图只作为迁移源，正式场景引用改到图集或规范单图 |
| BGM 提示 | 左上角 BGM 入场提示条、音符图标、淡入淡出遮罩 | 新增 `vn_bgm_notice_panel.png` 和 `icon_music_note.png` |
| 转场 | 黑幕淡入、白闪、章节标题底纹、路线确认提示 | 新增 `ui/atlases/transition_overlays.png` |

### 15.2 据点与系统页面

| 页面 | 必要素材 | 说明 |
| --- | --- | --- |
| 据点 Hub | 营地背景、主面板、功能入口图标、当前路线卡片、资源栏图标 | 迁移到正式 UI atlas / panel 命名；场景引用同步更新 |
| 副本选择 | 副本卡片、难度角标、奖励预览图标、路线确认按钮 | 后续章节复用，图标优先 atlas 化 |
| 技能树 | 大背景魔法阵、圆形节点、锁定/可学/已学/选中状态、曲线路径、分支徽记、右侧详情面板 | 迁移到 `skill_tree_nodes_192.png`、正式技能图标 atlas 或规范单图 |
| 装备 | 背包格子、已装备槽、装备图标、材料图标、强化按钮、对比箭头、悬浮 tooltip 面板 | 迁移到 `equipment_icons_128.png`、正式材料图标 atlas 或规范单图 |
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
| 道具栏 | 1/2/3 道具槽、药水、专注药剂、爆裂道具、数量角标 | 迁移到正式 `battle_item_icons_128.png` 或规范单图，旧 `item_slot_*` 引用清零 |
| 掉落 | 世界内掉落 sprite、吸附光效、结算图标、背包图标 | 世界内掉落用 `pickup_*_strip.png`，UI 图标用 `battle_drop_icons_128.png` |
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
| 通用图标 | 返回、关闭、确认、取消、保存、读取、设置、分页箭头、排序、锁定、提示 | 做成 `system_icons_128.png` |
| 拖动/分页 | 滑条轨道、滑块、页码按钮、左右箭头、滚动条 | 对应引擎通用 Scroll/Pager 组件 |
| 鼠标反馈 | hover 边框、选中框、拖拽影子、不可用遮罩 | 编辑器和运行时 UI 都可复用 |
| 缺失资源占位 | missing texture、missing icon、missing portrait | 统一红黑警示风格，便于调试 |

## 16. 交付给我的方式

每次给我一批素材时，建议按这个结构：

```text
Batch03_SideCombat_Protag/
  protag_idle_strip.png
  protag_run_strip.png
  protag_basic1_strip.png
  ...
  sheet_params.yaml
```

`sheet_params.yaml` 示例：

```yaml
protag_idle_strip:
  cellWidth: 384
  cellHeight: 384
  frameCount: 8
  frameRate: 12
  pivot: bottom_center
  baselineY: 320
  notes: "idle loop, magic sword glow"
```

我收到后会按下面顺序处理：

1. 放入规范资源目录。
2. 用 Sprite Sheet Picker 或导入脚本生成 Clip。
3. 更新 `side_combat_tuning.yaml` 或对应场景组件。
4. 烟测动画播放、碰撞框时机、UI 显示和打包依赖。
5. 记录到竖切工程文档。
