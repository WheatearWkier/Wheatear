# 第 1 编 02：窗口与 OpenGL 上下文

> 本章所属层级：**第三方与平台层**
>
> 本章目标：理解 Wheatear 是怎么把一个桌面窗口创建出来的，OpenGL 上下文是怎么绑定到这个窗口上的，为什么渲染器必须在上下文之后初始化，以及 VSync、全屏、尺寸变化和系统事件是怎么落到这一层的。

如果说上一章讲的是“这些第三方库和平台依赖分别负责什么”，  
那这一章讲的是：

```text
窗口是怎么真正被创建出来的？
OpenGL 为什么能在这个窗口里画？
为什么窗口层和图形上下文要拆开？
窗口大小变化以后，渲染面该怎么跟着变？
```

这章很重要，因为它处在最底层的“入口边界”上。  
引擎上层的所有事情，最终都得落到这个边界上来：

- 程序要先有窗口
- 窗口要先有图形上下文
- 上下文要先被激活
- 渲染器才能初始化
- 主循环才能开始

所以你可以把这一层记成一句话：

```text
Window 负责“有个能显示的桌面表面”，
GraphicsContext 负责“让这个表面能接收 OpenGL 命令”。
```

---

## 1.1 本章要读的源码

```text
Wheatear/src/Wheatear/Core/Window.h
Wheatear/src/Wheatear/Renderer/GraphicsContext.h

Wheatear/src/Platform/Windows/WindowsWindow.h
Wheatear/src/Platform/Windows/WindowsWindow.cpp

Wheatear/src/Platform/OpenGL/OpenGLContext.h
Wheatear/src/Platform/OpenGL/OpenGLContext.cpp

Wheatear/src/Wheatear/Core/Application.h
Wheatear/src/Wheatear/Core/Application.cpp
Wheatear/src/Wheatear/Renderer/Renderer.h
Wheatear/src/Wheatear/Renderer/Renderer.cpp

Wheatear/src/Wheatear/ImGui/ImGuiLayer.cpp
Wheatear/src/Platform/Windows/WindowsPlatformUtils.cpp
Wheatear/src/Wheatear/Utils/PlatformUtils.h
```

如果你只想先抓主线，就先看这五个：

```text
Window.h
WindowsWindow.cpp
OpenGLContext.cpp
Application.cpp
Renderer.cpp
```

它们分别回答：

```text
窗口接口长什么样？
Windows 窗口怎么创建？
OpenGL 上下文怎么初始化？
应用主循环怎么跑？
渲染器为什么必须在窗口之后初始化？
```

---

## 1.2 这一层在架构中的位置

```text
第三方与平台层
    GLFW / OpenGL / ImGui / glad / Win32
        ↓
核心平台层
    Application / Window / EventBus / LayerStack
        ↓
引擎服务层
    Renderer / Input / Audio / UI / AssetPath
        ↓
Scene / Gameplay / Editor
```

这一层的职责不是“实现玩法”，也不是“做编辑器 UI”，  
它只解决最底层的三个问题：

1. 程序有没有一个真正的桌面窗口
2. 这个窗口能不能绑定图形上下文
3. 主循环能不能稳定地拿到事件和帧呈现

如果这一层不稳，上面所有层都会跟着不稳。  
所以这里的代码虽然看起来“基础”，实际上是全局地基。

---

## 1.3 `Window`：窗口层的统一接口

### 1.3.1 为什么要先抽象 `Window`

`Window` 不是 GLFW 本身，也不是 Win32 本身。  
它是引擎对“桌面窗口”这件事的统一接口。

`Window.h` 里最重要的东西有两部分：

- `WindowProps`
- `Window` 抽象类

`WindowProps` 负责描述启动时的参数：

- `Title`
- `Width`
- `Height`
- `IconPath`

默认值也很清楚：

- 标题：`Wheatear`
- 宽高：`1920 x 1080`
- 图标：`Resources/Icons/icon.png`

这说明窗口不是“创建完再说”，而是从一开始就带着宿主配置的。

