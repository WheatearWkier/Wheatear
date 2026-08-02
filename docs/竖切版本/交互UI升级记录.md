# 交互 UI 升级记录

日期：2026-07-28

本文记录本轮竖切 UI 的系统完整性升级，重点是把“看起来像页面”的占位 UI 推进到“玩家能看见、能点击、能切换、能观察状态变化”的阶段。

## 2026-07-29 音量设置补丁

- 默认音量当时下调为 `Master 50 / BGM 50 / SFX 50`，避免新开游戏时战斗音效过响；该结论已被 2026-07-30 的 VN 音量补丁替换，新默认值改为 `Master 85 / BGM 90 / SFX 85`。
- `AudioEngine::PercentToGain()` 新增听感曲线，设置页滑条在中间时会映射为更舒适的实际增益，而不是线性 50% 振幅。
- 设置页运行时补齐 `Master / BGM / SFX` 三条滑杆和对应加减按钮，滑动后会写入 `GameProgress::State.Settings`。
- 战斗调参表 `side_combat_tuning.yaml` 的跳跃、落地、攻击、受击音效基准音量整体下调，后续仍可在表中继续精调。

## 1. 本轮范围

- 战斗场景新增技能栏：正式 HUD 只显示 `S+J / U / I / L`，均有技能图标、槽位、按键标识、冷却遮罩、倒计时文本和悬浮 tooltip。
- `J` 普攻与 `K` 一段跳属于基础动作，不进入技能栏，只在教程提示和操作说明中出现。
- 魔剑技能树改为完整大画布：五大分支 `ME / MA / FU / MO / LI` 共 60 个分支节点，加 `magic_sword_core` 核心节点。
- 技能树支持拖动画布浏览；已学节点正常点亮，未学或后续章节节点灰暗并显示锁定遮罩。
- 点击任意技能节点会刷新右侧详情、材料需求和学习按钮状态。
- 装备界面分页改为真实切换：当前页只显示 4 个物品，拖动页码滑条或点击页码按钮会切换页面。
- 设置页滑条接入原生进度命令：文字速度和主音量滑条松开后会写入 `GameProgress::State.Settings`。

## 2. 生成脚本

本轮新增可复跑脚本：

```text
tools/vertical_slice/generate_interactive_skill_ui.py
```

脚本负责生成或更新：

- `WheatearEditor/assets/scenes/VerticalSliceSkillTree.wt`
- `WheatearEditor/assets/scenes/VerticalSliceEquipment.wt`
- `WheatearEditor/assets/scenes/VerticalSliceSettings.wt`
- `WheatearEditor/assets/scenes/SideCombatVerticalSlice.wt`
- `WheatearEditor/assets/scenes/SideCombatBeastPath.wt`
- `WheatearEditor/assets/vertical_slice/ui/skill_tree/*.png`
- `WheatearEditor/assets/vertical_slice/side_combat/ui/skill_*.png`

临时图标仍然是程序化像素资源，目标是保证可运行、可识别、可替换。后续正式美术替换必须沿用当前正式命名；若需要调整命名，必须同步迁移场景、脚本、调参表、生成脚本和打包清单，不再用旧路径做长期兼容。

## 3. 资源命名

战斗技能栏图标：

```text
assets/vertical_slice/side_combat/ui/icon_skill_basic_slash.png
assets/vertical_slice/side_combat/ui/icon_skill_launcher_slash.png
assets/vertical_slice/side_combat/ui/icon_skill_uppercut.png
assets/vertical_slice/side_combat/ui/icon_skill_magic_bolt.png
assets/vertical_slice/side_combat/ui/icon_skill_ally_support.png
assets/vertical_slice/side_combat/ui/icon_skill_break_limit.png
```

这些文件已按正式图标命名迁移，不再使用 `skill_j_*`、`skill_k_*` 这类键位型旧名；当前战斗 HUD 使用 `icon_skill_uppercut.png` 表示 `S+J` 裂空挑斩指令。

技能树图标：

