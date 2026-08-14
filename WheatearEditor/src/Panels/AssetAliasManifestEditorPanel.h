#pragma once

#include "Editor/EditorToolRegistry.h"
#include "Editor/YamlAssetDocument.h"

#include <string>

namespace Wheatear {

    class AssetAliasManifestEditorPanel
    {
    public:
        void Open(const EditorToolContext& context);
        void OnImGuiRender();

    private:
        void Load();
        void Save();
        void DrawAliasDetails();
        void DrawReferenceTools(const std::string& alias, const std::string& target);
        void ApplyReferenceReplacement(const std::string& searchText, const std::string& replacementText, bool dryRun);

    private:
        bool m_Open = false;
        std::string m_SourcePath = "assets/gameplay/content_manifest.yaml";
        std::string m_SelectedAlias;
        std::string m_NewAliasName;
        std::string m_NewAliasTarget = "assets/";
        std::string m_ReferenceSearchText;
        std::string m_ReferenceReplaceStatus;
        std::string m_Status;
        EditorDocuments::YamlAssetDocument m_Document;
    };

} // namespace Wheatear
