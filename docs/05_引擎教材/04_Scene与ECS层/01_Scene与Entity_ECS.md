# 第 4 编 01：Scene 与 Entity，正式进入 ECS

> 本章所属层级：**Scene ECS 层**
>
> 本章目标：理解 Wheatear 的世界模型。也就是：
> - `Scene` 如何持有和驱动一个游戏世界
> - `Entity` 为什么只是轻量句柄
> - `Component` 为什么是数据
> - `System` 为什么是逻辑
> - `Serialize / Copy / Prefab / Play Mode` 如何一起工作
>
> 这一章也会顺手把 Unity 和 UE 的常见实现方式对照起来，因为这是面试里最常被追问的部分。

## 4.1 这一层在架构中的位置

```text
应用层
    Editor / Sandbox
        ↓
核心平台层
    Application / EventBus / LayerStack
        ↓
引擎服务层
    Input / Renderer / UI / Audio / Physics
        ↓
Scene ECS 层
    Scene / Entity / Components / Systems / Serializer
        ↓
平台实现层
    Windows / OpenGL / GLFW
        ↓
第三方库
    entt / YAML / Box2D
```

如果说上一章讲的是“输入怎么进来”，那这一章讲的就是：

```text
进来的输入最后作用在谁身上？
这个谁，是怎么被表示的？
这个谁，怎么保存？
这个谁，怎么复制？
这个谁，在 Edit 和 Play 之间怎么切换？
```

## 4.2 本章要读的源码

```text
Wheatear/src/Wheatear/Scene/Scene.h
Wheatear/src/Wheatear/Scene/Scene.cpp
Wheatear/src/Wheatear/Scene/SceneCopy.cpp
Wheatear/src/Wheatear/Scene/SceneSerializer.h
Wheatear/src/Wheatear/Scene/SceneSerializer.cpp
Wheatear/src/Wheatear/Scene/Entity.h
Wheatear/src/Wheatear/Scene/Entity.inl
Wheatear/src/Wheatear/Scene/ComponentLifecycleRegistry.h
Wheatear/src/Wheatear/Scene/ComponentLifecycleRegistry.cpp
Wheatear/src/Wheatear/Scene/SceneSystemRegistry.h
Wheatear/src/Wheatear/Scene/SceneSystemRegistry.cpp
Wheatear/src/Wheatear/Scene/Serialization/SceneSerializerComponentGroups.h
Wheatear/src/Wheatear/Systems/ISystem.h
```

## 4.3 先给结论：Wheatear 的 ECS 长什么样

Wheatear 的 ECS 可以先记成一句话：

```text
Scene 持有 entt::registry，
Entity 是 registry 里的一个 handle 包装，
Component 是纯数据，
System 是按帧读取组件并执行逻辑的对象。
```

再补一句：

```text
Scene 还负责生命周期、序列化、Prefab、运行时/编辑器模式切换、
实体查找缓存，以及系统注册和调度。
```

这比传统“一个场景里放一堆对象”更数据导向，也更适合做 2D 游戏。

## 4.4 `Scene`：世界容器

### 4.4.1 `Scene` 属于哪一层

`Scene` 是 **Scene ECS 层** 的核心。

它不是单纯的“场景文件”，而是：

- 世界容器
- 实体工厂
- 系统容器
- 序列化入口
- Play/Edit 模式切换点

### 4.4.2 `Scene` 的核心字段

`Scene` 里最重要的几个状态是：

```cpp
entt::registry m_Registry;
std::vector<Scope<ISystem>> m_Systems;
SavePolicy m_SavePolicy;
SceneExecutionMode m_ExecutionMode;
std::vector<entt::entity> m_DestroyQueue;
```

还有两个缓存：

```cpp
std::unordered_map<std::string, entt::entity> m_EntityNameCache;
std::unordered_map<UUID, entt::entity> m_EntityUUIDCache;
```

以及系统缓存：

```cpp
std::unordered_map<std::type_index, ISystem*> m_SystemCache;
```

