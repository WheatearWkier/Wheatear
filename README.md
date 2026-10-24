# Wheatear

Wheatear 是一个基于 C++20 开发的自研 2D 游戏引擎与编辑器项目。

项目以 2D 游戏内容生产为主要目标，同时包含基础 3D 渲染能力。当前仓库已经完成一套可运行的竖切演示，用于验证场景系统、资源管线、编辑器工具、事件脚本、战斗模块和运行时打包之间的协作。

Wheatear 的重点不是实现一个“大而全”的通用引擎，而是围绕视觉小说、横版战斗、弹幕玩法、回合制战斗和战棋等类型化玩法，建立一套结构清晰、可调试、可扩展的内容生产流程。

## 演示

**编辑器演示**

https://github.com/user-attachments/assets/2d08f3e8-6c46-4bae-82af-4a9c0884c037

**Sandbox 演示**

https://github.com/user-attachments/assets/594114a5-ba8b-4847-9f34-6be1f15181f7

有声版演示： https://www.bilibili.com/video/BV1tGue6vEAt/?share_source=copy_web&vd_source=da98153b23f4943d9fa70056f1fb1ae8

（以下图片为 Wheatear 旧版界面，新版展示图片待上传。）

<p align="center">
  <img src="docs/readme-media/editor_overview.png" width="49%" alt="Wheatear 编辑器" />
  <img src="docs/readme-media/game_start.png" width="49%" alt="游戏开始界面" />
</p>

<p align="center">
  <img src="docs/readme-media/visual_novel.png" width="49%" alt="视觉小说演示" />
  <img src="docs/readme-media/story_branching.png" width="49%" alt="剧情分支演示" />
</p>

<p align="center">
  <img src="docs/readme-media/side_combat_1.png" width="49%" alt="横版战斗演示" />
  <img src="docs/readme-media/side_combat_2.png" width="49%" alt="横版战斗演示" />
</p>

### 编辑器界面

新版编辑器使用统一的视觉系统（仍基于 Dear ImGui，不引入 Qt/CEF 等重型 UI 框架）：

