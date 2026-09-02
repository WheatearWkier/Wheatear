# 第 3 编 03：图集与 SpriteSheet

> 本章所属层级：**引擎服务层**
>
> 本章目标：理解一张大图怎么被组织成可复用的图集，怎么切成格子、命名子矩形、热重载，最后又怎么喂给 `SpriteRendererComponent`、`UIImageComponent` 和动画系统。
>
> 这一章只讲“图集定义、切片和运行时解析”。  
> 下一章的 `Animation` 会在这个基础上，继续讲“时间到了以后到底该显示哪一帧”。

如果说资产章回答的是“资源从哪来”，那这一章回答的就是：

```text
一张贴图进到引擎以后，怎么变成“第 12 格”“名叫 face_smile 的子图”“带碰撞框的走路帧”？
```

Wheatear 里图集不是单纯为了省显存，更重要的是它把 2D 内容生产流程统一了：

```text
原始图片
    ↓
.wtsheet 图集定义
    ↓
SpriteSheetAsset::Load / Save / ResolveCell
    ↓
SpriteSheetSystem
    ├─ SpriteRendererComponent
    ├─ UIImageComponent
    └─ BoxCollider2DComponent (FollowAnimation)
    ↓
Renderer2D / UI 渲染
```

在 2D 游戏里，这层特别值钱。因为常见需求几乎都靠它吃饭：

- 角色走路、攻击、待机帧
- UI 图标、头像、技能按钮
- 对话立绘、表情切换
- 带 trim 的不规则帧
- 带碰撞框的动作帧

---

## 3.3.1 本章要读的源码

```text
Wheatear/src/Wheatear/Assets/SpriteSheetAsset.h
Wheatear/src/Wheatear/Assets/SpriteSheetAsset.cpp
Wheatear/src/Wheatear/Systems/SpriteSheetSystem.h
Wheatear/src/Wheatear/Systems/SpriteSheetSystem.cpp

Wheatear/src/Wheatear/Scene/Components/RenderingComponents.h
Wheatear/src/Wheatear/Scene/Components/UIComponents.h
Wheatear/src/Wheatear/Scene/Components/Physics2DComponents.h
```

如果先抓主线，就先看这三个：

```text
SpriteSheetAsset.h
SpriteSheetAsset.cpp
SpriteSheetSystem.cpp
```

它们分别回答：

```text
图集文件长什么样？
怎么把格子、子矩形和碰撞框解析出来？
怎么把解析结果同步回实体组件？
```

---

## 3.3.2 这一层在架构中的位置

```text
应用层
    Editor / Sandbox
        ↓
引擎服务层
    AssetPath / Texture / SpriteSheetAsset / SpriteSheetSystem
        ↓
Scene ECS 层
    SpriteRendererComponent / UIImageComponent / BoxCollider2DComponent
        ↓
平台实现层
    OpenGLTexture / OpenGLRendererAPI
```

这一层很像“资源语义转换器”。
它不负责真正画图，但它负责告诉渲染器：

- 这张图该用哪一块 UV
- 这块图该对应哪个碰撞框
- 这块图是规则网格还是命名子图
- 编辑器改完图集后，哪些实体要自动跟着更新

这也是它为什么不应该被塞进渲染器内部。  
渲染器只想知道“纹理和 UV 是什么”，而图集系统负责回答“这两个值应该怎么来”。

---

## 3.3.3 `.wtsheet`：图集定义文件长什么样

`SpriteSheetAsset` 把图集定义成一个普通的 YAML 资源。  
源码里的注释已经很直接了：这是一个“texture + grid layout”的复用资产。

一个典型的 `.wtsheet` 可以长成这样：

```yaml
texture: assets/sprites/hero.png
columns: 8
rows: 4

trims:
  - left: 4
    top: 6
    width: 52
    height: 48
    collider:
      left: 10
      top: 12
      width: 30
      height: 28

rects:
  - name: portrait_smile
    left: 512
    top: 16
    width: 128
    height: 128
```

这里有两个层次：

