#include "wtpch.h"
#include "ScriptEngine.h"

#if defined(WT_ENABLE_CSHARP_SCRIPTING)
#include "ScriptGlue.h"

#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/EngineInfo.h"

#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Components.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/object.h>
#include <mono/metadata/tabledefs.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/threads.h>
#include <algorithm>
#include <vector>

namespace Wheatear {


    struct ScriptEngineData
    {
        MonoDomain* RootDomain = nullptr;
        MonoDomain* AppDomain = nullptr;
        MonoAssembly* CoreAssembly = nullptr;
        MonoImage* CoreAssemblyImage = nullptr;

        std::unordered_map<std::string, Ref<ScriptClass>> EntityClasses;

        Scene* SceneContext = nullptr;
        std::unordered_map<UUID, Ref<ScriptInstance>> EntityInstances;
        float LastDeltaTime = 0.0f;
        float ElapsedTime = 0.0f;
        uint64_t FrameCount = 0;
    };

    static ScriptEngineData* s_Data = nullptr;

    static std::unordered_map<UUID, ScriptFieldMap> s_EntityScriptFields;

    static MonoAssembly* LoadMonoAssembly(const std::filesystem::path& filepath)
    {
        std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
        WT_CORE_ASSERT(stream, "Failed to open assembly file: {}", filepath.string());

        auto end = stream.tellg();
        stream.seekg(0, std::ios::beg);
        uint32_t size = static_cast<uint32_t>(end - stream.tellg());

        std::vector<char> buffer(size);
        stream.read(buffer.data(), size);
        stream.close();

        MonoImageOpenStatus status;
        MonoImage* image = mono_image_open_from_data_full(
            buffer.data(), size, true, &status, false);
        WT_CORE_ASSERT(status == MONO_IMAGE_OK, mono_image_strerror(status));

        MonoAssembly* assembly = mono_assembly_load_from_full(
            image, filepath.string().c_str(), &status, false);
        mono_image_close(image);
        return assembly;
    }

    static MonoMethod* FindMethodWithULongParam(MonoClass* klass, const char* methodName)
    {
        for (MonoClass* current = klass; current; current = mono_class_get_parent(current))
        {
            void* iter = nullptr;
            MonoMethod* method = nullptr;
            while ((method = mono_class_get_methods(current, &iter)) != nullptr)
            {
                if (strcmp(mono_method_get_name(method), methodName) != 0)
                    continue;

                MonoMethodSignature* sig = mono_method_signature(method);
                if (mono_signature_get_param_count(sig) != 1)
                    continue;

                void* paramIter = nullptr;
                MonoType* paramType = mono_signature_get_params(sig, &paramIter);
                if (mono_type_get_type(paramType) == MONO_TYPE_U8)
                    return method;
            }
        }
        return nullptr;
    }

    static ScriptFieldType MonoTypeToScriptFieldType(MonoType* monoType)
    {
        static const std::unordered_map<std::string, ScriptFieldType> s_TypeMap =
        {
            { "System.Single",  ScriptFieldType::Float   },
            { "System.Double",  ScriptFieldType::Double  },
            { "System.Boolean", ScriptFieldType::Bool    },
            { "System.Char",    ScriptFieldType::Char    },
            { "System.Byte",    ScriptFieldType::Byte    },
            { "System.Int16",   ScriptFieldType::Short   },
            { "System.Int32",   ScriptFieldType::Int     },
            { "System.Int64",   ScriptFieldType::Long    },
            { "System.String",  ScriptFieldType::String  },
            { "Wheatear.Vector2",  ScriptFieldType::Vector2 },
            { "Wheatear.Vector3",  ScriptFieldType::Vector3 },
            { "Wheatear.Vector4",  ScriptFieldType::Vector4 },
            { "System.Numerics.Vector4", ScriptFieldType::Vector4 },
        };

        std::string typeName = mono_type_get_name(monoType);
        auto it = s_TypeMap.find(typeName);
        return it != s_TypeMap.end() ? it->second : ScriptFieldType::None;
    }


    void ScriptEngine::Init()
    {
        if (s_Data)
            return;

        s_Data = new ScriptEngineData();
        InitMono();
        LoadAssembly(AssetPath::Resolve(EngineInfo::ScriptCoreAssemblyPath));
        ScriptGlue::RegisterFunctions();
    }

