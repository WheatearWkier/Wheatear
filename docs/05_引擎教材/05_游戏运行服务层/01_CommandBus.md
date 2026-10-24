# 第 5 编 01：CommandBus

> 本章所属层级：**游戏运行服务层**
>
> 本章目标：理解 Wheatear 如何把“按钮、剧情、场景、存读档、玩法命令”统一成一条命令链。
> 这一层不负责真正执行玩法规则，它负责把不同来源的请求整理成稳定、可调度、可追踪的命令。
>
> 本章会反复使用三个具体例子：
> 1. 主菜单按钮点击后，为什么会进入新游戏。
> 2. 战斗、VN、据点为什么都能共用同一条 `CommandBus`。
> 3. 一个 `event:` 命令为什么最后会落到 `EventScriptSystem`。

## 5.1 这一层在架构中的位置

如果把引擎按层看，`CommandBus` 不属于最底层的平台，也不属于最上层的玩法规则。它更像一条“运行时交通枢纽”：

```text
UI / VN / Scene / Combat / Editor
    ↓
CommandBus
    ↓
SceneTransitionService / GameProgress / EventScriptSystem / 各玩法系统
```

它解决的不是“游戏怎么画”，而是“**这个请求该交给谁**”。

这也是为什么它放在 **游戏运行服务层** 而不是核心平台层：  
核心平台层解决的是程序怎么跑起来；这里解决的是跑起来之后，游戏流程怎么被串起来。

### 5.1.1 先把四个容易混淆的概念分开

在阅读 `CommandBus`、`.wts` 和玩法代码时，最容易出现的误解是把所有“事件”都看成同一种东西。当前工程里至少要区分下面四层：

| 对象 | 主要职责 | 当前工程中的例子 |
| --- | --- | --- |
| `EventBus` | 分发底层、强类型的应用或输入事件 | 窗口关闭、键盘按下、鼠标移动 |
| `CommandBus` | 识别命令前缀，并把命令路由到正确的队列或服务 | `scene:...`、`event:...`、`side:basic` |
| `EventScriptSystem` / `.wts` | 根据事件名找到脚本块，按顺序推进低频流程 | `event menu_new_game:` |
| `WAO` / `ActionSignalRouter` | 解析动作意图、结算效果，并发出动作表现信号 | 攻击命中、受击、硬直、死亡 |

因此，准确的说法不是“WTS 监听了按钮点击”，而是：

```text
UIInputSystem 发现点击
    -> 读取 UIButtonComponent.OnClickFunction
    -> 调用 CommandBus::Execute(...)
    -> 只有命令是 event:... 时，才进入 EventScriptSystem
```

如果按钮直接写 `scene:`，它会走场景请求；如果写 `side:basic`，它会走玩法命令队列；这两种情况都不会经过 `.wts`。

## 5.2 本章要读的源码

建议按这几份源码一起看：

- `Wheatear/src/Wheatear/Runtime/CommandBus.h`
- `Wheatear/src/Wheatear/Runtime/CommandBus.cpp`
- `Wheatear/src/Wheatear/Runtime/SceneTransitionService.h`
- `Wheatear/src/Wheatear/Runtime/SceneTransitionService.cpp`
- `Wheatear/src/Wheatear/Modules/ModuleBootstrap.cpp`
- `Wheatear/src/Wheatear/Modules/VisualNovel/VisualNovelSystem.cpp`
- `Wheatear/src/Wheatear/Modules/Progression/ProgressionSystem.cpp`
- `Wheatear/src/Wheatear/Scripting/EventScriptSystem.cpp`
- `WheatearSandbox/src/RuntimeSceneLayer.cpp`

其中 `EventScriptSystem.cpp` 会在下一章详细讲，这一章先把 `CommandBus` 讲透。

## 5.3 先给结论：`CommandBus` 不是“万能消息总线”

Wheatear 里的 `CommandBus` 更准确地说是一个 **命令路由器 + 命令队列**。

它把命令分成几种：

- 立即能解释的命令，比如 `ui:pager:...`、`progression:...`、`anim:...`
- 要请求场景切换的命令，比如 `scene:`、`newgame:`、`loadgame:`
- 要交给事件脚本的命令，比如 `event:menu_new_game`
- 要留给玩法模块消费的命令，比如 `vn:`、`turn:`、`tactic:`、`side:`

所以它既不是单纯的发布订阅，也不是纯文本解释器，而是两者的混合体。

### 5.3.1 `CommandBus` 是路由器，不是事件监听器

`CommandBus` 不会主动观察鼠标、键盘、角色碰撞或玩家状态。它只有在某个系统明确调用 `Execute()`、`QueueGameplayCommand()` 或 `QueueEventCommand()` 后，才会处理已经产生的请求。

可以把职责分成两步：

```text
外部系统发现事实
    例如：按钮被点击、VN 选项被选择、战斗已经胜利

外部系统生成命令
    例如：event:go_skill_tree、side:basic、event:side_combat_victory

CommandBus 负责路由
    决定进入事件队列、玩法队列、场景请求或其他服务
```

