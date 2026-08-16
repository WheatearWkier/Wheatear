#include "wtpch.h"
#include "Log.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/base_sink.h"

namespace Wheatear {

	std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
	std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

	namespace {

		constexpr size_t kEditorLogCapacity = 2048;

		// Ring buffer shared by every logger; captured by the editor console.
		std::vector<LogMessage>& EditorLogBuffer()
		{
			static std::vector<LogMessage> messages;
			return messages;
		}

		// A spdlog sink that mirrors every formatted line into the shared ring
		// buffer. base_sink<std::mutex> already serializes sink_it_ calls, so
		// no extra locking is needed here.
		class EditorLogSink final : public spdlog::sinks::base_sink<std::mutex>
		{
		protected:
			void sink_it_(const spdlog::details::log_msg& msg) override
			{
				spdlog::memory_buf_t formatted;
				if (formatter_)
					formatter_->format(msg, formatted);
				else
					spdlog::pattern_formatter().format(msg, formatted);

				std::vector<LogMessage>& messages = EditorLogBuffer();
				messages.push_back({
					msg.level,
					fmt::to_string(formatted),
					std::chrono::system_clock::now()
				});
				if (messages.size() > kEditorLogCapacity)
					messages.erase(messages.begin(),
						messages.begin() + (messages.size() - kEditorLogCapacity));
			}

			void flush_() override
			{
			}
		};

	} // namespace

	void Log::Init() {
		spdlog::set_pattern("%^[%T] %n: %v%$");
		auto editorSink = std::make_shared<EditorLogSink>();
		s_CoreLogger = spdlog::stdout_color_mt("WHEATEAR");
		s_CoreLogger->sinks().push_back(editorSink);
		s_ClientLogger = spdlog::stdout_color_mt("APP");
		s_ClientLogger->sinks().push_back(editorSink);

#ifdef WT_DEBUG
		// Full verbosity in debug builds; release builds skip trace/debug noise.
		s_CoreLogger->set_level(spdlog::level::trace);
		s_ClientLogger->set_level(spdlog::level::trace);
#else
		s_CoreLogger->set_level(spdlog::level::info);
		s_ClientLogger->set_level(spdlog::level::info);
#endif
	}

	void Log::DrainEditorMessages(std::vector<LogMessage>& out)
	{
		std::vector<LogMessage>& messages = EditorLogBuffer();
		if (messages.empty())
			return;
		out.insert(out.end(), messages.begin(), messages.end());
		messages.clear();
	}

	void Log::ClearEditorMessages()
	{
		EditorLogBuffer().clear();
	}

}
