# 第 7 编 02：资源浏览器与 Inspector

> 本章所属层级：**应用层**
>
> 本章目标：理解编辑器里的资源树是怎么扫出来的，资产类型是怎么识别的，资源浏览器怎么打开不同文件，Inspector 又是怎么把内容改回磁盘的。
>
> 这一章讲的是**内容编辑入口**。  
> 它不负责渲染、场景执行或玩法逻辑；它负责让人看见项目内容，并把修改落回项目文件。

如果说上一章讲的是编辑器这个宿主怎么活起来，那这一章讲的就是：

```text
内容是怎么被看见的？
看见以后怎么判断它是什么类型？
选中以后怎么编辑？
编辑完以后怎么写回去？
```

Wheatear 的资源浏览器不是单纯一个文件树，它更像一个**项目内容工作台**：

```text
项目根
  -> assets/
  -> AssetRegistry
  -> ContentBrowserPanel
  -> 资源打开路由
  -> YAML / 文本 / 图集 / 预览 / 片段编辑器
  -> 回写磁盘
```

它的存在解决了一个很实际的问题：  
如果没有统一入口，`.wt`、`.wtsheet`、`.wtanim`、`.wts`、`.vn`、`.yaml`、`.json`、`.wtprefab` 这些文件会各自散落在不同编辑器里，最后整个项目会变成“到处打开外部工具”的状态。

---

## 7.2 本章要读的源码

```text
WheatearEditor/src/Panels/ContentBrowserPanel.h
WheatearEditor/src/Panels/ContentBrowserPanel.cpp
WheatearEditor/src/Assets/AssetRegistry.h
WheatearEditor/src/Assets/AssetRegistry.cpp
WheatearEditor/src/Editor/AssetDocument.h
WheatearEditor/src/Editor/YamlAssetDocument.h
WheatearEditor/src/Editor/TextAssetEditor.h
WheatearEditor/src/Editor/YamlTreeEditor.h
WheatearEditor/src/Panels/ContentBrowserRequests.h
WheatearEditor/src/Editor/EditorLayerBase.cpp
```

如果先抓主线，就先看这五个：

```text
ContentBrowserPanel.h
ContentBrowserPanel.cpp
AssetRegistry.h
AssetRegistry.cpp
YamlAssetDocument.h
```

它们分别回答：

```text
资源树怎么扫？
资产类型怎么识别？
点击一个文件会打开什么？
文本/YAML 资产怎么读写？
```

---

## 7.3 这一层在架构中的位置

```text
应用层
    Editor / Sandbox
        ↓
Editor 内部工具层
    ContentBrowser / Inspector / Document Editors
        ↓
资产层
    AssetPath / AssetRegistry / AssetAliasRegistry
        ↓
Scene / Renderer / Gameplay / Data Files
```

这层和“Scene 层”不一样。  
Scene 层关心的是世界怎么组织；  
资源浏览器关心的是内容怎么被找到、被打开、被编辑。

你可以把它理解成：

- Scene / ECS 是“游戏世界”
- ContentBrowser / Inspector 是“内容工作台”

两者都很重要，但职责不同。

---

## 7.4 `AssetRegistry`：资产目录先要被认出来

### 7.4.1 它解决什么问题

资源浏览器不是直接扫描整个硬盘，而是依赖 `AssetRegistry` 给项目建立一份“资产目录”。

它记录：

- 资产相对路径
- 类型
- 显示名
- 大小
- 修改时间
- 引用关系
- 导入设置
- UUID

也就是说，`AssetRegistry` 不是单纯文件列表，而是**项目资产索引**。

### 7.4.2 为什么要有 registry cache

扫描大项目时，不能每次打开浏览器就完全从头解析所有文件。  
所以 `AssetRegistry` 会先读缓存，再在必要时重新扫描。

它支持两件事：

- `LoadCache()`：直接读已有索引
- `Scan()`：重新遍历 `assets/` 并重建索引

这就是编辑器性能的基础之一。

### 7.4.3 它怎么决定一个文件是什么类型

`DetectKind()` 会根据扩展名和路径判断类型，比如：

