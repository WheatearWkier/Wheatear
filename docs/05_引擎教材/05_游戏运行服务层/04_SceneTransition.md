# 第 5 编 04：SceneTransition 与场景切换

> 本章所属层级：**游戏运行服务层**
>
> 本章目标：理解 Wheatear 为什么把“切场景、开新游戏、读存档”从玩法逻辑里抽出来，集中交给 `SceneTransitionService`、`RuntimeSceneLayer` 和 `GameProgress` 协作处理。
>
> 先提醒一句：这里讲的“切场景”不是单纯换一个 `.wt` 文件，而是连着输入状态、命令队列、场景生命周期、存档上下文一起换。你会看到它和 `CommandBus`、`EventScript`、`GameProgress` 是一条连续链，不是三个互不相干的功能。
>
> 本章会反复使用三个例子：
> 1. 主菜单点击“新游戏”后，为什么会进入新场景。
> 2. 战斗胜利后，为什么可以从事件脚本一路跳到结果场景。
> 3. 读取存档时，为什么要先知道“读哪一格”，再决定“回到哪个场景”。

## 5.1 这一层在架构中的位置

`SceneTransition` 处在 **游戏运行服务层** 的后半段。  
前面两章已经把“命令怎么来”和“动作怎么编排”讲过了，这一章就该讲“场景怎么换”。

它的位置很像一条桥：

```text
CommandBus / EventScript / Gameplay modules
    ↓
SceneTransitionService
    ↓
RuntimeSceneLayer / EditorSceneHost
    ↓
Scene / SceneSerializer / GameProgress
```

它解决的不是“这个按钮按下去有没有效果”，而是：

- 这次请求是切普通场景，还是新游戏，还是读存档。
- 切场景前要不要清输入、清命令队列、停掉旧场景。
- 切完之后要不要更新进度系统里的当前场景信息。

这也是为什么它不能散在玩法里：

- VN 想切场景。
- 战斗想切结果页。
- 存档系统想切回读取点。

如果这些地方各写各的，后面就会变成“谁都能切，谁都没收尾”。

## 5.2 本章要读的源码

建议按这个顺序看：

- `Wheatear/src/Wheatear/Runtime/SceneTransitionService.h`
- `Wheatear/src/Wheatear/Runtime/SceneTransitionService.cpp`
- `WheatearSandbox/src/RuntimeSceneLayer.h`
- `WheatearSandbox/src/RuntimeSceneLayer.cpp`
- `Wheatear/src/Wheatear/Runtime/CommandBus.cpp`
- `Wheatear/src/Wheatear/Modules/Progression/GameProgress.h`
- `Wheatear/src/Wheatear/Modules/Progression/GameProgress.cpp`
- `Wheatear/src/Wheatear/Modules/VisualNovel/VisualNovelSystem.cpp`
- `Wheatear/src/Wheatear/Modules/Progression/ProgressionSystem.cpp`
- `WheatearEditor/src/Editor/EditorLayerBaseScene.cpp`

如果你只想先看一条最核心的链路，就看：

- `CommandBus::Execute()`
- `SceneTransitionService::RequestNewGame()`
- `RuntimeSceneLayer::ConsumeSceneTransitionRequests()`
- `RuntimeSceneLayer::ExecuteSceneTransitionRequest()`
- `RuntimeSceneLayer::LoadScene()`

## 5.3 先给结论：切场景不是直接切

Wheatear 里的场景切换不是“某个函数立刻把当前场景替换掉”，而是一个两段式流程：

1. 先发请求。
2. 再由宿主层统一消费请求并真正加载。

这能避免很多问题：

- 在 UI 回调里直接把场景对象删掉。
- 在事件脚本执行中途就硬切。
- 同一帧里多个模块同时请求跳转时互相打架。

所以本章要记住的核心句子是：

> `SceneTransitionService` 负责排队，`RuntimeSceneLayer` 负责落地。

