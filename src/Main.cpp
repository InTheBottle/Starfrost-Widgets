#include "Input.h"
#include "Overlay.h"
#include "Settings.h"
#include "SurvivalData.h"

using namespace StarfrostWidgets;

namespace
{
	void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			Settings::GetSingleton()->Load();
			SurvivalData::GetSingleton()->ResolveForms();
			Input::GetSingleton()->Install();
			Overlay::Install();
			break;

		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			// Never resume into a save with the player's controls still parked.
			Input::GetSingleton()->LeaveEditMode();
			SurvivalData::GetSingleton()->Refresh();
			if (!Overlay::Installed()) {
				Overlay::Install();  // retry, in case the swap chain was late
			}
			break;

		default:
			break;
		}
	}
}

SKSEPluginInfo(
	.Version = { 1, 1, 0, 0 },
	.Name = "StarfrostWidgets",
	.Author = "bottle")

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	// Init sets up the log file and pattern for us.
	SKSE::Init(a_skse);

	// One write_call<5> for the input poll hook, with room to spare.
	SKSE::AllocTrampoline(32);

	const auto messaging = SKSE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(MessageHandler)) {
		return false;
	}

	SKSE::log::info("StarfrostWidgets loaded");
	return true;
}
