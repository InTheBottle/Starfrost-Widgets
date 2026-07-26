#pragma once

#include "Settings.h"

namespace StarfrostWidgets
{
	// The render thread reads this while the main thread writes it, so everything here
	// stays trivially copyable - a torn float or a clipped label is harmless, a torn
	// std::string would not be.
	struct GaugeState
	{
		bool          available{ false };
		bool          timer{ false };  // value/maxValue are seconds remaining / total
		float         value{ 0.0f };   // raw need value, injury tier, or seconds left
		float         maxValue{ 1.0f };
		float         fill{ 0.0f };    // 0..1
		std::size_t   stage{ 0 };      // 0 (satisfied) .. 5 (critical)
		std::uint32_t attributes{ 0 };  // BuffAttribute mask, for the buff gauges
		char          label[64]{};      // the spell that granted it, for the edit panel
	};

	// Read-only view of everything the widgets draw: Survival Mode's need globals,
	// Blade & Blunt's injury abilities, and the timed buffs from Gourmet and Pilgrim.
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

		// One base effect that marks a buff, plus the attribute it fortifies.
		struct TrackedEffect
		{
			RE::EffectSetting* form{ nullptr };
			std::uint32_t      attribute{ 0 };
		};

		// Food carries the most: three regen effects, their marriage-meal twins, and warmth.
		static constexpr std::size_t kMaxTrackedEffects = 8;

		struct BuffForms
		{
			TrackedEffect effects[kMaxTrackedEffects]{};
			std::size_t   count{ 0 };

			void Add(RE::EffectSetting* a_form, BuffAttribute a_attribute);
		};

		void RefreshNeed(Gauge a_gauge, const NeedForms& a_forms);
		void RefreshInjury();
		void RefreshBuffs();

		NeedForms      hunger{};
		NeedForms      sleep{};
		NeedForms      cold{};
		RE::TESGlobal* survivalModeEnabled{ nullptr };
		RE::SpellItem* injurySpells[3]{};  // minor, major, critical

		BuffForms foodBuff{};
		BuffForms alcohol{};
		BuffForms blessing{};

		GaugeState states[kGaugeCount]{};
		bool       resolved{ false };
	};
}
