#include "Widgets.h"

#include "Input.h"
#include "Settings.h"
#include "SurvivalData.h"

namespace StarfrostWidgets::Widgets
{
	namespace
	{
		constexpr const char* kNames[kGaugeCount] = { "Hunger", "Sleep", "Injury", "Cold" };

		// Base pixel footprints at scale 1.0, sized against a 1080p HUD.
		constexpr float kRingBox = 76.0f;
		constexpr float kIconBox = 56.0f;
		constexpr ImVec2 kBarBox{ 190.0f, 46.0f };

		// imgui.h does not export IM_PI; that lives in imgui_internal.h and we do
		// not need the rest of it.
		constexpr float kPi = 3.14159265358979323846f;

		constexpr ImU32 kTrackColor = IM_COL32(12, 12, 14, 150);
		constexpr ImU32 kBackingColor = IM_COL32(8, 8, 10, 120);
		constexpr ImU32 kShadowColor = IM_COL32(0, 0, 0, 190);

		// Panel positions are pushed into the widget windows for one frame after
		// they change; the rest of the time the windows own their own position so
		// dragging works.
		bool sPositionsDirty = true;

		[[nodiscard]] ImVec2 operator+(const ImVec2& a_lhs, const ImVec2& a_rhs)
		{
			return { a_lhs.x + a_rhs.x, a_lhs.y + a_rhs.y };
		}

		// Everything in this file draws from the render thread. Game state must not
		// be poked from there, so anything that does goes through SKSE's task queue.
		template <class F>
		void RunOnMainThread(F&& a_work)
		{
			if (const auto tasks = SKSE::GetTaskInterface()) {
				tasks->AddTask(std::forward<F>(a_work));
			}
		}

