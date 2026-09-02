# 第 7 编 04：Build 打包发布

> 本章所属层级：**应用层**
>
> 本章目标：理解 Wheatear 是怎么把项目内容打成可分发的玩家程序的，编辑器里“打包玩家 + 编辑器”按钮背后到底做了什么，最终产物怎么组织，打包后的 Sandbox 又是怎么把包体跑起来的。

如果说上一章讲的是：

```text
编辑器里怎么预览场景、怎么切 Play、怎么停下来
```

那这一章讲的就是：

```text
这些内容怎么变成一个真正能交给别人运行的成品？
成品里该放哪些东西？
启动场景从哪里来？
编辑器和玩家程序为什么要一起打包？
```

这章很像“工程最后一公里”。  
它不是在讲编译器语法，而是在讲产品化流程：

- 先把当前工程状态保存下来
- 再做依赖检查
- 然后编译 Editor 和 Sandbox
- 再把资源、配置、启动场景和报告整理成包
- 最后让 Sandbox 从这个包里启动

Wheatear 的做法很实用：

```text
Build 不是单纯“编译”，
而是“编译 + 组装 + 预检 + 发布”。
```

---

## 7.1 本章要读的源码

```text
WheatearEditor/src/WheatearEditorApp.cpp
WheatearEditor/src/Build/PlayerPackager.h
WheatearEditor/src/Build/PlayerPackager.cpp
WheatearEditor/src/Editor/EditorLayerBaseScene.cpp
WheatearEditor/src/Editor/EditorLayerBaseUI.cpp
WheatearEditor/src/Editor/EditorLayerBaseUI_Viewport.cpp

WheatearSandbox/src/WheatearSandboxApp.cpp
Wheatear/src/Wheatear/Assets/AssetPath.cpp
Wheatear/src/Wheatear/Assets/AssetPath.h

scripts/Build-Windows.ps1
```

如果你只想先抓主线，就先看这四个：

```text
WheatearEditorApp.cpp
PlayerPackager.cpp
EditorLayerBaseScene.cpp
WheatearSandboxApp.cpp
```

它们分别回答：

```text
编辑器怎么接收打包命令？
打包器到底怎么组装产物？
编辑器里点击打包以后怎么异步执行？
打包后的玩家程序怎么找到自己的包体？
```

---

## 7.2 这一层在架构中的位置

```text
应用层
    Editor / Sandbox
        ↓
构建与发布
    Package / Preflight / Artifact Layout / Runtime Bootstrap
        ↓
核心平台层
    Application / Window / EventBus / LayerStack
        ↓
引擎服务层
    AssetPath / AssetRegistry / Scene / Renderer / Runtime Services
```

这一层不是“编译系统”的替代品。  
它更像是引擎自己的发布流水线：

- `Editor` 负责发起动作
- `PlayerPackager` 负责做具体的打包工作
- `Sandbox` 负责验证包体能不能独立跑起来

也就是说，Build 在 Wheatear 里是应用层的一部分，不是散落在某个工具脚本里的杂项。

---

## 7.3 Build 在 Wheatear 里分成哪几件事

很多人一听 “Build”，会只想到 “编译代码”。  
但在这个项目里，Build 至少分成四件事：

### 1. 编译

把 `WheatearSandbox` 和 `WheatearEditor` 两个工程编出来。

### 2. 打包

把运行所需的二进制、资源、配置、启动场景和报告组装成发布目录。

### 3. 预检

先扫依赖、扫场景跳转、扫缺失引用。  
有问题就先停，不要把坏包发出去。

### 4. 发布

让 `WheatearSandbox.exe` 能直接从包体中启动，形成“可运行的最终产品”。

所以你在看这章时，脑子里要一直有这个顺序：

```text
编辑器当前场景
  -> 保存
  -> 依赖扫描
  -> 编译
  -> 组装
  -> 输出
  -> Sandbox 启动
```

---

## 7.4 编辑器里的打包入口

### 7.4.1 菜单和工具栏

编辑器里有两个很直接的入口：

- 顶部菜单 `Build -> Package Player + Editor`
- 工具栏上的打包按钮

它们都会走同一条链路：

