# 第 5 编 05：GameProgress 与进度状态中心

> 本章所属层级：**游戏运行服务层**
>
> 本章目标：理解 Wheatear 为什么需要一个 `GameProgress` 来记录“玩家走到哪里、获得了什么、解锁了什么、该回到哪个场景”，以及它如何和 `CommandBus`、`EventScript`、`SceneTransition`、VN 存档、横板战斗结算、据点 UI 串成一条完整链路。
>
> 先提醒一句：这一章讲的不是“怎么把文件写到硬盘”这么窄的存档问题。`GameProgress` 更像是一个**进度状态中心**。存档只是它的一部分，它还负责章节、目标、材料、等级、技能、装备、副本、结果页、支援角色和一些 UI 状态。
>
> 本章会反复使用三个例子：
>
> 1. 主菜单点击“新游戏”后，为什么进度会重置。
> 2. VN 或据点点击“保存 / 读取”后，为什么既有全局进度存档，也有剧情运行时存档。
> 3. 横板副本打完后，为什么据点、结果页、技能树、装备页都能看到新的材料和解锁状态。

## 5.1 这一层在架构中的位置

`GameProgress` 位于 **游戏运行服务层**。

它上接：

- `CommandBus`
- `EventScript`
- `SceneTransitionService`
- `ProgressionSystem`
- 各玩法模块

它下接：

- `AssetPath`
- `UserSettings`
- `ProgressionContent`
- 文件系统
- 场景的 `SavePolicy`

可以先把它放进这张图里：

```text
UI / EventScript / Gameplay modules
    ↓
CommandBus
    ↓
GameProgress
    ↓
progress state / save slots / unlocks / result data
    ↓
ProgressionSystem / VN / SceneTransition / Result page / Hub
```

它不是 **Scene ECS 层**。

因为它不负责：

- 创建实体
- 挂组件
- 更新系统
- 序列化 `.wt` 场景

它也不是纯粹的 **玩法层**。

因为它不负责：

- 横板战斗手感
- 回合制行动顺序
- 战棋格子规则
- VN 文本推进

它负责的是这些玩法之间共享的低频状态：

- 当前章节
- 当前目标
- 当前场景
- 已完成副本
- 已解锁副本
- 已解锁技能
- 已拥有装备
- 材料、金币、经验
- 最近一次战斗结果

这就是为什么它应该放在游戏运行服务层。

## 5.2 本章要读的源码

建议按这个顺序看：

- `Wheatear/src/Wheatear/Modules/Progression/GameProgress.h`
- `Wheatear/src/Wheatear/Modules/Progression/GameProgress.cpp`
- `Wheatear/src/Wheatear/Modules/Progression/ProgressionContent.h`
- `Wheatear/src/Wheatear/Modules/Progression/ProgressionContent.cpp`
- `Wheatear/src/Wheatear/Modules/Progression/ProgressionSystem.cpp`
- `Wheatear/src/Wheatear/Modules/Progression/ProgressionSaveLoadPageService.cpp`
- `Wheatear/src/Wheatear/Modules/Progression/ProgressionResultPageService.cpp`
- `Wheatear/src/Wheatear/Modules/VisualNovel/VisualNovelSystem.cpp`
- `Wheatear/src/Wheatear/Modules/SideCombat/SideCombatOutcomeService.cpp`
- `Wheatear/src/Wheatear/Modules/SideCombat/SideCombatPickupService.cpp`
- `Wheatear/src/Wheatear/Runtime/CommandBus.cpp`
- `WheatearSandbox/src/RuntimeSceneLayer.cpp`
- `Projects/WheatearDemo/assets/events/sandbox_flow.wts`
- `Projects/WheatearDemo/assets/gameplay/progression/progression_content.yaml`

如果只看一条最核心链路，就先看：

- `CommandBus::Execute()`
- `GameProgress::ExecuteCommand()`
- `GameProgress::SaveSlot()`
- `GameProgress::LoadSlot()`
- `GameProgress::RecordDungeonClear()`
- `GameProgress::RecordLastDungeonResult()`

## 5.3 先给结论：GameProgress 让游戏世界记住玩家

前一章 `SceneTransition` 讲的是“场景怎么换”。

这一章 `GameProgress` 讲的是：

```text
换过去以后，游戏为什么还记得玩家走到了哪里。
```

更具体一点：

```text
SceneTransition 负责去哪，
GameProgress 负责记住玩家为什么要去、从哪来、已经做过什么。
```

比如玩家打完副本：

```text
SideCombat
    ↓
RecordDungeonClear()
    ↓
CompletedDungeons / UnlockedDungeons / StoryFlags / Objective / EXP
    ↓
RecordLastDungeonResult()
    ↓
Result page / Hub / SkillTree / Equipment
```

如果没有 `GameProgress`，每个页面都要自己问一遍：

- 副本是不是打过了？
- 材料是不是拿到了？
- 结果页要显示什么？
- 据点按钮要不要改字？
- 技能树节点能不能点？

那样项目很快就会散。

## 5.4 GameProgress 和 SceneTransition 为什么要分开

这点要先讲清楚，因为很容易混。

`SceneTransitionService` 管的是**请求切场景**：

- `RequestLoadScene`
- `RequestNewGame`
- `RequestLoadGame`
- `DrainRequests`

`GameProgress` 管的是**进度状态**：

- `ResetForNewGame`
- `SaveSlot`
- `LoadSlot`
- `RecordDungeonClear`
- `AddMaterial`
- `AddExperience`

两者会协作，但职责不同。

比如新游戏：

```text
newgame:assets/scenes/Intro.wt
    ↓
SceneTransitionService::RequestNewGame()
    ↓
RuntimeSceneLayer::ExecuteSceneTransitionRequest()
    ↓
GameProgress::ResetForNewGame()
    ↓
RuntimeSceneLayer::LoadScene()
```

所以真正重置进度的是 `GameProgress`，真正加载场景的是 `RuntimeSceneLayer`。

这就是分层的价值：

```text
场景切换服务不变成进度系统，
进度系统也不直接变成场景宿主。
```

## 5.5 GameProgress 的形态：namespace 服务加静态 State

源码里 `GameProgress` 不是一个普通对象类，而是一个 namespace 里的服务集合。

它的状态来自：

```cpp
State& GetState()
{
    static State state = MakeDefaultState();
    return state;
}
```

这意味着当前项目里只有一份全局进度状态。

对简单 2D 单机项目来说，这很实用：

- 好访问
- 好调试
- 不需要复杂依赖注入
- `CommandBus`、VN、战斗、UI 都能读到同一份状态

但它也有代价：

- 不适合多存档同时常驻内存
- 不适合多人联机状态隔离
- 不适合复杂测试并发
- 容易让太多模块直接依赖全局状态

面试时要讲得客观：

> 我这里用的是轻量全局进度服务，适合单机 2D 竖切。它不是大型商业引擎里最纯的架构，但对当前项目足够直观，也方便把剧情、战斗、据点和存档串起来。

## 5.6 `State` 里到底存什么

`GameProgress::State` 是这章最核心的数据结构。

它大致可以分成八组。

## 5.7 第一组：章节、目标和场景上下文

相关字段：

```cpp
int CurrentChapter = 2;
std::string Objective;
std::string CurrentScenePath;
std::string PreviousScenePath;
```

