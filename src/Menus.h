#pragma once

#include <atomic>
#include <set>
#include <string>

namespace StarfrostWidgets
{
	class Menus :
		public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
		public REX::Singleton<Menus>
	{
	public:
		void Install();

		[[nodiscard]] bool CoveringMenuOpen() const { return covering.load(std::memory_order_relaxed) != 0; }

		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent*        a_event,
			RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;

	private:
		friend class REX::Singleton<Menus>;
		Menus() = default;

		[[nodiscard]] static bool CoversScreen(const RE::BSFixedString& a_name);

		std::atomic<std::uint32_t>        covering{ 0 };
		std::set<std::string, std::less<>> counted{};
	};
}
