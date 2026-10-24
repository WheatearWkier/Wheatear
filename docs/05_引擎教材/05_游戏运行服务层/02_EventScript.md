# 第 5 编 02：EventScript

> 本章所属层级：**游戏运行服务层**
>
> 本章目标：理解 Wheatear 的 `.wts` 事件脚本是怎么把低频流程串起来的。
> 它解决的不是“每帧怎么动”，而是“什么时候做什么”。
>
> 本章会反复使用三个具体例子：
> 1. 主菜单按钮怎么通过 `.wts` 进入新游戏。
> 2. 战斗胜利后为什么能继续跳到下一场景。
> 3. 为什么脚本改完后，运行时可以热重载。

## 5.1 这一层在架构中的位置

如果把 `CommandBus` 看成命令交通枢纽，那么 `EventScript` 就是“流程编排层”。

```text
场景启动 + RunOnStart
    -> EventScriptSystem::StartEvent(StartEvent)

UIInputSystem / VisualNovelSystem / 玩法结果 / C++
    -> 生成 event:xxx
    -> CommandBus
    -> EventCommandQueue
    -> EventScriptSystem::StartEvent(xxx)

EventScriptSystem
    -> .wts 中的 command / wait / if
    -> CommandBus / GameProgress / SceneTransitionService / 各玩法系统
```

它的职责很明确：

- 把按钮、剧情节点、战斗结果、门禁、过场串成脚本
- 把“什么时候发生什么”从 C++ 里挪出来
- 让编辑器可以直接维护文本流程

这里有一个必须说清楚的边界：`.wts` 不负责发现点击、按键、攻击或受击。外部系统先发现操作或结果，再生成一个 `event:` 请求；WTS 只负责请求到达之后的流程编排。

## 5.2 本章要读的源码

建议一起看这些文件：

- `Wheatear/src/Wheatear/Scripting/EventScript.h`
- `Wheatear/src/Wheatear/Scripting/EventScript.cpp`
- `Wheatear/src/Wheatear/Scripting/EventScriptSystem.h`
- `Wheatear/src/Wheatear/Scripting/EventScriptSystem.cpp`
- `Wheatear/src/Wheatear/Scene/Components/ScriptComponents.h`
- `Wheatear/src/Wheatear/Scene/Serialization/SceneSerializerScriptingComponents.cpp`
- `Wheatear/src/Wheatear/Runtime/CommandBus.cpp`
- `Projects/WheatearDemo/assets/events/sandbox_flow.wts`

## 5.3 `.wts` 到底是什么

`.wts` 是 Wheatear 自己的轻量事件脚本格式。

它不是通用脚本语言，也不是 Lua 替代品。  
它更像一份“可读、可回放、可热重载”的流程清单。

最适合它的内容是：

- 剧情节点
- 按钮流程
- 场景跳转
- 条件分支
- 等待
- 胜负后的流转

不适合它的内容是：

- 每帧移动
- 敌人 AI
- 复杂状态机
- 高频碰撞

### 5.3.1 WTS 不负责发现事件

可以把 `.wts` 理解成“事件名到流程块的映射表”，而不是一个一直监听输入的脚本线程：

```text
事件请求 event:go_skill_tree
    -> EventScriptSystem 找到 event go_skill_tree:
    -> 从第一条指令开始推进
```

因此，`.wts` 文件本身就是一张提前写好的“事件名 -> 流程指令列表”表：事件名和后续要执行的步骤在内容制作时已经写入文件，运行时只负责根据请求查找并推进对应的块。

要特别区分下面三种文本：

| 文本 | 所在位置 | 含义 |
| --- | --- | --- |
| `event go_skill_tree:` | `.wts` | 定义一个名为 `go_skill_tree` 的事件处理块 |
| `event:go_skill_tree` | UI、VN、玩法结果或 WTS 指令 | 请求启动这个事件处理块 |
| `OnClickFunction: event:go_skill_tree` | `.wt` 场景按钮配置 | 告诉 UI 点击后要发出什么命令 |

所以 WTS 不是运行时临时生成的，也不是通过轮询去猜“玩家现在想做什么”。如果某个事件名没有提前写在 `.wts` 中，`EventScriptSystem` 就找不到对应的流程块并发出警告。

以据点场景为例，`Hub.wt` 中的按钮配置可能是：

```text
OnClickFunction: event:go_skill_tree
```

完整链路是：

1. `UIInputSystem` 做鼠标命中测试，确认这个按钮被点击。
2. `UIInputSystem::FireOnClick()` 读取 `OnClickFunction`。
3. `CommandBus::Execute()` 识别 `event:go_skill_tree`，把它放入事件队列。
4. 下一次 `EventScriptSystem::OnUpdateRuntime()` 先取出事件请求。
5. 系统在 `sandbox_flow.wts` 中找到 `go_skill_tree` 事件块并开始执行。

