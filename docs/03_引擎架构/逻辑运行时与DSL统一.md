# 逻辑运行时与 DSL 统一

更新时间：2026-10-23

Wheatear 的玩法逻辑由多种轻量 DSL 和 C++ 模块共同驱动：`.wts` 事件脚本、`.vn` 视觉小说脚本、WAO 动作 YAML、`.wtb` 行为图，以及 C++ 玩法模块。过去每种格式各自实现条件解析、变量查询和命令校验，导致同一套语义维护多份。本页记录统一后的公共运行时契约。

## 三层职责

| 层 | 位置 | 职责 |
| --- | --- | --- |
| 表达式语言 | `Wheatear/Logic/LogicExpression.*` | 数值/布尔表达式、条件归一化、进度谓词解析、变量作用域 |
| 命令目录 | `Wheatear/Logic/LogicCommandRegistry.*` | 命令前缀、分类、参数提示和合法性校验 |
| 诊断汇聚 | `Wheatear/Logic/LogicDiagnostics.*` | 收集 DSL 加载/求值问题，供编辑器和日志共同消费 |

DSL 本身不消失：`.wts` 仍然描述事件流程，`.vn` 仍然描述对白与分支，WAO 仍然描述动作配方。统一的是它们底层的表达式、变量和命令约定，而不是把所有格式塞进一个运行时。

## 共享表达式语法

```
expr     := or
or       := and ('or' and)*
and      := cmp ('and' cmp)*
cmp      := add (('==' | '!=' | '<' | '<=' | '>' | '>=') add)*
add      := mul (('+' | '-') mul)*
mul      := unary (('*' | '/' | '%') unary)*
unary    := ('-' | 'not') unary | primary
primary  := number | identifier | '(' expr ')' | function '(' args ')'
function := min | max | clamp | abs | round | floor | ceil | if
```

标识符允许字母、数字、`_`、`.` 和 `:`，因此 `flag:CH01`、`material:iron`、`target.health` 可以共用同一解析器。语法错误会返回 `fallback`，不会抛出异常；保存/导入路径通过 `Validate` 提前拒绝坏表达式。

### 使用位置

- WAO 效果公式：`EvaluateEffectFormula` / `ValidateEffectFormula` 已改为 `Logic::Evaluate` / `Logic::Validate` 的薄封装。
- `.wts` 条件：`EventScriptSystem::EvaluateCondition` 先归一化，再走共享求值。
- `.vn` 条件：`EvaluateVNExpression` 使用局部变量作用域叠加进度谓词。
- 编辑器 Action Editor：保存校验直接调用共享 `Validate`。

### 历史条件兼容

`NormalizeCondition` 把旧写法转换为共享语法：

| 旧写法 | 归一化结果 |
| --- | --- |
| `always` | `1` |
| `never` | `0` |
| `flag CH01` | `flag:CH01` |
| `not flag CH01` | `not (flag:CH01)` |
| `material iron` | `material:iron > 0` |
| `material iron >= 3` | `material:iron >= 3` |
| `chapter >= 2` | 保持不变 |

支持的谓词包括 `flag`、`skill`、`dungeon`、`dungeon_unlocked`、`completed`、`dungeon_completed`、`last_dungeon`、`last_result`、`equipment`、`equipped`、`material`，以及无参数的 `chapter`。

## 变量作用域

`Logic::VariableScope` 提供统一的四层解析顺序：

```
local -> scene -> persistent -> gameplay predicate
```

- `local`：脚本/对话运行时变量，例如 `.vn` 的 `@set` 结果。
- `scene`：当前场景或关卡内的临时状态。
- `persistent`：存档、进度和跨场景数据。
- `gameplay`：`GameProgress` 的旗标、技能、副本、装备、材料等谓词。

`MakeContext` 捕获共享状态快照，返回的 `EvalContext` 生命周期独立于原 `VariableScope` 对象。VN 已使用该模型；Event Script 目前只读取 gameplay 层，后续加入局部变量时无需再写解析器。

## 命令目录

`Logic::CommandRegistry` 是命令前缀的单一目录，不替代 `CommandBus` 的分发，而是回答三个问题：

1. 这个前缀是否已注册？
2. 它属于哪个分类？
3. 参数长什么样？

内置条目包括 `quit`、`scene`、`newgame`、`loadgame`、`progression`、`anim`、`ui`、`event`，以及玩法前缀 `vn`、`gamesave`、`turn`、`tactic`、`side`。玩法模块通过 `CommandBus::RegisterGameplayCommandPrefix` 注册时，会同时写入目录。

`.wts` 加载时会调用 `CommandBus::ValidateCommand`，未知前缀写入日志和逻辑诊断，而不是等到运行时才静默失败。`CommandBus::DescribeCommandPrefixes` 可生成编辑器/工具用的目录文本。

## 诊断

`Logic::Diagnostics` 是线程安全的有界环形列表（最多 512 条，连续重复消息去重）。当前来源：

- `event-script`：非法条件、未知命令前缀（带文件和行号）。
- `visual-novel`：非法条件。
- `wao`：非法效果公式。

