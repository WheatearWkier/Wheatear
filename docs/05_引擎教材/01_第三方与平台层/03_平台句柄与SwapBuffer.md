# 第 1 编 03：平台句柄与 SwapBuffer

> 本章所属层级：**第三方与平台层**
>
> 本章目标：理解 `GetNativeWindow()` 为什么要存在，原生窗口句柄会被哪些平台功能和第三方库使用，`SwapBuffers()` 为什么属于图形上下文而不是窗口本身，以及一帧的最终呈现是怎么收口的。

如果说上一章讲的是：

```text
窗口怎么创建
OpenGL 上下文怎么绑定
```

那这一章讲的是：

```text
这个窗口句柄到底给谁用？
什么时候需要原生句柄？
为什么最后是 context 在 swap，而不是 window 在画？
```

这一层看起来很薄，实际上非常关键。  
它是平台和引擎之间的那条小口子：

- 上面想要一个抽象的 `Window`
- 下面偶尔又必须碰到原生平台句柄
- 最后再通过 `SwapBuffers()` 把一帧真正送到屏幕上

你可以把这一章记成一句话：

```text
GetNativeWindow 负责“把窗口交给需要原生句柄的地方”，
SwapBuffers 负责“把这一帧真正提交到屏幕上”。
```

---

## 1.1 本章要读的源码

```text
Wheatear/src/Wheatear/Core/Window.h
Wheatear/src/Platform/Windows/WindowsWindow.h
Wheatear/src/Platform/Windows/WindowsWindow.cpp

Wheatear/src/Wheatear/Renderer/GraphicsContext.h
Wheatear/src/Platform/OpenGL/OpenGLContext.h
Wheatear/src/Platform/OpenGL/OpenGLContext.cpp

Wheatear/src/Wheatear/ImGui/ImGuiLayer.cpp
Wheatear/src/Wheatear/Utils/PlatformUtils.h
Wheatear/src/Platform/Windows/WindowsPlatformUtils.cpp

Wheatear/src/Wheatear/Core/Application.cpp
```

如果你先抓主线，就先看这五个：

```text
Window.h
WindowsWindow.h
WindowsWindow.cpp
ImGuiLayer.cpp
WindowsPlatformUtils.cpp
```

它们分别回答：

```text
窗口接口为什么要暴露 native handle？
WindowsWindow 把什么对象交给上层？
ImGui 和文件对话框为什么都要这个句柄？
SwapBuffers 最终在哪里发生？
```

---

## 1.2 这一层在架构中的位置

```text
第三方与平台层
    GLFW / OpenGL / Win32 / ImGui backend
        ↓
核心平台层
    Application / Window / EventBus / LayerStack
        ↓
引擎服务层
    Renderer / Input / UI / AssetPath
        ↓
Scene / Gameplay / Editor
```

这一层不是在做“业务功能”，而是在做两种桥接：

1. 把统一窗口接口桥接到真实平台句柄
2. 把一帧渲染结果桥接到系统显示输出

所以这里的代码虽然短，但它决定了：

- 上层能不能调用 Win32 相关能力
- ImGui 后端能不能挂上
- 渲染结果能不能稳定呈现

---

## 1.3 `GetNativeWindow()`：为什么要把原生句柄露出来

### 1.3.1 `Window` 不是完整世界

`Window` 这个抽象接口只负责引擎需要的通用能力：

- 尺寸
- VSync
- 全屏
- 更新
- 事件回调

但有些平台能力并不属于这个抽象层。

例如：

- Windows 文件打开/保存对话框需要 `HWND`
- Dear ImGui 的 GLFW 后端需要 `GLFWwindow*`
- 某些原生平台 API 只能接受原生窗口句柄

所以 `Window` 里额外提供了：

```text
void* GetNativeWindow() const
```

它就是一个“必要时打开平台底层”的出口。

### 1.3.2 为什么返回 `void*`