### 1.3.2 `Window` 接口提供什么

`Window` 的职责很克制，只做这些：

- `OnUpdate()`
- `GetWidth() / GetHeight()`
- `SetEventCallback()`
- `SetVSync() / IsVSync()`
- `SetFullscreen() / IsFullscreen()`
- `GetNativeWindow()`

你会发现它没有提供：

- 绘制三角形
- 创建纹理
- 发送 draw call
- 管场景

这很重要。  
窗口层不碰渲染逻辑，它只提供“表面”和“输入源”。

### 1.3.3 `Window::Create()` 是工厂，不是实例

`Window.h` 里的 `static Window* Create(...)` 是一个工厂入口。  
在当前项目里，它最终会返回 `WindowsWindow`。

这意味着：

```text
上层只认 Window
底层决定它是 WindowsWindow
```

这样做的好处是：

- 上层代码不依赖具体平台类名
- 后面要换平台实现时，改动面更小
- 业务层看到的是统一接口，不是 GLFW 细节

---

## 1.4 `WindowsWindow`：真正把窗口造出来

### 1.4.1 `WindowsWindow` 解决什么

`WindowsWindow` 是 `Window` 在 Windows 平台上的实现。  
它干的事不是“画画”，而是：

- 调用 GLFW 创建原生窗口
- 绑定 OpenGL 上下文
- 设置图标
- 注册系统事件回调
- 管 VSync 和全屏
- 在窗口更新时交换缓冲

### 1.4.2 `glfwInit()` 为什么只做一次

`WindowsWindow.cpp` 里有一个 `s_GLFWWindowCount`。  
它的意义是：

- 第一个窗口创建时 `glfwInit()`
- 最后一个窗口销毁时 `glfwTerminate()`

这样可以避免：

- 重复初始化 GLFW
- 过早终止 GLFW

这类“全局库生命周期”问题，平台层必须自己兜住。

### 1.4.3 `Init()` 的完整顺序

`WindowsWindow::Init()` 的顺序很关键，大概是：

```text
保存 WindowProps
  -> glfwInit()
  -> glfwCreateWindow()
  -> 加载窗口图标
  -> 创建 OpenGLContext
  -> OpenGLContext::Init()
  -> glfwSetWindowUserPointer()
  -> SetVSync(true)
  -> SetupCallbacks()
```

这条顺序不能乱。

因为：

- 先有 GLFW 窗口，才有原生句柄
- 先有原生句柄，才有 OpenGL 上下文
- 先有上下文，渲染器后面才能初始化
- 先设置 user pointer，回调里才能把系统事件转回引擎数据

### 1.4.4 图标是怎么来的

窗口图标不是写死在代码里的像素数组。  
它会通过 `AssetPath::ResolveResource(props.IconPath)` 找到资源路径，然后用 `stbi_load` 读成图像，再交给 GLFW 设置窗口图标。

这说明平台层虽然低，但它仍然走引擎统一的资源路径规则。  
也就是说，资源系统的最底层路径语义，在这里就已经开始起作用了。

### 1.4.5 `SetupCallbacks()`：窗口事件怎么变成引擎事件

这是这一节最该看的地方。

`WindowsWindow` 会把 GLFW 的回调注册到：

- 窗口尺寸变化
- 窗口关闭
- 键盘按下 / 释放 / 重复
- 字符输入
- 鼠标按键
- 鼠标滚轮
- 鼠标移动

每个回调做的事都很像：

```text
GLFW 回调
  -> 构造引擎事件对象
  -> 调用 m_Data.EventCallback(event)
```

也就是说，这一层不是直接处理“业务逻辑”，  
它只是把操作系统/GLFW 的消息翻译成引擎自己的事件对象。

这很像一层翻译器。

### 1.4.6 为什么要用 `WindowUserPointer`

GLFW 的回调是 C 风格的函数指针。  
它不知道你的 `WindowsWindow` 实例在哪，所以 `WindowsWindow` 会把 `WindowData` 塞进 `glfwSetWindowUserPointer()`。

