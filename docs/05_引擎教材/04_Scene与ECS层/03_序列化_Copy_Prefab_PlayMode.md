# 第 4 编 03：序列化、Copy、Prefab 与 Play Mode Copy

> 本章所属层级：**Scene ECS 层**
>
> 本章目标：把四件很容易被混在一起的事分清楚：
> - `SceneSerializer` 怎么把场景写到 YAML 文件里
> - `Scene::Copy()` 怎么把整个场景在内存里复制一份
> - `Prefab` 怎么把一个子树做成可复用模板
> - 编辑器里的 `Play Mode Copy` 为什么既不是保存，也不是 prefab 实例化
>
> 这一章是场景体系里最容易在面试里被追问的部分，因为它同时回答了：
> - “你怎么存场景？”
> - “你怎么做预制体？”
> - “编辑器 Play 时为什么不会污染原场景？”
> - “实体引用、UUID、父子关系怎么保住？”

如果上一章讲的是“Scene 怎么活”，那这一章讲的就是：

```text
Scene 怎么被写下来，
怎么被复制出来，
怎么被复用，
怎么在编辑器里安全地进入运行态。
```

Wheatear 这里其实有三种不同的“复制/保存”语义：

| 概念 | 作用对象 | 是否落盘 | UUID 是否保留 | 典型用途 |
| --- | --- | --- | --- | --- |
| Scene 序列化 | 整个场景 | 是 | 保留 | 保存 / 读取场景文件 |
| Scene::Copy | 整个场景 | 否 | 保留 | 编辑器 Play Mode Copy |
| Prefab | 一个子树 / 模板 | 是 | 不保留，重新生成 | 可复用模板、实例化 |

这个表是本章最先要记住的东西。

---

## 4.3.1 本章要读的源码

```text
Wheatear/src/Wheatear/Scene/SceneSerializer.h
Wheatear/src/Wheatear/Scene/SceneSerializer.cpp
Wheatear/src/Wheatear/Scene/SceneCopy.cpp
Wheatear/src/Wheatear/Scene/SceneComponentGroups.h
Wheatear/src/Wheatear/Scene/Serialization/SceneSerializerComponentGroups.h
Wheatear/src/Wheatear/Scene/Serialization/SceneSerializerComponentSupport.h

Wheatear/src/Wheatear/Scene/Serialization/SceneSerializerCoreComponents.cpp
Wheatear/src/Wheatear/Scene/Serialization/SceneSerializerAnimationComponents.cpp
Wheatear/src/Wheatear/Scene/Serialization/SceneSerializerScriptingComponents.cpp
Wheatear/src/Wheatear/Scene/Serialization/SceneSerializerModuleComponents.cpp
Wheatear/src/Wheatear/Scene/Serialization/SceneSerializerModuleComponents_Arcade.cpp
Wheatear/src/Wheatear/Scene/Serialization/SceneSerializerModuleComponents_SideCombat.cpp
Wheatear/src/Wheatear/Scene/Serialization/SceneSerializerModuleComponents_Tactical.cpp
Wheatear/src/Wheatear/Scene/Serialization/SceneSerializerModuleComponents_Turn.cpp
Wheatear/src/Wheatear/Scene/Serialization/SceneSerializerUIComponents.cpp

WheatearEditor/src/Editor/EditorLayerBaseScene.cpp
WheatearEditor/src/Editor/EditorCommands.h
WheatearSandbox/src/RuntimeSceneLayer.cpp
```

如果只想先抓主线，就先看这五个：

```text
SceneSerializer.h
SceneSerializer.cpp
SceneCopy.cpp
SceneComponentGroups.h
EditorLayerBaseScene.cpp
```

它们分别回答这五个问题：

```text
场景怎么写到文件？
场景怎么从文件读回来？
整个场景怎么在内存里复制？
复制时哪些组件会跟着走？
编辑器 Play Mode 为什么要先复制一份 Scene？
```

---

## 4.3.2 这一层在架构中的位置