所以 `Scene` 不只是一个“装实体的袋子”，它还维护了一些面向查询和运行时调度的基础设施。

### 4.4.3 构造和析构

构造时：

```cpp
Scene::Scene()
{
    RegisterCoreComponentLifecycles();
    ComponentLifecycleRegistry::BindScene(*this);
}
```

析构时：

```cpp
Scene::~Scene()
{
    ComponentLifecycleRegistry::UnbindScene(*this);
}
```

这说明场景一创建，就要把核心组件生命周期回调接上；场景销毁时也要把绑定关系解除。

### 4.4.4 一个场景怎么创建实体

```cpp
Entity Scene::CreateEntity(const std::string& name)
{
    return CreateEntityWithUUID(UUID(), name);
}
```

真正的创建逻辑在：

```cpp
Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
{
    Entity entity = { m_Registry.create(), this };
    entity.AddComponent<IDComponent>(uuid);
    entity.AddComponent<TransformComponent>();
    entity.AddComponent<TagComponent>().Tag =
        name.empty() ? "Entity" : name;
    InvalidateEntityLookupCache();
    return entity;
}
```

这行代码说明了一个非常重要的事实：

```text
Entity 不是先天就“有名字、有位置”的。
Scene 创建它时，会先补上最基础的三个组件：
ID、Transform、Tag。
```

### 4.4.5 具体例子：在编辑器里点“新建实体”

当你在 Hierarchy 里点“Create Entity”时，本质上是：

```text
Scene::CreateEntity("New Entity")
    ↓
entt::registry.create()
    ↓
IDComponent
    ↓
TransformComponent
    ↓
TagComponent
```

于是这个实体马上就能：

- 在层级树里显示名字
- 在场景里有变换
- 被序列化为 `.wt`
- 被编辑器选择和复制

### 4.4.6 销毁实体为什么分两种

Wheatear 提供两种销毁：

```cpp
DestroyEntityImmediate(Entity entity)
DestroyEntity(Entity entity)
```

#### 立即销毁

```cpp
void Scene::DestroyEntityImmediate(Entity entity)
```

会先检查句柄是否有效，然后立即通知所有系统：

```cpp
for (auto& system : m_Systems)
    system->OnEntityDestroy(this, entity);

m_Registry.destroy(static_cast<entt::entity>(entity));
```

#### 延迟销毁

```cpp
void Scene::DestroyEntity(Entity entity)
{
    m_DestroyQueue.push_back(
        static_cast<entt::entity>(entity));
}
```

真正统一处理在帧末：

```cpp
FlushDestroyQueue()
FlushDestroyQueueEditor()
```

### 4.4.7 为什么要延迟销毁

因为系统在更新时可能正在遍历 registry。

如果一边遍历一边直接销毁，很容易出现：

- 迭代器失效
- 组件被提前释放
- 系统之间顺序不稳定

所以 Scene 把销毁改成了“先排队，后统一处理”。

这和 `Application` 延迟增删 Layer 是同一个思想。

### 4.4.8 具体例子：战斗里敌人死亡

在横板战斗里，敌人死亡后通常不会在当前回调栈里直接立刻“整棵树删除”。

更常见的是：

```text
命中判定
    ↓
标记死亡
    ↓
Scene::DestroyEntity(entity)
    ↓
本帧结束统一 FlushDestroyQueue()
```

这样系统仍然可以在这一帧里完成：

- 播放死亡表现
- 结算伤害
- 通知 UI
- 通知掉落系统

### 4.4.9 `OnRuntimeStart` / `OnEditorStart`

Scene 的模式切换非常关键：

```cpp
void Scene::OnRuntimeStart()
void Scene::OnRuntimeStop()
void Scene::OnEditorStart()
void Scene::OnEditorStop()
```

#### Runtime start

```cpp
StopSystems();
ClearSavePolicyOverrides();
ConfigureRuntimeSystems();
StartSystems(SceneExecutionMode::Runtime);
```

