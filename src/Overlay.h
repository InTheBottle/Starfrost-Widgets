#pragma once

namespace StarfrostWidgets::Overlay
{
	// Patches IDXGISwapChain::Present so we get a slot to draw in after the game
	// has finished its own frame. Returns false if the swap chain is not up yet;
	// the caller can try again on a later SKSE message.
	bool Install();

	[[nodiscard]] bool Installed();
}