```text
应用层
    Editor / Sandbox
        ↓
核心平台层
    Application / EventBus / LayerStack
        ↓
引擎服务层
    Renderer / UI / Input / Audio / Physics
        ↓
Scene ECS 层
    Scene / Entity / Components / Systems / Serializer
        ↓
第三方与平台层
    entt / YAML / Box2D / GLFW / OpenGL
```

`SceneSerializer` 和 `Scene::Copy()` 都属于 Scene ECS 层，但它们解决的是两类完全不同的问题：

- `SceneSerializer` 解决“持久化”
- `Scene::Copy()` 解决“运行时复制”

这两件事很像，但绝不能混。

因为一旦混了，你就会在面试里说出这种危险句子：

```text
“Play Mode 我直接把场景存盘再读回来就行了。”
```

这在真实项目里通常不够好，甚至会把编辑器状态、运行态状态和落盘数据混成一锅。

---

## 4.3.3 先给结论：这章到底在讲什么

Wheatear 的场景体系可以先记成四句话：

```text
Scene 序列化：把世界写到 YAML 文件里。
Scene::Copy：把整个世界在内存里复制一份，UUID 不变。
Prefab：把一个可复用子树做成模板，实例化时重建 UUID。
Play Mode Copy：编辑器进入运行态时复制一份 Scene，保证原编辑场景不被污染。
```

再说得直白一点：

- 场景文件是“一个世界”
- Prefab 是“一个子世界模板”
- Copy 是“一个正在跑的世界的镜像”
- Play Mode Copy 是“编辑器临时开一份运行中的镜像”

这四者的边界，后面一定要背熟。

---

## 4.3.4 `SceneSerializer`：场景怎么写到 YAML

### 4.3.4.1 `SceneSerializer` 在做什么

`SceneSerializer` 负责把 `Scene` 写到磁盘，也负责从磁盘读回 `Scene`。

它的接口很直接：

```text
SerializeYaml(filepath)
DeserializeYaml(filepath)
```

这个设计的好处是简单明了：

- 场景文件是人能读的
- 出错时能直接打开 YAML 查
- Git diff 也能看出改动

这对一个偏 2D、偏工具链、偏面试讲解的引擎非常合适。

### 4.3.4.2 场景文件长什么样

场景 YAML 的顶层结构很朴素：

```yaml
Scene: Untitled
SavePolicy:
  CanSave: true
  CanLoad: true
  SaveDirectory: assets/saves
  AutoLoadSlot: 0
Entities:
  - Entity: 123456789
    TagComponent:
      Tag: MainCanvas
    UICanvasComponent:
      ...
```

也就是说，Scene 文件不是只有实体，它还会带上：

- 场景名字
- 保存策略
- 实体列表

这点很重要。  
因为场景不只是“画面对象”，它还是“流程对象”。

### 4.3.4.3 `SerializeYaml()` 的链路

写场景时，核心链路是：

```text
SceneSerializer::SerializeYaml
    -> SerializeSavePolicy
    -> 遍历 registry 里的 IDComponent
    -> SerializeEntity
    -> 写到 YAML::Emitter
    -> 通过 AssetPath::Resolve 找到磁盘路径
    -> create_directories
    -> 写文件
```

这里有两个点很值得注意：

1. 它只遍历带 `IDComponent` 的实体
2. 真实磁盘路径要先经过 `AssetPath::Resolve`

这意味着场景文件不是随手写到哪都行，而是要走项目自己的资源路径规则。

### 4.3.4.4 `SerializeEntity()` 写了什么

`SerializeEntity()` 的工作很直接：

```text
Entity UUID
  + Core components
  + Animation components
  + Scripting components
  + Module components
  + UI components
```

它不是按“所有组件一锅端”来写，而是按模块分组去写。

这样做的好处是：

- 引擎核心组件和玩法模块组件分层清楚
- 新增模块时更容易扩展 serializer
- 读代码时一眼能看出组件归属

### 4.3.4.5 为什么要按分组序列化

因为你的项目本身就不是一个“只有核心引擎”的项目。  
它还有：

- VN
- Side Combat
- Turn Combat
- Tactical Combat
- Progression
- UI
- Audio

所以 Scene 文件必须能存下这些模块自己的组件。