`Window` 是跨平台接口。  
它不应该强迫上层知道自己到底是：

- `GLFWwindow*`
- `HWND`
- 还是别的平台句柄

所以接口层用 `void*` 只表达一件事：

```text
这是一个原生窗口对象，但具体类型留给平台层自己解释。
```

这种写法的好处是：

- 上层不被平台类型绑死
- 平台代码可以自己做安全转换
- 后面换平台实现时，接口不用改得太多

### 1.3.3 `WindowsWindow` 如何实现

在 Windows 实现里，`GetNativeWindow()` 直接返回 `m_Window`。  
而 `m_Window` 本身就是 `GLFWwindow*`。

也就是说：

```text
Window -> GetNativeWindow() -> GLFWwindow*
```

之后如果某个 Windows API 需要 `HWND`，再通过 GLFW 的 native helper 去转。

---

## 1.4 原生句柄到底给谁用

### 1.4.1 `ImGuiLayer` 需要它

`ImGuiLayer::OnAttach()` 里会做这件事：

```text
Application::Get().GetWindow().GetNativeWindow()
  -> GLFWwindow*
  -> ImGui_ImplGlfw_InitForOpenGL(...)
```

这说明 ImGui 的 GLFW 后端并不是只吃“一个抽象窗口”，  
它要的是 GLFW 句柄本身。

如果没有 `GetNativeWindow()`，ImGui 层就得和 `WindowsWindow` 强绑定。  
那样上层会更脏。

### 1.4.2 文件对话框需要它

`WindowsPlatformUtils.cpp` 里的 `FileDialogs` 是这一章最典型的消费方。

它的链路是：

```text
Application::Get().GetWindow().GetNativeWindow()
  -> GLFWwindow*
  -> glfwGetWin32Window(...)
  -> HWND
  -> GetOpenFileNameA / GetSaveFileNameA
```

也就是说，窗口句柄先从引擎抽象里拿出来，再通过 GLFW 转成 Win32 的 `HWND`，最后交给系统文件对话框。

这就是平台句柄真正的价值：

- 引擎层保持抽象
- 平台工具在需要时能下钻到底层

### 1.4.3 多文件打开为什么也放在这里

`FileDialogs::OpenFiles()` 还支持 Explorer 风格的多选：

- 用户选一个目录 + 多个文件
- 函数返回完整路径列表

这类能力本身也依赖原生窗口做 owner，  
否则弹窗和主窗口的关系就不稳定。

### 1.4.4 什么时候不该直接碰原生句柄

虽然原生句柄很有用，但它不应该到处飞。

你应该只在这些场合碰它：

- 平台对话框
- 第三方后端初始化
- 需要系统级窗口 API 的地方

如果 gameplay、scene、editor 面板开始直接拿这个句柄做事，就说明边界开始乱了。

### 1.4.5 一条完整的原生句柄流转链

你可以把它记成三段：

```text
Application::Get().GetWindow().GetNativeWindow()
  -> GLFWwindow*
  -> 需要原生句柄的地方
```

然后再分成两类消费方：

- `ImGuiLayer::OnAttach()` -> `ImGui_ImplGlfw_InitForOpenGL(...)`
- `FileDialogs::OpenFile()` / `OpenFiles()` / `SaveFile()` -> `glfwGetWin32Window(...)` -> `HWND` -> Win32 common dialogs

这一条链说明：`GetNativeWindow()` 不是给 gameplay 的，它是给平台工具和第三方后端的受控出口。

---

## 1.5 `ImGui` 为什么也要这个窗口

### 1.5.1 `ImGuiLayer` 的初始化链

`ImGuiLayer::OnAttach()` 的核心动作可以概括成：

```text
CreateContext()
  -> 设置 ini 路径
  -> 配字体
  -> 设置主题
  -> 获取 GLFWwindow*
  -> ImGui_ImplGlfw_InitForOpenGL(window, true)
  -> ImGui_ImplOpenGL3_Init("#version 410")
```