- `trims`：规则网格里的每一格可以再裁一层
- `rects`：不规则的命名子矩形，类似 Unity 的命名 Sprite

这两个层次不是重复，而是分别服务两类工作流：

- 规则网格适合战斗动作帧、行走帧、序列帧
- 命名子矩形适合头像、UI 图标、立绘局部、特殊不规则素材

---

## 3.3.4 `SpriteSheetData`：图集的内存模型

在源码里，图集数据被拆成 `SpriteSheetData`：

- `TexturePath`
- `Columns`
- `Rows`
- `Trims`
- `Rects`

### `CellTrim`

`CellTrim` 是“格子内裁切”。
它表示：

- 这格里真正想要的内容框
- 可选的碰撞框

这对动画帧特别重要。因为很多美术帧都不是正方形，外面会留透明边。  
如果直接拿整格画，不但浪费空间，还会让碰撞和锚点很别扭。

### `NamedRect`

`NamedRect` 是“命名子图”。
它的用途更像：

- `portrait_smile`
- `icon_fire`
- `ui_button_hover`

这种东西不适合硬塞到 `CellIndex` 里，因为它不一定是规则网格。

---

## 3.3.5 加载、保存与热重载

### `Load()`

`SpriteSheetAsset::Load(path)` 会直接读 YAML 文件，解析：

- `texture`
- `columns`
- `rows`
- `trims`
- `rects`

如果格式有点乱，它会尽量保守处理，最后至少保证 `Columns >= 1`、`Rows >= 1`。  
这很像引擎里的很多资源读取逻辑：宁可回退默认值，也不要让编辑器一改文件就炸。

### `Save()`

`Save()` 会把当前 `SpriteSheetData` 再写回 YAML。
这很适合编辑器里的图集面板：

- 改裁切框
- 改 collider
- 改命名子图
- 改纹理路径

然后一键落盘。

### `GetCachedSheetData()`

这个接口是给编辑器和系统层用的缓存入口。
它的意义不是“省一点 IO”，而是：

- inspector 可以快速拿到子图名列表
- 运行时不必每帧重复读文件
- 图集文件改了以后能自动重新读取

### 热重载策略

`SpriteSheetAsset.cpp` 里有一个很实用的策略：

- 每个 sheet 有缓存
- 大约每 250ms 检查一次文件写入时间
- 写时间变了就重新加载

这个节奏挺聪明：

- 既能响应编辑器里手改文件
- 又不会让每帧都刷一堆文件系统调用

---

## 3.3.6 `ResolveCell()`：最关键的一步

`ResolveCell()` 做的是“把逻辑引用变成可直接渲染的结果”。

它会返回一个 `ResolvedCell`，里面有：

- `Texture`
- `UVMin`
- `UVMax`
- `ContentX / ContentY / ContentWidth / ContentHeight`
- 可选 `ColliderLeft / Top / Width / Height`

你可以把它理解成：

```text
图集声明
    ↓
可直接塞给渲染器和物理体的最终结果
```

### 1. grid 版本

`ResolveCell(sheetPath, cellIndex, out)` 适合规则网格。
它会先：

1. 读 sheet 缓存
2. 算总格子数
3. 根据 `CellIndex` 算列和行
4. 根据纹理大小和 `Columns / Rows` 算单格像素尺寸
5. 根据 `Trims` 决定是否再裁一层
6. 再算 UV 和碰撞框

### 2. named rect 版本

`ResolveCell(sheetPath, rectName, out)` 适合不规则子图。
它会直接在 `Rects` 里找同名项，然后按像素矩形返回结果。

### 3. UV 为什么要翻 V

源码里有一个很容易被忽略，但非常重要的点：

- 图集定义按“图片左上角”为起点理解
- OpenGL 的 UV 习惯里 `v=0` 在底部

所以 `CellUVMin` / `CellUVMax` 里会做 V 方向翻转。  
这个小细节不讲清楚，初学者很容易在面试里被问住。

---

## 3.3.7 `SpriteSheetSystem`：把图集结果推回实体

`SpriteSheetSystem` 才是把图集真正接到 Scene 里的那一层。

它会扫描：

