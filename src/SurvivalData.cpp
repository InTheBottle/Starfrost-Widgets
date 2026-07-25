#include "SurvivalData.h"

namespace StarfrostWidgets
{
	namespace
	{
		constexpr const char* kSurvivalPlugin = "ccQDRSSE001-SurvivalMode.esl";
		constexpr const char* kSMIPlugin = "SurvivalModeImproved.esp";
		constexpr const char* kInjuryPlugin = "BladeAndBlunt.esp";

		// Editor IDs survive being overridden by Starfrost or any other patch, so
		// they are the primary lookup. The plugin + local form id is only a fallback
		// for the (unlikely) case that the editor id map has no entry.
		template <class T>
		[[nodiscard]] T* Lookup(std::string_view a_editorID, std::uint32_t a_localID, const char* a_plugin)
		{
			if (const auto byEditorID = RE::TESForm::LookupByEditorID<T>(a_editorID)) {
				return byEditorID;
			}

			if (const auto handler = RE::TESDataHandler::GetSingleton()) {
				if (const auto form = handler->LookupForm<T>(a_localID, a_plugin)) {
					SKSE::log::debug("Resolved {} by form id fallback", a_editorID);
					return form;
				}
			}

			SKSE::log::warn("Could not resolve {} ({}|{:06X})", a_editorID, a_plugin, a_localID);
			return nullptr;
		}

		[[nodiscard]] RE::TESGlobal* LookupGlobal(std::string_view a_editorID, std::uint32_t a_localID, const char* a_plugin)
		{
			return Lookup<RE::TESGlobal>(a_editorID, a_localID, a_plugin);
		}

		[[nodiscard]] float ValueOr(const RE::TESGlobal* a_global, float a_fallback)
		{
			return a_global ? a_global->value : a_fallback;
		}

		// Survival Mode counts a stage as reached once the need is >= that stage's
		// threshold (Survival_NeedBase.IncrementNeed), so this mirrors that exactly.
		[[nodiscard]] std::size_t StageFromThresholds(float a_value, RE::TESGlobal* const (&a_stages)[5])
		{
			std::size_t stage = 0;
			for (std::size_t i = 0; i < 5; ++i) {
				if (a_stages[i] && a_value >= a_stages[i]->value) {
					stage = i + 1;
				} else {
					break;
				}
			}
			return stage;
		}
	}

	void SurvivalData::ResolveForms()
	{
		survivalModeEnabled = LookupGlobal("Survival_ModeEnabled", 0x000826, kSurvivalPlugin);

		hunger.value = LookupGlobal("Survival_HungerNeedValue", 0x00081A, kSurvivalPlugin);
		hunger.maxValue = LookupGlobal("Survival_HungerNeedMaxValue", 0x00080C, kSurvivalPlugin);
		hunger.stages[0] = LookupGlobal("Survival_HungerStage1Value", 0x000806, kSurvivalPlugin);
		hunger.stages[1] = LookupGlobal("Survival_HungerStage2Value", 0x000802, kSurvivalPlugin);
		hunger.stages[2] = LookupGlobal("Survival_HungerStage3Value", 0x000803, kSurvivalPlugin);
		hunger.stages[3] = LookupGlobal("Survival_HungerStage4Value", 0x000804, kSurvivalPlugin);
		hunger.stages[4] = LookupGlobal("Survival_HungerStage5Value", 0x000805, kSurvivalPlugin);
		hunger.currentStage = LookupGlobal("SMI_CurrentHungerStage", 0x000A14, kSMIPlugin);
		hunger.shouldBeEnabled = LookupGlobal("SMI_HungerShouldBeEnabled", 0x000F27, kSMIPlugin);

		sleep.value = LookupGlobal("Survival_ExhaustionNeedValue", 0x000816, kSurvivalPlugin);
		sleep.maxValue = LookupGlobal("Survival_ExhaustionNeedMaxValue", 0x00084A, kSurvivalPlugin);
		// The exhaustion and cold stage thresholds only exist as globals because
		// Survival Mode Improved added them; vanilla kept them as script floats.
		sleep.stages[0] = LookupGlobal("Survival_ExhaustionStage1Value", 0x000A17, kSMIPlugin);
		sleep.stages[1] = LookupGlobal("Survival_ExhaustionStage2Value", 0x000A18, kSMIPlugin);
		sleep.stages[2] = LookupGlobal("Survival_ExhaustionStage3Value", 0x000A19, kSMIPlugin);
		sleep.stages[3] = LookupGlobal("Survival_ExhaustionStage4Value", 0x000A1A, kSMIPlugin);
		sleep.stages[4] = LookupGlobal("Survival_ExhaustionStage5Value", 0x000A1B, kSMIPlugin);
		sleep.currentStage = LookupGlobal("SMI_CurrentExhaustionStage", 0x000A1C, kSMIPlugin);
		sleep.shouldBeEnabled = LookupGlobal("SMI_ExhaustionShouldBeEnabled", 0x000F29, kSMIPlugin);

		cold.value = LookupGlobal("Survival_ColdNeedValue", 0x00081B, kSurvivalPlugin);
		cold.maxValue = LookupGlobal("Survival_ColdNeedMaxValue", 0x00084B, kSurvivalPlugin);
		cold.stages[0] = LookupGlobal("Survival_ColdStage1Value", 0x000D20, kSMIPlugin);
		cold.stages[1] = LookupGlobal("Survival_ColdStage2Value", 0x000D21, kSMIPlugin);
		cold.stages[2] = LookupGlobal("Survival_ColdStage3Value", 0x000D22, kSMIPlugin);
		cold.stages[3] = LookupGlobal("Survival_ColdStage4Value", 0x000D23, kSMIPlugin);
		cold.stages[4] = LookupGlobal("Survival_ColdStage5Value", 0x000D24, kSMIPlugin);
		cold.currentStage = LookupGlobal("SMI_CurrentColdStage", 0x000D1E, kSMIPlugin);
		cold.shouldBeEnabled = LookupGlobal("SMI_ColdShouldBeEnabled", 0x000F28, kSMIPlugin);

		injurySpells[0] = Lookup<RE::SpellItem>("MAG_InjurySpell01", 0x00084A, kInjuryPlugin);
		injurySpells[1] = Lookup<RE::SpellItem>("MAG_InjurySpell02", 0x00084B, kInjuryPlugin);
		injurySpells[2] = Lookup<RE::SpellItem>("MAG_InjurySpell03", 0x00084D, kInjuryPlugin);

		resolved = true;

		SKSE::log::info("Form resolution: hunger={} sleep={} cold={} injuries={}",
			hunger.value != nullptr,
			sleep.value != nullptr,
			cold.value != nullptr,
			injurySpells[0] != nullptr);
	}

