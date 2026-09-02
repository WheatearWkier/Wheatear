# 第 7 编 01：Editor 与 Sandbox

> 本章所属层级：**应用层**
>
> 本章目标：理解 Wheatear 是怎么把同一套引擎核心包装成两个不同宿主的：
> - `WheatearEditor`：内容制作、项目选择、场景编辑、调试与打包
> - `WheatearSandbox`：独立运行、加载内容、执行剧情与玩法、验证最终玩家流程
>
> 这一章不讲具体的场景、渲染或玩法细节。  
> 它讲的是：**引擎外面那层壳，为什么要分成 Editor 和 Sandbox，以及它们怎么共享同一套核心。**

如果说前面的章节回答的是：

```text
Application / Core / Renderer / Scene / Systems
```

那这一章回答的就是：

```text
这些核心能力，最终是被谁“装起来”运行的？
编辑器为什么能改内容？
沙盒为什么能独立跑游戏？
为什么这两个宿主看起来不一样，却又共享很多东西？
```

Wheatear 的应用层不是一个“随便放界面代码”的地方。  
它是整个项目的**宿主层**：

```text
main()
  -> CreateApplication()
  -> Application
  -> Editor / Sandbox
  -> EditorLayer / RuntimeSceneLayer
  -> Scene / Systems / Renderer / Runtime Services
```

这层写得好不好，直接决定你的引擎是“一个能跑的程序”，还是“一个能做内容生产工具的程序”。

---

## 7.1 本章要读的源码

```text
Wheatear/src/Wheatear/Core/EntryPoint.h
Wheatear/src/Wheatear/Core/Application.h
Wheatear/src/Wheatear/Core/Application.cpp

WheatearEditor/src/WheatearEditorApp.cpp
WheatearEditor/src/Editor/ModeSelectLayer.h
WheatearEditor/src/Editor/ModeSelectLayer.cpp
WheatearEditor/src/Editor/EditorLayerBase.h
WheatearEditor/src/Editor/EditorLayerBase.cpp
WheatearEditor/src/Editor/EditorLayerBaseLifecycle.cpp
WheatearEditor/src/Editor/EditorLayer2D.h
WheatearEditor/src/Editor/EditorLayer2D.cpp
WheatearEditor/src/Editor/EditorLayer3D.h
WheatearEditor/src/Editor/EditorLayer3D.cpp
WheatearEditor/src/Assets/AssetRegistry.h
WheatearEditor/src/Build/PlayerPackager.h

WheatearSandbox/src/WheatearSandboxApp.cpp
WheatearSandbox/src/RuntimeSceneLayer.h
WheatearSandbox/src/RuntimeSceneLayer.cpp
```

如果你只想先抓主线，就先读这六个：

```text
EntryPoint.h
Application.h
Application.cpp
WheatearEditorApp.cpp
WheatearSandboxApp.cpp
RuntimeSceneLayer.cpp
```

它们分别回答：

```text
程序怎么启动？
谁创建 Application？
Application 负责什么？
编辑器怎么挑项目？
沙盒怎么找内容？
运行时场景怎么加载和切换？
```

---

## 7.2 这一层在架构中的位置

```text
应用层
    Editor / Sandbox
        ↓
核心平台层
    Application / Window / EventBus / LayerStack / Log / Timestep
        ↓
引擎服务层
    Renderer / AssetPath / UI / Input / Audio / Physics
        ↓
Scene ECS 层
    Scene / Entity / Components / Systems
```

应用层的职责不是“再造一套引擎”，而是：

- 把引擎能力包装成可用的宿主程序
- 把项目、场景、资源、构建流程串起来
- 把编辑器工具和玩家运行时分开
- 但仍然共享同一套底层核心

这就是为什么 Wheatear 会把应用层拆成两个主宿主：

- `WheatearEditor`
- `WheatearSandbox`

前者面向内容生产。  
后者面向最终运行。

---

## 7.3 `Application`：所有宿主共享的壳

### 7.3.1 `main()` 和 `CreateApplication()`

Wheatear 的入口不是把业务写进 `main()`，而是用一个很标准的宿主模式：

```cpp
int main(int argc, char** argv)
{
    Wheatear::Log::Init();
    Wheatear::Application* app = Wheatear::CreateApplication({ argc, argv });
    app->Run();
    delete app;
}
```