这些字段回答三个问题：

- 现在是第几章？
- 当前目标是什么？
- 保存时应该回到哪个场景？

其中 `CurrentScenePath` 和 `PreviousScenePath` 特别重要。

因为玩家可能在 `SaveLoad.wt` 存档页里保存。

如果直接把当前场景保存成 `SaveLoad.wt`，下次读档就会回到存档菜单，而不是回到玩家原来的游戏位置。

所以源码里有 `ResolveScenePathForSave()`：

```text
如果当前是 SaveLoad 场景，并且 previous 不为空，就保存 previous。
否则保存 current。
否则 fallback 到默认场景。
```

这就是一个很真实的工程问题。

初学者常会以为“保存当前场景”很简单，实际一做存档菜单就会发现：当前界面未必是玩家真正的游戏进度位置。

## 5.8 第二组：玩家成长状态

相关字段：

```cpp
int PlayerLevel = 1;
int Experience = 0;
int ExperienceToNext = 100;
int MagicSwordLevel = 1;
int TravelerArmorLevel = 0;
int Gold = 0;
PlayerAttributes Attributes;
```

`PlayerAttributes` 又包含：

```cpp
int HP = 420;
int ATK = 24;
int DEF = 10;
int MATK = 18;
int MDEF = 8;
```

它们用于：

- 据点展示玩家状态
- 结算后加经验
- 技能树升级
- 装备强化加属性
- 简单 2D 战斗竖切里做数值反馈

当前 `AddExperience()` 里会处理升级：

```text
经验增加
    ↓
如果经验超过 ExperienceToNext
    ↓
升级
    ↓
提高 HP / ATK / DEF / MATK / MDEF
    ↓
刷新下一等级经验需求
```

这里还有一个安全细节：源码对单次经验增长的升级循环做了上限，避免手改存档导致一帧里死循环或卡很久。

这就是很好的面试点。

你可以说：

> 我在加经验时不是只写一个 while，还加了安全上限，避免异常存档把升级循环拖死。

## 5.9 第三组：材料、金币和资源消耗

相关字段：

```cpp
std::unordered_map<std::string, int> Materials;
std::unordered_map<std::string, std::string> MaterialNames;
int Gold = 0;
```

相关 API：

- `AddMaterial(itemId, displayName, amount)`
- `GetMaterialAmount(itemId)`
- `HasMaterials(costs)`
- `SpendMaterials(costs)`

这组 API 很适合初学者学习，因为它体现了一个稳定模式：

```text
先检查能不能扣，
再统一扣。
```

`SpendMaterials()` 不会一边扣一边发现不够。它先调用：

```cpp
HasMaterials(costs)
```

确认全部足够后，再真正扣。

这叫避免半提交。

比如魔剑 Lv2 需要：

```text
魔核碎片 x1
兽筋 x2
熊爪 x1
```

如果玩家只有兽筋但没有魔核，系统不应该先把兽筋扣掉，然后再告诉玩家失败。

这就是为什么材料扣除要先验证再执行。

## 5.10 第四组：副本、技能、装备、剧情旗标

相关字段：

```cpp
std::unordered_map<std::string, int> BestCombosByDungeon;
std::unordered_set<std::string> CompletedDungeons;
std::unordered_set<std::string> UnlockedDungeons;
std::unordered_set<std::string> UnlockedSkills;
std::unordered_set<std::string> OwnedEquipment;
std::unordered_map<std::string, std::string> EquippedItemsBySlot;
std::unordered_set<std::string> StoryFlags;
```

这些字段是“内容门禁”的核心。

例如：

- `CompletedDungeons` 决定某个副本是否首通。
- `UnlockedDungeons` 决定副本选择页能不能进入。
- `UnlockedSkills` 决定技能树节点状态。
- `OwnedEquipment` 决定装备页能不能装备。
- `EquippedItemsBySlot` 决定每个槽位当前穿了什么。
- `StoryFlags` 决定 `.wts` 或 VN 选项是否可见。

它们不是每帧高频变化的数据。

它们更像：

```text
玩家档案里的事实。
```

## 5.11 第五组：UI 选择状态

相关字段：

```cpp
std::string ActiveSupportCharacterId = "mentor";
std::string SelectedSkillNodeId = "magic_sword_core";
std::string SelectedEquipmentId;
int EquipmentPage = 1;
float SkillTreePanX = 0.0f;
float SkillTreePanY = 0.0f;
```

这组字段比较微妙。

它们既不是纯游戏进度，也不是渲染状态。

它们更像“据点页面当前看哪里”的状态：

- 当前选中的技能节点
- 当前选中的装备
- 当前装备背包页
- 技能树镜头偏移
- 当前支援角色

小项目里放在 `GameProgress::State` 可以接受，因为方便保存和恢复。

但大型项目里，通常会把这类状态拆成：

- 长期进度
- 界面会话状态
- 编辑器状态

这就是一个可以在面试中主动说明的设计边界。

## 5.12 第六组：关系和支援

相关字段：

```cpp
std::vector<RelationshipRecord> Relationships;
std::string ActiveSupportCharacterId = "mentor";
```

`RelationshipRecord` 包含：

- `CharacterId`
- `DisplayName`
- `Affinity`
- `SupportLevel`
- `Unlocked`
- `Role`
- `NextMilestone`

这服务于你项目里的 2D 剧情和支援玩法。

当前 `select_support:` 命令会通过 `ProgressionContent` 查角色定义，再把 `ActiveSupportCharacterId` 改成对应角色。

这说明支援不是写死在场景按钮里的：

```text
按钮
    ↓ progression:select_support:mentor
GameProgress
    ↓ ActiveSupportCharacterId
BuildSupportStatus()
    ↓
Support 页面文本刷新
```

## 5.13 第七组：当前横板副本和最近结果

相关字段：

```cpp
std::string ActiveSideCombatDungeonId;
DungeonResult LastDungeonResult;
std::string LastResultMessage;
```

`ActiveSideCombatDungeonId` 用来告诉横板战斗：

```text
这次进 SideCombatBeastPath，是主线黑熊，还是材料副本黑林兽道？
```

这个字段的来源通常是 `.wts`：

```text
progression:set_active_dungeon:CH02_MAIN_BearAwakening
scene:assets/scenes/SideCombatBeastPath.wt
```

或：

```text
progression:set_active_dungeon:CH02_MAT_BeastPath
scene:assets/scenes/SideCombatBeastPath.wt
```

也就是说，同一个横板场景可以根据进度状态进入不同副本语义。

`LastDungeonResult` 则服务于结果页。

它包含：

- 副本 id
- 副本名称
- 评分
- 是否首通
- 最佳连击
- 受击次数
- 通关时间
- 获得经验
- 奖励摘要
- 奖励数量表

所以结果页不需要回头问横板战斗组件。

战斗结束后，战斗模块把结果写进 `GameProgress`，结果页只读 `GameProgress`。

## 5.14 第八组：通知和最近消息

相关字段：

```cpp
std::vector<std::string> Notifications;
std::string LastResultMessage;
```

`PushNotification()` 会把消息推入通知列表，并把数量限制在 5 条。

这是一种轻量的“近期反馈”机制。

比如：

- 获得材料
- 获得经验
- 魔剑觉醒
- 读取存档
- 保存成功