```text
StartPlayerPackageBuild()
  -> 选择启动场景
  -> ExecutePlayerPackageBuild()
  -> std::async 启动后台打包
  -> PollPlayerPackageBuild() 轮询结果
```

### 7.4.2 为什么打包前要先保存场景

`StartPlayerPackageBuild()` 会先处理当前场景：

- 如果编辑器还在 Play，就先 Stop
- 如果当前场景没保存，就先 `SaveSceneAs()`
- 如果已经有场景文件，就先 `SaveScene()`

这不是多余动作。  
因为打包器应该基于“磁盘上最新保存的内容”，而不是基于一个只存在于内存里的临时编辑状态。

### 7.4.3 为什么会弹出启动场景选择器

打包不是只说“把工程打出去”就完了，  
还必须明确回答：

```text
玩家一运行，第一关从哪张 Scene 开始？
```

所以编辑器会弹出一个 Startup Scene 选择框。  
默认值通常来自：

- 当前打开的场景
- 项目级 `assets/game/player.config`

如果你手动改了它，最终会影响包体中的启动入口。

### 7.4.4 异步打包为什么要放在后台

`ExecutePlayerPackageBuild()` 不会阻塞 UI 主线程，而是用 `std::async` 起一个后台任务：

- 主线程继续响应编辑器
- 后台线程跑 `PlayerPackager::PackagePlayer()`
- `PollPlayerPackageBuild()` 负责收结果

这样做的好处很明显：

- 打包很慢，但编辑器不会卡死
- 结果出来以后还能显示目录和报告
- 用户可以在后台任务结束后直接打开产物目录

### 7.4.5 命令行打包入口

除了 UI，`WheatearEditorApp.cpp` 还支持命令行直接打包：

```text
--package
--package-player
--build-player
```

这条路径会直接执行 `RunPackagePlayer(args)`，然后退出。  
适合自动化脚本、CI、批处理发布。

这说明 Wheatear 不是只会“点按钮打包”，  
它也考虑了无人值守的发布流程。

---

## 7.5 `PlayerPackager::PackagePlayer()` 的主链路

这一部分是整章的核心。

`PlayerPackager::PackagePlayer()` 做的事情可以按顺序理解成下面这条流水线：

```text
Repository Root
  -> Output Directory
  -> Build Sandbox / Build Editor
  -> Clean Package
  -> Copy Binaries
  -> Copy Resources
  -> Resolve Startup Scene
  -> Dependency Scan
  -> Write content.wtpack
  -> Copy Loose Runtime Data
  -> Write Report
  -> Save player.config
```

### 7.5.1 先定位仓库根和工程根

打包器先做两件事：

- 找整个仓库根
- 读取当前项目根

因为后面的：

- `Builds/Windows/Player/...`
- `Builds/Windows/Editor/...`
- `bin/Debug-windows-x86_64/...`

这些路径都依赖仓库结构。

### 7.5.2 输出目录怎么定

默认情况下，玩家包会落在：

```text
Builds/Windows/Player/<PackageName>
```

编辑器包会落在：

```text
Builds/Windows/Editor
```

其中 `<PackageName>` 会优先来自项目的 `player.config`，  
如果没配，就用项目路径派生一个可读的名字。

这意味着打包产物不是“随便往一个文件夹塞”，而是有稳定布局的。

### 7.5.3 为什么打包器先编译两个工程

打包器不会假设你已经手动把二进制编好了。  
它会通过 `scripts/Build-Windows.ps1` 去构建：

- `WheatearSandbox.vcxproj`
- `WheatearEditor.vcxproj`

`Build-Windows.ps1` 再调用 MSBuild，并且用清洁环境启动。  
这比直接依赖 IDE 当前状态稳定得多。

#### 一个很重要的细节

如果当前运行的就是编辑器自己，而编辑器又要被打到它自己的包目录里，Windows 会锁住这个 exe。  
所以打包器会识别这种“自打包”场景，避免覆盖正在运行的可执行文件。

这类细节很像工程经验题，面试官通常会喜欢问。

### 7.5.4 清理旧包

正式写入之前，打包器会先清理旧目录。  
这样做的原因很简单：

