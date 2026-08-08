#include "Layout.h"

#include "Menus.h"

namespace StarfrostWidgets::Layout
{
	namespace
	{
		constexpr float kFadeInSeconds = 0.60f;
		constexpr float kFadeOutSeconds = 0.20f;

		// The screen fade has to have been over for this long before the widgets start rising.
		constexpr float kFadeSettleSeconds = 0.40f;

		// A ceiling on the hold, so a fade flag we misread can never keep the widgets off.
		constexpr float kFadeHoldSeconds = 5.0f;

		float sFade = 0.0f;
		float sHoldRemaining = 0.0f;
		float sSettleRemaining = 0.0f;
		bool  sWasLoading = false;
		bool  sWasHUDOpen = false;
	}

	ImVec2 BoxSize(const WidgetSettings& a_widget, float a_scale)
	{
		switch (a_widget.style) {
		case WidgetStyle::kIcon:
			return { kIconBox * a_scale, kIconBox * a_scale };
		case WidgetStyle::kBar:
			return { kBarBox.x * a_scale, kBarBox.y * a_scale };
		case WidgetStyle::kRing:
		default:
			return { kRingBox * a_scale, kRingBox * a_scale };
		}
	}

	bool ShouldHideForUI(const Settings& a_settings)
	{
		const auto ui = RE::UI::GetSingleton();
		if (!ui) {
			return true;
		}

		const auto menus = Menus::GetSingleton();

		// Not a covering menu: the loading screen keeps kAlwaysOpen throughout.
		if (menus->LoadingScreenOpen()) {
			return true;
		}
		// No HUD means the main menu.
		if (!menus->HUDOpen()) {
			return true;
		}
		if (a_settings.hideWhenHUDHidden && !ui->IsShowingMenus()) {
			return true;
		}
		if (a_settings.hideInMenus && menus->CoveringMenuOpen()) {
			return true;
		}

		return false;
	}

	bool GaugeVisible(Gauge a_gauge, const Settings& a_settings, const GaugeState& a_state,
		bool a_survivalOn)
	{
		const auto& widget = a_settings.Widget(a_gauge);
		if (!widget.enabled || !a_state.available) {
			return false;
		}
		// Injuries and the buff timers come from other mods, so they ignore Survival Mode.
		if (IsSurvivalNeed(a_gauge) && a_settings.requireSurvivalMode && !a_survivalOn) {
			return false;
		}
		if (widget.dynamic && a_state.stage < widget.showFromStage) {
			return false;
		}
		return true;
	}

	std::size_t IconTier(const GaugeState& a_state)
	{
		if (a_state.tiered) {
			return static_cast<std::size_t>(std::clamp(a_state.value, 0.0f, 3.0f));
		}
		return std::min(a_state.stage, kStageCount - 1) / 2;
	}

	ImU32 StageColor(const WidgetSettings& a_widget, const GaugeState& a_state)
	{
		const bool idle = a_state.timer && !a_state.active;
		return idle ? kIdleColor : a_widget.stageColors[std::min(a_state.stage, kStageCount - 1)];
	}

	float StageAlpha(const Settings& a_settings, const GaugeState& a_state, float a_fade, double a_time)
	{
		const bool idle = a_state.timer && !a_state.active;
		float      alpha = a_settings.opacity * a_fade * (idle ? kIdleAlpha : 1.0f);

		// Only the top stage pulses; anything more and the HUD never settles.
		if (!idle && a_settings.pulseAtCritical && a_state.stage >= kStageCount - 1) {
			alpha *= 0.62f + 0.38f * (0.5f + 0.5f * std::sin(static_cast<float>(a_time) * 4.2f));
		}
		return std::clamp(alpha, 0.0f, 1.0f);
	}

	// Buff timers count down, so "8h 02m" beats a bare number of seconds.
	std::string FormatDuration(float a_seconds)
	{
		const auto total = static_cast<int>(std::max(a_seconds, 0.0f) + 0.5f);
		if (total >= 3600) {
			return std::format("{}h {:02}m", total / 3600, (total % 3600) / 60);
		}
		if (total >= 60) {
			return std::format("{}:{:02}", total / 60, total % 60);
		}
		return std::format("{}s", total);
	}

	std::string CaptionFor(const Settings& a_settings, const GaugeState& a_state)
	{
		if (a_state.timer) {
			if (!a_state.active || !a_settings.showTimers) {
				return {};
			}
			return FormatDuration(a_state.value);
		}
		if (!a_settings.showValues) {
			return {};
		}
		return a_state.tiered ?
		           std::format("{}/3", static_cast<int>(a_state.value)) :
		           std::format("{}", static_cast<int>(a_state.value));
	}

	// Loading screens and the HUD coming up both start a fresh entrance.
	float UpdateFade(bool a_visible, float a_deltaTime)
	{
		const auto menus = Menus::GetSingleton();
		const bool loading = menus->LoadingScreenOpen();
		const bool hudOpen = menus->HUDOpen();

		if ((sWasLoading && !loading) || (!sWasHUDOpen && hudOpen)) {
			sHoldRemaining = kFadeHoldSeconds;
			sSettleRemaining = kFadeSettleSeconds;
		}
		sWasLoading = loading;
		sWasHUDOpen = hudOpen;

		if (sHoldRemaining > 0.0f) {
			sHoldRemaining -= a_deltaTime;
			sSettleRemaining = menus->ScreenFading() ?
			                       kFadeSettleSeconds :
			                       sSettleRemaining - a_deltaTime;
			if (sSettleRemaining <= 0.0f) {
				sHoldRemaining = 0.0f;
			}
		}

		const bool  rising = a_visible && sHoldRemaining <= 0.0f;
		const float step = rising ? a_deltaTime / kFadeInSeconds : -a_deltaTime / kFadeOutSeconds;

		sFade = std::clamp(sFade + step, 0.0f, 1.0f);
		return sFade;
	}

	void ForceFade(float a_value)
	{
		sFade = std::clamp(a_value, 0.0f, 1.0f);
		sHoldRemaining = 0.0f;
	}

	void ResetFade()
	{
		sFade = 0.0f;
		sHoldRemaining = 0.0f;
		sSettleRemaining = 0.0f;
	}

	float Fade()
	{
		return sFade;
	}
}