```text
assets/vertical_slice/ui/skill_tree/skill_magic_sword_core.png
assets/vertical_slice/ui/skill_tree/skill_me_01.png
...
assets/vertical_slice/ui/skill_tree/skill_li_12.png
```

技能树辅助图：

```text
assets/vertical_slice/ui/skill_tree/skill_lock_overlay.png
assets/vertical_slice/ui/skill_tree/skill_selected_frame.png
```

## 4. 场景标签规范

战斗技能栏标签：

```text
SC_SkillBarPanel
SC_SkillSlot_SJ / SC_SkillIcon_SJ / SC_SkillCooldown_SJ / SC_SkillCooldownText_SJ / SC_SkillKey_SJ
SC_SkillSlot_U / ...
SC_SkillSlot_I / ...
SC_SkillSlot_L / ...
SC_SkillTooltipPanel / SC_SkillTooltipText
```

技能树标签：

```text
SkillTree_NetworkPanel
UISkillTreeViewComponent.Nodes[*].Id
UISkillTreeViewComponent.Nodes[*].ParentId
UISkillTreeViewComponent.Nodes[*].IconPath
```

现在技能树不再生成大量 `SkillTree_Node_*` 和 `SkillTree_Line_*` 场景实体。整棵树由 `SkillTree_NetworkPanel` 上的 `UISkillTreeViewComponent` 承载，节点 ID 仍使用策划数据里的 `ME-01`、`MA-01` 等稳定 ID。

装备分页标签：

```text
Equipment_Item_1_Frame / Equipment_Item_1 / Equipment_Item_1_Button
...
Equipment_Item_8_Frame / Equipment_Item_8 / Equipment_Item_8_Button
Equipment_PageSlider
Equipment_Button_Page1
Equipment_Button_Page2
```

## 5. 新增命令

```text
progression:select_skill_node:<skill_id>
progression:learn_selected_skill_v2
progression:equipment_page_slider:<value>
progression:set_text_speed:<value>
progression:set_master_volume:<value>
```

`RuntimeSceneLayer` 现在会在滑条释放时把当前数值附加到 `OnValueChangedFunction` 后面，因此滑条不再只是视觉组件。

## 6. UI 父子层级与裁剪

本轮 UI 系统新增了父子层级语义：

- `UIWidgetComponent.ParentEntity`：子 UI 保存父 UI 的 UUID，子节点坐标会按父节点矩形的局部空间解析。
- `UIPanelComponent.ClipChildren`：面板可以裁剪自己的子节点，渲染和输入命中都会遵守裁剪范围。
- `SkillTree_NetworkPanel` 已启用 `ClipChildren`，技能树节点、连线、锁定层、选中框和标签在运行时挂到该面板下。
- 魔剑技能树由原先的十字展开改为圆环/螺旋式分支布局，拖动画布时只有进入面板范围的部分可见。

这个能力后续应复用到背包滚动区、对话历史窗口、任务列表、商店列表和大型技能树。

## 7. 当前边界

- 技能树已经有完整视觉结构和学习状态，但材料消耗、前置技能、好感门槛仍是竖切级逻辑，正式版应拆成数据表。
- 装备页已经有真实分页，但装备实例、词条、穿戴变化和背包排序仍未正式化。
- 战斗技能栏目前使用横向冷却遮罩，不是最终的环形或径向冷却表现。后续可在 UI renderer 中增加专门的冷却材质或裁剪组件。
- 当前只实现了矩形父级裁剪；若后续要做圆形遮罩、环形冷却或复杂不规则遮罩，应继续扩展专用 UI 裁剪/遮罩组件。

## 8. 战斗与结算掉落图标化

- 战斗 HUD 的主要掉落曾从长文字改为三个像素图标槽；当前即时 HUD 已进一步迁移为 `SC_ItemSlot_1 / SC_ItemSlot_2 / SC_ItemSlot_3` 道具栏，掉落展示保留给结算页。
- 结算页新增 `Result_Drop_Core / Result_Drop_Sinew / Result_Drop_Claw` 图标槽，数量由本次 `RewardSummary` 解析后刷新。
- 掉落图标使用 hover-only 透明按钮，鼠标悬浮时显示材料名、本次数量、背包数量和用途说明。
- 规则统一为：空间紧张 UI 优先图标，必要说明进入 tooltip；正式页面不再用大段文本罗列材料。