如果按钮配置的是：

```text
OnClickFunction: scene:assets/scenes/SkillTree.wt
```

它会直接走 `CommandBus -> SceneTransitionService`，不会经过 WTS。`RunOnStart` 也属于另一条路径：场景启动时由 `EventScriptSystem` 直接调用 `StartEvent(StartEvent)`，不是等待玩家点击。

## 5.4 脚本长什么样

### 5.4.1 一个最小例子

`sandbox_flow.wts` 里最简单的例子之一是：

```text
event menu_new_game:
    newgame:assets/scenes/Intro.wt
end
```

### 5.4.2 更像教材的例子

再看一个战斗胜利后的流程：

```text
event fake_combat_victory:
    progression:set_flag:FLAG_CH01_FAKE_COMBAT_CLEARED
    wait 0.10
    scene:assets/scenes/TurnCombat.wt
end
```

这段脚本很典型：

- 先改进度
- 再等待一点点时间
- 再请求切场景

### 5.4.3 语法支持什么

解析器支持这些行型：

- `event <name>` / `event:<name>`
- `command ...` / `cmd ...`
- 裸命令，例如 `quit`、`scene:...`
- `wait 0.5`
- `if ...`
- `endif`
- `end` / `endevent`

不知道怎么解释的行不会直接丢掉，而是保留成 `RawLine`。

这里的“裸命令”不是指 `CommandBus` 能认识的所有命令。当前解析器允许直接写入 WTS 的主要是：

```text
quit
scene:...
newgame:...
loadgame:...
progression:...
gamesave:...
ui:...
event:...
```

对于 `anim:`、`vn:`、`side:`、`turn:`、`tactic:` 等命令，建议显式写成：

```text
command anim:play:@17382910492300000000:idle
command vn:advance
command side:basic
command turn:wait
command tactic:move:player:3:4
```

否则这一行可能会成为 `RawLine`：编辑器会保留它，但运行时会跳过它。解析器是否把一行识别成可执行指令，和 `CommandBus` 运行时是否认识这个前缀，是两个不同问题。

## 5.5 `EventScript` 解析器做了什么

`EventScript::FromFile()` 会先通过 `AssetPath::ResolveRuntimeData()` 找到运行时路径，然后读入文本，再交给 `Parse()`。

`Parse()` 里最值得记住的几件事：

1. 会按 `event` 分块。
2. 会保留注释行。
3. 会把无法理解的行原样保存。
4. 会记录每条指令的源行号。

这意味着 `.wts` 不是一次性丢给运行时的“黑盒字符串”，而是带有结构信息的文本资产。

## 5.6 `EventScriptInstruction` 的几种类型

解析后，脚本指令主要有几类：

- `Command`
- `Wait`
- `If`
- `EndIf`
- `RawLine`

其中最关键的是 `RawLine`。

它的意义是：  
编辑器回写脚本时，不要把人手写的未知内容悄悄删掉。  
这是一个很实用的工程取舍，虽然不“纯”，但很稳。

## 5.7 `EventScriptComponent`：脚本挂在哪

`.wts` 本身只是文本，真正挂到场景里的是 `EventScriptComponent`：

```cpp
struct EventScriptComponent
{
    std::string ScriptPath = "";
    std::string StartEvent = "on_start";
    bool RunOnStart = true;
    bool RunOnce = true;
    bool Enabled = true;

    bool RuntimeActive = false;
    bool RuntimeCompleted = false;
    bool RuntimeStarted = false;
    std::string RuntimeEventName;
    size_t RuntimeInstructionIndex = 0;
    float RuntimeWaitRemaining = 0.0f;
};
```

可以把它理解成：

- `ScriptPath`：脚本文件在哪
- `StartEvent`：默认从哪个事件开始
- `RunOnStart`：场景一启动要不要自动跑
- `RunOnce`：跑完后要不要只执行一次
- 后面那一串 `Runtime*`：运行时状态

很多流程控制实体会叫 `FlowController`，本质上就是挂了这个组件。

## 5.8 `EventScriptSystem` 是怎么跑的

运行时的主流程有三步：

### 5.8.1 `OnRuntimeStart()`

场景开始时，系统会重置所有脚本组件的运行态。  
如果组件启用了 `RunOnStart`，就会直接从 `StartEvent` 启动。

### 5.8.2 `OnUpdateRuntime()`

每帧更新时，系统会先处理 `CommandBus::DrainEventCommands(scene)`。

这表示：

1. 某个 UI 或玩法发出了 `event:xxx`
2. `CommandBus` 把它存成事件请求
3. `EventScriptSystem` 再按目标实体启动对应事件

事件队列是在推进活动脚本之前清空的。于是，如果某个正在执行的 WTS 又发出：

```text
event:another_event
```

这个新请求会进入队列，但不会在当前这次 `DrainEventCommands()` 中立刻被取出，通常要等下一次更新再启动。

