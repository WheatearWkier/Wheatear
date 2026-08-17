#pragma once

#include "Editor/AssetDocument.h"

#include <yaml-cpp/yaml.h>

#include <exception>
#include <string>
#include <utility>

namespace Wheatear::EditorDocuments {

    class YamlAssetDocument
    {
    public:
        void SetSourcePath(std::string sourcePath)
        {
            if (m_TextDocument.GetSourcePath() == sourcePath)
                return;

            m_TextDocument.SetSourcePath(std::move(sourcePath));
            m_Root = YAML::Node(YAML::NodeType::Map);
            m_RawPreview.clear();
            m_Loaded = false;
            m_ParseValid = false;
            m_Dirty = false;
            m_Status.clear();
        }

        const std::string& GetSourcePath() const { return m_TextDocument.GetSourcePath(); }
        const std::filesystem::path& GetResolvedPath() const { return m_TextDocument.GetResolvedPath(); }

        void SetWriteDestination(DocumentWriteDestination destination)
        {
            m_TextDocument.SetWriteDestination(destination);
        }
        DocumentWriteDestination GetWriteDestination() const
        {
            return m_TextDocument.GetWriteDestination();
        }
        YAML::Node& Root() { return m_Root; }
        const YAML::Node& Root() const { return m_Root; }
        const std::string& GetRawPreview() const { return m_RawPreview; }
        const std::string& GetStatus() const { return m_Status; }
        bool IsLoaded() const { return m_Loaded; }
        bool IsParseValid() const { return m_ParseValid; }
        bool IsDirty() const { return m_Dirty; }

        bool Load()
        {
            const bool textLoaded = m_TextDocument.Load();
            m_Loaded = true;
            m_Dirty = false;
            m_ParseValid = false;
            m_RawPreview = m_TextDocument.GetText();

            if (m_TextDocument.GetSourcePath().empty())
            {
                m_Root = YAML::Node(YAML::NodeType::Map);
                m_ParseValid = true;
                m_Status = m_TextDocument.GetStatus();
                return false;
            }

            if (m_RawPreview.empty() && !textLoaded)
            {
                m_Root = YAML::Node(YAML::NodeType::Map);
                m_ParseValid = true;
                m_Status = m_TextDocument.GetStatus();
                return false;
            }

            try
            {
                m_Root = YAML::Load(m_RawPreview);
                if (!m_Root || m_Root.IsNull())
                    m_Root = YAML::Node(YAML::NodeType::Map);
                RefreshRawPreview();
                m_ParseValid = true;
                m_Status = "Loaded.";
                return true;
            }
            catch (const std::exception& e)
            {
                m_Root = YAML::Node(YAML::NodeType::Map);
                m_Status = std::string("Parse failed: ") + e.what();
                return false;
            }
        }

        void MarkDirty()
        {
            m_Dirty = true;
            m_Status = "Modified.";
        }

        void RefreshRawPreview()
        {
            YAML::Emitter out;
            out.SetIndent(2);
            out << m_Root;
            m_RawPreview = out.good() ? std::string(out.c_str()) : std::string{};
        }

        bool Save(const std::string& header = {})
        {
            if (!m_ParseValid)
            {
                m_Status = "Save blocked: YAML is invalid.";
                return false;
            }

            RefreshRawPreview();
            const std::string output = header.empty()
                ? m_RawPreview
                : header + m_RawPreview;

            m_TextDocument.SetText(output, true);
            const bool saved = m_TextDocument.Save();
            m_RawPreview = output;
            m_Dirty = !saved;
            m_Status = saved ? "Saved." : m_TextDocument.GetStatus();
            return saved;
        }

    private:
        TextAssetDocument m_TextDocument;
        YAML::Node m_Root = YAML::Node(YAML::NodeType::Map);
        std::string m_RawPreview;
        bool m_Loaded = false;
        bool m_ParseValid = false;
        bool m_Dirty = false;
        std::string m_Status;
    };

} // namespace Wheatear::EditorDocuments
