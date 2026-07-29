project "WheatearEditor"
    kind "ConsoleApp"
    wt_cpp_defaults()
    pchheader "wepch.h"
    pchsource "src/wepch.cpp"
    forceincludes { "wepch.h" }

    files {
        "src/**.h",
        "src/**.cpp",
    }

    wt_app_includes()

    includedirs {
        "src",
        "%{IncludeDir.ImGuizmo}",
        "%{IncludeDir.miniaudio}",
        "%{IncludeDir.yaml_cpp}",
    }

    links { "Wheatear", "yaml-cpp" }
    dependson { "Wheatear-ScriptCore" }

    wt_configurations()