父事件在发出 `event:` 后会暂停；下一次更新把子事件压入同一个组件的 `RuntimeFrames` 调用栈，子事件结束后父事件再从下一条指令继续。这样就不是“子事件覆盖父事件”，也不是同步跳进另一个事件块。

处理完本批事件后，系统才会继续推进每个正在执行的脚本。广播 `event:name` 只会在实际包含该事件的组件上启动，不会对场景中每个组件刷 “event not found”。

### 5.8.3 `UpdateScript()`

每条指令按顺序执行：

- `Command`：交给 `CommandBus::Execute(scene, instruction.Text)`；失败会记录脚本路径、行号和原因
- `event:`：把子事件压入调用栈，父事件暂停；同一事件已在栈中时拒绝递归，栈深上限为 8
- `scene:` / `newgame:` / `loadgame:`：成功后清空当前组件的调用栈
- `Wait`：暂停当前栈帧若干秒
- `If`：条件满足才继续，否则跳到匹配的 `EndIf`
- `EndIf`：单纯往后走
- `RawLine`：跳过，但不执行

## 5.9 解析器到底怎么读 `.wts`

这一节先不看运行时，先看文本是怎么变成结构的。

### 5.9.1 入口：`FromFile()` 和 `Parse()`

`EventScript::FromFile()` 先做两件事：

1. 通过 `AssetPath::ResolveRuntimeData()` 找到运行时路径
2. 读入整份文本，再交给 `Parse()`

这意味着 `.wts` 不是“硬编码字符串”，而是标准资产。

### 5.9.2 识别事件块

解析器支持两种事件头：

```text
event menu_new_game:
event:menu_new_game
```

不管哪一种，都会被归一成一个 `EventScriptBlock`。

### 5.9.3 指令识别顺序

每一行会按顺序试：

1. `command` / `cmd`
2. 裸命令，例如 `quit`、`scene:`、`event:`
3. `wait`
4. `if`
5. `endif`
6. 其他内容 -> `RawLine`

这个顺序很重要，因为 `.wts` 的目标不是做完整语言，而是做一套可维护的流程语法。

### 5.9.4 注释和空行

脚本里最容易被忽略的是注释处理。

- `#` 和 `//` 都会被保留
- 事件前的注释会成为文件头或块头
- 指令前的注释会跟着指令走

这不是小修小补，而是为了让编辑器回写时不把人写的说明弄丢。

### 5.9.5 `RawLine` 的意义

识别不了的行不会被删掉，而是保存成 `RawLine`。

这是一个非常工程化的决定：

- 运行时不执行它
- 编辑器可以原样写回
- 未来扩语法时不会把旧内容全炸掉

所以 `GetUnrecognizedLineCount()` 不是摆设，它是给编辑器和排错工具看的。

## 5.10 事件块和指令的数据模型

### 5.10.1 `EventScriptBlock`

一个事件块包含：

- 名称
- 源行号
- 指令列表
- 块头注释

这说明脚本不只是“事件名 + 函数体”，它还保留了源码位置信息。

### 5.10.2 `EventScriptInstruction`

一条指令包含：

- 类型
- 文本
- 秒数
- 源行号
- 前置注释

这样的设计很适合做：

- 编辑器高亮
- 错误定位
- 文本回写
- 运行时调试

### 5.10.3 为什么要保留源行号

因为脚本出错时，最该告诉人的不是“解析失败”，而是“第几行出了什么问题”。

这和编译器思路一样，都是为了让内容作者更快定位问题。

## 5.11 `EventScriptComponent`：脚本挂在哪

脚本不是自己在空中运行，必须挂在场景实体上。

```cpp
struct EventScriptComponent
{
    std::string ScriptPath = "";
    std::string StartEvent = "on_start";
    bool RunOnStart = true;
    bool RunOnce = true;
    bool Enabled = true;

    bool RuntimeActive = false;
    bool RuntimeCompleted = false;
    bool RuntimeStarted = false;
    std::string RuntimeEventName;
    size_t RuntimeInstructionIndex = 0;
    float RuntimeWaitRemaining = 0.0f;
};
```

可以把它分成两半来看：

- 上半部分是配置
- 下半部分是运行时状态

### 5.11.1 `RunOnStart`

如果为 true，场景一启动就会尝试从 `StartEvent` 开始跑。

### 5.11.2 `RunOnce`

如果为 true，执行完一次后就不会再重复触发。

### 5.11.3 `Runtime*` 字段

这些字段记录：

- 当前是不是在跑
- 跑到哪个事件了
- 跑到哪条指令了
- 还要等多久

所以 `EventScriptComponent` 本质上就是一个轻量状态机。

## 5.12 `EventScriptSystem`：运行时怎么推进

### 5.12.1 `OnRuntimeStart()`