		[[nodiscard]] ImU32 WithAlpha(ImU32 a_color, float a_alpha)
		{
			const auto alpha = static_cast<ImU32>(std::clamp(a_alpha, 0.0f, 1.0f) * 255.0f + 0.5f);
			return (a_color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
		}

		[[nodiscard]] ImVec2 BoxSize(const WidgetSettings& a_widget, float a_scale)
		{
			switch (a_widget.style) {
			case WidgetStyle::kIcon:
				return { kIconBox * a_scale, kIconBox * a_scale };
			case WidgetStyle::kBar:
				return { kBarBox.x * a_scale, kBarBox.y * a_scale };
			case WidgetStyle::kRing:
			default:
				return { kRingBox * a_scale, kRingBox * a_scale };
			}
		}

		void AddCenteredText(ImDrawList* a_list, ImVec2 a_center, float a_size, ImU32 a_color, const char* a_text)
		{
			const auto  font = ImGui::GetFont();
			const ImVec2 extent = font->CalcTextSizeA(a_size, FLT_MAX, 0.0f, a_text);
			const ImVec2 pos{ a_center.x - extent.x * 0.5f, a_center.y - extent.y * 0.5f };

			// Cheap outline, so the text stays readable over snow and over sky alike.
			a_list->AddText(font, a_size, pos + ImVec2{ 1.0f, 1.0f }, WithAlpha(kShadowColor, (a_color >> IM_COL32_A_SHIFT & 0xFF) / 255.0f), a_text);
			a_list->AddText(font, a_size, pos, a_color, a_text);
		}

		// --- icons -----------------------------------------------------------
		// All of these draw inside a circle of the given radius around a_center,
		// so a gauge can swap styles without the artwork changing size.

		void DrawHungerIcon(ImDrawList* a_list, ImVec2 a_center, float a_radius, ImU32 a_color)
		{
			// A drumstick: bone running down-left, meat up-right.
			const ImVec2 boneEnd{ a_center.x - a_radius * 0.55f, a_center.y + a_radius * 0.55f };
			const ImVec2 boneJoint{ a_center.x + a_radius * 0.12f, a_center.y - a_radius * 0.12f };
			constexpr float kDiagonal = 0.70710678f;  // the bone runs at 45 degrees
			const ImVec2 perpendicular{ kDiagonal * a_radius * 0.16f, kDiagonal * a_radius * 0.16f };

			a_list->AddLine(boneEnd, boneJoint, a_color, a_radius * 0.20f);
			a_list->AddCircleFilled(boneEnd + perpendicular, a_radius * 0.155f, a_color, 12);
			a_list->AddCircleFilled(boneEnd + ImVec2{ -perpendicular.x, -perpendicular.y }, a_radius * 0.155f, a_color, 12);
			a_list->AddCircleFilled({ a_center.x + a_radius * 0.30f, a_center.y - a_radius * 0.30f }, a_radius * 0.44f, a_color, 20);
		}

		void DrawSleepIcon(ImDrawList* a_list, ImVec2 a_center, float a_radius, ImU32 a_color)
		{
			// A thick arc reads as a crescent without needing a concave fill.
			const ImVec2 moonCenter{ a_center.x + a_radius * 0.10f, a_center.y + a_radius * 0.06f };
			a_list->PathArcTo(moonCenter, a_radius * 0.50f, kPi * 0.38f, kPi * 1.62f, 24);
			a_list->PathStroke(a_color, 0, a_radius * 0.30f);

			// Two Zs drifting off the top-right horn.
			auto drawZ = [&](ImVec2 a_origin, float a_size) {
				const ImVec2 points[4] = {
					a_origin,
					{ a_origin.x + a_size, a_origin.y },
					{ a_origin.x, a_origin.y + a_size },
					{ a_origin.x + a_size, a_origin.y + a_size }
				};
				a_list->AddPolyline(points, 4, a_color, ImDrawFlags_None, std::max(1.0f, a_size * 0.24f));
			};

			drawZ({ a_center.x + a_radius * 0.28f, a_center.y - a_radius * 0.78f }, a_radius * 0.26f);
			drawZ({ a_center.x + a_radius * 0.62f, a_center.y - a_radius * 0.36f }, a_radius * 0.18f);
		}

		void DrawInjuryIcon(ImDrawList* a_list, ImVec2 a_center, float a_radius, ImU32 a_color, std::size_t a_tier)
		{
			// Two lobes and a point make a heart; the crack grows with the tier.
			const float lobeRadius = a_radius * 0.31f;
			a_list->AddCircleFilled({ a_center.x - a_radius * 0.29f, a_center.y - a_radius * 0.20f }, lobeRadius, a_color, 18);
			a_list->AddCircleFilled({ a_center.x + a_radius * 0.29f, a_center.y - a_radius * 0.20f }, lobeRadius, a_color, 18);
			a_list->AddTriangleFilled(
				{ a_center.x - a_radius * 0.585f, a_center.y - a_radius * 0.14f },
				{ a_center.x + a_radius * 0.585f, a_center.y - a_radius * 0.14f },
				{ a_center.x, a_center.y + a_radius * 0.62f },
				a_color);

			if (a_tier == 0) {
				return;
			}

			// A dark fissure punched through the middle. More tiers, wider crack.
			const float width = a_radius * (0.09f + 0.04f * static_cast<float>(a_tier));
			const ImVec2 crack[5] = {
				{ a_center.x - a_radius * 0.02f, a_center.y - a_radius * 0.46f },
				{ a_center.x + a_radius * 0.16f, a_center.y - a_radius * 0.16f },
				{ a_center.x - a_radius * 0.14f, a_center.y + a_radius * 0.04f },
				{ a_center.x + a_radius * 0.10f, a_center.y + a_radius * 0.26f },
				{ a_center.x - a_radius * 0.02f, a_center.y + a_radius * 0.52f }
			};
			a_list->AddPolyline(crack, 5, IM_COL32(10, 8, 10, 235), ImDrawFlags_None, width);
		}

		void DrawColdIcon(ImDrawList* a_list, ImVec2 a_center, float a_radius, ImU32 a_color)
		{
			const float thickness = a_radius * 0.14f;
			for (int spoke = 0; spoke < 3; ++spoke) {
				const float angle = kPi / 3.0f * static_cast<float>(spoke) + kPi * 0.5f;
				const ImVec2 axis{ std::cos(angle), std::sin(angle) };
				const ImVec2 tip{ a_center.x + axis.x * a_radius * 0.72f, a_center.y + axis.y * a_radius * 0.72f };
				const ImVec2 tail{ a_center.x - axis.x * a_radius * 0.72f, a_center.y - axis.y * a_radius * 0.72f };
				a_list->AddLine(tip, tail, a_color, thickness);

				// Barbs at both ends turn three crossed lines into a snowflake.
				for (const float branch : { angle + kPi * 0.72f, angle - kPi * 0.72f }) {
					const ImVec2 offset{ std::cos(branch) * a_radius * 0.26f, std::sin(branch) * a_radius * 0.26f };
					a_list->AddLine(tip, tip + offset, a_color, thickness * 0.8f);
					a_list->AddLine(tail, ImVec2{ tail.x - offset.x, tail.y - offset.y }, a_color, thickness * 0.8f);
				}
			}
		}

		void DrawIcon(ImDrawList* a_list, Gauge a_gauge, ImVec2 a_center, float a_radius, ImU32 a_color, std::size_t a_tier)
		{
			switch (a_gauge) {
			case Gauge::kHunger:
				DrawHungerIcon(a_list, a_center, a_radius, a_color);
				break;
			case Gauge::kSleep:
				DrawSleepIcon(a_list, a_center, a_radius, a_color);
				break;
			case Gauge::kInjury:
				DrawInjuryIcon(a_list, a_center, a_radius, a_color, a_tier);
				break;
			case Gauge::kCold:
			default:
				DrawColdIcon(a_list, a_center, a_radius, a_color);
				break;
			}
		}

		// --- gauge bodies ----------------------------------------------------

		void DrawRingGauge(ImDrawList* a_list, ImVec2 a_origin, ImVec2 a_size, Gauge a_gauge,
			const GaugeState& a_state, ImU32 a_color, float a_alpha)
		{
			const ImVec2 center{ a_origin.x + a_size.x * 0.5f, a_origin.y + a_size.y * 0.5f };
			const float  outer = std::min(a_size.x, a_size.y) * 0.5f;
			const float  thickness = outer * 0.20f;
			const float  radius = outer - thickness * 0.5f;

			a_list->AddCircleFilled(center, radius - thickness * 0.5f, WithAlpha(kBackingColor, a_alpha * 0.72f), 32);
			a_list->PathArcTo(center, radius, 0.0f, kPi * 2.0f, 48);
			a_list->PathStroke(WithAlpha(kTrackColor, a_alpha), ImDrawFlags_Closed, thickness);

			if (a_state.fill > 0.001f) {
				constexpr float kTop = -kPi * 0.5f;
				a_list->PathArcTo(center, radius, kTop, kTop + kPi * 2.0f * a_state.fill, 48);
				a_list->PathStroke(WithAlpha(a_color, a_alpha), ImDrawFlags_None, thickness);
			}

			DrawIcon(a_list, a_gauge, center, outer * 0.62f, WithAlpha(a_color, a_alpha), a_state.stage ? a_state.stage / 2 : 0);
		}

		void DrawIconGauge(ImDrawList* a_list, ImVec2 a_origin, ImVec2 a_size, Gauge a_gauge,
			const GaugeState& a_state, ImU32 a_color, float a_alpha)
		{
			const ImVec2 center{ a_origin.x + a_size.x * 0.5f, a_origin.y + a_size.y * 0.5f };
			const float  outer = std::min(a_size.x, a_size.y) * 0.5f;

			a_list->AddCircleFilled(center, outer, WithAlpha(kBackingColor, a_alpha * 0.6f), 32);
			DrawIcon(a_list, a_gauge, center, outer * 0.78f, WithAlpha(a_color, a_alpha), a_state.stage ? a_state.stage / 2 : 0);
		}

		void DrawBarGauge(ImDrawList* a_list, ImVec2 a_origin, ImVec2 a_size, Gauge a_gauge,
			const GaugeState& a_state, ImU32 a_color, float a_alpha)
		{
			const float  iconRadius = a_size.y * 0.42f;
			const ImVec2 iconCenter{ a_origin.x + iconRadius, a_origin.y + a_size.y * 0.5f };
			DrawIcon(a_list, a_gauge, iconCenter, iconRadius, WithAlpha(a_color, a_alpha), a_state.stage ? a_state.stage / 2 : 0);

			const float barLeft = a_origin.x + iconRadius * 2.3f;
			const float barHeight = a_size.y * 0.42f;
			const ImVec2 barMin{ barLeft, a_origin.y + (a_size.y - barHeight) * 0.5f };
			const ImVec2 barMax{ a_origin.x + a_size.x, barMin.y + barHeight };
			const float  rounding = barHeight * 0.5f;

			a_list->AddRectFilled(barMin, barMax, WithAlpha(kTrackColor, a_alpha), rounding);
			if (a_state.fill > 0.001f) {
				const float filledRight = barMin.x + (barMax.x - barMin.x) * a_state.fill;
				a_list->AddRectFilled(barMin, { std::max(filledRight, barMin.x + rounding), barMax.y },
					WithAlpha(a_color, a_alpha), rounding);
			}
		}

		// --- frame -----------------------------------------------------------

		[[nodiscard]] bool ShouldHideForUI(const Settings& a_settings)
		{
			const auto ui = RE::UI::GetSingleton();
			if (!ui) {
				return true;
			}

			// No HUD means a loading screen or the main menu.
			if (!ui->IsMenuOpen(RE::HUDMenu::MENU_NAME)) {
				return true;
			}
			if (a_settings.hideWhenHUDHidden && !ui->IsShowingMenus()) {
				return true;
			}
			if (a_settings.hideInMenus && (ui->GameIsPaused() || ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME))) {
				return true;
			}

			return false;
		}