这也是 `event:` 和普通玩法命令的根本区别：`event:` 不是“监听某个事件”，而是“已经发生了某件事，现在请求启动一个同名事件脚本”。

## 5.4 `CommandResult`：命令执行后的返回值

`CommandBus::Execute()` 的返回值不是一个布尔值，而是 `CommandResult`：

```cpp
struct CommandResult
{
    bool Handled = false;
    bool Success = false;
    bool Changed = false;
    std::string Message;
};
```

这四个字段很重要：

- `Handled`：这条命令有没有被识别
- `Success`：识别后是否执行成功
- `Changed`：有没有产生状态变化
- `Message`：失败或提示信息

这让上层可以区分三件事：

1. 命令写错了。
2. 命令识别了，但当前条件不允许。
3. 命令执行了，并且真的改了状态。

需要注意当前实现的一个边界：`EventScriptSystem::UpdateScript()` 调用 `CommandBus::Execute()` 时没有保存或检查返回的 `CommandResult`。因此，脚本中的命令即使没有被识别或执行失败，当前脚本也不会因为这个返回值自动中止；如果需要失败即停止，必须另外设计脚本错误策略。

## 5.5 `CommandBus` 的公开 API

核心接口只有几个：

- `Execute(Scene* scene, const std::string& command)`
- `RegisterGameplayCommandPrefix(const std::string& prefix)`
- `QueueRuntimeCommand(const std::string& command)`
- `DrainRuntimeCommands()`
- `QueueGameplayCommand(const std::string& command)`
- `DrainGameplayCommands(const std::string& prefix = {})`
- `DrainAllGameplayCommands()`
- `QueueEventCommand(const EventCommandRequest& request)`
- `DrainEventCommands(Scene* scene)`

其中 `EventCommandRequest` 很简单：

```cpp
struct EventCommandRequest
{
    UUID TargetEntity = 0;
    std::string EventName;
};
```

`TargetEntity = 0` 表示广播，非 0 表示精确找某个实体。

## 5.6 `Execute()` 的分发顺序

`CommandBus::Execute()` 会按顺序尝试不同处理器：

1. `ui:pager:` 类命令
2. `progression:` 类命令
3. `anim:` 类命令
4. `event:` 类命令
5. 玩法前缀命令
6. `scene:` / `newgame:` / `loadgame:` / `quit`

这个顺序不是随便排的。  
例如 `ui:pager:` 要优先于通用玩法命令，因为它属于通用 UI 控件；  
`event:` 也要单独拎出来，因为它不是“执行某个行为”，而是“把事件转交给脚本系统”。

## 5.7 四类最常见命令

### 5.7.1 场景命令

这类命令不直接加载场景，而是向 `SceneTransitionService` 发请求：

```text
scene:assets/scenes/Hub.wt
newgame:assets/scenes/Intro.wt
loadgame:assets/scenes/Intro.wt:1
```

它们的共同特点是：**先请求，后执行**。  
这样可以避免在 UI 回调、脚本执行中途直接销毁当前场景，导致状态不稳定。

### 5.7.2 进度命令

`progression:` 交给 `GameProgress` 处理，例如：

```text
progression:set_flag:FLAG_CH02_TUTORIAL_STARTED
progression:set_chapter:3
progression:select_skill_node:magic_sword_core
```

这类命令适合做剧情旗标、章节解锁、成长系统和材料变化。

### 5.7.3 UI 和动画命令

`ui:pager:` 和 `anim:` 这种命令属于“通用表现层命令”，直接修改某个实体上的 UI 或动画状态。

例如：

```text
ui:pager:@17382910492300000000:next
anim:play:@17382910492300000000:idle
```

这类命令的价值在于：  
按钮、剧情、脚本、调试工具都可以用同一套短命令，不必每个地方都写一套专门 API。

### 5.7.4 事件命令

`event:` 不执行逻辑本体，它只是向 `EventScriptSystem` 投递一个事件请求。

例如：

```text
event:menu_new_game
event:@17382910492300000000:open_skill_tree
```

前者是广播给当前场景里的事件脚本，后者是精确命中某个实体。

`event:` 本身不包含“等待点击”的逻辑，也不会主动寻找点击来源。它只把目标实体和事件名放进 `EventCommandQueue`，稍后由 `EventScriptSystem` 消费。

### 5.7.5 谁负责发现用户操作

以 UI 为例，真正的调用链是：

```text
鼠标按下
    -> UIInputSystem 做命中测试
    -> UIInputSystem::FireOnClick()
    -> 读取 UIButtonComponent.OnClickFunction
    -> CommandBus::Execute(scene, "event:go_skill_tree")
```

所以 `UIButtonComponent.OnClickFunction` 更像一个“命令入口配置”，而不是 WTS 自己注册的回调。`UIInputSystem` 负责发现点击，`CommandBus` 负责接收字符串，`EventScriptSystem` 只处理其中的 `event:` 请求。

## 5.8 一个完整链路：主菜单“新游戏”

这是最适合面试时讲的例子。

### 5.8.1 场景里的按钮

主菜单场景里，按钮写的是：