- `.wt` -> Scene
- `.png` / `.jpg` -> Texture
- `.wts` -> EventScript
- `.vn` -> VN 脚本
- `.wtanim` -> AnimationClip
- `.wtsheet` -> SpriteSheet
- `.wtprefab` -> Prefab
- `.wtsettings` / `.yaml` / `.json` / `.txt` -> Data

这意味着资源浏览器里显示的不是“文件后缀”，而是“语义类型”。

### 7.4.4 为什么要排除 `.wheatear`、cache、saves 这些目录

`AssetRegistry` 会跳过一批编辑器内部目录：

- `.wheatear`
- `cache`
- `saves`
- `previews`
- `source_frames`

这些目录里的内容很多是中间产物，不应该当成项目内容直接暴露给普通编辑工作流。

这点很重要，因为一个项目的“可编辑内容”与“编辑器中间结果”必须分开。

### 7.4.5 引用关系为什么也要存

`AssetRegistry` 不是只记录“有什么文件”，还会尽量提取“谁引用了谁”。

这让编辑器能做很多事：

- 资源健康检查
- 引用追踪
- 依赖分析
- 打包时筛出需要的内容

所以它不只是 Content Browser 的后台，也还是项目健康和打包的基础数据层。

---

## 7.5 `ContentBrowserPanel`：内容浏览器本体

### 7.5.1 它从哪里开始看

`ContentBrowserPanel` 的默认起点是：

```text
AssetPath::GetAssetRoot()
```

也就是当前项目的 `assets/`。

这很合理，因为浏览器的目标不是展示整个系统，而是展示**当前项目内容**。

### 7.5.2 资源图标怎么来的

浏览器会为不同资产类型准备图标：

- Folder
- Scene
- Texture
- Shader
- Audio
- Script
- Prefab
- UI Template
- Material
- Animation Clip
- Mesh
- Sprite Sheet

这样用户一眼就能看出一个资源是什么，不必挨个打开。

### 7.5.3 `GetAssetType()` 怎么分类型

`GetAssetType()` 的逻辑本质上就是：

- 目录就是目录
- 扩展名决定其余类型

这听上去简单，但它是编辑器里很多后续行为的基础。  
因为类型一旦确定，后面的：

- 图标
- 预览
- 双击打开方式
- 创建模板
- 过滤器

都会跟着变。

### 7.5.4 资源列表怎么扫

`GetFilteredEntries()` 会根据：

- 当前目录
- 搜索框
- 类型过滤器

去过滤结果，并且做缓存。

这里有一个很好的细节：  
它不是每帧都重新扫文件系统，而是按目录和过滤条件缓存扫描结果。  
这会让大资源目录浏览起来更稳。

### 7.5.5 目录导航怎么做

浏览器支持：

- 进入目录
- 返回
- 前进

这让它的交互更像真正的文件浏览器，而不是一个只会显示当前目录的静态面板。

---

## 7.6 打开一个资源以后会发生什么

`ContentBrowserPanel::OpenEntry()` 是这章最值得看的路由之一。

它会按类型把文件分给不同编辑器：

- `.vn` -> `VisualNovelEditorRequests::RequestOpenScript()`
- `.wts` -> `EventScriptGraphRequests::RequestOpenScript()`
- `.wt` -> 场景打开回调
- `.wtprefab` -> prefab 实例化
- `.wttemplate` / `.wttemplate` 类资源 -> UI template 实例化
- `.wtsheet` -> 图集选择器
- `.yaml` / `.json` / `.wtsettings` / `.wtanim` -> 数据文件编辑器

这件事很重要，因为它说明浏览器不是“统一打开一个文本编辑器”，而是按内容语义把文件交给最合适的工具。

### 一个很直观的理解

你可以把 `OpenEntry()` 看成：

```text
“文件类型 -> 打开哪个编辑器” 的分发器。
```

这样做的好处是：

- 用户体验更自然
- 每种文件都能用合适的编辑器打开
- 不会把 YAML / 图形化脚本 / 预览流程混在一起

---

## 7.7 新建、导入、重命名、删除

### 7.7.1 导入外部文件

`ImportAssetsIntoCurrentDirectory()` 支持把外部文件复制进当前目录。

它会：

- 打开系统文件选择器
- 复制进当前浏览目录
- 处理重名
- 再触发一次 `AssetRegistry::Get().Scan()`

这很符合编辑器的实际需求，因为内容制作经常不是从零开始，而是先从外部拷一批素材进项目。