`SceneSerializerModuleComponents.cpp` 就是这种设计的入口：

- `VisualNovelComponent`
- `ArcadeCombat...`
- `SideCombat...`
- `TurnCombat...`
- `TacticalCombat...`

这说明场景文件不是死的，它会跟着玩法模块一起长。

### 4.3.4.6 `DeserializeYaml()` 的链路

读场景时，链路和写相反：

```text
SceneSerializer::DeserializeYaml
    -> YAML::LoadFile
    -> 检查 Scene 键
    -> 读取 SavePolicy
    -> 遍历 Entities
    -> DeserializeEntityFromNode
    -> CreateEntityWithUUID
    -> DeserializeCore / Animation / Scripting / Module / UI
```

这里最重要的是：  
**读场景会尽量让整张场景继续加载，而不是一个实体坏了就把整张图扔掉。**

### 4.3.4.7 为什么要容错

`DeserializeEntityFromNode()` 对 YAML 异常是做了保护的。

这表示：

- 某个实体字段缺失
- 某个旧文件字段变化
- 某个手写 YAML 有误

都不应该让整张场景崩掉。

更现实一点说，编辑器里经常会遇到：

- 老版本场景
- 人工修过的场景
- 字段名变了的场景

所以“能尽量加载剩下的部分”比“严格到一处错都不让进”更实用。

### 4.3.4.8 `SavePolicy` 为什么被写进场景文件

因为场景本身就会决定：

- 能不能存
- 能不能读
- 存到哪个目录
- 读档时默认槽位是什么

所以 `SavePolicy` 不是单独放在系统配置里的，而是跟场景一起保存。

这非常符合“场景是流程对象”的思路。

---

## 4.3.5 组件怎么被拆开存

### 4.3.5.1 组件序列化不是一坨大 JSON

Wheatear 不是把实体序列化成一个“万能对象图”。

它是把每种组件交给对应的 `ComponentSerializer<T>` 去处理。

这类代码分布在：

- `SceneSerializerCoreComponents.cpp`
- `SceneSerializerAnimationComponents.cpp`
- `SceneSerializerScriptingComponents.cpp`
- `SceneSerializerModuleComponents.cpp`
- `SceneSerializerUIComponents.cpp`

这是很典型的 C++ 引擎写法：

```text
每类组件各写各的，最后由 SceneSerializer 汇总。
```

### 4.3.5.2 核心组件写什么

核心组件通常包括：

- `TagComponent`
- `EditorFolderComponent`
- `TransformComponent`
- `CameraComponent`
- `SpriteRendererComponent`
- `CircleRendererComponent`
- `Rigidbody2DComponent`
- `BoxCollider2DComponent`
- `CircleCollider2DComponent`
- `MeshRendererComponent`
- `DirectionalLightComponent`
- `PointLightComponent`

这说明场景文件里不只是 2D 画面，也能存基础 3D 预览内容。

### 4.3.5.3 UI 组件为什么也要单独序列化

UI 是场景的一部分，不是编辑器的附属品。

所以这些都会被写入 Scene 文件：

- `UICanvasComponent`
- `UIWidgetComponent`
- `UIAnimatorComponent`
- `UIImageComponent`
- `UITextComponent`
- `UIButtonComponent`
- `UIProgressBarComponent`
- `UISliderComponent`
- `UIPagerComponent`
- `UIScrollViewComponent`
- `UIPathComponent`
- `UISkillTreeViewComponent`
- `UIPageItemComponent`
- `UICheckboxComponent`

这就能解释为什么一个 VN 场景、一个战斗 HUD、一个设置页都能被场景直接承载。

### 4.3.5.4 模块组件为什么也要存

你的项目最有特点的地方之一，就是玩法模块自己也有场景组件。

例如：

- `VisualNovelComponent`
- `SideCombatLevelComponent`
- `TurnCombatLevelComponent`
- `TacticalCombatLevelComponent`
- `ArcadeCombatLevelComponent`

这些组件不是“附加项”，而是玩法状态的一部分。

所以场景文件必须能把它们一起存下来，不然读档后剧情、战斗、UI 绑定都会断。