这样回调里就能：

- 找回当前窗口的数据
- 更新宽高状态
- 把事件转发给引擎

这就是平台层常见的“C 回调 + 对象实例”桥接方式。

### 1.4.7 `OnUpdate()` 为什么这么短

`WindowsWindow::OnUpdate()` 只有一件事：

```text
m_Context->SwapBuffers();
```

这不是偷懒，而是职责划分清楚。  
窗口层不负责主循环，不负责渲染管线，它只负责“这个窗口这一帧已经画完了，把后缓冲切到前台”。

### 1.4.8 从窗口回调到应用事件队列

这一层还有一个常被忽略的角色：把“系统消息”变成“引擎消息”的起点。

```text
GLFW 回调
  -> 构造 KeyPressedEvent / WindowResizeEvent / MouseMovedEvent
  -> m_Data.EventCallback(event)
  -> Application::OnEvent()
  -> EventBus::Queue(event)
  -> 后续在 FlushEventQueue() 里统一分发
```

这样做的好处是：

- 窗口层只负责把消息翻译出来
- 应用层决定什么时候分发
- 主循环里能把输入、窗口和逻辑更新收口到同一帧语义里

如果你把这条链和上一节的 `SetupCallbacks()` 连起来看，就会发现：  
系统回调只是入口，真正的“引擎事件处理”是在 `Application` 这一层开始的。  
这也是为什么窗口层不能自己直接做玩法逻辑。

---

## 1.5 `GraphicsContext` 和 `OpenGLContext`

### 1.5.1 为什么要把上下文单独抽一层

很多人会把“窗口”和“OpenGL 上下文”混成一件事。  
Wheatear 没这么做。

它把两件事拆开：

- `Window`：系统窗口表面
- `GraphicsContext`：图形 API 的上下文生命周期

这样拆的好处是：

- 窗口层和图形 API 分离
- 后面换渲染后端时更方便
- 上下文初始化顺序更清楚

### 1.5.2 `GraphicsContext` 只保留两个动作

`GraphicsContext.h` 里只有：

- `Init()`
- `SwapBuffers()`

这说明它不是一个“大而全的渲染接口”，  
而只是“把图形 API 绑定到窗口，并负责翻页”的最小合同。

### 1.5.3 `OpenGLContext::Init()` 做了什么

`OpenGLContext.cpp` 里，初始化过程非常典型：

```text
glfwMakeContextCurrent(window)
  -> gladLoadGLLoader(glfwGetProcAddress)
  -> 打印 GL Vendor / Renderer / Version
```

这一步的核心意义是：

1. 把这个 GLFW 窗口变成当前 OpenGL context
2. 把所有 OpenGL 函数指针加载出来
3. 确认当前驱动和渲染器信息

如果没有这一步，后面的 `glClear`、`glViewport`、`glDraw*` 都没有可靠入口。

### 1.5.4 为什么要先 `glfwMakeContextCurrent`

因为 OpenGL 的很多调用都依赖“当前上下文”。  
如果当前上下文没设好，glad 虽然加载了函数指针，但那些调用仍然没有正确的对象状态可用。

所以顺序一定是：

```text
先把 context 绑到窗口
再加载函数入口
最后才让 Renderer 开始初始化
```

### 1.5.5 `SwapBuffers()` 为什么放在 context 里

`OpenGLContext::SwapBuffers()` 最终调用的是 `glfwSwapBuffers(m_WindowHandle)`。

它属于 context，不属于 window 的原因是：

- 交换前后缓冲是图形 API 的动作
- Window 只提供宿主表面
- context 才知道“这一帧的图形命令已经提交完了，应该翻页了”

这个拆法很干净。  
后面你看 `Renderer`、`RenderCommand`、`Framebuffer` 时，也能更容易理解它们和平台层的边界。

---

## 1.6 为什么窗口一定要先于渲染器

这一点非常重要，面试时也很容易被问。

