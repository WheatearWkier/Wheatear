#pragma once

#include <functional>
#include <memory>
#include <cstdlib>
#include <utility>

#if !defined(WT_PLATFORM_WINDOWS) && !defined(WT_PLATFORM_LINUX) && !defined(WT_PLATFORM_MACOS)
	#if defined(_WIN32)
		#define WT_PLATFORM_WINDOWS
	#elif defined(__APPLE__)
		#define WT_PLATFORM_MACOS
	#elif defined(__linux__)
		#define WT_PLATFORM_LINUX
	#else
		#error Wheatear could not detect this platform.
	#endif
#endif

#if defined(WT_PLATFORM_WINDOWS) && defined(WT_DYNAMIC_LINK)
	#ifdef WT_BUILD_DLL
		#define WHEATEAR_API __declspec(dllexport)
	#else
		#define WHEATEAR_API __declspec(dllimport)
	#endif
#else
	#define WHEATEAR_API
#endif

#if defined(WT_PLATFORM_WINDOWS)
	#include <intrin.h>
	#define WT_DEBUGBREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
	#define WT_DEBUGBREAK() __builtin_trap()
#else
	#define WT_DEBUGBREAK() std::abort()
#endif

#define WT_EXPAND_MACRO(x) x
#define WT_STRINGIFY_MACRO(x) #x

#ifdef WT_ENABLE_ASSERTS
	#define WT_INTERNAL_ASSERT_GET_MACRO_NAME(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, macro, ...) macro
	#define WT_INTERNAL_CORE_ASSERT_WITH_MSG(check, ...) { if (!(check)) { ::Wheatear::Log::GetCoreLogger()->error(__VA_ARGS__); WT_DEBUGBREAK(); } }
	#define WT_INTERNAL_CORE_ASSERT_NO_MSG(check) { if (!(check)) { ::Wheatear::Log::GetCoreLogger()->error("Assertion '{0}' failed at {1}:{2}", WT_STRINGIFY_MACRO(check), __FILE__, __LINE__); WT_DEBUGBREAK(); } }
	#define WT_INTERNAL_CLIENT_ASSERT_WITH_MSG(check, ...) { if (!(check)) { ::Wheatear::Log::GetClientLogger()->error(__VA_ARGS__); WT_DEBUGBREAK(); } }
	#define WT_INTERNAL_CLIENT_ASSERT_NO_MSG(check) { if (!(check)) { ::Wheatear::Log::GetClientLogger()->error("Assertion '{0}' failed at {1}:{2}", WT_STRINGIFY_MACRO(check), __FILE__, __LINE__); WT_DEBUGBREAK(); } }

	#define WT_CLIENT_ASSERT(...) WT_EXPAND_MACRO(WT_INTERNAL_ASSERT_GET_MACRO_NAME(__VA_ARGS__, WT_INTERNAL_CLIENT_ASSERT_WITH_MSG, WT_INTERNAL_CLIENT_ASSERT_WITH_MSG, WT_INTERNAL_CLIENT_ASSERT_WITH_MSG, WT_INTERNAL_CLIENT_ASSERT_WITH_MSG, WT_INTERNAL_CLIENT_ASSERT_WITH_MSG, WT_INTERNAL_CLIENT_ASSERT_WITH_MSG, WT_INTERNAL_CLIENT_ASSERT_WITH_MSG, WT_INTERNAL_CLIENT_ASSERT_WITH_MSG, WT_INTERNAL_CLIENT_ASSERT_WITH_MSG, WT_INTERNAL_CLIENT_ASSERT_WITH_MSG, WT_INTERNAL_CLIENT_ASSERT_WITH_MSG, WT_INTERNAL_CLIENT_ASSERT_WITH_MSG, WT_INTERNAL_CLIENT_ASSERT_WITH_MSG, WT_INTERNAL_CLIENT_ASSERT_WITH_MSG, WT_INTERNAL_CLIENT_ASSERT_WITH_MSG, WT_INTERNAL_CLIENT_ASSERT_NO_MSG)(__VA_ARGS__))
	#define WT_CORE_ASSERT(...) WT_EXPAND_MACRO(WT_INTERNAL_ASSERT_GET_MACRO_NAME(__VA_ARGS__, WT_INTERNAL_CORE_ASSERT_WITH_MSG, WT_INTERNAL_CORE_ASSERT_WITH_MSG, WT_INTERNAL_CORE_ASSERT_WITH_MSG, WT_INTERNAL_CORE_ASSERT_WITH_MSG, WT_INTERNAL_CORE_ASSERT_WITH_MSG, WT_INTERNAL_CORE_ASSERT_WITH_MSG, WT_INTERNAL_CORE_ASSERT_WITH_MSG, WT_INTERNAL_CORE_ASSERT_WITH_MSG, WT_INTERNAL_CORE_ASSERT_WITH_MSG, WT_INTERNAL_CORE_ASSERT_WITH_MSG, WT_INTERNAL_CORE_ASSERT_WITH_MSG, WT_INTERNAL_CORE_ASSERT_WITH_MSG, WT_INTERNAL_CORE_ASSERT_WITH_MSG, WT_INTERNAL_CORE_ASSERT_WITH_MSG, WT_INTERNAL_CORE_ASSERT_WITH_MSG, WT_INTERNAL_CORE_ASSERT_NO_MSG)(__VA_ARGS__))
#else
	#define WT_CLIENT_ASSERT(...)
	#define WT_CORE_ASSERT(...)
#endif

#define BIT(x) (1 << x)

#define WT_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)

namespace Wheatear {

	template<typename T>
	using Scope = std::unique_ptr<T>;
	template<typename T, typename ... Args>
	constexpr Scope<T> CreateScope(Args&& ... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	template<typename T>
	using Ref = std::shared_ptr<T>;
	template<typename T, typename ... Args>
	constexpr Ref<T> CreateRef(Args&& ... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}
}