## 8. 验证

- `Wheatear.sln` Debug x64 构建通过。
- 烟测通过：`VerticalSliceSkillTree.wt`、`VerticalSliceEquipment.wt`、`SideCombatVerticalSlice.wt` 均能启动并保持运行。
- 打包入口仍为 `assets/scenes/VisualNovelMainMenu.wt`。
- 打包成功输出：`Builds/Windows/Player/content.wtpack`。
- 本轮打包收集资源数：186。
- 打包版 `Builds/Windows/Player/WheatearSandbox.exe` 烟测通过。

## 9. 2026-07-28 修复记录

- 字体渲染撤掉临时运行时 MSDF，改为高分辨率 alpha 字形图集。原因是中文轮廓复杂，简化 MSDF 边着色容易污染笔画，且逐字生成会导致 VN 打字机和首次打开 UI 时卡顿。
- 文本 shader 改为 alpha atlas 填充 + 屏幕空间邻域采样描边，优先保证中文笔画稳定、不卡顿、可读。
- `Font` 增加 `PreloadText`，字体图集新增字形时支持合并上传；UI 系统在场景启动时预热静态文本，VN、战斗和成长系统在运行时设置文本前预热动态文本。
- 修复 `VerticalSliceSkillTree.wt` 中技能节点文本序列化损坏导致的 yaml-cpp 解析失败；UTF-8 场景文本重新检查后没有未闭合引号。
- 装备页补齐悬浮 tooltip：鼠标悬浮背包装备或已装备槽位时显示简短属性卡片。
- 装备页补齐装备 / 脱下流程：已装备物品会从背包网格隐藏，点击已装备槽位后可脱下，脱下后重新回到背包分页。
- 装备页背包网格改为按“拥有且未装备”的列表连续填格，避免穿上装备后中间出现空洞。
- `VerticalSliceSkillTree.wt` 和 `VerticalSliceEquipment.wt` 已重新烟测，均能直接加载并保持运行。
- 最新打包版 `Builds/Windows/Player/WheatearSandbox.exe` 已重新生成；包缓存为 `wtpack_aab1b8ee35109916`，入口场景、包内技能树场景和包内装备场景均通过 15 秒串行启动烟测。

## 10. 2026-07-28 微软雅黑预览

- `UITextComponent.FontPath` 默认值临时切换为 `C:/Windows/Fonts/msyh.ttc`，现有竖切 `.wt` 场景中的 UI 文本也同步切换到微软雅黑，用来对比中文笔画观感。
- `TextRenderer` 默认字体 fallback 顺序调整为优先查找系统中文字体，再回退到项目内 `NotoSansSC-VF.ttf` 和 `Open-Sans-2.ttf`。
- 这次没有把微软雅黑复制进 `assets/fonts`，因为系统字体不适合作为项目资源随包分发。若最终确定使用接近雅黑的风格，正式方案应改为字体别名/字体角色，并选用可再分发的中文字体文件。
- 微软雅黑预览版已重新打包并烟测：`Builds/Windows/Player/WheatearSandbox.exe` 主入口、技能树、装备页均能启动并保持运行；当前包缓存为 `wtpack_f841c128825798a8`。

## 11. 2026-07-28 内置中文 UI 字体与背包误翻页修复

- 正式项目字体改为 `assets/fonts/wqy-microhei.ttc`，使用开源的 WenQuanYi Micro Hei 作为第一版内置中文 UI 字体。
- 字体许可证、README、作者信息保存在 `assets/fonts/licenses/WenQuanYiMicroHei/`，打包器会把字体和许可证文本一起加入 `content.wtpack`。
- `UITextComponent.FontPath` 默认值、竖切场景文本、`TextRenderer` fallback 顺序均已切换到项目内字体；`C:/Windows/Fonts/msyh.ttc` 仅作为本机雅黑对照参考，不再写入场景。
- 修复装备背包点击物品后跳页的问题：背包页现在按“拥有且未装备”的连续列表计算，点击物品只选中当前物品，不再根据装备表里的旧 `Page` 字段强制跳页。
- UI 输入系统改为一次点击只触发最上层按钮，避免透明按钮或重叠控件同时响应。

