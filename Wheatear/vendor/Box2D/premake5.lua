project "Box2D"
	kind "StaticLib"
	pchheader "pch.h"
	pchsource "src/pch.cpp"
	forceincludes { "pch.h" }
	language "C++"
	cppdialect "C++11"
	staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"src/**.h",
		"src/**.cpp",
		"include/**.h"
	}

	includedirs
	{
		"include",
		"src"
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"
		editandcontinue "Off"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		runtime "Release"
		optimize "on"
