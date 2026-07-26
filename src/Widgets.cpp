#include "Widgets.h"

#include "Input.h"
#include "Menus.h"
#include "Settings.h"
#include "SurvivalData.h"

namespace StarfrostWidgets::Widgets
{
	namespace
	{
		constexpr const char* kNames[kGaugeCount] = {
			"Hunger", "Sleep", "Injury", "Cold", "Food", "Alcohol", "Blessing", "Stress"
		};
		static_assert(kNames[kGaugeCount - 1] != nullptr, "A new Gauge needs a display name");

		// Health, magicka, stamina - the Skyrim bar colours, near enough - then warmth.
		constexpr ImU32 kAttributeColors[kBuffAttributeCount] = {
			IM_COL32(214, 74, 66, 255),
			IM_COL32(78, 138, 222, 255),
			IM_COL32(94, 186, 96, 255),
			IM_COL32(232, 146, 62, 255)
		};

		// Footprints at scale 1.0, sized against a 1080p HUD.
		constexpr float kRingBox = 76.0f;
		constexpr float kIconBox = 56.0f;
		constexpr ImVec2 kBarBox{ 190.0f, 46.0f };

		// IM_PI lives in imgui_internal.h, which is not worth pulling in.
		constexpr float kPi = 3.14159265358979323846f;

		constexpr ImU32 kTrackColor = IM_COL32(12, 12, 14, 150);
		constexpr ImU32 kBackingColor = IM_COL32(8, 8, 10, 120);
		constexpr ImU32 kShadowColor = IM_COL32(0, 0, 0, 190);

		constexpr ImU32 kIdleColor = IM_COL32(146, 143, 136, 255);
		constexpr float kIdleAlpha = 0.45f;

		// Set for one frame after the panel moves a widget; otherwise the window owns its position.
		bool sPositionsDirty = true;

		[[nodiscard]] ImVec2 operator+(const ImVec2& a_lhs, const ImVec2& a_rhs)
		{
			return { a_lhs.x + a_rhs.x, a_lhs.y + a_rhs.y };
		}

		// This file draws from the render thread, which must not poke game state.
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

		[[nodiscard]] float AlphaOf(ImU32 a_color)
		{
			return static_cast<float>((a_color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
		}

		// Buff timers count down, so "8h 02m" beats a bare number of seconds.
		[[nodiscard]] std::string FormatDuration(float a_seconds)
		{
			const auto total = static_cast<int>(std::max(a_seconds, 0.0f) + 0.5f);
			if (total >= 3600) {
				return std::format("{}h {:02}m", total / 3600, (total % 3600) / 60);
			}
			if (total >= 60) {
				return std::format("{}:{:02}", total / 60, total % 60);
			}
			return std::format("{}s", total);
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

			// Outline, so the text reads over snow and sky alike.
			a_list->AddText(font, a_size, pos + ImVec2{ 1.0f, 1.0f }, WithAlpha(kShadowColor, AlphaOf(a_color)), a_text);
			a_list->AddText(font, a_size, pos, a_color, a_text);
		}

		// Icons all draw inside a circle of a_radius around a_center.

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
			// Two lobes and a point make a heart.
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

			// Wider crack for a worse tier.
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

				// Barbs turn three crossed lines into a snowflake.
				for (const float branch : { angle + kPi * 0.72f, angle - kPi * 0.72f }) {
					const ImVec2 offset{ std::cos(branch) * a_radius * 0.26f, std::sin(branch) * a_radius * 0.26f };
					a_list->AddLine(tip, tip + offset, a_color, thickness * 0.8f);
					a_list->AddLine(tail, ImVec2{ tail.x - offset.x, tail.y - offset.y }, a_color, thickness * 0.8f);
				}
			}
		}

