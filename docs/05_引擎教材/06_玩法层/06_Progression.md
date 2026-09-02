# 第 6 编 06：Progression

> 本章所属层级：**玩法层**
>
> 本章目标：理解 Wheatear 里的 Progression 是怎么把“成长数据、进度状态、结果页、技能树、装备页、存读档页、设置页、编辑器工具”串成一个统一中枢的。
>
> 先提醒一句：这里的 `Progression` 不是普通意义上的“菜单页”。  
> 它更像一个**全局进度中枢**：副本结算后把结果写进去，回到据点后把结果展开成页面，点技能树会读写成长状态，点装备页会读写装备状态，点存档会和场景上下文一起落盘，点设置会把运行时偏好同步到用户设置。
>
> 如果说 `VN` 负责“剧情怎么跑”，`TurnCombat` 负责“回合战斗怎么跑”，那 `Progression` 负责的就是：  
> **这盘游戏跑到哪了，玩家现在能做什么，下一步该去哪。**

## 6.1 这一层在架构里的位置

如果按我们前面约定的分层来讲，`Progression` 仍然属于**玩法层**，不是核心平台层，也不是引擎服务层。
它做的事情不是“画一个菜单”，而是把游戏的长期状态组织起来：

- 哪一章已经推进到哪了
- 哪些副本解锁了
- 哪些技能学了
- 哪些装备拥有了、装上了
- 玩家现在有多少材料、金币、经验
- 最近一次副本结果是什么
- 现在应该跳回哪个场景

这件事非常适合简单 2D 游戏。
因为这类游戏的核心循环往往就是：

```text
剧情 / 战斗
    ↓
记录进度
    ↓
回据点 / 结果页 / 存档页
    ↓
强化、解锁、读档、继续下一段
```

`Progression` 就是把这条循环从“零散按钮”变成“统一状态机”。

它的价值有两个：

1. **对玩家来说**，成长信息不会散落在一堆页面里。
2. **对开发者来说**，剧情、战斗、存档、设置、养成这些系统都有一个共同的落点。

你可以把这一层理解成：

```text
CommandBus
    ↓
GameProgress
    ↓
ProgressionSystem
    ├─ Hub
    ├─ Result
    ├─ Skill Tree
    ├─ Equipment
    ├─ Dungeon Select
    ├─ Relationship / Support
    ├─ Save / Load
    └─ Settings
```

所以它不是“菜单页集合”，而是**全局成长中枢 + 页面分发器**。

## 6.2 本章先看哪些源码

建议按这个顺序看：

| 文件 | 作用 |
| --- | --- |
| `Wheatear/src/Wheatear/Modules/Progression/GameProgress.h/.cpp` | 全局进度状态、存档、命令入口 |
| `Wheatear/src/Wheatear/Modules/Progression/ProgressionContent.h/.cpp` | 成长内容数据表、初始配置、查表接口 |
| `Wheatear/src/Wheatear/Modules/Progression/ProgressionSystem.h/.cpp` | 页面更新主入口，负责把状态翻译成 UI |
| `Wheatear/src/Wheatear/Modules/Progression/ProgressionSaveLoadPageService.h/.cpp` | 存档 / 读档页 |
| `Wheatear/src/Wheatear/Modules/Progression/ProgressionResultPageService.h/.cpp` | 结果页 |
| `Wheatear/src/Wheatear/Modules/Progression/ProgressionSkillTreePageService.h/.cpp` | 技能树页 |
| `Wheatear/src/Wheatear/Modules/Progression/ProgressionEquipmentPageService.h/.cpp` | 装备页 |
| `Wheatear/src/Wheatear/Modules/Progression/ProgressionSettingsCommandService.h/.cpp` | 设置命令解析与同步 |
| `Wheatear/src/Wheatear/Modules/Progression/ProgressionSettingsPageService.h/.cpp` | 设置页 UI |
| `WheatearEditor/src/Modules/Progression/ProgressionContentEditorPanel.cpp` | 成长内容编辑器 |

如果你只想先抓住主链路，就先看：

`ProgressionContent` -> `GameProgress` -> `ProgressionSystem` -> 各个 PageService -> `ProgressionContentEditorPanel`

