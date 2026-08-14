#pragma once

#include "Wheatear/Core/Layer.h"

struct ImFont;

namespace Wheatear {

	class Event;

	class WHEATEAR_API ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer();
		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnEvent(Event& e) override;

		void Begin();
		void End();

		void BlockEvents(bool block) { m_BlockEvents = block; }
		void SetDarkThemeColors();

		// Compact (20px) variant of the UI font for status bars, badges and
		// dense tool rows. Push with ImGui::PushFont(ImGuiLayer::Get().GetSmallFont()).
		ImFont* GetSmallFont() const { return m_SmallFont; }

	private:
		bool m_BlockEvents = true;
		ImFont* m_SmallFont = nullptr;
	};

}