## 12. 2026-07-29 Canvas 规范迁移

- WheatearSandbox 主流程相关场景已统一补入 `WT_UI_Canvas` 根画布，非 Canvas 的 UIWidget 都挂到该根画布下。
- 编辑器右键创建 UI 的规则收紧：Canvas 可直接创建，其他 UI 控件只能作为 Canvas 或 Canvas 子控件创建。
- Inspector 添加 UI 表现组件时会检查 Canvas 归属，减少普通场景实体误挂 UI 组件的情况。
- UI Canvas Editor 只显示当前 Canvas 下的控件，选中 Canvas 子控件时会高亮其所属 Canvas；`Use Selected Canvas` 可以从子控件反查 Canvas。
- 打包版已重新生成：`Builds/Windows/Player/WheatearSandbox.exe`，本轮包内资源数为 191。
- 烟测通过：入口场景、包内技能树、包内装备页、包内正式横板战斗场景均能启动并保持运行。

## 13. 2026-07-29 UI 复用组件第一轮

- `UIPanelComponent` 新增 `Draggable`、`ConstrainDragToParent` 和 `DragHandleHeight`，面板可以在运行时通过勾选项获得通用拖动能力。
- 拖动范围默认限制在父控件内；直接挂在 Canvas 下时相当于限制在整张画布内。
- 新增 `UIPagerComponent` 和 `UIPageItemComponent`，用于总页数已知的分页内容，例如信件、图鉴、任务、存档页。
- 通用分页按钮命令：`ui:pager:@<PagerUUID>:next`、`ui:pager:@<PagerUUID>:prev`、`ui:pager:@<PagerUUID>:page:<number>`。
- `UIWidgetLayout` 会根据分页器当前页自动隐藏或显示 `UIPageItemComponent` 内容项，避免每个页面重复写专用显示逻辑。
- 编辑器 UI 轮廓优化：纯文本控件使用虚线框，避免和 Panel 实体的实线边框混淆。
- Hierarchy 和 Canvas Editor 增强 Canvas 选中提示：选中 Canvas 或其子控件时，所属 Canvas 会以高亮背景/边框提示。
- 打包器调用 `WheatearSandbox` 构建时改为 `LinkIncremental=false`，避免 Debug 增量链接偶发 `LNK1168` 导致打包失败。
- 最新打包版已重新生成：`Builds/Windows/Player/WheatearSandbox.exe`，入口场景、技能树场景、装备页场景均通过隐藏窗口启动烟测。

## 14. 2026-07-29 ScrollView 与装备分页复用

- `UISliderComponent` 继续用于数值滑条，例如文本速度、音量、战斗参数等连续数值。
- 新增 `UIScrollViewComponent`，专门用于长内容滚动区：长文本、背包列表、任务列表、图鉴、邮件列表都可以复用它，而不是在业务代码里硬写拖动逻辑。
- ScrollView 需要搭配 `UIPanelComponent.ClipChildren` 使用；子 UI 可以超出面板高度，运行时根据 `OffsetY` 在父面板内裁剪显示。
- ScrollView 支持鼠标滚轮和拖动滚动条，编辑器 Inspector 可调 `ContentHeight`、`WheelStep`、`ScrollbarWidth`、是否启用滚轮、是否显示滚动条。
- `UIPagerComponent/UIPageItemComponent` 专门用于已知页数的分页内容，和 ScrollView 分工明确：前者按页切，后者连续滚。
- 装备页现在同步到通用 Pager，页码按钮走 `ui:pager:@<PagerUUID>:page:<n>`；背包物品按未装备列表连续填格。
- 本轮打包版已更新并烟测：主菜单、装备页、技能树场景均能启动并保持运行。