```text
OnClickFunction: "event:menu_new_game"
```

### 5.8.2 事件脚本

`assets/events/sandbox_flow.wts` 里对应的脚本是：

```text
event menu_new_game:
    newgame:assets/scenes/Intro.wt
end
```

### 5.8.3 `CommandBus` 做了什么

按钮点击不是 WTS 发现的，而是 `UIInputSystem` 先完成命中测试，再把 `OnClickFunction` 交给 `CommandBus`。

`CommandBus` 识别到 `event:` 后，只把 `menu_new_game` 塞进事件队列。
之后 `EventScriptSystem` 才把这个事件取出来，查找并执行脚本里的 `menu_new_game` 块。

### 5.8.4 真正切场景的是谁

`newgame:` 不是 `CommandBus` 自己切场景，而是：

```text
CommandBus::Execute(...)
    -> SceneTransitionService::RequestNewGame(...)
    -> RuntimeSceneLayer::ConsumeSceneTransitionRequests()
    -> LoadScene(...)
```

也就是说，`CommandBus` 只负责把“要新游戏”这件事表达清楚，真正的场景切换由宿主层完成。

### 5.8.5 如果按钮写的不是 `event:`

并不是所有按钮都需要经过 WTS：

```text
OnClickFunction: event:go_skill_tree
    -> CommandBus
    -> EventCommandQueue
    -> EventScriptSystem
    -> .wts 中的 go_skill_tree

OnClickFunction: scene:assets/scenes/Hub.wt
    -> CommandBus
    -> SceneTransitionService
    -> RuntimeSceneLayer

OnClickFunction: side:basic
    -> CommandBus
    -> GameplayCommandQueue
    -> SideCombatSystem
```

这三条路径都可以由 UI 触发，但只有第一条是“UI 触发 WTS 事件”。

## 5.9 为什么要有队列

`CommandBus` 里有三条队列：

- `RuntimeCommandQueue`
- `GameplayCommandQueue`
- `EventCommandQueue`

它们不是装饰，而是为了避免两个问题：

1. **重入**：命令执行时又立刻触发新的命令，容易把当前流程打断。
2. **时序**：有些命令必须等到合适的系统更新阶段再消费。

例如 `RuntimeSceneLayer` 会在自己的输入处理里统一消费运行时命令和场景切换请求，这样不会在按钮回调里直接乱切场景。

`EventCommandQueue` 还带来一个容易忽略的时序：事件不是同步嵌套调用。假设当前 WTS 执行到：

```text
event route_after_result:
    event:result_go_hub
end
```

当前 `event:result_go_hub` 只会再次入队。`EventScriptSystem::OnUpdateRuntime()` 已经在本帧开头完成了事件队列的 `Drain`，所以这个新请求通常要等下一次更新时才会被取出并启动。它更接近“下一批次启动另一个事件”，而不是 C++ 函数栈上的同步递归调用。

## 5.10 `DrainGameplayCommands()` 为什么还带前缀

`DrainGameplayCommands(prefix)` 会先把整批玩法命令取出来，再按前缀筛选。

这适合这种场景：

- `VisualNovelSystem` 只关心 `vn:`
- `ProgressionSystem` 只关心 `gamesave:`
- `TurnCombatSystem` 只关心 `turn:`
- `TacticalCombatSystem` 只关心 `tactic:`
- `SideCombatSystem` 只关心 `side:`

如果一次把所有命令都拆给各系统，会反复扫描队列。  
现在这种做法更像“先统一收口，再按模块分发”。

例如横板战斗中可以有：

```text
side:basic
side:launcher
side:item:1
```

但这不代表玩家的每一次攻击、受击和喝药都要先写进 WTS。键盘输入通常由 `InputBindingService` 直接驱动玩法动作；只有 UI、脚本或其他系统明确生成 `side:` 命令时，才会经过 `CommandBus` 的玩法队列。

## 5.11 逐函数阅读 `CommandBus.cpp`

这一节开始进入函数级阅读。  
如果前面讲的是“这层做什么”，这里就要看“它到底怎么做”。

### 5.11.1 `ExecuteUIPagerCommand()`

`ui:pager:` 是最能体现 `CommandBus` 价值的一类命令，因为它说明了一个事实：

- 按钮不是只能调业务函数
- UI 也可以被当成可寻址对象
- 命令文本里可以直接带目标和动作

它的处理顺序大致是：

1. 识别 `ui:pager:` 前缀
2. 用 `SplitCommand()` 把字符串拆成段
3. 解析 `@UUID` 选择器
4. 通过 `UIWidgetLayout::Context` 找到对应实体
5. 确认实体上真的有 `UIPagerComponent`
6. 根据 `next / prev / first / last / page / set` 修改分页状态

这说明 `CommandBus` 不是“只会发消息”，它还能做轻量的命令解释。

一个很实际的例子是结果页或技能树分页：

```text
ui:pager:@17382910492300000000:next
ui:pager:@17382910492300000000:page:2
```

这类命令非常适合 UI，因为它们短、稳定、可写进按钮、也方便调试。

