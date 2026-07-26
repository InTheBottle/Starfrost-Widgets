#include "Menus.h"

namespace StarfrostWidgets
{
	void Menus::Install()
	{
		if (const auto ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(this);
			SKSE::log::info("Menu watcher installed");
		} else {
			SKSE::log::error("No UI singleton, menus will not hide the widgets");
		}

		Refresh();
	}

	void Menus::Refresh()
	{
		if (const auto ui = RE::UI::GetSingleton()) {
			hudOpen.store(ui->IsMenuOpen(RE::HUDMenu::MENU_NAME), std::memory_order_relaxed);
			loadingOpen.store(ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME), std::memory_order_relaxed);
		}
	}

	bool Menus::CoversScreen(const RE::BSFixedString& a_name)
	{
		const auto ui = RE::UI::GetSingleton();
		if (!ui) {
			return false;
		}

		if (a_name == RE::DialogueMenu::MENU_NAME) {
			return true;
		}

		const auto menu = ui->GetMenu(a_name);
		if (!menu) {
			return false;
		}

		using Flag = RE::UI_MENU_FLAGS;

		if (menu->menuFlags.any(Flag::kAlwaysOpen)) {
			return false;
		}

		const bool covers = menu->menuFlags.any(
			Flag::kPausesGame,
			Flag::kModal,
			Flag::kUsesCursor,
			Flag::kUsesMenuContext,
			Flag::kApplicationMenu,
			Flag::kFreezeFrameBackground);

		static std::set<std::string, std::less<>> reported;
		if (reported.emplace(a_name.c_str()).second) {
			SKSE::log::info("Menu {} flags {:#010x}, hides the widgets: {}",
				a_name.c_str(), menu->menuFlags.underlying(), covers);
		}

		return covers;
	}

	RE::BSEventNotifyControl Menus::ProcessEvent(const RE::MenuOpenCloseEvent*        a_event,
		RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
	{
		if (!a_event || !a_event->menuName.c_str()) {
			return RE::BSEventNotifyControl::kContinue;
		}

		// Both carry kAlwaysOpen, so CoversScreen never counts either one.
		if (a_event->menuName == RE::HUDMenu::MENU_NAME) {
			hudOpen.store(a_event->opening, std::memory_order_relaxed);
		} else if (a_event->menuName == RE::LoadingMenu::MENU_NAME) {
			loadingOpen.store(a_event->opening, std::memory_order_relaxed);
		}

		if (a_event->opening) {
			if (CoversScreen(a_event->menuName) && counted.emplace(a_event->menuName.c_str()).second) {
				covering.fetch_add(1, std::memory_order_relaxed);
			}
		} else if (const auto found = counted.find(a_event->menuName.c_str()); found != counted.end()) {
			counted.erase(found);
			covering.fetch_sub(1, std::memory_order_relaxed);
		}

		return RE::BSEventNotifyControl::kContinue;
	}
}