    void ScriptEngine::Shutdown()
    {
        if (!s_Data)
            return;

        s_Data->EntityInstances.clear();
        ShutdownMono();
        delete s_Data;
        s_Data = nullptr;
    }

    bool ScriptEngine::IsInitialized()
    {
        return s_Data && s_Data->RootDomain;
    }

    void ScriptEngine::InitMono()
    {
        static std::string s_MonoAssemblyPath;
        s_MonoAssemblyPath = AssetPath::Resolve("mono/lib").string();
        mono_set_assemblies_path(s_MonoAssemblyPath.c_str());

        MonoDomain* rootDomain = mono_jit_init("WheatearJITRuntime");
        WT_CORE_ASSERT(rootDomain, "Failed to initialize Mono JIT");
        s_Data->RootDomain = rootDomain;
    }

    void ScriptEngine::ShutdownMono()
    {
        if (!s_Data || !s_Data->RootDomain)
            return;

        if (s_Data->AppDomain)
        {
            mono_domain_set(mono_get_root_domain(), false);
            mono_domain_unload(s_Data->AppDomain);
            s_Data->AppDomain = nullptr;
        }
        mono_jit_cleanup(s_Data->RootDomain);
        s_Data->RootDomain = nullptr;
    }

    void ScriptEngine::LoadAssembly(const std::filesystem::path& filepath)
    {
        s_Data->AppDomain = mono_domain_create_appdomain(
            const_cast<char*>("WheatearScriptRuntime"), nullptr);
        mono_domain_set(s_Data->AppDomain, true);

        s_Data->CoreAssembly = LoadMonoAssembly(filepath);
        s_Data->CoreAssemblyImage = mono_assembly_get_image(s_Data->CoreAssembly);

        LoadEntityClasses();
    }


    void ScriptEngine::OnRuntimeStart(Scene* scene)
    {
        s_Data->SceneContext = scene;
        s_Data->LastDeltaTime = 0.0f;
        s_Data->ElapsedTime = 0.0f;
        s_Data->FrameCount = 0;
    }

    void ScriptEngine::OnRuntimeUpdate(Timestep ts)
    {
        s_Data->LastDeltaTime = ts.GetSeconds();
        s_Data->ElapsedTime += ts.GetSeconds();
        s_Data->FrameCount++;
    }

    void ScriptEngine::OnRuntimeStop()
    {
        s_Data->SceneContext = nullptr;
        s_Data->EntityInstances.clear();
    }


    void ScriptEngine::OnCreateEntity(Entity entity)
    {
        const auto& sc = entity.GetComponent<ScriptComponent>();
        if (!EntityClassExists(sc.ClassName))
            return;

        UUID entityID = entity.GetUUID();

        Ref<ScriptClass>    scriptClass = GetEntityClass(sc.ClassName);
        Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(scriptClass, entity);
        s_Data->EntityInstances[entityID] = instance;

        auto fieldsIt = s_EntityScriptFields.find(entityID);
        if (fieldsIt != s_EntityScriptFields.end())
        {
            const auto& classFields = scriptClass->GetFields();
            for (auto& [name, fieldInst] : fieldsIt->second)
            {
                auto classFieldIt = classFields.find(name);
                if (classFieldIt == classFields.end())
                    continue;

                fieldInst.Field = classFieldIt->second;
                switch (fieldInst.Field.Type)
                {
                case ScriptFieldType::Float:
                    instance->SetFieldValue<float>(name, fieldInst.GetValue<float>());
                    break;
                case ScriptFieldType::Double:
                    instance->SetFieldValue<double>(name, fieldInst.GetValue<double>());
                    break;
                case ScriptFieldType::Bool:
                    instance->SetFieldValue<bool>(name, fieldInst.GetValue<bool>());
                    break;
                case ScriptFieldType::Byte:
                    instance->SetFieldValue<uint8_t>(name, fieldInst.GetValue<uint8_t>());
                    break;
                case ScriptFieldType::Short:
                    instance->SetFieldValue<int16_t>(name, fieldInst.GetValue<int16_t>());
                    break;
                case ScriptFieldType::Int:
                    instance->SetFieldValue<int32_t>(name, fieldInst.GetValue<int32_t>());
                    break;
                case ScriptFieldType::Long:
                    instance->SetFieldValue<int64_t>(name, fieldInst.GetValue<int64_t>());
                    break;
                case ScriptFieldType::Vector2:
                    instance->SetFieldValue<glm::vec2>(name, fieldInst.GetValue<glm::vec2>());
                    break;
                case ScriptFieldType::Vector3:
                    instance->SetFieldValue<glm::vec3>(name, fieldInst.GetValue<glm::vec3>());
                    break;
                case ScriptFieldType::Vector4:
                    instance->SetFieldValue<glm::vec4>(name, fieldInst.GetValue<glm::vec4>());
                    break;
                case ScriptFieldType::String:
                    instance->SetStringFieldValue(name, fieldInst.GetStringValue());
                    break;
                default:
                    break;
                }
            }
        }

        instance->InvokeOnCreate();
    }