据点页面、结果页、调试文本都可以显示这些消息。

## 5.15 默认状态从哪里来：`MakeDefaultState()`

`GetState()` 第一次调用时，会通过 `MakeDefaultState()` 创建默认状态。

`MakeDefaultState()` 不只是写死几个数值，它会读：

```cpp
const auto& content = ProgressionContent::Get();
```

然后填入：

- 默认目标
- 初始已解锁技能
- 初始拥有装备
- 初始装备槽
- 初始解锁副本
- 初始剧情旗标
- 初始选中装备
- 默认关系记录
- 默认结果提示

这就是数据驱动的起点。

默认状态不应该散在 `Hub.wt`、`SkillTree.wt`、`SideCombatBeastPath.wt` 这些场景文件里。

它应该来自统一内容表。

## 5.16 `ProgressionContent`：进度系统的数据来源

`GameProgress` 本身不应该写死所有策划内容。

所以它依赖 `ProgressionContent`。

入口文件是：

```text
assets/gameplay/progression/progression_content.yaml
```

这个文件再分发到多个表：

- `core/defaults.yaml`
- `inventory/materials.yaml`
- `inventory/upgrades.yaml`
- `dungeons/dungeons.yaml`
- `dungeons/reward_summary.yaml`
- `relationships/relationships.yaml`
- `skill_tree/skill_nodes.yaml`
- `equipment/equipment_slots.yaml`
- `equipment/equipment.yaml`

源码里的 `ProgressionContent::LoadContent()` 会加载这些 YAML。

如果文件缺失或解析失败，它会退回到 `BuildFallbackContent()`。

这对教学项目很友好：

- 数据正常时走外部表
- 数据坏了时不至于引擎直接崩
- 面试时能讲出数据驱动和 fallback 的边界

这里要注意：

> 本章只把 `ProgressionContent` 当作 `GameProgress` 的数据来源来讲。后面玩法层的 `Progression` 章节，会更详细讲据点、技能树、装备页这些具体玩法页面。

## 5.17 命令入口：`progression:` 怎么进来

前面 `CommandBus` 章已经讲过，`progression:` 命令会先进入：

```cpp
CommandBus::Execute(scene, command)
```

然后走：

```cpp
ExecuteProgressionCommand(command)
```

再进入：

```cpp
GameProgress::ExecuteCommand(command)
```

所以完整链路是：

```text
按钮 / .wts / UI 控件
    ↓
CommandBus::Execute()
    ↓
ExecuteProgressionCommand()
    ↓
GameProgress::ExecuteCommand()
    ↓
修改 GameProgress::State
```

这就是这一章和 `CommandBus` 的连接点。

## 5.18 `ExecuteCommand()` 支持哪些进度命令

当前 `GameProgress::ExecuteCommand()` 支持的命令大致可以分成六类。

第一类是剧情和章节：

```text
progression:set_flag:FLAG_CH02_TUTORIAL_STARTED
progression:clear_flag:FLAG_CH02_TUTORIAL_STARTED
progression:set_chapter:3
progression:reset
```

第二类是副本上下文：

```text
progression:set_active_dungeon:CH02_MAIN_BearAwakening
progression:clear_active_dungeon
```

第三类是技能树：

```text
progression:select_skill_node:magic_sword_core
progression:learn_selected_skill
```

第四类是装备和背包：

```text
progression:select_equipment_traveler_armor
progression:select_equipment_slot:armor
progression:toggle_selected_equipment
progression:upgrade_traveler_armor
progression:upgrade_item:travelerArmorLv1
progression:equipment_page_next
progression:equipment_page_prev
progression:equipment_page:2
progression:equipment_page_slider:2
```

第五类是支援角色：

```text
progression:select_support:mentor
```

第六类是设置命令：

```text
progression:set_text_speed:60
progression:set_master_volume:80
progression:set_bgm_volume:70
progression:set_sfx_volume:70
progression:text_speed_up
progression:toggle_fullscreen
progression:toggle_screen_shake
```

设置命令当前已经抽到了 `ProgressionSettingsCommandService`。

阅读源码时要注意一个历史痕迹：`GameProgress.cpp` 后面还保留了一些设置命令分支，但实际阅读主线应该优先看：

```cpp
ProgressionSettingsCommandService::IsSettingsCommand(action)
ProgressionSettingsCommandService::Execute(action, GetState())
```

这说明项目在从“集中写在一个文件里”逐步往“按职责拆服务”演进。

## 5.19 `CommandResult`：为什么进度命令也要返回结果

`GameProgress` 也定义了 `CommandResult`：

```cpp
struct CommandResult
{
    bool Handled = false;
    bool Success = false;
    bool Changed = false;
    std::string Message;
};
```

这和 `CommandBus` 里的结果语义保持一致。

它能区分：

- 有没有识别命令
- 命令是否成功
- 有没有真的修改状态
- 要给 UI 或日志显示什么消息

例如：

```text
progression:learn_selected_skill
```

可能出现三种情况：

- 节点可学，插入 `UnlockedSkills`，`Changed = true`
- 节点已经学过，`Success = true`，但 `Changed = false`
- 节点无效，`Success = true` 或失败消息，根据实现返回提示

这比只返回 `bool` 更适合 UI。

因为按钮点击后，玩家需要知道：

```text
是没有反应，
还是已经完成，
还是材料不足。
```

## 5.20 存档槽体系：不是只有一个文件

Wheatear 里有两种存档文件概念。

第一种是进度存档：

```text
assets/saves/progression_slotN.wtsave
```

它由 `GameProgress::SaveSlot()` 和 `GameProgress::LoadSlot()` 管。

第二种是 VN 运行时存档：

```text
assets/saves/slotN.vnstate
```

它由 VN runtime 的 `SaveState()` / `LoadState()` 管，`GameProgress` 只提供路径和占用检测辅助。

所以保存一格存档时，可能同时包含两类数据：

```text
全局进度：章节、材料、技能、装备、副本、场景路径
VN 运行时：脚本源、当前行、变量、选项状态
```

这是非常重要的一点。

面试官如果问“你怎么做存档”，不要只说“写文件”。要说：

> 我把全局进度和 VN 剧情运行时状态分开存。`GameProgress` 负责 `progression_slotN.wtsave`，VN runtime 负责 `slotN.vnstate`。读取时通过同一个槽位组合恢复。

## 5.21 存档槽数量和路径

当前最多 20 个槽位：

```cpp
static constexpr int kMaxSaveSlots = 20;
```

所有对外 slot 都会先经过：

```cpp
ClampSaveSlot(slot)
```

把槽位限制到 1 到 20。

进度存档路径来自：

```cpp
GameProgress::GetProgressSavePath(slot)
```

底层是：

```text
AssetPath::GetWritableRoot() / assets / saves / progression_slotN.wtsave
```

VN 运行时存档路径来自：

```cpp
GameProgress::GetGameRuntimeSavePath(slot, saveDirectory)
```

默认是：

```text
assets/saves/slotN.vnstate
```

这里用了 `GetWritableRoot()` 很关键。

因为运行时生成的数据不应该写进只读资源包，也不应该写进构建缓存中容易被覆盖的位置。

## 5.22 `SaveSlotInfo`：菜单为什么能显示槽位摘要