- **Inter + 微软雅黑** 合并字体：拉丁字符使用 Inter，中文使用系统雅黑，16px 正文 / 13px 辅助 / 22px 标题；面板边距、控件高度和 Tab 同步收紧到现代商业引擎密度。
- **Midnight + Wheat** 主题：深色中性面板、麦穗金强调色，统一圆角、边框、间距、Tab、Dock 分隔条和 hover 状态。
- 顶部品牌区与透明麦穗 Logo（无深色底、细金环）；窗口图标、`.ico` 应用图标同步更新。
- 主操作工具栏固定在菜单栏下方（不再悬浮遮挡场景/编辑器 Tab），包含新建/打开/保存、撤销/重做、播放/停止/单步、聚焦、UI/事件工具、健康检查、打包、重置布局与资源抽屉；右侧可一键收起，Window → Action Toolbar 可恢复。
- 编辑器启动即最大化，避免高 DPI 下右侧 Inspector / 底部 Dock 落到屏幕外；标题栏、工具栏、Dock 与状态栏按同一套几何约束布局；无边框窗口支持鼠标拖动边缘缩放，最小尺寸 900×560。
- 界面缩放：`Ctrl + 鼠标滚轮` 在面板上缩放字体与控件间距，`Settings → UI Scale` 提供 85%–200% 预设；100% 使用更大的文字基准（约原先 143% 的文字），图标保持独立尺寸，Content Browser 缩略图仍用自身 Ctrl+滚轮缩放。
- 启动器按 1920×1080 设计空间整体缩放：字体、图标、卡片、间距随窗口尺寸同步变化，内容卡片铺满窗口；自绘标题栏 + 渐变/网格/麦穗光晕背景避免大片纯黑边。
- 新增底部状态栏：项目、工作区、Edit/Play、场景与脏标记、当前选中实体、FPS、帧耗时、2D/3D draw call 数。
- 新增稳定 Dock 体系：窗口标题不再使用会随语言变化的本地化字符串；Dock 版本号变化时自动清理旧 `imgui.ini` 并重建，修复打开场景后窗口散落成独立浮动窗的问题；工具窗口默认停靠到中央文档区。
- Viewport 内嵌工具条：Select/Move/Rotate/Scale、Frame、编辑/播放状态，不再使用悬浮按钮覆盖场景。
- 图标统一为 128px 源图、2.35px 圆头线宽；资源类型按语义着色，播放/暂停/停止/单步使用实心圆角图形；所有 ImGui 图标采样统一修正垂直翻转，避免上下颠倒。
- `PanelHeader` 使用标题字体 + 强调色竖条；`StatusBadge` 改为深色胶囊标签；`IconButton` 等编辑器控件统一到新主题。
- 启动器改为 **Workspace + Template + Project** 三段式：2D/3D 工作区卡片、2D/3D 模板卡片、项目列表，新建项目自动写入模板场景、示例流程脚本、`TEMPLATE.md` 与 `Workspace` 配置；启动器同样最大化并垂直居中内容。
- 2D/3D 编辑器顶部显示工作区徽章（2D Studio 蓝色 / 3D Studio 绿色），保留共享底层但明确制作边界。
- 编辑器使用无边框窗口 + 自绘标题栏：品牌、工作区徽章、场景名、最小化/最大化/关闭、拖动和边缘缩放。
- 项目列表支持搜索过滤、最近项目、收藏星标、拖拽排序与持久化顺序；保存场景时自动把编辑器最后一帧写成项目缩略图（`Saved/Thumbnails/project_thumbnail.png`）。
- 工作区功能隔离：2D 工作区只显示 Sprite/Circle/2D Physics/精灵动画等工具与组件；3D 工作区只显示 Mesh/Terrain/Nav/Light/Animator 3D；Dock 布局、Object 创建菜单、Add Component、Window/Toolbar 菜单同步过滤。
- 主题支持 **Midnight / Paper** 切换，并持久化到编辑器设置目录；图编辑画布、控制台日志颜色、WAO 时间条、动画时间轴与资源浏览器自定义背景均适配浅色主题。
- 模板场景开始内容化：UI 模板带标题文本、RTS 模板带地形/基地占位、2D 模板带相机与角色占位；模板自动附带可运行的 `assets/events/start_flow.wts` 流程控制器。
- 视口缩放改为在渲染前统一重建 Framebuffer，避免拖动分隔条时出现“天空 → 黑屏 → 天空”的闪烁。

## 项目定位

Wheatear 由三个主要部分组成：

- `Wheatear`：引擎运行时静态库。
- `WheatearEditor`：场景编辑器与内容生产工具。
- `WheatearSandbox`：独立运行器，用于运行编辑器制作的项目和打包后的玩家内容。

当前 Demo 的主要流程为：

```text
主菜单
  -> 视觉小说剧情
  -> 弹幕式假玩法
  -> 回合制战斗过渡
  -> 横版战斗
  -> Boss 战
  -> 战斗结算
  -> 据点
  -> 技能树、装备、关系与存档
  -> 后续剧情
```

回合制、战棋和弹幕玩法目前既作为独立演示场景存在，也用于验证多玩法共享引擎基础设施的可行性。

## 架构概览

```text
Application
├── Wheatear Runtime
│   ├── Core / Platform
│   ├── Renderer
│   ├── Scene + ECS
│   ├── Animation / Physics / Audio / UI
│   ├── Asset / Serialization
│   ├── CommandBus / EventScript
│   └── Gameplay Modules
│       ├── VisualNovel
│       ├── ArcadeCombat
│       ├── SideCombat
│       ├── TurnCombat
│       ├── TacticalCombat
│       └── Progression
│
├── WheatearEditor
│   ├── Scene Hierarchy
│   ├── Inspector
│   ├── Content Browser
│   ├── Animation Editor
│   ├── UI Canvas Editor
│   ├── VN Script Editor
│   ├── Event Script Editor
│   ├── Combat Tuning Editors
│   ├── WAO Action Editor
│   ├── Project Health
│   └── Player Packager
│
└── WheatearSandbox
    └── Loose Assets / content.wtpack
```

