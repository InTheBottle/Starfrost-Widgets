#include "Settings.h"

#include <SimpleIni.h>

namespace StarfrostWidgets
{
	namespace
	{
		constexpr const char* kSections[kGaugeCount] = {
			"Hunger", "Sleep", "Injury", "Cold", "FoodBuff", "Alcohol", "Blessing", "Stress"
		};
		static_assert(kSections[kGaugeCount - 1] != nullptr, "A new Gauge needs an ini section name");

		[[nodiscard]] ImU32 ParseColor(const char* a_text, ImU32 a_fallback)
		{
			if (!a_text) {
				return a_fallback;
			}

			std::string_view view{ a_text };
			while (!view.empty() && (view.front() == '#' || view.front() == ' ')) {
				view.remove_prefix(1);
			}
			if (view.starts_with("0x") || view.starts_with("0X")) {
				view.remove_prefix(2);
			}

			std::uint32_t rgb{};
			const auto    result = std::from_chars(view.data(), view.data() + view.size(), rgb, 16);
			if (result.ec != std::errc{}) {
				return a_fallback;
			}

			return PackStageColor(rgb);
		}

		[[nodiscard]] std::string FormatColor(ImU32 a_color)
		{
			// Back to the RRGGBB the ini stores.
			return std::format("{:02X}{:02X}{:02X}",
				(a_color >> IM_COL32_R_SHIFT) & 0xFF,
				(a_color >> IM_COL32_G_SHIFT) & 0xFF,
				(a_color >> IM_COL32_B_SHIFT) & 0xFF);
		}

		[[nodiscard]] float ReadFloat(const CSimpleIniA& a_ini, const char* a_section, const char* a_key, float a_default)
		{
			return static_cast<float>(a_ini.GetDoubleValue(a_section, a_key, a_default));
		}
	}

	Settings::Settings()
	{
		// Stacked down the left edge, clear of the compass and the vanilla meters.
		constexpr float kDefaultY[kGaugeCount] = {
			0.300f, 0.375f, 0.450f, 0.525f, 0.600f, 0.675f, 0.750f, 0.825f
		};
		static_assert(kDefaultY[kGaugeCount - 1] != 0.0f, "A new Gauge needs a default position");

		for (std::size_t i = 0; i < kGaugeCount; ++i) {
			auto& widget = widgets[i];
			widget.enabled = (i != static_cast<std::size_t>(Gauge::kCold));
			widget.posX = 0.030f;
			widget.posY = kDefaultY[i];
			widget.scale = 1.0f;
			widget.style = WidgetStyle::kRing;
			widget.dynamic = false;
			widget.showFromStage = 1;
			for (std::size_t stage = 0; stage < kStageCount; ++stage) {
				widget.stageColors[stage] = PackStageColor(kDefaultStageRamp[stage]);
			}
		}
	}

	std::filesystem::path Settings::IniPath()
	{
		return std::filesystem::path{ "Data/SKSE/Plugins/StarfrostWidgets.ini" };
	}

	void Settings::Load()
	{
		CSimpleIniA ini;
		ini.SetUnicode();
		ini.SetMultiKey(false);

		const auto path = IniPath();
		if (const auto rc = ini.LoadFile(path.string().c_str()); rc < 0) {
			// First run: write the defaults out so there is something to edit.
			SKSE::log::info("No ini at {}, writing defaults", path.string());
			Save();
			return;
		}

		enabled = ini.GetBoolValue("General", "bEnabled", enabled);
		editModeKey = static_cast<std::uint32_t>(ini.GetLongValue("General", "iEditModeKey", static_cast<long>(editModeKey)));
		globalScale = std::clamp(ReadFloat(ini, "General", "fGlobalScale", globalScale), 0.25f, 4.0f);
		opacity = std::clamp(ReadFloat(ini, "General", "fOpacity", opacity), 0.05f, 1.0f);
		hideInMenus = ini.GetBoolValue("General", "bHideInMenus", hideInMenus);
		hideWhenHUDHidden = ini.GetBoolValue("General", "bHideWhenHUDHidden", hideWhenHUDHidden);
		requireSurvivalMode = ini.GetBoolValue("General", "bRequireSurvivalMode", requireSurvivalMode);
		showValues = ini.GetBoolValue("General", "bShowValues", showValues);
		showTimers = ini.GetBoolValue("General", "bShowTimers", showTimers);
		showAttributes = ini.GetBoolValue("General", "bShowAttributes", showAttributes);
		pulseAtCritical = ini.GetBoolValue("General", "bPulseAtCritical", pulseAtCritical);
		pollInterval = std::clamp(ReadFloat(ini, "General", "fPollInterval", pollInterval), 0.05f, 5.0f);
		renderTarget = static_cast<RenderTarget>(std::clamp(
			ini.GetLongValue("General", "iRenderTarget", static_cast<long>(renderTarget)), 0L, 2L));

		for (std::size_t i = 0; i < kGaugeCount; ++i) {
			const auto* section = kSections[i];
			auto&       widget = widgets[i];

			widget.enabled = ini.GetBoolValue(section, "bEnabled", widget.enabled);
			widget.posX = std::clamp(ReadFloat(ini, section, "fPosX", widget.posX), -0.5f, 1.5f);
			widget.posY = std::clamp(ReadFloat(ini, section, "fPosY", widget.posY), -0.5f, 1.5f);
			widget.scale = std::clamp(ReadFloat(ini, section, "fScale", widget.scale), 0.25f, 4.0f);
			widget.style = static_cast<WidgetStyle>(std::clamp(
				ini.GetLongValue(section, "iStyle", static_cast<long>(widget.style)), 0L, 2L));
			widget.dynamic = ini.GetBoolValue(section, "bDynamic",
				ini.GetBoolValue(section, "bHideWhenSatisfied", widget.dynamic));
			widget.showFromStage = static_cast<std::size_t>(std::clamp(
				ini.GetLongValue(section, "iShowFromStage", static_cast<long>(widget.showFromStage)),
				1L, static_cast<long>(kStageCount) - 1L));

			for (std::size_t stage = 0; stage < kStageCount; ++stage) {
				const auto key = std::format("sStage{}Color", stage);
				widget.stageColors[stage] = ParseColor(
					ini.GetValue(section, key.c_str(), nullptr), widget.stageColors[stage]);
			}
		}

		SKSE::log::info("Loaded settings from {}", path.string());
	}

