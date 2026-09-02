# 第 2 编 01：Core 层与主循环

> 本章学习目标：理解 Wheatear 是如何从 `main()` 启动起来的，如何创建窗口，如何管理
> `Layer`，如何分发事件，以及每一帧是怎样被组织起来的。
>
> 本章会反复使用三个具体例子：
>
> 1. 双击 `WheatearEditor.exe` 后，编辑器是怎样启动的。
> 2. 在启动界面点击“2D 编辑器”后，为什么当前 Layer 能切换成 `EditorLayer2D`。
> 3. 在 Sandbox 里点击一个 UI 按钮时，鼠标事件怎样从 GLFW 走到 `UIInputSystem`。

## 2.1 本章在整个架构中的位置

上一章把 Wheatear 分成了几个层级。本章学习“核心平台层”：它位于第三方与平台层之上，是引擎自己的运行地基：

```text
应用层
    WheatearEditor / WheatearSandbox
        ↓
Core 层
    EntryPoint / Application / Layer / LayerStack
    Event / EventBus / Timestep / Log
        ↓
平台实现层
    Window / WindowsWindow / OpenGLContext
        ↓
第三方库
    GLFW / OpenGL / ImGui
```

Core 层不应该知道“横板战斗怎么打”“VN 对话怎么推进”。它只负责提供一套稳定的程序骨架：

- 程序如何启动。
- 窗口如何创建。
- 每一帧如何运行。
- 功能模块如何插入和移除。
- 操作系统事件如何传递给功能模块。
- 引擎什么时候初始化和释放基础服务。

如果把一个游戏比作舞台，Core 层不是演员，也不是剧本，而是剧场的开场、灯光调度、节目顺序和散场流程。

## 2.2 需要阅读的源码

本章的主要源码如下：

```text
Wheatear/src/Wheatear/Core/EntryPoint.h
Wheatear/src/Wheatear/Core/Core.h
Wheatear/src/Wheatear/Core/Application.h
Wheatear/src/Wheatear/Core/Application.cpp
Wheatear/src/Wheatear/Core/Layer.h
Wheatear/src/Wheatear/Core/LayerStack.h
Wheatear/src/Wheatear/Core/LayerStack.cpp
Wheatear/src/Wheatear/Core/Timestep.h

Wheatear/src/Wheatear/Core/Window.h
Wheatear/src/Platform/Windows/WindowsWindow.h
Wheatear/src/Platform/Windows/WindowsWindow.cpp
Wheatear/src/Platform/OpenGL/OpenGLContext.h
Wheatear/src/Platform/OpenGL/OpenGLContext.cpp

Wheatear/src/Wheatear/Events/Event.h
Wheatear/src/Wheatear/Events/EventBus.h
Wheatear/src/Wheatear/Events/EventBus.cpp
Wheatear/src/Wheatear/Events/ApplicationEvent.h
Wheatear/src/Wheatear/Events/MouseEvent.h
Wheatear/src/Wheatear/Events/KeyEvent.h
Wheatear/src/Wheatear/ImGui/ImGuiLayer.h
Wheatear/src/Wheatear/ImGui/ImGuiLayer.cpp

WheatearEditor/src/WheatearEditorApp.cpp
WheatearEditor/src/Editor/ModeSelectLayer.cpp
WheatearSandbox/src/WheatearSandboxApp.cpp
WheatearSandbox/src/RuntimeSceneLayer.cpp
```

## 2.3 先看全局调用链

先不要急着看每一行。先看程序的宏观调用链：

```text
操作系统启动进程
    ↓
EntryPoint.h 中的 main()
    ↓
CreateApplication(args)
    ↓
WheatearEditor 或 WheatearSandbox 构造
    ↓
Application 构造
    ├─ 设置项目路径
    ├─ 创建 Window
    ├─ 初始化 Renderer / Audio
    ├─ 创建 ImGuiLayer
    └─ 排队加入业务 Layer
    ↓
Application::Run()
    ↓
提交第一次待处理的 Layer
    ↓
主循环
    ├─ glfwPollEvents()
    ├─ EventBus::Flush()
    ├─ Layer::OnUpdate()
    ├─ ImGuiLayer::Begin()
    ├─ Layer::OnImGuiRender()
    ├─ ImGuiLayer::End()
    ├─ Input::EndFrame()
    ├─ Window::OnUpdate()
    └─ 提交待添加 / 待删除 Layer
```

整个 Core 层的关键就是：

```text
Application 负责调度
Layer       负责功能
EventBus    负责通信
Window      负责平台接入
```

## 2.4 程序入口：`main()` 只负责总生命周期

### 2.4.1 `EntryPoint.h` 的定位

源码位置：

```text
Wheatear/src/Wheatear/Core/EntryPoint.h
```

入口函数是：

```cpp
int main(int argc, char** argv)
{
    EnablePerMonitorDpiAwareness();
    Wheatear::Log::Init();

    Wheatear::Application* app = nullptr;
    try
    {
        app = Wheatear::CreateApplication({ argc, argv });
        app->Run();
    }
    catch (const std::exception& e)
    {
        Wheatear::Log::GetCoreLogger()->error(
            "Fatal unhandled exception: {}", e.what());
    }

    delete app;
    return 0;
}
```

真实源码还包含性能分析会话和未知异常捕获，这里先保留主干。

这里有一个重要设计：

```cpp
Application* CreateApplication(ApplicationCommandLineArgs args);
```

它只在引擎中声明，具体应用自己实现。

### 2.4.2 为什么引擎可以提供 `main()`，又让 Editor/Sandbox 决定应用

如果每个程序都自己写一套 `main()`，编辑器和 Sandbox 很容易出现不同的初始化顺序：

```text
Editor:
    自己初始化日志
    自己创建窗口
    自己初始化 Renderer
    自己写循环

Sandbox:
    另一套日志
    另一套窗口
    另一套循环
```

久而久之，两个程序的行为会逐渐分叉。

Wheatear 采用的是“固定入口，应用工厂可变”：

```text
引擎统一提供：
    main()
    异常捕获
    日志启动
    App Run / Shutdown 生命周期

具体程序决定：
    创建哪一种 Application
    添加哪些 Layer
    注册哪些玩法模块
```

编辑器实现：

```cpp
Wheatear::Application* Wheatear::CreateApplication(
    Wheatear::ApplicationCommandLineArgs args)
{
    return new WheatearEditor(args);
}
```

Sandbox 实现：

```cpp
Wheatear::Application* Wheatear::CreateApplication(
    Wheatear::ApplicationCommandLineArgs args)
{
    return new WheatearSandbox(args);
}
```

这就是一个简单的应用工厂入口。`main()` 不需要知道编辑器和 Sandbox 的具体类，只需要调用统一的 `CreateApplication()`。

### 2.4.3 具体例子：双击编辑器后发生了什么

假设你运行：

```text
WheatearEditor.exe --project Projects/WheatearDemo
```

调用过程是：

```text
Windows 创建进程
    ↓
main(argc, argv)
    ↓
Log::Init()
    ↓
CreateApplication({argc, argv})
    ↓
WheatearEditor(args)
    ↓
Application(CreateEditorSpecification(args))
```

`WheatearEditor` 的构造函数会：