场景开始时，系统会把所有脚本组件重置到初始状态。

如果组件允许自动启动，就调用 `StartEvent()`。

### 5.12.2 `OnRuntimeStop()`

场景结束时，系统会把运行态清掉，避免旧场景状态残留到下一场景。

这和 `RuntimeSceneLayer::LoadScene()` 的清理逻辑是配套的。

### 5.12.3 `OnUpdateRuntime()`

每帧更新时，系统会先处理 `CommandBus::DrainEventCommands(scene)`。

这意味着：

```text
event:xxx
    -> CommandBus 队列
    -> EventScriptSystem
    -> StartEvent(xxx)
```

然后再推进当前正在跑的脚本。

## 5.13 指令执行是怎么循环的

### 5.13.1 `Command`

`Command` 直接回到 `CommandBus::Execute(scene, instruction.Text)`。

这就是 `.wts` 和命令系统真正接起来的地方。

### 5.13.2 `Wait`

`wait` 不阻塞线程，只是让当前脚本暂停若干秒。

所以它更像“下一帧继续”，不是睡眠。

### 5.13.3 `If`

条件成立就继续，不成立就跳到匹配的 `EndIf` 后面。

这让 `.wts` 可以做很轻的分支控制。

### 5.13.4 `EndIf`

它本身不做事，只是把控制流继续往后推。

### 5.13.5 `RawLine`

保留但不执行。

这是编辑器友好性的关键。

### 5.13.6 一次 `UpdateScript()` 的实际推进方式

脚本并不是启动后从头到尾阻塞执行，而是由 `RuntimeInstructionIndex` 记录当前位置，每次更新从这个位置继续：

```text
如果 RuntimeWaitRemaining > 0
    扣除本帧 deltaSeconds
    如果仍然大于 0，结束本帧更新

while RuntimeInstructionIndex < 当前事件的指令数量
    取出当前指令

    Command
        CommandBus::Execute(scene, instruction.Text)
        指令下标 + 1
        继续看下一条

    Wait
        指令下标 + 1
        设置 RuntimeWaitRemaining
        如果等待时间 > 0，结束本帧更新

    If
        条件成立：下标 + 1
        条件失败：跳到匹配的 EndIf

    EndIf / RawLine
        指令下标 + 1

循环结束
    RuntimeActive = false
    RuntimeCompleted = true
```

因此普通命令可能在同一帧连续执行，而 `wait` 才会把控制权交给后续帧。例如：

```text
event demo:
    progression:set_flag:FLAG_A
    progression:set_flag:FLAG_B
    wait 1.0
    progression:set_flag:FLAG_C
end
```

通常第一帧会设置 `FLAG_A` 和 `FLAG_B`，然后停在 `wait 1.0`；约 1 秒后才继续设置 `FLAG_C`。

### 5.13.7 `wait` 不是等待用户输入

当前 `wait` 只有“等待一段时间”的语义：

```text
wait 0.5
```

它不能表示：

- 等待玩家点击 UI
- 等待玩家按下攻击键
- 等待某个敌人被击中
- 等待 VN 玩家点下一句

这些事实必须由对应的外部系统发现。事实发生后，外部系统可以再次调用 `CommandBus`，例如投递 `event:on_player_hit`，由 WTS 启动后续流程。WTS 本身不会轮询输入或碰撞。

## 5.14 条件语言怎么工作

`EvaluateCondition()` 读取的是 `GameProgress::GetState()`。

所以脚本可以判断：

- `flag`
- `skill`
- `dungeon`
- `completed`
- `last_dungeon`
- `equipment`
- `equipped`
- `chapter`
- `material`

还支持：

- `always`
- `never`
- `not`

### 5.14.1 为什么没有 `else`

因为这是轻量脚本，不是通用编程语言。  
如果要复杂分支，就拆成多个事件块更清楚。

### 5.14.2 `FindMatchingEndIf()`

这个函数用深度计数去找匹配的 `EndIf`。

它能处理嵌套 `if`，避免简单跳行导致结构错乱。

## 5.15 热重载与编辑器回写

### 5.15.1 按修改时间缓存

`EventScriptSystem` 会缓存脚本和最后写入时间。  
文件改了就重新加载。

### 5.15.2 为什么这很重要

因为流程脚本最常见的工作方式就是：

1. 改一行
2. 运行
3. 看结果
4. 再改

没有热重载，这个循环会很慢。

### 5.15.3 为什么要保留注释和原始行

因为编辑器不是只给机器看，它也是给内容作者看的。  
保留注释、RawLine、源行号，才能保证文本往返不丢信息。

## 5.16 真实链路拆解：主菜单到新游戏

### 第一步：按钮发出 `event:`

主菜单按钮配置是：

```text
OnClickFunction: "event:menu_new_game"
```

### 第二步：脚本接住事件

`sandbox_flow.wts` 里对应的是：