### 7.7.2 新建场景

`CreateNewSceneFile()` 会直接写一个最小可运行场景：

- 主相机
- UI Canvas
- 宽松的 SavePolicy

这相当于给新人提供“我一创建就能跑”的起点。

### 7.7.3 新建数据文件

`CreateNewDataFile()` 会创建一个基础数据文档。  
这类文件通常会进 `YamlAssetDocument` 或 `TextAssetDocument`，作为通用可编辑资源存在。

### 7.7.4 新建图集

`CreateNewSpriteSheetFile()` 会生成一份图集模板。  
这意味着图集在编辑器里不是手工从零写 YAML，而是有标准起点的。

### 7.7.5 重命名

`CommitRename()` 会直接在文件系统里 rename，并更新当前选中项。

这看上去像小功能，但它说明了一个关键点：

```text
资源浏览器不是只看文件，它也负责改文件。
```

---

## 7.8 Inspector：编辑器里最关键的修改入口

### 7.8.1 Inspector 本质上是什么

Inspector 不是单一面板，而是“根据资产类型或组件类型，展示可编辑字段”的那层 UI。

在 `ContentBrowserPanel` 里，它更像右侧的属性编辑器；  
在 Scene Hierarchy 里，它更像组件详细面板。

本章主要讲资源侧的 Inspector，也就是：

- YAML 文档怎么树状显示
- 文本文件怎么显示 raw preview
- 哪些资源可以直接修改后保存

### 7.8.2 `AssetDocument` 和 `YamlAssetDocument`

`TextAssetDocument` 和 `YamlAssetDocument` 是这一层的核心数据结构。

#### `TextAssetDocument`

它负责：

- 记录 source path
- 解析资源路径
- 读取文本
- 保存文本
- 管 dirty / status / loaded

#### `YamlAssetDocument`

它在 `TextAssetDocument` 上再加一层：

- 解析 YAML node
- 保留 raw preview
- 支持结构化修改
- 保存时重新发回文本

这意味着很多文本类资源并不直接靠外部编辑器，而是可以在编辑器内完成结构化编辑。

### 7.8.3 为什么要区分 `RuntimeResolve` 和 `ProjectRoot`

`DocumentWriteDestination` 很重要。

有些文档保存时应该遵守运行时解析规则，  
但有些文档属于项目内容，必须明确写回项目根。

这类区分让内容编辑更安全：

- 项目数据不会被写到奇怪的位置
- 运行时资源和项目资产不会混淆

### 7.8.4 典型的 Inspector 用法

比如一个 `.yaml` 配置文件：

1. 浏览器打开它
2. `YamlAssetDocument::Load()`
3. Inspector 显示树
4. 用户改一个字段
5. `MarkDirty()`
6. `Save()`
7. 文本重写回项目

这是典型的“可视化编辑 + 结构化保存”链路。

### 7.8.5 对脚本、动画、设置文件的意义

这层对很多项目文件都很关键：

- `player.config`
- 运行时设置
- `.wtanim`
- 一般 YAML 数据表
- JSON 配置

如果没有这个层，很多内容就只能在外部文本编辑器里改。  
那就不算真正把编辑器做成工具链了。

### 7.8.6 `TextAssetDocument`：文本文档的读写协议

如果把这一层拆到最底，`TextAssetDocument` 做的事其实很朴素：

- 记住源路径 `SourcePath`
- 记住解析后的真实路径 `ResolvedPath`
- 保存一份文本缓冲 `Text`
- 记录是否已加载、是否脏、当前状态信息

它最关键的两个函数是：

- `ResolvePath()`：读的时候到底去哪里找
- `ResolveWritePath()`：保存的时候到底往哪里写

这就是为什么它要有 `DocumentWriteDestination`。
对于引擎自带文件，`RuntimeResolve` 就够了；  
但对于游戏内容，尤其是会被项目自己的工具链编辑的文件，`ProjectRoot` 更合适，
因为它能保证保存回工程目录，而不是误写到引擎目录。

读写链路可以直接记成：

```text
SetSourcePath()
  -> Load()
  -> SetText() / MarkDirty()
  -> Save()
```