```cpp
RegisterCoreEditorComponents();
RegisterDefaultGameplayEditorModules();
RegisterDefaultGameplayModules();
AssetRegistry::Get().LoadCache(projectRoot);
PushLayer(std::make_unique<ModeSelectLayer>());
```

注意：这里的 `PushLayer()` 并不会立刻把 `ModeSelectLayer` 放入 `LayerStack`，后面会专门解释。

此时编辑器还没有开始正式绘制界面。它只是完成了“准备工作”和“提出添加 Layer 的请求”。

接着回到 `main()`：

```cpp
app->Run();
```

`Run()` 首先调用：

```cpp
PostUpdateLayers();
```

这时 `ModeSelectLayer` 才真正被放入 LayerStack，并调用：

```cpp
ModeSelectLayer::OnAttach();
```

然后才进入第一轮循环。于是启动画面才有机会绘制出来。

## 2.5 `ApplicationSpecification`：把启动参数集中起来

源码位置：

```text
Wheatear/src/Wheatear/Core/Application.h
```

核心结构体：

```cpp
struct ApplicationSpecification
{
    std::string Name = "Wheatear App";
    std::filesystem::path WorkingDirectory;
    std::filesystem::path ProjectRoot;
    std::filesystem::path AssetDirectoryName = "assets";
    ApplicationCommandLineArgs CommandLineArgs;
};
```

这比下面这种构造函数更容易扩展：

```cpp
Application(
    const std::string& name,
    const std::filesystem::path& workingDirectory,
    const std::filesystem::path& projectRoot,
    const std::filesystem::path& assetDirectory,
    int argc,
    char** argv);
```

使用 Specification 的好处是：

```cpp
Application(CreateEditorSpecification(args));
Application(CreateWheatearSandboxSpecification(args));
```

调用者先构造一份有名字的配置，再交给 Application。以后新增窗口模式、默认场景、日志目录等字段，不需要不断修改构造函数参数列表。

### 2.5.1 Editor 如何决定项目根目录

编辑器的 `ResolveEditorProjectRoot()` 按这个顺序找项目：

```text
--project <dir>
    ↓
WHEATEAR_PROJECT 环境变量
    ↓
上次打开的项目
    ↓
Projects/WheatearDemo
    ↓
AssetPath::DiscoverProjectRoot()
```

这属于应用层逻辑，而不是 Core 层逻辑。Core 只接受最终结果：

```cpp
specification.ProjectRoot = ResolveEditorProjectRoot(args);
```

这体现了一个边界：

```text
Application 知道“我要一个 ProjectRoot”
Editor 知道“应该怎样选择 ProjectRoot”
```

### 2.5.2 Sandbox 如何决定资源来源

Sandbox 的 `CreateWheatearSandboxSpecification()` 还要区分：

```text
开发模式：
    --project <dir>
    直接从项目 assets/ 读取

打包模式：
    找到 .wtpack
    解压到运行时缓存
    从缓存目录读取

仓库默认运行：
    Projects/WheatearDemo
```

但无论哪种模式，最后都会生成：

```cpp
ApplicationSpecification specification;
specification.Name = "Wheatear Sandbox";
specification.ProjectRoot = ...;
specification.CommandLineArgs = args;
```

Core 层不需要知道资源是 loose files 还是 `.wtpack` 解压目录。

## 2.6 `Application` 构造：建立引擎运行环境

源码位置：

```text
Wheatear/src/Wheatear/Core/Application.cpp
```

构造函数的主要步骤：

```cpp
Application::Application(const ApplicationSpecification& specification)
    : m_Specification(specification)
{
    WT_CORE_ASSERT(!s_Instance, "Application already exists!");
    s_Instance = this;

    if (!m_Specification.WorkingDirectory.empty())
        std::filesystem::current_path(...);

    AssetPath::SetAssetDirectoryName(...);
    AssetPath::SetProjectRoot(...);

    m_Window = std::unique_ptr<Window>(
        Window::Create(WindowProps(m_Specification.Name)));

    m_Window->SetEventCallback(BIND_EVENT_FN(OnEvent));

    UserSettings::Load();
    UserSettings::ApplyToRuntime();

    InstallApplicationEventHandlers();

    Renderer::Init();
    AudioEngine::Init();

    m_ImGuiLayer = new ImGuiLayer();
    PushOverlay(std::unique_ptr<ImGuiLayer>(m_ImGuiLayer));
}
```

下面分别理解。

### 2.6.1 单例：`Application::Get()`

Application 里有：

```cpp
static Application* s_Instance;

inline static Application& Get()
{
    return *s_Instance;
}

inline static bool Exists()
{
    return s_Instance != nullptr;
}
```

构造时：

```cpp
WT_CORE_ASSERT(!s_Instance, "Application already exists!");
s_Instance = this;
```

这表示当前进程预期只有一个 Application。

因此代码中可以写：

```cpp
Application::Get().GetWindow();
Application::Get().PushLayer(...);
Application::Get().GetEventBus();
```

### 设计好处

- 获取全局窗口、LayerStack、EventBus 很方便。
- 对个人项目和单窗口 2D 游戏来说，调用成本低。
- 不需要把 Application 指针层层传给每个系统。

### 设计代价

- 依赖关系隐藏在 `Application::Get()` 中。
- 单元测试不容易同时创建多个独立 Application。
- 将来如果支持多窗口、多游戏世界或多个运行上下文，单例会变成限制。

面试时不要只说“用了单例”。要继续说：

```text
这是针对单窗口桌面游戏的工程取舍。它简化了全局基础设施访问，
但牺牲了多实例隔离和测试便利性。
```

### 2.6.2 设置 AssetPath

Application 不负责加载具体图片，但必须先让资源系统知道项目根目录：

```cpp
AssetPath::SetAssetDirectoryName(
    m_Specification.AssetDirectoryName);

AssetPath::SetProjectRoot(
    m_Specification.ProjectRoot.empty()
        ? AssetPath::DiscoverProjectRoot()
        : m_Specification.ProjectRoot);
```

这样后续代码才能写：

```cpp
AssetPath::Resolve("assets/scenes/Intro.wt");
```

而不必每个系统都自己拼绝对路径。

这一层关系是：

```text
Application 决定项目根
AssetPath 负责路径解析
Scene / Texture / Audio 使用解析结果
```

### 2.6.3 创建 Window

Application 只调用抽象接口：

```cpp
m_Window = std::unique_ptr<Window>(
    Window::Create(WindowProps(m_Specification.Name)));
```

`Window` 是接口：

```cpp
class Window
{
public:
    virtual ~Window() {}

    virtual void OnUpdate() = 0;

    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;

    virtual void SetEventCallback(
        const EventCallbackFn& callback) = 0;

    virtual void SetVSync(bool enabled) = 0;
    virtual void SetFullscreen(bool enabled) = 0;

    static Window* Create(
        const WindowProps& props = WindowProps());
};
```

当前平台实现是：

```cpp
Window* Window::Create(const WindowProps& props)
{
    return new WindowsWindow(props);
}
```

这就是一个简单的工厂和抽象接口组合。

上层看到的是：

```cpp
Window::Create(...)
```

平台实现里才出现：

```cpp
glfwInit();
glfwCreateWindow(...);
```