- `SpriteRendererComponent`
- `UIImageComponent`

然后根据组件上的：

- `SpriteSheet`
- `CellIndex`
- `SubRect`

去做解析。

### 优先级

源码里很明确：

- `SubRect` 非空时优先
- 否则才用 `CellIndex`

这个优先级是合理的，因为命名子图往往更明确，适合 UI 和立绘裁片。

### 更新哪些字段

解析后，系统会把结果写回组件：

- `Texture`
- `UVMin`
- `UVMax`

这意味着后面的渲染器根本不用知道图集是怎么来的。  
它只要按组件里的纹理和 UV 画就行。

### 碰撞框联动

如果实体上有 `BoxCollider2DComponent`，并且：

- `FollowAnimation == true`
- 组件是 `SpriteRendererComponent`
- `PixelsPerUnit > 0`
- 图集 cell 自己带 collider

那 `SpriteSheetSystem::ApplyColliderToEntity()` 会自动把碰撞体改成当前帧对应的大小和偏移。

这一步很关键，因为它让“美术帧变化”能顺手带动“可碰撞区域变化”。

---

## 3.3.8 一个完整例子：角色走路帧

假设你有一个角色贴图 `hero.png`，配一个 `.wtsheet`：

```yaml
texture: assets/sprites/hero.png
columns: 6
rows: 4
trims:
  - { left: 2, top: 8, width: 46, height: 52,
      collider: { left: 10, top: 20, width: 24, height: 26 } }
```

Scene 里的实体可能是这样：

- `SpriteRendererComponent.SpriteSheet = "assets/sheets/hero.wtsheet"`
- `SpriteRendererComponent.CellIndex = 7`
- `SpriteRendererComponent.PixelsPerUnit = 100.0f`
- `BoxCollider2DComponent.FollowAnimation = true`

运行时流程就是：

```text
SpriteSheetSystem::ResolveSheets(scene)
    ↓
ResolveCell("hero.wtsheet", 7, resolved)
    ↓
SpriteRendererComponent.Texture = resolved.Texture
SpriteRendererComponent.UVMin/UVMax = resolved.UV
    ↓
ApplyColliderToEntity(...)
    ↓
Renderer2D 画出当前帧
```

所以这一层真正做的是：

```text
把“第 7 格”变成“能画、能碰、能热更新”的运行时数据。
```

---

## 3.3.9 一个完整例子：UI 头像和图标

UI 那边的 `UIImageComponent` 也能走同一套逻辑。

比如：

- `SpriteSheet = "assets/ui/icons.wtsheet"`
- `SubRect = "quest_accept"`

那系统就不用管它是角色图还是 UI 图标。  
它只负责把 `quest_accept` 这块子图的纹理和 UV 算出来。

这就是图集系统的价值：

- 角色和 UI 共享同一套资源逻辑
- 不同层级都能用同样的描述方式
- 以后换图、重排图集、改命名子图，不用改玩法代码

---

## 3.3.10 和别的引擎怎么对照

### Unity

Unity 的直观对照是：

- Sprite Atlas
- Sprite Editor
- 命名 Sprite
- 9-slice / slicing

Wheatear 的 `SpriteSheetAsset + SpriteSheetSystem` 和它的思路很接近：

- 一个资源描述图集
- 一个系统把描述变成渲染数据
- 编辑器可以改，运行时可以热更新

### Unreal

UE 里可以类比到：

- Paper 2D 的 Sprites
- Flipbook
- 从导入的 sprite sheet 生成分片

Wheatear 这边没有把这一层做成很重的编辑器资产体系，而是用 YAML 和系统解析去承接，思路更轻。

### Godot

Godot 里更像：

- `AnimatedSprite2D`
- `SpriteFrames`
- `TextureRect`
- `AtlasTexture`

它们也都在做同一件事：  
把一张大图拆成“可以逐帧播放的逻辑帧”。

---

## 3.3.11 常见误区

