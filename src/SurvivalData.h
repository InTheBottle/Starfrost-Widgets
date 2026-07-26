#pragma once

#include "Settings.h"

namespace StarfrostWidgets
{
	struct GaugeState
	{
		bool        available{ false };
		float       value{ 0.0f };  // raw need value, or injury tier for kInjury
		float       maxValue{ 1.0f };
		float       fill{ 0.0f };  // 0..1
		std::size_t stage{ 0 };    // 0 (satisfied) .. 5 (critical)
	};

	// Read-only: needs from the Survival Mode globals, injuries from Blade & Blunt's abilities.
	class SurvivalData : public REX::Singleton<SurvivalData>
	{
	public:
		void ResolveForms();
		void Refresh();

		[[nodiscard]] const GaugeState& Get(Gauge a_gauge) const { return states[static_cast<std::size_t>(a_gauge)]; }
		[[nodiscard]] bool              SurvivalModeEnabled() const;
		[[nodiscard]] bool              AnyNeedResolved() const;

	private:
		friend class REX::Singleton<SurvivalData>;
		SurvivalData() = default;

		struct NeedForms
		{
			RE::TESGlobal* value{ nullptr };
			RE::TESGlobal* maxValue{ nullptr };
			RE::TESGlobal* stages[5]{};
			RE::TESGlobal* currentStage{ nullptr };  // SMI's cache, -1 until it starts
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