### 2.6.4 创建 OpenGL 上下文

`WindowsWindow::Init()` 做了几件事：

```cpp
glfwInit();
glfwCreateWindow(...);
m_Context = CreateScope<OpenGLContext>(m_Window);
m_Context->Init();
SetupCallbacks();
```

其中：

```cpp
class OpenGLContext : public GraphicsContext
{
public:
    void Init() override;
    void SwapBuffers() override;
};
```

`OpenGLContext::Init()` 负责让当前窗口拥有可用的 OpenGL 上下文，并通过 GLAD 加载 OpenGL 函数。

这时才具备真正绘制精灵和 UI 的条件。

### 2.6.5 初始化 Renderer 和 Audio

Application 继续调用：

```cpp
Renderer::Init();
AudioEngine::Init();
```

这说明 Core 层负责“启动服务”，但不负责“使用服务”。

例如：

```text
Application:
    Renderer::Init()

RenderSystem:
    Renderer2D::BeginScene()
    Renderer2D::DrawSprite()
    Renderer2D::EndScene()
```

服务的初始化集中在 Application，具体使用则由 Scene/System 完成。

### 2.6.6 ImGuiLayer 为什么是 Overlay

构造函数最后：

```cpp
m_ImGuiLayer = new ImGuiLayer();
PushOverlay(std::unique_ptr<ImGuiLayer>(m_ImGuiLayer));
```

ImGui 是编辑器和调试界面的上层显示内容，通常应该位于普通游戏画面之上。因此它被作为 Overlay 放在 LayerStack 末尾。

但这里有一个容易误解的地方：

```text
ImGuiLayer 作为 LayerStack 中的 Overlay 存在
    主要为了参与生命周期和事件路由

ImGuiLayer::Begin / End
    由 Application::Run() 显式调用

其他 Layer::OnImGuiRender()
    在 Begin 和 End 之间执行
```

主循环是：

```cpp
m_ImGuiLayer->Begin();

for (Layer* layer : m_LayerStack)
    layer->OnImGuiRender();

m_ImGuiLayer->End();
```

因此 `EditorLayer2D` 可以在自己的 `OnImGuiRender()` 中调用：

```cpp
ImGui::Begin("Scene Hierarchy");
...
ImGui::End();
```

而不需要自己管理 ImGui 帧的开始和结束。

## 2.7 `Layer`：功能模块的生命周期接口

源码位置：

```text
Wheatear/src/Wheatear/Core/Layer.h
```

接口非常小：

```cpp
class Layer
{
public:
    Layer(const std::string& name = "Layer");
    virtual ~Layer();

    virtual void OnAttach() {}
    virtual void OnDetach() {}
    virtual void OnUpdate(Timestep ts) {}
    virtual void OnImGuiRender() {}
    virtual void OnEvent(Event& event) {}

    const std::string& GetName() const;
};
```

这个接口体现了引擎最基本的功能模块模型：

```text
加入引擎
    → OnAttach()

每帧更新
    → OnUpdate(ts)

每帧绘制工具 UI
    → OnImGuiRender()

收到输入/窗口事件
    → OnEvent(event)

离开引擎
    → OnDetach()
```

### 2.7.1 现有 Layer 例子

```text
ModeSelectLayer
    编辑器启动模式选择界面

EditorLayer2D
    2D 场景编辑、视口、Inspector、Hierarchy

EditorLayer3D
    3D 编辑和光照预览

RuntimeSceneLayer
    Sandbox 中加载并运行游戏场景

ImGuiLayer
    ImGui 上下文、输入拦截、Begin / End
```

它们都不需要 Application 为每一种功能写专门的字段：

```cpp
std::vector<std::unique_ptr<Layer>> m_Layers;
```

Application 只知道“这里有一组 Layer”，然后统一调度。

## 2.8 `LayerStack`：普通 Layer 和 Overlay 的顺序

源码位置：

```text
Wheatear/src/Wheatear/Core/LayerStack.h
Wheatear/src/Wheatear/Core/LayerStack.cpp
```

LayerStack 内部：

```cpp
std::vector<std::unique_ptr<Layer>> m_Layers;
unsigned int m_LayerInsertIndex = 0;
```

插入普通 Layer：

```cpp
void LayerStack::PushLayer(std::unique_ptr<Layer> layer)
{
    m_Layers.emplace(
        m_Layers.begin() + m_LayerInsertIndex,
        std::move(layer));

    m_LayerInsertIndex++;
}
```

插入 Overlay：

```cpp
void LayerStack::PushOverlay(std::unique_ptr<Layer> overlay)
{
    m_Layers.emplace_back(std::move(overlay));
}
```

因此内部顺序类似：

```text
普通 Layer 区域
    [ModeSelectLayer]
    [EditorLayer2D]
    [RuntimeSceneLayer]

Overlay 区域
    [ImGuiLayer]
```

### 2.8.1 为什么要区分 Overlay

假设一个编辑器有：

```text
Scene View
Hierarchy
Inspector
ImGui
```

场景视图应该先绘制，ImGui 面板应该覆盖在它上面。

如果 ImGui 被插入普通 Layer 区域中间，可能出现：

```text
Scene View
ImGui
某个后加入的游戏 Layer
```

后加入的游戏 Layer 可能又把画面覆盖掉。

Overlay 的设计保证：

```text
普通内容先处理
Overlay 最后处理
```

除了绘制顺序，Overlay 通常也应该有更高的事件优先级。

### 2.8.2 所有权关系

LayerStack 通过 `unique_ptr` 拥有 Layer：

```text
Application 创建 unique_ptr
    ↓
LayerStack 接管 unique_ptr
    ↓
Pop / Clear 时调用 OnDetach()
    ↓
unique_ptr 自动销毁对象
```

所以调用者不应该在 `PopLayer()` 后再次 `delete`。

## 2.9 为什么 Layer 的添加和删除要延迟

Application 的公开 API：

```cpp
void PushLayer(std::unique_ptr<Layer> layer);
void PopLayer(Layer* layer);
void PushOverlay(std::unique_ptr<Layer> overlay);
void PopOverlay(Layer* overlay);
```

但是 `PushLayer()` 只是：

```cpp
m_PendingLayersToPush.push_back(std::move(layer));
```

`PopLayer()` 只是：

```cpp
m_PendingLayersToPop.push_back(layer);
```

真正提交在：

```cpp
Application::PostUpdateLayers();
```

### 2.9.1 具体例子：点击“2D 编辑器”

编辑器启动时，普通 Layer 是：

```text
[ModeSelectLayer]
[ImGuiLayer]
```

用户在 ModeSelectLayer 中点击“2D 编辑器”按钮，代码是：

```cpp
void ModeSelectLayer::LaunchEditor2D()
{
    if (m_Decided)
        return;

    m_Decided = true;

    Application::Get().PushLayer(
        std::make_unique<EditorLayer2D>());

    Application::Get().PopLayer(this);
}
```

执行之后，LayerStack 并没有立刻变成：

```text
[EditorLayer2D]
[ImGuiLayer]
```

而是暂时保持：

```text
[ModeSelectLayer]
[ImGuiLayer]
```

同时待处理队列是：

```text
待添加：
    EditorLayer2D

待删除：
    ModeSelectLayer
```

