# 《Wheatear 异世界项目》竖切 VN 剧本文本 v0.1

## 0. 文档定位

本文档负责第一个可玩竖切中的 VN 剧情文本、节点、选项、标记和场景跳转。

竖切剧情范围：

- 主菜单进入新游戏。
- 序章：现实世界上学路。
- 第一章：异世界醒来、假青梅被掳、假玩法吐槽。
- 第二章：导师送魔剑、正式横板教程、黑熊丈夫 Boss。
- 据点解锁。
- 第三章入口预告。

本文档不是完整终稿小说，而是可导入引擎的数据化剧本母稿。后续可以拆成 `VNDialogueScript` 数据资产。

## 1. 剧本格式约定

建议 VN 节点字段：

| 字段 | 说明 |
| --- | --- |
| `NodeId` | 节点 ID |
| `SceneId` | 场景 ID |
| `Background` | 背景资源 |
| `Music` | 音乐资源 |
| `Sfx` | 音效资源，可为空 |
| `Speaker` | 说话人 |
| `Portrait` | 立绘和表情 |
| `Text` | 对白或旁白 |
| `Choices` | 选项，可为空 |
| `SetFlags` | 节点完成后写入标记 |
| `Next` | 下一节点或场景 |

文字风格：

- 主角吐槽感要保留，尤其第一章假玩法结束后。
- 导师语气冷静、专业，但偶尔露出青梅才会知道的细节。
- 第一章的假玩法要故意让玩家怀疑游戏很粗糙，再由主角主动吐槽转化成剧情包袱。
- 第二章开始要明显提升“这才是真正战斗”的质感。

## 2. 角色代号

| CharacterId | 显示名 | 说明 |
| --- | --- | --- |
| `CHAR_PROTAG` | 主角 | 玩家扮演角色 |
| `CHAR_OS_REAL` | 青梅 | 现实世界青梅 |
| `CHAR_FAKE_OS` | 青梅？ | 假青梅傀儡，前期不暴露 |
| `CHAR_MENTOR` | 魔剑士导师 | 真青梅伪装身份 |
| `CHAR_GRANDMAGE_FAKE` | 大魔法师 | 国师伪装 / 示弱阶段 |
| `CHAR_SYSTEM` | 系统 | 教学、结算、提示 |

注意：

- 第一部终章前不要在 UI 或脚本元数据里把 `CHAR_MENTOR` 显示成真青梅。
- 假青梅在玩家可见文本里先显示为“青梅？”。
- 内部可以用 `CHAR_FAKE_OS` 记录，避免后续反转难以维护。

## 3. 全局剧情标记

| Flag | 写入时机 | 用途 |
| --- | --- | --- |
| `Flag_NewGameStarted` | 新游戏开始 | 初始化存档 |
| `Flag_PrologueComplete` | 序章结束 | 解锁第一章 |
| `Flag_CH01_FakeGameplayComplete` | 第一章假玩法结束 | 标记假玩法完成 |
| `Flag_FakeSweetheartAbducted` | 假青梅被掳走 | 主线动机 |
| `Flag_CH01_Cliffhanger` | 主角被黑熊逼入绝境 | 跳转第二章 |
| `Flag_MagicSwordGiven` | 导师送魔剑 | 解锁魔剑 |
| `Flag_CH02_BearMotherDefeated` | 黑熊母熊被击败 | 掉落吸收演出 |
| `Flag_CH02_BearHusbandAppeared` | 黑熊丈夫登场 | Boss 战 |
| `Flag_CH02_BossDefeated` | 黑熊丈夫被击败 | 解锁据点 |
| `Flag_HubUnlocked` | 第二章结算后 | 据点 UI 可用 |
| `Flag_CH02_Mat_BeastPathUnlocked` | 据点解锁时 | 黑林兽道可刷 |
| `Flag_CH03_EntryUnlocked` | 竖切末尾 | 第三章入口 |

## 4. 场景流程

```text
SCN_MainMenu
-> SCN_PrologueVN
-> SCN_CH01_FakeStart
-> ArcadeCombat: CH01_FAKE_ArcadeAwakening
-> SCN_CH01_PostFakeGameplay
-> SCN_CH02_AwakeningVN
-> SideCombat: CH02_MAIN_BearAwakening
-> SCN_CH02_PostBossVN
-> SCN_Hub_Prototype
-> SideCombat: CH02_MAT_BeastPath
-> SCN_Hub_Prototype
-> SCN_CH03_EntryVN
```