```text
event menu_new_game:
    newgame:assets/scenes/Intro.wt
end
```

### 第三步：脚本再发命令

`newgame:` 会回到 `CommandBus`，再转成 `SceneTransitionService` 请求。

### 第四步：运行时宿主落地

`RuntimeSceneLayer` 消费切场景请求，清队列、停旧场景、反序列化新场景。

所以这条链路可以写成：

```text
按钮
 -> UIInputSystem::FireOnClick()
 -> event:menu_new_game
 -> CommandBus / EventCommandQueue
 -> EventScriptSystem
 -> newgame:assets/scenes/Intro.wt
 -> SceneTransitionService
 -> RuntimeSceneLayer
 -> LoadScene()
```

按钮点击是 UI 系统发现的，WTS 只从 `event:menu_new_game` 开始接手。

### 5.16.1 VN 点击“下一句”

VN 的普通“点击继续”并不经过 WTS。当前实现的链路是：

```text
鼠标或 vn.advance 输入
    -> VisualNovelSystem::UpdateInput()
    -> VisualNovelRuntime::Advance()
    -> VN 内部推进到下一行
```

这是 VN 自己的高频交互状态，不需要为每一句话都生成一个 WTS 事件。

但 VN 选项可以把流程交给外部命令。例如：

```text
@choice 进入第一章假玩法 -> event:go_fake_combat | 先吐槽两句 -> roast
```

选择 `event:go_fake_combat` 时，VN 系统把这个外部命令记录到 `RuntimeRequestedCommand`，之后再由场景运行时交给 `CommandBus`；选择 `roast` 时，则仍然是 VN 运行时内部跳转到标签。也就是说，VN 同时支持“内部剧情跳转”和“跨系统流程请求”，但只有后者会进入 WTS。

### 5.16.2 横板战斗中的攻击、受击和喝药

横板战斗也不是“所有动作都通过 WTS”：

```text
键盘 / 手柄输入
    -> InputBindingService
    -> SideCombatPlayerService

UI 或 WTS 发出 side:basic
    -> CommandBus / GameplayCommandQueue
    -> SideCombatSystem
    -> InjectActionPress("side.basic")
    -> SideCombatPlayerService
```

之后两条路径会汇合到动作、攻击判定和命中结算。攻击、受击、硬直、浮空、死亡属于高频玩法规则，由横板战斗服务和 `SideCombatHitResolutionService` 处理；当前命中结算会记录 `PlayerWasHit`、受击次数和效果账本，但不会自动生成 `event:on_player_hit`。

物品也类似：`side:item:1` 先由 `SideCombatSystem` 根据配置找到第 1 个物品槽，再注入对应动作。当前示例配置中第 1 格是治疗物品，但“使用治疗物品”仍然是玩法动作，不是 WTS 监听到的事件。

战斗胜利则属于低频结果，适合桥接到 WTS：战斗结果系统在延迟后发出场景配置里的 `event:side_combat_victory`，WTS 再负责设置旗标、等待和决定返回哪个场景。

## 5.17 真实链路拆解：战斗胜利后的返回

这条链路更适合讲“脚本怎么和玩法结果结合”。

在 `sandbox_flow.wts` 里，胜利流程大致是：

```text
event side_combat_victory:
    if last_dungeon CH02_MAIN_BearAwakening
        progression:set_flag:FLAG_CH02_TUTORIAL_CLEARED
    endif
    wait 0.10
    scene:assets/scenes/Result.wt
end
```

而战斗系统里会把胜利命令设置成：

```text
event:side_combat_victory
```

当前实现中，`SideCombatSystem` 在关卡运行态初始化时把 `VictorySceneCommand` 设置为这个字符串；`SideCombatOutcomeService` 等待结算延迟结束后，再通过 `GameplayFlowService` 发出它。于是 WTS 不是每帧检查“敌人是否死光”，而是在玩法系统已经确认胜利后，接收一个低频结果请求。

### 5.17.1 这说明了什么

- 战斗系统只管“胜利了”
- 具体回到哪里由脚本决定
- 剧情旗标也由脚本控制

这样战斗逻辑和流程逻辑就分开了。

### 5.17.2 失败重试也是同理

`side_combat_retry`、`turn_combat_retry`、`result_retry_last_dungeon` 都是同一套路：

- 结果系统给一个事件
- `.wts` 决定重试到哪里
- 命令系统负责执行跳转

### 5.17.3 当前实现没有自动的 `on_*` 事件桥

当前 `EventScriptSystem` 能直接处理的指令类型只有：

```text
Command / Wait / If / EndIf / RawLine
```

它没有内建“按钮一点击就自动调用 `on_click`”“玩家一受击就自动调用 `on_player_hit`”这样的映射。是否进入 WTS，取决于外部系统有没有显式发出 `event:` 请求。

以横板受击为例，当前代码的事实是：