- 防止旧的 dll / exe / 资源残留
- 防止旧报告误导排查
- 防止脏文件让你以为新包已经成功了

### 7.5.5 复制运行时二进制

打包器会把运行时可执行文件和依赖 dll 复制到目标目录。

如果你选择包含调试符号，也可以把 `.pdb` 一起带上。  
不过默认发布流程是 Release，并且不带 debug symbols。

### 7.5.6 复制编辑器二进制和资源

编辑器包也会单独生成。  
它除了 exe/dll，还会带上 `WheatearEditor/Resources`，  
这样内容工具、图标、模板和编辑器内建资源才不会丢。

所以这次打包不是只产出一个玩家程序，而是两套：

- 玩家程序包
- 编辑器程序包

这是很典型的“工具链发布”做法。

### 7.5.7 启动场景怎么处理

`ResolvePackagedStartupScene()` 会先看启动场景是不是已经在 `assets/` 里。

如果它本来就是项目资产，就直接使用相对路径。

如果它不在 `assets/` 下，打包器会把它复制成：

```text
assets/scenes/PackagedStartup.wt
```

然后在包内用这个新场景作为启动入口。

这一步很关键，因为最终包体必须有一个稳定、可定位的启动场景。

### 7.5.8 为什么要做依赖扫描

正式写包前，打包器会跑 `AssetDependencyScanner::BuildReport()`。

它会检查：

- 资产引用是否存在
- 场景切换目标是否存在
- 各种文本资产是否可解析
- 是否有未使用资源

如果发现：

- 缺失引用
- 缺失场景跳转

打包就会直接失败，并输出 `package_preflight_report.txt`。

这一步不是“可有可无的检查”，  
它是把坏内容挡在发布前的门槛。

### 7.5.9 `content.wtpack` 是什么

依赖扫描通过以后，打包器会把资产写入一个二进制包：

```text
content.wtpack
```

这个包有自己的头部、版本号和条目表。  
每个条目都记录：

- 相对路径
- 压缩方式
- 原始大小
- 存储大小
- 实际数据

简单说，它就是 Wheatear 自己的资产容器。

压缩策略也很实用：

- png / jpg / 音频 / 字体这类已经比较“像最终态”的内容，通常不再额外压
- 文本、场景、脚本、shader 这类更容易被压缩的内容，优先尝试 zlib

### 7.5.10 为什么还有 loose runtime data

除了 `content.wtpack`，打包器还会把一部分运行时数据以 loose file 的形式放在包目录里。

这些通常包括：

- `.yaml` / `.json`
- `.wt` / `.wts` / `.vn`
- shader 源文件
- 音频文件
- `assets/game/player.config`

原因不是“重复劳动”，而是为了让运行时能继续按文件路径找到它们。

这对下面几种场景很有用：

- 直接路径解析
- 热重载
- 调试时人工查看
- 某些文本资源的运行时读取

### 7.5.11 报告和 `player.config`

打包完成后，打包器会写两份很有价值的输出：

- `package_report.txt`
- `assets/game/player.config`

`package_report.txt` 会告诉你：

- 打包了哪些资产
- 多少字节
- 有无缺失引用
- 有无场景跳转缺失
- 启动场景是什么

`player.config` 则会把玩家程序启动时最关心的参数写进去，尤其是启动场景。

所以发布不是黑箱。  
你拿到的不是“一个 exe”，而是一整套可追踪的产物。

---

## 7.6 `content.wtpack` 是怎么被玩家程序吃进去的

打包只是第一半。  
另一半是：玩家程序怎么把这个包跑起来。

### 7.6.1 `WheatearSandbox` 的启动模式

沙盒启动时有两种大模式：

#### 开发者模式

如果命令行传了 `--project <dir>`，  
沙盒会直接跑这个项目的 loose assets。

这种模式适合开发时反复看改动，不需要每次都重新打包。

#### 发布模式

如果没有 `--project`，沙盒就会去找 `content.wtpack`。

找到后会：

- 计算包的 fingerprint
- 抽取到本地缓存目录
- 把抽取后的目录当作 ProjectRoot
- 把可写根放到 exe 旁边或用户缓存区