## 5.4 `SceneTransitionService`：只负责发请求

### 5.4.1 它的公开 API

`SceneTransitionService` 目前就这几个接口：

- `RequestLoadScene(scenePath, sourceCommand)`
- `RequestNewGame(scenePath, sourceCommand)`
- `RequestLoadGame(scenePath, slot, sourceCommand)`
- `HasPendingRequests()`
- `DrainRequests()`

这说明它很克制：

- 不直接加载。
- 不直接改当前场景。
- 不直接碰 `Scene` 对象。

它只是把请求放进一个静态队列里。

### 5.4.2 请求里装什么

`SceneTransitionRequest` 里有四个字段：

- `Mode`
- `ScenePath`
- `Slot`
- `SourceCommand`

这四个字段已经能覆盖大多数场景跳转需求了。

例如：

- `Mode = NewGame`
- `ScenePath = assets/scenes/Intro.wt`
- `Slot = 1`
- `SourceCommand = newgame:assets/scenes/Intro.wt`

这比直接传一个字符串清楚得多。  
因为后面宿主层不用再猜这条请求到底是新游戏还是读档。

### 5.4.3 为什么要有队列

因为场景切换本质上是“有副作用的大事件”。

如果你在任意地方都能立刻切：

- 输入回调可能刚处理一半。
- 事件脚本可能还在跑。
- 玩法系统可能还没把这一帧的状态写完。

用队列的好处是：

- 请求先收集起来。
- 一帧结束时统一决定谁生效。
- 多个请求同时出现时，可以做明确的优先策略。

当前实现里，`DrainRequests()` 会把整批请求一次性拿走。  
宿主层会按“最后一个请求”来处理本帧结果。

这点很适合简单 2D 项目，因为它够清楚，也够容易调试。

## 5.5 `CommandBus` 里的场景命令是怎么转成请求的

这一段是整条链的起点之一。

### 5.5.1 `scene:`、`newgame:`、`loadgame:`

在 `CommandBus.cpp` 里，运行时队列命令会走这几种分支：

- `scene:` -> `SceneTransitionService::RequestLoadScene(...)`
- `newgame:` -> `SceneTransitionService::RequestNewGame(...)`
- `loadgame:` -> `SceneTransitionService::RequestLoadGame(...)`

也就是说：

> `CommandBus` 不负责真的切场景，它只负责把命令翻译成场景请求。

### 5.5.2 `loadgame:` 还要带槽位

`loadgame:` 不是单纯一个路径字符串，它还有 slot。

它会解析成这样的逻辑：

```text
loadgame:scenePath:slot
```

如果当前场景不允许读取，`CommandBus` 会直接返回失败，并把提示写进 `GameProgress::GetState().LastResultMessage`。

这说明场景切换不是无条件的，宿主和进度状态都能参与约束。

### 5.5.3 为什么 `CommandBus` 要先转一层

因为 `CommandBus` 已经是“命令路由器”了。  
它不应该直接拥有“加载场景对象”的权力。

把它和 `SceneTransitionService` 分开，结果会更稳定：

- `CommandBus` 只关心命令语义。
- `SceneTransitionService` 只关心切换请求。
- `RuntimeSceneLayer` 只关心真正加载。

这三层拆开以后，后面调试时会特别省心。

## 5.6 宿主层是什么：`RuntimeSceneLayer`

`RuntimeSceneLayer` 是真正把请求落地的人。

### 5.6.1 它在做什么

它做的事情可以概括成四类：

- 启动时决定先加载哪个场景。
- 每帧更新当前场景。
- 消费运行时命令和场景请求。
- 真正加载、停止、切换场景。

### 5.6.2 启动时先读什么

`OnAttach()` 会先做几件事：

- 清空 WAO 运行历史。
- 读 `player.config`。
- 看命令行有没有指定启动场景。
- 没有的话就用默认场景。
- 然后调用 `LoadScene(requestedPath)`。

