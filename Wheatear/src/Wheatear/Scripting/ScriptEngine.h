#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/Timestep.h"
#include "Wheatear/Core/UUID.h"
#include "Wheatear/Scene/Entity.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
    typedef struct _MonoClass      MonoClass;
    typedef struct _MonoObject     MonoObject;
    typedef struct _MonoMethod     MonoMethod;
    typedef struct _MonoAssembly   MonoAssembly;
    typedef struct _MonoDomain     MonoDomain;
    typedef struct _MonoImage      MonoImage;
    typedef struct _MonoClassField MonoClassField;
    typedef struct _MonoType       MonoType;
}

namespace Wheatear {

    class Scene;

    enum class ScriptFieldType
    {
        None = 0,
        Float, Double,
        Bool, Char,
        Byte, Short, Int, Long,
        Vector2, Vector3, Vector4,
        String
    };

    struct ScriptField
    {
        ScriptFieldType Type = ScriptFieldType::None;
        std::string Name;
        MonoClassField* ClassField = nullptr;
    };

    struct ScriptFieldInstance
    {
        ScriptField Field;

        template<typename T>
        T GetValue() const
        {
            static_assert(sizeof(T) <= 16, "ScriptFieldInstance: T exceeds 16-byte buffer");
            return *reinterpret_cast<const T*>(m_Buffer);
        }

        template<typename T>
        void SetValue(const T& value)
        {
            static_assert(sizeof(T) <= 16, "ScriptFieldInstance: T exceeds 16-byte buffer");
            memcpy(m_Buffer, &value, sizeof(T));
        }

        const std::string& GetStringValue() const { return m_StringBuffer; }
        void SetStringValue(const std::string& value) { m_StringBuffer = value; }

    private:
        uint8_t m_Buffer[16] = {};
        std::string m_StringBuffer;
    };

    using ScriptFieldMap = std::unordered_map<std::string, ScriptFieldInstance>;

    class ScriptClass
    {
    public:
        ScriptClass() = default;
        ScriptClass(const std::string& classNamespace, const std::string& className);

        MonoObject* Instantiate();
        MonoMethod* GetMethod(const std::string& name, int parameterCount);
        MonoObject* InvokeMethod(MonoObject* instance, MonoMethod* method, void** params = nullptr);

        const std::string& GetNamespace() const { return m_ClassNamespace; }
        const std::string& GetName() const { return m_ClassName; }
        MonoClass* GetMonoClass() const { return m_MonoClass; }

        const std::unordered_map<std::string, ScriptField>& GetFields() const { return m_Fields; }

    private:
        std::string m_ClassNamespace;
        std::string m_ClassName;
        MonoClass* m_MonoClass = nullptr;
        std::unordered_map<std::string, ScriptField> m_Fields;

        friend class ScriptEngine;
    };

    class ScriptInstance
    {
    public:
        ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity);
        ~ScriptInstance();

        void InvokeOnCreate();
        void InvokeOnUpdate(float ts);
        void InvokeOnCollisionEnter(Entity other);
        void InvokeOnCollisionExit(Entity other);

        template<typename T>
        T GetFieldValue(const std::string& name)
        {
            uint8_t buffer[16] = {};
            if (!GetFieldValueInternal(name, buffer))
                return T{};
            return *reinterpret_cast<T*>(buffer);
        }

        template<typename T>
        void SetFieldValue(const std::string& name, const T& value)
        {
            SetFieldValueInternal(name, &value);
        }

        std::string GetStringFieldValue(const std::string& name);
        void SetStringFieldValue(const std::string& name, const std::string& value);

        Ref<ScriptClass> GetScriptClass() const { return m_ScriptClass; }
        MonoObject* GetMonoObject() const;
        MonoClass* GetMonoClass() const { return m_ScriptClass->GetMonoClass(); }

    private:
        bool GetFieldValueInternal(const std::string& name, void* outBuffer);
        bool SetFieldValueInternal(const std::string& name, const void* value);

    private:
        Ref<ScriptClass> m_ScriptClass;
        MonoObject* m_Instance = nullptr;
        uint32_t m_GCHandle = 0;

        MonoMethod* m_OnCreateMethod = nullptr;
        MonoMethod* m_OnUpdateMethod = nullptr;
        MonoMethod* m_OnCollisionEnterMethod = nullptr;
        MonoMethod* m_OnCollisionExitMethod = nullptr;
    };

    class ScriptEngine
    {
    public:
        static void Init();
        static void Shutdown();
        static bool IsInitialized();

        static void LoadAssembly(const std::filesystem::path& filepath);

        static bool EntityClassExists(const std::string& fullClassName);
        static Ref<ScriptClass> GetEntityClass(const std::string& fullClassName);
        static std::vector<std::string> GetEntityClassNames();
        static Ref<ScriptInstance> GetEntityScriptInstance(UUID entityID);
        static Scene* GetSceneContext();
        static MonoImage* GetCoreAssemblyImage();

        static float GetDeltaTime();
        static float GetElapsedTime();
        static uint64_t GetFrameCount();

        static void OnRuntimeStart(Scene* scene);
        static void OnRuntimeUpdate(Timestep ts);
        static void OnRuntimeStop();

        static void OnCreateEntity(Entity entity);
        static void OnUpdateEntity(Entity entity, Timestep ts);
        static void OnDestroyEntity(Entity entity);

        static void OnCollisionBegin(Entity entity, Entity other);
        static void OnCollisionEnd(Entity entity, Entity other);

        static void InvokeMethod(Entity entity, const std::string& methodName);

        static ScriptFieldMap& GetScriptFieldMap(Entity entity);
        static bool HasScriptFieldMap(UUID entityID);
        static void InitializeScriptFieldMap(Entity entity);
        static void ClearScriptFieldMap(Entity entity);

    private:
        static void InitMono();
        static void ShutdownMono();
        static void LoadEntityClasses();
    };

} // namespace Wheatear