### 5.11.2 `ExecuteProgressionCommand()`

`progression:` 是进度系统的入口。

这一层不自己知道怎么加旗标、怎么解锁技能、怎么记录章节，而是把事情交给 `GameProgress::ExecuteCommand()`。

这也是 `CommandBus` 一个很重要的设计：  
它既可以直接处理一些通用命令，也可以把“专有规则”交给更专业的模块。

比如：

```text
progression:set_flag:FLAG_CH02_TUTORIAL_STARTED
progression:set_chapter:3
progression:select_skill_node:magic_sword_core
```

这些命令都很像“剧情脚本语言”，但最终状态由 `GameProgress` 统一维护。

### 5.11.3 `ExecuteAnimationCommand()`

动画命令让 `CommandBus` 变得更像一个“运行时控制台”。

它支持的动作比较直观：

- `play`
- `restart`
- `pause`
- `resume`
- `stop`
- `seek`

这种命令适合调试、过场和轻量剧情触发。

比如：

```text
anim:play:@17382910492300000000:idle
anim:pause:@17382910492300000000
anim:seek:@17382910492300000000:0.5
```

这套逻辑做了几件事：

1. 通过 `EntityReferences::ResolveSelector()` 找目标实体
2. 检查实体上是否有 `SpriteAnimatorComponent`
3. 再决定具体动作

它告诉我们一个细节：  
`CommandBus` 不是不管类型，它其实非常依赖场景里的对象模型。

### 5.11.4 `ExecuteRuntimeQueuedCommand()`

这部分处理的是最关键的运行时命令：

- `quit`
- `scene:`
- `newgame:`
- `loadgame:`

这里最重要的点是：**不立即切场景**。

例如：

```text
scene:assets/scenes/Hub.wt
newgame:assets/scenes/Intro.wt
loadgame:assets/scenes/Intro.wt:1
```

它们会变成 `SceneTransitionService` 的请求。

为什么要这么做？

- 避免在按钮回调里直接销毁场景
- 避免在脚本执行中途切断当前流程
- 让宿主层统一决定什么时候真正加载

`loadgame:` 还会先检查当前场景的存读档策略，避免不允许读取的界面被误跳。

### 5.11.5 `ExecuteEventCommand()`

`event:` 是最容易和 `CommandBus` 混淆的一类命令。

它看起来像“命令”，但实际并不是执行动作本体，而是把事件名塞进 `EventCommandQueue`。

两种常见形式：

```text
event:menu_new_game
event:@17382910492300000000:open_skill_tree
```

第一种是广播，第二种是定点。

这使得 `.wts` 脚本和 UI 按钮都能用同一种风格表达“触发某个事件”，而真正响应它的是 `EventScriptSystem`。

### 5.11.6 `ExecuteGameplayCommand()`

玩法命令不是直接执行，而是先排进 `GameplayCommandQueue`。

它会先判断：

- 有没有注册过前缀
- 是否是内建玩法前缀

然后才决定是否接收。

在当前工程里，常见前缀是：

- `vn:`
- `gamesave:`
- `turn:`
- `tactic:`
- `side:`

这样做的好处是：

- 新玩法能注册自己的命令域
- 系统之间不会互相抢命令
- 命令名可以按模块分开管理

## 5.12 `SceneTransitionService` 与 `RuntimeSceneLayer`

这一层是理解 `CommandBus` 的关键配套。

`CommandBus` 只负责发请求，真正加载场景的是运行时宿主。

### 5.12.1 `SceneTransitionService`

它很简单，只有三种请求：

- `LoadScene`
- `NewGame`
- `LoadGame`

本质上就是一个场景切换请求队列。

### 5.12.2 `RuntimeSceneLayer::LoadScene()`

运行时场景层在真正加载场景前会先做几件事：

1. `UIInputSystem::Reset()`
2. `CommandBus::ClearQueuedCommands()`
3. 停止旧场景的 runtime
4. 反序列化新场景

这一步很重要，因为它说明队列不是主流程本体，只是过渡期缓存。

### 5.12.3 `ConsumeRuntimeSceneCommands()`

运行时宿主每帧会消费两类东西：

- 场景里的 gameplay runtime 命令
- `CommandBus` 的 runtime 命令

也就是说，命令可能来自按钮、脚本、组件、调试输入，最后都汇到宿主层统一处理。

### 5.12.4 为什么这比“直接加载”更稳

因为命令来源很多，直接加载会让控制流四处分叉。  
现在这种做法把“请求”和“执行”拆开了，所以更容易调试，也更容易把编辑器和正式包统一起来。

## 5.13 真实链路拆解：主菜单“新游戏”

这条链路是面试最值得讲的一条。

### 第一步：按钮发出命令

主菜单场景中的按钮写的是：

```text
OnClickFunction: "event:menu_new_game"
```

### 第二步：`CommandBus` 只收下请求

`CommandBus::Execute()` 识别 `event:`，把 `menu_new_game` 变成事件请求。

### 第三步：`EventScriptSystem` 接管

`sandbox_flow.wts` 里有：