- 把 `CellIndex` 当成“纯渲染问题”，忘了 collider 也要跟着走
- 忘了 UV 的 V 方向和图片坐标系不是一回事
- 用了 `SubRect` 之后还以为 `CellIndex` 会生效
- 图集文件改了，但缓存没刷新
- `PixelsPerUnit` 没设，导致碰撞框尺寸不对
- 把图集只当“省 draw call 的技巧”，忽略了它还是内容组织方式

---

## 3.3.12 面试时怎么讲

如果面试官问你“你为什么要单独做 SpriteSheet 系统”，你可以这么答：

```text
因为我想把“资源描述”和“运行时使用”拆开。
SpriteSheetAsset 负责定义 texture、网格、裁切和命名子图；
SpriteSheetSystem 负责把这些定义同步到 SpriteRendererComponent 和 UIImageComponent；
如果某些 cell 带碰撞框，还能顺便驱动 BoxCollider2DComponent。
这样图集改动时，不用改玩法代码，Scene 里引用这些图的实体也能自动跟着更新。
```

如果继续追问“这和 Unity 的 Sprite Atlas 有什么区别”，你就补一句：

```text
思路上是一样的，都是把大图切成可复用的小单元；
区别只是我这边用 YAML 资源和 ECS 系统来承接，而不是一个更重的内置资产管线。
```

---

## 3.3.13 编辑器里怎么配图集

这一章如果只讲运行时，会少一块很关键的东西：**图集是怎么在编辑器里被改出来的**。

Wheatear 的思路不是把图集做成一套很重的导入管线，而是尽量让编辑器直接围着 `.wtsheet` 工作：

```text
选中 .wtsheet
    ↓
在 Inspector 里改 texture / columns / rows
    ↓
编辑 trims / rects / collider
    ↓
点保存
    ↓
SpriteSheetAsset::Save()
    ↓
文件写回 YAML
    ↓
SpriteSheetAsset::GetCachedSheetData() / ResolveCell() 感知写时间变化
    ↓
SpriteSheetSystem::ResolveSheets(scene)
    ↓
实体上的 SpriteRendererComponent / UIImageComponent 立刻更新
```

这个流程的价值很大，因为它把“改图集”和“看结果”之间的距离压得很短。  
新手最怕的就是：

- 改了半天，结果只能等导入
- 改了 collider，角色却还是老碰撞框
- 改了 trim，动画帧位置全乱但不知道为什么

Wheatear 这里的做法更直接：

### 1. 先改纹理和网格

在编辑器里先选一个贴图作为 `texture`，再设置 `columns` / `rows`。  
这一步决定：

- 一张大图会被切成多少格
- 每格大概多大
- 后续 `CellIndex` 怎么算

### 2. 再改 trim

`trims` 适合做两类事：

- 去掉透明边
- 给不规则帧一个更贴合的内容框

这一点对 2D 角色动画特别有用。  
很多动作帧不是标准方块，如果不 trim，人物就会在播放时“跳格子”。

### 3. 顺手配 collider

如果这格图后面还要拿来做碰撞，就直接把 collider 一起配掉。  
后面 `SpriteSheetSystem::ApplyColliderToEntity()` 就能根据 cell 的 collider 自动更新 `BoxCollider2DComponent`。

这比在战斗系统里手动猜碰撞框省事得多。

### 4. 命名子图给 UI 和立绘用

如果图集里有 UI 图标、头像、表情、按钮态，就用 `rects` 和 `SubRect`。
这样编辑器里看到的名字会比纯 `CellIndex` 更直观：

- `portrait_smile`
- `icon_sword`
- `button_hover`

这类资源一多，名字比数字更适合人读。

### 5. 保存后立即热更新

这套方案最舒服的地方在于：保存后不用重启引擎。

`SpriteSheetAsset.cpp` 里会按时间片检查文件写时间，发现变化就重读。  
所以编辑器里改一格 trim，场景里的角色和 UI 图标就能马上跟着变。

### 一个更具体的例子

假设你在做一个角色走路图集：

- `hero.png`
- `columns = 6`
- `rows = 4`
- 第 7 格是左走第二帧
- 第 7 格配了一个更窄的 collider

