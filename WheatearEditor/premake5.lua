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

    -- Editor TUs instantiate large component snapshots (EditorEntitySnapshot
    -- covers 40+ component types); keep /bigobj on so C1128 stays away.
    buildoptions { "/bigobj" }

    includedirs {
        "src",
        "%{IncludeDir.ImGuizmo}",
        "%{IncludeDir.miniaudio}",
        "%{IncludeDir.yaml_cpp}",
    }

    links { "Wheatear", "yaml-cpp" }
    dependson { "Wheatear-ScriptCore" }

    wt_app_linker_defaults()
    wt_configurations()