## 6.3 `ProgressionContent`：成长内容不是写死在代码里

这一层最重要的概念是：**内容**和**状态**分开。

- `ProgressionContent` 负责“游戏里有哪些固定数据”
- `GameProgress::State` 负责“玩家现在走到哪了”

这两个东西不能混。

### 6.3.1 内容从哪里来

进度内容的总入口是：

```text
assets/gameplay/progression/progression_content.yaml
```

但它通常不是把所有内容一股脑塞在一个大文件里，而是通过 manifest 里的 `files` 映射拆成多个 YAML 段。

可以把它理解成这种结构：

```yaml
files:
  defaults: assets/gameplay/progression/defaults.yaml
  materials: assets/gameplay/progression/materials.yaml
  dungeons: assets/gameplay/progression/dungeons.yaml
  relationships: assets/gameplay/progression/relationships.yaml
  skillNodes: assets/gameplay/progression/skill_nodes.yaml
  equipmentSlots: assets/gameplay/progression/equipment_slots.yaml
  equipment: assets/gameplay/progression/equipment.yaml
  upgrades: assets/gameplay/progression/upgrades.yaml
  dungeonRewardSummary: assets/gameplay/progression/dungeon_reward_summary.yaml
```

编辑器就是按这个 manifest 去展开的。
`ProgressionContentEditorPanel` 先读 manifest，再逐个打开对应的资产文件。

### 6.3.2 `Content` 里有什么

`ProgressionContent::Content` 这一坨不是“显示用字符串表”，而是**整个成长系统的静态设定**。

大概可以分成几类：

- 默认入口和起始设定
  - `MainDungeonId`
  - `MaterialDungeonId`
  - `DefaultObjective`
  - `DefaultLastResultMessage`
- 初始状态
  - `InitialUnlockedSkills`
  - `InitialOwnedEquipment`
  - `InitialEquippedItemsBySlot`
  - `InitialSelectedEquipmentId`
  - `InitialUnlockedDungeons`
  - `InitialStoryFlags`
- 成长表
  - `Materials`
  - `Upgrades`
  - `Dungeons`
  - `SkillNodes`
  - `EquipmentSlots`
  - `Equipment`
  - `Relationships`
  - `DungeonRewardSummary`
- 兼容旧字段
  - `MagicSwordLv2`
  - `TravelerArmorLv1`
  - `TravelerArmorUpgradeEquipmentId`

这个结构很重要，因为它说明 `Progression` 不是单纯“UI 绑定层”，而是**数据驱动的玩法层**。

### 6.3.3 这些定义分别代表什么

#### `UpgradeDefinition`

它描述的是一条升级配方：

- 升级对象是谁
- 升到几级
- 需要多少金币
- 需要哪些材料
- 额外给什么属性
- 解锁什么技能
- 成功后显示什么提示

#### `DungeonDefinition`

它描述一个副本或章节节点：

- 名字
- 分类
- 推荐等级
- 锁定 / 解锁状态文本
- 首通奖励、重复奖励
- 首通后解锁什么
- 首通后写哪些旗标
- 首通后把目标改成什么

#### `SkillNodeDefinition`

它描述技能树上的一个节点：

- 节点 ID
- 父节点
- 名称
- 分支
- 输入
- 连招职责
- 前置要求
- 说明
- 坐标
- 解锁章节

#### `EquipmentDefinition`

它描述一个装备条目：

- ID 和名字
- 属于哪个槽
- 在哪一页
- 状态文本
- 属性文本
- 来源
- 说明
- 图标路径

#### `EquipmentSlotDefinition`

它只是装备槽的基础定义：

- `Id`
- `Name`

### 6.3.4 读内容时要记住的一点

`ProgressionContent::Reload()` 会在编辑器保存后重新加载。
这意味着你的内容表不是“写完后必须重启引擎才生效”，而是可以在编辑器里改完就热更新回运行时。

这对教材来说很重要，因为它正好体现了这套架构的气质：

- 内容尽量数据化
- 运行时尽量轻量化
- 编辑器尽量能直接改出结果

