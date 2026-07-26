#include "SurvivalData.h"

namespace StarfrostWidgets
{
	namespace
	{
		constexpr const char* kSurvivalPlugin = "ccQDRSSE001-SurvivalMode.esl";
		constexpr const char* kSMIPlugin = "SurvivalModeImproved.esp";
		constexpr const char* kInjuryPlugin = "BladeAndBlunt.esp";
		constexpr const char* kGourmetPlugin = "Gourmet.esp";
		constexpr const char* kPilgrimPlugin = "Pilgrim.esp";
		constexpr const char* kStarfrostPlugin = "Starfrost.esp";
		constexpr const char* kStressPlugin = "Stress and Fear.esp";

		constexpr float kStressThresholds[5] = { 0.0f, 25.0f, 45.0f, 65.0f, 85.0f };
		constexpr float kStressMax = 100.0f;

		// Editor IDs survive Starfrost's overrides; plugin + form id is the fallback.
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

		// Mirrors Survival_NeedBase.IncrementNeed: a stage is reached at >= its threshold.
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

		[[nodiscard]] std::size_t StageFromStress(float a_value)
		{
			std::size_t stage = 0;
			for (std::size_t i = 0; i < std::size(kStressThresholds); ++i) {
				if (a_value > kStressThresholds[i]) {
					stage = i + 1;
				} else {
					break;
				}
			}
			return stage;
		}

		// The need ramp inverted: a fresh buff sits at 0, one about to drop off at 5.
		[[nodiscard]] std::size_t StageFromRemaining(float a_fraction)
		{
			constexpr float kThresholds[5] = { 0.75f, 0.50f, 0.30f, 0.15f, 0.05f };

			std::size_t stage = 0;
			for (std::size_t i = 0; i < 5; ++i) {
				if (a_fraction < kThresholds[i]) {
					stage = i + 1;
				} else {
					break;
				}
			}
			return stage;
		}

		void CopyLabel(char (&a_dest)[64], const char* a_source)
		{
			std::size_t i = 0;
			if (a_source) {
				for (; i + 1 < sizeof(a_dest) && a_source[i] != '\0'; ++i) {
					a_dest[i] = a_source[i];
				}
			}
			a_dest[i] = '\0';
		}

		// What a single gauge picked up from one pass over the active effect list.
		struct BuffAccumulator
		{
			float         remaining{ -1.0f };
			float         duration{ 0.0f };
			std::uint32_t attributes{ 0 };
			const char*   label{ nullptr };
		};
	}

	void SurvivalData::BuffForms::Add(RE::EffectSetting* a_form, BuffAttribute a_attribute)
	{
		if (a_form && count < kMaxTrackedEffects) {
			effects[count++] = { a_form, static_cast<std::uint32_t>(a_attribute) };
		}
	}

	void SurvivalData::BuffForms::AddKeyword(RE::BGSKeyword* a_keyword)
	{
		if (a_keyword && keywordCount < std::size(keywords)) {
			keywords[keywordCount++] = a_keyword;
		}
	}

