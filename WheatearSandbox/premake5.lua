project "WheatearSandbox"
    kind "ConsoleApp"
    wt_cpp_defaults()

    files {
        "src/**.h",
        "src/**.cpp",
    }

    wt_app_includes()

    links { "Wheatear" }
    dependson { "Wheatear-ScriptCore" }

    wt_app_linker_defaults()
    wt_configurations()
