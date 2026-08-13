#pragma once

#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

namespace Wheatear::EditorDocuments {

    class TextAssetDocument
    {
    public:
        void SetSourcePath(std::string sourcePath)
        {
            if (m_SourcePath == sourcePath)
                return;

            m_SourcePath = std::move(sourcePath);
            m_ResolvedPath.clear();
            m_Text.clear();
            m_Loaded = false;
            m_Dirty = false;
            m_Status.clear();
        }

        const std::string& GetSourcePath() const { return m_SourcePath; }
        const std::filesystem::path& GetResolvedPath() const { return m_ResolvedPath; }
        const std::string& GetText() const { return m_Text; }
        bool IsLoaded() const { return m_Loaded; }
        bool IsDirty() const { return m_Dirty; }
        const std::string& GetStatus() const { return m_Status; }

        std::filesystem::path ResolvePath() const
        {
            if (m_SourcePath.empty())
                return {};
            return AssetPath::Resolve(AssetAliasRegistry::Resolve(m_SourcePath));
        }

        void SetText(std::string text, bool dirty = true)
        {
            m_Text = std::move(text);
            m_Loaded = true;
            m_Dirty = dirty;
            if (dirty)
                m_Status = "Modified.";
        }

        void MarkDirty()
        {
            m_Loaded = true;
            m_Dirty = true;
            m_Status = "Modified.";
        }

        void ClearDirty()
        {
            m_Dirty = false;
        }

        bool Load(size_t maxBytes = 2 * 1024 * 1024)
        {
            m_ResolvedPath = ResolvePath();
            m_Text.clear();

            if (m_SourcePath.empty())
            {
                m_Loaded = true;
                m_Dirty = false;
                m_Status = "No asset path set.";
                return false;
            }

            if (!std::filesystem::exists(m_ResolvedPath))
            {
                m_Loaded = true;
                m_Dirty = false;
                m_Status = "File does not exist yet. Save will create it.";
                return false;
            }

            std::ifstream input(m_ResolvedPath, std::ios::binary);
            if (!input)
            {
                m_Loaded = true;
                m_Dirty = false;
                m_Status = "Failed to open file for reading.";
                return false;
            }

            bool truncated = false;
            bool readBySize = false;
            try
            {
                const std::uintmax_t fileSize = std::filesystem::file_size(m_ResolvedPath);
                if (maxBytes > 0 && fileSize > static_cast<std::uintmax_t>(maxBytes))
                {
                    m_Text.assign(maxBytes, '\0');
                    input.read(m_Text.data(), static_cast<std::streamsize>(maxBytes));
                    m_Text.resize(static_cast<size_t>(input.gcount()));
                    truncated = true;
                    readBySize = true;
                }
            }
            catch (...)
            {
            }

            if (!readBySize)
            {
                m_Text.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
                if (maxBytes > 0 && m_Text.size() > maxBytes)
                {
                    m_Text.resize(maxBytes);
                    truncated = true;
                }
            }

            m_Loaded = true;
            m_Dirty = false;
            m_Status = truncated
                ? "Loaded with truncation because the file is larger than the editor text limit."
                : "Loaded.";
            return true;
        }

        bool Save()
        {
            m_ResolvedPath = ResolvePath();

            if (m_SourcePath.empty())
            {
                m_Status = "Cannot save because the asset path is empty.";
                return false;
            }

            try
            {
                const std::filesystem::path parent = m_ResolvedPath.parent_path();
                if (!parent.empty())
                    std::filesystem::create_directories(parent);
            }
            catch (...)
            {
                m_Status = "Failed to create asset directory.";
                return false;
            }

            std::ofstream output(m_ResolvedPath, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                m_Status = "Failed to open file for writing.";
                return false;
            }

            if (!m_Text.empty())
                output.write(m_Text.data(), static_cast<std::streamsize>(m_Text.size()));

            if (!output.good())
            {
                m_Status = "Failed while writing file.";
                return false;
            }

            m_Loaded = true;
            m_Dirty = false;
            m_Status = "Saved.";
            return true;
        }

    private:
        std::string m_SourcePath;
        std::filesystem::path m_ResolvedPath;
        std::string m_Text;
        bool m_Loaded = false;
        bool m_Dirty = false;
        std::string m_Status;
    };

} // namespace Wheatear::EditorDocuments