## 5. SCN_MainMenu：主菜单

### Node MENU_000

| 字段 | 内容 |
| --- | --- |
| Background | `BG_MainMenu_FantasyForest` |
| Music | `BGM_MainTheme_Soft` |
| Speaker | 系统 |
| Text | `Wheatear` |
| Choices | `新游戏 -> PRO_000`，`继续 -> LoadGame`，`退出 -> Quit` |

新游戏写入：

- `Flag_NewGameStarted`
- 初始化主角等级 Lv1。
- 初始化材料、装备、技能、剧情标记。

## 6. SCN_PrologueVN：序章，上学路

### PRO_000

| 字段 | 内容 |
| --- | --- |
| Background | `BG_Modern_SchoolRoad_Morning` |
| Music | `BGM_Daily_Morning` |
| Speaker | 旁白 |
| Text | 清晨的上学路和往常一样。路边的便利店刚刚亮灯，风吹过树梢，青梅走在我身侧，书包带被她绕在手指上，一圈又一圈。 |
| Next | PRO_010 |

### PRO_010

| Speaker | 青梅 |
| Portrait | `OS_REAL_Normal` |
| Text | 你今天又差点迟到。 |
| Next | PRO_020 |

### PRO_020

| Speaker | 主角 |
| Portrait | `PROTAG_School_Tired` |
| Text | 只是差点而已。没有迟到就说明我的时间管理还活着。 |
| Next | PRO_030 |

### PRO_030

| Speaker | 青梅 |
| Portrait | `OS_REAL_Smile` |
| Text | 你的时间管理每次都是被我从地上捡起来的。 |
| Next | PRO_040 |

### PRO_040

| Speaker | 主角 |
| Text | 那就麻烦你以后也继续捡。 |
| Choices | `谢谢你一直等我 -> PRO_050A`，`明天我肯定早起 -> PRO_050B` |

### PRO_050A

| Speaker | 青梅 |
| Portrait | `OS_REAL_Blush` |
| Text | 这句话我记下了。你以后想反悔也没用。 |
| SetFlags | `Var_PrologueTone=Gentle` |
| Next | PRO_060 |

### PRO_050B

| Speaker | 青梅 |
| Portrait | `OS_REAL_Smirk` |
| Text | 这句话我也记下了。明天早上我会提前十分钟敲你窗户。 |
| SetFlags | `Var_PrologueTone=Comedic` |
| Next | PRO_060 |

### PRO_060

| Speaker | 旁白 |
| Text | 她笑起来的时候总像在宣布一件理所当然的事，好像我们会这样一路走下去，今天、明天，很多年后也一样。 |
| Next | PRO_070 |

### PRO_070

| Sfx | `SFX_DistantMetal` |
| Speaker | 主角 |
| Portrait | `PROTAG_School_Alert` |
| Text | 等一下。刚才那是什么声音？ |
| Next | PRO_080 |

### PRO_080

| Speaker | 青梅 |
| Portrait | `OS_REAL_Worried` |
| Text | 你听错了吧？这条路平时很安静的。 |
| Next | PRO_090 |

### PRO_090

| Sfx | `SFX_FootstepFast` |
| Speaker | 旁白 |
| Text | 有人从背后靠近。太快了。快到我还没能回头，青梅的手已经用力抓住了我的袖口。 |
| Next | PRO_100 |

### PRO_100

| Sfx | `SFX_Impact_Stab_Muffled` |
| Music | `BGM_Silence` |
| Speaker | 主角 |
| Portrait | `PROTAG_Shocked` |
| Text | ……青梅？ |
| Next | PRO_110 |

### PRO_110

| Speaker | 青梅 |
| Portrait | `OS_REAL_Pain` |
| Text | 别……看。 |
| Next | PRO_120 |

### PRO_120

| Speaker | 旁白 |
| Text | 她向我倒过来。我伸手去接，身体却像被抽走了力气。视野里的天空、街道和她的脸，一起碎成了白光。 |
| SetFlags | `Flag_PrologueComplete` |
| Next | CH01_000 |

## 7. SCN_CH01_FakeStart：第一章，假的开始

### CH01_000

| Background | `BG_Otherworld_Forest_Dim` |
| Music | `BGM_Otherworld_Unease` |
| Speaker | 主角 |
| Portrait | `PROTAG_Isekai_Confused` |
| Text | ……我没死？ |
| Next | CH01_010 |