`EntryPoint.h` 里会先做几件很基础但很关键的事：

- 启用 Windows DPI awareness
- 初始化日志
- 调用 `CreateApplication()`
- 统一跑 `Application::Run()`

也就是说：

```text
EntryPoint 负责“怎么起程序”，
CreateApplication 负责“起哪个程序”，
Application 负责“程序跑起来以后怎么活”。
```

这三个概念不要混。

### 7.3.2 `ApplicationSpecification`

`Application` 不是裸创建的，它会带一个 `ApplicationSpecification`：

- `Name`
- `WorkingDirectory`
- `ProjectRoot`
- `AssetDirectoryName`
- `CommandLineArgs`

这意味着宿主不是“写死的全局程序”，而是可以携带上下文运行的。

尤其是这两个字段很重要：

- `ProjectRoot`：当前项目根
- `AssetDirectoryName`：资源目录名，默认是 `assets`

这让同一套宿主能跑不同项目，不必把路径硬写死。

### 7.3.3 `Application` 构造时做了什么

在 `Application.cpp` 里，构造函数会按顺序做这些事：

1. 设当前工作目录
2. 设置 `AssetPath` 的项目根和资源目录名
3. 创建窗口
4. 加载用户设置
5. 初始化渲染器
6. 初始化音频引擎
7. 创建并推入 `ImGuiLayer`

这个顺序很重要。

因为你必须先把：

- 路径
- 窗口
- 配置
- 渲染
- 音频

这些基础条件准备好，后面的 Editor 或 Sandbox 层才有地方依附。

### 7.3.4 `Run()` 主循环

`Application::Run()` 的主循环是整个宿主层最核心的节拍器：

```text
glfwPollEvents()
-> FlushEventQueue()
-> 计算 dt
-> 所有 Layer::OnUpdate()
-> ImGuiLayer::Begin()
-> 所有 Layer::OnImGuiRender()
-> ImGuiLayer::End()
-> Input::EndFrame()
-> Window::OnUpdate()
-> PostUpdateLayers()
```

你可以把它记成三段：

- 先处理平台事件
- 再更新业务层
- 再画界面和提交一帧

这里有两个很容易被忽略的点：

#### 1. `EventBus` 是延迟分发的

窗口回调先把事件入队，等主循环里统一 flush。  
这比“回调里直接 Dispatch”更稳，因为层的增删改不会跟事件分发互相打架。

#### 2. 输入状态要在帧末收口

`InputBindingService::EndFrame()` 和 `Input::EndFrame()` 会在每帧结尾执行。  
这样边缘按键、鼠标按下、重复触发这些状态就不会乱掉。

### 7.3.5 `LayerStack`、Overlay 和事件路由

`Application` 不是把所有功能都写死在一个类里，而是通过 `LayerStack` 管理：

- 普通 `Layer`
- `Overlay`

`ImGuiLayer` 就是一个 overlay。  
Editor 的各个工具层也都是挂在这个宿主上，而不是写死在主循环里。

事件路由也不是一股脑往下扔。  
`Application` 会按照层级给事件订阅优先级：

- 应用层先处理
- 再到各个 Layer
- 某个层 `Handled()` 后就可以 Consume

这也是为什么应用层适合做“壳”，不适合把业务逻辑堆进 `main()`。

---

## 7.4 `WheatearEditor`：内容制作宿主

### 7.4.1 编辑器不是一个单层界面

编辑器的入口在 `WheatearEditorApp.cpp`。  
它不是直接进入某个固定场景，而是先做一层启动分流：

```text
CreateApplication()
    -> new WheatearEditor(args)
        -> 处理命令行模式
        -> 设置项目根
        -> 初始化资产注册表
        -> 写入内建模板
        -> 推入 ModeSelectLayer
```

也就是说，Editor 先做的不是“打开界面”，而是“确认自己要以什么方式工作”。

### 7.4.2 命令行优先级

`WheatearEditorApp.cpp` 里有几个很实用的启动路径：

- `--project <dir>`：指定项目根
- `WHEATEAR_PROJECT`：环境变量指定项目
- `last_project.txt`：上次打开的项目
- `Projects/WheatearDemo`：默认示例项目
- 自动发现：兜底

这说明编辑器不是靠“当前目录碰巧对了”在工作，而是把项目定位做成了明确的规则。