    void ScriptEngine::OnUpdateEntity(Entity entity, Timestep ts)
    {
        UUID uuid = entity.GetUUID();
        auto it = s_Data->EntityInstances.find(uuid);
        if (it == s_Data->EntityInstances.end())
        {
            WT_CORE_ERROR("ScriptEngine::OnUpdateEntity - no instance for UUID {}", (uint64_t)uuid);
            return;
        }
        it->second->InvokeOnUpdate(ts);
    }

    void ScriptEngine::OnDestroyEntity(Entity entity)
    {
        s_Data->EntityInstances.erase(entity.GetUUID());
    }


    void ScriptEngine::OnCollisionBegin(Entity entity, Entity other)
    {
        if (!entity.HasComponent<ScriptComponent>()) return;

        auto it = s_Data->EntityInstances.find(entity.GetUUID());
        if (it == s_Data->EntityInstances.end()) return;

        it->second->InvokeOnCollisionEnter(other);
    }

    void ScriptEngine::OnCollisionEnd(Entity entity, Entity other)
    {
        if (!entity.HasComponent<ScriptComponent>()) return;

        auto it = s_Data->EntityInstances.find(entity.GetUUID());
        if (it == s_Data->EntityInstances.end()) return;

        it->second->InvokeOnCollisionExit(other);
    }


    void ScriptEngine::InvokeMethod(Entity entity, const std::string& methodName)
    {
        UUID id = entity.GetUUID();
        auto it = s_Data->EntityInstances.find(id);
        if (it == s_Data->EntityInstances.end()) return;

        Ref<ScriptInstance> instance = it->second;
        MonoClass* klass = instance->GetMonoClass();

        MonoMethod* method = mono_class_get_method_from_name(klass, methodName.c_str(), 0);
        if (!method)
        {
            WT_CORE_WARN("ScriptEngine::InvokeMethod - '{}' not found on script", methodName);
            return;
        }

        MonoObject* exception = nullptr;
        mono_runtime_invoke(method, instance->GetMonoObject(), nullptr, &exception);
        if (exception)
            WT_CORE_ERROR("ScriptEngine::InvokeMethod - exception in '{}'", methodName);
    }


    ScriptFieldMap& ScriptEngine::GetScriptFieldMap(Entity entity)
    {
        return s_EntityScriptFields[entity.GetUUID()];
    }

    bool ScriptEngine::HasScriptFieldMap(UUID entityID)
    {
        return s_EntityScriptFields.count(entityID) > 0;
    }


    void ScriptEngine::ClearScriptFieldMap(Entity entity)
    {
        s_EntityScriptFields.erase(entity.GetUUID());
    }

