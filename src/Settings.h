#pragma once

namespace StarfrostWidgets
{
	// The four things we can draw. Hunger/Sleep/Injury are on by default; Cold is
	// shipped disabled because Starfrost's cold already has a vanilla HUD meter.
	enum class Gauge : std::size_t
	{
		kHunger = 0,
		kSleep,
		kInjury,
		kCold,

		kTotal
	};

	inline constexpr std::size_t kGaugeCount = static_cast<std::size_t>(Gauge::kTotal);

	// Needs report a severity of 0 (satisfied) through 5 (critical), matching the
	// Survival_*Stage1..5Value thresholds. Injuries only reach 3, and get remapped
	// onto this scale so every gauge can share one colour ramp.
	inline constexpr std::size_t kStageCount = 6;

	enum class WidgetStyle : std::int32_t
	{
		kRing = 0,  // radial gauge with the icon in the middle
		kIcon = 1,  // icon only, tinted by severity
		kBar = 2    // horizontal bar with the icon to its left
	};

	struct WidgetSettings
	{
		bool         enabled{ true };
		float        posX{ 0.03f };  // normalized to screen width, top-left of the widget box
		float        posY{ 0.30f };  // normalized to screen height
		float        scale{ 1.0f };
		WidgetStyle  style{ WidgetStyle::kRing };
		bool         hideWhenSatisfied{ false };
		ImU32        stageColors[kStageCount]{};
	};

	class Settings : public REX::Singleton<Settings>
	{
	public:
		void Load();
		void Save() const;

		[[nodiscard]] WidgetSettings&       Widget(Gauge a_gauge) { return widgets[static_cast<std::size_t>(a_gauge)]; }
		[[nodiscard]] const WidgetSettings& Widget(Gauge a_gauge) const { return widgets[static_cast<std::size_t>(a_gauge)]; }

		// [General]
		bool          enabled{ true };
		std::uint32_t editModeKey{ 0xD2 };  // DIK_INSERT
		float         globalScale{ 1.0f };
		float         opacity{ 1.0f };
		bool          hideInMenus{ true };
		bool          hideWhenHUDHidden{ true };
		bool          requireSurvivalMode{ true };
		bool          showValues{ false };
		bool          pulseAtCritical{ true };
		float         pollInterval{ 0.25f };  // seconds between game-data reads

		WidgetSettings widgets[kGaugeCount]{};

	private:
		friend class REX::Singleton<Settings>;
		Settings();

		[[nodiscard]] static std::filesystem::path IniPath();
	};
}