		void DrawGaugeBody(ImDrawList* a_list, ImVec2 a_origin, ImVec2 a_size, Gauge a_gauge,
			const WidgetSettings& a_widget, const GaugeState& a_state, const Settings& a_settings, float a_scale)
		{
			ImU32 color = a_widget.stageColors[std::min(a_state.stage, kStageCount - 1)];
			float alpha = a_settings.opacity;

			// Only the top stage pulses; anything more and the HUD never settles.
			if (a_settings.pulseAtCritical && a_state.stage >= kStageCount - 1) {
				const auto now = static_cast<float>(ImGui::GetTime());
				alpha *= 0.62f + 0.38f * (0.5f + 0.5f * std::sin(now * 4.2f));
			}

			switch (a_widget.style) {
			case WidgetStyle::kIcon:
				DrawIconGauge(a_list, a_origin, a_size, a_gauge, a_state, color, alpha);
				break;
			case WidgetStyle::kBar:
				DrawBarGauge(a_list, a_origin, a_size, a_gauge, a_state, color, alpha);
				break;
			case WidgetStyle::kRing:
			default:
				DrawRingGauge(a_list, a_origin, a_size, a_gauge, a_state, color, alpha);
				break;
			}

			if (a_settings.showValues) {
				const auto text = a_gauge == Gauge::kInjury ?
				                      std::format("{}/3", static_cast<int>(a_state.value)) :
				                      std::format("{}", static_cast<int>(a_state.value));
				AddCenteredText(a_list,
					{ a_origin.x + a_size.x * 0.5f, a_origin.y + a_size.y + 8.0f * a_scale },
					ImGui::GetFontSize() * a_scale,
					WithAlpha(IM_COL32(235, 232, 226, 255), alpha),
					text.c_str());
			}
		}