		void DrawFoodBuffIcon(ImDrawList* a_list, ImVec2 a_center, float a_radius, ImU32 a_color)
		{
			// A bowl of soup: a half disc under an overhanging rim, steam rising off it.
			const float rimY = a_center.y + a_radius * 0.18f;
			const float bowl = a_radius * 0.72f;

			a_list->PathArcTo({ a_center.x, rimY }, bowl, 0.0f, kPi, 24);
			a_list->PathFillConvex(a_color);
			a_list->AddLine({ a_center.x - bowl * 1.18f, rimY }, { a_center.x + bowl * 1.18f, rimY },
				a_color, a_radius * 0.17f);

			// Three ribbons with the same lean, so they read as rising rather than as bars.
			for (int ribbon = 0; ribbon < 3; ++ribbon) {
				const float baseY = rimY - a_radius * 0.24f;
				const float originX = a_center.x + (static_cast<float>(ribbon) - 1.0f) * a_radius * 0.38f;
				const float height = a_radius * (ribbon == 1 ? 0.90f : 0.68f);

				constexpr int kSteps = 5;
				ImVec2       points[kSteps];
				for (int step = 0; step < kSteps; ++step) {
					const float t = static_cast<float>(step) / static_cast<float>(kSteps - 1);
					points[step] = { originX + std::sin(t * kPi * 1.6f) * a_radius * 0.14f, baseY - height * t };
				}
				a_list->AddPolyline(points, kSteps, a_color, ImDrawFlags_None, a_radius * 0.11f);
			}
		}

		void DrawAlcoholIcon(ImDrawList* a_list, ImVec2 a_center, float a_radius, ImU32 a_color)
		{
			// A mead bottle: square-shouldered body, tapered neck, corked.
			const float bodyHalf = a_radius * 0.42f;
			const float neckHalf = a_radius * 0.135f;
			const float bodyTop = a_center.y - a_radius * 0.10f;
			const float bodyBottom = a_center.y + a_radius * 0.92f;
			const float neckBottom = a_center.y - a_radius * 0.34f;
			const float neckTop = a_center.y - a_radius * 0.70f;
			const ImU32 seam = WithAlpha(IM_COL32(16, 13, 11, 255), AlphaOf(a_color) * 0.9f);

			// Only the base rounds, so the shoulder meets a flat edge instead of a notch.
			a_list->AddRectFilled({ a_center.x - bodyHalf, bodyTop }, { a_center.x + bodyHalf, bodyBottom },
				a_color, a_radius * 0.16f, ImDrawFlags_RoundCornersBottom);

			const ImVec2 shoulder[4] = {
				{ a_center.x - bodyHalf, bodyTop },
				{ a_center.x - neckHalf, neckBottom },
				{ a_center.x + neckHalf, neckBottom },
				{ a_center.x + bodyHalf, bodyTop }
			};
			a_list->AddConvexPolyFilled(shoulder, 4, a_color);
			a_list->AddRectFilled({ a_center.x - neckHalf, neckTop }, { a_center.x + neckHalf, neckBottom }, a_color);

			// Cork, held off the neck by a dark seam so the two do not merge.
			a_list->AddRectFilled({ a_center.x - a_radius * 0.22f, a_center.y - a_radius * 0.96f },
				{ a_center.x + a_radius * 0.22f, a_center.y - a_radius * 0.66f }, a_color, a_radius * 0.05f);
			a_list->AddLine({ a_center.x - a_radius * 0.22f, a_center.y - a_radius * 0.665f },
				{ a_center.x + a_radius * 0.22f, a_center.y - a_radius * 0.665f }, seam, a_radius * 0.07f);

			// A label knocked out of the body, so the bottle is not one flat blob.
			a_list->AddRectFilled({ a_center.x - bodyHalf, a_center.y + a_radius * 0.24f },
				{ a_center.x + bodyHalf, a_center.y + a_radius * 0.54f }, seam);
		}