你在编辑器里把第 7 格的 trim 从 `52x54` 改成 `48x50`，点保存。  
随后：

1. `.wtsheet` 被写回磁盘
2. 缓存检测到写时间变化
3. `ResolveCell()` 重新计算 UV 和 collider
4. 场景里所有用这格图的实体同步更新

这就把图集从“静态切片表”变成了一个**能被编辑器直接驱动的内容资产**。

---

## 3.3.14 小结

这一章最重要的结论其实只有一句：

```text
图集系统不是单纯切图，它是在把“图片上的区域”变成“Scene 里可复用、可热更、可碰撞的内容单元”。
```

Wheatear 的这一层做了三件事：

- `SpriteSheetAsset` 定义图集
- `ResolveCell()` 解析格子和子图
- `SpriteSheetSystem` 把结果同步到实体和 UI

下一章再往前走一步，就是动画系统了。  
到了那里，我们就不再只问“这格图是什么”，而是要问：

```text
“这一秒该显示哪一格？” 
```

---

## 3.3.15 `SpriteSheetSystem` 在 Scene 里怎么跑

图集定义本身只负责“怎么切”。  
真正把切好的结果塞回实体组件的，是 `SpriteSheetSystem`。

它之所以放在 Scene 里，而不是塞到资产系统里，是因为它处理的是**运行时实体状态**：

- `SpriteRendererComponent`
- `UIImageComponent`
- `BoxCollider2DComponent`（当 `FollowAnimation = true` 时）

### 3.3.15.1 它什么时候执行

`Scene` 在进入编辑态或运行态时，会把系统注册好：

```text
Runtime:
    PhysicsSystem
    AnimationSystem
    SpriteSheetSystem
    AudioSystem
    UISystem
    RenderSystem

Editor:
    AnimationSystem
    SpriteSheetSystem
    UISystem
    RenderSystem
```

这意味着图集解析不是“独立后台任务”，而是场景更新链的一部分。

### 3.3.15.2 它每帧做什么

`SpriteSheetSystem::ResolveSheets(scene)` 会遍历带有图集引用的组件：

1. 如果组件没有写 `SpriteSheet`，直接跳过。
2. 如果 `SubRect` 非空，优先按命名子矩形解析。
3. 否则按 `CellIndex` 解析网格 cell。
4. 成功后把 `Texture / UVMin / UVMax` 写回组件。
5. 如果是 `SpriteRendererComponent`，还会顺便把 collider 联动到实体上。

所以你可以把它理解成：

```text
“.wtsheet 是资源定义，
SpriteSheetSystem 是把定义落成实体状态的运行时桥梁。”
```

### 3.3.15.3 为什么它要和 Physics 联动

当 `BoxCollider2DComponent::FollowAnimation = true` 时，碰撞盒不应该永远固定不动。  
角色走路、攻击、俯身、跳跃时，碰撞盒往往要跟着当前帧的视觉内容走。

`SpriteSheetSystem::ApplyColliderToEntity()` 做的就是这件事：

- 先拿到当前 cell 的 collider
- 再结合 `PixelsPerUnit` 把像素转世界单位
- 最后写回 `BoxCollider2DComponent::Size` 和 `Offset`

这能保证动画帧和碰撞框不会各说各话。

### 3.3.15.4 一个典型链路

```text
Scene::OnUpdateRuntime()
    ↓
SpriteSheetSystem::ResolveSheets(scene)
    ↓
SpriteRendererComponent / UIImageComponent
    ↓
Texture + UVMin / UVMax 更新
    ↓
RenderSystem / Renderer2D 画出来
```

如果是带碰撞的 sprite：

```text
SpriteSheetSystem::ApplyColliderToEntity()
    ↓
BoxCollider2DComponent 更新
    ↓
PhysicsSystem 下一帧拿到新的碰撞盒
```

这一层很适合面试时解释“为什么图集不只是渲染资源”，因为它已经开始影响实体的物理语义了。

## 3.3.16 动画系统怎么复用 `.wtsheet`

图集和动画的关系，不是“动画依赖贴图”，而是“动画可以直接依赖图集定义里的 cell”。