还有几个 CLI-only 模式也很值得注意：

- `--refresh-assets`
- `--health`
- `--package` / `--package-player` / `--build-player`

这些命令会在不打开完整 UI 的情况下执行任务，做完就退出。  
这很像一个成熟工具链该有的样子。

### 7.4.3 为什么编辑器要先处理 `AssetPath::SetEngineRoot()`

编辑器有一类资源是“引擎内建资源”：

- 编辑器图标
- 默认字体
- 内建 shader
- 资源模板

这些资源不一定跟当前项目在一个目录里。  
所以编辑器先把 `EngineRoot` 设好，才能保证：

```text
项目资源走项目根，
编辑器资源走引擎根。
```

这一步如果不做，`Resources/Icons/Editor/...` 一类路径很容易在换项目时失效。

### 7.4.4 `ModeSelectLayer`：第一次进入编辑器看到什么

`ModeSelectLayer` 是编辑器的启动门面。

它解决两个问题：

1. 先选 2D 还是 3D 编辑模式
2. 再选当前项目

源码里它有两个启动按钮：

- `2D 模式`
- `3D 模式`

这两个按钮会分别推入：

- `EditorLayer2D`
- `EditorLayer3D`

然后把自己从层栈里移除。

### 7.4.5 项目怎么选

`ModeSelectLayer::DrawProjectSection()` 会扫描：

```text
<engine>/Projects/<project>/assets
```

也就是说，编辑器里的“项目列表”不是随便扫整个硬盘，而是围绕引擎仓库旁边的 `Projects` 目录组织的。

`ApplyProject()` 会做这些事：

- 检查是否有 `assets/`
- `AssetPath::SetProjectRoot(projectRoot)`
- `AssetRegistry::Get().Scan(projectRoot)`
- `AssetRegistry::Get().WriteRegistry()`
- 写 `last_project.txt`

这一步的意义很大：  
编辑器不仅切换了项目，还同步刷新了资产索引。

### 7.4.6 新项目怎么创建

`ModeSelectLayer` 里还有一个“创建项目”的入口。

它会：

1. 创建 `Projects/<name>/assets/scenes/Start.wt`
2. 写入一个最小起始场景
3. 拷贝 `ContentTemplates` 里的 gameplay / fonts / input
4. 写 `player.config`
5. 然后再 `ApplyProject()`

这个流程说明了一件事：

```text
编辑器不是只帮你打开项目，它还负责把项目的最小内容骨架搭起来。
```

### 7.4.7 `EditorLayerBase`：编辑器真正的壳层

`ModeSelectLayer` 只是启动门面。  
真正的编辑器工作流在 `EditorLayerBase` 里。

这个基类做了几件非常关键的事：

- 管场景的 Edit / Play 状态
- 管编辑器相机
- 管视口 framebuffer
- 管菜单栏、工具栏、内容浏览器、层级树、控制台
- 管撤销重做和脏标记
- 管保存、打开、创建、实例化 prefab / UI template
- 管 Play Mode 的暂停和单步

它的核心思想是：

```text
编辑器不是一整个新引擎，
而是一个挂在 Application 上的 Layer，
专门负责“看、改、存、跑”。
```

这也解释了为什么编辑器里会有 `SceneState::Edit` 和 `SceneState::Play`：

- Edit：在编辑场景里工作
- Play：切到运行时副本里预览

### 7.4.8 `EditorLayer2D` 和 `EditorLayer3D`

这两个类都继承自 `EditorLayerBase`，但目标不同。

- `EditorLayer2D`：以 2D 内容生产为主
- `EditorLayer3D`：以 3D 预览验证为主，带 IBL / SSAO / skybox / grid 等验证能力

也就是说，应用层把“编辑器”拆成了两个模式，不是为了炫技，而是为了让当前项目的主路径和验证路径分开。

对你这个以 2D 为主的引擎来说，2D 是主编辑器。  
3D 更像一个验证壳，用来确认 mesh / material / shadow / IBL / SSAO 这些能力没坏。

---

## 7.5 `WheatearSandbox`：玩家运行宿主

### 7.5.1 沙盒不是编辑器的简化版

沙盒的入口在 `WheatearSandboxApp.cpp`。  
它做的不是打开工具界面，而是把项目内容装进一个独立运行器里。

