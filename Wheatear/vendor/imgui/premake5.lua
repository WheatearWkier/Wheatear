project "ImGui"
	kind "StaticLib"
	pchheader "pch.h"
	pchsource "pch.cpp"
	forceincludes { "pch.h" }
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"imconfig.h",
		"pch.h",
		"pch.cpp",
		"imgui.h",
		"imgui.cpp",
		"imgui_draw.cpp",
		"imgui_tables.cpp",
		"imgui_internal.h",
		"imgui_widgets.cpp",
		"imstb_rectpack.h",
		"imstb_textedit.h",
		"imstb_truetype.h",
		"imgui_demo.cpp"
	}

	filter "system:windows"
		systemversion "latest"
		buildoptions { "/utf-8", "/wd4828" }

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"
		editandcontinue "Off"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"
