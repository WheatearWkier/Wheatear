# Wheatear 引擎教材目录

> 目标：把 Wheatear 当作一套面向简单 2D 游戏的教学型引擎来讲解。
> 本教材按分层架构自底向上组织，正文统一采用“层级定位、问题背景、核心概念、源码入口、具体例子、引擎对照、面试表达、小结衔接”的写法。

## 总体分层

```text
应用层
Editor / Sandbox

玩法层
VN / SideCombat / TurnCombat / TacticalCombat / Arcade / Progression

游戏运行服务层
CommandBus / EventScript / ActionSystem / SceneTransition / GameProgress

场景 ECS 层
Scene / Entity / Components / Systems / SceneSerializer

引擎服务层
Renderer / Audio / Input / AssetPath / UI / Physics

核心平台层
Application / Window / EventBus / LayerStack / Log / Timestep

第三方与平台层
GLFW / OpenGL / ImGui / entt / Box2D / yaml-cpp / stb
```

## 推荐阅读顺序

如果你是第一次顺着读，建议按这个顺序走：

1. `00_序章`
2. `01_第三方与平台层`
3. `02_核心平台层`
4. `03_引擎服务层`
5. `04_Scene与ECS层`
6. `05_游戏运行服务层`
7. `06_玩法层`
8. `07_应用层`
9. `99_附录`

如果你是面试复习，优先看：

1. `05_游戏运行服务层`
2. `06_玩法层`
3. `99_附录/02_面试高频问题`
4. `99_附录/01_Unity_UE_Godot对照`

## 00_序章

- [01_分层架构与学习目标](00_序章/01_分层架构与学习目标.md)
- [02_阅读方法与术语](00_序章/02_阅读方法与术语.md)

## 01_第三方与平台层

- [01_第三方库与平台依赖](01_第三方与平台层/01_第三方库与平台依赖.md)
- [02_窗口与OpenGL上下文](01_第三方与平台层/02_窗口与OpenGL上下文.md)
- [03_平台句柄与SwapBuffer](01_第三方与平台层/03_平台句柄与SwapBuffer.md)

## 02_核心平台层

- [01_Core层与主循环](02_核心平台层/01_Core层与主循环.md)
- [02_事件与输入](02_核心平台层/02_事件与输入.md)
- [03_日志与时间步](02_核心平台层/03_日志与时间步.md)

## 03_引擎服务层

- [01_渲染地基](03_引擎服务层/01_渲染地基.md)
- [02_资产系统](03_引擎服务层/02_资产系统.md)
- [03_图集与SpriteSheet](03_引擎服务层/03_图集与SpriteSheet.md)
- [04_动画系统](03_引擎服务层/04_动画系统.md)
- [05_输入抽象](03_引擎服务层/05_输入抽象.md)
- [06_UI系统](03_引擎服务层/06_UI系统.md)
- [07_物理系统](03_引擎服务层/07_物理系统.md)
- [08_音频系统](03_引擎服务层/08_音频系统.md)
- [09_3D预览验证](03_引擎服务层/09_3D预览验证.md)

## 04_Scene与ECS层

- [01_Scene与Entity_ECS](04_Scene与ECS层/01_Scene与Entity_ECS.md)
- [02_场景生命周期与切换](04_Scene与ECS层/02_场景生命周期与切换.md)
- [03_序列化_Copy_Prefab_PlayMode](04_Scene与ECS层/03_序列化_Copy_Prefab_PlayMode.md)
- [04_系统接入渲染物理动画](04_Scene与ECS层/04_系统接入渲染物理动画.md)

## 05_游戏运行服务层

- [01_CommandBus](05_游戏运行服务层/01_CommandBus.md)
- [02_EventScript](05_游戏运行服务层/02_EventScript.md)
- [03_ActionSystem](05_游戏运行服务层/03_ActionSystem.md)
- [04_SceneTransition](05_游戏运行服务层/04_SceneTransition.md)
- [05_GameProgress](05_游戏运行服务层/05_GameProgress.md)

## 06_玩法层

- [01_VN模块](06_玩法层/01_VN模块.md)
- [02_SideCombat与WAO](06_玩法层/02_SideCombat与WAO.md)
- [03_TurnCombat](06_玩法层/03_TurnCombat.md)
- [04_TacticalCombat](06_玩法层/04_TacticalCombat.md)
- [05_Arcade](06_玩法层/05_Arcade.md)
- [06_Progression](06_玩法层/06_Progression.md)

## 07_应用层

- [01_Editor与Sandbox](07_应用层/01_Editor与Sandbox.md)
- [02_资源浏览器与Inspector](07_应用层/02_资源浏览器与Inspector.md)
- [03_场景预览与运行控制](07_应用层/03_场景预览与运行控制.md)
- [04_Build打包发布](07_应用层/04_Build打包发布.md)

## 99_附录

- [01_Unity_UE_Godot对照](99_附录/01_Unity_UE_Godot对照.md)
- [02_面试高频问题](99_附录/02_面试高频问题.md)
- [03_常见坑与重构路线](99_附录/03_常见坑与重构路线.md)
- [04_术语表](99_附录/04_术语表.md)
