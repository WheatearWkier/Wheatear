# Wheatear

自研 C++ 游戏引擎 + 编辑器项目，当前重点是一个可玩的 2D 竖切演示。

Wheatear 不是单纯的引擎样例，而是把剧情、战斗、成长、UI、脚本、渲染和打包串成一条完整链路，方便展示与方便快速说明“做了什么、为什么这样做”。

**编辑器演示**

https://github.com/user-attachments/assets/2d08f3e8-6c46-4bae-82af-4a9c0884c037

**Sandbox 演示**

https://github.com/user-attachments/assets/594114a5-ba8b-4847-9f34-6be1f15181f7

## 一眼看完

- 主线是 2D：视觉小说、横板战斗、弹幕玩法、回合制 / 战棋试验、据点成长、结算和存读档。
- 当前竖切流程已经覆盖：主菜单 -> VN 剧情 -> 假玩法 -> 正式横板战斗 -> Boss -> 结算 -> 据点 -> 成长 -> 继续剧情。
- 重点能力包括 `Gameplay`、`.wts` 事件脚本、WAO 动作系统、2D 渲染和编辑器工作流。
- 3D 渲染保留了基础能力展示，但不是当前项目的主线。

## 玩法展示

### 剧情和分支

<p align="center">
  <a href="https://github.com/user-attachments/assets/8f44f7e8-6d2d-4ffd-b467-301e4dd0911c"><img src="https://github.com/user-attachments/assets/8f44f7e8-6d2d-4ffd-b467-301e4dd0911c" width="49%" alt="Wheatear 游戏开始界面" /></a>
  <a href="https://github.com/user-attachments/assets/6142e531-143e-4349-92ae-79fd42b39280"><img src="https://github.com/user-attachments/assets/6142e531-143e-4349-92ae-79fd42b39280" width="49%" alt="视觉小说界面" /></a>
</p>

<p align="center">
  <a href="https://github.com/user-attachments/assets/c594f9ee-1472-4b32-8694-d25082232801"><img src="https://github.com/user-attachments/assets/c594f9ee-1472-4b32-8694-d25082232801" width="49%" alt="剧情分支选项" /></a>
  <a href="https://github.com/user-attachments/assets/5eb35f07-469a-4cff-9517-4926327411a0"><img src="https://github.com/user-attachments/assets/5eb35f07-469a-4cff-9517-4926327411a0" width="49%" alt="剧情历史记录" /></a>
</p>

VN 负责序章、章节推进、角色对话和轻量分支。它的目标不是把剧情做得很重，而是把“剧情怎么接战斗、战斗怎么回到成长”这条链路跑顺。

### 战斗和成长

<p align="center">
  <a href="https://github.com/user-attachments/assets/e9bbcf58-05ef-413f-bc7a-f8772f93bdec"><img src="https://github.com/user-attachments/assets/e9bbcf58-05ef-413f-bc7a-f8772f93bdec" width="49%" alt="2D 横板战斗" /></a>
  <a href="https://github.com/user-attachments/assets/8c51dbee-8b5c-4618-9ed7-d10548ad8e10"><img src="https://github.com/user-attachments/assets/8c51dbee-8b5c-4618-9ed7-d10548ad8e10" width="49%" alt="2D 横板战斗技能与反馈" /></a>
</p>

<p align="center">
  <a href="https://github.com/user-attachments/assets/4318ce15-1140-44bf-a003-7f7afd2388b4"><img src="https://github.com/user-attachments/assets/4318ce15-1140-44bf-a003-7f7afd2388b4" width="32%" alt="弹幕玩法" /></a>
  <a href="https://github.com/user-attachments/assets/ef228f74-7bf0-4738-8e82-8f9417eca908"><img src="https://github.com/user-attachments/assets/ef228f74-7bf0-4738-8e82-8f9417eca908" width="32%" alt="回合制玩法" /></a>
  <a href="https://github.com/user-attachments/assets/bcb6f328-18f2-465f-9f8e-163160b775d0"><img src="https://github.com/user-attachments/assets/bcb6f328-18f2-465f-9f8e-163160b775d0" width="32%" alt="战斗结算" /></a>
</p>

<p align="center">
  <a href="https://github.com/user-attachments/assets/615b1f66-ed21-4310-b2ac-3c4f26f36071"><img src="https://github.com/user-attachments/assets/615b1f66-ed21-4310-b2ac-3c4f26f36071" width="32%" alt="据点页面" /></a>
  <a href="https://github.com/user-attachments/assets/4a9c4fbc-12a0-4064-b91c-44f7a86ab796"><img src="https://github.com/user-attachments/assets/4a9c4fbc-12a0-4064-b91c-44f7a86ab796" width="32%" alt="技能树界面" /></a>
  <a href="https://github.com/user-attachments/assets/0f8d6367-3582-455e-92be-b64c5c39b05b"><img src="https://github.com/user-attachments/assets/0f8d6367-3582-455e-92be-b64c5c39b05b" width="32%" alt="装备界面" /></a>
</p>

横板战斗是当前最核心的展示点；弹幕、回合制和战棋则用来验证多玩法架构。战后会回到据点，在结算、技能树、装备、关系和存档之间形成可玩的循环。

## 脚本系统

项目里很多流程不是硬写死在 C++ 里，而是走 `CommandBus + 数据表 + .wts`。

`.wts` 是一套轻量事件脚本，用来串联：

