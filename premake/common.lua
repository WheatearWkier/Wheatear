outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

newoption {
    trigger = "csharp-scripting",
    description = "Enable optional Mono/C# scripting support"
}

local vulkan_sdk = os.getenv("VULKAN_SDK")
if not vulkan_sdk then
    error("VULKAN_SDK is not set. Install the Vulkan SDK or set the VULKAN_SDK environment variable before generating projects.")
end

IncludeDir = {
    Wheatear         = "%{wks.location}/Wheatear/src",
    WheatearVendor   = "%{wks.location}/Wheatear/vendor",
    spdlog        = "%{wks.location}/Wheatear/vendor/spdlog/include",
    GLFW          = "%{wks.location}/Wheatear/vendor/GLFW/include",
    GLAD          = "%{wks.location}/Wheatear/vendor/GLAD/include",
    ImGui         = "%{wks.location}/Wheatear/vendor/imgui",
    glm           = "%{wks.location}/Wheatear/vendor/glm",
    stb_image     = "%{wks.location}/Wheatear/vendor/stb_image",
    entt          = "%{wks.location}/Wheatear/vendor/entt/include",
    tinyobjloader = "%{wks.location}/Wheatear/vendor/tinyobjloader/include",
    yaml_cpp      = "%{wks.location}/Wheatear/vendor/yaml-cpp/include",
    ImGuizmo      = "%{wks.location}/Wheatear/vendor/ImGuizmo",
    Box2D         = "%{wks.location}/Wheatear/vendor/Box2D/include",
    mono          = "%{wks.location}/Wheatear/vendor/mono/include",
    miniaudio     = "%{wks.location}/Wheatear/vendor/miniaudio",
    VulkanSDK     = path.join(vulkan_sdk, "Include"),
}

Library = {
    ShaderC_Debug          = path.join(vulkan_sdk, "Lib/shaderc_sharedd.lib"),
    SPIRV_Cross_Debug      = path.join(vulkan_sdk, "Lib/spirv-cross-cored.lib"),
    SPIRV_Cross_GLSL_Debug = path.join(vulkan_sdk, "Lib/spirv-cross-glsld.lib"),

    ShaderC_Release          = path.join(vulkan_sdk, "Lib/shaderc_shared.lib"),
    SPIRV_Cross_Release      = path.join(vulkan_sdk, "Lib/spirv-cross-core.lib"),
    SPIRV_Cross_GLSL_Release = path.join(vulkan_sdk, "Lib/spirv-cross-glsl.lib"),

    mono_Debug   = "%{wks.location}/Wheatear/vendor/mono/lib/Debug/libmono-static-sgen.lib",
    mono_Release = "%{wks.location}/Wheatear/vendor/mono/lib/Release/libmono-static-sgen.lib",
}

function wt_cpp_defaults()
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    defines { "_CRT_SECURE_NO_WARNINGS" }

    filter "system:windows"
        systemversion "latest"
        buildoptions { "/utf-8", "/wd4828" }
        defines { "WT_PLATFORM_WINDOWS" }

    filter "system:linux"
        defines { "WT_PLATFORM_LINUX" }

    filter "system:macosx"
        defines { "WT_PLATFORM_MACOS" }
    filter {}
end

function wt_configurations(debug_links, release_links)
    debug_links = debug_links or {}
    release_links = release_links or {}

    filter "configurations:Debug"
        defines { "WT_DEBUG", "WT_ENABLE_ASSERTS" }
        runtime "Debug"
        symbols "on"
        links(debug_links)

    filter "configurations:Release"
        defines { "WT_RELEASE" }
        runtime "Release"
        optimize "on"
        links(release_links)

    filter "configurations:Dist"
        defines { "WT_DIST" }
        runtime "Release"
        optimize "on"
        links(release_links)

    filter {}
end

function wt_app_includes()
    includedirs {
        "%{IncludeDir.spdlog}",
        "%{IncludeDir.Wheatear}",
        "%{IncludeDir.WheatearVendor}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.entt}",
    }
end