#### Editor start

```cpp
StopSystems();
ConfigureEditorSystems();
StartSystems(SceneExecutionMode::Edit);
```

也就是说：

```text
同一份 Scene 数据，
根据模式挂不同的系统集。
```

### 4.4.10 Runtime 和 Editor 的系统集不同

运行时系统：

- Physics
- Animation
- SpriteSheet
- Audio
- UISystem
- 玩法模块注册进来的系统
- RenderSystem

编辑器系统：

- Animation
- SpriteSheet
- UISystem
- RenderSystem

编辑器不需要在场景编辑态跑完整物理和玩法逻辑，而运行时需要。

### 4.4.11 `OnUpdateRuntime()`

运行时更新的关键逻辑是：

```cpp
FlushDestroyQueue();

PhysicsSystem* deferredPhysics = nullptr;
for (auto& system : m_Systems)
{
    if (auto* physics = dynamic_cast<PhysicsSystem*>(system.get()))
    {
        deferredPhysics = physics;
        continue;
    }

    system->OnUpdateRuntime(this, ts);

    if (dynamic_cast<SpriteSheetSystem*>(system.get()) && deferredPhysics)
    {
        deferredPhysics->OnUpdateRuntime(this, ts);
        deferredPhysics = nullptr;
    }
}

if (deferredPhysics)
    deferredPhysics->OnUpdateRuntime(this, ts);
```

### 4.4.12 为什么物理要延后到 SpriteSheet 后面

因为动画和图集可能会驱动碰撞盒变化。

所以顺序是：

```text
Animation / SpriteSheet
    → 更新当前帧的视觉和关联数据
Physics
    → 再根据最新状态步进物理
```

这样可以避免“上一帧的碰撞盒”滞后太久。

### 4.4.13 `OnUpdateEditor()`

编辑器态更新逻辑：

```cpp
FlushDestroyQueueEditor();

for (auto& system : m_Systems)
    system->OnUpdateEditor(this, ts);

if (auto* render = GetSystem<RenderSystem>())
    render->RenderWithEditorCamera(this, camera, false);
```

说明编辑器模式下 Scene 仍然会更新系统，但它会用编辑器相机做渲染。

### 4.4.14 `OnViewportResize()` 和 `SetViewportOffset()`

`Scene` 会把视口变化传给所有非固定宽高比的相机：

```cpp
for (auto e : m_Registry.view<CameraComponent>())
{
    auto& cc = m_Registry.get<CameraComponent>(e);
    if (!cc.FixedAspectRatio)
        cc.Camera.SetViewportSize(width, height);
}
```

偏移量则会传给 UISystem：

```cpp
if (auto* ui = GetSystem<UISystem>())
    ui->SetViewportOffset(x, y);
```

所以 Scene 不只是“装对象”，它还负责把窗口变化传播给系统。

### 4.4.15 `GetPrimaryCameraEntity()`

主相机的判断很直接：

```cpp
for (auto e : m_Registry.view<CameraComponent>())
    if (m_Registry.get<CameraComponent>(e).Primary)
        return { e, this };
```

这说明当前引擎不是“摄像机对象自己掌控全局”，而是场景里约定某个相机组件为 Primary。

## 4.5 `Entity`：轻量句柄

### 4.5.1 `Entity` 属于哪一层

`Entity` 仍然属于 **Scene ECS 层**，但它不是一个厚对象。

它的定义非常短：

```cpp
entt::entity m_EntityHandle = entt::null;
Scene* m_Scene = nullptr;
```

### 4.5.2 为什么 Entity 不是“对象本体”

因为真正的数据在 `entt::registry` 里。

`Entity` 只是一个方便调用的包装：

```cpp
Entity entity{ handle, scene };
entity.AddComponent<TransformComponent>();
entity.GetComponent<TagComponent>();
entity.HasComponent<CameraComponent>();
```

### 4.5.3 `Entity::operator bool()`

