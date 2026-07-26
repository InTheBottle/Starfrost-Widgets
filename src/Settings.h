#pragma once

namespace StarfrostWidgets
{
	// Cold ships disabled - Starfrost already gives it a vanilla HUD meter.
	// The last three count a buff down instead of a need up; new entries go on the
	// end so an existing ini keeps lining up with the gauge it was written for.
	enum class Gauge : std::size_t
	{
		kHunger = 0,
		kSleep,
		kInjury,
		kCold,
		kFoodBuff,
		kAlcohol,
		kBlessing,

		kTotal
	};

	inline constexpr std::size_t kGaugeCount = static_cast<std::size_t>(Gauge::kTotal);

	// Survival Mode owns these three; the rest come from other mods and show regardless.
	[[nodiscard]] inline constexpr bool IsSurvivalNeed(Gauge a_gauge)
	{
		return a_gauge == Gauge::kHunger || a_gauge == Gauge::kSleep || a_gauge == Gauge::kCold;
	}

	// Buff timers read the stage ramp backwards: full is calm, nearly expired is critical.
	[[nodiscard]] inline constexpr bool IsTimerGauge(Gauge a_gauge)
	{
		return a_gauge == Gauge::kFoodBuff || a_gauge == Gauge::kAlcohol || a_gauge == Gauge::kBlessing;
	}

	// Which attribute a buff is working on, so the widget can badge it.
	enum class BuffAttribute : std::uint32_t
	{
		kNone = 0,
		kHealth = 1 << 0,
		kMagicka = 1 << 1,
		kStamina = 1 << 2,
		kWarmth = 1 << 3  // Survival Mode's warmth, which Gourmet's stews grant
	};

	inline constexpr std::uint32_t kBuffAttributeCount = 4;

	// 0 (satisfied) to 5 (critical), matching the Survival_*Stage1..5Value thresholds.
	inline constexpr std::size_t kStageCount = 6;

	enum class WidgetStyle : std::int32_t
	{
		kRing = 0,
		kIcon = 1,
		kBar = 2
	};

	// Frame generation composites the back buffer itself and would overwrite what we draw there.
	enum class RenderTarget : std::int32_t
	{
		kAuto = 0,
		kSwapChain = 1,
		kGameFrameBuffer = 2
	};

	struct WidgetSettings
	{
		bool         enabled{ true };
		float        posX{ 0.03f };  // fraction of screen size, top-left of the widget box
		float        posY{ 0.30f };
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

		bool          enabled{ true };
		std::uint32_t editModeKey{ 0xD2 };  // DIK_INSERT
		float         globalScale{ 1.0f };
		float         opacity{ 1.0f };
		bool          hideInMenus{ true };
		bool          hideWhenHUDHidden{ true };
		bool          requireSurvivalMode{ true };
		bool          showValues{ false };
		bool          showTimers{ true };
		bool          showAttributes{ true };
		bool          pulseAtCritical{ true };
		float         pollInterval{ 0.25f };
		RenderTarget  renderTarget{ RenderTarget::kAuto };

		WidgetSettings widgets[kGaugeCount]{};

	private:
		friend class REX::Singleton<Settings>;
		Settings();

		[[nodiscard]] static std::filesystem::path IniPath();
	};
}
