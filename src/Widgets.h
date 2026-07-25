#pragma once

namespace StarfrostWidgets::Widgets
{
	// Draws every enabled gauge for the current frame. Must be called between
	// ImGui::NewFrame and ImGui::Render.
	void Draw();
}
