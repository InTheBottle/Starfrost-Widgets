#pragma once

namespace StarfrostWidgets::Overlay
{
	// Patches IDXGISwapChain::Present. False means the swap chain is not up yet; retry later.
	bool Install();

	[[nodiscard]] bool Installed();
}