存读档菜单需要在按钮上显示：

```text
槽位 01  已有存档
槽位 1  第 2 章  Lv1  金币 0
```

这类文本不需要读取完整游戏状态。

所以有：

```cpp
SaveSlotInfo GetSaveSlotInfo(int slot)
```

`SaveSlotInfo` 只读摘要字段：

- `SaveVersion`
- `Chapter`
- `PlayerLevel`
- `Gold`
- `Objective`
- `ScenePath`

这样菜单可以快速判断：

- 有没有存档
- 显示第几章
- 显示玩家等级
- 读取时可能回到哪个场景

这是一个常见技巧：

```text
存档详情和槽位摘要分开。
```

大型项目里常会把摘要放到单独 metadata 文件或 save header 中。

Wheatear 当前把摘要直接写在 key-value 文本存档前面，简单但有效。

## 5.23 `SaveSlot()` 写了什么

`SaveSlot()` 会写一个文本格式的进度文件。

开头类似：

```text
schema=wheatear.progress.v3
saveVersion=3
scenePath=assets/scenes/Hub.wt
chapter=2
objective=...
```

然后写玩家状态：

```text
playerLevel=1
experience=0
experienceToNext=100
magicSwordLevel=1
travelerArmorLevel=0
gold=0
hp=420
atk=24
def=10
matk=18
mdef=8
```

再写集合和映射：

```text
completedDungeons=...
unlockedDungeons=...
unlockedSkills=...
ownedEquipment=...
equippedItems=armor:traveler_armor
equipmentLevels=traveler_armor:1
storyFlags=...
activeSupport=mentor
```

最后写材料、副本最高连击、关系记录：

```text
material.MAT-BEAST-SINEW=2
bestCombo.CH02_MAIN_BearAwakening=13
relationship.mentor.affinity=100
relationship.mentor.supportLevel=2
relationship.mentor.unlocked=1
```

这不是 YAML，也不是 JSON。

它是一个简单的 key-value 文本格式。

优点是：

- 新手容易看懂
- 手动排查方便
- 写起来很轻
- 面试讲解直观

缺点是：

- 类型系统弱
- 嵌套结构表达能力差
- schema 迁移要手写
- 没有校验和、压缩、加密
- 字符串转义规则需要自己维护

对当前 2D 竖切来说，这个选择是合理的。

但如果以后做更大的 RPG，就应该考虑：

- YAML / JSON
- 二进制存档
- 版本迁移系统
- 校验和
- 云存档同步

## 5.24 `LoadSlot()` 怎么读

`LoadSlot()` 的流程很清楚：

```text
打开 progression_slotN.wtsave
    ↓
如果打不开，写 LastResultMessage 并返回 false
    ↓
创建 loaded = MakeDefaultState()
    ↓
逐行读取 key=value
    ↓
把读到的字段覆盖到 loaded
    ↓
写入“已读取 N 号槽”提示
    ↓
GetState() = loaded
    ↓
ApplySettingsToRuntime()
```

这里最值得学习的是：

```cpp
State loaded = MakeDefaultState();
```

它不是从一个空状态开始读。

这样做的好处是：

- 老存档缺字段时仍然有默认值
- 新增字段后不用立刻让所有旧存档失效
- 数据表里的默认内容能继续补齐

这就是一种非常轻量的版本兼容策略。

它还不等于完整 migration 系统，但已经比“全字段必须存在”稳很多。

## 5.25 为什么保存时要特殊处理 SaveLoad 场景

`GetScenePathForSave()` 最终会走 `ResolveScenePathForSave()`。

它有一个关键判断：

```text
如果当前场景是 SaveLoad.wt，
就优先保存 PreviousScenePath。
```

这是因为存档菜单本身不是玩家的剧情或战斗进度。

举个例子：

```text
玩家在 Hub.wt
    ↓
点击保存
    ↓
SceneTransition 进入 SaveLoad.wt
    ↓
玩家选择槽位保存
```

如果保存 `CurrentScenePath`，结果是：

```text
scenePath=assets/scenes/SaveLoad.wt
```

下次读档就会回到存档页。

这显然不对。

正确做法是保存：

```text
scenePath=assets/scenes/Hub.wt
```

这就是 `PreviousScenePath` 的价值。

## 5.26 读取时怎么决定回到哪个场景

读取时不是简单地：

```text
loadgame:slot
```

而是生成：

```text
loadgame:scenePath:slot
```

对应 API 是：

```cpp
std::string BuildLoadGameCommand(int slot, const std::string& scenePath = "");
```

它内部会调用 `ResolveLoadScenePathForSlot()`。

优先级大致是：

1. 外部显式传入的 `scenePath`
2. `SaveSlotInfo` 里读出的 `ScenePath`
3. 从 VN runtime 存档里的 `SOURCE` 推断场景
4. 当前 / 上一个场景上下文
5. 默认加载场景

这里的第三条很有意思。

如果某个槽只有 VN runtime 状态，`GameProgress` 会尝试读取 `.vnstate` 里的脚本源，然后推断：

- `chapter3_preview.vn` -> `Chapter3Preview.wt`
- `post_fake.vn` -> `PostFake.wt`
- `intro.vn` -> `Intro.wt`

这不是一个大型通用资源索引系统，但对当前竖切非常实用。

它解决的是：

```text
我有剧情运行时存档，但全局进度摘要不完整时，还能尽量回到正确场景。
```

## 5.27 存档菜单链路一：据点打开保存页

`.wts` 里有：

```text
event go_save_load:
    gamesave:open_save_menu
end
```

链路是：

```text
按钮
    ↓ event:go_save_load
EventScriptSystem
    ↓ gamesave:open_save_menu
CommandBus
    ↓ gameplay queue
ProgressionSystem::UpdateGameSaveCommands()
    ↓
SceneTransitionService::RequestLoadScene(SaveLoad.wt)
    ↓
RuntimeSceneLayer::LoadScene()
```

这里要注意：

`gamesave:` 不是 `GameProgress::ExecuteCommand()` 直接处理。

它是 gameplay command，由 `ProgressionSystem` 消费。

原因是保存页有 UI 模式、覆盖确认、场景策略、VN 委托等流程，不只是改一个进度字段。

## 5.28 存档菜单链路二：点击槽位保存

保存页的按钮会被配置成：

```text
gamesave:slot_save_1
```

`ProgressionSystem::UpdateGameSaveCommands()` 收到后会先看：

- 当前场景是否允许保存
- 槽位是否已有存档
- 是否需要弹覆盖确认

如果可以保存，就调用：

```cpp
SaveProgressionOnlySlot(slot, saveDirectory)
```

里面会调用：

```cpp
GameProgress::SaveSlot(slot)
GameProgress::ClearGameRuntimeSaveSlot(slot, saveDirectory)
```

为什么这里要清旧 VN runtime 存档？

因为这是“进度页保存”，不一定有当前 VN runtime 状态。

如果旧的 `slotN.vnstate` 还留着，玩家下次读这一格时可能会读到旧剧情位置，和新的全局进度冲突。

所以这条链的策略是：

```text
进度页保存全局进度，
同时清理旧 VN 剧情状态，
避免混档。
```

## 5.29 VN 存档链路：保存剧情位置和全局进度

VN 模块里有另一条保存链：

