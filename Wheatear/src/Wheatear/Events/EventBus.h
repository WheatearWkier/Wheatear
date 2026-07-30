#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Events/Event.h"

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <deque>
#include <functional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Wheatear {

	namespace EventPriority
	{
		constexpr int Critical = 10000;
		constexpr int Application = 9000;
		constexpr int UI = 8000;
		constexpr int Overlay = 6000;
		constexpr int Layer = 1000;
		constexpr int Low = -1000;
	}

	class EventBus;

	class WHEATEAR_API EventSubscription
	{
	public:
		EventSubscription() = default;
		~EventSubscription();

		EventSubscription(const EventSubscription&) = delete;
		EventSubscription& operator=(const EventSubscription&) = delete;

		EventSubscription(EventSubscription&& other) noexcept;
		EventSubscription& operator=(EventSubscription&& other) noexcept;

		void Unsubscribe();
		bool IsValid() const { return m_Bus != nullptr && m_ID != 0; }

	private:
		friend class EventBus;

		EventSubscription(EventBus& bus, EventType type, uint64_t id);

		EventBus* m_Bus = nullptr;
		EventType m_Type = EventType::None;
		uint64_t m_ID = 0;
	};

	//
	class WHEATEAR_API EventBus
	{
	public:
		using HandlerFn = std::function<EventResult(Event&)>;

		EventSubscription Subscribe(EventType type, HandlerFn handler, int priority = 0);
		EventSubscription SubscribeAll(HandlerFn handler, int priority = 0);

		template<typename T, typename Fn>
		EventSubscription Subscribe(Fn&& handler, int priority = 0)
		{
			using HandlerType = std::decay_t<Fn>;

			auto wrapper = [callback = HandlerType(std::forward<Fn>(handler))](Event& event) mutable -> EventResult
			{
				using ReturnType = std::invoke_result_t<HandlerType&, T&>;

				if constexpr (std::is_same_v<ReturnType, EventResult>)
				{
					return callback(static_cast<T&>(event));
				}
				else if constexpr (std::is_same_v<ReturnType, bool>)
				{
					return callback(static_cast<T&>(event)) ? EventResult::Consume : EventResult::Continue;
				}
				else
				{
					callback(static_cast<T&>(event));
					return EventResult::Continue;
				}
			};

			return Subscribe(T::GetStaticType(), std::move(wrapper), priority);
		}

		void Dispatch(Event& event);

		void Queue(const Event& event);

		void Flush(size_t maxEvents = 1024);
		void Clear();

		size_t GetPendingEventCount() const { return m_QueuedEvents.size(); }

	private:
		friend class EventSubscription;

		struct HandlerEntry
		{
			uint64_t ID = 0;
			int Priority = 0;
			uint64_t Order = 0;
			bool Active = true;
			HandlerFn Handler;
		};

		struct HandlerCall
		{
			EventType Type = EventType::None;
			uint64_t ID = 0;
			int Priority = 0;
			uint64_t Order = 0;
		};

		void Unsubscribe(EventType type, uint64_t id);
		void AppendCallsForType(EventType type, std::vector<HandlerCall>& calls) const;
		HandlerEntry* FindHandler(EventType type, uint64_t id);
		void CompactInactiveHandlers();

	private:
		std::unordered_map<EventType, std::vector<HandlerEntry>> m_Handlers;
		std::deque<Scope<Event>> m_QueuedEvents;
		uint64_t m_NextHandlerID = 1;
		uint64_t m_NextOrder = 1;
		uint32_t m_DispatchDepth = 0;
		bool m_NeedsCompaction = false;
	};

} // namespace Wheatear