---

## 4.3.6 `Prefab`：什么叫可复用子树

### 4.3.6.1 Prefab 不是 Scene

Prefab 的定位和 Scene 不一样。

Scene 是一个完整世界，Prefab 是一个复用模板。

Prefab 适合用来表达：

- 一组固定结构的 UI
- 一个可复用对话框
- 一套设置面板
- 一个结果页
- 一个状态条组合

### 4.3.6.2 `SerializePrefab()` 的结果

Prefab 文件里会有这些字段：

```yaml
Prefab: DialogueBox
Version: 2
RootEntity: 987654321
Entities:
  - Entity: ...
```

这里有个关键区别：

- Scene 文件写的是整个世界
- Prefab 写的是一个根实体和它的子树

### 4.3.6.3 为什么 Prefab 要有 `Version`

因为模板比场景更容易演化。

一旦你开始复用 UI、剧情面板、战斗面板，模板格式就会不停变。

`Version` 的作用就是：

- 以后可以升级格式
- 老 prefab 能识别
- 新 prefab 能扩展

### 4.3.6.4 `CollectPrefabEntities()` 在收什么

当前实现里，Prefab 的收集是沿着 `UIWidgetComponent::ParentEntity` 这棵树往下走的。

它先建一个 child index，再递归收集子实体。

这说明它尤其适合：

- UI 复合控件
- 页面模板
- 父子节点清晰的 widget 树

### 4.3.6.5 为什么这里会看 `SortOrder`

收集子实体时会按：

1. `SortOrder`
2. 名字

排序。

这很实用，因为 UI 模板里父子关系虽然重要，但层级顺序也重要。  
一个 prefab 要能稳定复现，顺序不能随加载顺手乱掉。

### 4.3.6.6 为什么要防止层级太深

代码里对递归深度做了保护，避免手工或错误数据把层级搞得无限深。

这属于很典型的工程防线：

```text
人写的数据不一定完美，
序列化器要能防住极端情况。
```

### 4.3.6.7 Prefab 的真正关键：UUID 重映射

Prefab 反序列化时最重要的一步，不是把组件读回来，而是**给每个实体生成新的 UUID，并建立旧 UUID 到新 UUID 的映射**。

这是因为：

- 同一个 prefab 可能被实例化很多次
- 如果 UUID 还跟原模板一样，实例之间会打架
- 实体绑定也会串到错误目标上

所以 prefab 的逻辑是：

```text
旧 ID 只用来解析引用，
新 ID 才是实例真正拥有的身份。
```

### 4.3.6.8 Prefab 反序列化后还要 remap 什么

除了根实体和父子关系，Prefab 还会重映射这些绑定：

- `UIWidgetComponent::ParentEntity`
- `UIPageItemComponent::PagerEntity`
- `UIButtonComponent::OnClickFunction`
- 各种玩法模块组件里的实体名 / selector 字段

这说明你的 prefab 不是静态美术资源，而是**带引用关系的内容树**。

### 4.3.6.9 为什么要支持 `@UUID`、`uuid:`、`id:`

因为绑定实体时，文本字段里可能不是普通名字，而是 selector。

这样做的好处是：

- 设计时引用更稳
- 重命名实体不容易断绑
- prefab 实例化时可以自动重定向到自己的子实体

这在复杂 UI 和剧情场景里非常有用。

### 4.3.6.10 一个 prefab 例子

比如 VN 的对话框模板：

- 底板
- 说话人名字
- 正文文本
- 继续按钮
- 选项按钮若干

如果这些东西都手工在每张场景里重新搭，维护会非常累。  
做成 prefab 后，只要实例化一次，再改参数就行。

---

## 4.3.7 `SerializeUITemplate()`：UI 的特殊 prefab

### 4.3.7.1 为什么还要有 UITemplate

因为 UI 模板比普通 prefab 更像“设计器资产”。

它不仅要保存实体子树，还要保存：

- 显示名
- 分类
- 描述
- 模板类型

所以 `SerializeUITemplate()` 会同时写：