```text
敌人攻击命中玩家
    -> SideCombatHitResolutionService
    -> HitResolutionResult.PlayerWasHit = true
    -> RuntimePlayerHitsTaken 增加
    -> 写入 PlayerHitTaken 效果记录
```

到这里流程就结束了，并不会自动生成 WTS 事件。若以后需要受击触发剧情，可以在确认命中结果的位置增加桥接：

```text
SideCombatHitResolutionService
    -> CommandBus::QueueEventCommand({ flowEntity, "on_player_hit" })
    -> EventScriptSystem 下一次更新时启动 on_player_hit
```

这是一项明确的扩展设计，而不是当前 WTS 已经具备的能力。

另外，`EventScriptSystem` 当前忽略了 `CommandBus::Execute()` 返回的 `CommandResult`。所以脚本中的命令失败不会自动让脚本进入错误状态；如果面试时被问到错误处理，应把它作为当前实现边界说清楚。

## 5.18 把 `EventScriptSystem` 看成一个状态机

如果只把 `.wts` 理解成“按顺序执行几行命令”，很容易漏掉最关键的一点：

> 脚本的执行进度保存在 `EventScriptComponent` 里，它不是一次函数调用，而是跨多个游戏帧持续推进的状态机。

### 5.18.1 一次运行需要保存什么

至少需要保存：

- 当前事件名：`RuntimeEventName`
- 当前指令下标：`RuntimeInstructionIndex`
- 是否正在运行：`RuntimeActive`
- 是否已经完成：`RuntimeCompleted`
- 等待剩余时间：`RuntimeWaitRemaining`

如果没有这些字段，系统就无法在下一帧知道自己上次执行到了哪里。

### 5.18.2 一帧内可能执行多少条指令

`UpdateScript()` 使用 `while` 循环推进指令：

```cpp
while (component.RuntimeInstructionIndex < instructions.size())
{
    const EventScriptInstruction& instruction =
        instructions[component.RuntimeInstructionIndex];
    // 根据指令类型推进状态
}
```

因此：

- 普通命令可能在同一帧连续执行
- `if` 和 `endif` 可能在同一帧跳过
- `wait` 才会把执行让给后续帧

例如：

```text
event demo:
    progression:set_flag:FLAG_A
    progression:set_flag:FLAG_B
    wait 1.0
    progression:set_flag:FLAG_C
end
```

第一帧通常会完成 `FLAG_A` 和 `FLAG_B`，然后停在 `wait 1.0`；约一秒后才继续设置 `FLAG_C`。

### 5.18.3 `wait 0` 的特殊情况

如果等待时间是 `0`，系统不会真正停一帧，而是继续推进后面的指令。

这很适合把配置统一写成 `wait`，但也意味着脚本作者不能把 `wait 0` 当作“强制换帧”。

### 5.18.4 等待的时间从哪里来

运行时调用：

```cpp
const float deltaSeconds = ts.GetSeconds();
UpdateScript(scene, entity, deltaSeconds);
```

等待时间会被逐帧减去：

```cpp
component.RuntimeWaitRemaining =
    std::max(0.0f, component.RuntimeWaitRemaining - deltaSeconds);
```

所以它受到帧时间影响，但不会阻塞主线程。

## 5.19 条件执行的详细过程

条件是 `.wts` 从“命令列表”变成“流程脚本”的关键。

### 5.19.1 条件表达式如何求值

条件先经过 `Logic::NormalizeCondition` 做历史语法兼容，再交给共享表达式求值器：

```text
flag CH01              -> flag:CH01
material iron >= 3     -> material:iron >= 3
not flag CH01          -> not (flag:CH01)
```

求值器支持比较、算术、括号、`and` / `or` / `not`，以及 `min` / `max` / `clamp` / `abs` / `round` / `floor` / `ceil` / `if`。标识符可以带 `:`、`.`、`_`，因此进度谓词和普通变量共用一套语法。

### 5.19.2 条件如何读取游戏状态

`EvaluateCondition()` 通过 `GameProgress::GetState()` 读取进度状态。

例如：

```text
if flag FLAG_CH02_TUTORIAL_STARTED
    ...
endif
```

本质上是在查询：

```cpp
state.StoryFlags.count("FLAG_CH02_TUTORIAL_STARTED") > 0
```

实际求值由 `Logic::ResolveGameplayPredicate` 统一处理 `chapter`、`flag:`、`skill:`、`dungeon:`、`completed:`、`last_dungeon:`、`equipment:`、`equipped:` 和 `material:`；未识别的标识符返回 0，不会抛异常。非法表达式会在加载和保存时被 `EventScript::Validate` 拦下，并写入 `Diagnostics -> Logic Diagnostics`。

`chapter` 和 `material` 还会进一步解析比较运算符：

```text
if chapter >= 3
if material wood > 10
```

### 5.19.3 条件失败时如何跳转

