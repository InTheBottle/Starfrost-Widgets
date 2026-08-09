#pragma once

namespace StarfrostWidgets::Skin
{
	// Registers the widget movie as a menu. Safe to call when no skin is installed.
	void Install();

	// Opens the menu once a game is up, if a movie was found.
	void Show();

	// Main thread only. Reopens the menu if something closed it.
	void Tick();

	// True when the movie loaded and is the thing drawing the HUD.
	[[nodiscard]] bool Active();

	// True when the built-in drawing must stand down, including the "skin only"
	// case where a skin failed and drawing nothing is the point.
	[[nodiscard]] bool SuppressBuiltIn();
}