运行时使用 `Scene` 管理实体、组件和系统。系统通过 `ISystem` 统一生命周期，编辑器模式和运行模式分别注册不同的系统集合。

玩法模块通过 `SceneSystemRegistry` 注册，不直接写入场景核心流程。这样编辑器、Sandbox 和未来的其他运行时宿主可以复用同一套引擎模块。

## 核心系统

### Scene 与 ECS

场景系统基于 `entt` 实现实体组件模型：

- 实体由 UUID 标识。
- 组件按功能拆分为 Core、Rendering、Physics、Animation、UI 和 Gameplay 等类别。
- 场景文件使用 `.wt` 格式保存，并通过 YAML 序列化。
- Prefab 使用 `.wtprefab` 格式，支持多实体层级和实例化时的 UUID 重映射。
- 编辑器进入 Play 模式时使用运行时场景副本，运行时修改不会直接污染编辑场景。
- 实体引用优先使用 `@UUID`，实体重命名后不会破坏场景绑定。

与 Unity 的 `GameObject + MonoBehaviour` 或 UE 的 `Actor + Component` 相比，Wheatear 更强调组件数据与系统逻辑的分离。

这种设计有利于：

- 降低场景序列化复杂度。
- 让不同玩法共享同一套实体和组件基础设施。
- 方便编辑器统一绘制 Inspector。
- 控制运行时系统与编辑器代码之间的依赖。

代价是它没有成熟商业引擎中完善的反射、蓝图和行为组件生态，复杂行为仍然需要通过 C++ 系统或服务实现。

### 2D 渲染

2D 渲染使用 OpenGL 后端，并通过批处理降低绘制开销。

当前支持：

- Quad、Circle、Line 和 Polyline 绘制。
- Sprite、Sprite Sheet 和 Atlas 子图。
- SDF 文本和描边文本。
- UI Canvas、进度条、路径和战斗 HUD。
- 编辑器实体拾取使用 `EntityID` 写入渲染顶点。
- 纹理槽管理和批次 Flush。
- 纹理缓存与重复资源复用。

`Renderer2D` 将不同绘制对象组织到 Quad、Text、Circle 和 Line 等批次中，在纹理槽或顶点缓冲区达到上限时提交当前批次。

### 3D 渲染

3D 渲染主要用于基础能力验证、编辑器预览和技术测试，目前不是 Demo 的主要方向。

已实现内容包括：

