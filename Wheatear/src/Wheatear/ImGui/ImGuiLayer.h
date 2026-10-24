#pragma once

#include "Wheatear/Core/Layer.h"

#include <memory>
#include <string>

struct ImFont;
struct ImGuiStyle;

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
		void SetLightThemeColors();
		bool IsDarkTheme() const { return m_DarkTheme; }

		// Editor-wide UI scale. Scales both font rendering and style metrics
		// (padding, tabs, scrollbars) from the theme's base style.
		float GetUiScale() const { return m_UiScale; }
		void SetUiScale(float scale);
		void AdjustUiScale(float delta);
		// Launcher uses a resolution-derived scale without touching the
		// persisted editor preference.
		void SetUiScaleTransient(float scale);
		void RestoreSavedUiScale();

		// Compact body text for status bars, badges and dense rows.
		ImFont* GetSmallFont() const { return m_SmallFont; }
		// Display font for panel/section headings.
		ImFont* GetHeadingFont() const { return m_HeadingFont; }

		// Stable copy of the ImGui settings path. Do not read
		// ImGui::GetIO().IniFilename directly from editor code: across module
		// boundaries that pointer can refer to storage with a different
		// lifetime, which manifested as corrupted window/layout paths.
		const std::string& GetIniPath() const { return m_IniPath; }

	private:
		bool m_BlockEvents = true;
		bool m_DarkTheme = true;
		ImFont* m_SmallFont = nullptr;
		ImFont* m_HeadingFont = nullptr;
		std::string m_IniPath;
		std::unique_ptr<ImGuiStyle> m_BaseStyle;
		float m_UiScale = 1.0f;
		float m_SavedUiScale = 1.0f;
		bool m_HasBaseStyle = false;

		void ApplyUiScale();
		void PersistUiScale() const;
	};

}