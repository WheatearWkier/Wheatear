#pragma once

#include "Editor/YamlAssetDocument.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear {

    class ProgressionContentEditorPanel
    {
    public:
        void Open(const std::string& manifestPath = {});
        void OnImGuiRender();

    private:
        struct AssetEntry
        {
            std::string Key;
            std::string Label;
            std::string Path;
        };

        void LoadManifest();
        void SaveManifest();
        void RefreshEntriesFromManifest();
        void SelectEntry(const std::string& key);
        void LoadSelectedAsset();
        void SaveSelectedAsset();

        void DrawToolbar();
        void DrawFileList();
        void DrawSelectedAsset();
        void DrawManifestTab();
        void DrawSkillTreeTab();
        void DrawContentTab();
        void DrawValidationTab();
        void DrawRawPreview(const std::string& text, const char* id);

        bool DrawSequenceContentEditor(const char* rootKey);
        bool DrawDefaultsEditor(YAML::Node defaults);
        bool DrawUpgradesEditor(YAML::Node upgrades);
        bool DrawYamlNode(YAML::Node node, const std::string& path, int depth = 0);
        bool DrawYamlMap(YAML::Node node, const std::string& path, int depth);
        bool DrawYamlSequence(YAML::Node node, const std::string& path, int depth);
        bool DrawYamlScalar(YAML::Node node, const std::string& path);
        bool DrawYamlAddControls(YAML::Node node, const std::string& path, bool sequence);

    private:
        bool m_Open = false;
        bool m_ManifestLoaded = false;
        bool m_SelectedAssetLoaded = false;
        std::string m_ManifestPath = "assets/gameplay/progression/progression_content.yaml";
        std::string m_SelectedKey;
        std::string m_SelectedPath;
        std::string m_SelectedSkillNodeId;
        std::string m_SelectedContentRecordId;
        std::string m_Status;
        std::vector<AssetEntry> m_Entries;
        EditorDocuments::YamlAssetDocument m_ManifestDocument;
        EditorDocuments::YamlAssetDocument m_SelectedDocument;
        std::unordered_map<std::string, std::string> m_NewMapKeys;
        std::unordered_map<std::string, std::string> m_NewScalarValues;
    };

} // namespace Wheatear