### CH01_010

| Speaker | 旁白 |
| Text | 我睁开眼，看到的是陌生的树冠。空气里有潮湿的泥土味，远处传来不属于城市的低吼。 |
| Next | CH01_020 |

### CH01_020

| Speaker | 主角 |
| Text | 这里不是医院，也不是学校。更不可能是我家楼下。 |
| Next | CH01_030 |

### CH01_030

| Speaker | 主角 |
| Portrait | `PROTAG_Isekai_Panic` |
| Text | 青梅呢？她也被刺中了。她在哪里？ |
| Next | CH01_040 |

### CH01_040

| Sfx | `SFX_BranchSnap` |
| Speaker | 青梅？ |
| Portrait | `FAKE_OS_Dazed` |
| Text | ……你在这里。 |
| Next | CH01_050 |

### CH01_050

| Speaker | 主角 |
| Portrait | `PROTAG_Isekai_Relieved` |
| Text | 青梅！太好了，你没事！ |
| Next | CH01_060 |

### CH01_060

| Speaker | 青梅？ |
| Portrait | `FAKE_OS_EmptySmile` |
| Text | 我……没事。只是有点冷。 |
| Next | CH01_070 |

### CH01_070

| Speaker | 旁白 |
| Text | 她的声音很像她，可又有什么地方不对。她的眼神像隔着一层薄薄的雾，看着我，却没有真正落在我身上。 |
| Next | CH01_080 |

### CH01_080

| Sfx | `SFX_MagicCircle` |
| Music | `BGM_Threat_Mage` |
| Speaker | 大魔法师 |
| Portrait | `GRANDMAGE_Fake_Calm` |
| Text | 找到了。异世界的灵魂，和被标记的容器。 |
| Next | CH01_090 |

### CH01_090

| Speaker | 主角 |
| Portrait | `PROTAG_Isekai_Angry` |
| Text | 你是谁？离她远点！ |
| Next | CH01_100 |

### CH01_100

| Speaker | 大魔法师 |
| Portrait | `GRANDMAGE_Fake_Smile` |
| Text | 现在的你连这个世界的一粒尘土都保护不了。 |
| Next | CH01_110 |

### CH01_110

| Sfx | `SFX_MagicBind` |
| Speaker | 青梅？ |
| Portrait | `FAKE_OS_Fading` |
| Text | 等…… |
| Next | CH01_120 |

### CH01_120

| Speaker | 旁白 |
| Text | 魔法阵在她脚下展开。下一瞬，她被光束卷起，向森林深处的天空消失。 |
| SetFlags | `Flag_FakeSweetheartAbducted` |
| Next | CH01_130 |

### CH01_130

| Speaker | 大魔法师 |
| Text | 如果你还想追，就先从它口中活下来。 |
| Next | CH01_140 |

### CH01_140

| Sfx | `SFX_BearRoar` |
| Speaker | 旁白 |
| Text | 巨大的黑影从树后撞出来。那是一头黑熊，眼里燃着不自然的魔光。 |
| Next | CH01_150 |

### CH01_150

| Speaker | 系统 |
| Text | 进入第一章假玩法。 |
| Next | `ArcadeCombat:CH01_FAKE_ArcadeAwakening` |

## 8. ArcadeCombat 返回：假玩法吐槽

### CH01_POST_000

| Background | `BG_Otherworld_Forest_Dim` |
| Music | `BGM_Comedy_Dry` |
| Speaker | 主角 |
| Portrait | `PROTAG_Isekai_Exhausted` |
| Text | 等等，刚才那是什么战斗？为什么我感觉自己像在一个临时搭出来的方块世界里逃命？ |
| SetFlags | `Flag_CH01_FakeGameplayComplete` |
| Next | CH01_POST_010 |

### CH01_POST_010

| Speaker | 主角 |
| Text | 如果这就是异世界的战斗系统，那我大概不是被召唤来的，我是被坑来的。 |
| Next | CH01_POST_020 |

### CH01_POST_020

| Sfx | `SFX_BearRoar_Near` |
| Music | `BGM_Danger_Run` |
| Speaker | 旁白 |
| Text | 黑熊的咆哮再次逼近。它没有给我继续吐槽的余地。 |
| Next | CH01_POST_030 |

### CH01_POST_030