		void DrawBlessingIcon(ImDrawList* a_list, ImVec2 a_center, float a_radius, ImU32 a_color)
		{
			// Mara's shrine medallion: two concentric bands, four knotwork petals
			// crossing the inner one on the cardinals, and a plain boss at the middle.
			a_list->AddCircle(a_center, a_radius * 0.95f, a_color, 32, a_radius * 0.14f);
			a_list->AddCircle(a_center, a_radius * 0.55f, a_color, 28, a_radius * 0.085f);

			constexpr int kSteps = 11;
			for (int petal = 0; petal < 4; ++petal) {
				const float  angle = kPi * 0.5f * static_cast<float>(petal);
				const ImVec2 axis{ std::cos(angle), std::sin(angle) };
				const ImVec2 normal{ -axis.y, axis.x };

				// A lens that tapers to a point at the boss and again at the outer band.
				ImVec2 points[kSteps * 2];
				for (int step = 0; step < kSteps; ++step) {
					const float t = static_cast<float>(step) / static_cast<float>(kSteps - 1);
					const float along = a_radius * (0.18f + 0.72f * t);
					const float across = a_radius * 0.20f * std::sin(t * kPi);

					points[step] = { a_center.x + axis.x * along + normal.x * across,
						a_center.y + axis.y * along + normal.y * across };
					points[kSteps * 2 - 1 - step] = { a_center.x + axis.x * along - normal.x * across,
						a_center.y + axis.y * along - normal.y * across };
				}
				a_list->AddPolyline(points, kSteps * 2, a_color, ImDrawFlags_Closed, a_radius * 0.075f);
			}

			a_list->AddCircleFilled(a_center, a_radius * 0.19f, a_color, 20);
		}

		void DrawStressIcon(ImDrawList* a_list, ImVec2 a_center, float a_radius, ImU32 a_color)
		{
			const ImU32 hollow = WithAlpha(IM_COL32(14, 11, 12, 255), AlphaOf(a_color) * 0.92f);

			const float jawHalf = a_radius * 0.36f;
			a_list->AddCircleFilled({ a_center.x, a_center.y - a_radius * 0.22f }, a_radius * 0.72f, a_color, 32);

			const ImVec2 cheeks[4] = {
				{ a_center.x - a_radius * 0.645f, a_center.y + a_radius * 0.10f },
				{ a_center.x - a_radius * 0.46f, a_center.y + a_radius * 0.46f },
				{ a_center.x + a_radius * 0.46f, a_center.y + a_radius * 0.46f },
				{ a_center.x + a_radius * 0.645f, a_center.y + a_radius * 0.10f }
			};
			a_list->AddConvexPolyFilled(cheeks, 4, a_color);

			a_list->AddRectFilled({ a_center.x - jawHalf, a_center.y + a_radius * 0.38f },
				{ a_center.x + jawHalf, a_center.y + a_radius * 0.84f },
				a_color, a_radius * 0.16f, ImDrawFlags_RoundCornersBottom);

			for (const float side : { -1.0f, 1.0f }) {
				a_list->AddCircleFilled({ a_center.x + side * a_radius * 0.33f, a_center.y - a_radius * 0.20f },
					a_radius * 0.245f, hollow, 18);
			}

			a_list->AddTriangleFilled(
				{ a_center.x - a_radius * 0.10f, a_center.y + a_radius * 0.04f },
				{ a_center.x + a_radius * 0.10f, a_center.y + a_radius * 0.04f },
				{ a_center.x, a_center.y + a_radius * 0.28f },
				hollow);

			const float biteY = a_center.y + a_radius * 0.46f;
			a_list->AddLine({ a_center.x - a_radius * 0.30f, biteY }, { a_center.x + a_radius * 0.30f, biteY },
				hollow, std::max(1.0f, a_radius * 0.065f));
			for (int tooth = -1; tooth <= 1; ++tooth) {
				const float x = a_center.x + static_cast<float>(tooth) * a_radius * 0.15f;
				a_list->AddLine({ x, biteY }, { x, a_center.y + a_radius * 0.76f },
					hollow, std::max(1.0f, a_radius * 0.075f));
			}
		}

		// Circle, diamond, triangle and square, so the four stay apart by shape as well
		// as by colour - at badge size the shape is what actually carries.
		void DrawAttributeBadge(ImDrawList* a_list, std::uint32_t a_index, ImVec2 a_center, float a_radius, float a_alpha)
		{
			const ImU32 fill = WithAlpha(kAttributeColors[a_index], a_alpha);

			a_list->AddCircleFilled(a_center, a_radius * 1.42f, WithAlpha(IM_COL32(10, 9, 8, 255), a_alpha), 12);

			switch (a_index) {
			case 0:
				a_list->AddCircleFilled(a_center, a_radius, fill, 12);
				break;
			case 1: {
				const ImVec2 diamond[4] = {
					{ a_center.x, a_center.y - a_radius * 1.18f },
					{ a_center.x + a_radius * 1.18f, a_center.y },
					{ a_center.x, a_center.y + a_radius * 1.18f },
					{ a_center.x - a_radius * 1.18f, a_center.y }
				};
				a_list->AddConvexPolyFilled(diamond, 4, fill);
				break;
			}
			case 2:
				a_list->AddTriangleFilled(
					{ a_center.x, a_center.y - a_radius * 1.18f },
					{ a_center.x + a_radius * 1.06f, a_center.y + a_radius * 0.80f },
					{ a_center.x - a_radius * 1.06f, a_center.y + a_radius * 0.80f },
					fill);
				break;
			default:
				a_list->AddRectFilled(
					{ a_center.x - a_radius * 0.90f, a_center.y - a_radius * 0.90f },
					{ a_center.x + a_radius * 0.90f, a_center.y + a_radius * 0.90f },
					fill, a_radius * 0.22f);
				break;
			}
		}