本帧结束时，`PostUpdateLayers()` 执行：

```text
1. 清理旧的 Layer 事件订阅
2. PopLayer(ModeSelectLayer)
   ├─ ModeSelectLayer::OnDetach()
   └─ unique_ptr 销毁 ModeSelectLayer
3. PushLayer(EditorLayer2D)
   ├─ 放入 LayerStack
   └─ EditorLayer2D::OnAttach()
4. 重新构建 Layer 事件路由
```

下一帧开始时，LayerStack 才是：

```text
[EditorLayer2D]
[ImGuiLayer]
```

### 2.9.2 如果立即修改会有什么问题

主循环正在遍历：

```cpp
for (Layer* layer : m_LayerStack)
    layer->OnUpdate(timestep);
```

假设 `ModeSelectLayer::OnUpdate()` 中直接删除自己：

```cpp
// 危险示意
Application::Get().GetLayerStack().PopLayer(this);
```

那么底层 `std::vector` 可能发生元素移动，当前遍历使用的迭代器、引用或后续元素指针都可能失效。

延迟提交的本质是：

```text
更新阶段：只提出结构变化请求
安全点：统一修改 LayerStack
下一轮：使用新结构继续运行
```

这个思想在引擎里非常常见，也会在 ECS 实体销毁、资源卸载、场景切换中反复出现。

## 2.10 主循环：`Application::Run()`

源码位置：

```text
Wheatear/src/Wheatear/Core/Application.cpp
```

主循环核心：

```cpp
void Application::Run()
{
    PostUpdateLayers();

    while (m_Running)
    {
        glfwPollEvents();
        FlushEventQueue();

        if (!m_Running)
            break;

        const double nowTime = glfwGetTime();
        Timestep timestep =
            static_cast<float>(nowTime - m_LastFrameTime);
        m_LastFrameTime = nowTime;

        if (!m_Minimized)
        {
            for (Layer* layer : m_LayerStack)
                layer->OnUpdate(timestep);

            m_ImGuiLayer->Begin();

            for (Layer* layer : m_LayerStack)
                layer->OnImGuiRender();

            m_ImGuiLayer->End();
        }

        InputBindingService::EndFrame();
        Input::EndFrame();

        m_Window->OnUpdate();
        PostUpdateLayers();
    }
}
```

下面逐段解释。

### 2.10.1 第一步：`glfwPollEvents()`

```cpp
glfwPollEvents();
```

这一步让 GLFW 处理操作系统消息，例如：

```text
鼠标移动
鼠标按下
鼠标释放
键盘按下
窗口改变大小
窗口关闭
```

GLFW 会调用之前通过 `glfwSet...Callback()` 注册的回调。

### 2.10.2 第二步：`FlushEventQueue()`

Window 回调不直接调用 Layer，而是把事件交给：

```cpp
Application::OnEvent(Event& e)
{
    m_EventBus.Queue(e);
}
```

然后统一：

```cpp
FlushEventQueue()
{
    m_EventBus.Flush();
}
```

这样 Window 的平台回调只负责“翻译和上报”，EventBus 负责“调度和消费”。

### 2.10.3 第三步：计算 `Timestep`

```cpp
const double nowTime = glfwGetTime();
Timestep timestep =
    static_cast<float>(nowTime - m_LastFrameTime);
m_LastFrameTime = nowTime;
```

`Timestep` 的接口是：

```cpp
class Timestep
{
public:
    Timestep(float time = 0.0f)
        : m_Time(time) {}

    operator float() const { return m_Time; }
    float GetSeconds() const { return m_Time; }
    float GetMilliseconds() const
    {
        return m_Time * 1000.0f;
    }
};
```

系统可以写：

```cpp
void PlayerSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
{
    float seconds = ts.GetSeconds();
    playerSpeed += acceleration * seconds;
}
```

而不是假设每台电脑每帧都是固定的 `1 / 60` 秒。

源码使用 `double` 保存 GLFW 的时间差，再转换成 `float` 传给系统，是为了减少长时间运行时的时间精度损失。

### 2.10.4 第四步：更新所有 Layer

```cpp
for (Layer* layer : m_LayerStack)
    layer->OnUpdate(timestep);
```

这一步才真正进入编辑器或游戏逻辑：

```text
EditorLayer2D::OnUpdate()
    → 更新编辑相机
    → 更新编辑场景
    → 更新视口和选中对象

RuntimeSceneLayer::OnUpdate()
    → 更新运行时 Scene
    → 更新玩法系统
    → 处理场景切换
```

Application 不需要知道每个 Layer 具体做什么。

### 2.10.5 第五步：ImGui 帧

```cpp
m_ImGuiLayer->Begin();

for (Layer* layer : m_LayerStack)
    layer->OnImGuiRender();

m_ImGuiLayer->End();
```

注意 `OnUpdate()` 和 `OnImGuiRender()` 的职责不同：

```text
OnUpdate:
    游戏状态、相机、系统、数据更新

OnImGuiRender:
    编辑器面板、调试窗口、工具栏、Inspector
```

例如 `EditorLayerBase` 会在 `OnUpdate()` 中更新场景，而在 `OnImGuiRender()` 中绘制：

```text
Scene Hierarchy
Inspector
Content Browser
Viewport Toolbar
Console
Animation Editor
```

### 2.10.6 第六步：提交输入状态

```cpp
InputBindingService::EndFrame();
Input::EndFrame();
```

边沿输入需要知道：

```text
本帧状态
上一帧状态
```

例如：

```cpp
InputBindingService::IsActionPressed("side.jump");
```

应该只在按键刚从“未按下”变成“按下”的那一帧返回 `true`。

如果某个模块在本帧没有查询某个 action，系统仍然要在帧末统一提交状态，不能因为“没查询”就破坏下一帧的比较。

### 2.10.7 第七步：交换缓冲

```cpp
m_Window->OnUpdate();
```

Windows 实现：

```cpp
void WindowsWindow::OnUpdate()
{
    m_Context->SwapBuffers();
}
```

OpenGL 的绘制命令通常先提交到后备缓冲区，`SwapBuffers()` 把后备缓冲显示到窗口。

所以一个最小的“画面显示”过程是：

```text
Renderer2D 产生绘制命令
    ↓
OpenGL 写入 back buffer
    ↓
SwapBuffers()
    ↓
用户看到这一帧
```

## 2.11 窗口平台适配：从 GLFW 到 Wheatear Event

### 2.11.1 Window 接口的意义

引擎公共代码依赖：

```cpp
Window::GetWidth();
Window::SetVSync();
Window::SetEventCallback();
Window::OnUpdate();
```

而不是依赖：

```cpp
glfwGetWindowSize();
glfwSwapInterval();
glfwPollEvents();
```

这种隔离带来的好处是：

```text
上层：
    只知道 Window 接口

平台层：
    决定 Window 是 GLFW、Win32、SDL 还是其他实现
```

当前代码主要面向 Windows + GLFW + OpenGL，但接口已经给未来替换平台留下了位置。

### 2.11.2 GLFW 回调的注册

`WindowsWindow::SetupCallbacks()` 中注册：