## 6.4 `GameProgress`：真正保存的是运行时状态

如果说 `ProgressionContent` 是“地图”，那 `GameProgress` 就是“当前局势”。

它保存的不是固定设定，而是玩家当前这一局到底处于什么状态。

### 6.4.1 `State` 里大致存什么

你可以把 `GameProgress::State` 分成几组来看：

#### 场景上下文

- `CurrentScenePath`
- `PreviousScenePath`

它们决定了：

- 存档时应该记住哪个场景
- 读档后应该回到哪里
- `SaveLoad` 页面关闭后应该回到哪里

#### 主线和战斗进度

- `CurrentChapter`
- `Objective`
- `ActiveSideCombatDungeonId`
- `LastDungeonResult`
- `LastResultMessage`

这部分决定了：

- 当前剧情推进到哪
- 当前目标是什么
- 最近一次副本结果怎么显示

#### 成长数值

- `PlayerLevel`
- `Experience`
- `ExperienceToNext`
- `Gold`
- `MagicSwordLevel`
- `TravelerArmorLevel`
- `Attributes`
- `EquipmentLevels`
- `Materials`

这部分决定了：

- 角色成长到什么程度
- 升级和强化能不能做
- 材料够不够

#### 解锁集合

- `UnlockedDungeons`
- `UnlockedSkills`
- `OwnedEquipment`
- `EquippedItemsBySlot`
- `StoryFlags`
- `CompletedDungeons`

这部分决定了：

- 哪些内容能看到
- 哪些技能能学
- 哪些装备能装
- 哪些副本已经打过

#### 社交和页面状态

- `Relationships`
- `ActiveSupportCharacterId`
- `SelectedSkillNodeId`
- `SelectedEquipmentId`
- `EquipmentPage`
- `SkillTreePanX`
- `SkillTreePanY`

这部分决定了：

- 页面打开时应该定位到哪
- 当前选中了谁
- 当前页是第几页

### 6.4.2 为什么要把状态和内容分开

因为它们的生命周期不同。

- 内容是“项目资产”
- 状态是“玩家存档”

如果把两者搅在一起，后面就很难维护：

- 新增一个装备，会污染旧存档逻辑
- 改一个技能树节点，会影响当前运行时状态
- 调一个设置值，可能会误写进进度槽

Wheatear 现在的做法更清楚：

- `ProgressionContent` 管内容
- `GameProgress` 管进度
- `UserSettings` 管设置偏好

### 6.4.3 `GameProgress` 还负责什么

除了状态，`GameProgress` 还提供很多面向 UI 和命令系统的接口，例如：

- `ResetForNewGame()`
- `SetSceneTransitionContext(previousScenePath, currentScenePath)`
- `GetScenePathForSave()`
- `SaveSlot()`
- `LoadSlot()`
- `RecordDungeonClear()`
- `RecordLastDungeonResult()`
- `BuildLoadGameCommand()`
- `ExecuteCommand()`

所以它并不只是一个“存档容器”，而是**进度领域的核心服务对象**。

## 6.5 `ProgressionSystem`：把状态翻成页面

这一层是 `Progression` 的真正展示面。

它不是单个页面，而是多个页面的调度中心。
`ProgressionSystem` 会在场景运行时不断把 `GameProgress` 的状态翻译成各个页面的 UI 文本、进度条、按钮命令和高亮状态。

### 6.5.1 它负责哪些页面

从源码上看，`ProgressionSystem` 会更新这些页面：

- `Hub`
- `Result`
- `Skill Tree`
- `Equipment`
- `Dungeon Select`
- `Relationship`
- `Support`
- `Settings`
- `Save / Load`

这就是为什么它不是“某个菜单页”。
它其实是**整套成长菜单的页面分发器**。

### 6.5.2 `OnRuntimeStart()` 和 `OnUpdateRuntime()`

`ProgressionSystem::OnRuntimeStart(Scene*)` 主要做三件事：

1. 读取当前场景的保存策略
2. 处理待应用的 save/load 目标
3. 初始化并刷新各个页面

`ProgressionSystem::OnUpdateRuntime(Scene*, Timestep)` 则更像“每帧刷新器”：

