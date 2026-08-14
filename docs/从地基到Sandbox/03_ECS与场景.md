# Part 3 · ECS 与场景：实体、组件、序列化与双系统集

> 目标：理解实体组件系统（ECS）的组织方式、YAML 场景序列化机制，
> 以及"编辑器与运行时共用一份场景数据、两套系统行为"的核心设计。
> 结尾用 `Example.wt` 场景加载走完整条链路。

## 3.1 为什么是 ECS 而不是 GameObject 树

Unity 式 GameObject 树把"物体"建模为**继承树**：`MonoBehaviour` 派生类
持有自己的更新逻辑，每帧引擎遍历场景里的物体调用 `Update()`。
ECS 反过来：

- **实体（Entity）只是 ID**：一个整数句柄，不包含任何逻辑。
- **组件（Component）是纯数据**：POD 结构体（`TagComponent`、`TransformComponent`…），
  按类型存放在连续数组里（SoA 布局）。
- **系统（System）是逻辑**：每帧遍历"拥有某组组件"的实体执行逻辑。

三个直接收益：

1. **缓存友好**：同一类型的组件在内存里连续排列，遍历一万个 `TransformComponent`
   就是一次线性扫描，没有指针跳跃。
2. **组合优于继承**：一个敌人 = `Transform + SpriteRenderer + SpriteAnimator +
   BoxCollider2D + SideCombatant`；一个背景 = `Transform + SpriteRenderer`。
   没有"基类膨胀"，功能就是组件拼装。
3. **数据驱动序列化**：组件是 POD，序列化天然简单——这也直接支撑了
   本项目的核心工作流："场景是 YAML 文件，编辑器与运行时共用"。

引擎底层用 **entt**（header-only ECS 库）：`entt::registry` 是组件容器，
`registry.view<T1, T2>()` 返回"同时拥有 T1 和 T2"的实体视图。
entt 放进 PCH（Part 1 讲过），31 个 TU 共享解析成本。

## 3.2 Entity：实体的门面

`Scene/Entity.h` 的 `Entity` 是 `entt::entity` 句柄 + `Scene*` 的轻量封装：

```cpp
class Entity
{
public:
    Entity(entt::entity handle, Scene* scene);

    template<typename T, typename... Args>
    T& AddComponent(Args&&... args)
    {
        return m_Scene->GetRegistry().emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
    }

    template<typename T>
    T& GetComponent() { return m_Scene->GetRegistry().get<T>(m_EntityHandle); }

    template<typename T>
    bool HasComponent() const { return m_Scene->GetRegistry().all_of<T>(m_EntityHandle); }

    UUID GetUUID() const;
    const std::string& GetName() const;
    Scene* GetScene() const { return m_Scene; }
    // ...
};
```

关键点：

- **Entity 是值类型**（句柄 + 指针），拷贝廉价，到处传值。
- 所有组件操作转发给 `registry`——Entity 只是语法糖，
  真正干活的是 ECS 容器。
- `GetUUID()` 读 `IDComponent`（每个实体创建时自动附加），
  场景引用、命令寻址（`anim:play:@UUID:...`）都靠它。

## 3.3 组件组织：主题头 + 模块外置

`Scene/Components/` 下按主题分文件：

```
AnimationComponents.h   （SpriteAnimatorComponent...）
AudioComponents.h       （AudioSourceComponent...）
CameraComponents.h      （CameraComponent...）
CoreComponents.h        （Tag/Transform/ID...）
Physics2DComponents.h   （Rigidbody2D/BoxCollider2D/CircleCollider2D）
RenderingComponents.h   （SpriteRenderer/CircleRenderer/MeshRenderer...）
UIComponents.h          （UICanvas/UIWidget/UIImage/UIText/UIButton...）
```

`Components.h` 是聚合头，但它的注释写了重要纪律：

> 新组件放进主题头；聚合头只在"所有消费者都需要"时才加 include。
> **模块组件（VisualNovel/SideCombat/...）在各自模块头里，不放进聚合头**。

为什么？**编译时间**。战斗模块的 `SideCombatantComponent` 如果进了聚合头，
VN 模块的每个 TU 都要重新解析它；按需 include 主题头，改动一个模块
组件只重编该模块的 TU。配合 Part 1 的 PCH，这是"增量编译 3 秒"的
另一半功臣。

## 3.4 模板序列化器：组件 ↔ YAML 的桥梁