### 7.6.2 为什么要先解包再跑

沙盒不直接在压缩包里读内容，而是先解包到缓存。  
这样做的好处很实际：

- 文件访问路径统一
- 运行时逻辑不用知道包体内部格式
- 包更新后可以按 fingerprint 重新抽取
- 对 `AssetPath::ResolveRuntimeData()` 这种路径解析更友好

### 7.6.3 `ResolveRuntimeData()` 为什么重要

`AssetPath::ResolveRuntimeData()` 会优先找 loose runtime data。  
如果找不到，再回退到普通的 `Resolve()`。

所以同一个路径在运行时可能有两种来源：

- 直接的 loose file
- 从 `content.wtpack` 解包后的缓存文件

这就把“发布包”与“路径读取”接起来了。

### 7.6.4 打包产物到底长什么样

Wheatear 的打包不是只产出一个 exe，而是一整套成品目录。
最核心的东西可以先记成这几个：

- `Builds/<Platform>/Player`
- `Builds/<Platform>/Editor`
- `WheatearSandbox.exe`
- `WheatearEditor.exe`
- `content.wtpack`
- `assets/game/player.config`
- `package_report.txt`
- `package_preflight_report.txt`

这里面最关键的分工是：

- `content.wtpack`：真正给玩家程序消费的内容包
- `player.config`：告诉玩家程序该从哪里开始跑
- `package_report.txt`：记录这次打包到底带了什么
- `package_preflight_report.txt`：记录打包前扫出来的问题

也就是说，打包结果不是“一个可执行文件”，而是“可执行文件 + 内容包 + 配置 + 报告”。

### 7.6.5 `PlayerPackager::PackagePlayer()` 一步一步做了什么

`PlayerPackager::PackagePlayer()` 的链路其实很像一条小型发布流水线：

1. 定位 repository root 和 project root
2. 检查启动场景是否存在
3. 计算 Player / Editor 输出目录
4. 先构建 `WheatearSandbox`
5. 再构建 `WheatearEditor`
6. 清理旧包
7. 复制运行时二进制
8. 复制编辑器二进制和资源
9. 解析启动场景
10. 扫描依赖关系
11. 写 `content.wtpack`
12. 复制 loose runtime data
13. 写 `package_report.txt`
14. 写 `player.config`

其中最值得注意的是第 10 步。  
它不是“把目录下的文件全拷一遍”，而是通过 `AssetDependencyScanner::BuildReport()` 先算依赖闭包，再决定哪些内容真正进包。

这也解释了为什么打包前会先做预检：  
如果有缺失引用、缺失场景跳转，打包器会直接报错，而不是把坏包硬塞给玩家。

### 7.6.6 GUI 打包和 CLI 打包其实是同一条路

编辑器里的工具栏按钮 `Package Player + Editor` 走的是同一套核心逻辑，  
只是把入口从命令行换成了 UI。

这两条路最后都会落到 `PlayerPackager::PackagePlayer()`：

- GUI 路径：工具栏按钮 / 打包窗口 / 后台异步任务
- CLI 路径：`--package-player`、`--build-player`

所以你可以把它理解成：

- UI 负责让人点
- CLI 负责让脚本跑
- 真正的打包规则只有一份

这也是为什么我前面一直强调：  
应用层不只是几个面板，它还承担发布流水线。

---

## 7.7 一个完整例子：从按钮到可运行包

### 例子 1：编辑器里点“打包玩家 + 编辑器”

```text
当前场景在编辑器里打开
  -> StartPlayerPackageBuild()
  -> 先保存场景
  -> 弹出启动场景选择器
  -> ExecutePlayerPackageBuild()
  -> std::async 后台打包
  -> PlayerPackager::PackagePlayer()
  -> 编译 Sandbox 和 Editor
  -> 扫依赖、写 content.wtpack
  -> 复制 loose runtime data
  -> 写 player.config 和 report
  -> PollPlayerPackageBuild() 收结果
  -> 在 Player Build 窗口里显示目录和报告
```

### 例子 2：玩家程序怎么从包体启动