这里最关键的就是最后两步：

- GLFW 后端需要窗口
- OpenGL 后端需要当前渲染上下文已经可用

所以 `Window` 和 `OpenGLContext` 在前面必须先就位。

### 1.5.2 为什么 ImGui 还要回到原生句柄

ImGui 在启用多视口时，不只是画一个面板。  
它还可能创建额外的平台窗口。

所以它不只是“吃绘图数据”，  
它还需要知道当前平台窗口是谁，以便做：

- 输入分发
- 鼠标捕获
- 多平台窗口更新

`ImGuiLayer::End()` 里也能看到这一点：

- 先 `RenderDrawData`
- 如果启用了多视口，就更新平台窗口
- 再恢复当前上下文

这说明原生句柄不仅用于“第一次初始化”，也影响后续平台窗口行为。

---

## 1.6 `SwapBuffers()`：为什么最终是 context 在交换

### 1.6.1 交换缓冲到底在交换什么

图形渲染一般是双缓冲：

- 前缓冲：屏幕正在显示的内容
- 后缓冲：这一帧正在画的内容

`SwapBuffers()` 的作用就是：

```text
把后缓冲切成前缓冲
```

这样用户才会看到新的一帧。

### 1.6.2 为什么不是 `Window::OnUpdate()` 自己直接画

`Window::OnUpdate()` 在 Wheatear 里并不负责“画什么”，  
它只是把窗口这一帧的呈现动作委托给图形上下文。

也就是说：

```text
Window 负责宿主表面
GraphicsContext 负责呈现翻页
```

这比把 swap 写进 window 更清楚，因为 swap 本质上属于图形 API 生命周期，而不是单纯窗口生命周期。

### 1.6.3 `OpenGLContext::SwapBuffers()`

在 OpenGL 实现里，最终调用的是：

```text
glfwSwapBuffers(m_WindowHandle)
```

这意味着真正翻页的动作发生在：

```text
OpenGLContext -> GLFW -> 系统显示缓冲
```

### 1.6.4 `Application::Run()` 里为什么最后才 swap

主循环里，顺序大致是：

```text
Poll events
  -> Flush event queue
  -> 更新各层
  -> ImGui Begin / End
  -> EndFrame()
  -> Window::OnUpdate()
```

把 swap 放在最后的原因很简单：

- 这一帧所有更新都应该先完成
- 先把逻辑、渲染和 UI 画完
- 最后再把结果呈现出去

这就是帧的收口。

### 1.6.5 从主循环到最终呈现的收口链

把前面几节连起来看，完整的帧末链路大概是：

```text
Application::Run()
  -> Poll events
  -> Flush event queue
  -> Layer::OnUpdate()
  -> ImGuiLayer::Begin()
  -> ImGuiLayer::End()
  -> Window::OnUpdate()
  -> OpenGLContext::SwapBuffers()
```

这里要特别记住：`SwapBuffers()` 发生在所有更新完成之后。  
它不是“开始渲染”，而是“把这一帧已经画好的结果提交到屏幕”。

---

## 1.7 一个完整例子：打开文件、再看到画面

### 例子 1：文件对话框

```text
编辑器里点 Open
  -> FileDialogs::OpenFile()
  -> GetNativeWindow()
  -> GLFWwindow*
  -> glfwGetWin32Window()
  -> HWND
  -> GetOpenFileNameA()
  -> 返回用户选择的路径
```

这个例子说明：  
`GetNativeWindow()` 不是摆设，它是平台工具的入口。

### 例子 2：一帧画面怎么出现在屏幕上

```text
Application::Run()
  -> Layer::OnUpdate()
  -> Renderer::绘制到后缓冲
  -> ImGuiLayer::RenderDrawData()
  -> Window::OnUpdate()
  -> OpenGLContext::SwapBuffers()
  -> 看到新的一帧
```

这个例子说明：  
`SwapBuffers()` 不是“某个渲染函数的一部分”，  
它是帧结束时真正把结果提交给屏幕的那一步。