这说明“启动哪个场景”并不是写死在一个地方，而是支持配置和命令行覆盖的。

### 5.6.3 每帧怎么跑

`OnUpdate()` 的顺序很重要：

1. 更新 viewport。
2. 清屏。
3. 热重载 WAO 动作表。
4. 调当前场景的 `OnUpdateRuntime(ts)`。
5. 消费运行时命令和场景请求。

也就是说，真正的场景切换不是在输入回调里做，而是在 update 里统一消费。

这是个很稳的设计。

## 5.7 `LoadScene()`：真正的切换动作

### 5.7.1 先处理什么

`LoadScene()` 一上来先做两件事：

- `UIInputSystem::Reset()`
- `CommandBus::ClearQueuedCommands()`

这一步很关键。

因为如果你不清：

- 旧场景里残留的鼠标状态会污染新场景。
- 上一场景末尾没消费完的命令可能会在新场景里重新触发。

这就是为什么切场景不是“换个 Scene 指针”这么简单。

### 5.7.2 为什么要停旧场景

如果旧场景还在运行，`LoadScene()` 会先：

- `m_ActiveScene->OnRuntimeStop()`
- `m_RuntimeStarted = false`

这意味着场景切换前要先让旧场景有一个明确的收尾点。

这和很多引擎的做法很像：

- 先退出当前世界。
- 再进入新世界。

不然资源、脚本、输入状态和运行计时器都会乱套。

### 5.7.3 新场景怎么建

随后 `LoadScene()` 会：

1. 解析场景路径。
2. `CreateRef<Scene>()`
3. 用 `SceneSerializer` 反序列化 YAML。
4. 如果失败，报错并放弃。
5. 如果成功，继续初始化。

这说明运行时场景其实也是从序列化数据还原出来的，不是靠硬编码搭出来的。

### 5.7.4 为什么要设置 `GameProgress` 的场景上下文

`LoadScene()` 成功后，会调用：

```text
GameProgress::SetSceneTransitionContext(previousScenePath, sceneRequest);
```

这一步的作用是把“当前在哪个场景”和“上一个场景是什么”记录进进度系统。

这样后面保存、读取、结算、返回都能知道上下文。

### 5.7.5 为什么还要推 viewport

场景加载完后，`LoadScene()` 会把 viewport 强制刷新一次。

原因很朴素：

- 新场景可能有新的相机。
- 即使窗口大小没变，场景视口也该重新同步一次。

这类小细节特别容易被忽略，但它们往往决定“切完场景以后画面是不是正常”。

## 5.8 新游戏怎么切：`SceneTransitionMode::NewGame`

### 5.8.1 不是简单加载新场景

`NewGame` 模式会先：

- `GameProgress::ResetForNewGame()`
- 清掉 `m_PendingSceneAutoLoadSlot`
- 再 `LoadScene()`

这说明“新游戏”不仅是换一个起始场景，还包括清空进度。

### 5.8.2 为什么要先重置进度

因为新游戏的语义就是：

- 材料清零。
- 章节回到起点。
- 任务状态归零。
- 存档上下文重新开始。

如果不先重置进度，前一轮玩家留下的数据会污染新档。

### 5.8.3 一个很具体的例子

主菜单按钮常会走：

```text
event:menu_new_game
    -> newgame:assets/scenes/Intro.wt
```

实际链路是：

```text
UI 按钮
 -> CommandBus::Execute(scene, "event:menu_new_game")
 -> EventScriptSystem
 -> CommandBus::Execute(scene, "newgame:assets/scenes/Intro.wt")
 -> SceneTransitionService::RequestNewGame(...)
 -> RuntimeSceneLayer::ConsumeSceneTransitionRequests()
 -> RuntimeSceneLayer::ExecuteSceneTransitionRequest()
 -> GameProgress::ResetForNewGame()
 -> LoadScene("assets/scenes/Intro.wt")
```

