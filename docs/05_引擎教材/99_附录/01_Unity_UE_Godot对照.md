# 附录 01：Unity / UE / Godot 架构对照

> 本附录所属层级：**附录**
>
> 本附录目标：用 Unity、UE、Godot 这三类常见引擎当参照物，帮助你理解 Wheatear 的分层、组件、服务和玩法组织方式。
>
> 先提醒一句：这不是一份“功能百科”，而是一份**概念对照表**。  
> 同一个概念在不同引擎里可能会分散到多个对象里，所以这里讲的是“思路对照”，不是“名字一一相等”。
>
> 如果你要拿它去面试，最有用的不是背表格，而是能说出：
> - Wheatear 和 Unity / UE / Godot 的共同点
> - Wheatear 和它们最大的组织差异
> - 为什么我会把自己的项目拆成这种样子

## 1.1 这份对照怎么读

建议你按三个角度看：

1. **运行骨架**：程序怎么起、场景怎么活、对象怎么更新。
2. **数据和状态**：内容表、存档、设置、资源、调参分别放哪。
3. **玩法和工具**：剧情、战斗、成长、编辑器怎么接起来。

你会发现，Wheatear 不是在抄某一家引擎，而是在把大家都熟悉的概念拆成更清楚的职责边界。

## 1.2 总体气质

### 1.2.1 Wheatear 的气质

Wheatear 更偏：

- 服务化
- 命令驱动
- 数据驱动
- 场景实体承载状态
- 玩法层按模块拆开

这很适合一个以 **简单 2D 游戏** 为目标的项目。  
因为你真正要跑通的是：

- 2D 渲染
- 输入
- 剧情
- 战斗
- 成长
- 编辑器
- 存档

而不是追求一个特别重的通用框架。

### 1.2.2 Unity 的气质

Unity 更偏：

- 组件驱动
- MonoBehaviour 脚本化
- Editor + Runtime 一体
- 工程化工具生态非常成熟

Unity 的好处是上手快、生态大。  
缺点是项目一大，脚本和资源很容易变散，最后要靠团队约定把边界补回来。

### 1.2.3 UE 的气质

UE 更偏：

- 引擎框架重
- Actor / Component / World 组织清晰
- Gameplay Framework 更完整
- 蓝图 + C++ 双栈

UE 很适合复杂项目，但它的学习成本也更高。  
如果你不把职责讲清楚，很容易在 GameInstance、GameMode、GameState、Subsystem、Widget 之间绕晕。

### 1.2.4 Godot 的气质

Godot 更偏：

- SceneTree / Node 树状组织
- 场景即结构
- 信号系统很强
- 轻量、直接、好改

Godot 的组织方式很适合快速搭玩法原型。  
它和 Wheatear 的区别在于：Godot 更像“节点树”，Wheatear 更像“场景实体 + 服务 + 命令”。

## 1.3 核心骨架对照

| Wheatear | Unity | UE | Godot | 说明 |
| --- | --- | --- | --- | --- |
| `Application` | PlayerLoop / App 启动 | Engine / Game 启动 | `MainLoop` / `SceneTree` 启动 | 程序入口和主循环壳子 |
| `LayerStack` / `Overlay` | Script Execution Order / UI 层次 | 各类 Subsystem / UMG 层次 | 节点树和 Canvas 层次 | 更新优先级和显示层级 |
| `Scene` | Scene | World / Level | Scene / SceneTree 子树 | 世界容器 |
| `Entity` | GameObject | Actor | Node / Node2D | 场景里的可查询对象 |
| `Component` | Component / MonoBehaviour | Actor Component | Node 上挂的子节点或脚本状态 | 结构化状态 |
| `System` | Manager / System / Update Service | Subsystem / Gameplay 逻辑类 | Autoload / 管理脚本 | 按规则推进逻辑 |
| `SceneSerializer` | Scene/Prefab 序列化 | Map/Level 资产保存 | Scene 保存 / Resource 保存 | 场景持久化 |
| `CommandBus` | Event / Message / Action Channel | Delegate / Event / Gameplay Message | Signal / call_deferred | 跨系统通信 |
| `GameProgress` | GameManager + Save 数据 | GameInstance + SaveGame | Autoload + Config/File | 全局进度中枢 |
| `ProgressionContent` | ScriptableObject / DataTable | DataAsset / DataTable | Resource / Config | 固定内容表 |
| `InputBindingService` | Input System | Enhanced Input | InputMap | 逻辑输入抽象 |
| `Renderer2D` | SpriteRenderer / Canvas 批处理 | Paper2D / Slate / 渲染管线一层 | CanvasItem / Control 绘制 | 2D 渲染承载 |
| `Physics` | Rigidbody2D / Collider2D / Box2D | Chaos / 物理组件 | PhysicsBody2D / Area2D | 物理世界 |
| `Editor` | Unity Editor | Unreal Editor | Godot Editor | 内容工具 |