    void ScriptEngine::InitializeScriptFieldMap(Entity entity)
    {
        if (!s_Data || !entity || !entity.HasComponent<ScriptComponent>())
            return;

        const auto& scriptComponent = entity.GetComponent<ScriptComponent>();
        Ref<ScriptClass> scriptClass = GetEntityClass(scriptComponent.ClassName);
        if (!scriptClass)
            return;

        Ref<ScriptInstance> runningInstance = GetEntityScriptInstance(entity.GetUUID());
        Ref<ScriptInstance> defaultInstance = runningInstance ? nullptr : CreateRef<ScriptInstance>(scriptClass, entity);
        Ref<ScriptInstance> sourceInstance = runningInstance ? runningInstance : defaultInstance;

        auto& entityFields = s_EntityScriptFields[entity.GetUUID()];
        for (const auto& [name, field] : scriptClass->GetFields())
        {
            ScriptFieldInstance& fieldInst = entityFields[name];
            const bool needsDefaultValue = fieldInst.Field.Name.empty();
            fieldInst.Field = field;

            if (!needsDefaultValue || !sourceInstance)
                continue;

            switch (field.Type)
            {
            case ScriptFieldType::Float:
                fieldInst.SetValue(sourceInstance->GetFieldValue<float>(name));
                break;
            case ScriptFieldType::Double:
                fieldInst.SetValue(sourceInstance->GetFieldValue<double>(name));
                break;
            case ScriptFieldType::Bool:
                fieldInst.SetValue(sourceInstance->GetFieldValue<bool>(name));
                break;
            case ScriptFieldType::Byte:
                fieldInst.SetValue(sourceInstance->GetFieldValue<uint8_t>(name));
                break;
            case ScriptFieldType::Short:
                fieldInst.SetValue(sourceInstance->GetFieldValue<int16_t>(name));
                break;
            case ScriptFieldType::Int:
                fieldInst.SetValue(sourceInstance->GetFieldValue<int32_t>(name));
                break;
            case ScriptFieldType::Long:
                fieldInst.SetValue(sourceInstance->GetFieldValue<int64_t>(name));
                break;
            case ScriptFieldType::Vector2:
                fieldInst.SetValue(sourceInstance->GetFieldValue<glm::vec2>(name));
                break;
            case ScriptFieldType::Vector3:
                fieldInst.SetValue(sourceInstance->GetFieldValue<glm::vec3>(name));
                break;
            case ScriptFieldType::Vector4:
                fieldInst.SetValue(sourceInstance->GetFieldValue<glm::vec4>(name));
                break;
            case ScriptFieldType::String:
                fieldInst.SetStringValue(sourceInstance->GetStringFieldValue(name));
                break;
            default:
                break;
            }
        }
    }

    bool ScriptEngine::EntityClassExists(const std::string& fullClassName)
    {
        return s_Data && s_Data->EntityClasses.count(fullClassName) > 0;
    }

    Ref<ScriptClass> ScriptEngine::GetEntityClass(const std::string& fullClassName)
    {
        if (!s_Data)
            return nullptr;
        auto it = s_Data->EntityClasses.find(fullClassName);
        return it != s_Data->EntityClasses.end() ? it->second : nullptr;
    }

    std::vector<std::string> ScriptEngine::GetEntityClassNames()
    {
        std::vector<std::string> names;
        if (!s_Data)
            return names;

        names.reserve(s_Data->EntityClasses.size());
        for (const auto& [name, scriptClass] : s_Data->EntityClasses)
            names.push_back(name);
        std::sort(names.begin(), names.end());
        return names;
    }

    Ref<ScriptInstance> ScriptEngine::GetEntityScriptInstance(UUID entityID)
    {
        if (!s_Data)
            return nullptr;
        auto it = s_Data->EntityInstances.find(entityID);
        return it != s_Data->EntityInstances.end() ? it->second : nullptr;
    }

    Scene* ScriptEngine::GetSceneContext() { return s_Data ? s_Data->SceneContext : nullptr; }
    MonoImage* ScriptEngine::GetCoreAssemblyImage() { return s_Data ? s_Data->CoreAssemblyImage : nullptr; }

    float ScriptEngine::GetDeltaTime() { return s_Data ? s_Data->LastDeltaTime : 0.0f; }
    float ScriptEngine::GetElapsedTime() { return s_Data ? s_Data->ElapsedTime : 0.0f; }
    uint64_t ScriptEngine::GetFrameCount() { return s_Data ? s_Data->FrameCount : 0; }