这条链适合面试时完整讲一遍。

## 5.9 读存档怎么切：`SceneTransitionMode::LoadGame`

### 5.9.1 它比新游戏更麻烦一点

因为读存档除了场景路径，还带一个槽位。

所以 `LoadGame` 不只是“加载一个场景”，而是：

- 从某个槽读进进度。
- 再回到对应场景。
- 必要时自动回填存档槽信息。

### 5.9.2 先看能不能读

`RuntimeSceneLayer::ExecuteSceneTransitionRequest()` 里会先判断：

- 当前场景的 effective save policy 是否允许读取。

如果不允许，会把失败信息写进 `GameProgress::GetState().LastResultMessage`。

这和 `CommandBus` 里的前置检查形成呼应。

### 5.9.3 为什么要先记 slot

因为 `GameProgress::LoadSlot(slot)` 只知道读哪一格。  
而 `LoadScene()` 只负责换场景。

二者之间通过 `m_PendingSceneAutoLoadSlot` 衔接起来。

也就是说：

```text
LoadGame 请求
 -> 记录 slot
 -> 读进进度
 -> 切场景
 -> 新场景加载后自动带上 slot 上下文
```

### 5.9.4 `BuildLoadGameCommand()` 的作用

在 `GameProgress` 里有个很实用的函数：

```cpp
BuildLoadGameCommand(int slot, const std::string& scenePath = "");
```

它会把“读哪一格、回哪个场景”拼成一条命令。

所以 VN 或编辑器不用自己手拼 `loadgame:` 字符串，直接调用它就行。

这类辅助函数特别适合教材里讲：

> 不是所有流程都要写在 UI 里，数据层可以帮你把命令格式统一掉。

## 5.10 `scene:` 跳转的语义

### 5.10.1 它最普通，也最常见

`scene:` 就是最基础的场景跳转命令。

适合这些场景：

- 战斗结束跳结果页。
- VN 章节切换。
- 进出据点。
- 从一个测试场景跳到另一个测试场景。

### 5.10.2 一个战斗胜利的例子

事件脚本里可能会写：

```text
event side_combat_victory:
    progression:set_flag:FLAG_CH02_TUTORIAL_CLEARED
    wait 0.10
    scene:assets/scenes/Result.wt
end
```

链路会变成：

```text
.wts
 -> CommandBus
 -> SceneTransitionService::RequestLoadScene()
 -> RuntimeSceneLayer::ConsumeSceneTransitionRequests()
 -> LoadScene("assets/scenes/Result.wt")
```

这就是“流程编排”和“场景切换”接起来的地方。

### 5.10.3 为什么它不能直接加载

因为脚本中途还有别的命令可能在本帧继续跑。  
如果立刻切场景，剩余命令可能会在半截世界里执行。

用请求队列，就把这一切变得可控了。

## 5.11 多个场景请求同一帧出现时怎么办

### 5.11.1 当前实现的策略

`RuntimeSceneLayer::ConsumeSceneTransitionRequests()` 会把请求全部拿出来，然后只执行最后一个。

代码里甚至写了注释：

> Multiple scene requests in one frame are resolved by the final request.

这很直白，也很实用。

### 5.11.2 为什么这么做

因为在一帧里同时出现多个请求时，通常意味着：

- 一个事件脚本里连发了多个跳转命令。
- 一个 UI 回调和一个游戏逻辑同时请求切换。

让最后一个生效，规则简单，容易排查。

### 5.11.3 这是不是唯一正确答案

不是。  
但对当前项目来说，它足够稳，也足够好讲。

以后如果你要做更复杂的导航栈、场景栈、过渡动画队列，再改策略也不晚。

## 5.12 `GameProgress` 在这里到底干什么

### 5.12.1 它不是场景宿主

`GameProgress` 不是加载场景的执行器。  
它是“游戏进度状态”的中心。