1. 更新游戏存档相关命令
2. 更新 Progression 各页面

这意味着它不是一次性构建 UI，而是持续把状态投影到页面上。

### 6.5.3 典型的 UI 连接方式

这一层经常会把状态变成按钮命令和文本：

- `SetText(...)`
- `SetProgress(...)`
- `SetButtonCommand(...)`

也就是说，Progression 页面不是“写死的界面”，而是：

```text
GameProgress 的状态
    ↓
ProgressionSystem 生成文本和命令
    ↓
场景实体上的 UI 组件显示出来
```

这和很多传统菜单系统不一样。
它不是靠手搓一堆独立窗口，而是靠场景实体和服务层把页面拼起来。

## 6.6 命令怎么进来：`CommandBus -> GameProgress::ExecuteCommand()`

`Progression` 最关键的工程味道之一，就是它完全不是“按钮直接改变量”。

它也是命令驱动的。

### 6.6.1 一条命令怎么跑

典型链路是：

```text
UI 按钮 / 脚本 / 场景命令
    ↓
CommandBus
    ↓
GameProgress::ExecuteCommand()
    ↓
对应的 Progression 子服务
    ↓
修改 State / UserSettings / 页面状态
```

也就是说，UI 只是发消息，真正改状态的是服务。

### 6.6.2 常见的 `progression:` 命令

`GameProgress::ExecuteCommand()` 能识别很多命令，常见的有：

- `progression:set_flag`
- `progression:clear_flag`
- `progression:set_chapter`
- `progression:reset`
- `progression:set_active_dungeon`
- `progression:clear_active_dungeon`
- `progression:select_skill_node`
- `progression:learn_selected_skill`
- `progression:select_equipment`
- `progression:select_equipment_slot`
- `progression:toggle_selected_equipment`
- `progression:upgrade_traveler_armor`
- `progression:upgrade_item:<id>`
- `progression:equipment_page_next`
- `progression:equipment_page_prev`
- `progression:equipment_page:<n>`
- `progression:equipment_page_slider:<n>`
- `progression:select_support`
- `progression:set_text_speed`
- `progression:set_master_volume`
- `progression:set_bgm_volume`
- `progression:set_sfx_volume`
- `progression:text_speed_up`
- `progression:toggle_fullscreen`
- `progression:toggle_screen_shake`

这些命令看起来很多，但本质上都在做同一件事：

**把界面上的选择变成一个明确、可记录、可回放的状态变化。**

### 6.6.3 为什么设置命令单独抽了服务

`ProgressionSettingsCommandService` 不是多余的。

它做的是一类非常独立的事：

- 判断某条命令是不是设置命令
- 执行设置命令
- 把结果同步到运行时
- 生成设置页的状态文本

这就是为什么 `GameProgress` 里会调用：

```text
ProgressionSettingsCommandService::IsSettingsCommand(action)
ProgressionSettingsCommandService::Execute(action, GetState())
ProgressionSettingsCommandService::ApplyToRuntime()
```

设置不是进度槽的一部分，而是运行时偏好。
这条边界在教材里一定要讲清楚。

## 6.7 存档和读档：全局进度 vs VN runtime

这一层最容易被误解成“就是个存档页”。
其实不是。

Wheatear 现在把存档分成两块：

1. **全局进度**
2. **剧情运行时状态**

### 6.7.1 全局进度怎么存

`GameProgress::SaveSlot(slot)` 会把当前进度写成：

```text
progression_slotN.wtsave
```

里面存的是长期有效的东西：

- 章节
- 材料
- 技能
- 装备
- 副本
- 旗标
- 场景路径
- 当前目标

### 6.7.2 VN runtime 怎么存

剧情运行时状态是另外一份：

```text
slotN.vnstate
```

这里面更偏向：

- 当前脚本跑到哪一行
- 变量状态
- 场景里剧情运行的局部上下文

所以它不是跟全局进度混在一起存。

### 6.7.3 为什么要记录场景上下文

`GameProgress::SetSceneTransitionContext(previousScenePath, currentScenePath)` 和 `GetScenePathForSave()` 的作用就是：  
存档时必须知道“这份状态属于哪个场景”。