- 场景切换
- 剧情节点和剧情标记
- UI 按钮和分页
- 保存读取
- 战斗胜负后的流程

示例：

```text
event open_skill_tree:
    scene:assets/scenes/VerticalSliceSkillTree.wt
end
```

VN 使用数据化脚本记录台词、角色、表情、背景、音乐、选项和跳转；`.wts` 更适合安排“什么时候做什么”。这样剧情、UI 和玩法流程都能在编辑器和文本资源里直接调整。

## WAO 动作系统

WAO 是 **Wheatear Action Orchestration**，定位是统一动作语义层，思路上参考了 GAS，但没有照搬 GAS 的整体结构。

```text
ActionIntent -> ActionRecipe -> RuleResolver -> EffectBundle -> EffectLedger -> SignalRouter
```

它主要解决：

- 输入、AI、脚本都能进入同一套动作入口
- 技能、冷却、资源、效果和表现分开
- 横板、回合制、战棋、弹幕可以复用一部分动作数据
- 编辑器里可以查看、修改和追踪动作结果

面试里可以把它理解成：**一个更适合单机 2D、多玩法项目的轻量技能 / 动作组织层**。

## 渲染

### 2D 渲染

2D 是项目的主力。当前渲染主要围绕这些内容展开：

- 批量绘制 Quad、Circle、Line 和 Text
- Sprite、spritesheet、atlas 和 UV 子图
- UI Canvas、按钮、分页、进度条和路径绘制
- SDF 文本、技能图标和战斗 HUD
- 序列帧动画和动画事件

Sprite、UI 和特效共用同一套图集与动画工作流，角色动作、技能特效、VN 表情和界面动画都可以使用 `AnimationClip` 与动画事件。

### 3D 渲染

3D 主要用于基础能力验证和编辑器预览，包含：

- Mesh / Material
- 天空盒、方向光、点光源
- 阴影
- IBL / SSAO

<p align="center">
  <a href="https://github.com/user-attachments/assets/5ee8ff00-50bd-4f69-87a2-b80730bc6c85"><img src="https://github.com/user-attachments/assets/5ee8ff00-50bd-4f69-87a2-b80730bc6c85" width="49%" alt="3D 天空盒渲染" /></a>
  <a href="https://github.com/user-attachments/assets/1611b0b9-04c0-49aa-9939-332014c540ea"><img src="https://github.com/user-attachments/assets/1611b0b9-04c0-49aa-9939-332014c540ea" width="49%" alt="3D 阴影渲染" /></a>
</p>

## 编辑器

<p align="center">
  <a href="https://github.com/user-attachments/assets/d4d3ce10-222f-4d1e-9c0d-584c67f6003d"><img src="https://github.com/user-attachments/assets/d4d3ce10-222f-4d1e-9c0d-584c67f6003d" width="49%" alt="Wheatear 编辑器总览" /></a>
  <a href="https://github.com/user-attachments/assets/e10d6915-ef1c-4401-9717-33e0ecfbb5dd"><img src="https://github.com/user-attachments/assets/e10d6915-ef1c-4401-9717-33e0ecfbb5dd" width="49%" alt="视觉小说脚本编辑器" /></a>
</p>

<p align="center">
  <a href="https://github.com/user-attachments/assets/8322c832-2b47-400c-a794-8b9389ebc47d"><img src="https://github.com/user-attachments/assets/8322c832-2b47-400c-a794-8b9389ebc47d" width="49%" alt="横板战斗参数编辑器" /></a>
  <a href="https://github.com/user-attachments/assets/244b9731-6880-49f3-8ef0-dd3549f387ab"><img src="https://github.com/user-attachments/assets/244b9731-6880-49f3-8ef0-dd3549f387ab" width="49%" alt="WAO Action Debugger" /></a>
</p>

编辑器围绕内容生产效率设计，当前重点工具包括：

- VN Script Editor：编辑剧情、选项、角色、背景和音乐
- Event Script：查看和组织 `.wts` 流程
- Side Combat Tuning：编辑横板战斗参数
- WAO Action Debugger：查看动作配方、效果和运行记录
- Animation Editor / Sprite Sheet Picker：制作序列帧、图集和 UI 图标
- Content Browser：浏览资源、场景、Prefab 和 UI 模板

## 素材说明

- 部分图标、动作帧序列和 VFX 使用程序化生成
- 立绘、背景和部分 UI sheet 使用 GPT-Image-2 辅助生成
- 部分动作帧序列使用豆包生成后进行抠图和整理
- 这些素材主要用于个人项目展示和竖切验证

## 技术栈

- C++17
- OpenGL
- GLFW / ImGui / ImGuizmo
- entt / glm / yaml-cpp / Box2D / spdlog
- Mono 为可选脚本能力，默认构建不强依赖
- Git LFS 管理图片、视频和其他二进制资源

## 构建与运行

```powershell
vendor\bin\premake\premake5.exe vs2022
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\Build-WheatearEditor.ps1
```

常用入口：

- `WheatearEditor.exe`
- `WheatearSandbox.exe`
- `WheatearSandbox.exe --scripts`：启用可选脚本运行时

## 代码结构

- `Wheatear/`：运行时引擎
- `WheatearEditor/`：编辑器和内容生产工具
- `WheatearSandbox/`：独立运行器
- `docs/`：设计文档、系统说明和竖切记录
