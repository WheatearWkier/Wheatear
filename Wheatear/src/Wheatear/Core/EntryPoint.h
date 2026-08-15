#pragma once

#include "Wheatear/Core/Application.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Debug/Instrumentor.h"

namespace Wheatear {
	Application* CreateApplication(ApplicationCommandLineArgs args);
}

#ifdef WT_PLATFORM_WINDOWS
// The engine renders at the window's native resolution; on scaled displays a
// DPI-unaware process gets its window DPI-virtualized (the swap chain is
// rendered at logical size and bitblt-scaled down by DWM), which softens
// text and sprites. Opt into per-monitor DPI awareness before GLFW starts.
static void EnablePerMonitorDpiAwareness()
{
	using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(void*);
	if (HMODULE user32 = ::GetModuleHandleA("user32.dll"))
	{
		auto setContext = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
			::GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
		if (setContext)
		{
			// DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 (1703+)
			if (setContext(reinterpret_cast<void*>(-4)))
				return;
			// DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE
			if (setContext(reinterpret_cast<void*>(-3)))
				return;
			// DPI_AWARENESS_CONTEXT_SYSTEM_AWARE
			if (setContext(reinterpret_cast<void*>(-2)))
				return;
		}
	}

	using SetProcessDpiAwarenessFn = HRESULT(WINAPI*)(int);
	if (HMODULE shcore = ::LoadLibraryA("shcore.dll"))
	{
		auto setAwareness = reinterpret_cast<SetProcessDpiAwarenessFn>(
			::GetProcAddress(shcore, "SetProcessDpiAwareness"));
		if (setAwareness)
		{
			// PROCESS_PER_MONITOR_DPI_AWARE = 2, PROCESS_SYSTEM_DPI_AWARE = 1
			if (setAwareness(2) == S_OK || setAwareness(1) == S_OK)
			{
				::FreeLibrary(shcore);
				return;
			}
		}
		::FreeLibrary(shcore);
	}

	::SetProcessDPIAware();
}
#endif
	
int main(int argc, char** argv) {
#ifdef WT_PLATFORM_WINDOWS
	EnablePerMonitorDpiAwareness();
#endif
	Wheatear::Log::Init();

	WT_PROFILE_BEGIN_SESSION("Startup", "WheatearProfile-Startup.json");
	auto app = Wheatear::CreateApplication({ argc, argv });
	WT_PROFILE_END_SESSION();

	WT_PROFILE_BEGIN_SESSION("Runtime", "WheatearProfile-Runtime.json");
	app->Run();
	WT_PROFILE_END_SESSION();

	WT_PROFILE_BEGIN_SESSION("Shutdown", "WheatearProfile-Shutdown.json");
	delete app;
	WT_PROFILE_END_SESSION();
}
