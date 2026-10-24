#pragma once

#include "BehaviorTypes.h"
#include "Wheatear/Core/Core.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear {

    class Scene;

    namespace WAO {

        enum class BehaviorPrimitiveCategory
        {
            Query = 0,
            Effect,
            Flow,
            Presentation
        };

        enum class BehaviorThreadConstraint
        {
            MainThread = 0,
            GameplayThread,
            AnyThread
        };

        struct BehaviorPrimitivePort
        {
            std::string Name;
            BehaviorValueType Type = BehaviorValueType::None;
            bool Required = false;
        };

        struct BehaviorPrimitiveContext
        {
            Scene* SceneContext = nullptr;
            BehaviorInstance* Instance = nullptr;
            const ActionRecipe* Recipe = nullptr;
            ActionIntent Intent;
            ActionRuntime* Runtime = nullptr;
            const std::unordered_map<std::string, BehaviorValue>* Inputs = nullptr;
            std::unordered_map<std::string, BehaviorValue>* Outputs = nullptr;
            EffectBundle* AppliedEffects = nullptr;
            EffectLedger* Ledger = nullptr;
            std::function<void(const BehaviorEvent&)> EmitEvent;
            std::string FailureReason;

            const BehaviorValue* FindInput(const std::string& name) const;
            BehaviorValue Input(const std::string& name, const BehaviorValue& fallback = {}) const;
            void SetOutput(const std::string& name, const BehaviorValue& value);
            void Fail(const std::string& reason);
        };

        using BehaviorPrimitiveCallback = std::function<bool(BehaviorPrimitiveContext&)>;

        struct BehaviorPrimitiveDefinition
        {
            std::string Id;
            std::string DisplayName;
            uint32_t Version = 1;
            std::vector<BehaviorPrimitivePort> Inputs;
            std::vector<BehaviorPrimitivePort> Outputs;
            BehaviorPrimitiveCategory Category = BehaviorPrimitiveCategory::Effect;
            BehaviorThreadConstraint Thread = BehaviorThreadConstraint::MainThread;
            bool AllowHotReload = true;
            BehaviorPrimitiveCallback Callback;
        };

        class WHEATEAR_API BehaviorPrimitiveRegistry
        {
        public:
            static void Register(BehaviorPrimitiveDefinition definition);
            static void Clear();
            static bool Has(const std::string& id);
            static const BehaviorPrimitiveDefinition* Find(const std::string& id);
            static std::vector<BehaviorPrimitiveDefinition> All();
            static bool Execute(const std::string& id, BehaviorPrimitiveContext& context);
        };

    } // namespace WAO

} // namespace Wheatear