否则会出现两个问题：

1. 读档回不去正确的场景
2. `SaveLoad` 页打开时不知道该回哪个界面

所以 `GetScenePathForSave()` 会优先考虑当前和上一个场景，但会避开 `SaveLoad.wt` 这种过渡页。

### 6.7.4 `BuildLoadGameCommand()` 为什么重要

`BuildLoadGameCommand(slot, scenePath)` 会把“读哪个档、回哪个场景”编码成一条统一命令。

这样一来：

- VN 可以调它
- 事件脚本可以调它
- 进度页可以调它
- 场景切换服务也能理解它

典型结果类似：

```text
loadgame:assets/scenes/Intro.wt:1
```

这比“直接调用某个加载函数”更适合做教材，因为它把**加载逻辑**和**命令流**统一起来了。

## 6.8 结果页：副本结算如何变成长期进度

`ProgressionResultPageService` 负责把副本结束后的结果整理成结果页。

这一页很重要，因为它是“战斗 -> 成长”的过渡。

### 6.8.1 结果页从哪来

典型流程是：

```text
SideCombat / TurnCombat / 其他副本
    ↓
GameProgress::RecordDungeonClear(...)
    ↓
GameProgress::RecordLastDungeonResult(...)
    ↓
ProgressionSystem::UpdateResult(...)
    ↓
ProgressionResultPageService::UpdateDrops(scene)
```

也就是说，战斗系统不会自己画结果页。
它只负责把结果写进 `GameProgress`，然后由 Progression 页去展示。

### 6.8.2 `RecordDungeonClear()` 写什么

这个函数负责的是“长期成长”：

- 首通与否
- 最高连击
- 解锁副本
- 写剧情旗标
- 改目标
- 加经验 / 加奖励

### 6.8.3 `RecordLastDungeonResult()` 写什么

这个函数更像“结果页缓存”：

- 评分
- 时间
- 受击
- 奖励数量
- 描述文本

这样结果页和 hub 页都能读到同一份最新信息。

### 6.8.4 结果页为什么属于 Progression

因为结果页不是临时弹窗。
它是长期进度的一部分。

战斗结束后玩家最想看的，不只是“赢了没有”，而是：

- 拿到了什么
- 解锁了什么
- 下一步该去哪里

这正是 Progression 的职责。

## 6.9 技能树和装备页：成长系统的两条主线

### 6.9.1 技能树页

`ProgressionSkillTreePageService` 负责把技能树状态同步到页面。

它会做的事通常包括：

- 同步节点位置
- 同步连线和依赖
- 显示当前是否可解锁
- 显示材料是否足够
- 把“选中节点”变成高亮

对应的数据来源主要是：

- `ProgressionContent::SkillNodeDefinition`
- `GameProgress::State` 里的 `UnlockedSkills`
- `GameProgress::State` 里的 `SelectedSkillNodeId`
- `GameProgress::State` 里的材料和章节

常见命令有：

- `progression:select_skill_node`
- `progression:learn_selected_skill`

技能树页的关键点不是“画一棵树”，而是**前置关系和材料消耗如何被状态机驱动**。

### 6.9.2 装备页

`ProgressionEquipmentPageService` 则负责装备列表和装备槽。

它会做的事通常包括：

- 生成装备页布局
- 同步分页
- 更新装备卡片
- 根据当前槽位显示已装备状态
- 控制当前选中装备和槽位

对应的数据来源主要是：

- `ProgressionContent::EquipmentDefinition`
- `ProgressionContent::EquipmentSlotDefinition`
- `GameProgress::State` 里的 `OwnedEquipment`
- `GameProgress::State` 里的 `EquippedItemsBySlot`
- `GameProgress::State` 里的 `SelectedEquipmentId`
- `GameProgress::State` 里的 `EquipmentPage`

常见命令有：

- `progression:select_equipment`
- `progression:select_equipment_slot`
- `progression:toggle_selected_equipment`
- `progression:upgrade_item:<id>`
- `progression:equipment_page_next`
- `progression:equipment_page_prev`
- `progression:equipment_page:<n>`