```cpp
glfwSetWindowSizeCallback(...);
glfwSetWindowCloseCallback(...);
glfwSetKeyCallback(...);
glfwSetCharCallback(...);
glfwSetMouseButtonCallback(...);
glfwSetScrollCallback(...);
glfwSetCursorPosCallback(...);
```

以鼠标按键为例：

```cpp
glfwSetMouseButtonCallback(
    m_Window,
    [](GLFWwindow* window, int button, int action, int)
    {
        auto& data = *static_cast<WindowData*>(
            glfwGetWindowUserPointer(window));

        switch (action)
        {
        case GLFW_PRESS:
        {
            MouseButtonPressedEvent event(button);
            data.EventCallback(event);
            break;
        }
        case GLFW_RELEASE:
        {
            MouseButtonReleasedEvent event(button);
            data.EventCallback(event);
            break;
        }
        }
    });
```

这里做了一个非常重要的转换：

```text
GLFW_PRESS
    ↓
MouseButtonPressedEvent
    ↓
Application::OnEvent()
```

平台库的事件不应该直接暴露给玩法系统。

## 2.12 Event：引擎自己的事件对象

源码位置：

```text
Wheatear/src/Wheatear/Events/Event.h
```

所有事件继承自：

```cpp
class Event
{
public:
    virtual ~Event() = default;

    virtual EventType GetEventType() const = 0;
    virtual const char* GetName() const = 0;
    virtual int GetCategoryFlags() const = 0;
    virtual Scope<Event> Clone() const = 0;

    bool Handled() const;
    void SetHandled(bool handled = true);
    void Consume();
};
```

### 2.12.1 事件类型和分类

```cpp
enum class EventType
{
    None = 0,
    WindowClose,
    WindowResize,
    WindowFocus,
    WindowLostFocus,
    WindowMoved,
    KeyPressed,
    KeyReleased,
    KeyTyped,
    MouseButtonPressed,
    MouseButtonReleased,
    MouseMoved,
    MouseScrolled
};
```

分类使用位标志：

```cpp
enum EventCategory
{
    None = 0,
    EventCategoryApplication = BIT(0),
    EventCategoryInput = BIT(1),
    EventCategoryKeyboard = BIT(2),
    EventCategoryMouse = BIT(3),
    EventCategoryMouseButton = BIT(4),
};
```

因此可以判断：

```cpp
if (event.IsInCategory(EventCategoryMouse))
{
    ...
}
```

鼠标移动事件：

```cpp
EVENT_CLASS_TYPE(MouseMoved)
EVENT_CLASS_CATEGORY(
    EventCategoryMouse | EventCategoryInput)
```

窗口调整大小事件：

```cpp
EVENT_CLASS_TYPE(WindowResize)
EVENT_CLASS_CATEGORY(EventCategoryApplication)
```

类型和分类是两个不同概念：

```text
EventType：
    这到底是哪一种具体事件？

EventCategory：
    它属于窗口、鼠标、键盘还是输入？
```

### 2.12.2 为什么事件要有 `Clone()`

Window 回调中的事件通常是栈对象：

```cpp
MouseButtonPressedEvent event(button);
data.EventCallback(event);
```

回调结束后，`event` 生命周期就结束了。

但 Application 可能要把它放入队列：

```cpp
void Application::OnEvent(Event& e)
{
    m_EventBus.Queue(e);
}
```

EventBus 不能保存原始引用，因此：

```cpp
void EventBus::Queue(const Event& event)
{
    m_QueuedEvents.push_back(event.Clone());
}
```

每种具体事件通过宏生成自己的复制逻辑：

```cpp
virtual Scope<Event> Clone() const override
{
    return CreateScope<MouseButtonPressedEvent>(*this);
}
```

所以队列里保存的是独立副本，不会引用已经失效的 GLFW 回调局部变量。

## 2.13 EventBus：事件的集中分发器

源码位置：

```text
Wheatear/src/Wheatear/Events/EventBus.h
Wheatear/src/Wheatear/Events/EventBus.cpp
```

EventBus 提供：

```cpp
EventSubscription Subscribe(
    EventType type,
    HandlerFn handler,
    int priority = 0);

EventSubscription SubscribeAll(
    HandlerFn handler,
    int priority = 0);

void Dispatch(Event& event);
void Queue(const Event& event);
void Flush(size_t maxEvents = 1024);
```

## 2.14 具体例子：Sandbox 里鼠标点击 UI 按钮

现在追踪一个很常见的结果：

> 玩家在 Sandbox 里点击一个按钮，按钮最终执行了 `scene:assets/scenes/Hub.wt`。

这一次点击会经过多个层。

### 第一步：用户按下鼠标

用户在窗口中点击鼠标左键。

在当前主循环中：

```cpp
glfwPollEvents();
```

GLFW 调用：

```cpp
glfwSetMouseButtonCallback(...)
```

回调构造：

```cpp
MouseButtonPressedEvent event(button);
```

然后调用 Window 保存的回调：

```cpp
data.EventCallback(event);
```

这个回调是在 Application 构造时设置的：

```cpp
m_Window->SetEventCallback(
    BIND_EVENT_FN(OnEvent));
```

`BIND_EVENT_FN(OnEvent)` 展开后，本质类似：

```cpp
std::bind(
    &Application::OnEvent,
    this,
    std::placeholders::_1);
```

### 第二步：Application 把事件放入队列

```cpp
void Application::OnEvent(Event& e)
{
    m_EventBus.Queue(e);
}
```

EventBus 复制它：

```cpp
m_QueuedEvents.push_back(event.Clone());
```

此时调用栈已经回到 GLFW 回调，但 EventBus 里保存了一份安全副本。

### 第三步：主循环统一 Flush

`glfwPollEvents()` 返回后，主循环马上执行：

```cpp
FlushEventQueue();
```

EventBus：

```cpp
while (!m_QueuedEvents.empty()
    && processedEvents < maxEvents)
{
    Scope<Event> event =
        std::move(m_QueuedEvents.front());

    m_QueuedEvents.pop_front();
    Dispatch(*event);
    processedEvents++;
}
```

注意：事件虽然是“排队”，但通常会在同一轮主循环里、进入 `OnUpdate()` 之前被 Flush。它不是必然要等到下一帧。

### 第四步：EventBus 确定处理顺序

Application 注册窗口事件时使用：

```cpp
EventPriority::Application
```

其值是：

```cpp
Application = 9000
```

Layer 路由使用：

```cpp
EventPriority::Layer = 1000
```

并且后加入的 Layer 优先级更高：

```cpp
int priority = EventPriority::Layer;

for (Layer* layer : m_LayerStack)
{
    m_EventBus.SubscribeAll(..., priority++);
}
```

如果 LayerStack 是：

```text
[RuntimeSceneLayer]
[ImGuiLayer]
```

事件顺序大致是：

```text
Application handler: 9000
ImGuiLayer:          1001
RuntimeSceneLayer:   1000
```

Application 先处理窗口关闭和窗口大小变化，ImGui 作为 Overlay 再处理输入捕获，最后才轮到游戏 Layer。

### 第五步：进入 `RuntimeSceneLayer::OnEvent`

Sandbox 的运行时 Layer：