```yaml
UITemplate:
  Version: 1
  Kind: Composite
  DisplayName: ...
  Category: ...
  Description: ...
Prefab:
  ...
```

### 4.3.7.2 UITemplate 的意义

它的意义是让 UI 资产既能被设计器识别，又能被 prefab 逻辑复用。

这非常适合你这种项目：

- 引擎本身面向 2D
- UI 会反复用到
- 编辑器里需要可视化管理

所以 `UITemplate` 可以理解成：

```text
“有元数据的 UI prefab”
```

### 4.3.7.3 这和普通 prefab 有什么关系

`SerializeUITemplate()` 不是另起炉灶。

它只是把 prefab body 嵌进去，再套一层适合设计器读取的元信息。

也就是说：

```text
UITemplate = Metadata + Prefab body
```

这很聪明，因为它让 UI 复用和 UI 工具链共用一套实体结构。

---

## 4.3.8 `Scene::Copy()`：Play Mode Copy 的核心

### 4.3.8.1 `Scene::Copy()` 是什么

`Scene::Copy(Ref<Scene> other)` 是把整个 Scene 在内存里复制一份。

它不是写文件，也不是读文件。

它做的事情很明确：

- 复制视口宽高
- 复制视口偏移
- 复制保存策略
- 复制所有实体
- 复制所有可复制组件

### 4.3.8.2 为什么 Copy 要保留 UUID

这是最关键的一点。

`Scene::Copy()` 创建新实体时使用的是原来的 UUID 和名字。

也就是说，复制后的世界和原世界在“逻辑身份”上是对应的。

这样做的原因是：

- 编辑器的选择、绑定、查找可以继续工作
- 运行态和编辑态的实体一一对应
- Play Mode 期间各种引用关系不会断

所以 `Scene::Copy()` 不是“生成一堆新对象”，而是“保留身份地复制世界”。

### 4.3.8.3 `Scene::Copy()` 的实现思路

它的链路可以记成：

```text
newScene = CreateRef<Scene>()
复制 viewport / save policy
扫描源 Scene 的 IDComponent + TagComponent
为每个实体在新 Scene 里创建同 UUID 的实体
按 Copyable 组件组批量复制组件
```

这比一个个手写拷贝组件更稳，也更容易扩展。

### 4.3.8.4 `AllCopyableSceneComponents` 是什么

`Scene::Copy()` 不是复制所有组件，而是复制一个明确的可复制集合：

- 核心组件
- 动画组件
- 脚本组件
- UI 组件
- 玩法模块组件

这很重要，因为不是每个字段都适合直接复制。

比如物理运行态指针、某些临时缓存、GPU 资源句柄，就不应该盲拷。

### 4.3.8.5 为什么 Copy 和 Serialize 不是一回事

因为它们面对的问题不同。

- Serialize 面对的是磁盘
- Copy 面对的是内存

磁盘可以慢一点，但要稳定、可读、可修。
内存复制要快，而且要保持运行时语义。

### 4.3.8.6 Play Mode Copy 为什么要用它

编辑器进入 Play Mode 时，最自然的做法不是重新读一遍场景文件，而是：

```text
编辑场景 -> Scene::Copy() -> 运行场景
```

这样做的好处是：

- 编辑器不必先保存再读取
- 原编辑态不会被运行时改坏
- 切回编辑时可以直接回到原场景
- 运行时启动更快

这就是 Play Mode Copy 的核心价值。

### 4.3.8.7 编辑器里实际怎么用

`EditorLayerBase::TransitionToPlay()` 里就是这么干的：

```text
清输入
清命令
Scene::Copy(m_EditorScene)
记录场景上下文
EditorScene->OnEditorStop()
PlayScene->OnRuntimeStart()
```

这条链路很适合面试时讲，因为它一下就说明了：

- 编辑态和运行态是两份对象
- 运行态是复制出来的
- 原始编辑场景不会被改脏

### 4.3.8.8 `TransitionToStop()` 说明了什么

退出 Play Mode 时，编辑器会：

- 停掉运行时 Scene
- 清 UI 输入和命令队列
- 重新切回 Editor Scene
- 再调用 `OnEditorStart()`

