# 战斗调参与 HUD

## Side Combat 调参

打开：玩法 → Side Combat Tuning Editor（或选中 Side Combat Level 实体 → 组件按钮）。

页签说明：

- **Feel 手感**：移动/跳跃（速度、跳跃次数、重力、土狼时间）、冲刺（冷却/耗蓝/无敌）、空中连段、挑空/滞空、断连限
- **Rules 规则**：玩家数值（加速度/摩擦/前后摇）、伤害与连段（掉连时间/防御）、反馈（顿帧/音效）、Boss 保护与敌人/Boss 全套数值、拾取与场景视觉
- **Attacks 攻击**：Attack 下拉选攻击 → 命中框/运动、伤害与帧数据（Startup/Recovery/Cancel）、VFX/SFX（图集帧模式/音效/顿帧/震屏）
- **Animations 动画**：Player/Grunt/Boss 三组；输入框写新 id → **Add**；配置图集（Pattern/Atlas/行列/帧率/缩放）
- **Skills 技能**：技能下拉 → 显示名/输入/连段角色/攻击 id/解锁章节
- **Progression 成长**：默认配置、解锁技能、HUD 显示开关
- **Advanced Raw**：生成 YAML 预览

保存写回 YAML；解析失败会强制进 Raw 修复。

## Side Combat HUD 预设

打开：玩法 → Side Combat HUD Preset Editor。

- **Bindings**：绑定场景实体名（玩家/血条/连段/技能栏…）
- **Layout**：布置各 HUD 矩形（0~1 归一化坐标）
- **Slots**：技能槽/物品槽（按键、命令、图标、提示）
- **Text**：全部战斗文案
- 组件上「Apply HUD Preset」一键应用

## 战斗中的存档规则

战斗是否允许保存/读取不写在 Side Combat、Turn Combat 或 Tactical Combat 组件里。打开场景属性顶部的 **存档策略**：

- 战斗场景通常关闭 Can Save / Can Load。
- 战斗结束后的据点、整备、养成、选择关卡场景再开启。
- 如果某个暂停菜单、营火、检查点需要临时开放，进入时执行 `gamesave:push_policy:save=1:load=1`，退出时执行 `gamesave:pop_policy`。
- 这样同一套全局存档槽可以服务 VN、横版、回合制、战旗等玩法，而不会把存档逻辑塞进某一种玩法模块。
