#pragma once

namespace Wheatear {

    namespace EngineInfo
    {
        inline constexpr const char* Name = "Wheatear";
        inline constexpr const char* EditorName = "Wheatear Editor";
        inline constexpr const char* ScriptCoreAssemblyName = "Wheatear-ScriptCore";
        inline constexpr const char* ScriptCoreAssemblyPath = "assets/scripts/Wheatear-ScriptCore.dll";
        inline constexpr const char* DefaultStartupScene = "assets/scenes/VisualNovelMainMenu.wt";
    }

    namespace AssetFileType
    {
        inline constexpr const char* SceneExtension = ".wt";
        inline constexpr const char* SceneDialogFilter = "Wheatear Scene (*.wt)\0*.wt\0";
        inline constexpr const char* PrefabExtension = ".wtprefab";
        inline constexpr const char* UITemplateExtension = ".wtuit";
        inline constexpr const char* MetadataExtension = ".wtmeta";
        inline constexpr const char* MaterialExtension = ".wtmaterial";
    }

} // namespace Wheatear