		// A centred row across the bottom of the widget box.
		void DrawAttributeBadges(ImDrawList* a_list, ImVec2 a_origin, ImVec2 a_size,
			const GaugeState& a_state, float a_scale, float a_alpha)
		{
			std::uint32_t present[kBuffAttributeCount]{};
			std::uint32_t count = 0;
			for (std::uint32_t i = 0; i < kBuffAttributeCount; ++i) {
				if (a_state.attributes & (1u << i)) {
					present[count++] = i;
				}
			}

			if (count == 0) {
				return;
			}

			const float radius = 4.4f * a_scale;
			const float step = radius * 3.2f;
			const float y = a_origin.y + a_size.y - radius * 1.5f;
			float       x = a_origin.x + a_size.x * 0.5f - step * (static_cast<float>(count) - 1.0f) * 0.5f;

			for (std::uint32_t i = 0; i < count; ++i) {
				DrawAttributeBadge(a_list, present[i], { x, y }, radius, a_alpha);
				x += step;
			}
		}

		[[nodiscard]] std::size_t IconTier(const GaugeState& a_state)
		{
			if (a_state.tiered) {
				return static_cast<std::size_t>(std::clamp(a_state.value, 0.0f, 3.0f));
			}
			return std::min(a_state.stage, kStageCount - 1) / 2;
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
			case Gauge::kFoodBuff:
				DrawFoodBuffIcon(a_list, a_center, a_radius, a_color);
				break;
			case Gauge::kAlcohol:
				DrawAlcoholIcon(a_list, a_center, a_radius, a_color);
				break;
			case Gauge::kBlessing:
				DrawBlessingIcon(a_list, a_center, a_radius, a_color);
				break;
			case Gauge::kStress:
				DrawStressIcon(a_list, a_center, a_radius, a_color);
				break;
			case Gauge::kCold:
			default:
				DrawColdIcon(a_list, a_center, a_radius, a_color);
				break;
			}
		}

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

