#include "wtpch.h"
#include "Log.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace Wheatear {

	std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
	std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

	void Log::Init() {
		spdlog::set_pattern("%^[%T] %n: %v%$");
		s_CoreLogger = spdlog::stdout_color_mt("WHEATEAR");
		s_ClientLogger = spdlog::stdout_color_mt("APP");

#ifdef WT_DEBUG
		// Full verbosity in debug builds; release builds skip trace/debug noise.
		s_CoreLogger->set_level(spdlog::level::trace);
		s_ClientLogger->set_level(spdlog::level::trace);
#else
		s_CoreLogger->set_level(spdlog::level::info);
		s_ClientLogger->set_level(spdlog::level::info);
#endif
	}

}