这说明 Play Mode Copy 是**临时的运行镜像**，不是替换掉原编辑场景。

---

## 4.3.9 `DuplicateEntity()`：单个实体复制和整场景复制不是一回事

### 4.3.9.1 `DuplicateEntity()` 在干什么

`DuplicateEntity(entity)` 是在当前 Scene 里复制一个实体。

它会：

- 创建一个新实体
- 复制可复制组件
- 给它一个新的 UUID
- 保留名字

### 4.3.9.2 它和 `Scene::Copy()` 的区别

它们最大的区别是：

- `Scene::Copy()` 复制整个世界，UUID 保留
- `DuplicateEntity()` 复制一个实体，UUID 重新生成

所以前者是“世界级复制”，后者是“编辑器内复制一个对象”。

### 4.3.9.3 为什么重复实体要重新生成 UUID

因为同一个 Scene 里不能有两个一模一样的身份。

如果 UUID 不变，会出现：

- 查找冲突
- 保存冲突
- 引用冲突

所以单体复制必须换身份。

### 4.3.9.4 为什么要重置物理运行态指针

这点很关键。

复制后的实体不能共享旧的 Box2D 运行态对象，所以要清掉：

- `RuntimeBody`
- `RuntimeFixture`

不然就会出现：

- 两个实体指向同一个物理体
- 销毁时重复释放
- 运行时状态互相污染

这就是典型的“只复制数据，不复制运行时外壳”。

### 4.3.9.5 编辑器里 `Ctrl+D` 怎么来的

编辑器里的复制命令就是直接走 `Scene::DuplicateEntity()`。

这意味着：

- 复制是命令化的
- 能撤销
- 和编辑器命令历史结合得很好

这也很适合新人理解：  
编辑器复制，不是魔法，它就是 Scene 里的一个受控 API。

---

## 4.3.10 `InstantiateFromPrefab()`：Prefab 实例化

### 4.3.10.1 这个接口做什么

`Scene::InstantiateFromPrefab(prefabPath, position)` 是把一个 prefab 文件实例化到当前 Scene 里。

链路大致是：

```text
解析路径
DeserializePrefabEntities
得到一组新实体
设置根实体初始位置
通知系统 OnEntityCreated
```

### 4.3.10.2 它和 `Scene::Copy()` 的区别

Prefab 实例化不是复制整个 Scene。

它更像：

- 读一个模板
- 生成一批新实体
- 给新实体分配新 UUID
- 让它们成为当前场景的一部分

### 4.3.10.3 为什么实例化后要回调系统

因为系统需要知道：

- 新实体创建了
- 物理可以初始化了
- 动画可以同步了
- UI 可以预热了

所以 `OnEntityCreated` 是 prefab 实例化后很关键的一步。

### 4.3.10.4 这个例子怎么讲最直观

比如你在战斗场景里实例化一个“技能提示卡” prefab：

- 图标
- 名字
- 冷却条
- 描述文本

Prefab 实例化后，这些子实体都会变成新的实体对象，但引用关系会被 remap 成自己的那一套。

这和 `Scene::Copy()` 是完全不同的语义。

---

## 4.3.11 场景、Prefab、Copy、UI Template 的区别再压一次

这一段建议你背熟。

| 名称 | 作用 | 是否保留 UUID | 是否会 remap 引用 | 典型场景 |
| --- | --- | --- | --- | --- |
| Scene 序列化 | 保存整张场景 | 保留 | 不需要 | 编辑器保存、运行时读场景 |
| Scene::Copy | 复制整张场景 | 保留 | 不需要 | Play Mode Copy |
| Prefab | 复用一个子树模板 | 不保留 | 需要 | UI 复合控件、可复用模板 |
| UITemplate | 带元数据的 UI prefab | 不保留 | 需要 | 设计器里的 UI 资产 |

如果再加一句最通俗的话，就是：

```text
Scene 是世界，
Prefab 是模板，
Copy 是镜像，
UITemplate 是带说明书的 UI 模板。
```

---

## 4.3.12 和 Unity、UE、Godot 的对照

