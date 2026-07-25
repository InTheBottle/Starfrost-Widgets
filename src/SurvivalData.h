#pragma once

#include "Settings.h"

namespace StarfrostWidgets
{
	struct GaugeState
	{
		bool        available{ false };  // forms resolved and the system is switched on
		float       value{ 0.0f };       // raw need value, or injury tier for kInjury
		float       maxValue{ 1.0f };
		float       fill{ 0.0f };  // 0..1, for the gauge sweep
		std::size_t stage{ 0 };    // 0 (satisfied) .. 5 (critical)
	};

	// Reads Starfrost's needs straight out of the Survival Mode / Survival Mode
	// Improved global variables, and injuries out of Blade & Blunt's tiered
	// injury abilities. Nothing here writes to the game.
	class SurvivalData : public REX::Singleton<SurvivalData>
	{
	public:
		// Resolves every form. Safe to call again if a load order changes.
		void ResolveForms();

		// Re-reads the live values. Cheap: a handful of global reads plus three
		// HasSpell checks, throttled by Settings::pollInterval at the call site.
		void Refresh();

		[[nodiscard]] const GaugeState& Get(Gauge a_gauge) const { return states[static_cast<std::size_t>(a_gauge)]; }
		[[nodiscard]] bool              SurvivalModeEnabled() const;
		[[nodiscard]] bool              AnyNeedResolved() const;

	private:
		friend class REX::Singleton<SurvivalData>;
		SurvivalData() = default;

		// One need's worth of globals. Cold, hunger and exhaustion are identical
		// in shape, which is why Survival Mode's papyrus shares a base script.
		struct NeedForms
		{
			RE::TESGlobal* value{ nullptr };
			RE::TESGlobal* maxValue{ nullptr };
			RE::TESGlobal* stages[5]{};
			RE::TESGlobal* currentStage{ nullptr };  // SMI's cached stage, -1 until it starts
			RE::TESGlobal* shouldBeEnabled{ nullptr };
		};

		void RefreshNeed(Gauge a_gauge, const NeedForms& a_forms);
		void RefreshInjury();

		NeedForms      hunger{};
		NeedForms      sleep{};
		NeedForms      cold{};
		RE::TESGlobal* survivalModeEnabled{ nullptr };
		RE::SpellItem* injurySpells[3]{};  // minor, major, critical

		GaugeState states[kGaugeCount]{};
		bool       resolved{ false };
	};
}