    void ScriptEngine::LoadEntityClasses()
    {
        s_Data->EntityClasses.clear();

        MonoImage* image = s_Data->CoreAssemblyImage;
        MonoClass* entityClass = mono_class_from_name(image, "Wheatear", "Entity");

        const MonoTableInfo* table = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
        int32_t              numRows = mono_table_info_get_rows(table);

        for (int32_t i = 0; i < numRows; i++)
        {
            uint32_t cols[MONO_TYPEDEF_SIZE];
            mono_metadata_decode_row(table, i, cols, MONO_TYPEDEF_SIZE);

            const char* ns = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
            const char* name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);

            MonoClass* monoClass = mono_class_from_name(image, ns, name);
            if (!monoClass || monoClass == entityClass)
                continue;
            if (!mono_class_is_subclass_of(monoClass, entityClass, false))
                continue;

            std::string fullName = (strlen(ns) > 0)
                ? std::string(ns) + "." + name
                : name;

            s_Data->EntityClasses[fullName] = CreateRef<ScriptClass>(ns, name);
            WT_CORE_TRACE("ScriptEngine: found class '{}'", fullName);
        }
    }


    ScriptClass::ScriptClass(const std::string& classNamespace, const std::string& className)
        : m_ClassNamespace(classNamespace), m_ClassName(className)
    {
        m_MonoClass = mono_class_from_name(
            s_Data->CoreAssemblyImage,
            classNamespace.c_str(),
            className.c_str());

        void* iter = nullptr;
        while (MonoClassField* field = mono_class_get_fields(m_MonoClass, &iter))
        {
            if (!(mono_field_get_flags(field) & FIELD_ATTRIBUTE_PUBLIC))
                continue;

            const char* fieldName = mono_field_get_name(field);
            MonoType* fieldType = mono_field_get_type(field);

            ScriptFieldType type = MonoTypeToScriptFieldType(fieldType);
            if (type == ScriptFieldType::None)
                continue;

            m_Fields[fieldName] = { type, fieldName, field };
        }
    }

    MonoObject* ScriptClass::Instantiate()
    {
        return mono_object_new(s_Data->AppDomain, m_MonoClass);
    }

    MonoMethod* ScriptClass::GetMethod(const std::string& name, int parameterCount)
    {
        for (MonoClass* klass = m_MonoClass; klass; klass = mono_class_get_parent(klass))
        {
            MonoMethod* method = mono_class_get_method_from_name(
                klass, name.c_str(), parameterCount);
            if (method)
                return method;
        }
        return nullptr;
    }

    MonoObject* ScriptClass::InvokeMethod(MonoObject* instance, MonoMethod* method, void** params)
    {
        MonoObject* exception = nullptr;
        MonoObject* result = mono_runtime_invoke(method, instance, params, &exception);
        if (exception)
        {
            MonoString* msg = mono_object_to_string(exception, nullptr);
            char* cStr = msg ? mono_string_to_utf8(msg) : nullptr;
            WT_CORE_ERROR("C# Exception in {}.{}: {}",
                m_ClassNamespace, m_ClassName, cStr ? cStr : "<exception>");
            mono_free(cStr);
        }
        return result;
    }


    ScriptInstance::ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity)
        : m_ScriptClass(scriptClass)
    {
        m_Instance = scriptClass->Instantiate();
        WT_CORE_ASSERT(m_Instance, "Failed to instantiate C# script object '{}.{}'",
            scriptClass->GetNamespace(), scriptClass->GetName());
        m_GCHandle = mono_gchandle_new(m_Instance, false);

        m_OnCreateMethod = scriptClass->GetMethod("OnCreate", 0);
        m_OnUpdateMethod = scriptClass->GetMethod("OnUpdate", 1);

        m_OnCollisionEnterMethod = FindMethodWithULongParam(
            scriptClass->GetMonoClass(), "OnCollisionEnter");
        m_OnCollisionExitMethod = FindMethodWithULongParam(
            scriptClass->GetMonoClass(), "OnCollisionExit");

        if (!m_OnCollisionEnterMethod)
            WT_CORE_WARN("ScriptInstance: OnCollisionEnter(ulong) not found in '{}'",
                scriptClass->GetName());

        MonoMethod* scriptConstructor = mono_class_get_method_from_name(scriptClass->GetMonoClass(), ".ctor", 0);
        if (scriptConstructor)
        {
            MonoObject* exception = nullptr;
            mono_runtime_invoke(scriptConstructor, GetMonoObject(), nullptr, &exception);
            if (exception)
                WT_CORE_ERROR("ScriptInstance: exception while constructing '{}'", scriptClass->GetName());
        }

        MonoClass* entityClass = mono_class_from_name(s_Data->CoreAssemblyImage, "Wheatear", "Entity");
        MonoMethod* setRuntimeIDMethod = mono_class_get_method_from_name(entityClass, "SetRuntimeID", 1);
        WT_CORE_ASSERT(setRuntimeIDMethod, "Entity.SetRuntimeID(ulong) not found in core assembly!");

        uint64_t entityID = static_cast<uint64_t>(entity.GetUUID());
        void* idParam = &entityID;
        mono_runtime_invoke(setRuntimeIDMethod, GetMonoObject(), &idParam, nullptr);
    }

    ScriptInstance::~ScriptInstance()
    {
        if (m_GCHandle)
        {
            mono_gchandle_free(m_GCHandle);
            m_GCHandle = 0;
        }
    }

    MonoObject* ScriptInstance::GetMonoObject() const
    {
        return m_GCHandle ? mono_gchandle_get_target(m_GCHandle) : m_Instance;
    }

    void ScriptInstance::InvokeOnCreate()
    {
        if (m_OnCreateMethod)
            m_ScriptClass->InvokeMethod(GetMonoObject(), m_OnCreateMethod);
    }

    void ScriptInstance::InvokeOnUpdate(float ts)
    {
        if (m_OnUpdateMethod)
        {
            void* param = &ts;
            m_ScriptClass->InvokeMethod(GetMonoObject(), m_OnUpdateMethod, &param);
        }
    }

    void ScriptInstance::InvokeOnCollisionEnter(Entity other)
    {
        if (!m_OnCollisionEnterMethod) return;
        UUID  otherID = other.GetUUID();
        void* param = &otherID;
        m_ScriptClass->InvokeMethod(GetMonoObject(), m_OnCollisionEnterMethod, &param);
    }

    void ScriptInstance::InvokeOnCollisionExit(Entity other)
    {
        if (!m_OnCollisionExitMethod) return;
        UUID  otherID = other.GetUUID();
        void* param = &otherID;
        m_ScriptClass->InvokeMethod(GetMonoObject(), m_OnCollisionExitMethod, &param);
    }

    std::string ScriptInstance::GetStringFieldValue(const std::string& name)
    {
        const auto& fields = m_ScriptClass->GetFields();
        auto it = fields.find(name);
        if (it == fields.end())
            return {};

        MonoString* monoValue = nullptr;
        mono_field_get_value(GetMonoObject(), it->second.ClassField, &monoValue);
        if (!monoValue)
            return {};

        char* cStr = mono_string_to_utf8(monoValue);
        std::string value = cStr ? cStr : "";
        mono_free(cStr);
        return value;
    }

    void ScriptInstance::SetStringFieldValue(const std::string& name, const std::string& value)
    {
        const auto& fields = m_ScriptClass->GetFields();
        auto it = fields.find(name);
        if (it == fields.end())
            return;

        MonoString* monoValue = mono_string_new(s_Data->AppDomain, value.c_str());
        mono_field_set_value(GetMonoObject(), it->second.ClassField, &monoValue);
    }

    bool ScriptInstance::GetFieldValueInternal(const std::string& name, void* outBuffer)
    {
        const auto& fields = m_ScriptClass->GetFields();
        auto it = fields.find(name);
        if (it == fields.end())
            return false;
        mono_field_get_value(GetMonoObject(), it->second.ClassField, outBuffer);
        return true;
    }

    bool ScriptInstance::SetFieldValueInternal(const std::string& name, const void* value)
    {
        const auto& fields = m_ScriptClass->GetFields();
        auto it = fields.find(name);
        if (it == fields.end())
            return false;
        mono_field_set_value(GetMonoObject(), it->second.ClassField, const_cast<void*>(value));
        return true;
    }

} // namespace Wheatear
#else