```cpp
VisualNovelSystem::SaveToSlot()
```

它会做：

```text
GameProgress::SaveSlot(safeSlot)
VN runtime SaveState(savePath)
```

也就是说，在 VN 里保存时：

- `progression_slotN.wtsave` 保存全局进度
- `slotN.vnstate` 保存剧情运行时状态

这就是为什么同一个槽位可能有两份文件。

读取时，VN 还会在 `LoadRuntime()` 中根据 `AutoLoadSlot` 尝试：

```text
state.Runtime.LoadState(savePath)
GameProgress::LoadSlot(policy.AutoLoadSlot)
```

所以 VN 存档不是只恢复“第几行文本”，还会恢复全局材料、旗标、章节、装备等状态。

## 5.30 读取链路：从槽位回到正确场景

读取按钮通常是：

```text
gamesave:load_1
```

`ProgressionSystem` 会：

```text
检查 CanLoad
    ↓
检查 IsGameSaveSlotOccupied(slot)
    ↓
CommandBus::Execute(scene, GameProgress::BuildLoadGameCommand(slot))
```

`BuildLoadGameCommand()` 可能生成：

```text
loadgame:assets/scenes/Hub.wt:1
```

然后回到上一章的场景切换链路：

```text
CommandBus
    ↓
SceneTransitionService::RequestLoadGame(scenePath, slot)
    ↓
RuntimeSceneLayer::ExecuteSceneTransitionRequest()
    ↓
GameProgress::LoadSlot(slot)
    ↓
RuntimeSceneLayer::LoadScene(scenePath)
    ↓
ApplyPendingSceneAutoLoadSlot()
    ↓
Scene::OnRuntimeStart()
    ↓
VN 如有 AutoLoadSlot，再读 VN runtime state
```

这条链非常适合面试讲。

它能体现：

- 命令系统
- 场景切换
- 进度存档
- VN 运行时存档
- 场景存读档策略

这些不是孤立功能，而是连在一起的。

## 5.31 `SavePolicy`：为什么有些场景不能存读

`Scene` 里有：

```cpp
struct SavePolicy
{
    bool CanSave = true;
    bool CanLoad = true;
    std::string SaveDirectory = "assets/saves";
    int AutoLoadSlot = 0;
};
```

它属于 **Scene ECS 层** 的数据，但会被游戏运行服务层使用。

`ProgressionSystem` 和 `VisualNovelSystem` 都会检查：

- `CanSave`
- `CanLoad`
- `SaveDirectory`
- `AutoLoadSlot`

这说明存读档不是全局无条件开放。

比如：

- 主菜单可以读取，但不一定保存
- 战斗中可以禁止保存
- 过场中可以禁止读取
- VN 场景可以指定自动加载槽位

当前实现还支持临时覆盖策略：

```text
gamesave:push_policy:allow_all
gamesave:push_policy:block_all
gamesave:pop_policy
gamesave:clear_policy
```

这部分在 `ProgressionSystem` 里，不在 `GameProgress` 里。

这也再次说明：

```text
GameProgress 管状态，
ProgressionSystem 管页面和存读档流程，
Scene 管本场景的存读档策略。
```

## 5.32 横板副本链路：打完以后发生什么

现在看一个最能连接简历内容的例子。

玩家从据点进入横板副本：

```text
event tutorial_side_combat_start:
    progression:set_flag:FLAG_CH02_TUTORIAL_STARTED
    progression:set_active_dungeon:CH02_MAIN_BearAwakening
    scene:assets/scenes/SideCombatBeastPath.wt
end
```

这会设置：

```text
ActiveSideCombatDungeonId = CH02_MAIN_BearAwakening
```

然后进入横板战斗场景。

战斗过程中，怪物死亡可能掉材料。

`SideCombatPickupService::UpdatePickups()` 里会在玩家拾取时调用：

```cpp
GameProgress::AddMaterial(pickup.ItemId, pickup.DisplayName, pickup.Amount);
```

这表示材料不是等到结算页才凭空加的。

材料是在战斗中被拾取时写进 `GameProgress`。

战斗胜利后，`SideCombatOutcomeService` 会调用：

```cpp
GameProgress::RecordDungeonClear(...)
GameProgress::RecordLastDungeonResult(...)
```

第一步记录长期进度：

- 是否首通
- 已完成副本
- 最高连击
- 解锁新副本
- 设置剧情旗标
- 更新目标
- 加经验

第二步记录最近一次结果页数据：

- 评分
- 受击次数
- 通关时间
- 奖励摘要
- 奖励数量

所以链路是：

```text
横板战斗
    ↓ 拾取材料
GameProgress::AddMaterial()
    ↓ 胜利
GameProgress::RecordDungeonClear()
    ↓
GameProgress::RecordLastDungeonResult()
    ↓
event:side_combat_victory
    ↓
scene:assets/scenes/Result.wt
    ↓
ProgressionSystem 刷结果页
```

## 5.33 `RecordDungeonClear()` 做了什么

`RecordDungeonClear()` 是副本长期进度入口。

它的关键逻辑是：

```text
CompletedDungeons.insert(dungeonId)
    ↓
判断是不是 firstClear
    ↓
更新 BestCombosByDungeon
    ↓
首通时解锁新副本
    ↓
写入 FlagsOnClear
    ↓
更新 ObjectiveOnClear
    ↓
AddExperience()
```

以 `CH02_MAIN_BearAwakening` 为例，数据表里写了：

```text
unlocksOnFirstClear: CH02_MAT_BeastPath
flagsOnClear: FLAG_CH02_BOSS_DEFEATED, FLAG_HUB_UNLOCKED
objectiveOnClear: 在据点强化魔剑...
firstClearNotification: 新副本解锁：黑林兽道
```

所以打完黑熊后，不是 C++ 里硬编码“解锁黑林兽道”。

而是：

```text
dungeons.yaml
    ↓
ProgressionContent
    ↓
RecordDungeonClear()
    ↓
UnlockedDungeons / StoryFlags / Objective
```

这就是数据驱动流程。

## 5.34 `RecordLastDungeonResult()` 做了什么

`RecordLastDungeonResult()` 不是长期成长入口。

它更像“最近一次战斗结算快照”。

它会填：

```cpp
state.LastDungeonResult.Valid = true;
state.LastDungeonResult.DungeonId = dungeonId;
state.LastDungeonResult.DungeonName = DungeonDisplayName(dungeonId);
state.LastDungeonResult.Grade = grade.empty() ? "C" : grade;
state.LastDungeonResult.FirstClear = firstClear;
state.LastDungeonResult.BestCombo = bestCombo;
state.LastDungeonResult.HitsTaken = hitsTaken;
state.LastDungeonResult.ClearTimeSeconds = clearTimeSeconds;
state.LastDungeonResult.Experience = experience;
state.LastDungeonResult.RewardSummary = rewardSummary;
state.LastDungeonResult.RewardAmounts = rewardAmounts;
```

它服务的是：

- `BuildResultTitle()`
- `BuildResultStats()`
- `BuildResultRewards()`
- `ProgressionResultPageService::UpdateDrops()`

所以结果页只要读 `LastDungeonResult`。

这就是“玩法结果”和“展示页面”之间的缓冲。

## 5.35 结果页怎么显示材料图标