编辑器菜单 `Diagnostics -> Logic Diagnostics` 会显示严重级别、来源和消息，并支持过滤、分级别显示和清空。

## 热重载现状

| 资产 | 机制 |
| --- | --- |
| `.wts` | 按文件修改时间缓存，变化时重新解析并重新校验 |
| `.vn` | 文件时间热重载，保留变量与历史，按最近标签恢复播放位置 |
| WAO 动作 YAML | 原子重载，校验失败或结果为空时保留旧数据 |
| `.wtb` 行为图 | 源 YAML 在加载时编译为内存指令，重载走同一编译路径 |

`.wts` 编辑器支持 Ctrl+Z / Ctrl+Y 与工具栏撤销/重做；保存前会执行结构校验，重复事件、缺失 `endif`、非法条件和未知命令都会阻止保存。

## 内容校验

`ContentValidator` 把四类内容的检查收敛到一处：

| 内容 | 检查项 |
| --- | --- |
| `.wts` | 重复事件、`if/endif` 平衡、条件表达式、命令前缀、等待时间、无法识别的行 |
| `.vn` | 行号定位、重复标签、跳转/选项目标、条件表达式、`@set` 表达式、未知指令 |
| `.wtb` | 编译期节点、连线、类型、未知 primitive、入口节点 |
| 动作 YAML | 重复/空 id、未知效果类型、效果公式 |

三个入口共用同一结果：

```bat
:: 命令行（CI 或本地批量检查）
WheatearSandbox.exe --validate-content Projects/WheatearDemo/assets
```

- 编辑器 `Project Health -> Content Validation` 显示文件、行号、严重级别和消息。
- `--package` 在构建前执行校验，存在 Error 时直接拒绝打包。

## 编辑器工作流

- `.wts` / `.vn` / WAO 配方编辑器支持撤销/重做（`Ctrl+Z`、`Ctrl+Y`、`Ctrl+Shift+Z`）。
- 关闭窗口、切换脚本或切换配方时，如果有未保存修改，会弹出 Save / Discard / Cancel。
- `Project Health -> Content Validation` 与 `Diagnostics -> Logic Diagnostics` 的条目支持双击打开对应资源和行号。
- `.wts` Raw 模式使用内联彩色文本编辑器：行号、语法着色、错误行标记、原生撤销/重做和命令面板插入。
- `.vn` 的 Advanced Raw 使用同一套彩色编辑器；`Apply Raw to Timeline` 会把文本写回时间线行。
- WAO Effects 支持上移、下移、复制；每个效果带编辑器内部 `EditorId`，未知字段会跟随效果移动/复制，不会被错配。
- Behavior 图插入节点会自动接到原后继节点；断点跨实例保留；调试器支持 Run From Selected Node；Sandbox 使用内存中编译的程序，不修改全局 BehaviorDatabase。
- Behavior 静态分析会检查不可达节点、环、环内是否有 wait/event 挂起点、branch producer 顺序、重复 local 写入和缺失分支目标。
- 内容校验会跨文件检查重复 action id。

## WTS 事件栈

`.wts` 组件不再只有一个活动事件槽位，而是 `RuntimeFrames` 调用栈：

- `event:` / `event:call:` 把子事件压栈，父事件暂停并等待子事件结束（join 语义）。
- `event:return:<expr>` 结束当前子事件并把数值结果交给父事件；父事件可用 `event_result` 读取。
- `event:stop[:event_name]` 取消栈顶或指定事件，并让父事件恢复。
- 同一事件名已在栈中时拒绝递归调用；栈深上限 8，避免自触发死循环。
- 广播 `event:name` 只在实际包含该事件的组件上启动，不再对每个组件刷“event not found”。
- `scene:` / `newgame:` / `loadgame:` 成功后清空当前组件的栈。

## VN 存档 v2

`VNSTATE 2` 额外记录脚本 FNV 哈希、最近标签和标签偏移：

- 脚本未修改时仍按原始行号恢复。
- 脚本修改后，优先按“标签 + 偏移”恢复，避免插入/删除行导致旧存档跳错位置。
- 旧 `VNSTATE 1` 存档仍可读取。
- 所有选项都被条件过滤时，系统提示并跳过该选项节点，不再软锁。

## 验证

```bat
WheatearSandbox.exe --test-logic
WheatearSandbox.exe --test-wao
WheatearSandbox.exe --validate-content Projects/WheatearDemo/assets
WheatearEditor.exe --health --project Projects/WheatearDemo
```

`--test-logic` 覆盖：归一化、运算符优先级、函数、布尔表达式、错误输入拒绝、作用域遮蔽、WAO 公式、命令目录、`.wts` 结构校验、`.vn` 目标校验与选项条件、VN `@set` 表达式。

## 为什么暂不引入 Lua

当前 DSL 是数据驱动的内容格式，不是通用编程语言；替换为 Lua 会增加沙箱、绑定、调试和打包成本，却不会直接改善内容生产。统一表达式、变量、命令和诊断后，各 DSL 的重复已经收敛。若将来确实需要复杂脚本逃生口，再以可选 Lua 5.4 模块形式加入，并复用同一变量作用域和命令目录，而不是替换现有 DSL。