它记录的东西很多：

- 章节
- 目标
- 当前场景路径
- 上一个场景路径
- 玩家等级
- 金币
- 材料
- 已解锁内容
- 存档槽信息

### 5.12.2 它和切场景的关系

场景切换发生时，`GameProgress` 需要知道：

- 现在进到哪了。
- 之前在哪。
- 当前该回到哪个点保存。

所以 `SetSceneTransitionContext()` 很关键。

### 5.12.3 `ResetForNewGame()` 的语义

新游戏不是“换关卡”而已。  
它是“重新开一局”的动作。

所以 `ResetForNewGame()` 会把整个进度状态恢复成默认值，然后再把设置同步回运行时。

### 5.12.4 `LoadSlot()` 的语义

读档不是单纯读数据文件。  
它还要决定读进来以后回哪个场景。

这就是为什么 `BuildLoadGameCommand()` 里会把 scene path 一起编码进去。

## 5.13 真实链路一：主菜单新游戏怎么进

这是最适合新手理解的一条链。

### 5.13.1 从按钮开始

主菜单按钮通常会触发一个事件命令：

```text
event:menu_new_game
```

`CommandBus` 会把它识别成事件请求，交给 `EventScriptSystem`。

### 5.13.2 事件脚本怎么继续

脚本里一般会再发：

```text
newgame:assets/scenes/Intro.wt
```

这条命令不会立刻切场景，而是进入 `SceneTransitionService`。

### 5.13.3 什么时候真正切

真正切换发生在宿主层的 update 阶段：

```text
RuntimeSceneLayer::ConsumeSceneTransitionRequests()
```

它拿到请求后执行：

```text
GameProgress::ResetForNewGame()
LoadScene("assets/scenes/Intro.wt")
```

### 5.13.4 这条链为什么合理

因为它把三件事拆开了：

- 按钮只负责触发。
- 脚本只负责流程。
- 宿主只负责加载。

这样以后你换主菜单 UI、换开场剧本、换起始场景，都不会牵一发而动全身。

## 5.14 真实链路二：存档读取怎么进

### 5.14.1 VN 里最容易看到

`VisualNovelSystem::LoadFromSlot()` 会直接调用：

```cpp
CommandBus::Execute(scene, GameProgress::BuildLoadGameCommand(safeSlot));
```

这意味着 VN 不直接管切场景细节。  
它只知道“我要读第几个槽”。

### 5.14.2 命令长什么样

`BuildLoadGameCommand()` 会拼出类似这样的命令：

```text
loadgame:assets/scenes/Intro.wt:1
```

它把目标场景和槽位一起编码进去。

### 5.14.3 后面会发生什么

链路继续走：

```text
CommandBus
 -> SceneTransitionService::RequestLoadGame()
 -> RuntimeSceneLayer::ExecuteSceneTransitionRequest()
 -> GameProgress::LoadSlot(slot)
 -> LoadScene(scenePath)
```

### 5.14.4 为什么这个设计比“直接读文件再切”好

因为存档和场景不是同一层的概念：

- 存档是进度。
- 场景是宿主世界。

把它们拆开，调试和维护都更轻松。

## 5.15 真实链路三：战斗结束后跳结果页

这条链很适合讲“事件脚本 + 场景切换”的组合。

### 5.15.1 事件脚本负责低频流程

例如战斗胜利后，`.wts` 可以写：

```text
event side_combat_victory:
    progression:set_flag:FLAG_CH02_TUTORIAL_CLEARED
    wait 0.10
    scene:assets/scenes/Result.wt
end
```

### 5.15.2 `CommandBus` 负责把命令拆给对应系统

这里先改进度，再切场景。  
这顺序很常见，也很合理。

### 5.15.3 `SceneTransitionService` 负责把跳转请求挂起来

它不急着动世界，只先把请求收下。

### 5.15.4 `RuntimeSceneLayer` 最后统一收口