## 15. 2026-07-29 UI 模板与竖切 Demo 迁移

- 编辑器 Scene Hierarchy 的右键 `UI / Templates` 新增三类组合模板：`Titled Scroll Text`、`Paged Grid`、`Paged Inventory Grid`。
- 模板创建使用专门的复合创建命令，整套 UI 可以作为一次操作撤销/复原；子控件会写入稳定 UUID 父引用和 Tag 兜底。
- `Titled Scroll Text` 会自动生成标题、ScrollView、正文 Text，适合教程、任务说明、信件正文、VN 历史等长文本页面。
- `Paged Grid` 和 `Paged Inventory Grid` 会自动生成 Pager、上一页/下一页按钮、页码文本、两页 Slot，并给每个 Slot 写入 `UIPageItemComponent`。
- 竖切装备页运行时迁移到可复用结构：装备详情和材料说明被包进 ScrollView，可用滚轮/滚动条查看，背包分页继续走通用 `Equipment_Pager`。
- 竖切存档页运行时迁移到可复用结构：存档 Pager 控制 1 号槽页面和后续槽位预告页面，页码按钮使用 `ui:pager:@<PagerUUID>:prev/next`。
- 最新打包版已重新生成，主菜单、装备页、存档页均通过 8 秒隐藏窗口启动烟测。

## 16. 2026-07-29 VN BGM 与战斗道具栏

- VN 脚本新增 `@music` / `@bgm` 命令，支持按剧情段切换循环 BGM，并可写入面向玩家显示的曲名。
- VN 运行时新增左上角 BGM 提示：音乐切换时短暂滑入并淡出，文本显示当前 BGM 名称。
- 新增第一批程序化 BGM 占位资源：日常上学、异兆、异界森林、青梅被夺、吐槽假玩法、教程决意、边境旅路。
- WheatearEditor 新增独立 `VN Script Editor` 窗口，可在时间线里管理台词、人物、表情、背景、BGM、选择项和跳转。
- 战斗 HUD 左侧三格从掉落显示改为道具栏，占位为 `回复药`、`凝神药剂`、`裂空爆弹`，角标显示快捷键 `1 / 2 / 3`。
- 掉落信息后续应继续进入结算页和 hover tooltip，不再占用战斗中的即时道具栏。
- 最新编辑器已重新生成：`Builds/Windows/Editor/WheatearEditor.exe`。
- 最新玩家包已重新生成：`Builds/Windows/Player/WheatearSandbox.exe`，本轮 `content.wtpack` 收集 338 个引用资源；编辑器和玩家包均通过隐藏窗口启动烟测。

## 17. 2026-07-29 Side Combat 调参面板

- 新增 `Side Combat Tuning Editor`，战斗手感不再只能靠手改 YAML。
- `Feel` 页签直接暴露跳跃、重力、滞空、上挑、空中普攻、空中追击和断限追击等核心手感参数。
- `Rules` 页签暴露 Boss 保护槽、伤害规则、受击无敌、敌人 AI、拾取物和纵深移动参数。
- `Attacks` 页签可以逐招式编辑 hitbox、伤害、前后摇、取消窗口、击飞、滞空、特效和音效。
- `Skills / Progression` 页签可以维护技能显示、绑定招式、章节解锁和关卡 profile。
- Inspector 仍保留 `Raw Side Combat Tuning YAML` 兜底文本编辑入口。

## 18. 2026-07-29 大型 UI 与技能树性能优化