---

## 1.8 和 Unity、UE、Godot 怎么对照

### Unity

Unity 里你很少直接碰到 `HWND` 或 `GLFWwindow*`，  
但编辑器内部其实也有类似的 native window bridge。

Wheatear 这里把这层桥显式暴露出来了，  
更适合看源码和讲架构边界。

### Unreal

UE 里对应的是各种平台窗口句柄、Slate 窗口和系统 API 的接点。  
你不会在 gameplay 里直接碰它，但底层工具链会需要。

### Godot

Godot 也有很明确的窗口宿主和后端接口，  
只是它把很多细节包得更深。  
Wheatear 的写法更“直接”，更适合教学。

---

## 1.9 这一层的优点、坏处和常见误区

### 优点

- 平台工具和 ImGui 后端可以顺利接上
- 上层不必依赖具体 Win32 类型
- `SwapBuffers()` 的职责很清楚
- 窗口层和图形上下文职责分离
- 主循环的收口点明确

### 坏处

- `void*` 句柄有一定侵入性，使用时必须小心转换
- 原生平台代码容易让人不自觉越界
- 如果不理解 swap 的位置，很容易误把“渲染”和“呈现”混成一件事
- 多视口、系统对话框、窗口宿主这几条线会交叉，初学者容易乱

### 常见误区

- 以为 `GetNativeWindow()` 是给 gameplay 用的，其实不是
- 以为 `SwapBuffers()` 是“渲染开始”，其实它是渲染结束后的呈现动作
- 以为 `Window::OnUpdate()` 应该做很多事，实际上这里只该负责交换和宿主刷新
- 以为 ImGui 只需要 OpenGL，不需要窗口句柄
- 以为系统文件对话框和引擎窗口没关系，实际上它们通过 owner 句柄绑定

---

## 1.10 面试时怎么讲

如果面试官问你：“为什么 `Window` 还要暴露 `GetNativeWindow()`？”

你可以这样答：

```text
因为引擎的抽象窗口不能覆盖所有平台功能。
像 Win32 文件对话框、ImGui 的 GLFW 后端、以及一些系统级 API，都需要原生窗口句柄。
所以我把原生句柄作为一个受控出口暴露出来，平时上层仍然只用抽象 Window，需要下钻时再拿 native handle。
```

如果面试官继续问：“为什么 swap 在 context 里，不直接放到 window 里？”

你可以补一句：

```text
因为 swap 本质上是图形 API 的呈现动作，不是单纯的窗口行为。
Window 负责宿主表面，GraphicsContext 负责上下文生命周期和缓冲交换，这样边界会更清楚。
```

如果继续追问：“一帧是怎么最终显示出来的？”

你就说：

```text
主循环里先更新各层和 ImGui，再在帧末调用 Window::OnUpdate()；
WindowsWindow 会把这件事委托给 OpenGLContext::SwapBuffers()，最后通过 glfwSwapBuffers 把后缓冲翻到前台。
```

---

## 1.11 小结

这一章其实只讲了一件事：

```text
抽象窗口负责统一入口，原生句柄负责平台下钻，SwapBuffers 负责把一帧真正送上屏幕。
```

Wheatear 在这一层做了三件事：

- 用 `GetNativeWindow()` 给平台和第三方库留出口
- 用 `WindowsPlatformUtils` 把原生句柄接到系统文件对话框
- 用 `OpenGLContext::SwapBuffers()` 完成最终呈现

把这一章看懂以后，你再往上看：

- `ImGuiLayer`
- `Renderer`
- `Application::Run()`
- `WindowsPlatformUtils`

就会很顺。  
因为你会知道：

```text
这些东西为什么都要向窗口和上下文低头。
```

下一步我建议就回到更上层，把 `02_核心平台层` 的章节继续补起来。  
如果你愿意，我下一条就接着写核心平台层的第一篇。 