- Mesh 与 Material。
- 静态 FBX / glTF / GLB / OBJ 模型导入（ufbx + cgltf），支持材质拆分子网格。
- 模型 cook 为引擎原生 `.wtmodel` 二进制（`--cook-model` 或编辑器 Re-Cook），运行时优先加载 cooked 文件。
- 导入设置：缩放、Y-up 转换、法线绿通道翻转、Separate / ORM / Metallic-Smoothness 通道模式、纹理搜索路径。
- glTF / FBX 内嵌纹理提取，法线/ORM 贴图派生写出。
- 场景引用使用资源 UUID（模型、材质、Sprite/UI 纹理），注册表更新路径后引用仍指向同一 ID；跨路径移动资产时的自动 ID 迁移（`.meta`）尚未接入，UUID 缺失时回退到旧路径字段。
- 骨骼动画与 GPU 蒙皮：glTF skin/joint/weight、FBX cluster 权重、最多 4 骨骼影响/顶点、`Animator3DComponent` 播放/循环/交叉淡化；FBX 动画在导入时烘焙为逐帧蒙皮矩阵，避免 FBX 继承/单位规则导致姿态失真。
- 网格 AABB + 相机/阴影视锥剔除。
- 相同 Mesh + 相同材质状态的静态网格自动合批为实例化绘制（运行时；编辑器保持逐对象绘制以支持 picking）。
- Heightfield `TerrainComponent`（程序化高度图、编辑器重新生成/重建网格）。
- 网格导航：`NavObstacleComponent` 阻挡 + A* 8 邻域寻路 + 视线字符串拉直 + 地形高度采样；`NavAgentComponent` 路径跟随。
- 单位移动：空间哈希局部避让、圆 vs 阻挡格碰撞修正（不会穿过岩石/建筑）、按移动方向旋转的编队落点、追击目标定时重算路径。
- 单位状态机：Idle / Move / Attack / Dying，死亡单位保留 0.8 秒做倒地表现后再销毁；有 `Animator3DComponent` 的骨骼单位按状态交叉淡化 Idle/Walk/Attack/Death 片段，纯网格单位使用灰盒缩放动画反馈。
- RTS 经济：金币资源点、Harvester 自动采集、人口/补给上限、基地与兵营血量、建造中/已完工状态。
- 生产与 AI：兵营队列（Soldier 50 / Harvester 75）、按 `1`/`2` 快捷训练、`B` 进入兵营放置模式；AI 拥有被动收入、自动补兵和周期进攻波次，摧毁敌方基地获胜。
- 导航调试：F3 显示网格、阻挡格、路径与 waypoint；F4 选中全部玩家单位并寻路到地图另一侧，用于快速验证绕障。
- 天空盒。
- 方向光和点光源。
- 阴影。
- IBL。
- SSAO。
- 3D 编辑器相机和场景预览。

当前动画随 `.wtmodel` 一起 cook，独立可复用的 `.wtanim3d` 片段、动画状态机资产和 root motion 尚未接入。FBX 会按 Y-up / 目标单位转换，glTF 使用规范单位；材质支持 base color、metallic、roughness 与法线贴图。DDS/KTX 与 BC 纹理压缩仍在后续阶段。

当前渲染后端主要为 OpenGL。构建工程时仍需要 Vulkan SDK，主要用于 ShaderC 和 SPIR-V Cross 等 shader 工具链依赖，而不是表示当前已经实现 Vulkan 渲染后端。

<p align="center">
  <img src="docs/readme-media/render_3d_skybox.png" width="49%" alt="3D 天空盒渲染" />
  <img src="docs/readme-media/render_3d_shadow.png" width="49%" alt="3D 阴影渲染" />
</p>

### 物理、输入与音频

- 2D 物理使用 Box2D。
- 物理体和 Fixture 由 ECS 组件描述，运行时创建对应的 Box2D 对象。
- 支持 Sprite Sheet 每帧碰撞框驱动。
- 输入系统分为底层按键事件、输入状态查询和 Action Binding 三层。
- `InputBindingService` 允许玩法使用 `side.jump`、`arcade.attack`、`vn.advance` 等动作名，而不是直接依赖具体键盘码。
- 用户设置与游戏进度分离，音量、全屏、文字速度和按键绑定保存到 `UserSettings`。
- 音频系统基于 miniaudio，支持 BGM、SFX 和运行时音量控制。

## Sprite Sheet 与动画管线

Sprite Sheet 是 Wheatear 目前较完整的一条资产工作流。

`.wtsheet` 文件保存：

- 纹理路径。
- 行列划分。
- 每格内容裁切区域。
- 每格碰撞框。
- 不规则命名矩形。

实体和动画帧保存图集引用与格子编号，运行时统一解析为纹理和 UV。

因此，修改 `.wtsheet` 的网格、裁切或碰撞框后，所有引用该图集的实体和动画都可以自动获得更新。

动画系统基于 `AnimationClip`，支持：

- 序列帧。
- 循环播放。
- Sprite Sheet 引用。
- 属性轨道。
- 颜色、透明度、位置和缩放动画。
- 动画事件。
- 动画事件触发 `CommandBus` 命令。
- `.wtanim` 动画资产复用。
- PPU 真实尺寸计算。
- 动画帧驱动碰撞盒。

动画系统只负责时间采样和事件派发，不直接实现战斗规则。攻击命中、音效、特效或场景跳转由命令系统和对应玩法模块处理。

