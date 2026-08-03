#include "Input.h"
#include "Menus.h"
#include "Overlay.h"
#include "Settings.h"
#include "SurvivalData.h"

#include <Version.h>

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
			Menus::GetSingleton()->Install();
			Overlay::Install();
			break;

		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			// Never resume into a save still in edit mode.
			Input::GetSingleton()->LeaveEditMode();
			Menus::GetSingleton()->Refresh();
			SurvivalData::GetSingleton()->Refresh();
			if (!Overlay::Installed()) {
				Overlay::Install();  // the swap chain may have been late
			}
			break;

		default:
			break;
		}
	}
}

SKSEPluginInfo(
	.Version = { Version::MAJOR, Version::MINOR, Version::PATCH, 0 },
	.Name = Version::PROJECT,
	.Author = "bottle")

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);

	// One write_call<5> for the input poll hook, with room to spare.
	SKSE::AllocTrampoline(32);

	const auto messaging = SKSE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(MessageHandler)) {
		return false;
	}

	SKSE::log::info("{} v{} loaded", Version::PROJECT, Version::NAME);
	return true;
}