最后真正落地：

```text
LoadScene("assets/scenes/Result.wt")
```

这条链就是“流程编排”如何进入“场景宿主”的好例子。

## 5.16 `RuntimeSceneLayer::LoadScene()` 为什么要先做这些清理

### 5.16.1 清输入

`UIInputSystem::Reset()` 先做掉，避免旧场景的输入状态带过去。

### 5.16.2 清命令

`CommandBus::ClearQueuedCommands()` 先做掉，避免旧场景残留命令在新场景里误触发。

### 5.16.3 停旧场景

`OnRuntimeStop()` 先做掉，确保旧场景有完整收尾。

### 5.16.4 再建新场景

然后再创建新的 `Scene`，反序列化 YAML，成功后再调用 `OnRuntimeStart()`。

这条顺序很值得记：

```text
清输入
清命令
停旧场景
建新场景
反序列化
设置进度上下文
启动新场景
```

这就是一个合格的场景切换链。

## 5.17 为什么这层也要管 viewport

场景切换时，镜头和视口经常会变。

所以 `LoadScene()` 里会强制把 viewport 刷新一次，即使窗口尺寸没变。

这很像很多引擎里切场景后的“重新同步摄像机和渲染目标”。

对初学者来说，这种细节很容易忽略。  
但在实际项目里，它经常就是“为什么我切完场景画面是黑的/偏了”的根源。

## 5.18 和 Editor 的关系

编辑器里也有类似的场景宿主逻辑。

`EditorLayerBaseScene.cpp` 同样会：

- 清命令
- Drain `SceneTransitionService` 请求
- 根据请求执行切换

这说明场景切换不是 runtime 独有的，而是编辑器和运行端都要共享的一层基础能力。

区别在于：

- Sandbox 更像独立运行端。
- Editor 更像工具宿主。

但底层思路一致。

## 5.19 和 Unity、UE 的对照

### 5.19.1 和 Unity `SceneManager`

Unity 里常见的是：

- `SceneManager.LoadScene`
- `SceneManager.LoadSceneAsync`

Wheatear 的 `SceneTransitionService` 更像是：

- 先把“想切场景”的意图记下来。
- 再由宿主统一执行。

它不是完全等价的，但思想上有点像“把加载请求放进受控流程里”。

### 5.19.2 和 UE `OpenLevel`

UE 常见是：

- `UGameplayStatics::OpenLevel`
- `Level Streaming`

Wheatear 的做法更轻：

- 没有复杂的世界分区系统。
- 没有把关卡流当作主战场。
- 只做适合简单 2D 项目的场景切换和存读档切入。

### 5.19.3 为什么这样更适合当前项目

因为你的项目目标不是大型 3D 开放世界。  
它是：

- 简单 2D
- 多玩法
- 编辑器驱动
- 方便教学和面试表达

所以 `SceneTransitionService` 的轻量请求队列就够用了。

## 5.20 这一层的好处

- 切场景逻辑集中
- 新游戏、读档、普通跳转共享同一套路
- 输入清理、命令清理、旧场景收尾都能统一做
- 宿主和玩法分开，职责清楚
- 方便调试场景问题
- 方便解释给面试官听

## 5.21 这一层的坏处

- 多了一层请求转发，代码路径更长
- 切换流程不如“直接调用一个函数”那么直观
- 要维护请求队列和宿主消费顺序
- 如果宿主层写得不好，问题会集中在这一层

但对工程化来说，这些代价是值得的。

## 5.22 常见误区

### 5.22.1 误区一：`scene:` 就是立即切

不是。  
它只是发了一个请求。

### 5.22.2 误区二：`SceneTransitionService` 负责加载 YAML

也不是。  
它只管请求；真正加载由宿主层调用 `SceneSerializer` 完成。

### 5.22.3 误区三：`LoadGame` 和 `LoadScene` 是一回事