```cpp
operator bool() const
{
    return m_EntityHandle != entt::null
        && m_Scene != nullptr
        && m_Scene->GetRegistry().valid(m_EntityHandle);
}
```

这是很重要的安全门：

```text
Entity 不是只要有 handle 就算活着，
它必须在 registry 里仍然有效。
```

这能防止拿着已经销毁的句柄继续访问组件。

### 4.5.4 具体例子：选中一个已经被删掉的实体

编辑器里经常会发生：

```text
你选中了一个对象
它在另一处被删除了
UI 还保存着这个引用
```

如果没有 `operator bool()` 的有效性检查，很多代码会把悬空句柄当成活对象用。

### 4.5.5 `AddComponent` / `GetComponent` / `RemoveComponent`

`Entity.inl` 里这些方法本质上只是把调用转发到 registry：

```cpp
m_Scene->m_Registry.emplace<T>(...)
m_Scene->m_Registry.get<T>(...)
m_Scene->m_Registry.remove<T>(...)
```

但它还会在 ID / Tag 变动时失效查找缓存：

```cpp
if constexpr (std::is_same_v<T, IDComponent>
    || std::is_same_v<T, TagComponent>)
    m_Scene->InvalidateEntityLookupCache();
```

### 4.5.6 为什么要有 ID / Tag 缓存失效

因为场景里经常需要按名字或 UUID 快速找实体：

```cpp
Scene::GetEntityByUUID(UUID uuid)
Scene::GetEntityByName(const std::string& name)
```

如果实体名字改了，但缓存没失效，就会查错对象。

### 4.5.7 具体例子：Inspector 改 Tag

当你在编辑器 Inspector 里修改一个实体名时：

```text
TagComponent 变了
    ↓
InvalidateEntityLookupCache()
    ↓
下一次查询时重建缓存
```

这样层级树、按钮绑定、脚本引用才不会继续拿旧名字。

## 4.6 `Component`：数据和生命周期

### 4.6.1 组件是数据，不是逻辑主体

Wheatear 的组件分得比较清楚：

- `IDComponent`
- `TagComponent`
- `TransformComponent`
- `CameraComponent`
- `SpriteRendererComponent`
- `UIWidgetComponent`
- `Rigidbody2DComponent`
- `VisualNovelComponent`
- `SideCombatLevelComponent`
- 等等

它们承担的是“描述状态”。

逻辑真正放在：

- `System`
- `Service`
- `Runtime`
- `EditorLayer`

### 4.6.2 一些组件为什么有运行时缓存

有些组件里会带运行时指针或运行时句柄，例如物理 body、fixture、动画运行态等。

这些字段通常不是纯数据，而是：

```text
为了运行时性能和同步方便而存在的缓存
```

它们的特点是：

- 可能不适合直接序列化
- 可能不能直接拷贝给另一个实体
- 需要在运行时重新创建或重连

### 4.6.3 组件生命周期注册表

这是很值得面试讲的一个细节。

`ComponentLifecycleRegistry` 提供：

```cpp
RegisterOnAdded<Component>()
RegisterOnUpdated<Component>()
RegisterOnRemoved<Component>()
BindScene(Scene& scene)
UnbindScene(Scene& scene)
```

它通过 entt 的信号机制，在组件构造、更新、销毁时触发回调。

### 4.6.4 具体例子：CameraComponent 自动同步视口

核心注册里有一条非常典型的回调：

```cpp
void ApplyCameraViewport(Scene& scene, Entity, CameraComponent& component)
{
    if (scene.GetViewportWidth() > 0 && scene.GetViewportHeight() > 0)
        component.Camera.SetViewportSize(
            scene.GetViewportWidth(),
            scene.GetViewportHeight());
}
```

它被注册到：

```cpp
RegisterOnAdded<CameraComponent>(...)
RegisterOnUpdated<CameraComponent>(...)
```

于是你可以得到一个很自然的结果：

```text
我在编辑器里新加一个 CameraComponent
→ 它会自动吃到当前场景视口大小
→ 不需要我手动再点一次“适配视口”
```

