#pragma once

#include "Settings.h"
#include "SurvivalData.h"

// The presentation rules, shared by the two renderers. Whichever one is driving
// the HUD owns the fade; the other must not advance it.
namespace StarfrostWidgets::Layout
{
	// Footprints at scale 1.0, sized against a 1080p HUD.
	inline constexpr float  kRingBox = 76.0f;
	inline constexpr float  kIconBox = 56.0f;
	inline constexpr ImVec2 kBarBox{ 190.0f, 46.0f };

	inline constexpr ImU32 kIdleColor = IM_COL32(146, 143, 136, 255);
	inline constexpr float kIdleAlpha = 0.45f;

	inline constexpr float kCaptionSize = 13.0f;
	inline constexpr float kTimerDrop = 11.0f;
	inline constexpr float kValueDrop = 8.0f;

	[[nodiscard]] ImVec2 BoxSize(const WidgetSettings& a_widget, float a_scale);

	[[nodiscard]] bool ShouldHideForUI(const Settings& a_settings);

	// Everything except the fade: whether this gauge has earned a place on screen.
	[[nodiscard]] bool GaugeVisible(Gauge a_gauge, const Settings& a_settings,
		const GaugeState& a_state, bool a_survivalOn);

	[[nodiscard]] std::size_t IconTier(const GaugeState& a_state);
	[[nodiscard]] ImU32       StageColor(const WidgetSettings& a_widget, const GaugeState& a_state);
	[[nodiscard]] float       StageAlpha(const Settings& a_settings, const GaugeState& a_state,
			  float a_fade, double a_time);

	[[nodiscard]] std::string FormatDuration(float a_seconds);
	[[nodiscard]] std::string CaptionFor(const Settings& a_settings, const GaugeState& a_state);

	float UpdateFade(bool a_visible, float a_deltaTime);
	void  ForceFade(float a_value);
	void  ResetFade();

	[[nodiscard]] float Fade();
}