条件不成立时，系统不会一条一条执行到 `endif`，而是调用：

```cpp
FindMatchingEndIf(instructions, currentIndex)
```

这个函数用一个深度计数器处理嵌套条件：

```text
if A
    if B
        ...
    endif
endif
```

当内部 `if` 出现时深度加一，遇到 `endif` 时先消化内层，再确定外层结束位置。

### 5.19.4 为什么没有 `else`

当前实现没有 `else` 指令，所以不要把它当成完整编程语言。

如果需要分支，可以写成多个互斥条件：

```text
event result:
    if last_dungeon CH02_MAIN_BearAwakening
        scene:assets/scenes/Result.wt
    endif
    if last_dungeon CH02_MAT_BeastPath
        scene:assets/scenes/Result.wt
    endif
end
```

这虽然不如 `else if` 简洁，但解析器更简单，脚本行为也更容易预测。

## 5.20 事件目标、广播和多个脚本组件

### 5.20.1 广播事件

下面这条命令不指定实体：

```text
event:menu_new_game
```

`CommandBus` 会把 `TargetEntity` 设成 `0`。

`EventScriptSystem` 遍历当前场景中的所有 `EventScriptComponent`，所有匹配的组件都会尝试启动这个事件。

### 5.20.2 定点事件

下面这条命令指定 UUID：

```text
event:@17382910492300000000:open_skill_tree
```

只有拥有对应 `IDComponent` 的实体会响应。

### 5.20.3 为什么大型流程更适合定点事件

广播很方便，但场景里如果有多个流程控制器，可能出现：

- 多个脚本同时启动
- 同一个场景请求被发出多次
- 不同模块互相影响

所以：

- 全局唯一入口可以用广播
- 多控制器场景应优先使用 `@UUID`

### 5.20.4 当前实现的隐含前提

`CommandBus` 的事件队列是进程级静态队列，不是每个 `Scene` 自己拥有的队列。

当前运行端的隐含前提是：

```text
同一时刻主要只有一个活动运行时场景
场景切换前会清空旧命令
```

这也是 `RuntimeSceneLayer::LoadScene()` 调用 `CommandBus::ClearQueuedCommands()` 的原因之一。

## 5.21 脚本生命周期和边界情况

### 5.21.1 找不到脚本文件

`EventScript::FromFile()` 打不开文件时，会记录警告并返回一个空脚本。

结果是：

- `GetEvents()` 为空
- `FindEvent()` 找不到目标
- `EventScriptSystem` 不会继续执行

对内容作者来说，最重要的排查信息就是脚本路径是否正确。

### 5.21.2 找不到事件名

如果组件要求启动：

```text
menu_new_game
```

但脚本里没有这个事件，系统会发出警告：

```text
event 'menu_new_game' not found
```

脚本组件不会进入正常执行状态。

### 5.21.3 `RunOnce` 的实际效果

当 `RunOnce` 为 true 且 `RuntimeCompleted` 已经为 true 时，`StartEvent()` 会直接返回。

因此：

- 一次性剧情节点适合开启 `RunOnce`
- 可重复按钮、流程控制器和重试入口通常要关闭 `RunOnce`

不能只看组件的默认值，还要看 `.wt` 场景里的实际配置。

### 5.21.4 场景切换时脚本会怎样

场景切换前，旧场景会执行 `OnRuntimeStop()`，所有脚本的活动状态都会被清理。

新场景加载完成后，新场景的 `OnRuntimeStart()` 再决定哪些脚本自动启动。

因此 `.wts` 不是跨场景常驻线程，它依附于场景生命周期。

### 5.21.5 热重载后执行位置的限制

热重载会更新脚本内容，但当前组件保存的是指令下标，不是稳定的指令 ID。

如果运行中修改了脚本前面的行，原来的下标可能对应到另一条指令。  
这说明当前热重载适合调流程，但还不是完整的“运行中无缝迁移”系统。

这是一个值得面试时主动说明的边界：

> 当前热重载解决的是重新读入脚本，不保证脚本结构变化时保持原来的语义执行位置。

## 5.22 用日志和源码定位脚本问题

当某个事件没有效果时，可以按这条路径排查。

### 第一步：确认场景组件

检查场景里的实体是否真的有：

```yaml
EventScriptComponent:
  ScriptPath: "assets/events/sandbox_flow.wts"
  RunOnStart: false
  RunOnce: false
  Enabled: true
```

### 第二步：确认按钮或玩法发出的文本

例如：

```text
event:side_combat_victory
```

注意 `event:` 后的名字必须和脚本里的事件名一致。

### 第三步：确认命令有没有进入事件队列

看 `CommandBus::Execute()` 是否进入了：

```cpp
ExecuteEventCommand(scene, command)
```

### 第四步：确认脚本系统有没有消费

看 `EventScriptSystem::OnUpdateRuntime()` 是否从：