| Speaker | 主角 |
| Portrait | `PROTAG_Isekai_Panic` |
| Text | 行，至少逃跑这件事不用教学。 |
| SetFlags | `Flag_CH01_Cliffhanger` |
| Next | CH02_000 |

## 9. SCN_CH02_AwakeningVN：第二章，魔剑觉醒

### CH02_000

| Background | `BG_Otherworld_Forest_Cliff` |
| Music | `BGM_Danger_Run` |
| Speaker | 旁白 |
| Text | 我被逼到一处断崖前。前方无路，后方是黑熊沉重的脚步声。 |
| Next | CH02_010 |

### CH02_010

| Speaker | 主角 |
| Portrait | `PROTAG_Isekai_Desperate` |
| Text | 青梅被抓走，我却连一头熊都甩不掉。开什么玩笑。 |
| Next | CH02_020 |

### CH02_020

| Sfx | `SFX_SwordDraw_Light` |
| Music | `BGM_Mentor_Arrival` |
| Speaker | ??? |
| Portrait | `MENTOR_Cloak_Serious` |
| Text | 低头。 |
| Next | CH02_030 |

### CH02_030

| Sfx | `SFX_SwordSlash_Wind` |
| Speaker | 旁白 |
| Text | 银色剑光擦着我的头顶掠过。黑熊被逼退半步，利爪在地面划出深痕。 |
| Next | CH02_040 |

### CH02_040

| Speaker | 主角 |
| Portrait | `PROTAG_Isekai_Stunned` |
| Text | 你是…… |
| Next | CH02_050 |

### CH02_050

| Speaker | 魔剑士导师 |
| Portrait | `MENTOR_Cloak_Normal` |
| Text | 王国骑士团所属，魔剑士。你可以叫我导师。 |
| Next | CH02_060 |

### CH02_060

| Speaker | 主角 |
| Text | 你长得很像我认识的人。 |
| Next | CH02_070 |

### CH02_070

| Speaker | 魔剑士导师 |
| Portrait | `MENTOR_Cloak_Calm` |
| Text | 现在不是确认长相的时候。你想救被带走的少女，就先活下来。 |
| Next | CH02_080 |

### CH02_080

| Sfx | `SFX_MagicSword_Awaken` |
| Speaker | 旁白 |
| Text | 她把一把黑银色的剑递到我手中。剑柄冰冷，却像有心跳一样回应着我的掌心。 |
| SetFlags | `Flag_MagicSwordGiven` |
| Next | CH02_090 |

### CH02_090

| Speaker | 魔剑士导师 |
| Portrait | `MENTOR_Cloak_Serious` |
| Text | 这把魔剑只会回应合适的人。握住它，别松手。 |
| Next | CH02_100 |

### CH02_100

| Speaker | 主角 |
| Portrait | `PROTAG_Isekai_Nervous` |
| Text | 我不会用剑。 |
| Next | CH02_110 |

### CH02_110

| Speaker | 魔剑士导师 |
| Text | 你会。只是还没想起来。 |
| Next | CH02_120 |

### CH02_120

| Speaker | 主角 |
| Text | 什么叫还没想起来？ |
| Next | CH02_130 |

### CH02_130

| Speaker | 魔剑士导师 |
| Portrait | `MENTOR_Cloak_HiddenTender` |
| Text | 先别问。你以前紧张的时候，右手会先用力。现在也一样，放松肩膀。 |
| Next | CH02_140 |

### CH02_140

| Speaker | 主角 |
| Portrait | `PROTAG_Isekai_Suspicious` |
| Text | 你怎么知道我紧张的时候会…… |
| Next | CH02_150 |

### CH02_150

| Sfx | `SFX_BearCharge` |
| Speaker | 魔剑士导师 |
| Portrait | `MENTOR_Cloak_Command` |
| Text | 来了。挥剑。 |
| Next | `SideCombat:CH02_MAIN_BearAwakening:R0` |

## 10. SideCombat 教程台词

这些台词由战斗触发器调用，不作为普通 VN 连续播放。