## 内容驱动管线

Wheatear 将不同类型的数据分别交给不同格式管理：

| 文件格式 | 用途 |
| --- | --- |
| `.wt` | 场景与实体组件 |
| `.wtprefab` | 多实体 Prefab |
| `.wtmaterial` | 材质数据 |
| `.wtsheet` | Sprite Sheet 与 Atlas 定义 |
| `.wtanim` | 可复用动画 Clip |
| `.vn` | 视觉小说剧本、角色、台词、选项与跳转 |
| `.wts` | 低频流程编排 |
| `.yaml / .json` | 战斗调参、动作配方、成长内容和配置数据 |
| `.wtuit` | 编辑器 UI 模板描述 |
| `.wtpack` | 玩家运行时资源包 |

内容生产的基本分工是：

```text
数据表负责“是什么”
CommandBus 负责“执行什么”
.wts 负责“什么时候执行”
C++ Gameplay Module 负责“如何运行”
```

### 事件脚本与 CommandBus

`.wts` 是 Wheatear 自定义的轻量事件脚本格式，不是通用编程语言。

它主要用于：

- 场景切换。
- 新游戏和读档。
- 剧情旗标。
- UI 分页。
- 战斗结算。
- 低频流程等待。
- 简单条件分支。

示例：

```text
event on_start:
    wait 0.15
    progression:set_flag:FLAG_DEMO_STARTED
end

event open_skill_tree:
    scene:assets/scenes/SkillTree.wt
end
```

脚本解析器支持 `event`、`wait`、`if`、`endif` 和 CommandBus 命令。未知行会保留，编辑器在 Graph 与 Raw 两种模式之间切换时不会静默丢失原始内容。

这种方式参考了传统关卡工具和事件表的流程组织方式，但没有引入 C# 或 Lua 运行时。

与 Unity 中使用 C#、UE 中使用 Blueprint 或 Gameplay Task 相比，`.wts` 的优点是：

- 没有托管运行时和脚本绑定层。
- 打包结构简单。
- 流程资源可直接被编辑器和版本控制系统检查。
- 场景切换和存档流程更加集中。

限制是它不适合每帧逻辑、AI、复杂数值公式和战斗状态机。此类逻辑仍然由 C++ 服务和玩法系统实现。

## WAO 动作系统

WAO 是 `Wheatear Action Orchestration` 的缩写，用于统一动作语义、效果处理和调试入口。

当前核心流程为：

```text
ActionIntent
    -> ActionRecipe
    -> ActionResolver
    -> EffectBundle
    -> EffectLedger
    -> ActionSignalRouter
```

各部分职责：

- `ActionIntent`：描述输入、AI、脚本或编辑器调试产生的动作意图。
- `ActionRecipe`：保存动作名称、资源、冷却、时序、标签、效果和信号。
- `ActionResolver`：将动作配方交给具体玩法解析。
- `EffectBundle`：描述一次动作产生的效果集合。
- `EffectLedger`：记录效果是否应用、作用对象和结果。
- `ActionSignalRouter`：向音效、特效、UI 或玩法模块发送结构化表现信号。

WAO 的设计受到 UE GAS 的能力激活、属性、Gameplay Effect 和表现 Cue 等思想影响，但没有直接复制 `AbilitySystemComponent` 的整体结构。
可以把它理解成：**一个更适合单机 2D、多玩法项目的轻量技能 / 动作组织层**。

Wheatear 当前是单机、2D、多玩法项目，因此 WAO 没有实现：

- 网络复制。
- 网络预测。
- 服务器权威同步。
- UObject 反射对象树。
- 统一覆盖所有玩法规则的巨型能力组件。

横板战斗的动作帧、命中框、浮空和断限，弹幕玩法的投射物和遮挡，回合制的行动队列，以及战棋的格子规则，仍然保留在各自的玩法服务中。

WAO 负责公共语义和调试，玩法模块负责具体规则。这种取舍避免了为了抽象而强行统一完全不同的战斗结构。

