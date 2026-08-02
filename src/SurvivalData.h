#pragma once

#include "Settings.h"

namespace StarfrostWidgets
{
	struct GaugeState
	{
		bool          available{ false };
		bool          active{ false };
		bool          timer{ false };   // value/maxValue are seconds remaining / total
		bool          tiered{ false };  // value is a 0-3 step, not a continuous reading
		float         value{ 0.0f };    // raw need value, tier, or seconds left
		float         maxValue{ 1.0f };
		float         fill{ 0.0f };    // 0..1
		std::size_t   stage{ 0 };      // 0 (satisfied) .. 5 (critical)
		std::uint32_t attributes{ 0 };  // BuffAttribute mask, for the buff gauges
		char          label[64]{};      // the spell that granted it, for the edit panel
	};

	class SurvivalData : public REX::Singleton<SurvivalData>
	{
	public:
		void ResolveForms();
		void Refresh();

		[[nodiscard]] const GaugeState& Get(Gauge a_gauge) const { return states[static_cast<std::size_t>(a_gauge)]; }
		[[nodiscard]] bool              SurvivalModeEnabled() const;

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
			TrackedEffect   effects[kMaxTrackedEffects]{};
			std::size_t     count{ 0 };
			RE::BGSKeyword* keywords[2]{};
			std::size_t     keywordCount{ 0 };

			void Add(RE::EffectSetting* a_form, BuffAttribute a_attribute);
			void AddKeyword(RE::BGSKeyword* a_keyword);

			[[nodiscard]] bool Resolved() const { return count > 0 || keywordCount > 0; }

			// The attribute mask if a_base belongs to this buff, nothing otherwise.
			[[nodiscard]] std::optional<std::uint32_t> Match(const RE::EffectSetting* a_base) const;
		};

		struct StressForms
		{
			RE::TESGlobal* value{ nullptr };
			RE::TESGlobal* enabled{ nullptr };
		};

		void RefreshNeed(Gauge a_gauge, const NeedForms& a_forms);
		void RefreshHunger();
		void RefreshBuffs();
		void RefreshStress();

		// Three abilities standing in for a 0-3 severity ladder.
		void RefreshTiers(Gauge a_gauge, RE::SpellItem* const (&a_spells)[3]);

		[[nodiscard]] bool UseHungerTiers() const;

		NeedForms      hunger{};
		NeedForms      sleep{};
		NeedForms      cold{};
		StressForms    stress{};
		RE::TESGlobal* survivalModeEnabled{ nullptr };
		RE::SpellItem* injurySpells[3]{};  // minor, major, critical
		RE::SpellItem* hungerSpells[3]{};  // Starfrost's Hungry, Very Hungry, Famished

		BuffForms foodBuff{};
		BuffForms alcohol{};
		BuffForms blessing{};

		GaugeState states[kGaugeCount]{};
		bool       resolved{ false };
	};
}
