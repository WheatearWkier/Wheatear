# 资源数据库与 UI 模板系统

更新日期：2026-08-17

本文记录 Wheatear 当前资源管理和 UI 模板的正式方案。目标是让资源替换、图集切片、打包依赖、Prefab 和 UI 模板都走同一套工程化流程，避免“每个资源旁边一个元数据文件”和“代码里硬编码 UI 页面”造成的维护负担。

## 1. 设计结论

Wheatear 采用中央资源数据库作为唯一真相源：

```text
<项目根>/assets/.wheatear/asset_registry.yaml
```

（默认项目即 `Projects/WheatearDemo/assets/.wheatear/asset_registry.yaml`。
注意它不是引擎目录 `WheatearEditor/assets` 下的文件。）

它记录：

- 资源 UUID。
- 项目相对路径。
- 资源类型。
- 展示名。
- 文件大小和最后写入时间。
- 导入设置，例如纹理过滤、PPU、图集行列、音频用途和默认音量。
- 正向引用和反向引用。
- UI 模板描述，例如模板类别、模板类型和说明。

不再使用每个资源旁边的 `.wtmeta` sidecar 作为生产流程的一部分。旧 `.wtmeta` 文件只作为需要忽略的历史文件类型存在，不进入编辑器热路径，也不进入打包产物。

## 2. 为什么不用每资源 .wtmeta

最初尝试过给每个资源生成 `.wtmeta`。这个设计对大项目不合适：

- 刷新资源时要遍历和读取大量小文件，Windows 下会明显拖慢编辑器。
- 资源目录会被元数据文件污染，策划和美术替换素材时更容易误删或误提交。
- 中央 Registry 和 sidecar 同时存在时，会出现两个数据源谁覆盖谁的问题。
- 打包器还要额外过滤这些编辑器文件，规则复杂度上升。

现在的方案是：编辑器启动只读取中央 Registry；只有手动 `Rescan Asset Registry` 时才扫描文件系统。扫描时会根据路径、文件大小和最后写入时间复用已有引用结果，避免每次都重新解析所有文本资源。

## 3. 文件类型规则

| 类型 | 后缀 | 用途 | 是否打包 |
| --- | --- | --- | --- |
| 场景 | `.wt` | 运行时场景 | 是 |
| Prefab | `.wtprefab` | 多实体可复用模板 | 视依赖决定 |
| 材质 | `.wtmaterial` | 运行时材质 | 是 |
| 事件脚本 | `.wts` | 原生事件脚本 | 是 |
| UI 模板 | `.wtuit` | 编辑器 UI 模板描述 | 否 |
| 资源数据库 | `assets/.wheatear/asset_registry.yaml` | 编辑器资源索引 | 否 |
| 旧元数据 | `.wtmeta` | 历史 sidecar，当前不使用 | 否 |

`.wtuit` 是编辑器资产，它用于告诉编辑器要创建哪一种 UI 模板。真正实例化后，场景里保存的是普通 ECS 实体和 UI 组件，不依赖 `.wtuit` 在运行时存在。

## 4. 资源刷新流程

命令行刷新：

```powershell
WheatearEditor.exe --refresh-assets
```

编辑器内刷新：

```text
Content Browser -> Rescan Asset Registry
```

刷新流程：

1. 读取已有 `asset_registry.yaml`。
2. 扫描当前项目根的 `assets/` 下的真实资源文件（默认项目是 `Projects/WheatearDemo/assets`）。
3. 跳过 `.wheatear`、`cache`、`saves`、`.wtmeta`。
4. 通过路径复用旧 UUID 和导入设置。
5. 如果文件大小和最后写入时间没有变化，复用旧引用关系。
6. 如果资源变化，重新解析 `.wt`、`.wtprefab`、`.wtmaterial`、`.vn`、`.wts`、`.yaml`、`.json` 等文本资源里的引用。
7. 重建 `ReferencedBy` 反向引用。
8. 写回中央 Registry。

当前竖切工程资源量下，完整刷新约为 0.5 秒级。

## 5. UI 模板系统

UI 模板由两层组成：

- `.wtuit`：编辑器可见的模板描述资产。
- `UITemplateFactory`：真正创建实体、组件和父子关系的工厂。

内置模板由 `UITemplateFactory` 在刷新/打包时写入**项目根**的
`assets/ui_templates`（默认项目即 `Projects/WheatearDemo/assets/ui_templates`）。
项目创建/同步时，`WheatearEditor/ContentTemplates` 会把字体、WAO action、
Progression 内容和输入绑定模板复制到当前项目；`WheatearEditor/assets`
本身只保留引擎内置 shader。

当前模板包括：

- `titled_scroll_text.wtuit`
- `paged_grid.wtuit`
- `paged_inventory_grid.wtuit`
- `skill_button.wtuit`
- `equipment_slot.wtuit`
- `tooltip.wtuit`
- `save_slot.wtuit`
- `skill_tree_node.wtuit`
- `combat_skill_slot.wtuit`

模板创建入口：

- 在 Hierarchy 中选中 `Canvas` 或 Canvas 下的 UI 控件，右键 `UI / Templates`。
- 从 Content Browser 把 `.wtuit` 拖到 Viewport。

创建规则：

- UI 模板必须挂在 Canvas 或 Canvas 子控件下。
- 模板创建出来的是普通实体，可继续在 Inspector、Viewport 和 UI Canvas Editor 里修改。
- 创建操作进入撤销栈，可以 `Ctrl+Z` 撤销。
- 以后新增模板时，优先新增 `.wtuit` 描述和 `UITemplateFactory` 分支，不要把模板逻辑写进具体游戏页面系统。

## 6. Prefab v2

Prefab 当前只保留新版格式：

```yaml
Prefab: Example
Version: 2
RootEntity: 123456
Entities:
  - Entity: 123456
```

规则：

- Prefab 可以保存一棵多实体层级，不再只是单实体。
- 实例化时会重新分配 UUID，并自动 remap UI 父子引用。
- `UIWidgetComponent.ParentEntity`、`UIPageItemComponent.PagerEntity` 和命令里的 `@UUID` 都会跟随 remap。
- 不保留旧单实体 Prefab 兼容路径。

## 7. 打包边界

运行时打包只关心游戏真正需要的资源。以下内容不会进入 `.wtpack`：

- `assets/.wheatear`
- `.wtuit`
- `.wtmeta`
- `assets/cache`
- `assets/saves`

这样打包产物不会混入编辑器数据库、UI 模板描述和历史元数据文件。

## 8. 后续扩展方向

短期可以继续补：

- Content Browser 资源类型图标和缩略图预览。
- Asset Registry 中的资源重命名/移动，并自动维护引用。
- 图集切片设置和 Animation Clip 绑定到 Registry。
- UI Template Library 面板，支持搜索、预览和一键实例化。
- Project Health 增加重复 UUID、丢失 Registry 记录、未登记资源和模板损坏检查。

长期目标是把资源数据库升级成 Wheatear 的项目资产底座：Prefab、动画、UI 模板、WAO Action、音频、字体、材质和图集都能在同一个 Registry 里被追踪、检查、替换和打包。