namespace Wheatear {

    namespace {

        struct DisabledScriptEngineData
        {
            Scene* SceneContext = nullptr;
            float LastDeltaTime = 0.0f;
            float ElapsedTime = 0.0f;
            uint64_t FrameCount = 0;
        };

        static DisabledScriptEngineData s_DisabledData;
        static std::unordered_map<UUID, ScriptFieldMap> s_EntityScriptFields;

    } // namespace

    ScriptClass::ScriptClass(const std::string& classNamespace, const std::string& className)
        : m_ClassNamespace(classNamespace), m_ClassName(className)
    {
    }

    MonoObject* ScriptClass::Instantiate()
    {
        return nullptr;
    }

    MonoMethod* ScriptClass::GetMethod(const std::string&, int)
    {
        return nullptr;
    }

    MonoObject* ScriptClass::InvokeMethod(MonoObject*, MonoMethod*, void**)
    {
        return nullptr;
    }

    ScriptInstance::ScriptInstance(Ref<ScriptClass> scriptClass, Entity)
        : m_ScriptClass(std::move(scriptClass))
    {
    }

    ScriptInstance::~ScriptInstance() {}

    MonoObject* ScriptInstance::GetMonoObject() const
    {
        return nullptr;
    }

    void ScriptInstance::InvokeOnCreate() {}
    void ScriptInstance::InvokeOnUpdate(float) {}
    void ScriptInstance::InvokeOnCollisionEnter(Entity) {}
    void ScriptInstance::InvokeOnCollisionExit(Entity) {}