	std::optional<std::uint32_t> SurvivalData::BuffForms::Match(const RE::EffectSetting* a_base) const
	{
		for (std::size_t i = 0; i < count; ++i) {
			if (effects[i].form == a_base) {
				return effects[i].attribute;
			}
		}

		// Keyword-matched buffs are identified as a whole, not per attribute.
		for (std::size_t i = 0; i < keywordCount; ++i) {
			if (a_base->HasKeyword(keywords[i])) {
				return static_cast<std::uint32_t>(BuffAttribute::kNone);
			}
		}

		return std::nullopt;
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
		// Exhaustion and cold thresholds are only globals because SMI promoted them.
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

		stress.value = LookupGlobal("Stress_Total", 0x000801, kStressPlugin);
		stress.enabled = LookupGlobal("Stress_Enabled", 0x0008A5, kStressPlugin);

		injurySpells[0] = Lookup<RE::SpellItem>("MAG_InjurySpell01", 0x00084A, kInjuryPlugin);
		injurySpells[1] = Lookup<RE::SpellItem>("MAG_InjurySpell02", 0x00084B, kInjuryPlugin);
		injurySpells[2] = Lookup<RE::SpellItem>("MAG_InjurySpell03", 0x00084D, kInjuryPlugin);
		hungerSpells[0] = Lookup<RE::SpellItem>("MAG_HungerSpell01", 0x00084E, kStarfrostPlugin);
		hungerSpells[1] = Lookup<RE::SpellItem>("MAG_HungerSpell02", 0x000856, kStarfrostPlugin);
		hungerSpells[2] = Lookup<RE::SpellItem>("MAG_HungerSpell03", 0x000857, kStarfrostPlugin);

		// Gourmet hangs every cooked-food bonus off these three regen effects. The
		// marriage meal grants all three at once through its own copies.
		const auto food = [&](std::string_view a_editorID, std::uint32_t a_localID, BuffAttribute a_attribute) {
			foodBuff.Add(Lookup<RE::EffectSetting>(a_editorID, a_localID, kGourmetPlugin), a_attribute);
		};
		food("MAG_FoodFortifyHealthRegenBasic", 0x000800, BuffAttribute::kHealth);
		food("MAG_FoodFortifyMagickaRegenBasic", 0x000802, BuffAttribute::kMagicka);
		food("MAG_FoodFortifyStaminaRegenBasic", 0x000801, BuffAttribute::kStamina);
		food("MAG_FoodFortifyHealthRegenMarriage", 0x00080A, BuffAttribute::kHealth);
		food("MAG_FoodFortifyMagickaRegenMarriage", 0x00080B, BuffAttribute::kMagicka);
		food("MAG_FoodFortifyStaminaRegenMarriage", 0x00080C, BuffAttribute::kStamina);

		// Gourmet's survival stews warm you through Survival Mode's own effect, and
		// some of them grant nothing else - without this they would show no timer.
		foodBuff.Add(Lookup<RE::EffectSetting>("Survival_FoodFortifyWarmth", 0x002EE6, "Update.esm"),
			BuffAttribute::kWarmth);

		// Drink trades one pool for the other, so only the fortify half earns a badge.
		const auto drink = [&](std::string_view a_editorID, std::uint32_t a_localID, BuffAttribute a_attribute) {
			alcohol.Add(Lookup<RE::EffectSetting>(a_editorID, a_localID, kGourmetPlugin), a_attribute);
		};
		drink("MAG_AlcoholFortifyMagicka", 0x000804, BuffAttribute::kMagicka);
		drink("MAG_AlcoholFortifyStamina", 0x000805, BuffAttribute::kStamina);
		drink("MAG_AlcoholDamageStamina", 0x000803, BuffAttribute::kNone);
		drink("MAG_AlcoholDamageMagicka", 0x000806, BuffAttribute::kNone);

		// Pilgrim's 45 blessings each grant a different boon, but every one of those
		// boons carries a shrine-blessing keyword and none of them are conditional.
		//
		// The obvious hook - the MAG_PilgrimXPEffect / MAG_CultistXPEffect markers the
		// blessings also share - is the wrong one: those are gated behind Pilgrim's
		// anti-farming XP cooldown, so praying at a mat re-casts the blessing without
		// them and the widget would see nothing.
		//
		// Pilgrim injects both keywords into Update.esm's form space, hence the plugin
		// name here; the editor ID is what actually resolves them in practice.
		blessing.AddKeyword(Lookup<RE::BGSKeyword>("MAG_PilgrimShrineBlessing", 0x616101, "Update.esm"));
		blessing.AddKeyword(Lookup<RE::BGSKeyword>("MAG_CultistShrineBlessing", 0x616102, "Update.esm"));

		resolved = true;

		SKSE::log::info("Form resolution: hunger={} sleep={} cold={} injuries={} food={} alcohol={} blessing={} stress={}",
			hunger.value != nullptr,
			sleep.value != nullptr,
			cold.value != nullptr,
			injurySpells[0] != nullptr,
			foodBuff.count,
			alcohol.count,
			blessing.keywordCount,
			stress.value != nullptr);

		SKSE::log::info("Hunger source: {}",
			UseHungerTiers() ? "Starfrost hunger abilities" : "Survival Mode need value");
	}

	bool SurvivalData::UseHungerTiers() const
	{
		return hungerSpells[0] != nullptr;
	}

	bool SurvivalData::SurvivalModeEnabled() const
	{
		// Assume on when the global is missing, rather than hiding everything.
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

		RefreshHunger();
		RefreshNeed(Gauge::kSleep, sleep);
		RefreshNeed(Gauge::kCold, cold);
		RefreshTiers(Gauge::kInjury, injurySpells);
		RefreshBuffs();
		RefreshStress();
	}