场景文件是 YAML（`.wt`）。序列化分两层：

### 3.4.1 ComponentSerializer<T>：每种组件一个特化

`Serialization/SceneSerializerCoreComponents.cpp`（按主题分 TU）：

```cpp
template<> struct ComponentSerializer<SpriteRendererComponent>
{
    static constexpr const char* Key = "SpriteRendererComponent";

    static void Serialize(YAML::Emitter& o, const SpriteRendererComponent& c)
    {
        o << YAML::Key << Key << YAML::BeginMap;
        o << YAML::Key << "Color" << YAML::Value << c.Color;
        o << YAML::Key << "Texture" << YAML::Value << (c.Texture ? c.Texture->GetPath() : "");
        // ... SpriteSheet / CellIndex / SubRect / PixelsPerUnit ...
        o << YAML::EndMap;
    }

    static void Deserialize(const YAML::Node& n, SpriteRendererComponent& c) { ... }
};
```

新增一个组件 = 写一个特化，**序列化器与组件一一对应**，
没有注册表、没有反射、没有宏魔法——模板特化就是全部机制。

### 3.4.2 批量调度：ComponentGroup + 折叠表达式

`Serialization/SceneSerializerComponentSupport.h`：

```cpp
template<typename... Ts>
void SerializeComponents(ComponentGroup<Ts...>, YAML::Emitter& out, Entity entity)
{
    ([&]
    {
        if (entity.HasComponent<Ts>())
            ComponentSerializer<Ts>::Serialize(out, entity.GetComponent<Ts>());
    }(), ...);   // C++17 折叠表达式：逐个组件尝试
}
```

每个主题一个组件组（`SceneSerializerCoreComponents.cpp`）：

```cpp
using CoreSceneComponents = ComponentGroup<
    TagComponent, TransformComponent, CameraComponent,
    SpriteRendererComponent, CircleRendererComponent,
    Rigidbody2DComponent, BoxCollider2DComponent, CircleCollider2DComponent,
    MeshRendererComponent, DirectionalLightComponent, PointLightComponent>;
```

再按模块拆分 TU（Core / Animation / UI / Scripting / 四个玩法模块各一个），
每个模块的组件组独立编译——**改战斗组件序列化器不会重编 VN**。

### 3.4.3 实体块结构

序列化一个实体的产物（真实场景片段，`SideCombatBeastPath.wt`）：

```yaml
- Entity: 946201001
  TagComponent:
    Tag: SC_SkillIcon_J
  TransformComponent:
    Translation: [0, 0, 0]
    Rotation: [0, 0, 0]
    Scale: [1, 1, 1]
  UIWidgetComponent:
    Visible: true
    Position: [0.898, 0.810]
    Size: [0.072, 0.128]
    SortOrder: 64
    ParentEntity: 946209004
  UIImageComponent:
    TexturePath: assets/vertical_slice/side_combat/ui/sidecombat_ui_sheet.png
    SpriteSheet: assets/vertical_slice/side_combat/ui/sidecombat_ui_sheet.wtsheet
    SubRect: SC_SkillIcon_J
    UVMin: [0.112891, 0.170833]
  UIButtonComponent:
    OnClickFunction: "side:basic"
```

注意 `ParentEntity`——UI 通过 **UUID 引用**建立层级（父实体 UUID），
不是 YAML 嵌套树。序列化/反序列化时按 UUID 重建父子关系
（`SceneHierarchy` 树是查询结果，不是存储结构）。

## 3.5 系统生命周期与双系统集

`Systems/ISystem.h` 定义系统的全部钩子：

```cpp
class ISystem
{
    virtual void OnRuntimeStart(Scene* scene) {}     // 进入运行时
    virtual void OnRuntimeStop(Scene* scene) {}
    virtual void OnEditorStart(Scene* scene) {}      // 进入编辑态（默认转调 Runtime 钩子）
    virtual void OnEditorStop(Scene* scene) {}
    virtual void OnUpdateRuntime(Scene* scene, Timestep ts) {}  // 每帧
    virtual void OnUpdateEditor(Scene* scene, Timestep ts) {}
    virtual void OnEntityCreated(Scene* scene, Entity& entity) {}  // 实体钩子
    virtual void OnEntityDestroy(Scene* scene, Entity& entity) {}
};
```

`Scene` 按执行模式注册**两套系统集**（`Scene.cpp`）：