`Application::Application()` 里的顺序是：

```text
创建 Window
  -> 绑定 OnEvent
  -> Load UserSettings
  -> Renderer::Init()
  -> AudioEngine::Init()
  -> 创建 ImGuiLayer
```

而 `Window` 的创建内部又会完成：

```text
glfwCreateWindow
  -> OpenGLContext::Init
```

所以等到 `Renderer::Init()` 被调用时：

- OpenGL 上下文已经存在
- GL 函数已经加载
- 渲染器可以安全创建自己的资源

`Renderer::Init()` 本身也很直接：

```text
RenderCommand::Init()
Renderer2D::Init()
Renderer3D::Init()
```

这里的顺序也很讲究：

- 先让底层渲染命令表就位
- 再初始化 2D 渲染
- 再初始化 3D 渲染

也就是说，**窗口和 context 是渲染器的前置条件**，不是后置结果。

---

## 1.7 窗口尺寸、全屏和 VSync

### 1.7.1 `SetVSync()`

`WindowsWindow::SetVSync(bool enabled)` 最终调用的是：

```text
glfwSwapInterval(enabled ? 1 : 0)
```

这就是 VSync 的入口。  
打开后，缓冲交换会跟显示器刷新节奏对齐；关闭后，交换速度不再受刷新率约束。

### 1.7.2 `SetFullscreen()`

全屏切换时，窗口会先记住：

- 当前窗口位置
- 当前窗口宽高

然后通过 `glfwSetWindowMonitor()` 切到主显示器。  
退出全屏时再把原来的窗口位置和尺寸恢复回来。

这说明全屏不是“把窗口拉满”这么简单，  
而是要维护一份窗口化状态，方便退出时原样还原。

### 1.7.3 窗口尺寸变化

窗口大小变化是从 GLFW 回调开始的。  
`glfwSetWindowSizeCallback` 会更新 `WindowData.Width / Height`，再发出 `WindowResizeEvent`。

后面的链路在 `Application::Run()` 里继续：

```text
glfwPollEvents()
  -> OnEvent()
  -> EventBus::Queue()
  -> FlushEventQueue()
  -> Application::OnWindowResize()
  -> Renderer::OnWindowResize()
  -> RenderCommand::SetViewport()
```

这条链路很关键，因为它说明：

- 窗口层只负责“报尺寸变了”
- 应用层决定“怎么响应”
- 渲染层决定“把 viewport 改到什么尺寸”

这就是分层的价值。

---

## 1.8 一个完整的启动例子

把窗口、上下文、渲染器和主循环串起来，大概就是这样：

```text
main()
  -> CreateApplication()
  -> Application(spec)
  -> Window::Create()
  -> WindowsWindow::Init()
  -> glfwInit()
  -> glfwCreateWindow()
  -> OpenGLContext::Init()
  -> gladLoadGLLoader()
  -> Renderer::Init()
  -> RenderCommand::Init()
  -> Renderer2D::Init()
  -> Renderer3D::Init()
  -> Application::Run()
  -> glfwPollEvents()
  -> FlushEventQueue()
  -> Layer::OnUpdate()
  -> ImGui::Begin / End
  -> Window::OnUpdate()
  -> OpenGLContext::SwapBuffers()
```

这条链路说明了一件事：

```text
窗口不是目的，
上下文也不是目的，
它们只是为了让后面的渲染和交互能稳定运行。
```

### 1.8.1 启动时最容易漏掉的两个先后关系

这条链里有两个先后顺序最值得记住：

1. 先 `glfwCreateWindow()` 和 `OpenGLContext::Init()`，再让 `Renderer::Init()`
2. 先把窗口回调和 `EventCallback` 设好，再进入 `glfwPollEvents()`

前者保证图形 API 有当前上下文，后者保证窗口消息不会丢。  
如果把它们调换，问题通常不是“报错一行”，而是后面所有渲染和输入都在莫名其妙地失效。

同理，`ImGuiLayer::OnAttach()` 也必须在窗口和上下文就位之后，因为它要拿原生句柄去初始化后端。

