#include "wepch.h"
#include "DataFileEditorPanel.h"

#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Editor/YamlTreeEditor.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/Log.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace Wheatear {

    namespace {

        static bool s_HasPendingOpen = false;
        static std::string s_PendingOpenPath;

        static bool IsYamlLikeExtension(const std::string& ext)
        {
            return ext == ".yaml" || ext == ".yml"
                || ext == ".wtanim" || ext == ".wtsheet"
                || ext == ".wtprefab" || ext == ".wtuit"
                || ext == ".wt";
        }

        static bool ReadFileText(const std::filesystem::path& path, std::string& outText)
        {
            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
                return false;

            std::stringstream buffer;
            buffer << input.rdbuf();
            outText = buffer.str();
            return true;
        }

    } // namespace

    namespace DataFileEditorRequests {

        void RequestOpen(const std::string& sourcePath)
        {
            s_PendingOpenPath = sourcePath;
            s_HasPendingOpen = true;
        }

        bool ConsumeOpenRequest(std::string& sourcePath)
        {
            if (!s_HasPendingOpen)
                return false;

            sourcePath = s_PendingOpenPath;
            s_PendingOpenPath.clear();
            s_HasPendingOpen = false;
            return true;
        }

    } // namespace DataFileEditorRequests

    void DataFileEditorPanel::Open(const std::string& sourcePath)
    {
        m_Open = true;
        if (!sourcePath.empty() && sourcePath != m_Path)
        {
            m_Path = sourcePath;
            m_Loaded = false;
            m_Dirty = false;
            m_Status.clear();
        }
    }

    void DataFileEditorPanel::Load()
    {
        m_ResolvedPath = AssetPath::Resolve(m_Path);
        m_RawText.clear();
        m_UndoStack.clear();
        m_LastSerialized.clear();
        m_NewScalarValues.clear();
        m_NewMapKeys.clear();

        if (!ReadFileText(m_ResolvedPath, m_RawText))
        {
            m_Status = "Failed to read file: " + m_ResolvedPath.string();
            m_ParseValid = false;
            m_Loaded = true;
            return;
        }

        const std::string ext = m_ResolvedPath.extension().string();
        m_SupportsTree = IsYamlLikeExtension(ext);

        if (m_SupportsTree)
        {
            try
            {
                m_YamlRoot = YAML::Load(m_RawText);
                m_ParseValid = true;
                m_LastSerialized = SerializeTree();
            }
            catch (const YAML::Exception& e)
            {
                m_YamlRoot = YAML::Node();
                m_ParseValid = false;
                m_Status = "YAML parse failed: " + std::string(e.what());
            }
        }
        else
        {
            // JSON parses with yaml-cpp, but re-serializing would rewrite it
            // as YAML; keep structured editing to .yaml files only.
            m_ParseValid = true;
            m_Status.clear();
        }

        m_Loaded = true;
        if (m_ParseValid)
            m_Status = "Loaded " + m_Path;
    }

    std::string DataFileEditorPanel::SerializeTree() const
    {
        if (!m_YamlRoot || !m_YamlRoot.IsDefined())
            return {};

        YAML::Emitter out;
        out << m_YamlRoot;
        return out.good() ? std::string(out.c_str()) : std::string{};
    }

    void DataFileEditorPanel::ValidateRaw()
    {
        try
        {
            YAML::Load(m_RawText);
            m_ParseValid = true;
            m_Status = "Parse OK.";
        }
        catch (const YAML::Exception& e)
        {
            m_ParseValid = false;
            m_Status = "Parse failed: " + std::string(e.what());
        }
    }

    void DataFileEditorPanel::Save()
    {
        // The view edited last wins: tree edits serialize the structured node,
        // raw edits write the text as-is.
        std::string output = m_RawText;
        if (m_SupportsTree && m_TreeEdited && m_YamlRoot && m_YamlRoot.IsDefined())
        {
            const std::string serialized = SerializeTree();
            if (!serialized.empty())
                output = serialized;
        }

        try
        {
            YAML::Load(output);
        }
        catch (const YAML::Exception& e)
        {
            m_ParseValid = false;
            m_Status = "Save blocked: parse failed - " + std::string(e.what());
            return;
        }
        m_ParseValid = true;

        std::ofstream out(m_ResolvedPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
        {
            m_Status = "Save failed: cannot open " + m_ResolvedPath.string();
            return;
        }
        out.write(output.data(), static_cast<std::streamsize>(output.size()));
        out.flush();

        m_RawText = output;
        if (m_SupportsTree)
        {
            m_YamlRoot = YAML::Load(output);
            m_LastSerialized = SerializeTree();
            m_TreeEdited = false;
        }
        m_Dirty = false;
        m_Status = "Saved " + m_Path;
    }

    void DataFileEditorPanel::ReloadFromDisk()
    {
        m_Loaded = false;
        Load();
        m_Dirty = false;
        m_Status = "Reloaded from disk.";
    }

    void DataFileEditorPanel::OnImGuiRender()
    {
        std::string requestedPath;
        if (DataFileEditorRequests::ConsumeOpenRequest(requestedPath))
            Open(requestedPath);

        if (!m_Open)
            return;

        if (!m_Loaded)
            Load();

        EditorFloatingWindow::Begin("Data File Editor", &m_Open, 0, { 980.0f, 700.0f });
        EditorWidgets::PanelHeader("Data File Editor",
            "Generic structured/raw editing for any data file. YAML files also get a tree view; JSON and other formats edit as raw text with validation.");
        EditorFloatingWindow::DrawToggleButton("Data File Editor");

        if (ImGui::Button(EditorLocale::Text("Save", "保存")))
            Save();
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("Reload from Disk", "从磁盘重新加载")))
            ReloadFromDisk();
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("Validate", "校验")))
            ValidateRaw();
        ImGui::SameLine();
        if (m_SupportsTree && !m_UndoStack.empty() && ImGui::Button(EditorLocale::Text("Undo Tree Edit", "撤销树编辑")))
        {
            m_YamlRoot = YAML::Load(m_UndoStack.back());
            m_UndoStack.pop_back();
            m_LastSerialized = SerializeTree();
            m_Dirty = true;
            m_Status = "Undid last tree edit.";
        }

        if (m_Dirty)
            EditorWidgets::InlineStatus("Unsaved changes.", EditorWidgets::StatusKind::Warning);
        if (!m_Status.empty())
            EditorWidgets::InlineStatus(m_Status,
                m_ParseValid ? EditorWidgets::StatusKind::Info : EditorWidgets::StatusKind::Error);

        ImGui::Separator();

        if (m_SupportsTree && m_ParseValid)
        {
            if (ImGui::BeginTabBar("##DataFileTabs"))
            {
                if (ImGui::BeginTabItem(EditorLocale::Text("Structure", "结构")))
                {
                    const bool changed = YamlTreeEditor::DrawYamlNode(
                        m_YamlRoot, "root", 0, m_NewScalarValues, m_NewMapKeys);

                    if (changed)
                    {
                        // Push the state captured before this frame's edits so
                        // Undo restores exactly the pre-change content.
                        if (!m_LastSerialized.empty()
                            && (m_UndoStack.empty() || m_UndoStack.back() != m_LastSerialized))
                        {
                            m_UndoStack.push_back(m_LastSerialized);
                            if (m_UndoStack.size() > 32)
                                m_UndoStack.erase(m_UndoStack.begin());
                        }
                        m_LastSerialized = SerializeTree();
                        m_TreeEdited = true;
                        m_Dirty = true;
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(EditorLocale::Text("Raw", "原始文本")))
                {
                    // Find & replace toolbar (global text replacement across
                    // the whole raw document; useful for renaming ids/flags).
                    ImGui::SetNextItemWidth(180.0f);
                    ImGui::InputText("##DataFileFind", m_FindText, sizeof(m_FindText),
                        ImGuiInputTextFlags_CharsNoBlank);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(180.0f);
                    ImGui::InputText("##DataFileReplace", m_ReplaceText, sizeof(m_ReplaceText));
                    ImGui::SameLine();
                    const bool hasFind = m_FindText[0] != '\0';
                    ImGui::BeginDisabled(!hasFind);
                    if (ImGui::Button(EditorLocale::Text("Replace All", "全部替换")))
                    {
                        const std::string find = m_FindText;
                        const std::string replace = m_ReplaceText;
                        size_t count = 0;
                        std::string result;
                        result.reserve(m_RawText.size());
                        size_t pos = 0;
                        while (true)
                        {
                            const size_t hit = m_RawText.find(find, pos);
                            if (hit == std::string::npos)
                            {
                                result.append(m_RawText, pos, std::string::npos);
                                break;
                            }
                            result.append(m_RawText, pos, hit - pos);
                            result.append(replace);
                            pos = hit + find.size();
                            ++count;
                        }
                        if (count > 0)
                        {
                            m_RawText = std::move(result);
                            m_TreeEdited = false;
                            m_Dirty = true;
                            m_ParseValid = false;
                        }
                        m_FindStatus = count > 0
                            ? "Replaced " + std::to_string(count) + " occurrence(s)."
                            : "No matches.";
                    }
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    if (!m_FindStatus.empty())
                        ImGui::TextColored(ImVec4(0.9f, 0.85f, 0.4f, 1.0f), "%s", m_FindStatus.c_str());

                    std::string edited = m_RawText;
                    const bool changed = EditorWidgets::InputMultilineString(
                        "##DataFileRaw",
                        edited,
                        ImVec2(-1.0f, -1.0f),
                        std::max<size_t>(m_RawText.size() + 1, 8192),
                        ImGuiInputTextFlags_AllowTabInput);
                    if (changed)
                    {
                        m_RawText = std::move(edited);
                        m_TreeEdited = false;
                        m_Dirty = true;
                        m_ParseValid = false; // re-validate on save/validate
                    }
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        else
        {
            std::string edited = m_RawText;
            const bool changed = EditorWidgets::InputMultilineString(
                "##DataFileRaw",
                edited,
                ImVec2(-1.0f, -1.0f),
                std::max<size_t>(m_RawText.size() + 1, 8192),
                ImGuiInputTextFlags_AllowTabInput);
            if (changed)
            {
                m_RawText = std::move(edited);
                m_TreeEdited = false;
                m_Dirty = true;
            }
        }

        EditorFloatingWindow::End();
    }

} // namespace Wheatear