    std::string ScriptInstance::GetStringFieldValue(const std::string&)
    {
        return {};
    }

    void ScriptInstance::SetStringFieldValue(const std::string&, const std::string&) {}

    bool ScriptInstance::GetFieldValueInternal(const std::string&, void*)
    {
        return false;
    }

    bool ScriptInstance::SetFieldValueInternal(const std::string&, const void*)
    {
        return false;
    }

    void ScriptEngine::Init()
    {
        WT_CORE_INFO("C# scripting is disabled for this build. Define WT_ENABLE_CSHARP_SCRIPTING to enable Mono.");
    }

    void ScriptEngine::Shutdown() {}

    bool ScriptEngine::IsInitialized()
    {
        return false;
    }

    void ScriptEngine::LoadAssembly(const std::filesystem::path&) {}

    bool ScriptEngine::EntityClassExists(const std::string&)
    {
        return false;
    }

    Ref<ScriptClass> ScriptEngine::GetEntityClass(const std::string&)
    {
        return nullptr;
    }

    std::vector<std::string> ScriptEngine::GetEntityClassNames()
    {
        return {};
    }

    Ref<ScriptInstance> ScriptEngine::GetEntityScriptInstance(UUID)
    {
        return nullptr;
    }

    Scene* ScriptEngine::GetSceneContext()
    {
        return s_DisabledData.SceneContext;
    }

    MonoImage* ScriptEngine::GetCoreAssemblyImage()
    {
        return nullptr;
    }

    float ScriptEngine::GetDeltaTime()
    {
        return s_DisabledData.LastDeltaTime;
    }

    float ScriptEngine::GetElapsedTime()
    {
        return s_DisabledData.ElapsedTime;
    }

    uint64_t ScriptEngine::GetFrameCount()
    {
        return s_DisabledData.FrameCount;
    }

    void ScriptEngine::OnRuntimeStart(Scene* scene)
    {
        s_DisabledData.SceneContext = scene;
        s_DisabledData.LastDeltaTime = 0.0f;
        s_DisabledData.ElapsedTime = 0.0f;
        s_DisabledData.FrameCount = 0;
    }

    void ScriptEngine::OnRuntimeUpdate(Timestep ts)
    {
        s_DisabledData.LastDeltaTime = ts.GetSeconds();
        s_DisabledData.ElapsedTime += ts.GetSeconds();
        s_DisabledData.FrameCount++;
    }

    void ScriptEngine::OnRuntimeStop()
    {
        s_DisabledData.SceneContext = nullptr;
    }

    void ScriptEngine::OnCreateEntity(Entity) {}
    void ScriptEngine::OnUpdateEntity(Entity, Timestep) {}

    void ScriptEngine::OnDestroyEntity(Entity entity)
    {
        if (entity)
            s_EntityScriptFields.erase(entity.GetUUID());
    }

    void ScriptEngine::OnCollisionBegin(Entity, Entity) {}
    void ScriptEngine::OnCollisionEnd(Entity, Entity) {}
    void ScriptEngine::InvokeMethod(Entity, const std::string&) {}

    ScriptFieldMap& ScriptEngine::GetScriptFieldMap(Entity entity)
    {
        return s_EntityScriptFields[entity.GetUUID()];
    }

    bool ScriptEngine::HasScriptFieldMap(UUID entityID)
    {
        return s_EntityScriptFields.find(entityID) != s_EntityScriptFields.end();
    }

    void ScriptEngine::InitializeScriptFieldMap(Entity entity)
    {
        if (entity)
            s_EntityScriptFields.try_emplace(entity.GetUUID());
    }

    void ScriptEngine::ClearScriptFieldMap(Entity entity)
    {
        if (entity)
            s_EntityScriptFields.erase(entity.GetUUID());
    }

    void ScriptEngine::InitMono() {}
    void ScriptEngine::ShutdownMono() {}
    void ScriptEngine::LoadEntityClasses() {}

} // namespace Wheatear

#endif