这个例子特别适合说明：

```text
组件不只是数据，也可以通过生命周期回调和系统联动。
```

### 4.6.5 哪些东西适合放进组件

适合：

- 位置、旋转、缩放
- 名称、UUID
- 相机参数
- 精灵、材质、灯光
- UI 布局
- 角色状态
- 玩法配置

不太适合：

- 大量全局控制逻辑
- 复杂流程编排
- 引擎对象之间的隐式连接

## 4.7 `System`：逻辑和调度

### 4.7.1 `ISystem` 的接口

```cpp
virtual void OnRuntimeStart(Scene* scene) {}
virtual void OnRuntimeStop(Scene* scene) {}
virtual void OnEditorStart(Scene* scene) { OnRuntimeStart(scene); }
virtual void OnEditorStop(Scene* scene) { OnRuntimeStop(scene); }
virtual void OnUpdateRuntime(Scene* scene, Timestep ts) {}
virtual void OnUpdateEditor(Scene* scene, Timestep ts) {}
virtual void OnEntityCreated(Scene* scene, Entity& entity) {}
virtual void OnEntityDestroy(Scene* scene, Entity& entity) {}
```

这就是系统生命周期。

### 4.7.2 为什么有 Runtime / Editor 两套回调

因为编辑器和运行时虽然共享一份场景数据，但行为不完全一样。

比如：

- 编辑器态要显示 gizmo、面板、预览
- 运行时态要跑完整物理、战斗逻辑、剧情逻辑

### 4.7.3 `Scene::GetSystem<T>()`

`Scene` 里有一个系统类型缓存：

```cpp
std::unordered_map<std::type_index, ISystem*> m_SystemCache;
```

`GetSystem<T>()` 会懒重建这个 cache，避免每次都线性扫一遍系统列表。

这对一些高频调用很有用，比如：

- RenderSystem
- UISystem
- AnimationSystem

### 4.7.4 `SceneSystemRegistry`

这是扩展系统的注册点：

```cpp
SceneSystemRegistry::RegisterRuntimeSystem(
    "VisualNovel",
    [] { return CreateScope<VisualNovelSystem>(); });
```

或者：

```cpp
SceneSystemRegistry::ForEachRuntimeSystem(...)
```

Scene 在 `ConfigureRuntimeSystems()` 里会把这些系统挂进去。

### 4.7.5 为什么要有这个注册表

它允许玩法模块把自己的系统插进通用 Scene 流程里，而不是把所有玩法都硬编码在 Scene.cpp。

这很适合这种项目：

```text
核心引擎固定，
玩法模块可插拔。
```

### 4.7.6 具体例子：运行时系统顺序

`Scene::ConfigureRuntimeSystems()` 的顺序大致是：

```text
Physics
Animation
SpriteSheet
Audio
UI
玩法模块系统
Render
```

这说明系统顺序不是随机的，而是有意安排的。

### 4.7.7 具体例子：为什么 Physics 要延后

如果某个角色的碰撞盒跟动画帧有关，那么先更新动画，再更新物理更合理。

所以 `OnUpdateRuntime()` 才会把 Physics 挪到 SpriteSheet 之后执行。

这是一个很典型的“系统顺序决定结果”的例子。

## 4.8 `SceneSerializer`：保存、加载、Prefab

### 4.8.1 `SceneSerializer` 属于哪一层

序列化仍然属于 **Scene ECS 层**，但它和资源系统、编辑器、Prefab 密切相关。

### 4.8.2 场景保存结构

`SerializeYaml()` 会写成类似：

```text
Scene
SavePolicy
Entities
```

其中 `Entities` 里逐个写实体和它的组件。

### 4.8.3 实体怎么被写入

核心逻辑是：

```cpp
SerializeCoreSceneComponents(out, entity);
SerializeAnimationSceneComponents(out, entity);
SerializeScriptingSceneComponents(out, entity);
SerializeModuleSceneComponents(out, entity);
SerializeUISceneComponents(out, entity);
```