- `UIWidgetLayout` 新增“与视口和父级裁剪区域相交”的通用判断，框外控件不再进入渲染和鼠标命中流程。
- `UISystem::RenderUI` 在调用具体绘制函数前先做可见性和裁剪剔除，避免离屏技能节点、文字、图标继续测量和绘制。
- `UIInputSystem` 同步使用同一套裁剪规则，已经被 `ClipChildren` 裁掉的按钮、滑条、滚动条不会再抢鼠标事件。
- `SkillTree_NetworkPanel` 的运行时刷新加入脏标记：只有拖动画布、选中节点、解锁技能或重新加载场景时才重算节点和连线。
- 技能树刷新从“每次按名字扫描全场景”改为本帧一次性建立 Tag 索引，再批量更新节点、按钮、锁定层、选中框和连线。
- 技能树节点与连线保留少量预加载边距，避免拖动到边缘时突然闪现；真正超出面板的部分仍由父级裁剪和渲染剔除负责。
- 这个方案后续也适用于大地图、任务节点、图鉴节点、巨大背包页等“大画布只显示局部”的 UI。
- 最新编辑器与玩家包已重新生成；`content.wtpack` 本轮收集 338 个引用资源。
- 烟测通过：`WheatearEditor.exe`、玩家包主入口、玩家包技能树场景均能隐藏窗口启动并保持运行。

## 19. 2026-07-29 真正的 SkillTreeView 渲染器

- `Renderer2D` 新增 `DrawPolyline`，用于把多段线作为基础渲染能力开放给 UI 和后续调试绘制。
- `UIRenderer` 新增 `DrawUIBezier`，技能树连线可以使用曲线采样，不再只能用直线或大量独立 Line 实体拼接。
- 新增 `UISkillTreeViewComponent`：单个控件持有整棵技能树的节点、图标、父子关系、可用/已学/锁定/选中状态、拖动偏移和颜色参数。
- `UISkillTreeViewComponent` 已接入场景序列化、复制、编辑器 Inspector、运行时渲染、鼠标悬浮、拖动画布和点击选中。
- `VerticalSliceSkillTree.wt` 已改成直接使用 `UISkillTreeViewComponent`，旧的 `SkillTree_Node_*` / `SkillTree_Line_*` 实体已从该场景生成结果中移除。
- 运行时仍保留旧实体技能树的兼容迁移逻辑：如果老场景只有 `SkillTree_NetworkPanel`，系统会自动挂载新组件并隐藏旧节点实体。
- 最新 `WheatearEditor.exe` 和 `WheatearSandbox.exe` 已重新构建；编辑器和 `VerticalSliceSkillTree.wt` 均通过 6 秒隐藏窗口启动烟测。

## 20. 2026-07-30 UI Path 与技能树曲线升级

- 新增 `UIPathComponent`：用于在 UI 局部坐标内绘制 Polyline、Quadratic Bezier、Cubic Bezier 路径，支持线宽、采样段数、闭合、发光颜色和发光倍率。
- `UIPathComponent` 已接入运行时渲染、场景序列化、场景复制、编辑器撤销快照、Inspector 和 Scene Hierarchy 右键 UI 创建菜单。
- `UIRenderer::DrawUIBezier` 内部改为复用统一路径采样辅助逻辑，后续技能树、地图路线、任务路线和事件图预览不再各写一套曲线采样。
- `UISkillTreeViewComponent` 新增 `VirtualizationMargin`、`LineSegments`、`BackgroundRingCount`、`DrawLineGlow` 和 `LineGlowColor`，技能树连线从硬直控制点改为按节点相对中心的径向弯曲，更接近圆环/螺旋分支观感。
- 技能树连线边缘缩进改为按节点半径约束，避免线段与圆形节点之间出现明显断开。
- `VerticalSliceSkillTree.wt` 默认参数已更新：更小的节点边缘缩进、更强的曲线弯曲、更多采样段和线条发光，用于当前竖切技能树页。
- 当前仍然使用 Renderer2D line batch 绘制曲线采样段；如果以后需要更宽的发光带、箭头流动、描边贴图，再升级到 ribbon mesh 或专门 path 材质。

## 21. 2026-07-30 运行时中文文本编码修复

