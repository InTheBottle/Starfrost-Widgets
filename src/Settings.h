#pragma once

namespace StarfrostWidgets
{
	// Cold ships disabled - Starfrost already gives it a vanilla HUD meter.
	enum class Gauge : std::size_t
	{
		kHunger = 0,
		kSleep,
		kInjury,
		kCold,

		kTotal
	};

	inline constexpr std::size_t kGaugeCount = static_cast<std::size_t>(Gauge::kTotal);

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