这说明序列化不是一大坨，而是按领域拆开的。

### 4.8.4 组件分组

序列化分组来自：

- Core
- Animation
- Scripting
- Module
- UI

这非常适合解释“哪些是基础场景数据，哪些是玩法模块数据，哪些是运行时 UI 数据”。

### 4.8.5 `DeserializeYaml()`

加载时会把 YAML 重新变成实体和组件。

如果某个实体解析失败，会记录错误并丢弃该实体，而不是直接让整个场景崩掉。

这对于手改 YAML 或兼容旧版本场景很重要。

### 4.8.6 Prefab

Prefab 相关 API：

```cpp
SerializePrefab(Entity entity, const std::filesystem::path& filepath)
DeserializePrefabEntities(const std::filesystem::path& filepath, Scene* scene)
DeserializePrefab(const std::filesystem::path& filepath, Scene* scene)
```

Prefab 的本质是：

```text
把一个实体树或实体子树导出成可复用模板，
再在别的场景里重新实例化。
```

### 4.8.7 具体例子：UI 模板

`SerializeUITemplate()` 会把选中的 UI 子树写成 `.wtuit`。

这个文件包含：

- UI 模板元数据
- prefab body

因此一个 UI 模板既能被编辑器当作素材管理，又能被运行时实例化。

### 4.8.8 prefab 为什么要重映射 UUID

如果 prefab 里有多层互相引用的实体，直接复制旧 UUID 会导致新实例和旧实例互相串线。

所以 `DeserializePrefabEntities()` 会构造一个：

```text
旧 UUID → 新 UUID
```

的映射表，然后把相关引用都重写。

### 4.8.9 具体例子：UI 子树实例化

假设你复制了一个包含：

- 面板
- 文本
- 按钮
- 滑条
- 进度条

的 UI 子树。

如果不重映射，按钮里的 `ParentEntity`、分页控件里的 `PagerEntity`、按钮命令里的实体引用都可能还指向旧实体。

所以 prefab 实例化时会把这些引用重新指向新实例。

### 4.8.10 gameplay 绑定为什么也要重写

一些玩法组件里会存字符串引用，比如：

- `SpeakerTextEntityName`
- `PlayerEntityName`
- `BossEntityName`
- `CommandBarEntityName`

这些字段既可能是普通名字，也可能是 `@UUID` selectors。

Prefab 实例化时会把 selector 文本里的旧 ID 改成新 ID，但保留普通名字不动。

这个规则很重要，因为：

```text
名字用于可读性，
UUID selector 用于复制后不串线。
```

## 4.9 `Scene::Copy()`：Play 模式为什么不会污染编辑场景

### 4.9.1 这是这一章最关键的设计之一

编辑器按下 Play 时，不是在原编辑场景上直接跑，而是复制出一份运行时场景：

```cpp
m_ActiveScene = Scene::Copy(m_EditorScene);
```

### 4.9.2 `Scene::Copy()` 做了什么

它会：

1. 创建一个新 Scene
2. 复制 viewport 和 SavePolicy
3. 按 ID + Tag 先创建所有实体
4. 按组件分组复制所有可复制组件

### 4.9.3 为什么要先创建实体，再复制组件

因为很多组件互相引用。

先把实体都建好，才有：

```text
旧 UUID -> 新 entt::entity
```

的映射，后面复制组件时才能把引用正确落到新实体上。

### 4.9.4 具体例子：编辑器 Play 模式

假设你在编辑器里调了一个场景：

- 玩家实体
- Boss 实体
- UI 实体

按下 Play 后：

```text
编辑场景
    ↓ Copy
运行时场景
```

此后运行时里：

- 玩家可以掉血
- Boss 可以死亡
- UI 可以变化

但编辑场景本体不会被脏掉。

### 4.9.5 为什么这比“直接在原场景上跑”更稳

因为 Play 模式里的行为经常是可破坏的：

- 删除实体
- 改数值
- 改状态机
- 修改运行时缓存

