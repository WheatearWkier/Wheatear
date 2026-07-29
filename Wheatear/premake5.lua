project "Wheatear"
    kind "StaticLib"
    wt_cpp_defaults()

    pchheader "wtpch.h"
    pchsource "src/wtpch.cpp"

    files {
        "src/**.h",
        "src/**.cpp",
        "vendor/stb_image/**.h",
        "vendor/stb_image/**.cpp",
        "vendor/glm/glm/**.hpp",
        "vendor/glm/glm/**.inl",
        "vendor/ImGuizmo/ImGuizmo.h",
        "vendor/ImGuizmo/ImGuizmo.cpp",
    }

    includedirs {
        "src",
        "vendor/spdlog/include",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.GLAD}",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.stb_image}",
        "%{IncludeDir.entt}",
        "%{IncludeDir.tinyobjloader}",
        "%{IncludeDir.yaml_cpp}",
        "%{IncludeDir.ImGuizmo}",
        "%{IncludeDir.VulkanSDK}",
        "%{IncludeDir.Box2D}",
        "%{IncludeDir.miniaudio}",
    }

    links {
        "GLFW",
        "GLAD",
        "ImGui",
        "opengl32.lib",
        "yaml-cpp",
        "Box2D",
    }

    filter "files:vendor/ImGuizmo/**.cpp"
        enablepch "Off"

    filter "system:windows"
        defines {
            "WT_BUILD_DLL",
            "GLFW_INCLUDE_NONE",
        }
        links {
            "Ws2_32",
            "Winmm",
            "Version",
            "Bcrypt",
        }

    filter {}

    if _OPTIONS["csharp-scripting"] then
        defines { "WT_ENABLE_CSHARP_SCRIPTING" }
        includedirs { "%{IncludeDir.mono}" }

        filter "configurations:Debug"
            links { Library.mono_Debug }

        filter "configurations:Release"
            links { Library.mono_Release }

        filter "configurations:Dist"
            links { Library.mono_Release }

        filter {}
    end

    wt_configurations(
        {
            Library.ShaderC_Debug,
            Library.SPIRV_Cross_Debug,
            Library.SPIRV_Cross_GLSL_Debug,
        },
        {
            Library.ShaderC_Release,
            Library.SPIRV_Cross_Release,
            Library.SPIRV_Cross_GLSL_Release,
        }
    )
