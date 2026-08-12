#pragma once

#include "Wheatear/Core/UUID.h"
#include "Wheatear/Scene/Entity.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Wheatear {

    enum class UITemplateKind
    {
        Unknown = 0,
        TitledScrollText,
        PagedGrid,
        PagedInventoryGrid,
        SkillButton,
        EquipmentSlot,
        Tooltip,
        SaveSlot,
        SkillTreeNode,
        CombatSkillSlot,
        // Designer-authored composite carrying an embedded Prefab v2 body in the
        // .wtuit file instead of a C++ builder; recognized by KindFromString but
        // intentionally has no builtin descriptor (no fixed asset path/builder).
        Composite
    };

    struct UITemplateDescriptor
    {
        UITemplateKind Kind = UITemplateKind::Unknown;
        std::string KindName;
        std::string DisplayName;
        std::string Category;
        std::string Description;
        std::string DefaultAssetPath;
    };

    class UITemplateFactory
    {
    public:
        static std::vector<UITemplateDescriptor> GetBuiltinTemplates();
        static const UITemplateDescriptor* FindBuiltinTemplate(UITemplateKind kind);
        static UITemplateKind KindFromString(const std::string& value);
        static std::string KindToString(UITemplateKind kind);

        static std::vector<Entity> Create(Scene* scene,
            UITemplateKind kind,
            UUID parentID,
            const std::string& namePrefix = {});

        static std::vector<Entity> CreateFromAsset(Scene* scene,
            const std::filesystem::path& assetPath,
            UUID parentID);

        static bool WriteBuiltinTemplateAssets(const std::filesystem::path& projectRoot = {});
    };

} // namespace Wheatear
