#pragma once

#include "Editor/EditorToolRegistry.h"
#include "Wheatear/Gameplay/Behavior/BehaviorTypes.h"
#include "Wheatear/Gameplay/Behavior/BehaviorVM.h"

#include <array>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <imgui/imgui.h>
#include <yaml-cpp/yaml.h>

namespace Wheatear {

    class Scene;

    namespace BehaviorGraphRequests {
        void RequestOpenBehavior(const std::string& sourcePath,
            const std::string& entryId = {});
        bool ConsumeOpenBehaviorRequest(std::string& sourcePath,
            std::string& entryId);
    }

    class BehaviorGraphPanel
    {
    public:
        ~BehaviorGraphPanel();
        void Open(const std::string& sourcePath,
            const std::string& entryId = {},
            Scene* debugScene = nullptr);
        void OnImGuiRender();

    private:
        void Load();
        bool Save();
        void DrawToolbar();
        void DrawGraph();
        void DrawDetails();
        void DrawRuntimeDebug();
        void DrawSandbox();
        void RunSandbox();
        void StopSandbox();
        void AddBehavior();
        void AddNode(const std::string& operation);
        void AddNodeAt(const std::string& operation, const ImVec2& logicalPosition);
        void AddCallNode(const std::string& primitiveId, const ImVec2& logicalPosition);
        void SelectNode(int index, bool additive);
        void ClearNodeSelection();
        void DuplicateSelectedNode();
        void CopySelectedNode();
        void PasteNode();
        void DeleteSelectedNode();
        void DeleteSelectedNodes();
        void AutoLayoutGraph();
        void ClearNodeLink(int sourceIndex, const std::string& outputSlot);
        void ClearNodeInput(int nodeIndex, const std::string& inputName);
        void ConnectDataPin(int sourceIndex, const std::string& outputSlot,
            int targetIndex, const std::string& inputName);
        void ValidateGraph();
        void FitGraph();
        void ChangeNodeOperation(YAML::Node node,
            const std::string& operation);
        void SetNodeLink(int sourceIndex,
            const std::string& outputSlot,
            const std::string& targetId);
        void CaptureUndoSnapshot();
        void Undo();
        void Redo();
        bool RestoreSnapshot(const std::string& snapshot,
            const char* status);
        void SetSelectedBehaviorFromEntry(const std::string& entryId);

        YAML::Node SelectedBehavior();
        YAML::Node SelectedNode();
        std::string SelectedBehaviorId() const;

    private:
        bool m_Open = false;
        bool m_Loaded = false;
        bool m_LoadFailed = false;
        bool m_Dirty = false;
        std::string m_SourcePath = "assets/gameplay/behaviors/demo_combat.wtb";
        std::string m_SelectedEntry;
        std::string m_Status;
        std::array<char, 512> m_SourcePathInput{};
        YAML::Node m_Root;
        int m_SelectedBehaviorIndex = 0;
        int m_SelectedNodeIndex = -1;
        ImVec2 m_GraphPan = { 18.0f, 18.0f };
        float m_Zoom = 1.0f;
        float m_DetailsWidth = 360.0f;
        Scene* m_DebugScene = nullptr;
        std::set<std::string> m_Breakpoints;
        Scene* m_SandboxScene = nullptr;
        std::shared_ptr<WAO::ActionRuntime> m_SandboxRuntime;
        WAO::BehaviorStartResult m_SandboxResult;
        uint64_t m_SandboxHandle = 0;
        bool m_SandboxRan = false;
        float m_SandboxTime = 0.0f;
        std::vector<std::string> m_UndoStack;
        std::vector<std::string> m_RedoStack;
        int m_DraggingNodeIndex = -1;
        ImVec2 m_DragStartMouse = {};
        ImVec2 m_DragStartPosition = {};
        bool m_DragUndoCaptured = false;
        std::vector<std::pair<int, ImVec2>> m_DragNodeStates;
        std::set<int> m_SelectedNodeIndices;

        bool m_BoxSelecting = false;
        bool m_BoxSelectAdditive = false;
        ImVec2 m_BoxSelectStart = {};
        ImVec2 m_BoxSelectEnd = {};
        int m_PendingLinkSourceIndex = -1;
        std::string m_PendingLinkOutputSlot;

        int m_PendingDataLinkSourceIndex = -1;
        std::string m_PendingDataLinkOutputSlot;
        WAO::BehaviorValueType m_PendingDataLinkType = WAO::BehaviorValueType::None;

        bool m_FitRequested = false;
        ImVec2 m_PalettePosition = { 42.0f, 32.0f };
        std::array<char, 256> m_NodeSearchInput{};
        std::array<char, 256> m_PaletteSearchInput{};
        std::string m_NodeClipboard;
        std::vector<std::string> m_Diagnostics;
        std::set<int> m_ErrorNodeIndices;
    };

} // namespace Wheatear