```text
双击 WheatearSandbox.exe
  -> 找 content.wtpack
  -> 抽取到 cache
  -> 读取 assets/game/player.config
  -> 加载启动场景
  -> OnRuntimeStart()
  -> 正式进入游戏
```

这两条链路放在一起看，就能看懂：

```text
编辑器负责生产，
Sandbox 负责消费。
```

---

## 7.8 和 Unity、UE、Godot 怎么对照

### Unity

Unity 对应的是：

- `Build Settings`
- `Build and Run`
- `Player`
- `Editor`

Wheatear 的做法和它很像，只是更轻，  
没有把发布流水线藏在一个非常大的 IDE 壳里。

### Unreal

UE 里可以类比成：

- 打包配置
- Cook / Package
- Standalone Game
- Editor / PIE

Wheatear 这里更偏向“自己写包体规则”，  
而不是依赖特别重的引擎内部 cook 系统。

### Godot

Godot 里可以对照：

- Export Project
- Project Settings / Main Scene
- Packed 资源

Wheatear 的风格和 Godot 最接近的地方是：  
都强调“编辑器内容”和“可运行成品”是两层东西。

---

## 7.9 这一层的优点、坏处和常见误区

### 优点

- 打包流程和编辑器深度绑定，操作直观
- 产物结构清晰，玩家包和编辑器包分离
- 有依赖预检，不容易把坏内容带进发布包
- 包体和 loose runtime data 可以同时存在
- Sandbox 能直接验证包体是否可运行

### 坏处

- 逻辑比“单纯编译”复杂得多
- 包体格式、路径规则、启动场景、可写目录都要设计
- 依赖扫描和打包步骤一多，排错就不再是纯编译问题
- 一旦 editor / player / runtime 的语义混在一起，问题会很难查

### 常见误区

- 以为打包只是把 exe 拷走
- 以为场景能跑就说明包一定没问题
- 忘了先保存场景就开始打包
- 忘了 startup scene 必须明确
- 以为 `content.wtpack` 是唯一需要的内容，忽略 loose runtime data
- 打包失败后不看 preflight report
- 不区分 `Builds/Windows/Player` 和 `Builds/Windows/Editor`

---

## 7.10 面试时怎么讲

如果面试官问你：“Wheatear 的打包发布流程是怎么设计的？”

你可以这样答：

```text
我们把打包当成应用层的发布流水线，而不只是编译。
编辑器先保存当前场景，再通过 PlayerPackager 做预检、编译和产物组装。
PlayerPackager 会先构建 Sandbox 和 Editor，再把运行时二进制、编辑器资源、启动场景、内容包 content.wtpack、loose runtime data 和 player.config 一起输出到 Builds 目录。
打包前还会跑依赖扫描，缺失引用或场景跳转会直接阻断发布。
```

如果面试官继续问：“为什么要单独做 content.wtpack？”

你可以补一句：

```text
因为这样可以把大部分资产集中进一个稳定的容器里，运行时抽取后再按统一路径访问；
而文本型、可热重载的资源还可以保留 loose file 形式，方便调试和路径解析。
```

如果继续追问：“玩家程序怎么知道跑哪个场景？”

你就说：

```text
打包时会写入 assets/game/player.config，里面保存 startup scene。
Sandbox 启动时先读这个配置，再从 content.wtpack 或 loose runtime data 中加载对应场景。
```

---

## 7.11 小结

这一章其实讲的是：

```text
引擎不只是能做出来，
还得能稳定地发出去、跑起来、查问题。
```

Wheatear 在 Build 这一层做了三件事：

- `Editor` 提供打包入口和任务状态
- `PlayerPackager` 负责编译、扫描、组装和报告
- `Sandbox` 负责从包体中独立启动

当你把这一章看懂以后，前面那些章节会更完整：

- `Application` 不只是宿主壳
- `Scene` 不只是编辑对象
- `AssetRegistry` 不只是索引
- `CommandBus` 不只是命令分发

因为它们最后都会落到一个最现实的问题上：

```text
这套引擎能不能变成一个别人拿去就能跑的游戏？
```

下一部分我们就会从应用层回到前面的章节，把整套教材的主线再串一次。  
如果你后面要继续补，我建议下一步去补“附录：Unity / UE / Godot 架构对照”。
