#pragma once

#include "Core.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"


namespace Wheatear {

	class WHEATEAR_API Log
	{
	public:
		static void Init();

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

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