当前 WAO 已用于：

- 弹幕武器和 Boss 投射物。
- 横板战斗玩家与敌人动作。
- 回合制与战棋动作配方。
- 公共状态效果。
- 动作资源加载与热重载。
- 动作账本调试。
- Action Editor 沙盒执行。

<p align="center">
  <img src="docs/readme-media/wao_action_debugger.png" width="49%" alt="WAO 动作调试器" />
  <img src="docs/readme-media/side_combat_tuning_editor.png" width="49%" alt="横版战斗调参编辑器" />
</p>

## Gameplay 模块

### Visual Novel

视觉小说模块支持：

- 角色与立绘。
- 台词和打字效果。
- 背景切换。
- BGM。
- 表情差分。
- 选项与标签跳转。
- 条件选项。
- 历史记录。
- 自动播放和跳过。
- 存档与读档。

<p align="center">
  <img src="docs/readme-media/vn_script_editor.png" width="49%" alt="VN 剧本编辑器" />
  <img src="docs/readme-media/history.png" width="49%" alt="剧情历史记录" />
</p>

### SideCombat

横版战斗是当前 Demo 的主要玩法，包含：

- 横向移动和纵深移动。
- 跳跃、冲刺和空中动作。
- 普通攻击和连段。
- 上挑、空中追击和断限追击。
- Hitbox 与受击判定。
- Boss 保护条。
- Hit Pause、屏幕震动和音效反馈。
- 敌人 AI。
- 波次生成。
- 掉落和战斗结算。
- 技能槽、道具槽和状态图标。
- 键盘与鼠标摇杆控制。

### ArcadeCombat

弹幕式假玩法包含：

- 多种武器。
- 投射物生成与生命周期。
- Boss 发弹。
- 掩体碰撞。
- 武器切换。
- 触摸式摇杆与攻击按钮。
- 独立的弹幕调参面板。

<p align="center">
  <img src="docs/readme-media/arcade_combat.png" width="49%" alt="弹幕式假玩法" />
  <img src="docs/readme-media/turn_combat.png" width="49%" alt="回合制战斗" />
</p>

### TurnCombat

回合制模块包含：

- 回合顺序。
- 目标选择。
- 技能与资源消耗。
- 伤害、治疗和状态效果。
- AI 行动。
- 技能演出和战斗 UI。

### TacticalCombat

战棋模块包含：

- 格子坐标。
- 单位占位。
- 移动范围。
- 目标选择。
- 技能范围。
- 敌方 AI。
- 回合行动与战斗结算。

四种玩法共用引擎的场景、UI、输入、音频、动画、资源和部分 WAO 能力，但不强行共用玩法规则。

### Progression 与 UI

竖切 Demo 还包含据点、技能树、装备、关系、存档和战斗结算等页面，用于验证玩法模块与通用 UI、存档和成长数据之间的衔接。

<p align="center">
  <img src="docs/readme-media/hub.png" width="49%" alt="据点界面" />
  <img src="docs/readme-media/skill_tree.png" width="49%" alt="技能树界面" />
</p>

<p align="center">
  <img src="docs/readme-media/equipment.png" width="49%" alt="装备界面" />
  <img src="docs/readme-media/result.png" width="49%" alt="战斗结算界面" />
</p>

## 编辑器

WheatearEditor 使用 ImGui 构建，采用面板式工具结构，并支持 Docking。

当前主要工具包括：