```cpp
void RuntimeSceneLayer::OnEvent(Event& event)
{
    EventDispatcher dispatcher(event);

    dispatcher.Dispatch<MouseButtonPressedEvent>(
        [this](MouseButtonPressedEvent& e)
        {
            return OnMouseButtonPressed(e);
        });

    dispatcher.Dispatch<MouseButtonReleasedEvent>(
        [this](MouseButtonReleasedEvent& e)
        {
            return OnMouseButtonReleased(e);
        });

    dispatcher.Dispatch<MouseScrolledEvent>(
        [this](MouseScrolledEvent& e)
        {
            return OnMouseScrolled(e);
        });
}
```

这里出现了两个“事件层次”：

```text
EventBus：
    在整个 Application 中把事件送到各个 Layer

EventDispatcher：
    在某个 Layer 内判断事件具体类型并调用成员函数
```

`RuntimeSceneLayer` 里的局部 Dispatcher 判断成功后，调用：

```cpp
UIInputSystem::OnMousePressed(
    m_ActiveScene.get());
```

而 `UIInputSystem::OnMousePressed()` 主要设置：

```cpp
s_MouseWasPressed = true;
s_DragStartResolved = false;
```

它没有马上执行按钮命令，因为按下和释放是两个不同阶段。

### 第六步：鼠标释放时确认点击

用户松开鼠标后，同样会经历：

```text
GLFW_RELEASE
    ↓
MouseButtonReleasedEvent
    ↓
Application::OnEvent
    ↓
EventBus::Queue
    ↓
EventBus::Flush
    ↓
RuntimeSceneLayer::OnEvent
    ↓
UIInputSystem::OnMouseReleased
```

`UIInputSystem::OnMouseReleased()` 会找出当前悬停的按钮：

```cpp
auto buttonView =
    scene->GetRegistry().view<
        UIWidgetComponent,
        UIButtonComponent>();
```

然后从所有按钮中选出可接收指针、处于 Hover 状态且排序最高的按钮：

```cpp
if (!releaseConsumedBySkillTree
    && WidgetCanReceivePointer(layout, e)
    && button.IsHovered)
{
    ...
}
```

最后：

```cpp
if (clickedButton != entt::null)
    FireOnClick(scene, clickedButton);
```

### 第七步：按钮命令进入 CommandBus

`FireOnClick()`：

```cpp
void UIInputSystem::FireOnClick(Scene* scene, entt::entity e)
{
    Entity entity{ e, scene };

    if (!entity.HasComponent<UIButtonComponent>())
        return;

    auto& button =
        entity.GetComponent<UIButtonComponent>();

    if (button.OnClickFunction.empty())
        return;

    if (CommandBus::Execute(
            scene,
            button.OnClickFunction).Handled)
        return;
}
```

如果按钮的字段是：

```text
OnClickFunction =
scene:assets/scenes/Hub.wt
```

那么：

```text
按钮点击
    → CommandBus::Execute(...)
    → SceneTransitionService::RequestLoadScene(...)
    → RuntimeSceneLayer::ConsumeSceneTransitionRequests()
    → RuntimeSceneLayer::LoadScene(...)
```

这就是“点击按钮切换场景”的完整 Core 到 Runtime 链路。

Core 层本身不知道 `Hub.wt` 是什么，也不知道按钮属于哪个玩法；它只负责让鼠标事件安全地到达运行时 Layer。

## 2.15 EventBus 的优先级和 Consume

EventBus 的优先级常量：

```cpp
namespace EventPriority
{
    constexpr int Critical = 10000;
    constexpr int Application = 9000;
    constexpr int UI = 8000;
    constexpr int Overlay = 6000;
    constexpr int Layer = 1000;
    constexpr int Low = -1000;
}
```

注册时按：

```text
Priority 降序
Order 升序
```

排列。

### 2.15.1 为什么 ImGui 要拦截鼠标

假设用户正在编辑器 Inspector 的一个文本框里输入内容，同时鼠标也位于场景视口上方。

如果 ImGui 不拦截鼠标：

```text
用户点击 Inspector
    ↓
ImGui 收到
    ↓
EditorLayer2D 也收到
    ↓
场景中的实体可能被选中或拖动
```

这会让编辑器非常难用。

`ImGuiLayer::OnEvent()`：

```cpp
if (m_BlockEvents)
{
    ImGuiIO& io = ImGui::GetIO();

    e.SetHandled(
        e.Handled()
        || (e.IsInCategory(EventCategoryMouse)
            && io.WantCaptureMouse));

    e.SetHandled(
        e.Handled()
        || (e.IsInCategory(EventCategoryKeyboard)
            && io.WantCaptureKeyboard));
}
```

当 ImGui 判断自己要捕获输入时，就把事件标记为 Handled。

EventBus 分发循环：

```cpp
for (const HandlerCall& call : calls)
{
    if (event.Handled())
        break;

    HandlerEntry* entry =
        FindHandler(call.Type, call.ID);

    if (!entry || !entry->Active)
        continue;

    if (entry->Handler(event)
        == EventResult::Consume)
    {
        event.Consume();
    }
}
```

于是：

```text
ImGui 捕获鼠标
    ↓
event.Handled = true
    ↓
后面的编辑器视口不再处理
```

### 2.15.2 `EventDispatcher` 的 Handled 逻辑

局部 Dispatcher：

```cpp
template<typename T>
bool Dispatch(EventFn<T> func)
{
    if (!m_Event.Handled()
        && m_Event.GetEventType() == T::GetStaticType())
    {
        m_Event.m_Handled |=
            func(static_cast<T&>(m_Event));

        return true;
    }

    return false;
}
```

这里有两个条件：

```text
事件还没有被消费
    且
事件类型匹配
```

所以一个 Layer 可以：

```cpp
bool OnMouseScrolled(MouseScrolledEvent& e)
{
    if (!IsViewportHovered())
        return false;

    ZoomCamera(e.GetYOffset());
    return true;
}
```

返回 `true` 后，事件会被标记为已处理。

## 2.16 订阅的 RAII 生命周期

EventBus 返回：

```cpp
EventSubscription
```

Application 保存订阅：

```cpp
std::vector<EventSubscription>
    m_ApplicationEventSubscriptions;

std::vector<EventSubscription>
    m_LayerEventSubscriptions;
```

订阅对象不可复制，只能移动：

```cpp
EventSubscription(const EventSubscription&) = delete;
EventSubscription& operator=(
    const EventSubscription&) = delete;

EventSubscription(EventSubscription&& other) noexcept;
```

析构时自动：

```cpp
~EventSubscription()
{
    Unsubscribe();
}
```

### 2.16.1 为什么要这样做

假设 `EditorLayer2D` 被删除，但 EventBus 还保存着：

```cpp
[this](Event& event)
{
    layer->OnEvent(event);
}
```

如果订阅没有清理，下次事件到来时就会调用已经销毁对象的成员函数，形成悬空指针。

Application 在重新构建 Layer 路由前：

```cpp
m_LayerEventSubscriptions.clear();
```

清空订阅对象，RAII 自动取消旧的处理器，然后再为当前 LayerStack 建立新路由。

这也是为什么 `PostUpdateLayers()` 中要做：

```cpp
if (routeDirty)
    m_LayerEventSubscriptions.clear();
```

## 2.17 窗口事件的特殊处理

