#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/Timestep.h"

#include <string>

namespace Wheatear {
	class Event;

	class WHEATEAR_API Layer
	{
	public:
		Layer(const std::string& name = "Layer");
		virtual	~Layer();

		virtual void OnAttach() {};
		virtual void OnDetach() {};
		virtual void OnUpdate(Timestep ts) {};
		virtual void OnImGuiRender() {};
		virtual void OnEvent(Event& event) {}

		inline const std::string& GetName() const { return m_DebugName; }
	protected:
		std::string m_DebugName;
	};
}