---

## 1.9 原生句柄先埋一个钩子

`Window` 里有一个 `GetNativeWindow()`。  
在这一章里你只需要先记住它的存在，不要急着把它和所有平台功能混在一起。

它存在的原因很朴素：

- 有些第三方库要原生窗口句柄
- 有些系统 API 只能吃原生窗口句柄
- ImGui 的 GLFW 后端也要它

例如：

- `ImGuiLayer` 会把它交给 `ImGui_ImplGlfw_InitForOpenGL`
- `WindowsPlatformUtils` 会把它转成 Win32 窗口，用来打开文件对话框

这一点下一章会展开讲。  
这里先记住：**Window 既是窗口，也是原生宿主句柄的出口**。

---

## 1.10 这一层的优点、坏处和常见误区

### 优点

- `Window` 和 `GraphicsContext` 分离，职责干净
- GLFW 只停留在平台层，上层不被污染
- OpenGL 上下文初始化顺序清晰
- VSync、全屏、图标、回调都能统一管理
- 窗口变化能顺着事件链传到渲染层

### 坏处

- 这一层看起来简单，但一旦顺序错了，后面全会崩
- 错把窗口当成渲染器，容易写出混乱代码
- 上下文、事件、输入、渲染命令这几个概念很容易混
- 只看 `WindowsWindow`，容易忽略 `Application` 才是真正驱动主循环的人

### 常见误区

- 以为 `Window::OnUpdate()` 是“更新窗口内容”，其实它主要是交换缓冲
- 以为 `Window` 和 `GraphicsContext` 是一回事，其实不是
- 以为 `Renderer::Init()` 可以随便放在任何地方，实际上必须等 context 就绪
- 以为全屏只是改一下大小，实际上还要保存窗口化状态
- 以为 resize 只影响窗口外观，实际上会直接影响 viewport

---

## 1.11 面试时怎么讲

如果面试官问你：“Wheatear 的窗口和 OpenGL 上下文是怎么设计的？”

你可以这样答：

```text
我把窗口和图形上下文拆成两层。
Window 负责系统窗口、尺寸、VSync、全屏、事件回调和原生句柄；
GraphicsContext 负责把 OpenGL context 绑定到窗口，并在每帧结束时交换缓冲。
在 Windows 实现里，WindowsWindow 先 glfwCreateWindow，再创建 OpenGLContext，调用 gladLoadGLLoader，
然后 Application 才能安全调用 Renderer::Init()。
```

如果面试官继续问：“为什么不把上下文塞进窗口里写死？”

你可以补一句：

```text
因为我想把平台宿主和图形 API 解耦。
窗口是平台表面，context 是渲染后端，拆开后后面如果要换实现，边界会清楚很多。
```

如果继续追问：“窗口尺寸变化怎么传到渲染器？”

你就说：

```text
GLFW 的尺寸回调先变成 WindowResizeEvent，应用层收到后再调用 Renderer::OnWindowResize，
最终通过 RenderCommand::SetViewport 更新 OpenGL viewport。
```

---

## 1.12 小结

这一章其实只讲了一件事：

```text
引擎要先有一个可信的桌面宿主，
然后才能把渲染、输入和界面真正跑起来。
```

Wheatear 在这一层做了三件事：

- 用 `Window` 抽象统一桌面窗口接口
- 用 `WindowsWindow` 负责 GLFW 窗口与回调
- 用 `OpenGLContext` 负责上下文绑定和缓冲交换

把这一章看懂以后，你再看后面的内容会顺很多。  
因为后面无论是：

- `RenderCommand`
- `RendererAPI`
- `ImGuiLayer`
- `Input`
- `PlatformUtils`

都要建立在这一层提供的窗口句柄和上下文之上。

下一章我们就接着写 `平台句柄与 SwapBuffer`。  
那一章会把 `GetNativeWindow()`、Win32 句柄、文件对话框和缓冲交换这条线真正讲透。