如果都直接写回编辑场景，就会让撤销、保存和调试都变得非常痛苦。

## 4.10 `Scene::DuplicateEntity()`：复制实体时要小心什么

### 4.10.1 复制和实例化不是完全一样

`DuplicateEntity()` 复制一个现有实体时，会：

```cpp
Entity newEntity = CreateEntity(entity.GetName());
CopyComponentsIfExist(AllCopyableSceneComponents{}, newEntity, entity);
```

### 4.10.2 为什么要重置物理运行时指针

复制后会把：

```cpp
Rigidbody2DComponent::RuntimeBody = nullptr
BoxCollider2DComponent::RuntimeFixture = nullptr
CircleCollider2DComponent::RuntimeFixture = nullptr
```

清空。

原因很直接：

```text
两个实体不能共享同一个 b2Body / fixture 指针，
否则销毁时会重复释放。
```

### 4.10.3 具体例子：复制一个带物理的敌人

如果你在编辑器里复制一个敌人：

```text
外观、配置、组件数据可以复制
Box2D 运行时 body 不能复制
```

新的实体需要在下一次物理步进时自己重新创建 body。

## 4.11 Entity 查找缓存和引用系统

### 4.11.1 为什么要有缓存

Scene 支持按 UUID 或名字查找实体：

```cpp
FindEntityByUUID(UUID uuid)
GetEntityByUUID(UUID uuid)
GetEntityByName(const std::string& name)
```

如果每次都遍历整个 registry，会很慢。

所以 Scene 会维护：

```cpp
m_EntityUUIDCache
m_EntityNameCache
```

### 4.11.2 `GetEntityByName()` 支持 `@UUID`

这一点很适合面试讲。

场景里的实体绑定既可以写成普通名字：

```text
Player
Boss
```

也可以写成 selector：

```text
@1234567890123456789
```

Scene 会先判断是否是 UUID selector，再按 UUID 查找。

这能避免“重命名就把引用打断”的问题。

### 4.11.3 具体例子：名字变了也不怕

如果一个按钮绑定的是：

```text
@UUID
```

那么你把实体名字从 `DialogText` 改成 `BodyText`，绑定仍然有效。

这对编辑器非常友好。

## 4.12 Unity / UE / Wheatear 的对照

### 4.12.1 Unity 怎么讲

Unity 经典工作流更接近：

```text
GameObject
    ↓
Component
    ↓
MonoBehaviour 脚本
```

它的世界是一个对象树，场景里挂很多对象和组件。

Unity 的 DOTS ECS 则是另一套更偏数据导向的体系，但不是传统主工作流。

### 4.12.2 UE 怎么讲

UE 更接近：

```text
UObject
Actor
Component
World
Subsystem
```

UE 不是纯 ECS。

它的强项是：

- UObject 反射
- GC
- 蓝图
- 工具链
- Actor / Component 的组合式工作流

### 4.12.3 Wheatear 怎么讲

Wheatear 更接近 **entt 风格的轻量 ECS**：

- `Scene` 持有 `entt::registry`
- `Entity` 只是 handle 包装
- `Component` 是纯数据
- `System` 通过 view 读取数据并更新

这和 Unity / UE 的对象中心思路不一样。

### 4.12.4 面试里最稳的对比说法

你可以这样说：

```text
Unity 经典模式是 GameObject / Component，
UE 是 UObject / Actor / Component / World，
Wheatear 则是 entt registry + Entity 句柄 + Components + Systems。

前两者更偏对象中心和工具链，
Wheatear 更偏轻量数据导向 ECS，
这样更适合我这个面向 2D 游戏的个人引擎。
```

### 4.12.5 面试官继续追问时怎么接

如果对方问：

```text
那为什么不直接做 Unity 那样的对象树？
```

可以答：

```text
因为我这个项目的目标不是做通用商业引擎，而是做一个适合 2D 游戏、工具链和玩法模块都能共用的轻量引擎。
ECS 更适合把渲染、UI、动画、战斗、剧情这些系统统一到一套数据模型里。
```