- 修复新打包版主菜单、右下角菜单按钮、Panel 文本和 VN UI 中文显示乱码的问题：`VisualNovelMainMenu.wt` 与 12 个竖切 UI 场景曾以 GBK/ANSI 字节保存，运行时按 UTF-8 读取后会显示为乱码。
- 已将这些 `.wt` 场景统一转换为 UTF-8；`assets/scenes`、`assets/vn`、`assets/events` 和 `assets/vertical_slice/data` 下的运行时文本资产已通过 UTF-8 校验。
- 后续规则：`.wt`、`.vn`、`.wts`、`.json`、`.yaml` 等运行时文本资产必须保存为 UTF-8；打包器只收集和写包，不负责把 GBK/ANSI 自动转码。
- 最新玩家包已重新生成：`Builds/Windows/Player/WheatearSandbox.exe`，`content.wtpack` 本轮收集 450 个引用资源；包内确认包含 `VisualNovelMainMenu.wt`、`VerticalSliceTurnCombat.wt` 和 `vertical_slice_flow.wts`。
- 打包版 `WheatearSandbox.exe` 通过 8 秒隐藏窗口启动烟测。

## 22. 2026-07-30 回合制中文化与旧编码清理

- 项目自有文本文件完成一次性编码清理：源码、编辑器面板、着色器、脚本、文档和运行时资产已统一为 UTF-8；旧 GBK/ANSI 文件和局部损坏注释已处理，后续 `apply_patch` 不应再被这类文件反复绊住。
- 回合制战斗玩家可见文本中文化：技能名、技能说明、行动提示、目标提示、状态栏、行动顺序、胜利/失败文本和场景初始 UI 文本均改为中文。
- 修复回合制目标点击无效：场景目标按钮现在直接发送真实战斗实体 Tag；`TurnCombatSystem` 也新增目标解析兜底，可从目标按钮名反查战斗单位。
- 静态检查确认 `VerticalSliceTurnCombat.wt` 内 6 个 `turn:target:*` 命令均能匹配到战斗实体，玩家可见文本无英文残留。
- 最新编辑器、运行时和玩家包已重新构建；打包版主入口与打包版回合制场景均通过 8 秒隐藏窗口启动烟测。

## 23. 2026-07-30 Batch01 VN 美术实装

- 已将 `vn/backgrounds` 与 `vn/portraits` 中的 Batch01A/Batch01B 共 21 张 PNG 复制到正式工程目录 `WheatearEditor/assets/vertical_slice/vn/`，覆盖旧占位 VN 图。
- 资源规格校验通过：6 张背景为 1920x1080，15 张立绘为 1024x1536 透明 PNG；源文件与工程目标文件哈希一致。
- `vertical_slice_intro.vn` 在路口异常段切换到 `bg_modern_schoolroad_unease.png`；`vertical_slice_post_fake.vn` 和 `VerticalSlicePostFake.wt` 起始背景改为 `bg_forest_after_bear.png`。
- 精确旧名扫描确认运行时场景、VN 脚本和事件脚本中不再引用旧 demo VN 背景/立绘路径。
- 最新玩家包已重新生成：`Builds/Windows/Player/WheatearSandbox.exe`，`content.wtpack` 本轮收集 452 个引用资源；打包版主入口、序章 VN 和假玩法后 VN 均通过 8 秒隐藏窗口启动烟测。

## 24. 2026-07-30 VN 音量设置与默认音量回调

- 默认设置从 `Master 50 / BGM 50 / SFX 50` 调整为 `Master 85 / BGM 90 / SFX 85`，避免 `PercentToGain()` 听感曲线叠乘后新游戏 BGM 过小。
- VN 设置覆盖层运行时补齐主音量、BGM 音量和音效音量三条滑条，并带对应 `- / +` 微调按钮。
- VN BGM 每帧根据 `GameProgress::State.Settings` 重算增益，玩家在 VN 设置页调整主音量或 BGM 音量后，正在播放的 BGM 会立即跟随变化。
- 据点系统设置页的 Master / BGM / SFX 标签改为中文，并确认设置状态文案不再描述为“未来才接入 AudioEngine”。
- 最新玩家包已重新生成：`Builds/Windows/Player/WheatearSandbox.exe`，`content.wtpack` 本轮收集 452 个引用资源；打包版主入口和序章 VN 均通过 8 秒隐藏窗口启动烟测。