### 4.3.12.1 Unity

Unity 里最容易对照的是：

- Scene 文件
- Prefab
- Play Mode

Wheatear 的路线和它很像，但更明确地把“编辑场景复制到运行场景”写成了 `Scene::Copy()`。

你可以这么理解：

```text
Unity 的 Play Mode 关注编辑态和运行态隔离，
Wheatear 用 Scene::Copy 把这层隔离写得更直白。
```

### 4.3.12.2 Unreal

UE 里常见对照是：

- Level / World
- Blueprint
- PIE

Wheatear 的想法和它们一致：

- 场景是世界容器
- prefab / template 是可复用结构
- play mode 是运行时镜像

### 4.3.12.3 Godot

Godot 常见对照是：

- `.tscn`
- `PackedScene`
- `SceneTree`

Wheatear 的场景 YAML + prefab + UI template，也能和这个思路对上。

### 4.3.12.4 你的引擎特点

Wheatear 的特点不是“什么都做得像大引擎”，而是：

- Scene 很清楚
- Copy 很清楚
- Prefab 很清楚
- UI 模板很清楚
- Play Mode 的隔离很清楚

这对于一个以 2D 游戏和教材写作为目标的引擎来说，很合适。

---

## 4.3.13 这一层的优点、坏处和常见误区

### 优点

- Scene 可以直接落盘，结构清楚
- 复制是内存级操作，Play Mode 很快
- Prefab 能复用子树，UI 模板很好用
- UUID 保留和重映射这两套语义都做出来了
- 玩法模块自己的组件也能进场景文件
- 容错加载比“文件坏一点就全崩”更实用

### 坏处

- 概念很多，初学者容易混
- 不同复制语义必须区分清楚
- Prefab 绑定和场景绑定的规则不一样
- 组件分组越多，serializer 维护成本越高
- 以后格式升级时要注意兼容

### 常见误区

- 把 Scene 保存和 Play Mode Copy 当成一回事
- 以为 prefab 实例化可以保留原 UUID
- 忘了 prefab 要 remap 引用
- 复制实体时把运行时物理指针一起拷过去
- 把场景文件和存档槽混为一谈
- 忽略 `AssetPath::Resolve`，直接拿裸路径写文件
- 觉得 YAML 只是“方便看”，没意识到它是调试和版本管理的核心

---

## 4.3.14 面试时怎么讲

如果面试官问你“你这个项目里序列化、Prefab、Play Mode Copy 是怎么分的”，你可以这么答：

```text
SceneSerializer 负责把整个 Scene 写成 YAML 和读回来；
Prefab 负责把一个可复用子树保存成模板，实例化时会重新生成 UUID 并重映射引用；
Scene::Copy() 负责在内存里复制整张场景，UUID 保留，专门服务编辑器 Play Mode；
DuplicateEntity() 则是当前 Scene 里单个实体的编辑器复制，新的 UUID，运行时物理指针会被清掉。
```

如果继续追问“为什么要这么分”，你就补一句：

```text
因为保存、复用、运行时隔离这三个问题的语义不一样，必须分开处理。
```

这句话很短，但很有用。

---

## 4.3.15 小结

这一章其实是在讲一个非常核心的引擎问题：

```text
世界怎么被保存，
模板怎么被复用，
运行时怎么不污染编辑器，
实体引用怎么在复制后还保持正确。
```

Wheatear 的答案是：

- 场景用 `SceneSerializer` 写 YAML
- 整场复制用 `Scene::Copy()`
- 局部复用用 `Prefab`
- UI 复合资产用 `UITemplate`
- 编辑器 Play Mode 用复制出来的运行态 Scene

如果你已经把这一章读顺了，下一步再看“系统怎么接入渲染、物理、动画”，就会特别自然。

下一章我建议接着写：

```text
04_系统接入渲染物理动画.md
```

那一章会把：

- `ISystem`
- `RenderSystem`
- `PhysicsSystem`
- `AnimationSystem`
- `SpriteSheetSystem`
- `UISystem`

这些系统怎么挂到 Scene 上，怎么按生命周期跑起来，讲透。
