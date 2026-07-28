project "WheatearEditor"
    kind "ConsoleApp"
    wt_cpp_defaults()

    files {
        "src/**.h",
        "src/**.cpp",
    }

    wt_app_includes()

    includedirs {
        "src",
        "%{IncludeDir.ImGuizmo}",
        "%{IncludeDir.miniaudio}",
    }

    links { "Wheatear" }
    dependson { "Wheatear-ScriptCore" }

    wt_configurations()