```text
event menu_new_game:
    newgame:assets/scenes/Intro.wt
end
```

脚本执行到这里，会再次回到 `CommandBus`。

### 第四步：`newgame:` 变成场景切换请求

`CommandBus` 把它转成 `SceneTransitionService::RequestNewGame(...)`。

### 第五步：运行时宿主真正切场景

`RuntimeSceneLayer` 消费请求，重置状态、清队列、加载 `Intro.wt`。

这条链路很好讲，因为它清楚地体现了四层分工：

```text
UI -> CommandBus -> EventScriptSystem -> SceneTransitionService -> RuntimeSceneLayer
```

## 5.14 真实链路拆解：`side:`、`turn:`、`tactic:`

这类命令更适合讲“玩法命令的消费方式”。

### `side:` 的例子

在横板战斗里，按钮会写成：

```text
OnClickFunction: side:basic
```

`CommandBus` 会把它放进 gameplay 命令队列。  
然后 `SideCombatSystem::OnUpdateRuntime()` 再消费 `side:` 命令，把它转成 `InputBindingService::InjectActionPress()`。

这意味着：

- UI 按钮和键盘输入最终进入同一条动作链
- 玩法系统不需要区分命令是从哪来的

但这里要把“动作入口”和“战斗结算”分开看：

```text
side:basic
    -> GameplayCommandQueue
    -> SideCombatSystem
    -> InjectActionPress("side.basic")
    -> SideCombatPlayerService
    -> 生成攻击判定
    -> SideCombatHitResolutionService
    -> 伤害 / 命中 / 硬直 / 浮空 / 死亡
```

当前 tuning 中，物品命令也可以走同一入口，例如：

```text
side:item:1
```

`SideCombatSystem` 会根据战斗配置查找物品槽，再注入对应动作；当前示例配置里的第 1 格是治疗物品。这里的“喝药”是玩法系统执行的动作效果，不是 WTS 自动监听到的事件。

键盘或手柄输入则通常直接经过 `InputBindingService` 和横板战斗玩家服务，不必先转换成 `side:` 命令。敌人 AI 的攻击也由 `SideCombatEnemyAIService` 直接驱动。当前受击结算会记录 `PlayerWasHit`、累计受击次数并写入效果记录，但不会自动向 WTS 投递 `on_player_hit`。

如果未来确实需要“玩家每次受击都触发一段流程”，应在玩法层显式增加桥接，例如：

```cpp
CommandBus::QueueEventCommand({ flowEntity, "on_player_hit" });
```

这表示“受击结果已经被玩法系统确认，现在请求脚本处理后续流程”，而不是让 WTS 自己去轮询碰撞。

### `turn:` 和 `tactic:`

回合制和战棋也是同样思路：

- `TurnCombatSystem` 只消费 `turn:`
- `TacticalCombatSystem` 只消费 `tactic:`

这让玩法层保持清晰边界：  
玩法系统只认自己的命令前缀，不会被别的模块污染。

## 5.15 从源码看命令是怎样被识别的

这一节把 `CommandBus.cpp` 里几个辅助函数连起来看。

### 5.15.1 函数内静态对象：队列放在哪里

源码没有在 `CommandBus.h` 里暴露一个全局队列，而是在 `.cpp` 的匿名命名空间中提供函数：

```cpp
static std::vector<std::string>& RuntimeCommandQueue()
{
    static std::vector<std::string> commands;
    return commands;
}
```

其他队列也是同样结构：

```cpp
static std::vector<std::string>& GameplayCommandQueue();
static std::vector<EventCommandRequest>& EventCommandQueue();
```

这里有三个值得学习的 C++ 点：

1. `static` 局部对象只初始化一次。
2. 匿名命名空间让这些实现细节只在当前 `.cpp` 可见。
3. 外部只能通过 `CommandBus` 的公开 API 访问队列。

这是一种很轻量的封装方式。  
它没有引入一个复杂的 `CommandQueue` 类，但仍然避免了让外部直接修改内部容器。

### 5.15.2 `StartsWith()`：前缀就是命令域

Wheatear 的命令格式不是完整语法树，而是：

```text
域:动作:参数
```

例如：

```text
side:basic
turn:skill:player:arc_slash
progression:set_flag:FLAG_CH02_TUTORIAL_STARTED
```

所以当前实现大量使用前缀判断：

```cpp
StartsWith(command, "scene:")
StartsWith(command, "event:")
StartsWith(command, "turn:")
```

前缀的本质是一个命令命名空间。  
`turn:` 和 `side:` 不只是字符串开头不同，它们代表不同的消费系统和不同的规则域。

### 5.15.3 `SplitCommand()`：冒号是结构边界

对于需要多个参数的命令，源码使用 `SplitCommand()`：

```cpp
static std::vector<std::string> SplitCommand(const std::string& command)
{
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= command.size())
    {
        const size_t separator = command.find(':', start);
        if (separator == std::string::npos)
        {
            parts.push_back(command.substr(start));
            break;
        }

        parts.push_back(command.substr(start, separator - start));
        start = separator + 1;
    }
    return parts;
}
```