### 3.3.16.1 `AnimationFrame` 有两套来源

`AnimationFrame` 里同时保留了：

- `Texture + TexCoordMin + TexCoordMax`
- `SpriteSheet + CellIndex`

这不是重复，而是一个很实用的兼容策略：

- 旧资产和简单动画可以直接塞 baked UV
- 新资产和可热更动画可以引用 `.wtsheet`

### 3.3.16.2 `AnimationSystem` 怎么用它

`AnimationSystem` 在应用帧时，会先检查当前帧是不是带了 `SpriteSheet` 和 `CellIndex`。  
如果有，它不会直接用帧里缓存的 UV，而是去：

```text
SpriteSheetAsset::ResolveCell(frame.SpriteSheet, frame.CellIndex, resolved)
```

这样做的好处是：

- 图集重排后，动画不用重新烘焙
- trim 改了，动画马上跟着更新
- collider 也能跟着同一份定义走

### 3.3.16.3 事件也能顺着动画走

`AnimationClip` 不只是帧列表，它还可以带 `AnimationEvent`。  
这些事件会在播放过程中被触发，然后变成命令字符串或脚本动作。

这意味着动画不是“看图”，而是可以成为：

- 视觉节奏
- 行为触发器
- 战斗命中帧
- 特效挂点

### 3.3.16.4 一个具体例子

假设一个角色攻击动画有三帧：

1. 起手
2. 挥击
3. 收招

如果这三帧都来自同一个 `.wtsheet`，那么：

- 改网格会影响整套动画
- 改 trim 会影响画面位置
- 改 collider 会影响命中范围

而动画系统并不需要知道这些改动的细节，它只需要继续按时间推进帧即可。

## 3.3.17 侧边战斗和运行时拼接动画的复用方式

这章最后再看一个更贴近实战的例子：  
`SideCombatVisualService` 会在运行时拼出动画帧，而这些帧同样能回到 `.wtsheet` 这条链上。

### 3.3.17.1 运行时为什么还要认 `.wtsheet`

`SideCombatVisualService::EnsureAnimationClipLoaded()` 里会优先寻找和 atlas 对应的 `.wtsheet` 文件。  
如果存在，它就把 runtime 生成的 `AnimationFrame` 变成：

- `SpriteSheet = xxx.wtsheet`
- `CellIndex = 起始帧 + 偏移`

这样生成出来的动画，最后仍然走同一套 sheet 解析和 collider 跟随逻辑。

### 3.3.17.2 为什么这很值

这说明 `.wtsheet` 不只是编辑器手工资产，还是运行时内容拼接的共同约定。

它带来的好处是：

- 编辑器资源和战斗运行时共享语义
- demo 生成的动画和编辑器导出的动画行为一致
- 角色技能、波次、立绘等内容可以用同一套解析方式

### 3.3.17.3 一个完整链路

```text
SideCombatVisualService
    ↓
AnimationClip::Create()
    ↓
AnimationFrame 记录 SpriteSheet + CellIndex
    ↓
AnimationSystem 播放时调用 ResolveCell()
    ↓
SpriteSheetSystem 继续更新 collider / UV
    ↓
Renderer2D 画出最终帧
```

如果要把这一章讲成“教材”，这条链路很适合拿来当收尾示例。  
因为它把“资产定义”“动画播放”“碰撞联动”“最终渲染”四件事真正串在了一起。

## 3.3.18 再补一个面试好讲的结论

你可以把这一章最后的结论记成一句话：

```text
图集不是贴图工具，SpriteSheetAsset 是内容定义，SpriteSheetSystem 是运行时桥梁，AnimationSystem 和 UI / 战斗模块只是消费者。
```

这个说法很适合面试，因为它一口气把四层关系分清了：

- 资源定义层：`.wtsheet`
- 解析层：`ResolveCell()`
- 同步层：`SpriteSheetSystem`
- 消费层：`AnimationSystem`、`UISystem`、`SideCombatVisualService`

下一章如果继续写动画系统，就可以直接顺着这条线往下走，不需要再回头解释图集为什么存在。