```cpp
CommandBus::DrainEventCommands(scene)
```

拿到了请求。

### 第五步：确认事件块和指令

看：

```cpp
script->FindEvent(eventName)
```

是否找到了事件块，以及当前指令下标是否在有效范围内。

### 第六步：确认下游命令

如果事件已经启动但没有结果，就继续看脚本里的命令是否被 `CommandBus` 识别。

例如：

```text
newgame:assets/scenes/Intro.wt
```

可能已经被事件系统执行，但场景请求后续没有被宿主消费。

## 5.23 新人练习

### 练习一：手工模拟一帧

模拟下面脚本每一帧的状态：

```text
event demo:
    progression:set_flag:FLAG_A
    wait 0.5
    progression:set_flag:FLAG_B
end
```

记录：

- `RuntimeInstructionIndex`
- `RuntimeWaitRemaining`
- `RuntimeActive`
- 哪一帧设置了 `FLAG_A`
- 哪一帧设置了 `FLAG_B`

### 练习二：模拟条件跳转

分析下面脚本在条件真假两种情况下会执行哪些命令：

```text
event reward:
    if material wood >= 10
        progression:set_flag:CAN_CRAFT
        progression:set_chapter:2
    endif
    progression:set_flag:CHECKED
end
```

### 练习三：找出广播风险

假设一个场景里有两个 `FlowController`，它们都挂了同一个脚本。

思考：

```text
event:go_hub
```

会发生什么？如何改成只触发其中一个？

### 练习四：设计一个脚本校验器

编辑器可以在保存前检查：

- 事件名是否为空
- `if` 和 `endif` 是否配对
- `wait` 参数是否是合法数字
- `event:` 引用的事件是否存在
- `scene:` 指向的文件是否存在
- 是否有未识别的 `RawLine`

### 练习五：判断脚本边界

把下面功能分别判断为“适合 `.wts`”或“应该写 C++”：

- 战斗胜利后进入结算页
- 角色每帧移动
- 根据章节打开一个入口
- 敌人寻找玩家
- 等待 0.1 秒后播放动画
- 处理几十个碰撞体的每帧碰撞

## 5.24 什么时候用 `.wts`，什么时候不用

适合 `.wts` 的：

- 按钮点击
- 流程跳转
- 轻量条件
- 等待
- 剧情事件
- 胜负后的分发

不适合 `.wts` 的：

- 每帧移动
- 敌人 AI
- 复杂状态机
- 高频碰撞

这条边界很适合面试时讲：

> `.wts` 负责低频流程，C++ 负责高频规则。

这里的“按钮点击”需要准确理解为“按钮点击后的低频流程适合写进 `.wts`”，而不是“`.wts` 自己负责监听点击”。输入发现仍由 `UIInputSystem` 完成，VN 推进由 `VisualNovelSystem` 完成，战斗动作和命中由玩法系统完成。

## 5.25 和 Unity / UE 的对照

Unity 常用协程、事件函数、Timeline、ScriptableObject。  
UE 常用 Blueprint、Delegate、任务系统、Level Streaming。

Wheatear 的选择更轻：

- 文本脚本就是流程资产
- 事件脚本组件就是挂载点
- 运行时系统就是执行器

它不是在追求“万能脚本”，而是在做一个很适合 2D 项目的流程层。

## 5.26 面试时怎么讲

可以这样说：

> 我们用 `.wts` 做低频流程编排。  
> 场景里挂 `EventScriptComponent`，`EventScriptSystem` 负责解析和执行脚本，支持 `wait`、条件分支和热重载。  
> UI、VN 或玩法系统先发现操作或结果，再通过 `CommandBus` 投递 `event:xxx`；`EventScriptSystem` 根据事件名找到对应的 WTS 事件块。
> 脚本执行到 `scene:`、`newgame:`、`progression:` 这类命令时，再回到 `CommandBus`，交给 `SceneTransitionService`、`GameProgress` 或对应系统处理。
> 因此 WTS 负责的是“事件已经发生后，按什么顺序编排后续流程”，而不是监听输入或每帧执行玩法规则。

## 5.27 本章小结

这一章最重要的结论是：

```text
.wts 是流程资产，
EventScriptComponent 是挂载点，
EventScriptSystem 是执行器，
CommandBus 是流程继续向下分发的入口，
GameProgress 和 SceneTransitionService 是最终落地的状态与宿主。
```

如果把它缩成一条线，就是：

```text
场景启动或外部系统发现操作 / 结果
 -> StartEvent 或 event:xxx
 -> EventScriptSystem 找到 .wts 事件块
 -> 条件 / 等待 / 命令
 -> CommandBus
 -> 引擎状态变化
```

下一章再写的话，就该进入 **ActionSystem** 和 **WAO** 了。  
那会从“流程编排”继续走到“动作意图与结算”。