例如：

```text
ui:pager:@100:page:2
```

会得到：

```text
["ui", "pager", "@100", "page", "2"]
```

初学者要注意：这不是通用的 CSV 或 shell 解析器。  
如果未来参数本身允许冒号，就必须升级语法或引入转义规则。

### 5.15.4 `PayloadAfter()`：整段参数什么时候更合适

场景路径可能包含目录结构，所以场景命令不需要把所有内容重新切段：

```text
scene:assets/scenes/Hub.wt
```

源码使用 `PayloadAfter(command, "scene:")` 直接取得前缀之后的完整内容。

这体现了一个实际取舍：

- 结构化命令使用 `SplitCommand()`
- 路径型命令使用“取前缀后的剩余字符串”

否则路径、脚本名或未来的文本参数可能被错误拆开。

## 5.16 三条队列的语义和生命周期

虽然三条队列底层都是 `std::vector`，但它们的语义不一样。

### 5.16.1 `RuntimeCommandQueue`

用于交给宿主层的运行时命令，当前最典型的是：

```text
quit
```

场景切换命令不会直接进入这个队列，而是进入 `SceneTransitionService` 自己的请求队列。

因此不要把“所有延迟操作”都笼统地叫 runtime queue。

### 5.16.2 `GameplayCommandQueue`

用于玩法模块的命令：

```text
vn:...
turn:...
tactic:...
side:...
gamesave:...
```

它由玩法系统在 `OnUpdateRuntime()` 中消费。

### 5.16.3 `EventCommandQueue`

用于事件触发请求：

```cpp
struct EventCommandRequest
{
    UUID TargetEntity = 0;
    std::string EventName;
};
```

它不保存一条原始字符串，而是保存已经拆开的结构。

这一步很重要，因为事件目标和事件名已经成为运行时需要使用的两个独立字段。

## 5.17 `Drain` 为什么使用 `swap`

以运行时命令为例：

```cpp
std::vector<std::string> CommandBus::DrainRuntimeCommands()
{
    std::vector<std::string> commands;
    commands.swap(RuntimeCommandQueue());
    return commands;
}
```

这段代码完成了两件事：

1. 把旧队列的内容转移给局部变量 `commands`
2. 让全局队列变成一个空的 `vector`

它比逐个 `pop_front()` 更适合当前实现，因为 `vector` 不擅长头部删除。

### 5.17.1 为什么不直接返回引用

如果直接返回内部队列引用，消费方可能：

- 边遍历边追加
- 修改全局状态
- 让下一帧看到不完整的数据

`Drain` 返回一份独立的批次后，消费方就可以安全处理这一帧的快照。

### 5.17.2 复杂度

`swap` 通常只交换容器内部指针、大小和容量，属于常数级的队列交接。  
真正的遍历成本发生在消费方，而不是清空队列时。

### 5.17.3 这不是线程安全队列

`swap` 解决的是所有权和批次边界，不等于支持多线程。

当前 `CommandBus` 没有锁，也没有原子队列。  
它隐含的前提是：命令生产和消费发生在同一个游戏线程，或者外部已经做好同步。

面试时不要把它说成“线程安全消息总线”。

## 5.18 `DrainGameplayCommands(prefix)` 的细节

这个函数的实现可以分成三步：

```cpp
std::vector<std::string> all = DrainAllGameplayCommands();
```

先一次性拿走当前批次。

然后把命令拆成两组：

```cpp
if (StartsWith(command, prefix))
    matched.push_back(command);
else
    unmatched.push_back(command);
```

最后把不属于当前系统的命令放回队列：

```cpp
GameplayCommandQueue() = std::move(unmatched);
```

### 5.18.1 为什么不会丢掉别的玩法命令

假设队列里是：

```text
side:basic
turn:wait
tactic:move
```

`SideCombatSystem` 调用：

```cpp
DrainGameplayCommands("side:");
```

得到：

```text
side:basic
```

而 `turn:wait` 和 `tactic:move` 会被放回队列，等待各自的系统消费。

### 5.18.2 这种设计依赖什么前提

它依赖系统在同一帧内按约定消费命令。  
如果某个系统把所有 gameplay 命令都调用 `DrainAllGameplayCommands()` 拿走，其他系统就没有机会再看到这些命令。

因此，`DrainAllGameplayCommands()` 更像底层工具，常规玩法系统应该使用带前缀版本。

## 5.19 命令注册与模块启动

玩法命令前缀在 `ModuleBootstrap::RegisterDefaultGameplayModules()` 中注册：

```cpp
CommandBus::RegisterGameplayCommandPrefix("vn:");
CommandBus::RegisterGameplayCommandPrefix("gamesave:");
CommandBus::RegisterGameplayCommandPrefix("turn:");
CommandBus::RegisterGameplayCommandPrefix("tactic:");
CommandBus::RegisterGameplayCommandPrefix("side:");
```

### 5.19.1 为什么不把前缀写死在 `CommandBus`

因为 `CommandBus` 属于通用运行时层，不应该知道所有具体玩法。