它的职责更像：

- 找内容
- 解包内容
- 找启动场景
- 运行游戏

这也是为什么它叫 Sandbox，而不是 Editor Runtime。

### 7.5.2 `--project` 和 packaged 模式

沙盒有两条明显路径：

#### 开发者模式

如果命令行带 `--project <dir>`，那它就直接跑这个项目的 loose assets。

这适合开发时：

- 改文件
- 直接重启沙盒看结果
- 不需要先打包

#### 打包玩家模式

如果没有显式 `--project`，它会尝试找 `content.wtpack`。

找到后会：

- 计算 fingerprint
- 抽取到缓存目录
- 设置这个缓存目录为 `ProjectRoot`
- 把可写根放到可执行文件附近

这保证了：

- 打包后的内容可独立运行
- 运行时生成的数据不会污染原项目

### 7.5.3 为什么要解包到缓存

`WheatearSandboxApp.cpp` 里不是直接在压缩包里读文件，而是先抽到 cache root。

这么做的好处是：

- 文件系统接口更统一
- `AssetPath::ResolveRuntimeData()` 仍然可以按 loose asset 逻辑找资源
- 包更新时可按 fingerprint 重新抽取

也就是说，玩家并不感知“内容来自 zip 还是磁盘”，沙盒只负责把它变成可读的项目根。

### 7.5.4 `RuntimeSceneLayer`：真正跑游戏的地方

沙盒推入的核心层是 `RuntimeSceneLayer`。

它的职责就是一个字：**跑**。

启动时它会：

- 读取 `player.config`
- 确定启动场景
- 解析命令行中传入的场景
- 加载场景
- 启动运行时系统

每帧更新时它会：

- 刷新 viewport
- 清屏
- 热重载 WAO 动作资源
- 调 `Scene::OnUpdateRuntime()`
- 消费运行时命令

### 7.5.5 运行时命令怎么消费

沙盒这一层会把运行时产生的命令统一收口：

- `DrainGameplayRuntimeCommands(scene, commands)`
- `CommandBus::DrainRuntimeCommands()`
- `SceneTransitionService::DrainRequests()`

然后按顺序处理：

- 普通按钮命令
- 退出命令 `quit`
- 场景切换
- 新游戏
- 读档

这说明沙盒不是只负责“把场景画出来”，它还要把按钮、剧情、存档、场景切换这些运行时动作串起来。

### 7.5.6 为什么 `LoadScene()` 要先清状态

`RuntimeSceneLayer::LoadScene()` 里会先做几件事：

- `UIInputSystem::Reset()`
- `CommandBus::ClearQueuedCommands()`
- 如果当前场景在运行，先 `OnRuntimeStop()`

然后才去：

- 解析新场景
- 设置 `GameProgress` 的场景上下文
- 应用自动读档槽
- `OnRuntimeStart()`

这一步很重要，因为切场景不是“换个指针”那么简单。  
它要先把旧状态关掉，再开新状态。

### 7.5.7 `WheatearEditorApp`：编辑器启动时先做什么

真正把编辑器拉起来的不是某个面板，而是 `WheatearEditorApp` 这一层命令调度。
它先决定三件事：

1. 这次是不是普通启动
2. 项目根和启动场景怎么选
3. 有没有命令行任务要直接执行，比如健康检查、刷新资源或打包

项目根的优先级在源码里是固定的，可以直接记成：

```text
--project
> WHEATEAR_PROJECT 环境变量
> 上次打开的项目
> Projects/WheatearDemo
> 自动发现的第一个项目
```

如果没有显式指定启动场景，编辑器还会继续读项目级配置
`assets/game/player.config`，然后才回落到引擎默认场景。
这说明 `player.config` 不只是给打包后的玩家程序看的，编辑器自己启动时也会尊重它。

`WheatearEditorApp` 里还有几条很有价值的命令入口：

- `--refresh-assets`：重扫资产并写 registry
- `--health`：跑健康检查，找缺失引用和坏跳转
- `--package-player` / `--build-player`：直接进入打包流程
- `--startup` / `--startup-scene`：显式指定起始场景

其中健康检查不是纯 UI 行为，它会调用 `AssetDependencyScanner`、
`ProjectSourceScanner`、`AssetRegistry::Scan()`，最后把结果写回 registry 或报告文件。
这就是为什么应用层要先把项目和资源根弄清楚，再让下面的面板工作。