`ProgressionResultPageService::UpdateDrops()` 会读：

```cpp
const auto& state = GameProgress::GetState();
const bool hasResult = state.LastDungeonResult.Valid;
```

然后对每个图标查：

```cpp
state.LastDungeonResult.RewardAmounts
```

同时 tooltip 里会读：

```cpp
GameProgress::GetMaterialAmount(icon.ItemId)
```

这说明结果页显示了两层信息：

- 本次掉落多少
- 背包里现在总共有多少

这是很常见的 2D RPG / ACT 结算页设计。

玩家看到的不只是“这次拿了什么”，还会看到“这些材料对当前成长有什么意义”。

## 5.36 据点页面怎么读 GameProgress

`ProgressionSystem::UpdateHub()` 会调用：

- `BuildHubSubtitle()`
- `BuildHubStatus()`
- `GetDungeonButtonText()`
- `GetSkillButtonText()`
- `GetEquipmentButtonText()`

这些函数都在 `GameProgress` 里。

例如 `BuildHubSubtitle()` 会把：

- 当前章节
- 魔剑等级
- 玩家等级
- 经验条
- 材料副本是否解锁

拼成一段文本。

链路是：

```text
ProgressionSystem::OnUpdateRuntime()
    ↓
UpdateHub()
    ↓
GameProgress::BuildHubSubtitle()
    ↓
UIRuntimeTools::SetText()
```

这就是“进度状态驱动 UI”的最直接例子。

## 5.37 技能树升级链路

技能树相关字段：

```cpp
SelectedSkillNodeId
UnlockedSkills
MagicSwordLevel
Materials
CurrentChapter
```

常见命令：

```text
progression:select_skill_node:magic_sword_lv2
progression:learn_selected_skill
```

`learn_selected_skill` 会做：

```text
找当前选中节点
    ↓
检查章节是否开放
    ↓
如果是 magic_sword_lv2，走魔剑 Lv2 专属升级
    ↓
如果依赖魔剑 Lv2，检查 MagicSwordLevel
    ↓
插入 UnlockedSkills
```

魔剑 Lv2 的专属升级会：

- 检查材料
- 扣材料
- 把 `MagicSwordLevel` 改成 2
- 应用属性加成
- 解锁技能
- 改目标提示
- 推通知

这条链路很适合说明：

```text
技能树不是只点亮 UI，
它实际修改玩家长期进度。
```

## 5.38 装备强化链路

装备强化相关字段：

```cpp
OwnedEquipment
EquippedItemsBySlot
EquipmentLevels
TravelerArmorLevel
SelectedEquipmentId
Materials
Gold
Attributes
```

通用入口是：

```cpp
CanUpgradeEquipment(upgradeId)
TryUpgradeEquipment(upgradeId)
```

它会先从 `ProgressionContent` 找：

```cpp
ProgressionContent::FindUpgrade(upgradeId)
```

然后检查：

- 配方是否存在
- 是否是装备强化
- 玩家是否拥有装备
- 当前等级是否已经达到目标
- 材料是否足够
- 金币是否足够

真正强化成功后：

- 写入 `EquipmentLevels`
- 如果是旅人护衣，同步旧字段 `TravelerArmorLevel`
- 应用属性加成
- 解锁配方中配置的技能
- 写 `LastResultMessage`
- 推通知

这里有一个很现实的兼容点：

```cpp
TravelerArmorLevel
EquipmentLevels
```

两者同时存在。

`TravelerArmorLevel` 是早期固定字段，`EquipmentLevels` 是后续更通用的数据结构。

当前实现会让它们保持同步。

这说明项目不是一开始就完美抽象，而是在开发中从“竖切写死”演进到“表驱动”。

面试时这反而是好讲的：

> 我保留旧字段兼容已有 UI 和存档，同时引入 `EquipmentLevels` 支持通用装备强化。等后续重构稳定后，可以逐步迁移掉旧字段。

## 5.39 装备页选择和分页

装备页不只是强化。

它还支持：

- 按槽位查看当前装备
- 选择背包装备
- 装备 / 脱下
- 翻页
- slider 跳页

对应命令有：

```text
progression:select_equipment_slot:armor
progression:select_equipment_traveler_armor
progression:toggle_selected_equipment
progression:equipment_page_next
progression:equipment_page_slider:2
```

这些命令看起来像 UI 命令，但它们修改的是进度状态中的装备选择和装备槽。

所以也放在 `GameProgress::ExecuteCommand()`。

这再次说明 `GameProgress` 当前承担了两类状态：

- 真正长期进度
- 部分进度页面 UI 状态

对教学型项目可以接受，但后续扩展时可以拆成 `ProgressionViewState`。

## 5.40 支援角色选择链路

支援页按钮可能写：

```text
progression:select_support:mentor
progression:select_support:white_mage
```

`GameProgress` 会：

```text
通过 ProgressionContent 找角色
    ↓
检查角色是否开放
    ↓
写 ActiveSupportCharacterId
    ↓
写 LastResultMessage
```

`BuildSupportStatus()` 再根据 `ActiveSupportCharacterId` 显示当前支援效果。

这对 2D 游戏很常见：

- 剧情推进解锁队友
- 据点里选择支援
- 战斗中根据支援角色提供额外效果

当前实现的支援效果更多是竖切演示层，后续可以接到战斗系统。

## 5.41 `EventScript` 怎么读取进度条件

上一章 `EventScript` 讲过条件语言。

它在 `EvaluateCondition()` 里读取：

```cpp
GameProgress::GetState()
```

因此 `.wts` 可以判断：

```text
if flag FLAG_HUB_UNLOCKED
if last_dungeon CH02_MAIN_BearAwakening
if dungeon CH02_MAT_BeastPath
if completed CH02_MAIN_BearAwakening
if skill magic_sword_lv2
if equipment traveler_armor
if equipped traveler_armor
if chapter >= 3
if material MAT-BEAST-SINEW >= 2
```

这就是 `GameProgress` 和 `EventScript` 的关系：

```text
EventScript 负责编排流程，
GameProgress 提供流程判断所需的事实。
```

例如：

```text
event go_next_story:
    if flag FLAG_CH02_BOSS_DEFEATED
        progression:set_flag:FLAG_CH03_PREVIEW_STARTED
        progression:set_chapter:3
        scene:assets/scenes/Chapter3Preview.wt
    endif
end
```

这段脚本能成立，是因为 `RecordDungeonClear()` 已经把 `FLAG_CH02_BOSS_DEFEATED` 写进了 `StoryFlags`。

## 5.42 真实链路一：主菜单新游戏

主菜单脚本：

```text
event menu_new_game:
    newgame:assets/scenes/Intro.wt
end
```

完整链路：

```text
主菜单按钮
    ↓
event:menu_new_game
    ↓
EventScriptSystem
    ↓
newgame:assets/scenes/Intro.wt
    ↓
CommandBus
    ↓
SceneTransitionService::RequestNewGame()
    ↓
RuntimeSceneLayer::ExecuteSceneTransitionRequest()
    ↓
GameProgress::ResetForNewGame()
    ↓
RuntimeSceneLayer::LoadScene(Intro.wt)
```

`ResetForNewGame()` 做两件事：

```text
GetState() = MakeDefaultState()
ApplySettingsToRuntime()
```