如果把 `side:`、`turn:`、`vn:` 全部硬编码在 `CommandBus`：

- 新增玩法必须修改底层
- 底层会依赖越来越多上层模块
- 引擎和具体游戏的边界会变模糊

现在的方向是：

```text
底层提供注册能力
上层模块注册自己的命令域
```

### 5.19.2 注册函数的幂等性

`RegisterCommandPrefix()` 会先检查重复项：

```cpp
if (std::find(prefixes.begin(), prefixes.end(), prefix) != prefixes.end())
    return;
```

这让模块初始化重复调用时不会不断增加重复前缀。

## 5.20 `IsNativeCommand()` 的真实含义

`IsNativeCommand()` 名字容易让人误会。

它不是判断“这是不是 C++ 函数”，而是判断这条字符串是否属于引擎已经认识的命令域：

```cpp
bool CommandBus::IsNativeCommand(const std::string& command)
```

它会识别：

- `quit`
- `scene:`
- `newgame:`
- `loadgame:`
- `progression:`
- `anim:`
- `ui:`
- `event:`
- 已注册的玩法前缀

因此它更接近：

```text
IsKnownCommandDomain()
```

这个命名理解对以后阅读编辑器校验逻辑很有帮助。

还要注意：`CommandBus::IsNativeCommand()` 和 `.wts` 解析器里的 `IsBareCommand()` 不是同一个判断。

- `CommandBus::IsNativeCommand()` 能识别已注册的 `side:`、`turn:`、`tactic:`、`vn:` 等命令域。
- 当前 `.wts` 解析器允许直接写成裸命令的范围更窄，主要是 `quit`、`scene:`、`newgame:`、`loadgame:`、`progression:`、`gamesave:`、`ui:`、`event:`。
- 因此在 WTS 中使用 `side:`、`vn:`、`turn:`、`tactic:` 或某些 `anim:` 命令时，建议显式写成：

```text
command side:basic
command vn:advance
command anim:play:@17382910492300000000:idle
```

否则这些行可能被解析成 `RawLine`，在运行时保留但跳过执行。这里是“CommandBus 能不能识别”和“EventScript 解析器会不会把这一行识别成可执行指令”两个不同层次的问题。

## 5.21 设计好处和代价

### 好处

- UI、剧情、战斗、存档、切场景共用一条表达方式
- 玩法模块只认自己前缀，边界清楚
- 编辑器和运行时都能复用命令文本
- 新人很容易顺着命令链路理解流程
- 队列把命令生产和消费的时机分开

### 代价

- 命令命名必须统一
- 字符串调试不如强类型接口直接
- 时序要靠约定，不能随便在错误阶段消费
- 太多命令会让系统看起来像“字符串魔法”
- 当前队列不是线程安全结构
- 参数校验和错误提示需要持续维护

## 5.22 调试一条命令的阅读方法

当命令失效时，不要从头到尾盲目翻源码，可以按下面顺序排查。

### 第一步：确认命令文本

先确认实际字符串到底是什么：

```text
side:basic
event:menu_new_game
scene:assets/scenes/Intro.wt
```

多一个空格、少一个冒号、前缀大小写不同，都可能让命令无法识别。

### 第二步：确认入口

找是谁调用了：

```cpp
CommandBus::Execute(scene, command);
```

常见来源有：

- `RuntimeSceneLayer::ExecuteButtonCommand()`
- `EventScriptSystem::UpdateScript()`
- 某个玩法系统或 UI 服务

### 第三步：确认命令类别

根据前缀判断它应该进入哪里：

```text
event:       EventCommandQueue
side:        GameplayCommandQueue
scene:       SceneTransitionService
quit:        RuntimeCommandQueue
progression: GameProgress
```

### 第四步：确认消费方

例如：

- `side:` -> `SideCombatSystem`
- `turn:` -> `TurnCombatSystem`
- `event:` -> `EventScriptSystem`
- 场景请求 -> `RuntimeSceneLayer`

### 第五步：确认目标状态

如果命令被识别但没有效果，就看：

- 当前场景是否允许这个操作
- 目标实体是否存在
- 目标组件是否挂载
- 当前玩法系统是否已经启动
- 命令是否在场景切换前被清理

如果问题和 WTS 有关，还要额外确认：

- 触发来源是否真的调用了 `CommandBus::Execute()`
- 命令是否以 `event:` 开头；如果是 `scene:` 或 `side:`，它本来就不会经过 WTS
- `.wts` 中的目标事件名是否存在
- 脚本中的玩法命令是否用了显式 `command` 前缀
- 是否误认为 `wait` 会等待点击或受击；当前 `wait` 只等待时间

## 5.23 新人练习

### 练习一：给命令画分类表

把下面命令分别放进“立即处理、事件队列、玩法队列、场景请求”：

```text
event:go_hub
side:basic
scene:assets/scenes/Hub.wt
progression:set_chapter:3
quit
```

### 练习二：追踪 `side:basic`

从 `SideCombatBeastPath.wt` 的按钮配置开始，找到：

```text
OnClickFunction: side:basic
```

然后继续找：

