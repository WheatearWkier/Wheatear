# Mono / C# 脚本备份

本目录保存 Wheatear 移除 Mono 之前的全部 Mono/C# 相关代码，供日后参考或恢复。

## 备份内容

| 文件 | 说明 |
| --- | --- |
| `ScriptCore_Source/` | `Wheatear-ScriptCore` C# 托管 API 全部源码（25 个 .cs） |
| `Wheatear-ScriptCore.csproj` / `ScriptCore_premake5.lua` | C# 工程文件与 premake 配置 |
| `ScriptEngine.cpp/.h` | 引擎侧 Mono 宿主（初始化/程序集加载/字段反射/方法调用） |
| `ScriptGlue.cpp/.h` | 引擎 → C# 的 InternalCalls 绑定 |
| `ScriptSystem.cpp/.h` | 场景运行时脚本系统（OnCreate/OnUpdate/OnDestroy 桥接） |
| `ScriptDrawer.cpp/.h` | 编辑器 Inspector 的 Script 组件绘制 |
| `ScriptComponents.h.orig` | 原始组件头（含已删除的 `ScriptComponent` 结构体） |
| `Remove-DormantScriptComponents.ps1` | 清理场景/prefab/uit 中残留 ScriptComponent 块的脚本 |

## 移除范围（2026-08-15）

- 删除 `Wheatear/src/Wheatear/Scripting/ScriptEngine.*`、`ScriptGlue.*`
- 删除 `Wheatear/src/Wheatear/Systems/ScriptSystem.*`
- 删除 `WheatearEditor/src/Panels/SceneHierarchy/Drawers/ScriptDrawer.*`
- 删除 `Wheatear-ScriptCore/` 工程、`Wheatear/vendor/mono/`、`WheatearEditor/mono/`
- `ScriptComponents.h` 仅保留 `EventScriptComponent`（.wts 事件脚本，与 Mono 无关）
- `ApplicationSpecification.EnableScripting`、`PlayerConfig.EnableScripts`、
  `EngineInfo.ScriptCoreAssemblyPath`、Sandbox `--scripts/--no-scripts` 命令行
  以及 `AssetPath` 的 mono 路径分支一并移除

移除后引擎的脚本路径只有原生 `.wts` 事件脚本 + `.vn` 剧本 + 数据表。