	bool SurvivalData::SurvivalModeEnabled() const
	{
		// Without the global we cannot tell, so assume on rather than hiding
		// everything and looking broken.
		return !survivalModeEnabled || survivalModeEnabled->value != 0.0f;
	}

	bool SurvivalData::AnyNeedResolved() const
	{
		return hunger.value || sleep.value || cold.value || injurySpells[0];
	}

	void SurvivalData::Refresh()
	{
		if (!resolved) {
			return;
		}

		RefreshNeed(Gauge::kHunger, hunger);
		RefreshNeed(Gauge::kSleep, sleep);
		RefreshNeed(Gauge::kCold, cold);
		RefreshInjury();
	}

	void SurvivalData::RefreshNeed(Gauge a_gauge, const NeedForms& a_forms)
	{
		auto& state = states[static_cast<std::size_t>(a_gauge)];

		if (!a_forms.value) {
			state = {};
			return;
		}

		// SMI can switch an individual need off (vampires skip hunger, for one).
		if (a_forms.shouldBeEnabled && a_forms.shouldBeEnabled->value == 0.0f) {
			state = {};
			return;
		}

		state.available = true;
		state.value = a_forms.value->value;
		state.maxValue = std::max(ValueOr(a_forms.maxValue, 1.0f), 1.0f);
		state.fill = std::clamp(state.value / state.maxValue, 0.0f, 1.0f);

		// SMI keeps an authoritative cached stage; it sits at -1 until the need
		// system has run at least once, and only then is it worth trusting.
		const float cached = ValueOr(a_forms.currentStage, -1.0f);
		state.stage = cached >= 0.0f ?
		                  static_cast<std::size_t>(std::clamp(cached, 0.0f, 5.0f)) :
		                  StageFromThresholds(state.value, a_forms.stages);
	}

	void SurvivalData::RefreshInjury()
	{
		auto& state = states[static_cast<std::size_t>(Gauge::kInjury)];

		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!player || !injurySpells[0]) {
			state = {};
			return;
		}

		// Highest tier wins, whether Blade & Blunt swaps the abilities out or
		// leaves the lower ones attached.
		std::size_t tier = 0;
		for (std::size_t i = 3; i-- > 0;) {
			if (injurySpells[i] && player->HasSpell(injurySpells[i])) {
				tier = i + 1;
				break;
			}
		}

		// Three injury tiers spread over the shared six-stage colour ramp, so a
		// critical injury reads as red just like a critical need does.
		constexpr std::size_t kTierToStage[4] = { 0, 2, 4, 5 };

		state.available = true;
		state.value = static_cast<float>(tier);
		state.maxValue = 3.0f;
		state.fill = static_cast<float>(tier) / 3.0f;
		state.stage = kTierToStage[tier];
	}
}