			DrawIcon(a_list, a_gauge, center, outer * 0.62f, WithAlpha(a_color, a_alpha), IconTier(a_state));
		}

		void DrawIconGauge(ImDrawList* a_list, ImVec2 a_origin, ImVec2 a_size, Gauge a_gauge,
			const GaugeState& a_state, ImU32 a_color, float a_alpha)
		{
			const ImVec2 center{ a_origin.x + a_size.x * 0.5f, a_origin.y + a_size.y * 0.5f };
			const float  outer = std::min(a_size.x, a_size.y) * 0.5f;

			a_list->AddCircleFilled(center, outer, WithAlpha(kBackingColor, a_alpha * 0.6f), 32);
			DrawIcon(a_list, a_gauge, center, outer * 0.78f, WithAlpha(a_color, a_alpha), IconTier(a_state));
		}

		void DrawBarGauge(ImDrawList* a_list, ImVec2 a_origin, ImVec2 a_size, Gauge a_gauge,
			const GaugeState& a_state, ImU32 a_color, float a_alpha)
		{
			const float  iconRadius = a_size.y * 0.42f;
			const ImVec2 iconCenter{ a_origin.x + iconRadius, a_origin.y + a_size.y * 0.5f };
			DrawIcon(a_list, a_gauge, iconCenter, iconRadius, WithAlpha(a_color, a_alpha), IconTier(a_state));

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

		[[nodiscard]] bool ShouldHideForUI(const Settings& a_settings)
		{
			const auto ui = RE::UI::GetSingleton();
			if (!ui) {
				return true;
			}

			const auto menus = Menus::GetSingleton();

			// Not a covering menu: the loading screen keeps kAlwaysOpen throughout.
			if (menus->LoadingScreenOpen()) {
				return true;
			}
			// No HUD means the main menu.
			if (!menus->HUDOpen()) {
				return true;
			}
			if (a_settings.hideWhenHUDHidden && !ui->IsShowingMenus()) {
				return true;
			}
			if (a_settings.hideInMenus && menus->CoveringMenuOpen()) {
				return true;
			}

			return false;
		}

		void DrawGaugeBody(ImDrawList* a_list, ImVec2 a_origin, ImVec2 a_size, Gauge a_gauge,
			const WidgetSettings& a_widget, const GaugeState& a_state, const Settings& a_settings, float a_scale)
		{
			const bool  idle = a_state.timer && !a_state.active;
			const ImU32 color = idle ? kIdleColor : a_widget.stageColors[std::min(a_state.stage, kStageCount - 1)];
			float       alpha = a_settings.opacity * (idle ? kIdleAlpha : 1.0f);

			// Only the top stage pulses; anything more and the HUD never settles.
			if (!idle && a_settings.pulseAtCritical && a_state.stage >= kStageCount - 1) {
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

			const auto caption = [&](const std::string& a_text, float a_drop) {
				AddCenteredText(a_list,
					{ a_origin.x + a_size.x * 0.5f, a_origin.y + a_size.y + a_drop * a_scale },
					ImGui::GetFontSize() * a_scale,
					WithAlpha(IM_COL32(235, 232, 226, 255), alpha),
					a_text.c_str());
			};

			if (a_state.timer) {
				if (idle) {
					return;
				}
				if (a_settings.showAttributes) {
					DrawAttributeBadges(a_list, a_origin, a_size, a_state, a_scale, alpha);
				}
				if (a_settings.showTimers) {
					// Clear of the badge row, which sits inside the bottom of the box.
					caption(FormatDuration(a_state.value), 11.0f);
				}
			} else if (a_settings.showValues) {
				caption(a_state.tiered ?
						std::format("{}/3", static_cast<int>(a_state.value)) :
						std::format("{}", static_cast<int>(a_state.value)),
					8.0f);
			}
		}

		// A bare swatch; clicking it opens ImGui's own picker popup.
		void StageColorSwatch(const char* a_id, ImU32& a_color)
		{
			float rgb[3] = {
				static_cast<float>((a_color >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
				static_cast<float>((a_color >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
				static_cast<float>((a_color >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f
			};

			if (ImGui::ColorEdit3(a_id, rgb,
					ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha)) {
				a_color = IM_COL32(
					static_cast<int>(rgb[0] * 255.0f + 0.5f),
					static_cast<int>(rgb[1] * 255.0f + 0.5f),
					static_cast<int>(rgb[2] * 255.0f + 0.5f),
					0xFF);
			}
		}

		// Six swatches in a row, low stage to high, plus the two edits worth having.
		void DrawStageColors(Settings& a_settings, WidgetSettings& a_widget, bool a_isTimer)
		{
			ImGui::TextDisabled("Icon colours");
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			ImGui::SetItemTooltip(a_isTimer ?
					"The icon, ring and bar all draw in the colour for the current stage.\n"
					"Left to right: freshly applied through about to expire.\n"
					"Click a swatch to pick a colour; set all six the same for a flat colour." :
					"The icon, ring and bar all draw in the colour for the current stage.\n"
					"Left to right: satisfied through critical.\n"
					"Click a swatch to pick a colour; set all six the same for a flat colour.");

			for (std::size_t stage = 0; stage < kStageCount; ++stage) {
				if (stage > 0) {
					ImGui::SameLine();
				}
				const auto id = std::format("##stagecolor{}", stage);
				StageColorSwatch(id.c_str(), a_widget.stageColors[stage]);
				ImGui::SetItemTooltip("Stage %d", static_cast<int>(stage));
			}

			if (ImGui::SmallButton("Reset colours")) {
				for (std::size_t stage = 0; stage < kStageCount; ++stage) {
					a_widget.stageColors[stage] = PackStageColor(kDefaultStageRamp[stage]);
				}
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Copy to all widgets")) {
				for (std::size_t other = 0; other < kGaugeCount; ++other) {
					for (std::size_t stage = 0; stage < kStageCount; ++stage) {
						a_settings.widgets[other].stageColors[stage] = a_widget.stageColors[stage];
					}
				}
			}
			ImGui::SetItemTooltip("Give every widget this widget's six colours.");
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
			ImGui::SetItemTooltip("Applies to hunger, sleep and cold. Injuries, stress and buff timers always show.");
			ImGui::SameLine();
			ImGui::Checkbox("Show values", &a_settings.showValues);
			ImGui::Checkbox("Pulse at critical", &a_settings.pulseAtCritical);
			ImGui::Checkbox("Show time left", &a_settings.showTimers);
			ImGui::SameLine();
			ImGui::Checkbox("Show buff badges", &a_settings.showAttributes);
			ImGui::SetItemTooltip(
				"Red circle = health, blue diamond = magicka,\n"
				"green triangle = stamina, orange square = warmth.");

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

				const bool isTimer = IsTimerGauge(gauge);

				ImGui::PushID(static_cast<int>(i));
				if (ImGui::CollapsingHeader(kNames[i], i < 3 ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
					ImGui::Checkbox("Enabled", &widget.enabled);
					ImGui::SameLine();
					ImGui::Checkbox("Dynamic", &widget.dynamic);
					ImGui::SetItemTooltip(isTimer ?
							"Stay hidden until the buff starts running down." :
							"Stay hidden until there is something worth showing.");

					ImGui::BeginDisabled(!widget.dynamic);
					int showFrom = static_cast<int>(widget.showFromStage);
					if (ImGui::SliderInt("Show from stage", &showFrom, 1, static_cast<int>(kStageCount) - 1)) {
						widget.showFromStage = static_cast<std::size_t>(std::clamp(showFrom, 1, static_cast<int>(kStageCount) - 1));
					}
					ImGui::SetItemTooltip(isTimer ?
							"1 appears as soon as the buff is past full, 5 only in the last moments.\n"
							"The swatches below show which colour each stage draws in." :
							"1 appears at the first sign, 5 only when critical.\n"
							"The swatches below show which colour each stage draws in.");
					ImGui::EndDisabled();

					int style = static_cast<int>(widget.style);
					if (ImGui::Combo("Style", &style, "Ring\0Icon\0Bar\0")) {
						widget.style = static_cast<WidgetStyle>(style);
					}

					ImGui::SliderFloat("Scale", &widget.scale, 0.25f, 3.0f, "%.2f");
					sPositionsDirty |= ImGui::SliderFloat("X", &widget.posX, 0.0f, 1.0f, "%.4f");
					sPositionsDirty |= ImGui::SliderFloat("Y", &widget.posY, 0.0f, 1.0f, "%.4f");

					DrawStageColors(a_settings, widget, isTimer);
					ImGui::Separator();

					if (!state.available) {
						ImGui::TextDisabled(isTimer ? "no source - the mod that grants this buff is not installed" :
													  "no data - system off or forms missing");
					} else if (state.timer && !state.active) {
						ImGui::TextDisabled("idle - no buff running");
					} else if (state.timer) {
						ImGui::TextDisabled("%s   %s left of %s",
							state.label[0] ? state.label : "active",
							FormatDuration(state.value).c_str(),
							FormatDuration(state.maxValue).c_str());
					} else if (state.tiered) {
						ImGui::TextDisabled("tier %d of 3   %s",
							static_cast<int>(state.value),
							state.label[0] ? state.label : "none");
					} else {
						ImGui::TextDisabled("stage %d   value %.0f / %.0f",
							static_cast<int>(state.stage), state.value, state.maxValue);
					}
				}
				ImGui::PopID();
			}

			ImGui::Separator();
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
				// Injuries and the buff timers come from other mods, so they ignore Survival Mode.
				if (IsSurvivalNeed(gauge) && settings.requireSurvivalMode && !survivalOn) {
					continue;
				}
				if (widget.dynamic && state.stage < widget.showFromStage) {
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

			// A real window per widget, so ImGui handles the dragging.
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

				// Wherever the drag left it becomes the stored position.
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
