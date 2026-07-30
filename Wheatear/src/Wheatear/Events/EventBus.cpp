#include "wtpch.h"
#include "EventBus.h"

namespace Wheatear {

	EventSubscription::EventSubscription(EventBus& bus, EventType type, uint64_t id)
		: m_Bus(&bus), m_Type(type), m_ID(id)
	{
	}

	EventSubscription::~EventSubscription()
	{
		Unsubscribe();
	}

	EventSubscription::EventSubscription(EventSubscription&& other) noexcept
	{
		m_Bus = other.m_Bus;
		m_Type = other.m_Type;
		m_ID = other.m_ID;

		other.m_Bus = nullptr;
		other.m_Type = EventType::None;
		other.m_ID = 0;
	}

	EventSubscription& EventSubscription::operator=(EventSubscription&& other) noexcept
	{
		if (this == &other)
			return *this;

		Unsubscribe();

		m_Bus = other.m_Bus;
		m_Type = other.m_Type;
		m_ID = other.m_ID;

		other.m_Bus = nullptr;
		other.m_Type = EventType::None;
		other.m_ID = 0;

		return *this;
	}

	void EventSubscription::Unsubscribe()
	{
		if (!m_Bus || m_ID == 0)
			return;

		m_Bus->Unsubscribe(m_Type, m_ID);
		m_Bus = nullptr;
		m_Type = EventType::None;
		m_ID = 0;
	}

	EventSubscription EventBus::Subscribe(EventType type, HandlerFn handler, int priority)
	{
		WT_CORE_ASSERT(handler, "Event handler must be valid");

		const uint64_t id = m_NextHandlerID++;
		auto& handlers = m_Handlers[type];
		handlers.push_back({ id, priority, m_NextOrder++, true, std::move(handler) });

		return EventSubscription(*this, type, id);
	}

	EventSubscription EventBus::SubscribeAll(HandlerFn handler, int priority)
	{
		return Subscribe(EventType::None, std::move(handler), priority);
	}

	void EventBus::Dispatch(Event& event)
	{
		std::vector<HandlerCall> calls;
		AppendCallsForType(event.GetEventType(), calls);
		AppendCallsForType(EventType::None, calls);

		std::sort(calls.begin(), calls.end(),
			[](const HandlerCall& a, const HandlerCall& b)
			{
				if (a.Priority != b.Priority)
					return a.Priority > b.Priority;
				return a.Order < b.Order;
			});

		m_DispatchDepth++;

		for (const HandlerCall& call : calls)
		{
			if (event.Handled())
				break;

			HandlerEntry* entry = FindHandler(call.Type, call.ID);
			if (!entry || !entry->Active)
				continue;

			if (entry->Handler(event) == EventResult::Consume)
				event.Consume();
		}

		m_DispatchDepth--;

		if (m_DispatchDepth == 0 && m_NeedsCompaction)
			CompactInactiveHandlers();
	}

	void EventBus::Queue(const Event& event)
	{
		m_QueuedEvents.push_back(event.Clone());
	}

	void EventBus::Flush(size_t maxEvents)
	{
		size_t processedEvents = 0;

		while (!m_QueuedEvents.empty() && processedEvents < maxEvents)
		{
			Scope<Event> event = std::move(m_QueuedEvents.front());
			m_QueuedEvents.pop_front();

			Dispatch(*event);
			processedEvents++;
		}

		if (!m_QueuedEvents.empty())
			WT_CORE_WARN("EventBus flush stopped after {0} events; {1} events remain queued",
				processedEvents, m_QueuedEvents.size());
	}

	void EventBus::Clear()
	{
		m_QueuedEvents.clear();
		m_Handlers.clear();
		m_NeedsCompaction = false;
	}

	void EventBus::Unsubscribe(EventType type, uint64_t id)
	{
		auto it = m_Handlers.find(type);
		if (it == m_Handlers.end())
			return;

		for (HandlerEntry& entry : it->second)
		{
			if (entry.ID == id)
			{
				entry.Active = false;
				m_NeedsCompaction = true;
				break;
			}
		}

		if (m_DispatchDepth == 0)
			CompactInactiveHandlers();
	}

	void EventBus::AppendCallsForType(EventType type, std::vector<HandlerCall>& calls) const
	{
		auto it = m_Handlers.find(type);
		if (it == m_Handlers.end())
			return;

		for (const HandlerEntry& entry : it->second)
		{
			if (entry.Active)
				calls.push_back({ type, entry.ID, entry.Priority, entry.Order });
		}
	}

	EventBus::HandlerEntry* EventBus::FindHandler(EventType type, uint64_t id)
	{
		auto it = m_Handlers.find(type);
		if (it == m_Handlers.end())
			return nullptr;

		for (HandlerEntry& entry : it->second)
		{
			if (entry.ID == id)
				return &entry;
		}

		return nullptr;
	}

	void EventBus::CompactInactiveHandlers()
	{
		for (auto it = m_Handlers.begin(); it != m_Handlers.end(); )
		{
			auto& handlers = it->second;
			handlers.erase(
				std::remove_if(handlers.begin(), handlers.end(),
					[](const HandlerEntry& entry) { return !entry.Active; }),
				handlers.end());

			if (handlers.empty())
				it = m_Handlers.erase(it);
			else
				++it;
		}

		m_NeedsCompaction = false;
	}

} // namespace Wheatear
