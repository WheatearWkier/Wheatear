#pragma once

#include "Core.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

#include <chrono>
#include <string>
#include <vector>


namespace Wheatear {

	// A single captured log line, as surfaced to editor UI (console panel).
	struct LogMessage
	{
		spdlog::level::level_enum Level = spdlog::level::info;
		std::string Text;
		std::chrono::system_clock::time_point Time;
	};

	class WHEATEAR_API Log
	{
	public:
		static void Init();

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

		// Editor-facing console buffer: append pending messages to `out` and
		// clear the ring. Safe to call every frame from the editor UI.
		static void DrainEditorMessages(std::vector<LogMessage>& out);
		static void ClearEditorMessages();

	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};

}


//Core log macros
#define WT_CORE_TRACE(...)			::Wheatear::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define WT_CORE_INFO(...)			::Wheatear::Log::GetCoreLogger()->info(__VA_ARGS__)
#define WT_CORE_WARN(...)			::Wheatear::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define WT_CORE_ERROR(...)			::Wheatear::Log::GetCoreLogger()->error(__VA_ARGS__)
#define WT_CORE_FATAL(...)			::Wheatear::Log::GetCoreLogger()->fatal(__VA_ARGS__)

//Client log macros
#define WT_CLIENT_TRACE(...)		::Wheatear::Log::GetClientLogger()->trace(__VA_ARGS__)
#define WT_CLIENT_INFO(...)			::Wheatear::Log::GetClientLogger()->info(__VA_ARGS__)
#define WT_CLIENT_WARN(...)			::Wheatear::Log::GetClientLogger()->warn(__VA_ARGS__)
#define WT_CLIENT_ERROR(...)		::Wheatear::Log::GetClientLogger()->error(__VA_ARGS__)
#define WT_CLIENT_FATAL(...)		::Wheatear::Log::GetClientLogger()->fatal(__VA_ARGS__)