| Trigger | Speaker | Text | SetFlags |
| --- | --- | --- | --- |
| `Tutorial_MoveStart` | 魔剑士导师 | 先动起来。别把自己钉在地上。 |  |
| `Tutorial_JumpProjectile` | 魔剑士导师 | 投石的轨迹很慢，一段跳就够躲。不要乱跳第二次，空中没有退路。 |  |
| `Tutorial_DodgeCharge` | 魔剑士导师 | 它低头蓄力时立刻闪避。读招，比硬抗有用。 |  |
| `Tutorial_BasicAttack` | 魔剑士导师 | 三段斩。前两段稳住，第三段把它逼退。 |  |
| `Tutorial_Launch` | 魔剑士导师 | 第二段后接上挑，把敌人打到空中。 |  |
| `Tutorial_AirChase` | 魔剑士导师 | 跳起来，按普攻。空中也能砍，别让它白白落地。 |  |
| `Tutorial_Fireball` | 魔剑士导师 | 距离不够时用火球补一下 hit。魔剑不只会砍。 |  |
| `Tutorial_BearMotherLowHP` | 主角 | 能行……我真的能砍中它！ |  |
| `Tutorial_BearMotherDefeated` | 系统 | 黑熊母熊被击败。 | `Flag_CH02_BearMotherDefeated` |
| `Tutorial_MaterialAbsorb` | 魔剑士导师 | 看见那些发光的碎片了吗？靠近魔剑，它会自己吸收。 |  |
| `Tutorial_BearHusbandAppears` | 主角 | 等一下，为什么还有一头更大的？ | `Flag_CH02_BearHusbandAppeared` |
| `Tutorial_BossIntro` | 魔剑士导师 | 那是它的伴侣。别发呆，真正的战斗从现在开始。 |  |
| `Tutorial_BossBreak` | 魔剑士导师 | 冲撞落空后就是破绽。上挑，跳起来接普攻。 |  |
| `Tutorial_BossShockwave` | 魔剑士导师 | 地面震波用跳跃躲。只有一段跳也足够。 |  |
| `Tutorial_BossFinalPhase` | 魔剑士导师 | 它急了。越是这样，越要等它露出破绽。 |  |

## 11. SCN_CH02_PostBossVN：第二章收束

### CH02_POST_000

| Background | `BG_Otherworld_Forest_AfterBattle` |
| Music | `BGM_AfterBattle_FirstVictory` |
| Speaker | 主角 |
| Portrait | `PROTAG_Isekai_Breathing` |
| Text | 我赢了？我真的赢了？ |
| SetFlags | `Flag_CH02_BossDefeated` |
| Next | CH02_POST_010 |

### CH02_POST_010

| Speaker | 魔剑士导师 |
| Portrait | `MENTOR_Cloak_Calm` |
| Text | 只是活下来了。别把第一场胜利误会成强大。 |
| Next | CH02_POST_020 |

### CH02_POST_020

| Speaker | 主角 |
| Portrait | `PROTAG_Isekai_TiredSmile` |
| Text | 这时候不能稍微夸我一句吗？ |
| Next | CH02_POST_030 |

### CH02_POST_030

| Speaker | 魔剑士导师 |
| Portrait | `MENTOR_Cloak_HiddenTender` |
| Text | ……挥得不坏。比你刚才逃命的样子强一点。 |
| Next | CH02_POST_040 |

### CH02_POST_040

| Speaker | 主角 |
| Text | 这个评价真是精确到让人高兴不起来。 |
| Next | CH02_POST_050 |

### CH02_POST_050

| Sfx | `SFX_MagicSword_Pulse` |
| Speaker | 旁白 |
| Text | 魔剑吸收了黑熊留下的魔核。剑身上浮现出细小的纹路，像一条尚未完全点亮的路。 |
| Next | CH02_POST_060 |

### CH02_POST_060

| Speaker | 魔剑士导师 |
| Text | 魔剑会吞噬适合它的材料，打开新的技艺。你想追上那个魔法师，就必须学会养它，也学会养你自己。 |
| Next | CH02_POST_070 |

### CH02_POST_070

| Speaker | 主角 |
| Portrait | `PROTAG_Isekai_Determined` |
| Text | 我会变强。然后把青梅带回来。 |
| Next | CH02_POST_080 |

### CH02_POST_080

| Speaker | 魔剑士导师 |
| Portrait | `MENTOR_Cloak_Serious` |
| Text | 那就先找个能整理战利品的地方。你现在缺的不是决心，是基础。 |
| SetFlags | `Flag_HubUnlocked`，`Flag_CH02_Mat_BeastPathUnlocked` |
| Next | `SCN_Hub_Prototype` |

## 12. SCN_Hub_Prototype：据点首次进入

### HUB_INTRO_000

