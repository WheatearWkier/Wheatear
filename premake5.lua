workspace "Wheatear"
    architecture "x64"
    startproject "WheatearEditor"

    configurations {
        "Debug",
        "Release",
        "Dist",
    }

include "premake/common.lua"

group "Dependencies"
    include "Wheatear/vendor/GLFW"
    include "Wheatear/vendor/GLAD"
    include "Wheatear/vendor/imgui"
    include "Wheatear/vendor/yaml-cpp"
    include "Wheatear/vendor/Box2D"
group ""

group "Core"
    include "Wheatear"
group ""

group "Tools"
    include "WheatearEditor"
group ""

group "Runtime"
    include "WheatearSandbox"
group ""