这说明新游戏不是简单切场景。

它要先把：

- 材料
- 章节
- 旗标
- 解锁
- 装备
- 最近结果
- UI 选择

恢复到默认起点。

## 5.43 真实链路二：VN 中保存和读取

在 VN 里，玩家打开保存菜单：

```text
vn:savemenu
```

或者点击槽位：

```text
gamesave:slot_save_1
```

最终会进入 `VisualNovelSystem::SaveToSlot()`。

它会执行：

```text
GameProgress::SaveSlot(1)
VisualNovelRuntime::SaveState(slot1.vnstate)
```

读取时则是：

```text
gamesave:load_1
    ↓
VisualNovelSystem::LoadFromSlot()
    ↓
CommandBus::Execute(scene, GameProgress::BuildLoadGameCommand(1))
    ↓
loadgame:scenePath:1
    ↓
SceneTransitionService
    ↓
RuntimeSceneLayer
    ↓
GameProgress::LoadSlot(1)
    ↓
LoadScene(scenePath)
    ↓
VN AutoLoadSlot 读剧情运行时状态
```

这条链非常有教材价值，因为它让初学者看到：

```text
保存不是一个函数，
而是一组系统按顺序协作。
```

## 5.44 真实链路三：打完黑熊后据点为什么变了

现在看最完整的一条链。

玩家从据点进入黑熊主线：

```text
progression:set_active_dungeon:CH02_MAIN_BearAwakening
scene:assets/scenes/SideCombatBeastPath.wt
```

战斗里拾取掉落：

```text
SideCombatPickupService
    ↓
GameProgress::AddMaterial()
```

胜利时：

```text
SideCombatOutcomeService
    ↓
GameProgress::RecordDungeonClear()
    ↓
GameProgress::RecordLastDungeonResult()
```

然后 `.wts`：

```text
event side_combat_victory:
    if last_dungeon CH02_MAIN_BearAwakening
        progression:set_flag:FLAG_CH02_TUTORIAL_CLEARED
    endif
    wait 0.10
    scene:assets/scenes/Result.wt
end
```

结果页显示：

```text
ProgressionSystem::UpdateResult()
    ↓
BuildResultTitle()
BuildResultStats()
ProgressionResultPageService::UpdateDrops()
```

回据点后：

```text
ProgressionSystem::UpdateHub()
    ↓
BuildHubSubtitle()
BuildHubStatus()
GetDungeonButtonText()
GetSkillButtonText()
GetEquipmentButtonText()
```

于是玩家会看到：

- 副本完成
- 黑林兽道解锁
- 经验增加
- 材料增加
- 目标改变
- 技能树或装备按钮状态改变

这就是“一个战斗结果如何影响整个游戏流程”的完整答案。

## 5.45 和 Unity 的对照

Unity 项目里常见做法是：

- `GameManager` 或 `ProgressManager` 单例
- `ScriptableObject` 做配置表
- `PlayerPrefs` 或自定义 JSON 做存档
- `DontDestroyOnLoad` 保留跨场景对象
- `SceneManager.LoadScene()` 切场景

Wheatear 的 `GameProgress` 和 Unity 的 `GameManager` 有相似之处：

- 都是跨场景状态中心
- 都会被 UI、剧情、战斗访问
- 都可能成为存档入口

但 Wheatear 的区别是：

- 命令入口统一走 `CommandBus`
- 场景切换请求先进入 `SceneTransitionService`
- 存档场景路径会特殊处理 `SaveLoad.wt`
- 数据表通过 `ProgressionContent` 读取 YAML
- VN 运行时状态和全局进度状态分开保存

所以面试时不要说“就是 GameManager”。

更准确的说法是：

> 它类似 Unity 项目里的 ProgressManager，但我把它接到了命令总线、事件脚本、场景切换和存档槽体系里。

## 5.46 和 UE 的对照

UE 里常见相关概念有：

- `UGameInstance`
- `USaveGame`
- `AGameMode`
- `AGameState`
- `APlayerState`

如果拿 Wheatear 对照：

```text
GameProgress::State
    ≈ 单机项目里的 GameInstance 持久进度 + SaveGame 数据
```

但它不等价于 UE 的 `GameState`。

因为 UE 的 `GameState` 更偏当前关卡运行状态，尤其在联网游戏里还要复制给客户端。

`GameProgress` 现在更接近：

- 单机玩家进度
- 存档数据
- 跨场景状态
- 据点页面 view model

UE 项目里通常会把：

- 长期存档放 `USaveGame`
- 跨关卡临时状态放 `UGameInstance`
- 当前关卡规则放 `GameMode`
- 当前关卡可复制状态放 `GameState`

Wheatear 目前把这些里的一部分合在 `GameProgress`，这是小型单机 2D 项目可以接受的压缩设计。

## 5.47 和 Godot 的对照

Godot 里常见做法是：

- 用 Autoload 单例保存全局进度
- 用 `Resource` 或 `ConfigFile` 保存数据
- 用 signal 通知 UI 刷新
- 用 scene tree 切换场景

Wheatear 的 `GameProgress` 很像一个 C++ 版 Autoload：

- 全局可访问
- 跨场景存在
- 存一份玩家进度
- UI 和玩法都能读取

区别在于：

- Wheatear 没有 Godot 那套运行时反射和脚本信号环境
- 它用 `CommandBus` 和 `.wts` 连接流程
- 它直接用 C++ 和文本存档控制状态

## 5.48 这一层的好处

`GameProgress` 的好处很明显。

第一，状态集中。

材料、技能、装备、副本、章节不散在各个玩法里。

第二，流程好串。

`.wts` 可以通过 `progression:` 改状态，通过条件读取状态。

第三，存档好做。

`SaveSlot()` 和 `LoadSlot()` 能把核心进度写成一份统一文件。

第四，UI 好刷新。

据点、结果页、技能树、装备页都从同一份状态生成文本。

第五，面试好讲。

它能把你的简历关键词串起来：

```text
ECS 场景
CommandBus
EventScript
SceneTransition
Save/Load
SideCombat result
Progression UI
```

## 5.49 这一层的坏处

它也有明显代价。

第一，全局状态会让依赖变多。

很多模块都能直接 `GameProgress::GetState()`，如果不控制边界，会变成到处读写。

第二，状态和 UI 文案有耦合。

`BuildHubStatus()`、`BuildEquipmentDetails()` 这类函数把文案生成也放在 `GameProgress` 里。

对当前项目很方便，但以后做本地化或复杂 UI 时，最好拆成 view model 或 UI presenter。

第三，存档格式还比较简单。

key-value 文本很好读，但不适合特别复杂的嵌套对象。

第四，版本迁移还不完整。

当前通过 `MakeDefaultState()` 补默认值，能兼容一部分旧字段缺失，但还没有完整 migration 表。

第五，不适合联网状态。

它没有线程安全、复制、预测、回滚这些机制。

所以它适合：

```text
单机、简单 2D、教学型、竖切 Demo。
```

不适合直接吹成：

```text
大型商业 RPG 完整存档框架。
```

## 5.50 常见误区

### 误区一：GameProgress 就是存档系统

不是。

它是进度状态中心，存档只是其中一部分。

### 误区二：LoadSlot 会自动切场景

不会。

`LoadSlot()` 只读全局进度。

真正切场景要通过：