装备页里还有一个很有意思的点：  
有些升级是特殊升级，比如旅行者护甲、魔剑等级，这类历史字段还保留着兼容逻辑。

这说明进度系统不是“纯理想模型”，它要同时照顾新内容和旧内容。

## 6.10 好感度和支援页：轻量但很有价值

`ProgressionSystem` 里还会更新关系和支援相关页面。

这部分在很多项目里容易被忽略，但其实它很能体现一个进度系统是否完整：

- 角色关系有没有被记录
- 伙伴选择有没有被保存
- 据点里能不能切换支援对象

对应的内容来源主要是：

- `ProgressionContent::Relationships`
- `GameProgress::State` 里的 `Relationships`
- `GameProgress::State` 里的 `ActiveSupportCharacterId`

这类页面通常不如技能树和装备页复杂，但它说明 `Progression` 不是单一菜单，而是**一组共用同一份进度状态的页面族**。

## 6.11 设置页：运行时偏好和进度状态要分开

设置页也属于 `ProgressionSystem` 的一部分，但它和其他成长页不太一样。

### 6.11.1 设置页改的不是进度槽

这里最重要的一点是：  
设置项不应该和进度槽混在一起。

也就是说，音量、文字速度、全屏、震屏这种东西，不应该写进 `GameProgress::State` 里当作长期进度保存。

它们应该属于运行时用户偏好。

这也是为什么 `GameProgress` 的状态里已经不再放 `Settings` 字段。

### 6.11.2 设置页和设置命令怎么配合

`ProgressionSettingsPageService` 负责把设置 UI 刷出来。

`ProgressionSettingsCommandService` 负责：

- 识别设置命令
- 改 `UserSettings`
- 把当前设置同步到运行时
- 生成设置页状态文本

典型命令有：

- `progression:set_text_speed`
- `progression:text_speed_up`
- `progression:set_master_volume`
- `progression:set_bgm_volume`
- `progression:set_sfx_volume`
- `progression:toggle_fullscreen`
- `progression:toggle_screen_shake`

如果你把这一章讲给面试官，可以直接说：

> 设置页不是在改游戏进度，而是在改运行时偏好。  
> 进度槽存的是玩家走到哪了，设置页存的是玩家怎么舒服地玩。

## 6.12 编辑器：`ProgressionContentEditorPanel`

这个编辑器窗口很适合拿来讲“数据驱动”。

因为它说明了一件事：  
成长系统不是只能在代码里改，它还可以通过内容编辑器直接维护。

### 6.12.1 它怎么工作

`ProgressionContentEditorPanel` 打开后会先读 manifest：

- 先读 `progression_content.yaml`
- 再从 `files` 映射里展开具体内容文件
- 再把选中的 YAML 资产读进来

保存时它会：

1. 保存 manifest 或选中资产
2. 调 `ProgressionContent::Reload()`
3. 让运行时立刻看到新的内容

### 6.12.2 它有哪些编辑面

从界面上看，它大致提供这些部分：

- Content
- Skill Tree
- Validation
- Advanced YAML
- Advanced Raw
- Manifest

这几页已经很说明问题了：

- 结构化编辑
- 校验
- 原始 YAML 编辑
- 清单驱动的资源管理

### 6.12.3 这类编辑器最值钱的地方

不是“能不能编辑 YAML”。
而是它能把这些内容安全地维护起来：

- 技能树节点
- 副本定义
- 装备定义
- 关系记录
- 奖励摘要

所以这一章写到这里，你就能很自然地解释：

> 为什么成长系统不能只靠手写代码，必须配一个内容编辑器。

## 6.13 一个完整例子：打完副本以后发生了什么

我们把这整套链路连起来看一遍。

### 6.13.1 战斗结束

玩家打完一个副本，战斗系统会做两件事：

1. 调 `GameProgress::RecordDungeonClear(...)`
2. 调 `GameProgress::RecordLastDungeonResult(...)`

这时进度状态里已经有了：

- 首通信息
- 结果统计
- 奖励信息
- 目标变化
- 副本解锁

### 6.13.2 结果页出现