| Background | `BG_Hub_ForestCamp_Night` |
| Music | `BGM_Hub_FirstCamp` |
| Speaker | 系统 |
| Text | 据点 UI 已解锁。 |
| Next | HUB_INTRO_010 |

### HUB_INTRO_010

| Speaker | 魔剑士导师 |
| Portrait | `MENTOR_Cloak_Normal` |
| Text | 从现在开始，每次战斗后都要整理三件事：魔剑、装备、路线。 |
| Next | HUB_INTRO_020 |

### HUB_INTRO_020

| Speaker | 魔剑士导师 |
| Text | 魔剑需要材料，装备需要强化，你需要知道下一步往哪里走。 |
| Next | HUB_INTRO_030 |

### HUB_INTRO_030

| Speaker | 系统 |
| Text | 已解锁：魔剑技能树、装备强化、黑林兽道、继续剧情。 |
| Next | `HubAction:OpenHome` |

## 13. SCN_CH03_EntryVN：第三章入口预告

### CH03_ENTRY_000

| Background | `BG_BorderVillage_DistantSmoke` |
| Music | `BGM_NewChapter_Tension` |
| Speaker | 旁白 |
| Text | 离开黑林后，远处的村庄升起了灰色的烟。钟声断断续续，像有人在求救。 |
| SetFlags | `Flag_CH03_EntryUnlocked` |
| Next | CH03_ENTRY_010 |

### CH03_ENTRY_010

| Speaker | 主角 |
| Portrait | `PROTAG_Isekai_Alert` |
| Text | 那边有人。 |
| Next | CH03_ENTRY_020 |

### CH03_ENTRY_020

| Speaker | 魔剑士导师 |
| Portrait | `MENTOR_Cloak_Serious` |
| Text | 边境村。魔物袭击后，最先倒下的通常不是墙，而是人心。 |
| Next | CH03_ENTRY_030 |

### CH03_ENTRY_030

| Speaker | 主角 |
| Text | 你去过？ |
| Next | CH03_ENTRY_040 |

### CH03_ENTRY_040

| Speaker | 魔剑士导师 |
| Portrait | `MENTOR_Cloak_Calm` |
| Text | 骑士团的地图上有。走吧。你要找的人也许不在那里，但你要成为能把人带回来的人。 |
| Next | CH03_ENTRY_050 |

### CH03_ENTRY_050

| Speaker | 系统 |
| Text | 竖切版本结束。下一章将遇到第一位正式支援队友：白魔法队友。 |
| Next | `EndSandbox` |

## 14. 选项与变量

竖切阶段只保留轻量选项，不做复杂分支。

| Variable | 来源 | 用途 |
| --- | --- | --- |
| `Var_PrologueTone` | 序章选项 | 影响少量回忆台词 |
| `Var_CH01_PlayerReaction` | 假玩法后可扩展选项 | 影响主角吐槽口吻 |
| `Var_TutorialDeathCount` | 教程失败次数 | 导师提示是否更直接 |

后续扩展：

- 第三章开始正式启用好感选项。
- 第二章导师相关选项不显示青梅好感，避免提前暴露身份。

## 15. 失败与重试文本

### 第二章教程失败

| 条件 | 文本 |
| --- | --- |
| 第一次失败 | 魔剑士导师：别急。你刚拿到剑，先看敌人的动作。 |
| 第二次失败 | 魔剑士导师：冲撞前它会压低身体。看到那个动作就闪避。 |
| 第三次及以后 | 魔剑士导师：我会替你挡下一次致命攻击。你只要记住，上挑后跳起来按普攻。 |

### 黑熊丈夫失败

| 条件 | 文本 |
| --- | --- |
| 普通失败 | 魔剑士导师：你不是输给力量，是输给慌张。再来。 |
| 被震波击败 | 魔剑士导师：震波贴地扩散，跳起来。只有一段跳也够。 |
| 被冲撞击败 | 魔剑士导师：别站在它正面。等它撞空，再反击。 |

## 16. 导入检查

导入引擎前检查：

- 所有 `SceneId` 都存在。
- 所有 `CharacterId` 都存在。
- 第一部终章前不暴露导师真实身份。
- 选项写入变量但不阻塞主线。
- 第一章假玩法结束后必须写入 `Flag_CH01_FakeGameplayComplete`。
- 第二章 Boss 击败后必须写入 `Flag_CH02_BossDefeated`。
- 据点解锁时必须同时解锁黑林兽道。
- 竖切末尾必须能从据点跳到第三章入口。