### 7.5.8 `ModeSelectLayer`：第一次进入编辑器看到什么

`ModeSelectLayer` 负责两件最早发生的事：选项目和建项目。

选项目的时候，它会扫描项目列表，把可用工程列出来；  
建项目的时候，它不只是创建一个空文件夹，而是连模板都一起准备好。
源码里的动作大概是：

- 创建 `assets/scenes/Start.wt`
- 把 `ContentTemplates` 里的基础内容复制进项目
  - gameplay
  - fonts
  - input
- 写入 `assets/game/player.config`
- 让这个模板场景成为项目启动场景

也就是说，新项目第一次打开时就已经有一个可跑的最小闭环，
不是“只有文件夹，没有内容”。

这也解释了为什么 `ModeSelectLayer` 不是一个可有可无的欢迎页，
它其实是工程落地的一部分：项目模板、初始场景、默认配置、最近工程记录，
都从这里开始。

### 7.5.9 一条完整的编辑器启动链

把前面的东西串起来，编辑器这一层的启动链可以写成这样：

```text
main()
  -> CreateApplication()
  -> WheatearEditorApp
  -> 解析项目根 / 启动场景 / 命令行任务
  -> ModeSelectLayer 或直接进入已选项目
  -> EditorLayerBase::TransitionToEditScene()
  -> 同步 SceneHierarchy / ContentBrowser / Inspector / Viewport
  -> 按下 Play
  -> EditorLayerBase::TransitionToPlay()
  -> 复制编辑场景得到运行副本
  -> OnRuntimeStart()
  -> 按下 Stop
  -> EditorLayerBase::TransitionToStop()
  -> OnRuntimeStop()
  -> 回到原编辑场景
```

如果是沙盒程序，则会走另一条同样清楚的链：

```text
main()
  -> CreateApplication()
  -> WheatearSandbox
  -> RuntimeSceneLayer::OnAttach()
  -> 读取 player.config
  -> 找到 content.wtpack 或 loose runtime data
  -> LoadScene()
  -> OnRuntimeStart()
```

这就是这一章真正想讲清楚的东西：
Editor 负责制作和预览，Sandbox 负责独立运行，  
但它们共享的是同一套 Scene、Renderer、CommandBus 和 Runtime Services。

---

## 7.6 Editor 和 Sandbox 到底差在哪

这两个宿主看起来都能跑场景，但本质上不是同一种东西。

### Editor 更像“制作机器”

Editor 关注的是：

- 项目管理
- 资源浏览
- 组件编辑
- 场景编辑
- Play Mode 预览
- 打包前检查

### Sandbox 更像“玩家机器”

Sandbox 关注的是：

- 加载内容
- 跑玩法
- 接收输入
- 执行命令
- 读档 / 存档 / 切场景

### 它们共享什么

共享的是：

- `Application`
- `Window`
- `Renderer`
- `Scene`
- `CommandBus`
- `AssetPath`
- `EventBus`

### 它们不共享什么

不共享的是：

- 编辑器 UI 面板
- 资源浏览器
- 场景层级树
- 运行时工具栏
- 打包检查任务

所以你可以把 Wheatear 理解成：

```text
一个核心引擎 + 两个宿主壳 + 一套共享项目内容。
```

---

## 7.7 一个完整例子：从打开编辑器到跑出游戏

### 例子 1：编辑器工作流

```text
main()
  -> CreateApplication()
  -> new WheatearEditor(args)
  -> ModeSelectLayer
  -> 选择项目
  -> 选择 2D 模式
  -> 推入 EditorLayer2D
  -> 打开场景
  -> 编辑实体和资源
  -> 按 Play 进入运行副本
  -> 按 Stop 回到编辑态
```

这个流程里，编辑器做了两件很关键的事：

- 编辑态和运行态分离
- 但仍然共享同一套项目根和资源语义

### 例子 2：沙盒运行工作流

```text
main()
  -> CreateApplication()
  -> new WheatearSandbox(args)
  -> RuntimeSceneLayer
  -> 读 player.config
  -> 加载启动场景
  -> 运行 Update()
  -> 消费 UI / CommandBus / SceneTransition 命令
  -> 切场景 / 读档 / 新游戏
```