	void Settings::Save() const
	{
		CSimpleIniA ini;
		ini.SetUnicode();
		ini.SetMultiKey(false);

		const auto path = IniPath();
		ini.LoadFile(path.string().c_str());  // preserve anything we do not own

		ini.SetBoolValue("General", "bEnabled", enabled,
			"; Master switch for every widget.");
		ini.SetLongValue("General", "iEditModeKey", static_cast<long>(editModeKey),
			"; DirectInput scan code that opens the move/edit overlay. 210 = Insert.", false);
		ini.SetDoubleValue("General", "fGlobalScale", globalScale,
			"; Multiplies every widget's own scale.");
		ini.SetDoubleValue("General", "fOpacity", opacity,
			"; 0.05 - 1.0");
		ini.SetBoolValue("General", "bHideInMenus", hideInMenus,
			"; Hide while a menu (inventory, map, dialogue...) is open.");
		ini.SetBoolValue("General", "bHideWhenHUDHidden", hideWhenHUDHidden,
			"; Follow the game's own HUD visibility, so screenshots stay clean.");
		ini.SetBoolValue("General", "bRequireSurvivalMode", requireSurvivalMode,
			"; Only show the need widgets while Survival Mode is switched on.");
		ini.SetBoolValue("General", "bShowValues", showValues,
			"; Print the raw need value under each widget.");
		ini.SetBoolValue("General", "bShowTimers", showTimers,
			"; Print the time left under the buff widgets (food, alcohol, blessing).");
		ini.SetBoolValue("General", "bShowAttributes", showAttributes,
			"; Badge the buff widgets with what they fortify: a red circle for health,\n"
			"; a blue diamond for magicka, a green triangle for stamina, an orange\n"
			"; square for warmth.");
		ini.SetBoolValue("General", "bPulseAtCritical", pulseAtCritical,
			"; Pulse the widget at the highest severity stage.");
		ini.SetDoubleValue("General", "fPollInterval", pollInterval,
			"; Seconds between reads of the game's need globals.");
		ini.SetLongValue("General", "iRenderTarget", static_cast<long>(renderTarget),
			"; Where the overlay draws. 0 = auto, 1 = swap chain back buffer,\n"
			"; 2 = the game's framebuffer. Frame generation needs 0 or 2, because it\n"
			"; composites the back buffer itself and would overwrite the widgets.",
			false);

		for (std::size_t i = 0; i < kGaugeCount; ++i) {
			const auto* section = kSections[i];
			const auto& widget = widgets[i];

			ini.SetBoolValue(section, "bEnabled", widget.enabled);
			ini.SetDoubleValue(section, "fPosX", widget.posX,
				"; Position as a fraction of screen size, so it survives resolution changes.");
			ini.SetDoubleValue(section, "fPosY", widget.posY);
			ini.SetDoubleValue(section, "fScale", widget.scale);
			ini.SetLongValue(section, "iStyle", static_cast<long>(widget.style),
				"; 0 = ring gauge, 1 = icon only, 2 = bar", false);
			ini.SetBoolValue(section, "bDynamic", widget.dynamic,
				"; Only draw this widget once it has something to say. Needs and injuries\n"
				"; appear as they get worse; buff timers appear as they run down.");
			ini.SetLongValue(section, "iShowFromStage", static_cast<long>(widget.showFromStage),
				"; The stage bDynamic starts drawing at, 1 - 5.", false);

			for (std::size_t stage = 0; stage < kStageCount; ++stage) {
				const auto key = std::format("sStage{}Color", stage);
				ini.SetValue(section, key.c_str(), FormatColor(widget.stageColors[stage]).c_str());
			}
		}

		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);

		if (const auto rc = ini.SaveFile(path.string().c_str()); rc < 0) {
			SKSE::log::error("Failed to write {}", path.string());
		}
	}
}