- Scene Hierarchy：场景实体层级与筛选。
- Inspector：组件属性编辑。
- Content Browser：资源浏览、筛选、拖拽和定位。
- UI Canvas Editor：UI 控件拖拽、缩放、吸附、对齐与分布。
- Animation Editor：动画时间线、属性轨道与事件。
- Sprite Sheet Picker：图集切分与连续帧生成。
- VN Script Editor：视觉小说时间线编辑。
- Event Script Editor：`.wts` 事件图与 Raw 编辑。
- Side Combat Tuning Editor：横版战斗调参。
- Turn Combat Tuning Editor：回合制调参。
- Arcade Combat Tuning Editor：弹幕玩法调参。
- Tactical Combat Tuning Editor：战棋调参。
- Progression Content Editor：技能树、装备、材料、副本与关系数据。
- WAO Action Editor：动作配方编辑、验证和沙盒测试。
- Data File Editor：YAML、JSON、`.wtsettings` 和 `.wtanim` 等数据文件编辑。
- Input Bindings：输入动作和按键绑定。
- Project Health：缺失引用、场景跳转、资源注册、打包依赖和源码同步检查。
- Player Packager：构建运行版并生成资源包。

<p align="center">
  <img src="docs/readme-media/editor_overview.png" width="49%" alt="编辑器总览" />
  <img src="docs/readme-media/vn_script_editor.png" width="49%" alt="VN Script Editor" />
</p>

Content Browser 的设计参考了 UE Content Browser 的资源组织方式，但实现范围更集中于 Wheatear 的文件格式和工作流。

与直接使用商业引擎相比，自研编辑器的优势是：

- 编辑器行为与运行时数据结构完全一致。
- 可以针对项目中的 VN、战斗和成长系统设计专用面板。
- 不需要依赖通用引擎中大量与项目无关的功能。
- 便于观察和调试底层数据。

相应的限制是编辑器生态、插件系统和通用工具数量仍然远少于 UE、Unity 等成熟引擎。

## 资源管理与打包

编辑器使用中央资源注册表：

```text
Intermediate/AssetRegistry.yaml
```

注册表记录：

- 资源 UUID。
- 项目相对路径。
- 资源类型。
- 导入设置。
- 资源引用关系。
- 反向引用关系。

当前不再将每个资源的 `.wtmeta` 文件作为正式工作流。

打包时，`AssetDependencyScanner` 从启动场景出发扫描：

- 场景引用。
- Prefab 引用。
- VN 和 WTS 脚本引用。
- YAML 和 JSON 数据引用。
- 动画与图集引用。
- 场景跳转引用。
- 内置 gameplay 资源。

最终生成 `content.wtpack`。Sandbox 支持两种运行方式：

```text
Loose Assets
    直接读取项目 assets 目录

Packed Assets
    读取 content.wtpack
    启动时解包到运行时缓存
```

编辑器、开发模式 Sandbox 和打包后的玩家版本共用 `AssetPath` 资源解析逻辑。

## 构建

### 环境要求

当前主要验证环境为：

- Windows
- Visual Studio 2022
- MSBuild
- Vulkan SDK
- Git LFS（用于仓库中的图片、视频等二进制资源）

项目使用 C++20。

### 生成 Visual Studio 工程

```powershell
vendor\bin\premake\premake5.exe vs2022
```

生成前需要设置 `VULKAN_SDK` 环境变量。

### 构建引擎与编辑器

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
    -File scripts\Build-Windows.ps1 `
    -ProjectPath Wheatear.sln `
    -Configuration Debug `
    -Platform x64
```

也可以使用编辑器专用脚本：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
    -File scripts\Build-WheatearEditor.ps1
```

### 启动 Demo

```powershell
bin\Release-windows-x86_64\WheatearEditor\WheatearEditor.exe `
    --project Projects\WheatearDemo
```

```powershell
bin\Release-windows-x86_64\WheatearSandbox\WheatearSandbox.exe `
    --project Projects\WheatearDemo
```

编辑器启动器中的 **Open Project Folder...** 可以直接打开任意位置的
`WheatearProject.wtproject` 项目，并记住到已导入项目列表。命令行等价方式：

```powershell
WheatearEditor.exe --project D:\Games\MyProject
```

### 打包玩家版本

编辑器界面支持选择启动场景并打包。

也可以使用命令行：

```powershell
Builds\Win64\Editor\WheatearEditor.exe `
    --package `
    --project Projects\WheatearDemo