接着 `ProgressionSystem` 读到这些状态，结果页就会显示：

- 本次评分
- 掉落
- 首通奖励
- 下一步建议

### 6.13.3 回到据点

玩家从结果页返回 hub 页面后，会看到：

- 章节目标变了
- 解锁副本亮了
- 材料数量变了
- 技能树是否可学更新了
- 装备页是否可强化更新了

### 6.13.4 继续成长

然后玩家可能会：

- 去技能树页学一个节点
- 去装备页强化一件装备
- 去存档页写一个档
- 去设置页调音量或文字速度

这一条链路说明 `Progression` 的本质不是“结算后弹个面板”，而是：

**把一场战斗的结果，稳稳接回整个游戏的长期成长循环。**

## 6.14 和 Unity / UE 的对照

如果面试官问“这个和别的引擎像不像”，你可以这样讲。

### 6.14.1 和 Unity 的对照

Unity 里通常会把这些东西拆得比较散：

- 菜单页面
- ScriptableObject 配表
- Save 系统
- GameManager
- UI Controller
- Settings 存储

Wheatear 的做法更统一一点：

- `GameProgress` 相当于进度和存档的中枢
- `ProgressionContent` 相当于数据表
- `ProgressionSystem` 相当于页面驱动器
- `ProgressionContentEditorPanel` 相当于内容维护工具

### 6.14.2 和 UE 的对照

UE 里常见的对应物通常是：

- `GameInstance`：全局运行态
- `SaveGame`：存档对象
- `UserSettings`：用户偏好
- UMG：界面层
- DataTable / DataAsset：数据配置

Wheatear 也是这个思路，只是它把职责拆得更显式：

- 进度状态单独放
- 内容定义单独放
- 设置命令单独放
- 页面刷新单独放

所以你可以把它理解成一种更“服务化”的 2D 玩法架构。

## 6.15 这一层的好处、坏处和常见误区

### 6.15.1 好处

- 玩法状态统一，副本、技能树、装备页、结果页都能读同一份数据
- 数据驱动强，内容改动不必频繁碰代码
- 存档和运行时偏好分离，读档不会乱改设置
- 页面服务分离，界面逻辑更容易查
- 适合简单 2D 游戏的长期成长循环

### 6.15.2 坏处

- 服务类会变多，初学者容易觉得“文件好多”
- 状态字段一多，就必须更小心地维护命令边界
- 如果页面绑定名不统一，调试会比较烦
- 如果内容表和状态表混写，后期会乱

### 6.15.3 常见误区

1. 把设置当成进度槽的一部分。
2. 让 UI 直接改状态，不走命令。
3. 把内容定义和运行时状态混在一起。
4. 认为 `Progression` 只是菜单页，没有看到它是全局中枢。
5. 忘了保存场景上下文，导致读档回错地方。

## 6.16 面试时可以怎么讲

如果面试官问“你这个 Progression 怎么设计的”，你可以这样说：

> 我把 Progression 做成了一个全局进度中枢。  
> 静态内容放在 `ProgressionContent` 里，运行时状态放在 `GameProgress::State` 里，页面刷新由 `ProgressionSystem` 统一驱动。  
> 副本结算会写入长期进度，技能树和装备页读同一份状态，存档和读档会区分全局进度与 VN runtime，设置则单独走 `UserSettings`。  
> 这样做的好处是内容能数据化、页面能统一化、存档和成长能分开维护，也方便编辑器直接改内容后热重载。

如果对方继续追问，你就接着说：

- 为什么 `GameProgress` 不是菜单
- 为什么设置不写进存档槽
- 为什么 `BuildLoadGameCommand()` 要带场景路径
- 为什么技能树和装备页都要由同一份进度状态驱动

## 6.17 小结

这一章最重要的一句话是：

**Progression 不是“菜单集合”，而是把长期进度、成长内容、结果页、设置页、存读档和编辑器串起来的全局中枢。**

如果你理解了这一层，后面你再看：

- `GameProgress`
- `SceneTransition`
- `VN`
- `TurnCombat`
- `Editor`

这些东西就不会再是散的，而是会变成同一条游戏循环里的不同环节。