其中 `Load()` 会把文本装进内存，`Save()` 会先创建父目录，再把文本写回磁盘。
这也是为什么文本文档能在编辑器里形成闭环：  
不是“看一眼”，而是“改完真的能写回去”。

### 7.8.7 `YamlAssetDocument`：结构化 YAML 文档

`YamlAssetDocument` 是 `TextAssetDocument` 再包一层。
它同时保留两份东西：

- `m_TextDocument`：原始文本协议
- `YAML::Node`：结构化树

这层最重要的是“结构化”和“原文预览”同时存在：

- `Load()` 先让文本文档读进来
- 再 `YAML::Load()` 解析成树
- `RefreshRawPreview()` 把树重新吐回文本
- `Save()` 时再把结果交还给 `TextAssetDocument`

它还有一个很实用的设计：  
如果 YAML 解析失败，`m_ParseValid` 会变成 `false`，保存就会被阻止。
这比“带着坏树照样写回磁盘”安全得多。

所以这一层的意义不是“又包了一层”，而是把文档编辑从纯文本变成了：

- 可解析
- 可校验
- 可回写
- 可预览

### 7.8.8 `TextAssetEditor` 和 `YamlTreeEditor`：文档怎么变成面板

`TextAssetEditor` 和 `YamlTreeEditor` 负责把上面的文档对象真正画出来。

`TextAssetEditor` 的思路很直接：

- 用 `TextAssetEditorState` 缓存文档、缓冲区、状态和路径
- `LoadTextAsset()` 把文件读进 buffer
- `DrawTextAssetEditor()` 用 `ImGui::InputTextMultiline()` 编辑
- 用户一改文本，就 `MarkTextAssetDirty()`
- 点保存就 `SaveTextAsset()`

`YamlTreeEditor` 则更像一个通用树编辑器。
它不是只为了一个文件类型服务，而是给很多内容编辑器共用：

- `DataFileEditorPanel`
- `EventScriptGraphPanel`
- `ProgressionContentEditorPanel`
- 各种战斗参数编辑器

它能处理的东西也很“工程化”：

- map / list / scalar 节点
- `rect` 这种矩形序列的专用拖拽编辑
- 类型化资产选择器
- 任意 YAML 节点的树编辑兜底

这就是为什么后来很多面板看起来很像：  
它们不是重复造轮子，而是共用同一个 YAML 结构编辑底座。

### 7.8.9 三个最典型的完整例子

第一个例子是图集 / SpriteSheet。
双击 `.wtsheet` 以后，资源浏览器不会把它当成“普通文本”丢给一个纯文本框，
而是会走结构化文档或专用编辑器。
你可以在这里改：

- region
- trim
- pivot
- collider / hitbox

保存时，内容会回写到项目根，`AssetRegistry` 重扫后，  
`SpriteSheetSystem` / 预览渲染就能拿到新的帧定义。

第二个例子是动画。
`.wtanim` 往往会和 `.wtsheet` 绑在一起：  
动画资源负责描述播放顺序、帧时长、循环和事件，
图集负责提供每一帧真正取哪张图。
这两者通常都会通过同一套文档保存管线写回磁盘，
然后被动画系统热重载。

第三个例子是 VN 脚本。
`TextAssetEditor` 适合这一类“更偏文本”的内容：

- `@background`
- `@show`
- `@choice`
- `event:` 跳转

它改完以后并不会只停留在编辑器里，  
而是会继续进入脚本解析、命令分发和运行时更新那条链路。

所以这一章最值得背的一句话是：

```text
资源浏览器不是看文件名，Inspector 也不是只显示字段；
它们一起负责把“文件”变成“可编辑、可保存、可热更新的内容对象”。
```

---

## 7.9 资产浏览器里最典型的完整流程

### 例子 1：打开一个图集

```text
assets/sheets/hero.wtsheet
    ↓
ContentBrowserPanel::OpenEntry()
    ↓
onOpenSpriteSheet callback
    ↓
SpriteSheetPickerPanel::OpenSheet()
    ↓
显示 cell / rect / collider
    ↓
改 trim -> 保存 -> AssetRegistry / SpriteSheetSystem 跟进
```

### 例子 2：打开一个动画文件

```text
assets/anim/hero_attack.wtanim
    ↓
OpenEntry()
    ↓
DataFileEditorRequests::RequestOpen()
    ↓
YamlAssetDocument::Load()
    ↓
编辑 frames / events / tracks
    ↓
Save()
    ↓
运行时 AnimationSystem 热重载
```