这个流程里，沙盒做的事情非常纯粹：

- 把内容跑起来
- 把运行时副作用接住
- 让最终玩家看到的流程成立

---

## 7.8 和 Unity、UE、Godot 怎么对照

### Unity

Unity 的对应关系大概是：

- `Application` 约等于 Unity 的宿主框架
- `WheatearEditor` 约等于 Unity Editor
- `WheatearSandbox` 约等于 Build 出来的 Player
- `ModeSelectLayer` 约等于项目启动/模式选择壳
- `EditorLayerBase` 约等于 Editor 内部的场景编辑与工具层

区别在于，Wheatear 更轻，宿主和工具层更贴近 C++ 代码。

### Unreal

UE 里可以类比成：

- Editor
- PIE / SIE
- Standalone Game
- 打包后的运行程序

Wheatear 的思路和它很像，只是没有那么重的框架外壳。

### Godot

Godot 里可以对应到：

- Editor
- Project Manager
- Play Scene / Run Project
- Exported game

Wheatear 和 Godot 一样，都很强调“编辑器”和“运行时”是不同入口，但项目内容是一致的。

---

## 7.9 这一层的优点、坏处和常见误区

### 优点

- Editor 和 Sandbox 共用同一套底层引擎
- 项目根和资源根被明确管理
- CLI 可以直接做健康检查和打包
- 编辑器既能做内容制作，也能做运行预览
- 沙盒既能跑 loose assets，也能跑打包内容

### 坏处

- 应用层代码会比核心层更杂
- 启动分支多，容易把“工具任务”和“运行时任务”混在一起
- 项目路径、缓存路径、可写路径如果不讲清楚，后面很容易出 bug
- Editor / Sandbox / Build 三种语义交叉时，代码维护成本会上来

### 常见误区

- 把 Editor 当成“界面层”，忽略它其实是内容生产宿主
- 把 Sandbox 当成“简单测试程序”，忽略它其实是玩家流程宿主
- 觉得 `main()` 就该直接 new 场景，不需要 `CreateApplication`
- 在代码里到处直接写当前目录，而不走 `AssetPath`
- 忘了打包模式下可写数据应该和只读内容分开
- 忘了编辑器里还有 CLI-only 的健康检查和打包任务

---

## 7.10 面试时怎么讲

如果面试官问你：“Wheatear 的 Editor 和 Sandbox 为什么要分开？”

你可以这样答：

```text
因为它们面对的是两种完全不同的使用场景。
Editor 是内容制作宿主，负责项目选择、资源编辑、场景编辑、Play Mode 和打包前检查；
Sandbox 是运行时宿主，负责加载 loose assets 或打包内容，执行剧情、战斗、存档和场景切换。
它们共用同一套 Application、Renderer、Scene 和 Runtime Services，
但入口、层栈和生命周期是不一样的。
```

如果面试官继续问：“为什么不把这些都写进 main()？”

你可以补一句：

```text
因为 main 只应该做最小启动，真正的差异化逻辑应该通过 CreateApplication 和 LayerStack 注入。
这样同一套引擎核心才能同时服务编辑器和玩家运行时。
```

如果继续追问：“打包后的内容怎么跑？”

你就说：

```text
Sandbox 会找 content.wtpack，按 fingerprint 解包到缓存目录，再把缓存目录当成项目根；
可写数据则放到可执行文件附近或用户缓存目录。
这样玩家运行时看到的依然是同一套资源语义，只是来源换成了打包内容。
```

---

## 7.11 小结

这一章其实在讲一个很简单但很重要的事实：

```text
引擎不是只靠核心模块活着，
它还需要一个合理的宿主层，把核心能力组织成“编辑器”和“玩家程序”。
```

Wheatear 的应用层做了三件事：

- `Application` 提供统一宿主壳
- `WheatearEditor` 提供内容制作宿主
- `WheatearSandbox` 提供独立运行宿主

把这一层看懂以后，你后面再看：

- 资源浏览器
- Inspector
- 场景预览
- 打包发布

会更顺。  
因为那些都只是应用层壳子上的不同工具面。

下一章，我们就接着写 `资源浏览器与 Inspector`。  
那一章会直接回答：

```text
资源树是怎么扫出来的？
Inspector 又是怎么把 YAML / 组件 / 资产改回去的？
```