不是。  
`LoadGame` 还要先恢复进度槽、设置上下文和检查能不能读。

### 5.22.4 误区四：清命令是多余的

不是。  
不清就可能把旧场景尾巴带进新场景。

### 5.22.5 误区五：进度系统和场景系统应该合并

不应该。  
进度是进度，场景是场景。  
它们有交点，但不是一个概念。

## 5.23 面试怎么讲

如果面试官问“为什么要单独做 SceneTransitionService”，可以这样答：

> 因为我不想把切场景散在各个玩法模块里。我的做法是先通过 `CommandBus` 或事件脚本发请求，再由 `SceneTransitionService` 统一收集，最后交给宿主层 `RuntimeSceneLayer` 真正落地。这样能统一清输入、清命令、停旧场景、启动新场景，也更容易和 `GameProgress` 对接。

如果面试官问“新游戏和读档怎么区分”，可以这样答：

> 我把它们分成 `SceneTransitionMode::NewGame` 和 `SceneTransitionMode::LoadGame`。新游戏会先 `ResetForNewGame()`，读档会先 `LoadSlot()`，然后再进入宿主层加载目标场景。这样进度语义和场景语义是分开的。

如果面试官问“你为什么不用直接切”，可以这样答：

> 因为直接切会把输入状态、命令队列和旧场景生命周期搞乱。请求队列能把变化收口到一帧末尾统一处理，代码更稳，调试也更方便。

## 5.24 给初学者的阅读顺序

如果你第一次看这一章，建议按这个顺序：

1. 先看 `SceneTransitionService.h`
2. 再看 `SceneTransitionService.cpp`
3. 再看 `CommandBus.cpp` 里 `scene:`、`newgame:`、`loadgame:` 的分支
4. 再看 `RuntimeSceneLayer.cpp` 的 `ConsumeSceneTransitionRequests()`
5. 再看 `RuntimeSceneLayer.cpp` 的 `ExecuteSceneTransitionRequest()`
6. 再看 `RuntimeSceneLayer.cpp` 的 `LoadScene()`
7. 最后看 `GameProgress.cpp` 里的 `ResetForNewGame()`、`SetSceneTransitionContext()` 和 `BuildLoadGameCommand()`

这样读下来，链路就顺了。

## 5.25 你可以马上做的三个练习

### 练习 1：追一条新游戏链路

从主菜单按钮开始，自己把这条链写出来：

```text
按钮
 -> event:
 -> newgame:
 -> SceneTransitionService
 -> RuntimeSceneLayer
 -> LoadScene
```

### 练习 2：追一条读档链路

从 `VisualNovelSystem::LoadFromSlot()` 开始，看看它为什么先调用 `BuildLoadGameCommand()`，再交给 `CommandBus`。

### 练习 3：找一处需要清理的状态

想一想如果不清这些东西会怎样：

- UI 输入状态
- 命令队列
- 上一场景的运行状态

你会更容易理解为什么 `LoadScene()` 现在要先做这些收尾。

## 5.26 本章小结

这一章最重要的结论可以缩成四句：

```text
SceneTransitionService 只发请求，
RuntimeSceneLayer 负责落地，
GameProgress 负责进度语义，
CommandBus / EventScript 负责把流程送到这里。
```

再缩一点就是：

```text
流程先发请求，场景最后统一切。
```

这章讲完后，你就能把“主菜单 -> 新游戏 -> 场景加载”、“战斗结束 -> 结果页”、“读取存档 -> 回到正确场景”这三条链说清楚了。

## 5.27 下一章

下一章建议写 **GameProgress**。

因为场景切换已经把入口和宿主讲清楚了，接下来就该补“进度到底保存了什么、怎么存、怎么读、怎么驱动章节和解锁内容”。

如果说这一章讲的是“场景怎么换”，  
那么下一章讲的就是“换过去之后，游戏世界为什么还记得你是谁”。