```

默认游戏包输出到：

```text
Builds/Win64/<GameName>/
```

GUI 打包默认使用 Release 配置。命令行可以通过 `--configuration Debug|Release` 指定配置。

## 项目结构

Wheatear 采用轻量但规范的 Project / Build 结构，不需要安装器：

```text
Projects/<ProjectName>/
    WheatearProject.wtproject   项目描述（名称、版本、工作区、启动场景）
    Config/
        DefaultGame.ini         运行/打包配置
    assets/                     游戏内容（Unity 风格的 Assets）
        scenes/
        gameplay/
        models/
        materials/
        ui_templates/
        ...
    Saved/
        Logs/
        Config/
        Thumbnails/             项目缩略图
    Intermediate/
        AssetRegistry.yaml      UUID / 路径注册表
        Cache/
        Cooked/
    Binaries/
        Win64/                  项目构建产物占位
```

编辑器本身是可移植目录：

```text
Builds/Win64/Editor/
    WheatearEditor.exe
    assets/shaders/
    Resources/
    ContentTemplates/
```

把整个 `Editor` 目录复制到任意位置即可运行，不需要安装器；启动后可直接打开/导入项目。

游戏包：

```text
Builds/Win64/<GameName>/
    <GameName>.exe
    Content/
        Paks/content.wtpack
    Config/
        DefaultGame.ini
    WheatearProject.wtproject
    Saved/
    README.txt
```

编辑器通过 `WheatearProject.wtproject` 识别项目；`assets/` 是引擎的内容根，
`Intermediate/`、`Saved/`、`Binaries/` 不会被当作资源扫描。

## 技术栈

- C++20
- Premake
- OpenGL
- GLFW
- GLAD
- ImGui
- ImGuizmo
- entt
- glm
- yaml-cpp
- Box2D
- spdlog
- miniaudio
- ShaderC
- SPIR-V Cross

## 目录结构

```text
Wheatear/
    引擎运行时静态库

WheatearEditor/
    编辑器、资源工具、玩法调参工具和打包器

WheatearSandbox/
    独立运行器

Projects/
    项目目录
    WheatearDemo/ 为当前 Demo

docs/
    架构说明、玩法设计、编辑器手册和开发记录

Builds/
    Win64/Editor/       便携编辑器（exe + Resources + assets/shaders + ContentTemplates）
    Win64/<GameName>/   独立游戏（exe + Content/Paks + Config + Saved）

scripts/
    构建、检查和工程辅助脚本

vendor/
    第三方依赖
```

## 当前边界

Wheatear 目前仍是面向学习、验证和作品展示的自研引擎项目，主要边界包括：

- 当前主要验证平台为 Windows。
- 当前主要渲染后端为 OpenGL。
- 3D 功能以基础管线和编辑器预览为主。
- `.wts` 适合流程编排，不是通用脚本语言。
- 新增复杂玩法行为仍可能需要注册 C++ 服务或处理器。
- 当前未实现完整的网络同步、多人联机和商业引擎级插件生态。
- 部分编辑器工具仍在持续完善，项目优先保证 2D Demo 的内容生产闭环。

## 文档

详细设计文档位于 [`docs/`](docs/)：

- [引擎架构说明](docs/03_引擎架构/引擎架构说明.md)
- [编辑器操作手册](docs/03_引擎架构/编辑器操作手册.md)
- [命令、数据表与事件脚本](docs/03_引擎架构/命令数据表与事件脚本.md)
- [WAO 动作编排与效果结算系统](docs/03_引擎架构/WAO动作编排与效果结算系统.md)
- [资源数据库与 UI 模板系统](docs/03_引擎架构/资源数据库与UI模板系统.md)
- [逻辑运行时与 DSL 统一](docs/03_引擎架构/逻辑运行时与DSL统一.md)
- [从地基到 Sandbox](docs/从地基到Sandbox/从地基到Sandbox.md)

## 素材说明

Demo 中部分立绘、背景、UI 和动作帧使用程序化生成或图像生成工具辅助制作，主要用于验证引擎功能、编辑器流程和竖切版本表现，不代表最终商业项目美术资产。