		void DrawControlPanel(Settings& a_settings)
		{
			ImGui::SetNextWindowSize({ 420.0f, 0.0f }, ImGuiCond_Appearing);
			ImGui::SetNextWindowPos({ 60.0f, 60.0f }, ImGuiCond_Appearing);
			ImGui::SetNextWindowBgAlpha(0.92f);

			if (!ImGui::Begin("Starfrost Widgets", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::End();
				return;
			}

			ImGui::TextWrapped("Drag a widget to move it. Press Esc or your edit key to finish - settings save automatically.");
			ImGui::TextDisabled("Ctrl+click any slider to type an exact value.");
			ImGui::Separator();

			ImGui::Checkbox("Widgets enabled", &a_settings.enabled);
			ImGui::SliderFloat("Global scale", &a_settings.globalScale, 0.25f, 3.0f, "%.2f");
			ImGui::SliderFloat("Opacity", &a_settings.opacity, 0.05f, 1.0f, "%.2f");
			ImGui::Checkbox("Hide in menus", &a_settings.hideInMenus);
			ImGui::SameLine();
			ImGui::Checkbox("Follow HUD", &a_settings.hideWhenHUDHidden);
			ImGui::Checkbox("Only in Survival Mode", &a_settings.requireSurvivalMode);
			ImGui::SameLine();
			ImGui::Checkbox("Show values", &a_settings.showValues);
			ImGui::Checkbox("Pulse at critical", &a_settings.pulseAtCritical);

			int target = static_cast<int>(a_settings.renderTarget);
			if (ImGui::Combo("Draw into", &target, "Auto\0Swap chain\0Game framebuffer\0")) {
				a_settings.renderTarget = static_cast<RenderTarget>(target);
			}
			ImGui::SetItemTooltip(
				"Frame generation composites the back buffer itself, so widgets drawn there get\n"
				"overwritten. Auto and Game framebuffer put them in the same layer as the vanilla\n"
				"HUD, which survives that and is not interpolated. Switch to Swap chain only if\n"
				"the widgets misbehave without frame generation.");

			ImGui::Separator();

			const auto* data = SurvivalData::GetSingleton();
			for (std::size_t i = 0; i < kGaugeCount; ++i) {
				const auto gauge = static_cast<Gauge>(i);
				auto&      widget = a_settings.Widget(gauge);
				const auto& state = data->Get(gauge);

				ImGui::PushID(static_cast<int>(i));
				if (ImGui::CollapsingHeader(kNames[i], i < 3 ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
					ImGui::Checkbox("Enabled", &widget.enabled);
					ImGui::SameLine();
					ImGui::Checkbox("Hide when satisfied", &widget.hideWhenSatisfied);

					int style = static_cast<int>(widget.style);
					if (ImGui::Combo("Style", &style, "Ring\0Icon\0Bar\0")) {
						widget.style = static_cast<WidgetStyle>(style);
					}

					ImGui::SliderFloat("Scale", &widget.scale, 0.25f, 3.0f, "%.2f");
					sPositionsDirty |= ImGui::SliderFloat("X", &widget.posX, 0.0f, 1.0f, "%.4f");
					sPositionsDirty |= ImGui::SliderFloat("Y", &widget.posY, 0.0f, 1.0f, "%.4f");

					if (state.available) {
						ImGui::TextDisabled("stage %d   value %.0f / %.0f",
							static_cast<int>(state.stage), state.value, state.maxValue);
					} else {
						ImGui::TextDisabled("no data - system off or forms missing");
					}
				}
				ImGui::PopID();
			}

			ImGui::Separator();
			// This runs on the render thread, so anything that writes a file or
			// touches the control map is bounced to the main thread first.
			if (ImGui::Button("Save now")) {
				RunOnMainThread([]() { Settings::GetSingleton()->Save(); });
			}
			ImGui::SameLine();
			if (ImGui::Button("Stack down the left edge")) {
				for (std::size_t i = 0; i < kGaugeCount; ++i) {
					a_settings.widgets[i].posX = 0.030f;
					a_settings.widgets[i].posY = 0.300f + 0.075f * static_cast<float>(i);
				}
				sPositionsDirty = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Done")) {
				RunOnMainThread([]() { Input::GetSingleton()->LeaveEditMode(); });
			}

			ImGui::End();
		}
	}

	void Draw()
	{
		auto&      settings = *Settings::GetSingleton();
		const auto data = SurvivalData::GetSingleton();
		const auto input = Input::GetSingleton();
		const bool editing = input->EditMode();

		if (!editing && (!settings.enabled || ShouldHideForUI(settings))) {
			sPositionsDirty = true;  // re-seed window positions next time we edit
			return;
		}

		const bool  survivalOn = data->SurvivalModeEnabled();
		const auto& io = ImGui::GetIO();
		const ImVec2 display = io.DisplaySize;
		auto*        background = ImGui::GetBackgroundDrawList();

		for (std::size_t i = 0; i < kGaugeCount; ++i) {
			const auto  gauge = static_cast<Gauge>(i);
			auto&       widget = settings.Widget(gauge);
			const auto& state = data->Get(gauge);

			if (!widget.enabled) {
				continue;
			}
			if (!editing) {
				if (!state.available) {
					continue;
				}
				// Injuries are Blade & Blunt's, not Survival Mode's, so they keep
				// showing even with survival switched off.
				if (gauge != Gauge::kInjury && settings.requireSurvivalMode && !survivalOn) {
					continue;
				}
				if (widget.hideWhenSatisfied && state.stage == 0) {
					continue;
				}
			}

			const float  scale = widget.scale * settings.globalScale;
			const ImVec2 size = BoxSize(widget, scale);
			const ImVec2 origin{ widget.posX * display.x, widget.posY * display.y };

			if (!editing) {
				DrawGaugeBody(background, origin, size, gauge, widget, state, settings, scale);
				continue;
			}

			// Edit mode: a real window per widget, so ImGui handles the dragging.
			// Always while the panel is driving the position, Appearing otherwise -
			// which covers a widget being switched on part way through a session.
			const auto id = std::format("##sfw_gauge_{}", i);
			ImGui::SetNextWindowPos(origin, sPositionsDirty ? ImGuiCond_Always : ImGuiCond_Appearing);
			ImGui::SetNextWindowSize(size, ImGuiCond_Always);

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			if (ImGui::Begin(id.c_str(), nullptr,
					ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
						ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
						ImGuiWindowFlags_NoFocusOnAppearing)) {
				const ImVec2 windowPos = ImGui::GetWindowPos();
				auto*        list = ImGui::GetWindowDrawList();

				const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
				list->AddRectFilled(windowPos, windowPos + size, IM_COL32(255, 255, 255, hovered ? 34 : 16), 6.0f);
				list->AddRect(windowPos, windowPos + size, IM_COL32(255, 210, 140, hovered ? 220 : 130), 6.0f, 0, 1.5f);

				DrawGaugeBody(list, windowPos, size, gauge, widget, state, settings, scale);
				AddCenteredText(list, { windowPos.x + size.x * 0.5f, windowPos.y - 9.0f },
					ImGui::GetFontSize(), IM_COL32(255, 220, 160, 235), kNames[i]);

				// Whatever the drag left us at becomes the new stored position.
				widget.posX = display.x > 0.0f ? windowPos.x / display.x : widget.posX;
				widget.posY = display.y > 0.0f ? windowPos.y / display.y : widget.posY;
			}
			ImGui::End();
			ImGui::PopStyleVar(2);
		}

		if (editing) {
			sPositionsDirty = false;
			DrawControlPanel(settings);
		}
	}
}