```cpp
void Scene::ConfigureRuntimeSystems()
{
    RegisterSystem<AnimationSystem>();
    RegisterSystem<SpriteSheetSystem>();
    RegisterSystem<PhysicsSystem>();
    RegisterSystem<VisualNovelSystem>();
    RegisterSystem<SideCombatSystem>();
    // ...
}

void Scene::ConfigureEditorSystems()
{
    RegisterSystem<AnimationSystem>();     // 编辑态也有它
    RegisterSystem<SpriteSheetSystem>();
    // 没有 PhysicsSystem（Box2D 世界只在运行时创建）
    // 没有玩法系统（VN/战斗不在编辑器里跑）
}
```

`Scene::OnUpdateRuntime` 遍历 `m_Systems` 调 `OnUpdateRuntime`；
`OnUpdateEditor` 同理。`SceneExecutionMode`（Edit/Runtime）由
`Scene::StartSystems` 记录，`SceneState` 切换时调 `OnEditorStop/OnRuntimeStart`。

**同一个系统可以同时出现在两套系统集里，行为自己区分**——看
`AnimationSystem` 是怎么做的（Part 6 详讲）：

```cpp
void AnimationSystem::OnUpdateEditor(Scene* scene, Timestep ts)
{
    if (!m_EditorPreviewActive)
        SyncEditorPreviewFrame(scene);   // 编辑态：把第一帧同步到精灵上（预览）
}
void AnimationSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
{
    UpdateAnimations(scene, ts);         // 运行态：完整播放
}
```

这就是"**编辑器所见即所得**"的机制：编辑态跑"预览版"系统
（SpriteSheet 解析、动画第一帧、碰撞框跟随都在编辑态生效），
运行态跑"完整版"系统。玩家在编辑器里看到的画面
与按下 Play 后看到的几乎一致——**同一份场景数据，两套行为**。

## 3.6 实体生命周期与销毁队列

- `Scene::CreateEntity(name)`：创建实体 + 自动附加 `IDComponent`（UUID）+
  `TagComponent` + `TransformComponent`，触发各系统的 `OnEntityCreated`。
- 销毁走**延迟队列**：`RequestDestroy(entity)` 把实体放进待删列表，
  `FlushDestroyQueue()` 在每帧更新开头统一删除。为什么延迟？
  因为销毁可能在遍历（view）中途发生——"不要在循环里改容器"的纪律，
  和 Part 1 的 Layer 待办队列同源。

## 3.7 实例走读：Example.wt 从文件到画面

打开一个最简单场景 `assets/scenes/Example.wt` 的加载链路：

1. **读取**：`SceneSerializer::DeserializeYaml(path)` 用 yaml-cpp 解析文件
   → 遍历 `- Entity:` 块。
2. **重建实体**：每个块 `CreateEntityWithUUID(uuid, tag)` →
   按各模块组件组调用 `DeserializeComponents`——折叠表达式里
   每个 `ComponentSerializer<Ts>::Deserialize` 检查 YAML 是否有自己的
   `Key`，有则填充（没有该组件则 `AddComponent`）。
3. **重建层级**：UI 组件里的 `ParentEntity` UUID 在反序列化后统一
   `SetParent`（编辑器层级树依赖它）。
4. **启动**：编辑器打开场景 → `OnEditorStart` → 编辑系统集开始跑
   （精灵显示、动画预览帧、sheet 解析）。
5. **渲染**：每帧 `OnUpdateEditor` → 渲染循环把可见实体提交给
   Renderer2D（Part 2）→ 视口 framebuffer → ImGui 贴到窗口。

> 面试可讲：序列化是"组件组的折叠表达式"而不是反射/代码生成——
> 模板特化 + C++17 fold，新增组件零框架负担；拆分 TU 控制编译面。

## 3.8 当前状态

- ✅ entt 驱动的实体/组件/系统（Entity 门面 + registry）
- ✅ 主题化组件头 + 模块组件外置（编译面控制）
- ✅ 模板序列化器（ComponentGroup 折叠表达式，按主题分 TU）
- ✅ 双系统集（编辑态预览版 / 运行态完整版，SceneExecutionMode）
- ✅ 实体生命周期（延迟销毁队列、UUID 层级）

下一步：**Part 4 资产管线**——路径解析双路径、资产注册表、
热重载降频与打包。