	void SurvivalData::RefreshHunger()
	{
		if (UseHungerTiers()) {
			RefreshTiers(Gauge::kHunger, hungerSpells);
		} else {
			RefreshNeed(Gauge::kHunger, hunger);
		}
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

		// SMI's cached stage is authoritative, but sits at -1 until the need starts.
		const float cached = ValueOr(a_forms.currentStage, -1.0f);
		state.stage = cached >= 0.0f ?
		                  static_cast<std::size_t>(std::clamp(cached, 0.0f, 5.0f)) :
		                  StageFromThresholds(state.value, a_forms.stages);
	}

	void SurvivalData::RefreshTiers(Gauge a_gauge, RE::SpellItem* const (&a_spells)[3])
	{
		auto& state = states[static_cast<std::size_t>(a_gauge)];
		state = {};

		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!player || !a_spells[0]) {
			return;
		}

		// Highest tier wins, whether or not the lower abilities stay attached.
		std::size_t tier = 0;
		for (std::size_t i = 3; i-- > 0;) {
			if (a_spells[i] && player->HasSpell(a_spells[i])) {
				tier = i + 1;
				break;
			}
		}

		// Three tiers spread over the shared six-stage colour ramp.
		constexpr std::size_t kTierToStage[4] = { 0, 2, 4, 5 };

		state.available = true;
		state.tiered = true;
		state.value = static_cast<float>(tier);
		state.maxValue = 3.0f;
		state.fill = static_cast<float>(tier) / 3.0f;
		state.stage = kTierToStage[tier];
		CopyLabel(state.label, tier ? a_spells[tier - 1]->GetFullName() : nullptr);
	}

	void SurvivalData::RefreshBuffs()
	{
		constexpr Gauge kGauges[3] = { Gauge::kFoodBuff, Gauge::kAlcohol, Gauge::kBlessing };
		const BuffForms* const kForms[3] = { &foodBuff, &alcohol, &blessing };

		for (const auto gauge : kGauges) {
			states[static_cast<std::size_t>(gauge)] = {};
		}

		const auto player = RE::PlayerCharacter::GetSingleton();
		const auto target = player ? player->AsMagicTarget() : nullptr;
		const auto active = target ? target->GetActiveEffectList() : nullptr;
		if (!active) {
			return;
		}

		BuffAccumulator accumulators[3]{};

		// One pass over the list, matched against all three gauges as we go.
		for (const auto effect : *active) {
			if (!effect || effect->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled)) {
				continue;
			}

			const auto base = effect->GetBaseObject();
			if (!base || effect->duration <= 0.0f) {
				continue;
			}

			const float remaining = effect->duration - effect->elapsedSeconds;
			if (remaining <= 0.0f) {
				continue;
			}

			for (std::size_t i = 0; i < 3; ++i) {
				const auto attribute = kForms[i]->Match(base);
				if (!attribute) {
					continue;
				}

				auto& accumulator = accumulators[i];
				accumulator.attributes |= *attribute;

				// The longest runner decides when the buff is really gone.
				if (remaining > accumulator.remaining) {
					accumulator.remaining = remaining;
					accumulator.duration = effect->duration;
					accumulator.label = effect->spell ? effect->spell->GetFullName() : nullptr;
				}
			}
		}

		for (std::size_t i = 0; i < 3; ++i) {
			const auto& accumulator = accumulators[i];
			if (accumulator.remaining < 0.0f) {
				continue;
			}

			auto& state = states[static_cast<std::size_t>(kGauges[i])];
			state.available = true;
			state.timer = true;
			state.value = accumulator.remaining;
			state.maxValue = std::max(accumulator.duration, 1.0f);
			state.fill = std::clamp(accumulator.remaining / state.maxValue, 0.0f, 1.0f);
			state.stage = StageFromRemaining(state.fill);
			state.attributes = accumulator.attributes;
			CopyLabel(state.label, accumulator.label);
		}
	}

	void SurvivalData::RefreshStress()
	{
		auto& state = states[static_cast<std::size_t>(Gauge::kStress)];
		state = {};

		if (!stress.value || (stress.enabled && stress.enabled->value == 0.0f)) {
			return;
		}

		state.available = true;
		state.value = std::clamp(stress.value->value, 0.0f, kStressMax);
		state.maxValue = kStressMax;
		state.fill = state.value / kStressMax;
		state.stage = StageFromStress(state.value);
	}
}
