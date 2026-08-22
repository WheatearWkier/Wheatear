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
- OBJ 模型加载。
- 天空盒。
- 方向光和点光源。
- 阴影。
- IBL。
- SSAO。
- 3D 编辑器相机和场景预览。

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
assets/.wheatear/asset_registry.yaml
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

### 打包玩家版本

编辑器界面支持选择启动场景并打包。

也可以使用命令行：

```powershell
Builds\Windows\Editor\WheatearEditor.exe `
    --package-player `
    --project Projects\WheatearDemo
```

默认玩家包输出到：

```text
Builds/Windows/Player/<PackageName>/
```

GUI 打包默认使用 Release 配置。命令行可以通过 `--configuration Debug|Release` 指定配置。

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
    编辑器与玩家打包产物

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
- [从地基到 Sandbox 系列文档](docs/从地基到Sandbox/00_总纲.md)

## 素材说明

Demo 中部分立绘、背景、UI 和动作帧使用程序化生成或图像生成工具辅助制作，主要用于验证引擎功能、编辑器流程和竖切版本表现，不代表最终商业项目美术资产。