```text
BuildLoadGameCommand()
    ↓
loadgame:
    ↓
SceneTransitionService
    ↓
RuntimeSceneLayer::LoadScene()
```

### 误区三：VN 存档和全局进度存档是同一个文件

不是。

它们是同一个 slot 下的两类文件：

```text
progression_slotN.wtsave
slotN.vnstate
```

### 误区四：保存当前场景就是保存 `CurrentScenePath`

不一定。

如果当前在 `SaveLoad.wt`，应该保存 `PreviousScenePath`。

### 误区五：结果页负责结算

不是。

结果页只展示 `LastDungeonResult`。

真正结算在横板战斗和 `GameProgress` 的记录函数里。

### 误区六：所有 progression 命令都应该写在 GameProgress 里

也不是。

例如 `gamesave:` 菜单流程在 `ProgressionSystem`，设置命令逐步抽到了 `ProgressionSettingsCommandService`。

`GameProgress` 应该管状态，复杂页面流程可以拆出去。

## 5.51 面试怎么讲

如果面试官问“GameProgress 是什么”，可以这样答：

> `GameProgress` 是 Wheatear 的进度状态中心，处在游戏运行服务层。它保存跨场景的玩家进度，比如章节、目标、材料、经验、技能、装备、副本完成状态、剧情旗标和最近一次战斗结果。UI、事件脚本、场景切换和玩法模块都通过它读写这些低频状态。

如果面试官问“你怎么做存档”，可以这样答：

> 我把存档分成全局进度和 VN 运行时状态。全局进度由 `GameProgress::SaveSlot()` 写到 `progression_slotN.wtsave`，包含章节、材料、技能、装备、副本、场景路径等；VN runtime 自己保存 `slotN.vnstate`，包含剧情脚本位置和变量。读取时用 `BuildLoadGameCommand()` 决定回哪个场景，再由 `SceneTransitionService` 和 `RuntimeSceneLayer` 落地加载。

如果面试官问“为什么不直接把这些写在场景里”，可以这样答：

> 因为场景会被切换和重新加载，副本、据点、结果页、技能树都需要共享玩家长期进度。如果把这些散在场景组件里，切场景后状态很容易丢，也不好存档。`GameProgress` 把跨场景事实集中起来，场景只负责表现和局部运行状态。

如果面试官问“打完副本后如何影响据点和技能树”，可以这样答：

> 横板战斗胜利时会调用 `RecordDungeonClear()`，它记录首通、最高连击、解锁副本、剧情旗标、当前目标和经验；再调用 `RecordLastDungeonResult()` 保存结果页需要的评分、时间、受击和奖励数量。据点和技能树每帧通过 `ProgressionSystem` 读取 `GameProgress` 生成 UI 文本，所以结果会自然反映到多个页面。

如果面试官问“这个设计有什么不足”，可以这样答：

> 当前实现偏轻量，适合单机 2D 竖切。它的不足是全局状态依赖较多，存档格式是简单 key-value，UI 文案和进度状态还有耦合，版本迁移也比较基础。后续如果项目变大，可以拆出 SaveGame schema、ProgressionViewModel、迁移系统和更强的数据校验。

## 5.52 给初学者的阅读顺序

第一次看这章，不要从 `GameProgress.cpp` 第一行一直滚到底。

建议这样读：

1. 先看 `GameProgress.h` 里的 `State`
2. 再看 `MakeDefaultState()`
3. 再看 `ResetForNewGame()`
4. 再看 `SaveSlot()` 和 `LoadSlot()`
5. 再看 `BuildLoadGameCommand()`
6. 再看 `RecordDungeonClear()` 和 `RecordLastDungeonResult()`
7. 再看 `ExecuteCommand()`
8. 最后看 `BuildHubStatus()`、`BuildResultStats()`、`BuildEquipmentDetails()` 这些 UI 文本生成函数

这样你会先理解“状态是什么”，再理解“状态怎么进出文件”，最后理解“状态怎么驱动页面”。

## 5.53 你可以马上做的五个练习

### 练习一：给 State 分组

打开 `GameProgress.h`，把 `State` 里的字段手动分成：

- 章节和场景
- 玩家成长
- 材料和货币
- 解锁和旗标
- UI 选择状态
- 战斗结算状态

这是理解进度系统最快的一步。

### 练习二：模拟一次新游戏

手工写出这条链：

```text
event:menu_new_game
    ↓
newgame:
    ↓
ResetForNewGame()
    ↓
MakeDefaultState()
    ↓
LoadScene(Intro.wt)
```

然后想一想：哪些状态应该被清掉，哪些设置不应该丢？

### 练习三：手工读一个存档

假设存档里有：

```text
chapter=3
playerLevel=2
material.MAT-BEAST-SINEW=4
storyFlags=FLAG_HUB_UNLOCKED|FLAG_CH02_BOSS_DEFEATED
scenePath=assets/scenes/Hub.wt
```

说明读档后：

- 当前章节是多少？
- 背包里有什么？
- `.wts` 条件能判断哪些 flag？
- `BuildLoadGameCommand()` 会倾向回哪个场景？

### 练习四：追一次副本结算

从 `SideCombatOutcomeService.cpp` 里找到：

```cpp
RecordDungeonClear()
RecordLastDungeonResult()
```

然后追到：

```text
BuildResultTitle()
BuildResultStats()
ProgressionResultPageService::UpdateDrops()
```

你会看到结果页不是凭空显示，它读的是 `LastDungeonResult`。

### 练习五：设计一个新材料

在材料表里增加一个：

```text
MAT-SILVER-LEAF
```

然后思考需要改哪些地方：

- `materials.yaml`
- 某个副本掉落
- 某个 upgrade cost
- 结果页图标是否要增加
- `GameProgress::AddMaterial()` 是否需要改

答案通常是：如果只是材料数量和升级消耗，`GameProgress::AddMaterial()` 不需要改。

这就是数据驱动的价值。

## 5.54 本章小结

这一章最重要的结论是：

```text
GameProgress 是游戏运行服务层里的进度状态中心。
它不负责渲染，不负责 ECS，不负责真正切场景，
它负责记录跨场景、跨玩法、可存档的玩家进度事实。
```

再压成一条链：

```text
CommandBus / EventScript / Gameplay
    ↓
GameProgress
    ↓
State / SaveSlot / Unlocks / LastDungeonResult
    ↓
ProgressionSystem / VN / Result / Hub / SceneTransition
```

如果说前面几章分别解决了：

- `CommandBus`：命令怎么分发
- `EventScript`：流程怎么编排
- `ActionSystem`：动作怎么表达和记录
- `SceneTransition`：场景怎么切

那么这一章解决的就是：

```text
游戏世界如何记住玩家走到哪了。
```

## 5.55 下一章

到这里，**游戏运行服务层** 里最容易被面试追问的几条主链路已经补齐了：

- 命令总线
- 事件脚本
- WAO 动作编排
- 场景切换
- 进度状态中心

下一步建议进入 **第 6 编 玩法层**。

如果按面试优先级，下一章最适合写：

```text
06_玩法层/01_VN模块.md
```

因为 VN 是剧情演出和存读档链路最直观的入口，它能承接本章的 `.vnstate`、`GameProgress`、`CommandBus` 和 `EventScript`。