## 1.4 运行和场景的对照

### 1.4.1 场景生命周期

Wheatear 里的 `Scene` 很像 Unity 的 Scene、UE 的 World/Level、Godot 的 SceneTree 子场景。

但它的重点不是“文件格式叫什么”，而是：

- 场景能创建
- 能销毁
- 能复制
- 能切换
- 能被序列化

这和前面正文里讲的 `SceneTransition`、`Play Mode Copy`、`SceneSerializer` 是一条线。

### 1.4.2 Unity / UE / Godot 里的类似思路

- Unity：Scene 加载、Prefab 实例、DontDestroyOnLoad
- UE：World / Level 切换、GameInstance 持久化
- Godot：场景切换、Autoload 常驻、节点树重建

Wheatear 的差异在于它更显式地把这些职责拆成了：

- `SceneSerializer`
- `SceneTransitionService`
- `GameProgress`
- `Play Mode Copy`

这对讲架构很有帮助，因为职责边界更清楚。

## 1.5 数据与状态对照

### 1.5.1 内容表和状态表不是一回事

Wheatear 把内容和状态分得很开：

- `ProgressionContent`：固定内容、配方、定义、初始值
- `GameProgress`：玩家当前进度、当前槽位、当前场景、结果页缓存
- `UserSettings`：用户偏好

这和 Unity / UE / Godot 里常见的“数据资产 + 存档对象 + 配置文件”思路非常像，但 Wheatear 把它们的边界说得更明确。

### 1.5.2 对照举例

| Wheatear | Unity | UE | Godot | 说明 |
| --- | --- | --- | --- | --- |
| `ProgressionContent` | ScriptableObject / JSON / DataTable | DataAsset / DataTable | Resource / ConfigFile | 固定规则和静态内容 |
| `GameProgress` | Save 数据 + Manager 状态 | SaveGame + GameInstance 状态 | Autoload + 存档对象 | 长期进度和运行时状态 |
| `UserSettings` | PlayerPrefs / Settings 资产 | GameUserSettings | ConfigFile / ProjectSettings 的运行时部分 | 用户偏好 |
| YAML 调参文件 | ScriptableObject / JSON 调参 | DataAsset / DataTable | Resource / ConfigFile | 可热重载的调优数据 |

### 1.5.3 为什么要分开

因为这三类数据的生命周期不同：

- 内容：项目资产，改它是开发行为
- 进度：玩家局势，改它是游戏行为
- 设置：运行时偏好，改它是用户行为

如果把这三种东西混成一个文件或一个对象，后面会非常难维护。

## 1.6 输入、命令、动作的对照

### 1.6.1 输入抽象

Wheatear 的 `InputBindingService` 负责把“按键”翻译成“动作名”。

这和：

- Unity Input System
- UE Enhanced Input
- Godot InputMap

的思路是一致的。

区别在于，Wheatear 更强调：

- 玩法看到的是 `move.left`、`arcade.attack`、`vn.advance`
- 不直接看到键盘码或鼠标码

### 1.6.2 命令总线

`CommandBus` 更像 Unity 里的事件通道、UE 的委托、Godot 的信号。

但它不是简单的“事件总线”三个字就结束了。  
它在 Wheatear 里是：

- 玩法和 UI 的桥
- 场景和服务的桥
- 剧情和战斗的桥
- 存档和切场景的桥

这就是为什么前面正文里反复强调：  
**UI 不直接改状态，UI 只发命令。**

### 1.6.3 动作层

`ActionIntent`、`ActionRecipe`、`ActionResolver`、`EffectLedger` 这一套，很适合拿去和 UE 的 GAS 思路类比。