如果对方问：

```text
UE 不也是组件式吗？
```

可以答：

```text
UE 的确是组件式，但它的基础对象体系是 UObject / Actor / World，
并且有反射和 GC 这一整套底层支撑。
Wheatear 没有那套重量级对象系统，而是直接用 entt 做数据导向的 registry。
```

## 4.13 本章的优点和代价

### 优点一：轻量

实体本体很薄，数据和逻辑边界清楚。

### 优点二：适合复制和序列化

场景、Prefab、UI 模板都能围绕统一的 registry 做文章。

### 优点三：适合模块化玩法

Gameplay 模块可以向 Scene 注册系统，也可以挂自己的组件序列化。

### 代价一：要更小心句柄有效性

因为 Entity 不是厚对象，删了就可能变成悬空句柄。

### 代价二：要更小心系统顺序

Physics、Animation、UI、Render 的顺序会影响结果。

### 代价三：要更小心运行时缓存

物理 body、fixture、runtime pointers 不能随便复制。

## 4.14 面试回答模板

### 问题：你这个引擎的场景是怎么表示的？

```text
Wheatear 的 Scene 持有 entt::registry。
Entity 只是 registry 里的 handle 包装，不是厚对象。
Component 是数据，System 读组件并执行逻辑。
Scene 还负责系统生命周期、序列化、Prefab、Play 模式复制和实体查找缓存。
```

### 问题：为什么不用 GameObject 树？

```text
因为我这个项目更偏数据导向。
ECS 更适合把渲染、UI、动画、物理和玩法模块统一到同一套组件数据上。
而且它更方便复制、序列化和做模块化系统。
```

### 问题：Unity 和 UE 里类似的东西是什么？

```text
Unity 经典工作流是 GameObject / Component，UE 是 UObject / Actor / Component / World。
Wheatear 更接近 entt 风格的轻量 ECS。
```

### 问题：为什么 Play 模式要复制场景？

```text
为了避免运行时修改污染编辑场景。
Play 模式里会修改实体、状态和物理缓存，所以要先 Scene::Copy 出一份运行时副本。
```

## 4.15 新人练习

### 练习一：创建实体

自己追：

```text
Scene::CreateEntity("Player")
    ↓
IDComponent
TransformComponent
TagComponent
```

### 练习二：复制实体

自己追：

```text
Scene::DuplicateEntity()
    ↓
CopyComponentsIfExist(...)
    ↓
RuntimeBody / RuntimeFixture 清空
```

### 练习三：切 Play 模式

自己追：

```text
EditorLayerBase::TransitionToPlay()
    ↓
Scene::Copy(m_EditorScene)
    ↓
OnRuntimeStart()
```

### 练习四：查找实体

自己追：

```text
GetEntityByName()
GetEntityByUUID()
InvalidateEntityLookupCache()
RebuildEntityLookupCaches()
```

### 练习五：改一个 CameraComponent

自己看：

```text
为什么给 CameraComponent 增加或更新后，
它会自动吃到当前视口尺寸？
```

答案就在 `ComponentLifecycleRegistry`。

## 4.16 本章小结

这一章的核心结论是：

```text
Scene 是世界容器，
Entity 是轻量句柄，
Component 是数据，
System 是逻辑，
Serializer 负责持久化，
Copy 负责 Play 模式隔离，
ComponentLifecycleRegistry 负责组件联动。
```

再压缩成一句：

```text
Wheatear 用 entt 组织世界，用 Scene 调度系统，用 Serializer 和 Copy 把编辑态、运行态、Prefab 串成一条完整链路。
```

如果回看已经完成的内容，前面的 **渲染地基** 已经解释了实体如何被画出来。下一章建议继续拆 **场景生命周期与切换**，因为当 Scene / Entity 真正建立起来后，下一件最自然的事就是：

```text
场景如何创建、复制、运行、停止和切换？
```