```text
CommandBus::Execute
CommandBus::QueueGameplayCommand
SideCombatSystem::OnUpdateRuntime
InputBindingService::InjectActionPress
```

### 练习三：解释为什么不能在 `CommandBus` 里直接调用战斗函数

思考以下问题：

- `CommandBus` 是否应该 include `SideCombatSystem.h`？
- 如果所有玩法都直接写进 `CommandBus`，新增玩法会发生什么？
- UI 和 AI 是否还能够共享命令入口？

### 练习四：设计一个新命令

假设要增加：

```text
camera:shake:0.25:3
```

请先决定：

1. 它属于通用命令还是玩法命令？
2. 是立即修改相机，还是进入队列？
3. 参数解析失败时返回什么 `CommandResult`？
4. 它应该放在 `CommandBus`，还是单独的 `CameraCommandService`？

## 5.24 常见误区

### 误区一：`CommandBus` 就是 EventBus

不是。  
`EventBus` 解决的是事件分发，`CommandBus` 解决的是命令路由和流程调度。

### 误区二：`scene:` 应该立刻加载

也不是。  
它只是请求，真正加载由运行时宿主完成。

### 误区三：`event:` 会直接执行脚本

不对。  
它只是把事件塞进队列，等 `EventScriptSystem` 去消费。

### 误区四：所有命令都应该立即执行

不应该。  
`gameplay`、`event`、`scene transition` 这几类本来就有不同的消费时机。

### 误区五：WTS 会自己知道玩家什么时候点击、攻击或喝药

不会。
点击由 `UIInputSystem` 发现，VN 的下一句由 `VisualNovelSystem` 处理，横板攻击和物品使用由输入服务与玩法系统处理。WTS 只有在外部系统把一个 `event:` 请求投递给 `CommandBus` 后，才会启动对应事件块。

### 误区六：`wait` 可以等待用户输入

当前不可以。
`wait 0.5` 只会让脚本暂停 0.5 秒；它没有“等待按钮点击”“等待攻击命中”或“等待玩家选择”的语义。需要等待外部事实时，应由外部系统在事实发生后再次投递 `event:`。

### 误区七：`event:` 是同步调用另一个事件

当前也不是。
`event:` 会进入 `EventCommandQueue`，由下一次 `EventScriptSystem::OnUpdateRuntime()` 消费。它是排队触发，不是同步函数调用。

## 5.25 和 Unity / UE 的对照

Unity 里常见的做法是按钮回调、协程、ScriptableObject，或者自己再写一套流程管理器。  
UE 里则常见 Blueprint、Delegate、Gameplay Tag、Level Streaming。

Wheatear 的做法更轻：

- 用文本命令表达低频流程
- 用前缀区分模块
- 用宿主层统一消费

这特别适合一个以 2D 竖切为主、又要兼顾编辑器和运行时的小型引擎。

## 5.26 面试问答模板

### 问题：为什么不用一个纯事件系统？

因为这里不只是“谁监听谁”，还要表达“这个请求属于哪个子系统、什么时候执行、是否要延后”。命令比事件更适合串流程。

### 问题：为什么要有 `SceneTransitionService`？

因为切场景是高风险操作，不能在按钮回调或脚本执行中直接做。拆成请求和消费更稳。

### 问题：为什么 `event:` 要经过脚本而不是直接触发函数？

因为 `.wts` 让流程变成文本资产，编辑器可改、运行时可热重载，也更适合剧情和关卡流程编排。

### 问题：为什么玩法命令要按前缀分开？

因为横板、回合制、战棋、VN 的消费模型不同。前缀让它们既共享入口，又不互相干扰。

### 问题：WTS 怎么知道玩家什么时候点击了按钮？

它不知道，也不需要知道。
`UIInputSystem` 先完成命中测试，读取按钮上的 `OnClickFunction`，再调用 `CommandBus::Execute()`。只有当这个字符串是 `event:xxx` 时，事件队列才会把请求交给 `EventScriptSystem`。WTS 负责的是“事件被触发后按什么顺序做什么”，不是监听输入。

### 问题：玩家攻击、受击、喝药是不是都应该写成 WTS 事件？

通常不是。
这些是高频玩法动作，应由输入服务、玩家服务、敌人 AI 和命中结算系统处理。WTS 更适合处理战斗结果、奖励、流程跳转等低频逻辑。如果需要在某个受击结果后触发剧情，再由命中结算代码显式投递一个命名事件。

## 5.27 本章小结

这一章可以压缩成一句话：

```text
CommandBus 是运行时命令枢纽，
它把 UI、剧情、场景、存档和玩法请求统一收口，再分发给 SceneTransitionService、EventScriptSystem 和各玩法系统。
```

如果再压成一条链路，就是：

```text
外部系统发现操作或结果
    -> 生成 command / event request
    -> CommandBus
    -> EventScriptSystem（仅 event:）或玩法 / 宿主队列
    -> 下游系统执行
```

下一章就接着讲 **EventScript**。  
那一章会把 `.wts` 的语法、解析、运行时状态和热重载彻底拆开讲。
