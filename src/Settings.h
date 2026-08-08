#pragma once

namespace StarfrostWidgets
{
	enum class Gauge : std::size_t
	{
		kHunger = 0,
		kSleep,
		kInjury,
		kCold,
		kFoodBuff,
		kAlcohol,
		kBlessing,
		kStress,
		kLute,

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
		return a_gauge == Gauge::kFoodBuff || a_gauge == Gauge::kAlcohol || a_gauge == Gauge::kBlessing ||
		       a_gauge == Gauge::kLute;
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

	// The ini stores the friendlier RRGGBB; ImU32 is packed ABGR.
	[[nodiscard]] inline constexpr ImU32 PackStageColor(std::uint32_t a_rgb)
	{
		return IM_COL32((a_rgb >> 16) & 0xFF, (a_rgb >> 8) & 0xFF, a_rgb & 0xFF, 0xFF);
	}

	// Green through deep red.
	inline constexpr std::uint32_t kDefaultStageRamp[kStageCount] = {
		0x6FCF7F, 0xB7D96B, 0xE8C15A, 0xE3924A, 0xD4593C, 0xB3232B
	};

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

	// Which of the two renderers draws the widgets. Only ever one at a time.
	enum class Renderer : std::int32_t
	{
		kAuto = 0,      // the skin movie if it loads, the built-in drawing otherwise
		kBuiltIn = 1,   // never load a movie
		kSkin = 2       // movie only, so a broken skin shows up as nothing rather than a fallback
	};

	struct WidgetSettings
	{
		bool         enabled{ true };
		float        posX{ 0.03f };  // fraction of screen size, top-left of the widget box
		float        posY{ 0.30f };
		float        scale{ 1.0f };
		WidgetStyle  style{ WidgetStyle::kRing };
		bool         dynamic{ false };
		std::size_t  showFromStage{ 1 };
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
		Renderer      renderer{ Renderer::kAuto };

		WidgetSettings widgets[kGaugeCount]{};

	private:
		friend class REX::Singleton<Settings>;
		Settings();

		[[nodiscard]] static std::filesystem::path IniPath();
	};
}