Application 自己订阅两个应用级事件：

```cpp
m_EventBus.Subscribe<WindowCloseEvent>(
    [this](WindowCloseEvent& e)
    {
        return OnWindowClose(e);
    },
    EventPriority::Application);

m_EventBus.Subscribe<WindowResizeEvent>(
    [this](WindowResizeEvent& e)
    {
        return OnWindowResize(e);
    },
    EventPriority::Application);
```

窗口关闭：

```cpp
bool Application::OnWindowClose(WindowCloseEvent&)
{
    m_Running = false;
    return true;
}
```

于是：

```text
用户点击窗口关闭
    ↓
WindowCloseEvent
    ↓
Application::OnWindowClose
    ↓
m_Running = false
    ↓
主循环结束
```

窗口调整大小：

```cpp
bool Application::OnWindowResize(WindowResizeEvent& e)
{
    if (e.GetWidth() == 0 || e.GetHeight() == 0)
    {
        m_Minimized = true;
        return false;
    }

    m_Minimized = false;
    Renderer::OnWindowResize(
        e.GetWidth(),
        e.GetHeight());

    return true;
}
```

这里做了两件事：

```text
窗口最小化：
    m_Minimized = true
    暂停 Layer 更新和 ImGui 绘制

窗口恢复或调整大小：
    更新 Renderer viewport
    继续正常运行
```

这就是 Core 层和 Renderer 层之间的一条合理边界：Application 接收窗口尺寸变化，Renderer 负责调整图形资源和视口。

## 2.18 主循环中的暂停条件

主循环仍然运行，但如果：

```cpp
m_Minimized == true
```

则跳过：

```cpp
Layer::OnUpdate()
Layer::OnImGuiRender()
```

但仍然会执行：

```cpp
InputBindingService::EndFrame();
Input::EndFrame();
m_Window->OnUpdate();
PostUpdateLayers();
```

这样做的目的通常是：

- 最小化窗口时不浪费大量游戏更新和绘制。
- 保留窗口和输入状态的基本生命周期。
- 窗口恢复后可以继续工作。

## 2.19 Application 的销毁顺序

析构函数：

```cpp
Application::~Application()
{
    m_LayerEventSubscriptions.clear();
    m_ApplicationEventSubscriptions.clear();
    m_LayerStack.Clear();
    m_EventBus.Clear();

    Renderer::Shutdown();
    AudioEngine::Shutdown();

    s_Instance = nullptr;
}
```

顺序很重要：

```text
1. 取消事件订阅
2. 清理 Layer
3. 清理 EventBus
4. 关闭 Renderer
5. 关闭 Audio
6. 清空 Application 单例
```

为什么不是先关 Renderer 再销毁 Layer？

因为 Layer 的 `OnDetach()` 可能仍然需要释放或访问引擎服务。比如 `ImGuiLayer::OnDetach()` 需要关闭 ImGui OpenGL backend：

```cpp
ImGui_ImplOpenGL3_Shutdown();
ImGui_ImplGlfw_Shutdown();
ImGui::DestroyContext();
```

所以应该先让 Layer 有机会正常退出，再关闭底层服务。

## 2.20 Core 层中的几个基础工具

### `Scope` 和 `Ref`

源码位置：

```text
Wheatear/src/Wheatear/Core/Core.h
```

```cpp
template<typename T>
using Scope = std::unique_ptr<T>;

template<typename T, typename ... Args>
constexpr Scope<T> CreateScope(Args&& ... args)
{
    return std::make_unique<T>(
        std::forward<Args>(args)...);
}

template<typename T>
using Ref = std::shared_ptr<T>;

template<typename T, typename ... Args>
constexpr Ref<T> CreateRef(Args&& ... args)
{
    return std::make_shared<T>(
        std::forward<Args>(args)...);
}
```

引擎代码于是统一写：

```cpp
Scope<GraphicsContext> m_Context;
Ref<Scene> scene;
```

它们本质仍然是 C++ 标准智能指针，只是使用了项目自己的命名。

### `WT_CORE_ASSERT`

```cpp
WT_CORE_ASSERT(!s_Instance,
    "Application already exists!");
```

Debug 配置下，失败会记录日志并触发断点；没有启用断言时宏可能被编译为空。

断言适合表达：

```text
这个条件是架构不变量
```

例如 Application 不应该有两个实例。

### `WT_BIND_EVENT_FN`

```cpp
#define WT_BIND_EVENT_FN(fn) \
    std::bind(&fn, this, std::placeholders::_1)
```

使用：

```cpp
dispatcher.Dispatch<MouseScrolledEvent>(
    WT_BIND_EVENT_FN(EditorCamera::OnMouseScroll));
```

它把当前对象的成员函数绑定成可以被事件系统调用的可调用对象。

## 2.21 Core 层最重要的设计原则

### 原则一：平台细节向下隔离

```text
游戏代码
    不直接调用 glfwCreateWindow

Window 接口
    提供 GetWidth / SetVSync / OnUpdate

WindowsWindow
    具体调用 GLFW
```

### 原则二：Application 只做调度，不做玩法

Application 知道：

```text
什么时候更新 Layer
什么时候分发事件
什么时候交换缓冲
```

Application 不应该知道：

```text
玩家技能伤害是多少
VN 当前是哪句台词
横板 Boss 什么时候进入狂暴
```

### 原则三：功能通过 Layer 插拔

新增一个工具或运行模式时，可以写：

```cpp
class MyLayer : public Layer
{
public:
    void OnAttach() override;
    void OnUpdate(Timestep ts) override;
    void OnImGuiRender() override;
    void OnEvent(Event& event) override;
};
```

Application 无需增加：

```cpp
MyLayer* m_MyLayer;
```

只需：

```cpp
PushLayer(std::make_unique<MyLayer>());
```

### 原则四：事件与功能解耦

Window 不应该知道：

```text
点击后是打开菜单、选中实体，还是让角色攻击
```

Window 只产生：

```cpp
MouseButtonPressedEvent
```

具体 Layer 和系统自己决定是否关心。

## 2.22 这套 Core 设计的优点

### 优点一：Editor 和 Sandbox 共享主循环

编辑器和 Sandbox 都继承 Application：

```cpp
class WheatearEditor : public Application
class WheatearSandbox : public Wheatear::Application
```

所以二者共享：

```text
窗口创建
事件处理
Layer 管理
Renderer 初始化
Audio 初始化
主循环
```

只在应用构造函数中添加不同 Layer：

```text
Editor:
    ModeSelectLayer
    EditorLayer2D / EditorLayer3D

Sandbox:
    RuntimeSceneLayer
```

### 优点二：功能模块生命周期统一

任何 Layer 都有同样的：

```text
Attach
Update
ImGuiRender
Event
Detach
```

新人只要理解一套接口，就能读懂编辑器和运行时的功能入口。

### 优点三：事件处理顺序可控

通过：

```cpp
EventPriority
EventSubscription
event.Consume()
```

可以表达：

```text
Application 先处理窗口关闭
ImGui 优先拦截编辑器输入
具体 Layer 最后处理剩余事件
```

### 优点四：适合小型 2D 游戏

对于单窗口 2D 游戏，下面这些全局对象通常确实只有一份：