### 例子 3：打开一个 VN 脚本

```text
assets/vn/chapter1.vn
    ↓
OpenEntry()
    ↓
VisualNovelEditorRequests::RequestOpenScript()
    ↓
VN 脚本编辑器打开
    ↓
保存后影响运行时剧情
```

这三条链路说明了同一件事：

```text
浏览器只是入口，真正的内容编辑是按资产类型分流出去的。
```

---

## 7.10 和别的引擎怎么对照

### Unity

Unity 的对照最直观：

- Project 窗口 -> ContentBrowserPanel
- Inspector -> 右侧属性面板
- 选中资源后打开对应导入器 / 编辑器
- Sprite / Animation / Scene / Prefab 都有自己的工作流

Wheatear 这里走的是类似思路，只是实现更轻，更多靠自己写的文档编辑器和 YAML 面板。

### Unreal

UE 可以类比成：

- Content Browser
- Details Panel
- Asset Registry
- 资产类型驱动的打开方式

Wheatear 的 `AssetRegistry + ContentBrowserPanel` 和这个很接近。

### Godot

Godot 则像：

- FileSystem dock
- Inspector
- Resource picker
- 资源类型驱动打开

三者思路都一样：  
资源不能只是“文件”，还要是“可被编辑器理解的内容对象”。

---

## 7.11 这一层的优点、坏处和常见误区

### 优点

- 资源和编辑器入口统一
- 类型驱动打开方式，用户体验更顺
- AssetRegistry 提供索引、引用和健康检查基础
- 文本 / YAML 文档能在编辑器里闭环修改
- 图集、动画、VN、场景都能从同一浏览器进入

### 坏处

- 资产类型越多，打开路由越复杂
- Inspector 需要为不同文件类型维护不同逻辑
- 资源缓存、预览、回写三者如果没分清，会变乱
- 浏览器既要管目录、又要管文件、又要管预览，职责很容易膨胀

### 常见误区

- 把 Content Browser 当成纯文件管理器
- 认为 Inspector 只是“显示字段”，而不是“修改内容”的入口
- 忘了资产注册表是浏览器性能和打包的基础
- 忘了 `.wtsheet`、`.wtanim`、`.vn` 这些文件应该交给专用编辑器
- 保存后不触发 registry refresh
- 只做 UI 展示，不做内容回写

---

## 7.12 面试时怎么讲

如果面试官问你：“Wheatear 的资源浏览器和 Inspector 是怎么设计的？”

你可以这样答：

```text
我先用 AssetRegistry 给项目资产建立索引，记录类型、路径、引用和导入信息；
ContentBrowserPanel 负责目录浏览、搜索、过滤、导入、重命名和打开路由；
具体文件不会统一扔到一个文本编辑器，而是根据类型分发给对应编辑器，比如 VN、事件脚本、图集、动画和数据文件都有自己的入口；
Inspector 一侧则通过 TextAssetDocument 和 YamlAssetDocument 提供结构化编辑和保存。
```

如果面试官继续问：“为什么要这么分？”

你可以补一句：

```text
因为资源管理不只是看文件，
还要解决类型识别、引用追踪、预览、修改和回写这几件事。
```

如果继续追问：“怎么保证编辑器改完内容后不会乱掉？”

你就说：

```text
通过 AssetRegistry 的重扫、Document 的 dirty 状态、以及按类型分流的保存逻辑来保证。
```

---

## 7.13 小结

这一章最核心的结论是：

```text
资源浏览器不是文件夹视图，Inspector 也不是属性列表，
它们一起构成了编辑器里的内容操作面。
```

Wheatear 这层做了三件事：

- `AssetRegistry` 给项目资产建目录
- `ContentBrowserPanel` 提供浏览、打开、导入、创建、重命名
- `AssetDocument / YamlAssetDocument` 提供结构化修改和保存

把这一章看懂以后，后面的“场景预览与运行控制”就会很顺。  
因为你会更容易理解：

```text
编辑器到底是在看内容，还是在跑内容。
```

下一章我们就接着写 `场景预览与运行控制`。  
那一章会继续把“编辑态”和“运行态”的边界讲清楚。
