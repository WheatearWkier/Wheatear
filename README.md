# Wheatear Engine

Wheatear 是一个自制游戏引擎与编辑器工程，当前目标是支持视觉小说、据点流程、2D 俯视横板战斗、资源打包和可调参数驱动的竖切版本制作。

## 仓库结构

```text
Wheatear/              引擎运行时、渲染、场景、模块系统、资源与打包基础能力
WheatearEditor/        编辑器、示例工程资源、竖切 Demo、打包工具入口
WheatearSandbox/       独立运行时 Player / Sandbox
Wheatear-ScriptCore/   C# 脚本桥接层
docs/                  中文文档入口、策划案、系统设计、引擎架构和编辑器说明
scripts/               构建脚本
tools/                 资源生成与工程辅助工具
vendor/                工程工具，例如 premake
Wheatear/vendor/       引擎第三方依赖
```

## 文档入口

- [文档入口](docs/文档入口.md)：策划、系统设计、竖切、架构和编辑器手册的唯一入口。
- [商业游戏化路线图](docs/00_总览/商业游戏化路线图.md)：当前主路线，先补系统完整性，再统一美术资源，最后接入反馈和内容扩展。

## Git 管理规则

本仓库按大工程方式管理：

- 提交源码、项目文件、文档、必要运行资源和竖切 Demo 资源。
- 使用 Git LFS 管理图片、字体、音频、预编译库、可执行工具等二进制文件。
- 不提交 `.vs/`、`bin/`、`bin-int/`、`Builds/`、缓存、日志、调试符号、打包产物。
- 当前竖切不需要的示例素材统一归档到 `WheatearEditor/assets/archive/non_sandbox_demos/`，发行包默认不包含该目录。
- `Wheatear/vendor/spdlog` 使用 Git 子模块，clone 时需要拉取 submodule。

## 克隆

```powershell
git lfs install
git clone --recurse-submodules <repository-url>
cd Wheatear
git lfs pull
```

如果已经普通 clone 了仓库：

```powershell
git submodule update --init --recursive
git lfs pull
```

## 生成工程

```powershell
.\Win-GenProject.bat
```

该命令使用 `vendor/bin/premake/premake5.exe` 生成 Visual Studio 2022 工程。

## 构建

```powershell
.\scripts\Build-Windows.ps1 -Configuration Debug -Platform x64
```

也可以直接用 Visual Studio 2022 打开 `Wheatear.sln` 构建。

## 运行

构建成功后，常用入口为：

```text
bin/Debug-windows-x86_64/WheatearEditor/WheatearEditor.exe
bin/Debug-windows-x86_64/WheatearSandbox/WheatearSandbox.exe
```

竖切版本相关资源集中在：

```text
WheatearEditor/assets/vertical_slice/
WheatearEditor/assets/scenes/
WheatearEditor/assets/vertical_slice/data/side_combat_tuning.yaml
```

中文文档和运行时文本资产统一保存为 UTF-8。Windows PowerShell 5 读取中文文档时建议显式使用：

```powershell
Get-Content -Encoding UTF8 docs/文档入口.md
```

## GitHub 上传

初始化新仓库后，建议推送到一个全新的空 GitHub 仓库：

```powershell
git remote add origin https://github.com/<user>/<repo>.git
git push -u origin main
git lfs push --all origin main
```

如果远端仓库已经存在旧内容，优先新建空仓库；不要把旧 Hazel 历史混到 Wheatear 的新项目里。