```text
Application
Window
Renderer
AudioEngine
Input
EventBus
```

单例和静态服务减少了大量样板代码，让你可以把精力放到：

```text
场景
动画
UI
战斗
剧情
资源
```

## 2.23 这套 Core 设计的代价

### 代价一：Application 单例隐藏依赖

看到：

```cpp
Application::Get().GetWindow()
```

表面上当前函数没有参数依赖，但实际上它依赖全局 Application 已经创建。

### 代价二：事件队列不是类型安全到极致

EventBus 对外提供类型化的：

```cpp
Subscribe<T>()
```

但内部仍然存储：

```cpp
Event&
std::function<EventResult(Event&)>
```

事件在运行时通过 `EventType` 判断。它比到处传 `void*` 安全很多，但仍然有动态分发成本。

### 代价三：Layer 责任可能越来越大

如果所有功能都简单地塞进 Layer：

```text
EditorLayerBase
    场景
    UI
    相机
    输入
    打包
    调试
    资源
```

Layer 可能变成“上帝对象”。

Wheatear 已经通过拆分：

```text
EditorLayerBaseLifecycle.cpp
EditorLayerBaseScene.cpp
EditorLayerBaseUI.cpp
EditorLayerBaseUI_Viewport.cpp
```

减轻单个编译单元，但职责边界仍然需要持续维护。

### 代价四：主循环是集中式调度

集中式主循环很好理解，但如果未来加入：

```text
多线程资源加载
并行系统
后台网络
多个窗口
```

就需要继续设计线程安全的事件队列和任务调度机制。

## 2.24 和常见引擎的对比

### 和 Unity 的对比

Unity 的主循环由引擎内部管理，开发者通常通过：

```text
Awake
OnEnable
Start
Update
LateUpdate
OnGUI
```

等生命周期进入系统。

Wheatear 的 `Layer` 更接近“应用级功能容器”：

```text
Unity:
    引擎管理大量对象脚本生命周期

Wheatear:
    Application 直接遍历 Layer
    Layer 再驱动自己的 Scene / System
```

Wheatear 的代码更少、更容易从入口追踪；Unity 的运行时功能更成熟、更细化。

### 和 Unreal Engine 的对比

Unreal 使用更庞大的模块、World、Actor、Subsystem、Slate/UMG 等框架。

Wheatear 的 Core 层只解决：

```text
单窗口
单 Application
Layer 调度
事件分发
```

这不是功能缺陷，而是项目目标不同。对于个人 2D 游戏，引擎骨架保持小而清楚往往更利于学习和维护。

### 和 Godot 的对比

Godot 以 SceneTree 和 Node 生命周期为中心。Wheatear 则是：

```text
Application
    → LayerStack
        → Scene
            → ECS Systems
```

Godot 的 Node 更像场景中的行为对象；Wheatear 的 Layer 更像应用状态或工具模式容器。

## 2.25 面试回答模板

### 问题：你的引擎主循环是怎样的？

可以这样回答：

```text
Wheatear 的主循环集中在 Application::Run()。
每帧先调用 glfwPollEvents()，让 GLFW 产生平台事件；
窗口回调把事件转换成引擎自己的 Event，并通过 EventBus 排队。
随后 FlushEventQueue() 按优先级分发事件，再计算 Timestep，
遍历 LayerStack 执行每个 Layer 的 OnUpdate。
之后开始 ImGui 帧，调用所有 Layer 的 OnImGuiRender，
最后提交输入状态、交换窗口缓冲，并在帧末统一提交 Layer 的增删。
```

### 问题：为什么要用 Layer？

```text
Layer 是统一生命周期接口，可以把编辑器、运行时场景、ImGui、
调试工具等功能模块插入同一个 Application。
Application 不需要知道每个功能的细节，只负责按顺序调用
OnAttach、OnUpdate、OnImGuiRender、OnEvent 和 OnDetach。
```

### 问题：为什么不在遍历时直接删除 Layer？

```text
LayerStack 底层是 vector，遍历过程中增删可能导致迭代器和元素地址失效。
所以 PushLayer / PopLayer 只记录请求，在帧末 PostUpdateLayers()
统一提交，并重新构建事件订阅路由。
```

### 问题：一次鼠标点击如何到达游戏逻辑？

```text
GLFW 鼠标回调
 → MouseButtonPressedEvent
 → Application::OnEvent
 → EventBus::Queue
 → EventBus::Flush
 → Layer::OnEvent
 → EventDispatcher
 → RuntimeSceneLayer
 → UIInputSystem
 → CommandBus
 → 场景切换或其他游戏行为
```

## 2.26 新人阅读练习

建议打开 IDE，按下面顺序自己跳转。

### 练习一：追踪编辑器启动

从：

```cpp
main()
```

追到：

```cpp
CreateApplication()
WheatearEditor::WheatearEditor()
Application::Application()
Application::Run()
PostUpdateLayers()
ModeSelectLayer::OnAttach()
```

自己回答：

```text
ModeSelectLayer 是在哪个时刻真正进入 LayerStack 的？
```

### 练习二：追踪切换到 2D 编辑器

从：

```cpp
ModeSelectLayer::LaunchEditor2D()
```

追到：

```cpp
Application::PushLayer()
Application::PopLayer()
Application::PostUpdateLayers()
EditorLayer2D::OnAttach()
```

自己画出调用前后 LayerStack 的变化。

### 练习三：追踪窗口关闭

从：

```cpp
glfwSetWindowCloseCallback()
```

追到：

```cpp
WindowCloseEvent
Application::OnEvent()
EventBus::Queue()
EventBus::Flush()
Application::OnWindowClose()
m_Running = false
```

自己解释：

```text
为什么关闭窗口后不是直接调用 Application::Close()，
而是先变成一个 WindowCloseEvent？
```

### 练习四：追踪 UI 点击

从：

```cpp
RuntimeSceneLayer::OnMousePressed()
```

继续追到：

```cpp
UIInputSystem::OnMousePressed()
UIInputSystem::OnMouseReleased()
UIInputSystem::FireOnClick()
CommandBus::Execute()
```

自己回答：

```text
为什么按钮不是鼠标按下时立即执行命令，
而是在鼠标释放时确认点击？
```

## 2.27 本章小结

本章理解了 Core 层的完整职责：

```text
EntryPoint：
    提供统一程序入口

Application：
    初始化引擎服务并驱动主循环

Window：
    隔离窗口和平台实现

Layer：
    提供可插拔功能生命周期

LayerStack：
    管理 Layer 顺序、所有权和 Overlay

Event：
    表达平台和输入事件

EventBus：
    负责事件排队、优先级分发、消费和订阅生命周期

Timestep：
    把帧间隔传递给系统
```

最重要的一条真实链路是：

```text
用户点击按钮
    → GLFW
    → Window 回调
    → Event
    → Application
    → EventBus
    → Layer
    → UIInputSystem
    → CommandBus
    → 游戏结果
```

下一章进入 **Event 与 Input 层**，重点研究：

```text
原始按键查询和事件有什么区别？
Input::IsKeyPressed() 如何判断边沿？
InputBindingService 为什么要引入 actionId？
物理键、鼠标键、动作命令如何统一到同一套输入逻辑？
```
