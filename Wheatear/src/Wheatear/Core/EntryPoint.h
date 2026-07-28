#pragma once

#include "Wheatear/Core/Application.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Debug/Instrumentor.h"

namespace Wheatear {
	Application* CreateApplication(ApplicationCommandLineArgs args);
}
	
int main(int argc, char** argv) {
	Wheatear::Log::Init();

	WT_PROFILE_BEGIN_SESSION("Startup", "WheatearProfile-Startup.json");
	auto app = Wheatear::CreateApplication({ argc, argv });
	WT_PROFILE_END_SESSION();

	WT_PROFILE_BEGIN_SESSION("Startup", "WheatearProfile-Runtime.json");
	app->Run();
	WT_PROFILE_END_SESSION();

	WT_PROFILE_BEGIN_SESSION("Startup", "WheatearProfile-Shutdown.json");
	delete app;
	WT_PROFILE_END_SESSION();
}
