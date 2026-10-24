#include "wepch.h"
#include "BehaviorGraphPanel.h"

#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Wheatear/Gameplay/Behavior/BehaviorAssetLoader.h"
#include "Wheatear/Gameplay/Behavior/BehaviorBuiltinPrimitives.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Gameplay/Behavior/BehaviorCompiler.h"
#include "Wheatear/Gameplay/Behavior/BehaviorDatabase.h"
#include "Wheatear/Gameplay/Behavior/BehaviorPrimitiveRegistry.h"
#include "Wheatear/Gameplay/Behavior/BehaviorVM.h"
#include "Wheatear/Scene/Scene.h"

#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace Wheatear {

    namespace {

        bool s_HasPendingOpen = false;
        std::string s_PendingOpenPath;
        std::string s_PendingOpenEntry;

        constexpr float kSplitterWidth = 16.0f;
        constexpr float kMinimumGraphWidth = 300.0f;
        constexpr float kMinimumDetailsWidth = 280.0f;

        struct GraphNode
        {
            int Index = -1;
            ImVec2 Min;
            ImVec2 Max;
            ImVec2 LogicalPosition;
            std::string Id;
            std::string Title;
            std::string Body;
        };

        static std::string ReadString(const YAML::Node& node,
            const char* key,
            const std::string& fallback = {})
        {
            if (!node || !node[key])
                return fallback;
            return node[key].as<std::string>(fallback);
        }

        static float ReadFloat(const YAML::Node& node,
            const char* key,
            float fallback = 0.0f)
        {
            if (!node || !node[key])
                return fallback;
            return node[key].as<float>(fallback);
        }

        static ImVec2 ReadNodePosition(const YAML::Node& node,
            const ImVec2& fallback)
        {
            if (!node || !node["position"])
                return fallback;

            const YAML::Node position = node["position"];
            if (position.IsSequence() && position.size() >= 2)
            {
                return {
                    position[0].as<float>(fallback.x),
                    position[1].as<float>(fallback.y)
                };
            }

            if (position.IsMap())
            {
                return {
                    position["x"].as<float>(fallback.x),
                    position["y"].as<float>(fallback.y)
                };
            }
            return fallback;
        }

        static void WriteNodePosition(YAML::Node node,
            const ImVec2& position)
        {
            if (!node)
                return;

            node["position"]["x"] = position.x;
            node["position"]["y"] = position.y;
        }

        static std::string Operation(const YAML::Node& node)
        {
            if (!node || !node.IsMap())
                return {};
            if (node["call"]) return "call";
            if (node["branch"]) return "branch";
            if (node["jump"]) return "jump";
            if (node["wait"]) return "wait";
            if (node["waitEvent"]) return "waitEvent";
            if (node["set"]) return "set";
            if (node["end"] || ReadString(node, "op") == "end") return "end";
            return {};
        }

        static std::string OperationTitle(const std::string& operation)
        {
            if (operation == "call") return "Native Capability";
            if (operation == "branch") return "Branch";
            if (operation == "jump") return "Jump";
            if (operation == "wait") return "Wait";
            if (operation == "waitEvent") return "Wait Event";
            if (operation == "set") return "Set Local";
            if (operation == "end") return "End";
            return "Unknown";
        }

        static std::string NodeBody(const YAML::Node& node)
        {
            const std::string operation = Operation(node);
            if (operation == "call")
                return "call " + ReadString(node, "call");
            if (operation == "branch")
                return "if $" + ReadString(node, "branch");
            if (operation == "jump")
                return "jump " + ReadString(node, "jump");
            if (operation == "wait")
                return "wait " + std::to_string(ReadFloat(node, "wait"));
            if (operation == "waitEvent")
                return "event " + ReadString(node, "waitEvent");
            if (operation == "set")
                return "set $" + ReadString(node, "set");
            if (operation == "end")
                return "terminal";
            return "Unsupported node";
        }

        static bool PointInRect(const ImVec2& point,
            const ImVec2& min,
            const ImVec2& max)
        {
            return point.x >= min.x && point.x <= max.x
                && point.y >= min.y && point.y <= max.y;
        }

        static bool RectIntersects(const ImVec2& aMin, const ImVec2& aMax,
            const ImVec2& bMin, const ImVec2& bMax)
        {
            return aMin.x <= bMax.x && aMax.x >= bMin.x
                && aMin.y <= bMax.y && aMax.y >= bMin.y;
        }

        static bool PointInCircle(const ImVec2& point,
            const ImVec2& center,
            float radius)
        {
            const float dx = point.x - center.x;
            const float dy = point.y - center.y;
            return dx * dx + dy * dy <= radius * radius;
        }

        static ImVec2 Add(const ImVec2& left, const ImVec2& right)
        {
            return { left.x + right.x, left.y + right.y };
        }

        static ImVec2 Scale(const ImVec2& value, float scale)
        {
            return { value.x * scale, value.y * scale };
        }

        static void DrawGrid(ImDrawList* drawList,
            const ImVec2& min,
            const ImVec2& max,
            const ImVec2& pan,
            float zoom)
        {
            if (!drawList)
                return;

            const float spacing = std::max(18.0f, 32.0f * zoom);
            const float originX = min.x + std::fmod(pan.x, spacing);
            const float originY = min.y + std::fmod(pan.y, spacing);
            for (float x = originX; x < max.x; x += spacing)
                drawList->AddLine({ x, min.y }, { x, max.y }, IM_COL32(42, 60, 64, 100));
            for (float y = originY; y < max.y; y += spacing)
                drawList->AddLine({ min.x, y }, { max.x, y }, IM_COL32(42, 60, 64, 100));
        }

        static void DrawConnection(ImDrawList* drawList,
            const ImVec2& from,
            const ImVec2& to,
            ImU32 color,
            float thickness = 2.0f)
        {
            const float bend = std::max(24.0f, std::abs(to.y - from.y) * 0.35f);
            drawList->AddBezierCubic(
                from,
                { from.x, from.y + bend },
                { to.x, to.y - bend },
                to,
                color,
                thickness,
                20);
        }

        static void CopyPathToInput(std::array<char, 512>& buffer,
            const std::string& value)
        {
            buffer.fill('\0');
            const size_t length = std::min(value.size(), buffer.size() - 1);
            if (length > 0)
                std::memcpy(buffer.data(), value.data(), length);
        }

        static std::string UniqueBehaviorId(const YAML::Node& behaviors,
            const std::string& base)
        {
            std::string candidate = base;
            int suffix = 2;
            auto exists = [&](const std::string& value)
            {
                for (const auto& behavior : behaviors)
                    if (ReadString(behavior, "id") == value)
                        return true;
                return false;
            };
            while (exists(candidate))
                candidate = base + "_" + std::to_string(suffix++);
            return candidate;
        }

        static std::string UniqueNodeId(const YAML::Node& nodes,
            const std::string& base)
        {
            std::string candidate = base;
            int suffix = 2;
            auto exists = [&](const std::string& value)
            {
                for (const auto& node : nodes)
                    if (ReadString(node, "id") == value)
                        return true;
                return false;
            };
            while (exists(candidate))
                candidate = base + "_" + std::to_string(suffix++);
            return candidate;
        }

        static int FindNodeIndex(const YAML::Node& nodes,
            const std::string& id)
        {
            for (size_t i = 0; i < nodes.size(); ++i)
            {
                if (ReadString(nodes[i], "id") == id)
                    return static_cast<int>(i);
            }
            return -1;
        }

        static std::string Target(const YAML::Node& node,
            bool alternate)
        {
            if (!node)
                return {};
            if (Operation(node) == "branch")
                return ReadString(node, alternate ? "false" : "true");
            if (Operation(node) == "waitEvent" && alternate)
                return ReadString(node, "onTimeout");
            if (Operation(node) == "waitEvent")
                return ReadString(node, "next");
            return ReadString(node, "next");
        }

        static std::string LinkTarget(const YAML::Node& node,
            const std::string& outputSlot)
        {
            if (!node)
                return {};
            return ReadString(node, outputSlot.c_str());
        }

        static std::vector<std::string> OutputSlots(const YAML::Node& node)
        {
            const std::string operation = Operation(node);
            if (operation == "branch")
                return { "true", "false" };
            if (operation == "waitEvent")
                return { "next", "onTimeout" };
            if (operation == "jump")
                return { "jump" };
            if (operation == "end")
                return {};
            return { "next" };
        }

        static ImVec2 InputPin(const GraphNode& node)
        {
            return {
                node.Min.x,
                (node.Min.y + node.Max.y) * 0.5f
            };
        }

        static ImVec2 OutputPin(const GraphNode& node,
            const YAML::Node& source,
            const std::string& outputSlot)
        {
            const std::string operation = Operation(source);
            float y = (node.Min.y + node.Max.y) * 0.5f;
            if (operation == "branch" || operation == "waitEvent")
            {
                if (outputSlot == "true" || outputSlot == "next")
                    y = node.Min.y + (node.Max.y - node.Min.y) * 0.32f;
                else
                    y = node.Min.y + (node.Max.y - node.Min.y) * 0.68f;
            }
            return { node.Max.x, y };
        }

        static ImU32 LinkColor(const std::string& outputSlot)
        {
            if (outputSlot == "false" || outputSlot == "onTimeout")
                return IM_COL32(226, 180, 76, 190);
            return IM_COL32(82, 190, 166, 190);
        }

        static ImU32 NodeColor(const std::string& operation)
        {
            if (operation == "call") return IM_COL32(52, 112, 150, 242);
            if (operation == "branch") return IM_COL32(142, 100, 44, 242);
            if (operation == "wait" || operation == "waitEvent") return IM_COL32(92, 72, 138, 242);
            if (operation == "end") return IM_COL32(54, 122, 88, 242);
            if (operation == "set" || operation == "jump") return IM_COL32(74, 94, 108, 242);
            return IM_COL32(84, 84, 84, 242);
        }

        static std::string LowerAscii(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return text;
        }

        static const WAO::BehaviorPrimitiveDefinition* FindPrimitive(
            const YAML::Node& node)
        {
            if (!node || !node["call"])
                return nullptr;
            return WAO::BehaviorPrimitiveRegistry::Find(
                ReadString(node, "call"));
        }

        static std::vector<WAO::BehaviorPrimitivePort> DataInputPorts(
            const YAML::Node& node)
        {
            if (const WAO::BehaviorPrimitiveDefinition* definition = FindPrimitive(node))
                return definition->Inputs;

            std::vector<WAO::BehaviorPrimitivePort> ports;
            if (node && node["inputs"] && node["inputs"].IsMap())
            {
                for (auto it = node["inputs"].begin(); it != node["inputs"].end(); ++it)
                {
                    if (it->first.IsScalar())
                    {
                        ports.push_back({
                            it->first.as<std::string>(),
                            WAO::BehaviorValueType::None,
                            false
                        });
                    }
                }
            }
            return ports;
        }

        static std::vector<WAO::BehaviorPrimitivePort> DataOutputPorts(
            const YAML::Node& node)
        {
            if (const WAO::BehaviorPrimitiveDefinition* definition = FindPrimitive(node))
                return definition->Outputs;

            std::vector<WAO::BehaviorPrimitivePort> ports;
            if (node && node["outputs"] && node["outputs"].IsMap())
            {
                for (auto it = node["outputs"].begin(); it != node["outputs"].end(); ++it)
                {
                    if (it->first.IsScalar())
                    {
                        ports.push_back({
                            it->first.as<std::string>(),
                            WAO::BehaviorValueType::None,
                            false
                        });
                    }
                }
            }
            return ports;
        }

        static const char* PortTypeName(WAO::BehaviorValueType type)
        {
            switch (type)
            {
            case WAO::BehaviorValueType::Bool: return "bool";
            case WAO::BehaviorValueType::Number: return "number";
            case WAO::BehaviorValueType::String: return "string";
            case WAO::BehaviorValueType::UUID: return "uuid";
            case WAO::BehaviorValueType::None:
            default: return "any";
            }
        }

        static ImU32 PortColor(WAO::BehaviorValueType type)
        {
            switch (type)
            {
            case WAO::BehaviorValueType::Bool: return IM_COL32(228, 168, 74, 230);
            case WAO::BehaviorValueType::Number: return IM_COL32(112, 196, 232, 230);
            case WAO::BehaviorValueType::String: return IM_COL32(196, 148, 232, 230);
            case WAO::BehaviorValueType::UUID: return IM_COL32(232, 120, 156, 230);
            case WAO::BehaviorValueType::None:
            default: return IM_COL32(180, 196, 190, 210);
            }
        }

        static bool IsLocalReference(const YAML::Node& value)
        {
            if (!value || !value.IsScalar())
                return false;
            const std::string text = value.as<std::string>("");
            return !text.empty() && text.front() == '$';
        }

        static std::string LocalReferenceName(const YAML::Node& value)
        {
            if (!IsLocalReference(value))
                return {};
            return value.as<std::string>("").substr(1);
        }

        static float NodeHeight(const YAML::Node& node, float zoom)
        {
            const size_t dataPins = std::max(
                DataInputPorts(node).size(),
                DataOutputPorts(node).size());
            return std::max(92.0f, 74.0f + static_cast<float>(dataPins) * 17.0f) * zoom;
        }

        static float DataPinY(const GraphNode& node, size_t index, size_t count)
        {
            const float top = node.Min.y + (node.Max.y - node.Min.y) * 0.55f;
            const float bottom = node.Max.y - 6.0f;
            if (count <= 1)
                return (top + bottom) * 0.5f;
            return top + (bottom - top)
                * (static_cast<float>(index) / static_cast<float>(count - 1));
        }

        static ImVec2 DataInputPin(const GraphNode& node, size_t index, size_t count)
        {
            return { node.Min.x, DataPinY(node, index, count) };
        }

        static ImVec2 DataOutputPin(const GraphNode& node, size_t index, size_t count)
        {
            return { node.Max.x, DataPinY(node, index, count) };
        }

        struct LocalProducer
        {
            int NodeIndex = -1;
            std::string NodeId;
            std::string OutputSlot;
        };

        static LocalProducer FindLocalProducer(const YAML::Node& nodes,
            const std::string& localName)
        {
            if (localName.empty() || !nodes || !nodes.IsSequence())
                return {};

            for (size_t i = 0; i < nodes.size(); ++i)
            {
                const YAML::Node node = nodes[i];
                if (!node || !node["outputs"] || !node["outputs"].IsMap())
                    continue;

                for (auto it = node["outputs"].begin(); it != node["outputs"].end(); ++it)
                {
                    if (it->second.IsScalar() &&
                        it->second.as<std::string>("") == localName)
                    {
                        return {
                            static_cast<int>(i),
                            ReadString(node, "id"),
                            it->first.as<std::string>()
                        };
                    }
                }
            }
            return {};
        }

        static bool NodeMatchesSearch(const YAML::Node& node, const std::string& query)
        {
            if (query.empty())
                return true;

            const std::string lowerQuery = LowerAscii(query);
            const std::string haystack = LowerAscii(
                ReadString(node, "id") + " " +
                ReadString(node, "call") + " " +
                OperationTitle(Operation(node)) + " " +
                NodeBody(node));
            return haystack.find(lowerQuery) != std::string::npos;
        }

        static std::string BehaviorValueText(const WAO::BehaviorValue& value)
        {
            switch (value.Type())
            {
            case WAO::BehaviorValueType::Bool:
                return value.AsBool() ? "true" : "false";
            case WAO::BehaviorValueType::Number:
            {
                std::ostringstream stream;
                stream << value.AsNumber();
                return stream.str();
            }
            case WAO::BehaviorValueType::UUID:
                return std::to_string(static_cast<uint64_t>(value.AsUUID()));
            case WAO::BehaviorValueType::String:
                return value.AsString();
            case WAO::BehaviorValueType::None:
            default:
                return "(none)";
            }
        }

        static const char* EffectTypeLabel(WAO::EffectType type)
        {
            switch (type)
            {
            case WAO::EffectType::Damage: return "Damage";
            case WAO::EffectType::Heal: return "Heal";
            case WAO::EffectType::ModifyAttribute: return "ModifyAttribute";
            case WAO::EffectType::AddState: return "AddState";
            case WAO::EffectType::RemoveState: return "RemoveState";
            case WAO::EffectType::StartCooldown: return "StartCooldown";
            case WAO::EffectType::ConsumeResource: return "ConsumeResource";
            case WAO::EffectType::Launch: return "Launch";
            case WAO::EffectType::HitStun: return "HitStun";
            case WAO::EffectType::EmitSignal: return "EmitSignal";
            case WAO::EffectType::None:
            default: return "None";
            }
        }

        static const char* BehaviorStatusText(
            WAO::BehaviorInstanceStatus status)
        {
            switch (status)
            {
            case WAO::BehaviorInstanceStatus::Running: return "Running";
            case WAO::BehaviorInstanceStatus::Waiting: return "Waiting";
            case WAO::BehaviorInstanceStatus::Completed: return "Completed";
            case WAO::BehaviorInstanceStatus::Cancelled: return "Cancelled";
            case WAO::BehaviorInstanceStatus::Failed: return "Failed";
            default: return "Unknown";
            }
        }

    } // namespace

    namespace BehaviorGraphRequests {

        void RequestOpenBehavior(const std::string& sourcePath,
            const std::string& entryId)
        {
            s_HasPendingOpen = true;
            s_PendingOpenPath = sourcePath;
            s_PendingOpenEntry = entryId;
        }

        bool ConsumeOpenBehaviorRequest(std::string& sourcePath,
            std::string& entryId)
        {
            if (!s_HasPendingOpen)
                return false;

            s_HasPendingOpen = false;
            sourcePath = std::move(s_PendingOpenPath);
            entryId = std::move(s_PendingOpenEntry);
            s_PendingOpenPath.clear();
            s_PendingOpenEntry.clear();
            return true;
        }

    } // namespace BehaviorGraphRequests

    BehaviorGraphPanel::~BehaviorGraphPanel()
    {
        StopSandbox();
    }

    void BehaviorGraphPanel::Open(const std::string& sourcePath,
        const std::string& entryId,
        Scene* debugScene)
    {
        const bool sourceChanged = !sourcePath.empty() && sourcePath != m_SourcePath;
        const bool entryChanged = !entryId.empty() && entryId != m_SelectedEntry;
        if (sourceChanged && m_Loaded && m_Dirty)
        {
            m_Open = true;
            m_Status = "Save or reload the current behavior graph before switching files.";
            return;
        }
        if (debugScene)
            m_DebugScene = debugScene;
        if (!sourcePath.empty())
            m_SourcePath = sourcePath;
        if (!entryId.empty())
            m_SelectedEntry = entryId;

        m_Open = true;
        if (sourceChanged || entryChanged)
            ClearNodeSelection();
        CopyPathToInput(m_SourcePathInput, m_SourcePath);
        if (sourceChanged || !m_Loaded)
        {
            m_LoadFailed = false;
            Load();
        }
    }

    void BehaviorGraphPanel::CaptureUndoSnapshot()
    {
        if (!m_Loaded || !m_Root)
            return;

        const std::string snapshot = YAML::Dump(m_Root);
        if (m_UndoStack.empty() || m_UndoStack.back() != snapshot)
            m_UndoStack.push_back(snapshot);
        if (m_UndoStack.size() > 128)
            m_UndoStack.erase(m_UndoStack.begin());
        m_RedoStack.clear();
    }

    bool BehaviorGraphPanel::RestoreSnapshot(const std::string& snapshot,
        const char* status)
    {
        if (snapshot.empty())
            return false;

        const std::string selectedBehaviorId = SelectedBehaviorId();
        const std::string selectedNodeId = ReadString(SelectedNode(), "id");
        try
        {
            YAML::Node restored = YAML::Load(snapshot);
            if (!restored || !restored.IsMap() ||
                !restored["behaviors"] || !restored["behaviors"].IsSequence())
            {
                m_Status = "Cannot restore invalid behavior snapshot.";
                return false;
            }

            m_Root = restored;
            m_Loaded = true;
            m_SelectedBehaviorIndex = 0;
            ClearNodeSelection();
            SetSelectedBehaviorFromEntry(selectedBehaviorId);

            YAML::Node behavior = SelectedBehavior();
            if (behavior && behavior["nodes"].IsSequence() && !selectedNodeId.empty())
            {
                m_SelectedNodeIndex = FindNodeIndex(
                    behavior["nodes"],
                    selectedNodeId);
                if (m_SelectedNodeIndex >= 0)
                    m_SelectedNodeIndices.insert(m_SelectedNodeIndex);
            }

            m_Dirty = true;
            m_PendingLinkSourceIndex = -1;
            m_PendingLinkOutputSlot.clear();
            m_PendingDataLinkSourceIndex = -1;
            m_PendingDataLinkOutputSlot.clear();
            m_PendingDataLinkType = WAO::BehaviorValueType::None;
            m_Diagnostics.clear();
            m_ErrorNodeIndices.clear();
            m_Status = status ? status : "Restored behavior graph.";
            return true;
        }
        catch (const YAML::Exception& exception)
        {
            m_Status = std::string("Cannot restore behavior snapshot: ") +
                exception.what();
            return false;
        }
    }

    void BehaviorGraphPanel::Undo()
    {
        if (m_UndoStack.empty())
            return;

        const std::string current = YAML::Dump(m_Root);
        const std::string snapshot = m_UndoStack.back();
        m_UndoStack.pop_back();
        m_RedoStack.push_back(current);
        if (!RestoreSnapshot(snapshot, "Undid behavior graph edit."))
        {
            m_UndoStack.push_back(snapshot);
            m_RedoStack.pop_back();
        }
    }

    void BehaviorGraphPanel::Redo()
    {
        if (m_RedoStack.empty())
            return;

        const std::string current = YAML::Dump(m_Root);
        const std::string snapshot = m_RedoStack.back();
        m_RedoStack.pop_back();
        m_UndoStack.push_back(current);
        if (!RestoreSnapshot(snapshot, "Redid behavior graph edit."))
        {
            m_UndoStack.pop_back();
            m_RedoStack.push_back(snapshot);
        }
    }

    void BehaviorGraphPanel::Load()
    {
        m_Breakpoints.clear();
        const std::filesystem::path resolved =
            EditorWidgets::ResolveProjectAsset(m_SourcePath);
        if (!std::filesystem::is_regular_file(resolved))
        {
            m_Root = {};
            m_Loaded = false;
            m_LoadFailed = true;
            m_Dirty = false;
            m_UndoStack.clear();
            m_RedoStack.clear();
            m_PendingLinkSourceIndex = -1;
            m_PendingLinkOutputSlot.clear();
            m_PendingDataLinkSourceIndex = -1;
            m_PendingDataLinkOutputSlot.clear();
            m_PendingDataLinkType = WAO::BehaviorValueType::None;
            m_Diagnostics.clear();
            m_ErrorNodeIndices.clear();
            m_Status = "Behavior file not found: " + m_SourcePath;
            return;
        }

        try
        {
            m_Root = YAML::LoadFile(resolved.string());
            m_Loaded = m_Root && m_Root.IsMap() &&
                m_Root["behaviors"] && m_Root["behaviors"].IsSequence();
            m_LoadFailed = !m_Loaded;
            m_Dirty = false;
            m_UndoStack.clear();
            m_RedoStack.clear();
            m_PendingLinkSourceIndex = -1;
            m_PendingLinkOutputSlot.clear();
            m_PendingDataLinkSourceIndex = -1;
            m_PendingDataLinkOutputSlot.clear();
            m_PendingDataLinkType = WAO::BehaviorValueType::None;
            m_Diagnostics.clear();
            m_ErrorNodeIndices.clear();
            m_SelectedBehaviorIndex = 0;
            ClearNodeSelection();
            SetSelectedBehaviorFromEntry(m_SelectedEntry);
            if (m_Loaded)
                ValidateGraph();
            if (!m_Loaded)
                m_Status = "Behavior file must contain a behaviors sequence.";
            else
                m_Status = "Loaded " + std::to_string(m_Root["behaviors"].size()) + " behavior entr" +
                    (m_Root["behaviors"].size() == 1 ? "y." : "ies.");
        }
        catch (const YAML::Exception& exception)
        {
            m_Root = {};
            m_Loaded = false;
            m_LoadFailed = true;
            m_Dirty = false;
            m_UndoStack.clear();
            m_RedoStack.clear();
            m_PendingLinkSourceIndex = -1;
            m_PendingLinkOutputSlot.clear();
            m_PendingDataLinkSourceIndex = -1;
            m_PendingDataLinkOutputSlot.clear();
            m_PendingDataLinkType = WAO::BehaviorValueType::None;
            m_Diagnostics.clear();
            m_ErrorNodeIndices.clear();
            m_Status = std::string("YAML error: ") + exception.what();
        }
    }

    bool BehaviorGraphPanel::Save()
    {
        if (!m_Loaded)
            return false;

        ValidateGraph();
        if (!m_Diagnostics.empty())
        {
            m_Status = "Validation failed: " + m_Diagnostics.front();
            return false;
        }

        const std::filesystem::path resolved =
            EditorWidgets::ResolveWritableProjectAsset(m_SourcePath);
        WAO::BehaviorProgram program;
        std::string error;
        if (!WAO::BehaviorCompiler::Compile(m_Root, resolved, &program, &error))
        {
            m_Status = "Compile failed: " + error;
            return false;
        }

        if (!EditorWidgets::WriteFileText(resolved, YAML::Dump(m_Root)))
        {
            m_Status = "Failed to write " + resolved.string();
            return false;
        }

        m_Dirty = false;
        m_Status = "Saved " + std::to_string(program.Entries.size()) + " behavior entr" +
            (program.Entries.size() == 1 ? "y." : "ies.");
        return true;
    }

    YAML::Node BehaviorGraphPanel::SelectedBehavior()
    {
        if (!m_Loaded || !m_Root["behaviors"].IsSequence() ||
            m_SelectedBehaviorIndex < 0 ||
            m_SelectedBehaviorIndex >= static_cast<int>(m_Root["behaviors"].size()))
        {
            return {};
        }
        return m_Root["behaviors"][static_cast<size_t>(m_SelectedBehaviorIndex)];
    }

    YAML::Node BehaviorGraphPanel::SelectedNode()
    {
        YAML::Node behavior = SelectedBehavior();
        if (!behavior || !behavior["nodes"].IsSequence() ||
            m_SelectedNodeIndex < 0 ||
            m_SelectedNodeIndex >= static_cast<int>(behavior["nodes"].size()))
        {
            return {};
        }
        return behavior["nodes"][static_cast<size_t>(m_SelectedNodeIndex)];
    }

    std::string BehaviorGraphPanel::SelectedBehaviorId() const
    {
        if (!m_Loaded || !m_Root["behaviors"].IsSequence() ||
            m_SelectedBehaviorIndex < 0 ||
            m_SelectedBehaviorIndex >= static_cast<int>(m_Root["behaviors"].size()))
        {
            return {};
        }
        return ReadString(m_Root["behaviors"][static_cast<size_t>(m_SelectedBehaviorIndex)], "id");
    }

    void BehaviorGraphPanel::SetSelectedBehaviorFromEntry(const std::string& entryId)
    {
        if (!m_Loaded)
            return;

        const YAML::Node behaviors = m_Root["behaviors"];
        if (!entryId.empty())
        {
            for (size_t i = 0; i < behaviors.size(); ++i)
            {
                if (ReadString(behaviors[i], "id") == entryId)
                {
                    m_SelectedBehaviorIndex = static_cast<int>(i);
                    return;
                }
            }
        }

        if (m_SelectedBehaviorIndex >= static_cast<int>(behaviors.size()))
            m_SelectedBehaviorIndex = behaviors.size() == 0 ? -1 : 0;
        if (m_SelectedBehaviorIndex >= 0)
            m_SelectedEntry = ReadString(behaviors[static_cast<size_t>(m_SelectedBehaviorIndex)], "id");
    }

    void BehaviorGraphPanel::DrawToolbar()
    {
        ImGui::SetNextItemWidth(340.0f);
        ImGui::InputText("Behavior File", m_SourcePathInput.data(), m_SourcePathInput.size());
        const bool pathCommitted = ImGui::IsItemDeactivatedAfterEdit();
        ImGui::SameLine();
        if (ImGui::Button("Load") || pathCommitted)
        {
            const std::string nextPath = m_SourcePathInput.data();
            if (nextPath != m_SourcePath && m_Dirty)
            {
                m_Status = "Save or reload the current behavior graph before switching files.";
                CopyPathToInput(m_SourcePathInput, m_SourcePath);
            }
            else
            {
                m_SourcePath = nextPath;
                m_LoadFailed = false;
                Load();
            }
        }

        const YAML::Node behaviors = m_Loaded ? m_Root["behaviors"] : YAML::Node{};
        std::string selected = SelectedBehaviorId();
        if (ImGui::BeginCombo("Entry", selected.empty() ? "(none)" : selected.c_str()))
        {
            if (behaviors && behaviors.IsSequence())
            {
                for (size_t i = 0; i < behaviors.size(); ++i)
                {
                    const std::string id = ReadString(behaviors[i], "id", "(unnamed)");
                    const bool current = static_cast<int>(i) == m_SelectedBehaviorIndex;
                    if (ImGui::Selectable(id.c_str(), current))
                    {
                        m_SelectedBehaviorIndex = static_cast<int>(i);
                        m_SelectedEntry = id;
                        ClearNodeSelection();
                    }
                    if (current)
                        ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("Add Entry"))
            AddBehavior();
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_Loaded || m_SelectedBehaviorIndex < 0);
        if (ImGui::SmallButton("Add Node"))
        {
            YAML::Node behavior = SelectedBehavior();
            YAML::Node nodes = behavior ? behavior["nodes"] : YAML::Node{};
            m_PalettePosition = {
                42.0f,
                32.0f + (nodes && nodes.IsSequence()
                    ? static_cast<float>(nodes.size()) * 126.0f
                    : 0.0f)
            };
            if (nodes && nodes.IsSequence()
                && m_SelectedNodeIndex >= 0
                && m_SelectedNodeIndex < static_cast<int>(nodes.size()))
            {
                const ImVec2 selectedPosition = ReadNodePosition(
                    nodes[static_cast<size_t>(m_SelectedNodeIndex)],
                    m_PalettePosition);
                m_PalettePosition = {
                    selectedPosition.x,
                    selectedPosition.y + 126.0f
                };
            }
            ImGui::OpenPopup("BehaviorNodePalette");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Save"))
            Save();
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::SmallButton("Reload"))
            Load();
        ImGui::SameLine();
        ImGui::BeginDisabled(m_UndoStack.empty());
        if (ImGui::SmallButton("Undo"))
            Undo();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(m_RedoStack.empty());
        if (ImGui::SmallButton("Redo"))
            Redo();
        ImGui::EndDisabled();

        ImGui::NewLine();
        if (ImGui::SmallButton("Validate"))
        {
            ValidateGraph();
            m_Status = m_Diagnostics.empty()
                ? "Graph validation passed."
                : "Graph validation found " +
                    std::to_string(m_Diagnostics.size()) + " issue(s).";
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Fit"))
            FitGraph();
        ImGui::SameLine();
        if (ImGui::SmallButton("Auto Layout"))
            AutoLayoutGraph();
        ImGui::SameLine();
        if (ImGui::SmallButton("Copy"))
            CopySelectedNode();
        ImGui::SameLine();
        if (ImGui::SmallButton("Paste"))
            PasteNode();
        ImGui::SameLine();
        if (ImGui::SmallButton("Duplicate"))
            DuplicateSelectedNode();
        ImGui::SameLine();
        if (ImGui::SmallButton("Center"))
            m_GraphPan = { 18.0f, 18.0f };
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderFloat("Zoom", &m_Zoom, 0.65f, 1.45f, "%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputTextWithHint("##BehaviorNodeSearch", "Search nodes...",
            m_NodeSearchInput.data(), m_NodeSearchInput.size());

        if (!m_Status.empty())
            ImGui::TextDisabled("%s", m_Status.c_str());
        if (!m_Diagnostics.empty())
        {
            ImGui::TextColored({ 1.0f, 0.45f, 0.35f, 1.0f }, "%s",
                m_Diagnostics.front().c_str());
        }

        if (ImGui::BeginPopup("BehaviorNodePalette"))
        {
            ImGui::TextUnformatted("Add Node");
            ImGui::SetNextItemWidth(300.0f);
            ImGui::InputTextWithHint("##BehaviorPaletteSearch", "Search primitives...",
                m_PaletteSearchInput.data(), m_PaletteSearchInput.size());
            const std::string query = LowerAscii(m_PaletteSearchInput.data());

            auto matches = [&query](const std::string& text)
            {
                return query.empty()
                    || LowerAscii(text).find(query) != std::string::npos;
            };

            for (const char* operation : { "branch", "jump", "wait", "waitEvent", "set", "end" })
            {
                const std::string label = std::string("Flow: ") +
                    OperationTitle(operation);
                if (!matches(label))
                    continue;
                if (ImGui::Selectable(label.c_str()))
                {
                    AddNodeAt(operation, m_PalettePosition);
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::Separator();
            for (const WAO::BehaviorPrimitiveDefinition& definition :
                WAO::BehaviorPrimitiveRegistry::All())
            {
                const std::string label = (definition.DisplayName.empty()
                    ? definition.Id
                    : definition.DisplayName + " [" + definition.Id + "]");
                if (!matches(label) && !matches(definition.Id))
                    continue;
                if (ImGui::Selectable(label.c_str()))
                {
                    AddCallNode(definition.Id, m_PalettePosition);
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Validate Graph"))
                ValidateGraph();
            if (ImGui::MenuItem("Fit Graph"))
                FitGraph();
            ImGui::EndPopup();
        }
    }

    void BehaviorGraphPanel::AddBehavior()
    {
        if (!m_Loaded)
            return;

        CaptureUndoSnapshot();
        YAML::Node behaviors = m_Root["behaviors"];
        if (!behaviors || !behaviors.IsSequence())
        {
            behaviors = YAML::Node(YAML::NodeType::Sequence);
            m_Root["behaviors"] = behaviors;
        }

        YAML::Node behavior(YAML::NodeType::Map);
        behavior["id"] = UniqueBehaviorId(behaviors, "new.behavior");
        behavior["version"] = 1;
        behavior["entry"] = "begin";
        YAML::Node nodes(YAML::NodeType::Sequence);
        YAML::Node end(YAML::NodeType::Map);
        end["id"] = "begin";
        end["end"] = true;
        nodes.push_back(end);
        behavior["nodes"] = nodes;
        behaviors.push_back(behavior);

        m_SelectedBehaviorIndex = static_cast<int>(behaviors.size()) - 1;
        m_SelectedEntry = ReadString(behavior, "id");
        ClearNodeSelection();
        m_SelectedNodeIndices.insert(0);
        m_SelectedNodeIndex = 0;
        m_Dirty = true;
        m_Status = "Added behavior entry.";
    }

    void BehaviorGraphPanel::ChangeNodeOperation(YAML::Node node,
        const std::string& operation)
    {
        if (!node)
            return;

        static const char* keys[] = {
            "call", "inputs", "outputs", "branch", "true", "false",
            "jump", "wait", "waitEvent", "timeout", "onTimeout",
            "set", "value", "next", "end", "op"
        };
        for (const char* key : keys)
            node.remove(key);

        if (operation == "call")
        {
            node["call"] = "flow.always_true";
            node["outputs"]["value"] = "value";
        }
        else if (operation == "branch")
        {
            node["branch"] = "value";
        }
        else if (operation == "jump")
        {
            node["jump"] = "";
        }
        else if (operation == "wait")
        {
            node["wait"] = 0.10f;
        }
        else if (operation == "waitEvent")
        {
            node["waitEvent"] = "combat.hit_received";
            node["timeout"] = 0.0f;
        }
        else if (operation == "set")
        {
            node["set"] = "value";
            node["value"] = true;
        }
        else if (operation == "end")
        {
            node["end"] = true;
        }
        m_Dirty = true;
    }

    void BehaviorGraphPanel::AddNode(const std::string& operation)
    {
        YAML::Node behavior = SelectedBehavior();
        YAML::Node nodes = behavior ? behavior["nodes"] : YAML::Node{};
        ImVec2 position(42.0f,
            32.0f + (nodes && nodes.IsSequence()
                ? static_cast<float>(nodes.size()) * 126.0f
                : 0.0f));

        if (nodes && nodes.IsSequence()
            && m_SelectedNodeIndex >= 0
            && m_SelectedNodeIndex < static_cast<int>(nodes.size()))
        {
            const ImVec2 selectedPosition = ReadNodePosition(
                nodes[static_cast<size_t>(m_SelectedNodeIndex)],
                position);
            position = { selectedPosition.x, selectedPosition.y + 126.0f };
        }

        AddNodeAt(operation, position);
    }

    void BehaviorGraphPanel::AddNodeAt(const std::string& operation,
        const ImVec2& logicalPosition)
    {
        YAML::Node behavior = SelectedBehavior();
        if (!behavior)
            return;

        CaptureUndoSnapshot();
        YAML::Node nodes = behavior["nodes"];
        if (!nodes || !nodes.IsSequence())
        {
            nodes = YAML::Node(YAML::NodeType::Sequence);
            behavior["nodes"] = nodes;
        }

        YAML::Node node(YAML::NodeType::Map);
        node["id"] = UniqueNodeId(nodes, "node");
        ChangeNodeOperation(node, operation);
        WriteNodePosition(node, logicalPosition);

        const int insertAt = m_SelectedNodeIndex >= 0
            ? std::min(m_SelectedNodeIndex + 1, static_cast<int>(nodes.size()))
            : static_cast<int>(nodes.size());
        const std::string newId = ReadString(node, "id");

        std::string previousTarget;
        if (insertAt > 0)
        {
            YAML::Node previous = nodes[static_cast<size_t>(insertAt - 1)];
            if (Operation(previous) != "end")
                previousTarget = Target(previous, false);
        }

        YAML::Node replacement(YAML::NodeType::Sequence);
        for (int i = 0; i < insertAt; ++i)
            replacement.push_back(nodes[static_cast<size_t>(i)]);
        replacement.push_back(node);
        for (size_t i = static_cast<size_t>(insertAt); i < nodes.size(); ++i)
            replacement.push_back(nodes[i]);
        behavior["nodes"] = replacement;

        std::string previousOperation;
        if (insertAt > 0)
        {
            YAML::Node previous = behavior["nodes"][static_cast<size_t>(insertAt - 1)];
            previousOperation = Operation(previous);
            const bool linear = previousOperation == "call"
                || previousOperation == "wait"
                || previousOperation == "waitEvent"
                || previousOperation == "set";
            if (linear)
            {
                // Splice only linear flow nodes. Branch/jump/end need explicit
                // routing so insertion does not silently drop a branch.
                previous["next"] = newId;
            }
            else
            {
                m_Status = "Inserted node after a branching/terminal node; "
                    "connect it manually.";
            }
        }

        // Splice the inserted node into the old successor instead of leaving
        // the remainder of the chain orphaned.
        const std::string insertedOperation = Operation(node);
        if (!previousTarget.empty()
            && (previousOperation == "call" || previousOperation == "wait"
                || previousOperation == "waitEvent" || previousOperation == "set")
            && (insertedOperation == "call" || insertedOperation == "wait"
                || insertedOperation == "waitEvent" || insertedOperation == "set"))
        {
            node["next"] = previousTarget;
        }

        ClearNodeSelection();
        m_SelectedNodeIndices.insert(insertAt);
        m_SelectedNodeIndex = insertAt;
        m_Dirty = true;
        m_Status = "Added " + OperationTitle(operation) + " node.";
    }

    void BehaviorGraphPanel::AddCallNode(const std::string& primitiveId,
        const ImVec2& logicalPosition)
    {
        if (primitiveId.empty())
            return;

        AddNodeAt("call", logicalPosition);
        YAML::Node node = SelectedNode();
        if (!node)
            return;

        node["call"] = primitiveId;
        node.remove("inputs");
        node.remove("outputs");
        m_Dirty = true;
        m_Status = "Added call node " + primitiveId + ".";
    }

    void BehaviorGraphPanel::SelectNode(int index, bool additive)
    {
        YAML::Node behavior = SelectedBehavior();
        YAML::Node nodes = behavior ? behavior["nodes"] : YAML::Node{};
        if (!nodes || !nodes.IsSequence()
            || index < 0
            || index >= static_cast<int>(nodes.size()))
        {
            return;
        }

        if (!additive)
            m_SelectedNodeIndices.clear();

        if (additive && m_SelectedNodeIndices.count(index) > 0)
            m_SelectedNodeIndices.erase(index);
        else
            m_SelectedNodeIndices.insert(index);

        if (m_SelectedNodeIndices.empty())
            m_SelectedNodeIndex = -1;
        else if (m_SelectedNodeIndices.count(index) > 0)
            m_SelectedNodeIndex = index;
        else
            m_SelectedNodeIndex = *m_SelectedNodeIndices.rbegin();
    }

    void BehaviorGraphPanel::ClearNodeSelection()
    {
        m_SelectedNodeIndices.clear();
        m_SelectedNodeIndex = -1;
    }

    void BehaviorGraphPanel::DeleteSelectedNode()
    {
        DeleteSelectedNodes();
    }

    void BehaviorGraphPanel::DeleteSelectedNodes()
    {
        YAML::Node behavior = SelectedBehavior();
        YAML::Node nodes = behavior ? behavior["nodes"] : YAML::Node{};
        if (!behavior || !nodes || !nodes.IsSequence())
            return;

        std::set<int> selected = m_SelectedNodeIndices;
        if (selected.empty() && m_SelectedNodeIndex >= 0)
            selected.insert(m_SelectedNodeIndex);
        selected.erase(-1);
        if (selected.empty())
            return;

        if (static_cast<size_t>(selected.size()) >= nodes.size())
        {
            m_Status = "A behavior entry must keep at least one node.";
            return;
        }

        CaptureUndoSnapshot();

        std::unordered_map<std::string, std::string> replacementTargets;
        for (int index : selected)
        {
            int replacementIndex = -1;
            for (int next = index + 1; next < static_cast<int>(nodes.size()); ++next)
            {
                if (selected.count(next) == 0)
                {
                    replacementIndex = next;
                    break;
                }
            }
            if (replacementIndex < 0)
            {
                // No successor: clear inbound links instead of creating a
                // self-loop back to the previous node.
                replacementIndex = -1;
            }

            const std::string deletedId = ReadString(
                nodes[static_cast<size_t>(index)], "id");
            if (!deletedId.empty())
            {
                replacementTargets[deletedId] = replacementIndex >= 0
                    ? ReadString(nodes[static_cast<size_t>(replacementIndex)], "id")
                    : std::string{};
            }
        }

        YAML::Node replacement(YAML::NodeType::Sequence);
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            if (selected.count(static_cast<int>(i)) > 0)
                continue;

            YAML::Node node = nodes[i];
            for (const char* key : { "next", "true", "false", "onTimeout", "jump" })
            {
                const auto it = replacementTargets.find(ReadString(node, key));
                if (it == replacementTargets.end())
                    continue;

                if (it->second.empty())
                    node.remove(key);
                else
                    node[key] = it->second;
            }
            replacement.push_back(node);
        }

        behavior["nodes"] = replacement;

        const auto entryIt = replacementTargets.find(ReadString(behavior, "entry"));
        if (entryIt != replacementTargets.end())
        {
            if (entryIt->second.empty())
                behavior["entry"] = ReadString(
                    replacement[0], "id", "node");
            else
                behavior["entry"] = entryIt->second;
        }

        ClearNodeSelection();
        const int firstSelected = *selected.begin();
        m_SelectedNodeIndex = std::min(
            firstSelected,
            static_cast<int>(replacement.size()) - 1);
        m_SelectedNodeIndices.insert(m_SelectedNodeIndex);
        m_Dirty = true;
        m_Status = "Deleted " + std::to_string(selected.size()) +
            (selected.size() == 1 ? " node." : " nodes.");
    }

    void BehaviorGraphPanel::ClearNodeLink(int sourceIndex,
        const std::string& outputSlot)
    {
        YAML::Node behavior = SelectedBehavior();
        YAML::Node nodes = behavior ? behavior["nodes"] : YAML::Node{};
        if (!nodes || !nodes.IsSequence()
            || sourceIndex < 0
            || sourceIndex >= static_cast<int>(nodes.size())
            || outputSlot.empty())
        {
            return;
        }

        YAML::Node source = nodes[static_cast<size_t>(sourceIndex)];
        if (!source || !source[outputSlot])
            return;

        CaptureUndoSnapshot();
        source.remove(outputSlot);
        m_Dirty = true;
        m_Status = "Disconnected " + ReadString(source, "id") + "." + outputSlot + ".";
    }

    void BehaviorGraphPanel::ClearNodeInput(int nodeIndex,
        const std::string& inputName)
    {
        YAML::Node behavior = SelectedBehavior();
        YAML::Node nodes = behavior ? behavior["nodes"] : YAML::Node{};
        if (!nodes || !nodes.IsSequence()
            || nodeIndex < 0
            || nodeIndex >= static_cast<int>(nodes.size())
            || inputName.empty())
        {
            return;
        }

        YAML::Node node = nodes[static_cast<size_t>(nodeIndex)];
        if (!node || !node["inputs"] || !node["inputs"].IsMap()
            || !node["inputs"][inputName])
        {
            return;
        }

        CaptureUndoSnapshot();
        node["inputs"].remove(inputName);
        if (node["inputs"].size() == 0)
            node.remove("inputs");
        m_Dirty = true;
        m_Status = "Cleared input " + inputName + ".";
    }

    void BehaviorGraphPanel::CopySelectedNode()
    {
        YAML::Node behavior = SelectedBehavior();
        YAML::Node nodes = behavior ? behavior["nodes"] : YAML::Node{};
        if (!behavior || !nodes || !nodes.IsSequence())
            return;

        std::set<int> selected = m_SelectedNodeIndices;
        if (selected.empty() && m_SelectedNodeIndex >= 0)
            selected.insert(m_SelectedNodeIndex);
        selected.erase(-1);
        if (selected.empty())
        {
            m_Status = "Select a node before copying.";
            return;
        }

        YAML::Node clipboard(YAML::NodeType::Sequence);
        for (int index : selected)
        {
            if (index >= 0 && index < static_cast<int>(nodes.size()))
                clipboard.push_back(nodes[static_cast<size_t>(index)]);
        }

        m_NodeClipboard = YAML::Dump(clipboard);
        m_Status = "Copied " + std::to_string(selected.size()) +
            (selected.size() == 1 ? " node." : " nodes.");
    }

    void BehaviorGraphPanel::PasteNode()
    {
        if (m_NodeClipboard.empty())
        {
            m_Status = "Node clipboard is empty.";
            return;
        }

        YAML::Node behavior = SelectedBehavior();
        YAML::Node nodes = behavior ? behavior["nodes"] : YAML::Node{};
        if (!behavior || !nodes || !nodes.IsSequence())
            return;

        try
        {
            YAML::Node clipboard = YAML::Load(m_NodeClipboard);
            YAML::Node sourceNodes(YAML::NodeType::Sequence);
            if (clipboard && clipboard.IsMap())
                sourceNodes.push_back(clipboard);
            else if (clipboard && clipboard.IsSequence())
                sourceNodes = clipboard;

            if (!sourceNodes.IsSequence() || sourceNodes.size() == 0)
            {
                m_Status = "Clipboard does not contain any nodes.";
                return;
            }

            CaptureUndoSnapshot();

            std::unordered_map<std::string, std::string> idMap;
            std::unordered_map<std::string, std::string> localMap;
            std::set<std::string> generatedIds;
            std::vector<YAML::Node> pasted;

            for (const YAML::Node& source : sourceNodes)
            {
                if (!source || !source.IsMap())
                    continue;

                const std::string oldId = ReadString(source, "id", "node");
                const std::string base = ReadString(source, "id", "node");
                std::string newId = UniqueNodeId(nodes, base);
                int suffix = 2;
                while (generatedIds.count(newId) > 0)
                    newId = base + "_" + std::to_string(suffix++);
                generatedIds.insert(newId);
                idMap[oldId] = newId;

                YAML::Node node = source;
                node["id"] = newId;
                const ImVec2 oldPosition = ReadNodePosition(node, { 42.0f, 32.0f });
                WriteNodePosition(node, {
                    oldPosition.x + 40.0f,
                    oldPosition.y + 40.0f
                });
                pasted.push_back(node);
            }

            if (pasted.empty())
            {
                m_Status = "Clipboard does not contain valid node maps.";
                return;
            }

            // Give every copied output a namespaced local so pasted nodes do
            // not collide with locals in the destination graph.
            for (YAML::Node& node : pasted)
            {
                const std::string newId = ReadString(node, "id", "node");
                if (!node["outputs"] || !node["outputs"].IsMap())
                    continue;

                YAML::Node outputs(YAML::NodeType::Map);
                for (auto it = node["outputs"].begin(); it != node["outputs"].end(); ++it)
                {
                    if (!it->first.IsScalar())
                        continue;
                    const std::string oldLocal = it->second.IsScalar()
                        ? it->second.as<std::string>("")
                        : std::string{};
                    const std::string newLocal = newId + "_" +
                        it->first.as<std::string>("output");
                    if (!oldLocal.empty())
                        localMap[oldLocal] = newLocal;
                    outputs[it->first.as<std::string>()] = newLocal;
                }
                node["outputs"] = outputs;
            }

            for (YAML::Node& node : pasted)
            {
                for (const char* key : { "next", "true", "false", "onTimeout", "jump" })
                {
                    const std::string target = ReadString(node, key);
                    if (target.empty())
                        continue;

                    const auto idIt = idMap.find(target);
                    if (idIt != idMap.end())
                        node[key] = idIt->second;
                    else
                        node.remove(key);
                }

                if (node["inputs"] && node["inputs"].IsMap())
                {
                    YAML::Node inputs(YAML::NodeType::Map);
                    for (auto it = node["inputs"].begin(); it != node["inputs"].end(); ++it)
                    {
                        if (!it->first.IsScalar())
                            continue;

                        if (IsLocalReference(it->second))
                        {
                            const std::string local = LocalReferenceName(it->second);
                            const auto localIt = localMap.find(local);
                            if (localIt != localMap.end())
                                inputs[it->first.as<std::string>()] = "$" + localIt->second;
                            continue;
                        }

                        inputs[it->first.as<std::string>()] = it->second;
                    }

                    if (inputs.size() == 0)
                        node.remove("inputs");
                    else
                        node["inputs"] = inputs;
                }
            }

            const int insertAt = m_SelectedNodeIndex >= 0
                ? std::min(m_SelectedNodeIndex + 1, static_cast<int>(nodes.size()))
                : static_cast<int>(nodes.size());
            YAML::Node replacement(YAML::NodeType::Sequence);
            for (int i = 0; i < insertAt; ++i)
                replacement.push_back(nodes[static_cast<size_t>(i)]);
            for (const YAML::Node& node : pasted)
                replacement.push_back(node);
            for (size_t i = static_cast<size_t>(insertAt); i < nodes.size(); ++i)
                replacement.push_back(nodes[i]);
            behavior["nodes"] = replacement;

            ClearNodeSelection();
            for (int i = 0; i < static_cast<int>(pasted.size()); ++i)
                m_SelectedNodeIndices.insert(insertAt + i);
            m_SelectedNodeIndex = insertAt;
            m_Dirty = true;
            m_Status = "Pasted " + std::to_string(pasted.size()) +
                (pasted.size() == 1 ? " node." : " nodes.");
        }
        catch (const YAML::Exception& exception)
        {
            m_Status = std::string("Paste failed: ") + exception.what();
        }
    }

    void BehaviorGraphPanel::DuplicateSelectedNode()
    {
        CopySelectedNode();
        PasteNode();
    }

    void BehaviorGraphPanel::ConnectDataPin(int sourceIndex,
        const std::string& outputSlot,
        int targetIndex,
        const std::string& inputName)
    {
        YAML::Node behavior = SelectedBehavior();
        YAML::Node nodes = behavior ? behavior["nodes"] : YAML::Node{};
        if (!nodes || !nodes.IsSequence()
            || sourceIndex < 0 || targetIndex < 0
            || sourceIndex >= static_cast<int>(nodes.size())
            || targetIndex >= static_cast<int>(nodes.size())
            || outputSlot.empty() || inputName.empty())
        {
            return;
        }

        YAML::Node source = nodes[static_cast<size_t>(sourceIndex)];
        YAML::Node target = nodes[static_cast<size_t>(targetIndex)];
        if (!source || !target)
            return;

        WAO::BehaviorValueType outputType = WAO::BehaviorValueType::None;
        for (const WAO::BehaviorPrimitivePort& port : DataOutputPorts(source))
            if (port.Name == outputSlot)
                outputType = port.Type;

        WAO::BehaviorValueType inputType = WAO::BehaviorValueType::None;
        for (const WAO::BehaviorPrimitivePort& port : DataInputPorts(target))
            if (port.Name == inputName)
                inputType = port.Type;

        if (outputType != WAO::BehaviorValueType::None
            && inputType != WAO::BehaviorValueType::None
            && outputType != inputType)
        {
            m_Status = "Type mismatch: " + outputSlot + " (" +
                PortTypeName(outputType) + ") -> " + inputName + " (" +
                PortTypeName(inputType) + ").";
            return;
        }

        std::string localName;
        if (source["outputs"] && source["outputs"][outputSlot]
            && source["outputs"][outputSlot].IsScalar())
        {
            localName = source["outputs"][outputSlot].as<std::string>("");
        }
        if (localName.empty())
        {
            localName = ReadString(source, "id", "node") + "_" + outputSlot;
        }

        CaptureUndoSnapshot();
        if (!source["outputs"] || !source["outputs"].IsMap())
            source["outputs"] = YAML::Node(YAML::NodeType::Map);
        if (!target["inputs"] || !target["inputs"].IsMap())
            target["inputs"] = YAML::Node(YAML::NodeType::Map);
        source["outputs"][outputSlot] = localName;
        target["inputs"][inputName] = "$" + localName;
        m_Dirty = true;
        m_Status = "Connected data " + ReadString(source, "id") + "." +
            outputSlot + " -> " + ReadString(target, "id") + "." + inputName +
            " ($" + localName + ").";
    }

    void BehaviorGraphPanel::ValidateGraph()
    {
        m_Diagnostics.clear();
        m_ErrorNodeIndices.clear();

        YAML::Node behavior = SelectedBehavior();
        YAML::Node nodes = behavior ? behavior["nodes"] : YAML::Node{};
        if (!behavior || !nodes || !nodes.IsSequence())
            return;

        auto error = [this](const std::string& message)
        {
            m_Diagnostics.push_back(message);
        };
        auto nodeError = [this](const std::string& message, int nodeIndex)
        {
            m_Diagnostics.push_back(message);
            m_ErrorNodeIndices.insert(nodeIndex);
        };

        std::unordered_map<std::string, int> ids;
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const std::string id = ReadString(nodes[i], "id");
            if (id.empty())
            {
                nodeError("Node " + std::to_string(i + 1) + ": id is empty.",
                    static_cast<int>(i));
                continue;
            }
            const auto [it, inserted] = ids.emplace(id, static_cast<int>(i));
            (void)it;
            if (!inserted)
                nodeError("Duplicate node id: " + id, static_cast<int>(i));
        }

        const std::string entry = ReadString(behavior, "entry");
        if (!entry.empty() && ids.find(entry) == ids.end())
            error("Entry node is missing: " + entry);

        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const YAML::Node node = nodes[i];
            const int nodeIndex = static_cast<int>(i);
            const std::string nodeId = ReadString(node, "id", "node");
            const std::string operation = Operation(node);

            if (operation.empty())
                nodeError("Node " + nodeId + ": unsupported operation.", nodeIndex);

            if (operation == "call")
            {
                const WAO::BehaviorPrimitiveDefinition* definition =
                    FindPrimitive(node);
                if (!definition)
                {
                    nodeError("Node " + nodeId + ": unknown primitive '" +
                        ReadString(node, "call") + "'.", nodeIndex);
                }
                else
                {
                    for (const WAO::BehaviorPrimitivePort& port : definition->Inputs)
                    {
                        if (!port.Required)
                            continue;
                        if (!node["inputs"] || !node["inputs"][port.Name])
                        {
                            nodeError("Node " + nodeId + ": required input '" +
                                port.Name + "' is missing.", nodeIndex);
                        }
                    }
                }

                if (node["inputs"] && node["inputs"].IsMap())
                {
                    for (auto it = node["inputs"].begin(); it != node["inputs"].end(); ++it)
                    {
                        if (!IsLocalReference(it->second))
                            continue;
                        const std::string local = LocalReferenceName(it->second);
                        if (FindLocalProducer(nodes, local).NodeIndex < 0)
                        {
                            nodeError("Node " + nodeId + ": local $" + local +
                                " has no producer.", nodeIndex);
                        }
                    }
                }
            }

            for (const std::string& slot : OutputSlots(node))
            {
                const std::string target = LinkTarget(node, slot);
                if (target.empty())
                {
                    if (operation == "branch"
                        || operation == "waitEvent"
                        || operation == "jump")
                    {
                        nodeError("Node " + nodeId + ": output '" + slot +
                            "' is not connected.", nodeIndex);
                    }
                    continue;
                }
                if (ids.find(target) == ids.end())
                {
                    nodeError("Node " + nodeId + ": output '" + slot +
                        "' points to missing node '" + target + "'.", nodeIndex);
                }
            }
        }

        if (m_Diagnostics.empty())
            return;
    }

    void BehaviorGraphPanel::FitGraph()
    {
        m_FitRequested = true;
    }

    void BehaviorGraphPanel::AutoLayoutGraph()
    {
        YAML::Node behavior = SelectedBehavior();
        YAML::Node nodes = behavior ? behavior["nodes"] : YAML::Node{};
        if (!behavior || !nodes || !nodes.IsSequence() || nodes.size() == 0)
            return;

        CaptureUndoSnapshot();
        const size_t columns = 4;
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const size_t column = i % columns;
            const size_t row = i / columns;
            WriteNodePosition(
                nodes[i],
                {
                    42.0f + static_cast<float>(column) * 360.0f,
                    32.0f + static_cast<float>(row) * 180.0f
                });
        }
        m_Dirty = true;
        m_FitRequested = true;
        m_Status = "Auto-laid out " + std::to_string(nodes.size()) + " nodes.";
    }

    void BehaviorGraphPanel::SetNodeLink(int sourceIndex,
        const std::string& outputSlot,
        const std::string& targetId)
    {
        YAML::Node behavior = SelectedBehavior();
        YAML::Node nodes = behavior ? behavior["nodes"] : YAML::Node{};
        if (!nodes || !nodes.IsSequence() ||
            sourceIndex < 0 ||
            sourceIndex >= static_cast<int>(nodes.size()) ||
            outputSlot.empty() ||
            targetId.empty())
        {
            return;
        }

        YAML::Node source = nodes[static_cast<size_t>(sourceIndex)];
        if (!source)
            return;

        CaptureUndoSnapshot();
        source[outputSlot] = targetId;
        m_Dirty = true;
        m_Status = "Connected " + ReadString(source, "id") +
            "." + outputSlot + " -> " + targetId + ".";
    }

    void BehaviorGraphPanel::DrawGraph()
    {
        YAML::Node behavior = SelectedBehavior();
        YAML::Node nodesYaml = behavior ? behavior["nodes"] : YAML::Node{};
        if (!behavior || !nodesYaml || !nodesYaml.IsSequence())
        {
            ImGui::TextDisabled("No behavior entry selected.");
            return;
        }

        ValidateGraph();

        const ImVec2 contentSize = ImGui::GetContentRegionAvail();
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float panelsWidth = std::max(0.0f,
            contentSize.x - kSplitterWidth - spacing * 2.0f);
        const float minimumDetailsWidth = std::min(
            kMinimumDetailsWidth,
            panelsWidth * 0.5f);
        const float minimumGraphWidth = std::min(
            kMinimumGraphWidth,
            std::max(0.0f, panelsWidth - minimumDetailsWidth));
        const float maximumDetailsWidth = std::max(
            minimumDetailsWidth,
            panelsWidth - minimumGraphWidth);
        m_DetailsWidth = std::clamp(
            m_DetailsWidth,
            minimumDetailsWidth,
            maximumDetailsWidth);

        ImVec2 graphSize = contentSize;
        graphSize.x = std::max(0.0f, panelsWidth - m_DetailsWidth);
        ImGui::BeginChild("BehaviorGraphCanvas", graphSize, true,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
        const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        const ImVec2 canvasMax = Add(canvasMin, canvasSize);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const bool darkTheme = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg).x < 0.5f;
        drawList->AddRectFilled(canvasMin, canvasMax,
            darkTheme ? IM_COL32(12, 18, 21, 246) : IM_COL32(246, 242, 234, 246),
            4.0f);
        DrawGrid(drawList, canvasMin, canvasMax, m_GraphPan, m_Zoom);

        ImGui::InvisibleButton("##BehaviorGraphSurface", canvasSize,
            ImGuiButtonFlags_MouseButtonMiddle);
        const bool hovered = ImGui::IsItemHovered();
        if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
            m_GraphPan = Add(m_GraphPan, ImGui::GetIO().MouseDelta);
        if (hovered && ImGui::GetIO().MouseWheel != 0.0f && ImGui::GetIO().KeyCtrl)
            m_Zoom = std::clamp(
                m_Zoom * (1.0f + ImGui::GetIO().MouseWheel * 0.1f),
                0.65f,
                1.45f);

        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const std::string search = m_NodeSearchInput.data();

        struct LogicalNode
        {
            int Index = -1;
            ImVec2 Position;
            ImVec2 Size;
            std::string Id;
            std::string Title;
            std::string Body;
            bool Matches = true;
        };

        std::vector<LogicalNode> logicalNodes;
        logicalNodes.reserve(nodesYaml.size());
        for (size_t i = 0; i < nodesYaml.size(); ++i)
        {
            const YAML::Node node = nodesYaml[i];
            const ImVec2 fallbackPosition = {
                42.0f,
                32.0f + static_cast<float>(i) * 126.0f
            };
            const ImVec2 logicalPosition = ReadNodePosition(
                node,
                fallbackPosition);
            logicalNodes.push_back({
                static_cast<int>(i),
                logicalPosition,
                { 300.0f, NodeHeight(node, 1.0f) },
                ReadString(node, "id", "node"),
                OperationTitle(Operation(node)),
                NodeBody(node),
                NodeMatchesSearch(node, search)
            });
        }

        if (m_FitRequested && !logicalNodes.empty())
        {
            ImVec2 minPosition(
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max());
            ImVec2 maxPosition(
                -std::numeric_limits<float>::max(),
                -std::numeric_limits<float>::max());
            for (const LogicalNode& node : logicalNodes)
            {
                minPosition.x = std::min(minPosition.x, node.Position.x);
                minPosition.y = std::min(minPosition.y, node.Position.y);
                maxPosition.x = std::max(maxPosition.x,
                    node.Position.x + node.Size.x);
                maxPosition.y = std::max(maxPosition.y,
                    node.Position.y + node.Size.y);
            }

            const ImVec2 center(
                (minPosition.x + maxPosition.x) * 0.5f,
                (minPosition.y + maxPosition.y) * 0.5f);
            const ImVec2 extents(
                maxPosition.x - minPosition.x,
                maxPosition.y - minPosition.y);
            const float zoomX = canvasSize.x / std::max(1.0f, extents.x + 140.0f);
            const float zoomY = canvasSize.y / std::max(1.0f, extents.y + 140.0f);
            m_Zoom = std::clamp(std::min(zoomX, zoomY), 0.65f, 1.45f);
            m_GraphPan = {
                canvasSize.x * 0.5f - center.x * m_Zoom,
                canvasSize.y * 0.5f - center.y * m_Zoom
            };
            m_FitRequested = false;
        }

        std::vector<GraphNode> nodes;
        nodes.reserve(logicalNodes.size());
        for (const LogicalNode& node : logicalNodes)
        {
            const ImVec2 min = Add(
                canvasMin,
                Add(m_GraphPan, Scale(node.Position, m_Zoom)));
            nodes.push_back({
                node.Index,
                min,
                Add(min, Scale(node.Size, m_Zoom)),
                node.Position,
                node.Id,
                node.Title,
                node.Body
            });
        }

        auto findGraphNode = [&](const std::string& id) -> const GraphNode*
        {
            for (const GraphNode& node : nodes)
                if (node.Id == id)
                    return &node;
            return nullptr;
        };

        auto findGraphNodeByIndex = [&](int index) -> const GraphNode*
        {
            for (const GraphNode& node : nodes)
                if (node.Index == index)
                    return &node;
            return nullptr;
        };

        if (ImGui::BeginPopupContextItem("##BehaviorGraphContext"))
        {
            const ImVec2 logicalMouse = Scale(
                {
                    mouse.x - canvasMin.x - m_GraphPan.x,
                    mouse.y - canvasMin.y - m_GraphPan.y
                },
                1.0f / std::max(0.01f, m_Zoom));

            if (ImGui::BeginMenu("Add Flow Node"))
            {
                for (const char* operation : { "branch", "jump", "wait", "waitEvent", "set", "end" })
                {
                    if (ImGui::MenuItem(OperationTitle(operation).c_str()))
                    {
                        AddNodeAt(operation, logicalMouse);
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Add Call Node"))
            {
                for (const WAO::BehaviorPrimitiveDefinition& definition :
                    WAO::BehaviorPrimitiveRegistry::All())
                {
                    const std::string label = definition.DisplayName.empty()
                        ? definition.Id
                        : definition.DisplayName + " [" + definition.Id + "]";
                    if (ImGui::MenuItem(label.c_str()))
                    {
                        AddCallNode(definition.Id, logicalMouse);
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Paste Node"))
                PasteNode();
            if (ImGui::MenuItem("Validate Graph"))
                ValidateGraph();
            if (ImGui::MenuItem("Fit Graph"))
                FitGraph();
            ImGui::EndPopup();
        }

        drawList->PushClipRect(canvasMin, canvasMax, true);

        // Control-flow links.
        for (const GraphNode& node : nodes)
        {
            const YAML::Node source = nodesYaml[static_cast<size_t>(node.Index)];
            for (const std::string& outputSlot : OutputSlots(source))
            {
                const std::string targetId = LinkTarget(source, outputSlot);
                if (const GraphNode* target = findGraphNode(targetId))
                {
                    DrawConnection(drawList,
                        OutputPin(node, source, outputSlot),
                        InputPin(*target),
                        LinkColor(outputSlot),
                        2.0f * m_Zoom);
                }
            }
        }

        // Typed data links: an input authored as "$local" renders an edge from
        // the node that publishes that local in its outputs map.
        for (const GraphNode& targetNode : nodes)
        {
            const YAML::Node targetYaml = nodesYaml[static_cast<size_t>(targetNode.Index)];
            const std::vector<WAO::BehaviorPrimitivePort> inputPorts =
                DataInputPorts(targetYaml);
            if (!targetYaml["inputs"] || !targetYaml["inputs"].IsMap())
                continue;

            for (size_t inputIndex = 0; inputIndex < inputPorts.size(); ++inputIndex)
            {
                const WAO::BehaviorPrimitivePort& port = inputPorts[inputIndex];
                const YAML::Node value = targetYaml["inputs"][port.Name];
                if (!IsLocalReference(value))
                    continue;

                const LocalProducer producer = FindLocalProducer(
                    nodesYaml,
                    LocalReferenceName(value));
                if (producer.NodeIndex < 0)
                    continue;

                const GraphNode* sourceNode = findGraphNodeByIndex(producer.NodeIndex);
                if (!sourceNode)
                    continue;

                const YAML::Node sourceYaml =
                    nodesYaml[static_cast<size_t>(producer.NodeIndex)];
                const std::vector<WAO::BehaviorPrimitivePort> outputPorts =
                    DataOutputPorts(sourceYaml);
                const auto outputIt = std::find_if(
                    outputPorts.begin(),
                    outputPorts.end(),
                    [&](const WAO::BehaviorPrimitivePort& outputPort)
                    {
                        return outputPort.Name == producer.OutputSlot;
                    });
                if (outputIt == outputPorts.end())
                    continue;

                const size_t outputIndex = static_cast<size_t>(
                    std::distance(outputPorts.begin(), outputIt));
                DrawConnection(drawList,
                    DataOutputPin(*sourceNode, outputIndex, outputPorts.size()),
                    DataInputPin(targetNode, inputIndex, inputPorts.size()),
                    PortColor(port.Type),
                    1.7f * m_Zoom);
            }
        }

        if (m_PendingLinkSourceIndex >= 0)
        {
            if (const GraphNode* sourceNode =
                findGraphNodeByIndex(m_PendingLinkSourceIndex))
            {
                const YAML::Node source =
                    nodesYaml[static_cast<size_t>(sourceNode->Index)];
                DrawConnection(
                    drawList,
                    OutputPin(*sourceNode, source, m_PendingLinkOutputSlot),
                    mouse,
                    LinkColor(m_PendingLinkOutputSlot),
                    2.0f * m_Zoom);
            }
        }

        if (m_PendingDataLinkSourceIndex >= 0)
        {
            if (const GraphNode* sourceNode =
                findGraphNodeByIndex(m_PendingDataLinkSourceIndex))
            {
                const YAML::Node source =
                    nodesYaml[static_cast<size_t>(sourceNode->Index)];
                const std::vector<WAO::BehaviorPrimitivePort> outputPorts =
                    DataOutputPorts(source);
                const auto outputIt = std::find_if(
                    outputPorts.begin(),
                    outputPorts.end(),
                    [&](const WAO::BehaviorPrimitivePort& port)
                    {
                        return port.Name == m_PendingDataLinkOutputSlot;
                    });
                if (outputIt != outputPorts.end())
                {
                    const size_t outputIndex = static_cast<size_t>(
                        std::distance(outputPorts.begin(), outputIt));
                    DrawConnection(drawList,
                        DataOutputPin(*sourceNode, outputIndex, outputPorts.size()),
                        mouse,
                        PortColor(m_PendingDataLinkType),
                        2.0f * m_Zoom);
                }
            }
        }

        bool leftClickHandled = false;
        for (const GraphNode& node : nodes)
        {
            const YAML::Node source = nodesYaml[static_cast<size_t>(node.Index)];
            const bool selected = m_SelectedNodeIndices.count(node.Index) > 0
                || (m_SelectedNodeIndices.empty() && node.Index == m_SelectedNodeIndex);
            const bool nodeHovered = hovered && PointInRect(mouse, node.Min, node.Max);
            const bool hasError = m_ErrorNodeIndices.count(node.Index) > 0;
            const bool matches = logicalNodes[static_cast<size_t>(node.Index)].Matches;

            ImU32 border = selected
                ? IM_COL32(255, 216, 92, 255)
                : (hasError
                    ? IM_COL32(240, 82, 70, 255)
                    : (nodeHovered
                        ? IM_COL32(116, 226, 196, 255)
                        : IM_COL32(82, 114, 118, 220)));
            if (!matches && !selected && !hasError)
                border = IM_COL32(58, 68, 70, 150);

            drawList->AddRectFilled(node.Min, node.Max,
                NodeColor(Operation(source)), 7.0f * m_Zoom);
            drawList->AddRect(node.Min, node.Max, border, 7.0f * m_Zoom,
                0, selected ? 3.0f : 1.6f);

            const ImVec2 inputPin = InputPin(node);
            drawList->AddCircleFilled(inputPin, 6.0f * m_Zoom,
                IM_COL32(200, 248, 226, 240));
            drawList->AddText(
                { node.Min.x + 34.0f * m_Zoom, node.Min.y + 12.0f * m_Zoom },
                IM_COL32(232, 246, 240, 255),
                node.Title.c_str());
            drawList->AddText(
                { node.Min.x + 34.0f * m_Zoom, node.Min.y + 36.0f * m_Zoom },
                IM_COL32(178, 203, 196, 255),
                node.Body.c_str());
            drawList->AddText(
                { node.Min.x + 34.0f * m_Zoom, node.Min.y + 54.0f * m_Zoom },
                IM_COL32(150, 180, 174, 230),
                node.Id.c_str());

            for (const std::string& outputSlot : OutputSlots(source))
            {
                const ImVec2 outputPin = OutputPin(node, source, outputSlot);
                drawList->AddCircleFilled(outputPin, 6.0f * m_Zoom,
                    LinkColor(outputSlot));
                drawList->AddText(
                    { outputPin.x + 9.0f * m_Zoom, outputPin.y - 7.0f * m_Zoom },
                    LinkColor(outputSlot),
                    outputSlot.c_str());
            }

            const std::vector<WAO::BehaviorPrimitivePort> inputPorts =
                DataInputPorts(source);
            for (size_t i = 0; i < inputPorts.size(); ++i)
            {
                const WAO::BehaviorPrimitivePort& port = inputPorts[i];
                const ImVec2 pin = DataInputPin(node, i, inputPorts.size());
                const ImU32 color = PortColor(port.Type);
                drawList->AddCircleFilled(pin, 5.0f * m_Zoom, color);
                const std::string label = port.Name + " : " +
                    PortTypeName(port.Type) + (port.Required ? "*" : "");
                drawList->AddText(
                    { pin.x + 8.0f * m_Zoom, pin.y - 7.0f * m_Zoom },
                    color,
                    label.c_str());
            }

            const std::vector<WAO::BehaviorPrimitivePort> outputPorts =
                DataOutputPorts(source);
            for (size_t i = 0; i < outputPorts.size(); ++i)
            {
                const WAO::BehaviorPrimitivePort& port = outputPorts[i];
                const ImVec2 pin = DataOutputPin(node, i, outputPorts.size());
                const ImU32 color = PortColor(port.Type);
                drawList->AddCircleFilled(pin, 5.0f * m_Zoom, color);
                const std::string label = port.Name;
                const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(
                    ImGui::GetFontSize() * m_Zoom,
                    FLT_MAX,
                    0.0f,
                    label.c_str());
                drawList->AddText(
                    { pin.x - 8.0f * m_Zoom - textSize.x, pin.y - 7.0f * m_Zoom },
                    color,
                    label.c_str());
            }

            if (!hovered || !ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                continue;

            // Typed data pins take precedence over the node body so that a
            // click on a pin never starts a node drag.
            for (size_t i = 0; i < outputPorts.size(); ++i)
            {
                if (!PointInCircle(mouse, DataOutputPin(node, i, outputPorts.size()),
                        10.0f * m_Zoom))
                {
                    continue;
                }

                SelectNode(node.Index, ImGui::GetIO().KeyCtrl);
                m_PendingDataLinkSourceIndex = node.Index;
                m_PendingDataLinkOutputSlot = outputPorts[i].Name;
                m_PendingDataLinkType = outputPorts[i].Type;
                m_PendingLinkSourceIndex = -1;
                m_PendingLinkOutputSlot.clear();
                m_Status = "Connecting data " + node.Id + "." +
                    outputPorts[i].Name + ": click a matching input pin.";
                leftClickHandled = true;
                break;
            }
            if (leftClickHandled)
                break;

            for (size_t i = 0; i < inputPorts.size(); ++i)
            {
                if (!PointInCircle(mouse, DataInputPin(node, i, inputPorts.size()),
                        10.0f * m_Zoom))
                {
                    continue;
                }

                if (m_PendingDataLinkSourceIndex >= 0)
                {
                    ConnectDataPin(
                        m_PendingDataLinkSourceIndex,
                        m_PendingDataLinkOutputSlot,
                        node.Index,
                        inputPorts[i].Name);
                    m_PendingDataLinkSourceIndex = -1;
                    m_PendingDataLinkOutputSlot.clear();
                    m_PendingDataLinkType = WAO::BehaviorValueType::None;
                }
                else
                {
                    m_Status = "Select an output data pin before connecting.";
                }
                leftClickHandled = true;
                break;
            }
            if (leftClickHandled)
                break;

            if (PointInCircle(mouse, inputPin, 10.0f * m_Zoom))
            {
                if (m_PendingLinkSourceIndex >= 0 &&
                    !m_PendingLinkOutputSlot.empty())
                {
                    SetNodeLink(
                        m_PendingLinkSourceIndex,
                        m_PendingLinkOutputSlot,
                        node.Id);
                    m_PendingLinkSourceIndex = -1;
                    m_PendingLinkOutputSlot.clear();
                }
                else
                {
                    m_Status = "Select an output pin before connecting.";
                }
                leftClickHandled = true;
                break;
            }

            bool outputClicked = false;
            for (const std::string& outputSlot : OutputSlots(source))
            {
                if (!PointInCircle(
                    mouse,
                    OutputPin(node, source, outputSlot),
                    10.0f * m_Zoom))
                {
                    continue;
                }

                SelectNode(node.Index, ImGui::GetIO().KeyCtrl);
                m_PendingLinkSourceIndex = node.Index;
                m_PendingLinkOutputSlot = outputSlot;
                m_PendingDataLinkSourceIndex = -1;
                m_PendingDataLinkOutputSlot.clear();
                m_Status = "Connecting " + node.Id + "." + outputSlot +
                    ": click a target input pin.";
                outputClicked = true;
                break;
            }
            if (outputClicked)
            {
                leftClickHandled = true;
                break;
            }

            if (nodeHovered)
            {
                const bool additive = ImGui::GetIO().KeyCtrl
                    || ImGui::GetIO().KeyShift;
                if (additive)
                    SelectNode(node.Index, true);
                else if (m_SelectedNodeIndices.count(node.Index) == 0)
                    SelectNode(node.Index, false);

                if (m_SelectedNodeIndices.count(node.Index) > 0)
                {
                    m_DraggingNodeIndex = node.Index;
                    m_DragStartMouse = mouse;
                    m_DragNodeStates.clear();
                    for (int selectedIndex : m_SelectedNodeIndices)
                    {
                        if (selectedIndex >= 0
                            && selectedIndex < static_cast<int>(nodesYaml.size()))
                        {
                            m_DragNodeStates.emplace_back(
                                selectedIndex,
                                ReadNodePosition(
                                    nodesYaml[static_cast<size_t>(selectedIndex)],
                                    node.LogicalPosition));
                        }
                    }
                    if (m_DragNodeStates.empty())
                        m_DragNodeStates.emplace_back(node.Index, node.LogicalPosition);
                    m_DragUndoCaptured = false;
                }
                m_PendingLinkSourceIndex = -1;
                m_PendingLinkOutputSlot.clear();
                m_PendingDataLinkSourceIndex = -1;
                m_PendingDataLinkOutputSlot.clear();
                leftClickHandled = true;
                break;
            }
        }

        if (hovered && !leftClickHandled &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_BoxSelecting = true;
            m_BoxSelectAdditive = ImGui::GetIO().KeyCtrl
                || ImGui::GetIO().KeyShift;
            m_BoxSelectStart = mouse;
            m_BoxSelectEnd = mouse;
            if (!m_BoxSelectAdditive)
                ClearNodeSelection();
            m_DraggingNodeIndex = -1;
        }

        if (m_BoxSelecting)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                m_BoxSelectEnd = mouse;
            }
            else
            {
                m_BoxSelecting = false;
                const ImVec2 boxMin(
                    std::min(m_BoxSelectStart.x, m_BoxSelectEnd.x),
                    std::min(m_BoxSelectStart.y, m_BoxSelectEnd.y));
                const ImVec2 boxMax(
                    std::max(m_BoxSelectStart.x, m_BoxSelectEnd.x),
                    std::max(m_BoxSelectStart.y, m_BoxSelectEnd.y));
                if (boxMax.x - boxMin.x > 4.0f
                    || boxMax.y - boxMin.y > 4.0f)
                {
                    for (const GraphNode& node : nodes)
                    {
                        if (!RectIntersects(node.Min, node.Max, boxMin, boxMax))
                            continue;
                        m_SelectedNodeIndices.insert(node.Index);
                        m_SelectedNodeIndex = node.Index;
                    }
                }
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            m_PendingLinkSourceIndex = -1;
            m_PendingLinkOutputSlot.clear();
            m_PendingDataLinkSourceIndex = -1;
            m_PendingDataLinkOutputSlot.clear();
            m_PendingDataLinkType = WAO::BehaviorValueType::None;
            m_BoxSelecting = false;
        }

        if (!ImGui::GetIO().WantTextInput
            && ImGui::GetIO().KeyCtrl
            && ImGui::IsKeyPressed(ImGuiKey_A, false))
        {
            m_SelectedNodeIndices.clear();
            for (size_t i = 0; i < nodesYaml.size(); ++i)
                m_SelectedNodeIndices.insert(static_cast<int>(i));
            m_SelectedNodeIndex = nodesYaml.size() > 0 ? 0 : -1;
        }

        if (m_DraggingNodeIndex >= 0)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                YAML::Node draggingBehavior = SelectedBehavior();
                YAML::Node draggingNodes = draggingBehavior
                    ? draggingBehavior["nodes"]
                    : YAML::Node{};
                if (draggingNodes && draggingNodes.IsSequence())
                {
                    const ImVec2 delta = Scale(
                        {
                            mouse.x - m_DragStartMouse.x,
                            mouse.y - m_DragStartMouse.y
                        },
                        1.0f / std::max(0.01f, m_Zoom));
                    if (std::abs(delta.x) > 0.5f || std::abs(delta.y) > 0.5f)
                    {
                        if (!m_DragUndoCaptured)
                        {
                            CaptureUndoSnapshot();
                            m_DragUndoCaptured = true;
                        }

                        for (const auto& [index, startPosition] : m_DragNodeStates)
                        {
                            if (index < 0 || index >= static_cast<int>(draggingNodes.size()))
                                continue;
                            WriteNodePosition(
                                draggingNodes[static_cast<size_t>(index)],
                                Add(startPosition, delta));
                        }
                        m_Dirty = true;
                    }
                }
            }
            else
            {
                m_DraggingNodeIndex = -1;
                m_DragUndoCaptured = false;
                m_DragNodeStates.clear();
            }
        }

        if (m_BoxSelecting)
        {
            const ImVec2 boxMin(
                std::min(m_BoxSelectStart.x, m_BoxSelectEnd.x),
                std::min(m_BoxSelectStart.y, m_BoxSelectEnd.y));
            const ImVec2 boxMax(
                std::max(m_BoxSelectStart.x, m_BoxSelectEnd.x),
                std::max(m_BoxSelectStart.y, m_BoxSelectEnd.y));
            drawList->AddRectFilled(boxMin, boxMax, IM_COL32(80, 150, 255, 35));
            drawList->AddRect(boxMin, boxMax, IM_COL32(120, 200, 255, 220),
                0.0f, 0, 1.5f);
        }
        drawList->PopClipRect();
        ImGui::EndChild();

        ImGui::SameLine(0.0f, spacing);
        ImGui::InvisibleButton("##BehaviorGraphDetailsSplitter", { kSplitterWidth, -1.0f });
        if (ImGui::IsItemActive())
            m_DetailsWidth = std::clamp(
                m_DetailsWidth - ImGui::GetIO().MouseDelta.x,
                minimumDetailsWidth,
                maximumDetailsWidth);
        ImGui::SameLine(0.0f, spacing);
        DrawDetails();
    }

    void BehaviorGraphPanel::DrawDetails()
    {
        ImGui::BeginChild("BehaviorGraphDetails", { m_DetailsWidth, 0.0f }, true);
        ImGui::TextUnformatted("Behavior Details");
        ImGui::Separator();

        YAML::Node behavior = SelectedBehavior();
        if (!behavior)
        {
            ImGui::TextDisabled("No behavior entry.");
            ImGui::EndChild();
            return;
        }

        bool changed = false;
        std::string behaviorId = ReadString(behavior, "id");
        if (EditorWidgets::InputString("Entry Id", behaviorId))
        {
            behaviorId = behaviorId.empty() ? "new.behavior" : behaviorId;
            const std::string oldId = ReadString(behavior, "id");
            CaptureUndoSnapshot();
            behavior["id"] = behaviorId;
            if (m_SelectedEntry == oldId)
                m_SelectedEntry = behaviorId;
            changed = true;
        }
        int version = behavior["version"] ? behavior["version"].as<int>(1) : 1;
        if (ImGui::DragInt("Version", &version, 1.0f, 1, 100000))
        {
            CaptureUndoSnapshot();
            behavior["version"] = std::max(1, version);
            changed = true;
        }

        std::string entryNode = ReadString(behavior, "entry");
        if (EditorWidgets::InputString("Entry Node", entryNode))
        {
            CaptureUndoSnapshot();
            behavior["entry"] = entryNode;
            changed = true;
        }

        if (ImGui::Button("Add Call Node"))
            AddNode("call");
        ImGui::SameLine();
        if (ImGui::Button("Add Branch"))
            AddNode("branch");
        ImGui::SameLine();
        if (ImGui::Button("Add Wait"))
            AddNode("wait");
        if (ImGui::Button("Add Event Wait"))
            AddNode("waitEvent");
        ImGui::SameLine();
        if (ImGui::Button("Add End"))
            AddNode("end");

        DrawRuntimeDebug();
        DrawSandbox();

        if (!m_Diagnostics.empty())
        {
            ImGui::Separator();
            ImGui::TextColored({ 1.0f, 0.45f, 0.35f, 1.0f }, "Diagnostics");
            for (const std::string& message : m_Diagnostics)
                ImGui::BulletText("%s", message.c_str());
        }

        YAML::Node node = SelectedNode();
        if (m_SelectedNodeIndices.size() > 1)
        {
            ImGui::TextDisabled("%d nodes selected. Editing the primary node; Copy/Paste/Duplicate/Delete apply to the selection.",
                static_cast<int>(m_SelectedNodeIndices.size()));
        }
        if (node)
        {
            ImGui::Separator();
            ImGui::Text("Node %d", m_SelectedNodeIndex + 1);
            std::string nodeId = ReadString(node, "id");
            if (EditorWidgets::InputString("Node Id", nodeId))
            {
                const std::string oldId = ReadString(node, "id");
                CaptureUndoSnapshot();
                node["id"] = nodeId;
                YAML::Node nodes = behavior["nodes"];
                for (auto other : nodes)
                {
                    for (const char* key : { "next", "true", "false", "onTimeout", "jump" })
                    {
                        if (ReadString(other, key) == oldId)
                            other[key] = nodeId;
                    }
                }
                changed = true;
            }

            std::string operation = Operation(node);
            static const char* operations[] = {
                "call", "branch", "jump", "wait", "waitEvent", "set", "end"
            };
            if (ImGui::BeginCombo("Operation",
                OperationTitle(operation).c_str()))
            {
                for (const char* candidate : operations)
                {
                    const bool selected = operation == candidate;
                    if (ImGui::Selectable(OperationTitle(candidate).c_str(), selected))
                    {
                        CaptureUndoSnapshot();
                        ChangeNodeOperation(node, candidate);
                        operation = candidate;
                        changed = true;
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            auto drawStringField = [&](const char* key, const char* label)
            {
                std::string value = ReadString(node, key);
                if (EditorWidgets::InputString(label, value))
                {
                    CaptureUndoSnapshot();
                    node[key] = value;
                    changed = true;
                }
            };

            auto drawLinkField = [&](const char* key, const char* label)
            {
                std::string value = ReadString(node, key);
                if (EditorWidgets::InputString(label, value))
                {
                    CaptureUndoSnapshot();
                    node[key] = value;
                    changed = true;
                }

                if (!value.empty())
                {
                    ImGui::SameLine();
                    const std::string clearLabel = std::string("Clear##") + key;
                    if (ImGui::SmallButton(clearLabel.c_str()))
                        ClearNodeLink(m_SelectedNodeIndex, key);
                }
            };

            if (operation == "call")
            {
                std::string primitive = ReadString(node, "call");
                if (ImGui::BeginCombo("Primitive",
                    primitive.empty() ? "(select capability)" : primitive.c_str()))
                {
                    for (const auto& definition : WAO::BehaviorPrimitiveRegistry::All())
                    {
                        const bool selected = primitive == definition.Id;
                        const std::string label = definition.DisplayName.empty()
                            ? definition.Id
                            : definition.DisplayName + " [" + definition.Id + "]";
                        if (ImGui::Selectable(label.c_str(), selected))
                        {
                            CaptureUndoSnapshot();
                            node["call"] = definition.Id;
                            primitive = definition.Id;
                            changed = true;
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (ImGui::SmallButton("Reset Ports"))
                {
                    CaptureUndoSnapshot();
                    node.remove("inputs");
                    node.remove("outputs");
                    changed = true;
                }

                const WAO::BehaviorPrimitiveDefinition* definition =
                    FindPrimitive(node);
                if (!definition)
                {
                    ImGui::TextDisabled(
                        "Unknown primitive. Raw YAML remains available below.");
                }
                else
                {
                    const std::vector<WAO::BehaviorPrimitivePort> inputPorts =
                        definition->Inputs;
                    if (!inputPorts.empty())
                    {
                        ImGui::Separator();
                        ImGui::TextUnformatted("Inputs");
                    }

                    for (const WAO::BehaviorPrimitivePort& port : inputPorts)
                    {
                        ImGui::PushID(port.Name.c_str());
                        const bool hasValue = node["inputs"] && node["inputs"][port.Name];
                        const bool isLocal = hasValue &&
                            IsLocalReference(node["inputs"][port.Name]);
                        const char* mode = isLocal ? "Local" : "Literal";

                        ImGui::Text("%s (%s)%s", port.Name.c_str(),
                            PortTypeName(port.Type), port.Required ? " *" : "");
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(86.0f);
                        if (ImGui::BeginCombo("##Mode", mode))
                        {
                            if (ImGui::Selectable("Literal", !isLocal))
                            {
                                CaptureUndoSnapshot();
                                node["inputs"].remove(port.Name);
                                changed = true;
                            }
                            if (ImGui::Selectable("Local", isLocal))
                            {
                                CaptureUndoSnapshot();
                                const std::string local = ReadString(node, "id", "node") +
                                    "_" + port.Name;
                                node["inputs"][port.Name] = "$" + local;
                                changed = true;
                            }
                            ImGui::EndCombo();
                        }

                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(130.0f);
                        if (isLocal)
                        {
                            std::string local = LocalReferenceName(
                                node["inputs"][port.Name]);
                            if (EditorWidgets::InputString("##Local", local))
                            {
                                CaptureUndoSnapshot();
                                node["inputs"][port.Name] = "$" + local;
                                changed = true;
                            }
                        }
                        else
                        {
                            switch (port.Type)
                            {
                            case WAO::BehaviorValueType::Bool:
                            {
                                bool value = hasValue
                                    ? node["inputs"][port.Name].as<bool>(false)
                                    : false;
                                if (ImGui::Checkbox("##Value", &value))
                                {
                                    CaptureUndoSnapshot();
                                    node["inputs"][port.Name] = value;
                                    changed = true;
                                }
                                break;
                            }
                            case WAO::BehaviorValueType::Number:
                            {
                                float value = hasValue
                                    ? node["inputs"][port.Name].as<float>(0.0f)
                                    : 0.0f;
                                if (ImGui::DragFloat("##Value", &value, 0.01f,
                                        -1000000.0f, 1000000.0f, "%.4f"))
                                {
                                    CaptureUndoSnapshot();
                                    node["inputs"][port.Name] = value;
                                    changed = true;
                                }
                                break;
                            }
                            case WAO::BehaviorValueType::String:
                            case WAO::BehaviorValueType::UUID:
                            case WAO::BehaviorValueType::None:
                            default:
                            {
                                std::string value = hasValue
                                    ? node["inputs"][port.Name].as<std::string>("")
                                    : std::string{};
                                if (EditorWidgets::InputString("##Value", value))
                                {
                                    CaptureUndoSnapshot();
                                    node["inputs"][port.Name] = value;
                                    changed = true;
                                }
                                break;
                            }
                            }
                        }

                        if (hasValue)
                        {
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Clear"))
                            {
                                CaptureUndoSnapshot();
                                node["inputs"].remove(port.Name);
                                if (node["inputs"] && node["inputs"].size() == 0)
                                    node.remove("inputs");
                                changed = true;
                            }
                        }
                        ImGui::PopID();
                    }

                    const std::vector<WAO::BehaviorPrimitivePort> outputPorts =
                        definition->Outputs;
                    if (!outputPorts.empty())
                    {
                        ImGui::Separator();
                        ImGui::TextUnformatted("Outputs");
                    }

                    for (const WAO::BehaviorPrimitivePort& port : outputPorts)
                    {
                        ImGui::PushID(port.Name.c_str());
                        ImGui::Text("%s (%s)", port.Name.c_str(),
                            PortTypeName(port.Type));
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(150.0f);
                        std::string local = node["outputs"] && node["outputs"][port.Name]
                            ? node["outputs"][port.Name].as<std::string>("")
                            : std::string{};
                        if (EditorWidgets::InputString("##Local", local))
                        {
                            CaptureUndoSnapshot();
                            if (local.empty())
                            {
                                if (node["outputs"])
                                    node["outputs"].remove(port.Name);
                            }
                            else
                            {
                                if (!node["outputs"] || !node["outputs"].IsMap())
                                    node["outputs"] = YAML::Node(YAML::NodeType::Map);
                                node["outputs"][port.Name] = local;
                            }
                            changed = true;
                        }
                        ImGui::PopID();
                    }
                }

                if (ImGui::CollapsingHeader("Raw Inputs / Outputs"))
                {
                    std::string inputs = node["inputs"] ? YAML::Dump(node["inputs"]) : "{}";
                    if (EditorWidgets::InputMultilineString("Inputs YAML", inputs, { -1.0f, 86.0f }, 4096))
                    {
                        try
                        {
                            const YAML::Node parsed = YAML::Load(inputs);
                            if (parsed.IsMap())
                            {
                                CaptureUndoSnapshot();
                                node["inputs"] = parsed;
                                changed = true;
                            }
                        }
                        catch (...)
                        {
                            m_Status = "Inputs YAML is invalid.";
                        }
                    }

                    std::string outputs = node["outputs"] ? YAML::Dump(node["outputs"]) : "{}";
                    if (EditorWidgets::InputMultilineString("Outputs YAML", outputs, { -1.0f, 86.0f }, 4096))
                    {
                        try
                        {
                            const YAML::Node parsed = YAML::Load(outputs);
                            if (parsed.IsMap())
                            {
                                CaptureUndoSnapshot();
                                node["outputs"] = parsed;
                                changed = true;
                            }
                        }
                        catch (...)
                        {
                            m_Status = "Outputs YAML is invalid.";
                        }
                    }
                }
                drawLinkField("next", "Next");
            }
            else if (operation == "branch")
            {
                drawStringField("branch", "Condition Local");
                drawLinkField("true", "True");
                drawLinkField("false", "False");
            }
            else if (operation == "jump")
            {
                drawLinkField("jump", "Target");
            }
            else if (operation == "wait")
            {
                float seconds = ReadFloat(node, "wait");
                if (ImGui::DragFloat("Seconds", &seconds, 0.01f, 0.0f, 60.0f, "%.3f"))
                {
                    CaptureUndoSnapshot();
                    node["wait"] = std::max(0.0f, seconds);
                    changed = true;
                }
                drawLinkField("next", "Next");
            }
            else if (operation == "waitEvent")
            {
                drawStringField("waitEvent", "Event Id");
                float timeout = ReadFloat(node, "timeout");
                if (ImGui::DragFloat("Timeout", &timeout, 0.01f, 0.0f, 60.0f, "%.3f"))
                {
                    CaptureUndoSnapshot();
                    node["timeout"] = std::max(0.0f, timeout);
                    changed = true;
                }
                drawLinkField("next", "Next");
                drawLinkField("onTimeout", "On Timeout");
            }
            else if (operation == "set")
            {
                drawStringField("set", "Local");
                drawStringField("value", "Value");
                drawLinkField("next", "Next");
            }

            if (ImGui::Button("Delete Node"))
                DeleteSelectedNode();
        }
        else
        {
            ImGui::TextDisabled("Select a node in the graph.");
        }

        if (changed)
            m_Dirty = true;
        ImGui::EndChild();
    }

    void BehaviorGraphPanel::RunSandbox()
    {
        StopSandbox();

        const std::string behaviorId = ReadString(SelectedBehavior(), "id");
        if (behaviorId.empty())
        {
            m_Status = "Select a behavior entry before running the sandbox.";
            return;
        }

        const std::filesystem::path resolved =
            EditorWidgets::ResolveProjectAsset(m_SourcePath);
        WAO::RegisterBuiltinBehaviorPrimitives();

        WAO::BehaviorProgram compiled;
        std::string compileError;
        try
        {
            const YAML::Node root = YAML::LoadFile(resolved.string());
            if (!WAO::BehaviorCompiler::Compile(root, resolved, &compiled, &compileError))
            {
                m_Status = "Sandbox compile failed: " + compileError;
                return;
            }
        }
        catch (const YAML::Exception& exception)
        {
            m_Status = std::string("Sandbox YAML error: ") + exception.what();
            return;
        }

        const auto program = std::make_shared<const WAO::BehaviorProgram>(
            std::move(compiled));

        WAO::ActionRecipe recipe;
        recipe.Id = "sandbox." + behaviorId;
        recipe.BehaviorId = behaviorId;
        recipe.BehaviorEntry = m_SelectedEntry.empty() ? behaviorId : m_SelectedEntry;
        recipe.Duration = 5.0f;

        WAO::ActionIntent intent;
        intent.ActionId = recipe.Id;
        intent.Actor = 1;
        intent.ExplicitTarget = 2;

        m_SandboxRuntime = std::make_shared<WAO::ActionRuntime>();
        m_SandboxRuntime->Attributes.Set("Health", 100.0f);
        m_SandboxRuntime->Attributes.Set("MaxHealth", 100.0f);
        m_SandboxRuntime->Attributes.Set("hp", 100.0f);
        m_SandboxRuntime->Attributes.Set("max_hp", 100.0f);
        m_SandboxRuntime->Attributes.Set("atk", 10.0f);
        m_SandboxRuntime->Attributes.Set("attack", 10.0f);
        m_SandboxRuntime->Attributes.Set("mana", 50.0f);
        m_SandboxRuntime->Resources["mana"] = 50.0f;

        m_SandboxScene = new Scene();
        m_SandboxResult = WAO::BehaviorVM::StartWithProgram(
            m_SandboxScene, intent, recipe, program,
            recipe.BehaviorEntry, m_SandboxRuntime.get());
        m_SandboxHandle = m_SandboxResult.Handle;
        m_SandboxRan = true;
        m_SandboxTime = 0.0f;
        m_Status = m_SandboxResult.Detail.empty()
            ? (m_SandboxResult.Started ? "Sandbox started." : "Sandbox failed to start.")
            : m_SandboxResult.Detail;
    }

    void BehaviorGraphPanel::StopSandbox()
    {
        if (m_SandboxScene)
        {
            WAO::BehaviorVM::StopScene(m_SandboxScene);
            delete m_SandboxScene;
            m_SandboxScene = nullptr;
        }
        m_SandboxRuntime.reset();
        m_SandboxHandle = 0;
        m_SandboxRan = false;
    }

    void BehaviorGraphPanel::DrawSandbox()
    {
        if (m_SandboxScene && m_SandboxRan)
        {
            WAO::BehaviorVM::Update(m_SandboxScene, 1.0f / 60.0f);
            m_SandboxTime += 1.0f / 60.0f;
        }

        if (!ImGui::CollapsingHeader("Behavior Sandbox"))
            return;

        ImGui::TextDisabled("Runs the selected entry in an isolated empty scene.");
        if (m_Dirty)
            ImGui::TextColored(ImVec4(0.95f, 0.72f, 0.30f, 1.0f),
                "Sandbox loads the saved .wtb; save the graph before testing edits.");
        if (ImGui::Button("Run Behavior"))
            RunSandbox();
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_SandboxScene);
        if (ImGui::Button("Stop"))
            StopSandbox();
        ImGui::EndDisabled();

        if (!m_SandboxRan)
        {
            ImGui::TextDisabled("No sandbox run yet.");
            return;
        }

        ImGui::Text("Started: %s  Completed: %s  Success: %s",
            m_SandboxResult.Started ? "yes" : "no",
            m_SandboxResult.Completed ? "yes" : "no",
            m_SandboxResult.Success ? "yes" : "no");
        if (!m_SandboxResult.Detail.empty())
            ImGui::TextWrapped("%s", m_SandboxResult.Detail.c_str());

        if (m_SandboxScene)
        {
            const auto active = WAO::BehaviorVM::Active(m_SandboxScene);
            if (active.empty())
            {
                m_SandboxResult.Completed = true;
                ImGui::TextDisabled("Behavior finished or is waiting for a scene event.");
            }
            else
            {
                for (const WAO::BehaviorDebugSnapshot& snapshot : active)
                {
                    ImGui::BulletText("#%llu %s node=%s status=%s",
                        static_cast<unsigned long long>(snapshot.Handle),
                        snapshot.EntryId.c_str(),
                        snapshot.CurrentNodeId.c_str(),
                        BehaviorStatusText(snapshot.Status));
                }
            }
        }

        if (!m_SandboxResult.Ledger.Entries().empty())
        {
            ImGui::Separator();
            ImGui::TextDisabled("Sandbox Ledger");
            for (const WAO::EffectLedgerEntry& entry : m_SandboxResult.Ledger.Entries())
                ImGui::Text("%s  %s  %.3f", entry.ActionId.c_str(),
                    entry.Detail.c_str(), entry.Value);
        }
    }

    void BehaviorGraphPanel::DrawRuntimeDebug()
    {
        if (!m_DebugScene)
            return;

        // Apply persistent breakpoints even while the debug section is
        // collapsed so new instances still pause at the requested nodes.
        if (!m_Breakpoints.empty())
        {
            for (const WAO::BehaviorDebugSnapshot& snapshot
                : WAO::BehaviorVM::Active(m_DebugScene))
            {
                for (const std::string& breakpoint : m_Breakpoints)
                    WAO::BehaviorVM::SetBreakpoint(
                        m_DebugScene, snapshot.Handle, breakpoint, true);
            }
        }

        if (!ImGui::CollapsingHeader("Runtime Debug", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        const std::vector<WAO::BehaviorDebugSnapshot> snapshots =
            WAO::BehaviorVM::Active(m_DebugScene);
        ImGui::TextDisabled(
            "Active behavior instances: %d",
            static_cast<int>(snapshots.size()));
        if (snapshots.empty())
        {
            ImGui::TextDisabled("No active behavior instances.");
            return;
        }

        for (const WAO::BehaviorDebugSnapshot& snapshot : snapshots)
        {
            ImGui::PushID(static_cast<int>(snapshot.Handle & 0x7fffffff));
            for (const std::string& breakpoint : m_Breakpoints)
                WAO::BehaviorVM::SetBreakpoint(m_DebugScene, snapshot.Handle, breakpoint, true);
            ImGui::Separator();
            ImGui::Text(
                "#%llu  %s / %s",
                static_cast<unsigned long long>(snapshot.Handle),
                snapshot.BehaviorId.c_str(),
                snapshot.EntryId.c_str());
            ImGui::Text(
                "%s  node=%s  pc=%d",
                BehaviorStatusText(snapshot.Status),
                snapshot.CurrentNodeId.empty()
                    ? "(none)"
                    : snapshot.CurrentNodeId.c_str(),
                static_cast<int>(snapshot.ProgramCounter));
            if (!snapshot.WaitingEvent.empty())
                ImGui::TextDisabled(
                    "waiting event: %s",
                    snapshot.WaitingEvent.c_str());
            if (snapshot.WaitRemaining > 0.0f)
                ImGui::TextDisabled(
                    "remaining: %.3fs",
                    snapshot.WaitRemaining);
            if (!snapshot.LastPrimitiveId.empty())
                ImGui::TextDisabled(
                    "last primitive: %s",
                    snapshot.LastPrimitiveId.c_str());
            if (!snapshot.FailureReason.empty())
                ImGui::TextColored(
                    { 1.0f, 0.45f, 0.35f, 1.0f },
                    "failure: %s",
                    snapshot.FailureReason.c_str());
            if (snapshot.Paused)
            {
                ImGui::TextColored(
                    { 1.0f, 0.82f, 0.25f, 1.0f },
                    "PAUSED at %s",
                    snapshot.PausedNodeId.empty()
                        ? "(unknown node)"
                        : snapshot.PausedNodeId.c_str());
            }

            if (ImGui::SmallButton(snapshot.Paused ? "Resume" : "Pause"))
            {
                WAO::BehaviorVM::SetPaused(
                    m_DebugScene, snapshot.Handle, !snapshot.Paused);
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(!snapshot.Paused);
            if (ImGui::SmallButton("Step"))
            {
                WAO::BehaviorVM::Step(m_DebugScene, snapshot.Handle);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(snapshot.CurrentNodeId.empty());
            if (ImGui::SmallButton(snapshot.CurrentNodeBreakpoint
                    ? "Remove Breakpoint"
                    : "Add Breakpoint"))
            {
                const bool enabled = !snapshot.CurrentNodeBreakpoint;
                if (enabled)
                    m_Breakpoints.insert(snapshot.CurrentNodeId);
                else
                    m_Breakpoints.erase(snapshot.CurrentNodeId);
                WAO::BehaviorVM::SetBreakpoint(
                    m_DebugScene,
                    snapshot.Handle,
                    snapshot.CurrentNodeId,
                    enabled);
            }
            ImGui::EndDisabled();

            const std::string selectedNode = ReadString(SelectedNode(), "id");
            const bool sameBehavior = snapshot.BehaviorId == SelectedBehaviorId();
            ImGui::SameLine();
            ImGui::BeginDisabled(selectedNode.empty() || !sameBehavior
                || snapshot.Status == WAO::BehaviorInstanceStatus::Completed);
            if (ImGui::SmallButton("Run From Selected Node"))
            {
                if (WAO::BehaviorVM::JumpToNode(m_DebugScene, snapshot.Handle, selectedNode))
                    WAO::BehaviorVM::SetPaused(m_DebugScene, snapshot.Handle, false);
                else
                    m_Status = "Selected node is not part of this behavior instance.";
            }
            ImGui::EndDisabled();

            if (!snapshot.Locals.empty() &&
                ImGui::CollapsingHeader("Locals"))
            {
                for (const auto& [name, value] : snapshot.Locals)
                    ImGui::Text("%s = %s", name.c_str(),
                        BehaviorValueText(value).c_str());
            }

            if (!snapshot.AppliedEffects.empty() &&
                ImGui::CollapsingHeader("Applied Effects"))
            {
                for (const WAO::EffectSpec& effect : snapshot.AppliedEffects)
                    ImGui::Text("%s  %.3f", EffectTypeLabel(effect.Type),
                        effect.Value);
            }

            if (!snapshot.Ledger.empty() &&
                ImGui::CollapsingHeader("Effect Ledger"))
            {
                for (const WAO::EffectLedgerEntry& entry : snapshot.Ledger)
                {
                    ImGui::Text("%s  %s%s", EffectTypeLabel(entry.Type),
                        entry.Detail.c_str(),
                        entry.Applied ? "" : "  [blocked]");
                }
            }

            if (snapshot.BehaviorId == SelectedBehaviorId() &&
                !snapshot.CurrentNodeId.empty() &&
                ImGui::SmallButton("Select Current Node"))
            {
                YAML::Node behavior = SelectedBehavior();
                const int nodeIndex = FindNodeIndex(
                    behavior["nodes"],
                    snapshot.CurrentNodeId);
                if (nodeIndex >= 0)
                {
                    ClearNodeSelection();
                    m_SelectedNodeIndices.insert(nodeIndex);
                    m_SelectedNodeIndex = nodeIndex;
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Cancel"))
            {
                WAO::BehaviorVM::Cancel(
                    m_DebugScene,
                    snapshot.Handle,
                    "Cancelled by behavior graph debugger");
                m_Status = "Cancelled behavior #" +
                    std::to_string(snapshot.Handle) + ".";
            }
            ImGui::PopID();
        }
    }

    void BehaviorGraphPanel::OnImGuiRender()
    {
        std::string pendingPath;
        std::string pendingEntry;
        if (BehaviorGraphRequests::ConsumeOpenBehaviorRequest(pendingPath, pendingEntry))
            Open(pendingPath, pendingEntry);

        if (!m_Open)
            return;
        if (!m_Loaded && !m_LoadFailed)
            Load();

        if (EditorFloatingWindow::Begin("WAO Behavior Graph", &m_Open, 0, { 1320.0f, 820.0f }))
        {
            EditorFloatingWindow::DrawToggleButton("WAO Behavior Graph");
            ImGui::Separator();
            EditorWidgets::PanelHeader(
                EditorLocale::Text("WAO Behavior Graph", "WAO 行为图"),
                EditorLocale::Text(
                    "Compose registered native capabilities into hot-reloadable behavior programs.",
                    "将已注册的原生能力组合为可热重载的行为程序。"));
            ImGui::Separator();
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
            {
                const ImGuiIO& io = ImGui::GetIO();
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
                    Save();
                if (io.KeyCtrl)
                {
                    if (ImGui::IsKeyPressed(ImGuiKey_Z, false) && !io.WantTextInput)
                    {
                        if (io.KeyShift)
                            Redo();
                        else
                            Undo();
                    }
                    else if (ImGui::IsKeyPressed(ImGuiKey_Y, false) && !io.WantTextInput)
                    {
                        Redo();
                    }
                    else if (ImGui::IsKeyPressed(ImGuiKey_C, false) && !io.WantTextInput)
                    {
                        CopySelectedNode();
                    }
                    else if (ImGui::IsKeyPressed(ImGuiKey_V, false) && !io.WantTextInput)
                    {
                        PasteNode();
                    }
                    else if (ImGui::IsKeyPressed(ImGuiKey_D, false) && !io.WantTextInput)
                    {
                        DuplicateSelectedNode();
                    }
                }
                else if (!io.WantTextInput &&
                    ImGui::IsKeyPressed(ImGuiKey_Delete, false))
                {
                    DeleteSelectedNode();
                }
            }
            DrawToolbar();
            ImGui::Separator();
            if (m_Dirty)
                EditorWidgets::InlineStatus("Unsaved behavior graph changes.", EditorWidgets::StatusKind::Warning);
            if (m_Loaded)
                DrawGraph();
            else
                EditorWidgets::EmptyState("No behavior loaded.", m_Status.c_str());
        }
        EditorFloatingWindow::End();
    }

} // namespace Wheatear