但 Wheatear 的落点更轻：

- 意图说明“谁做什么”
- 配方说明“这个动作长什么样”
- 解析器说明“这个动作怎么执行”
- 账本说明“这次执行结果是什么”

如果你要在面试里类比，就说“它和 GAS 有相似的分层思想，但实现更轻、更偏 2D 玩法和数据驱动”。

## 1.7 玩法层对照

### 1.7.1 VN

- Unity：剧情系统、Timeline、UI 对话框、脚本驱动事件
- UE：UMG + 事件蓝图 / 对话系统插件
- Godot：对话场景、信号、控制器脚本

Wheatear 的特点是把 VN 做成了一个明确的玩法模块，而不是零散 UI 拼装。

### 1.7.2 TurnCombat / TacticalCombat / Arcade

这三者和别的引擎的对照可以这样理解：

- Unity：通常会拆成多个 MonoBehaviour + ScriptableObject + UI Controller
- UE：通常会拆成 Actor / Component / DataAsset / UMG / GameMode
- Godot：通常会拆成 Scene / Node / Signal / Resource

Wheatear 则是把这几类职责显式拆成：

- 场景组件
- 系统服务
- 命令入口
- 动作配方
- 调参文件
- 编辑器窗口

这使得你在面试里更容易讲“职责边界”，而不是只讲“我写了个战斗”。

### 1.7.3 Progression

Progression 很适合和 Unity / UE / Godot 里的这些概念对照：

- Unity：GameManager + Save/Load + 菜单 UI + ScriptableObject
- UE：GameInstance + SaveGame + UMG + DataTable
- Godot：Autoload + ConfigFile + Control 场景 + Resource

但 Wheatear 的做法更集中：

- `GameProgress` 管全局进度
- `ProgressionSystem` 管页面刷新
- `ProgressionContent` 管成长定义
- `ProgressionContentEditorPanel` 管内容编辑

## 1.8 工具链对照

### 1.8.1 Editor

Unity Editor、Unreal Editor、Godot Editor 的共同点是：  
都不只是“看场景”，而是直接围绕资产、调参、层级、预览和运行工作流来设计。

Wheatear 的 Editor 也走同一路线：

- 资源浏览
- Inspector
- 场景预览
- 运行控制
- 各玩法调参编辑器

### 1.8.2 Sandbox

Sandbox 可以类比成一个专门给内容作者或自己调试用的运行壳子。

你可以把它理解为：

- Unity 的 Play Mode 运行视图
- UE 的 PIE / Standalone
- Godot 的运行场景

只是 Wheatear 把它做成了单独的应用层入口。

## 1.9 什么时候可以类比，什么时候不要硬类比

### 1.9.1 可以类比的时候

当你在讲：

- 场景生命周期
- 数据驱动
- 存档
- 设置
- 输入映射
- 编辑器工具
- 渲染管线

这些概念时，类比 Unity / UE / Godot 很有帮助。

### 1.9.2 不要硬类比的时候

当你在讲：

- 具体类名
- 具体 API
- 具体执行顺序
- 具体存储格式

这时就不要硬说“一样”。

正确说法是：

> 思路相似，但实现方式和边界不完全一样。

## 1.10 面试时怎么用这份对照

如果面试官问“你这个引擎跟 Unity / UE / Godot 比像什么”，你可以这么答：

> Wheatear 的整体气质更偏服务化和命令驱动。  
> 和 Unity 比，它没有把很多逻辑都压在 MonoBehaviour 上，而是把场景、系统、内容和进度分得更清楚；和 UE 比，它保留了 Actor/Component 那种场景实体组织，但运行服务和玩法模块拆得更细；和 Godot 比，它也强调场景和节点的组织，只是我额外加了一层更明确的服务与命令边界。  
> 所以它不是在复制某一个引擎，而是在把这些引擎里成熟的思路，收束到一个更适合我自己 2D 目标的架构里。

## 1.11 小结

这份对照最重要的意义是：

**让你能把 Wheatear 的设计，翻译成面试官熟悉的语言。**

当你能说出：

- 这部分像 Unity 的什么
- 那部分像 UE 的什么
- 另一部分像 Godot 的什么

面试官就会更容易理解你的项目定位，也更容易顺着你的话继续追问。
