#include "Skin.h"

#include "GeneratedSkin.h"
#include "Input.h"
#include "Layout.h"
#include "Menus.h"
#include "Settings.h"
#include "SurvivalData.h"

namespace StarfrostWidgets::Skin
{
	namespace
	{
		constexpr const char* kMenuName = "StarfrostWidgetsMenu";

		static_assert(std::size(Skin::kGaugeClips) == kGaugeCount,
			"The generated clip list and the Gauge enum have drifted apart - rerun tools/swfgen/build.py");

		std::atomic<bool> gLoaded{ false };
		bool              gRegistered{ false };

		// A frame is addressed by its label, and every frame is labelled with its
		// own number, so the lookup lands on the same frame either way it is read.
		const char* FrameLabel(std::size_t a_frame, char (&a_buffer)[8])
		{
			const auto result = std::to_chars(a_buffer, a_buffer + sizeof(a_buffer) - 1, a_frame);
			*result.ptr = '\0';
			return a_buffer;
		}

		void SetTransform(RE::GFxValue& a_clip, double a_x, double a_y, double a_scale)
		{
			RE::GFxValue::DisplayInfo info;
			info.SetPosition(a_x, a_y);
			info.SetScale(a_scale, a_scale);
			a_clip.SetDisplayInfo(info);
		}

		void SetVisible(RE::GFxValue& a_clip, bool a_visible)
		{
			RE::GFxValue::DisplayInfo info;
			info.SetVisible(a_visible);
			a_clip.SetDisplayInfo(info);
		}

		// Multiply rather than blend, so white art takes the stage colour exactly
		// while the dark detail colours baked into the icons survive untouched.
		void SetTint(RE::GFxValue& a_clip, ImU32 a_color)
		{
			using Cxform = RE::GRenderer::Cxform;

			Cxform transform;
			transform.SetIdentity();
			transform.matrix[Cxform::kR][Cxform::kMult] =
				static_cast<float>((a_color >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
			transform.matrix[Cxform::kG][Cxform::kMult] =
				static_cast<float>((a_color >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
			transform.matrix[Cxform::kB][Cxform::kMult] =
				static_cast<float>((a_color >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
			a_clip.SetCxform(transform);
		}

		struct GaugeClips
		{
			RE::GFxValue root;
			RE::GFxValue ring;
			RE::GFxValue ringFill;
			RE::GFxValue bar;
			RE::GFxValue barFill;
			RE::GFxValue icon;
			RE::GFxValue badge[kBuffAttributeCount];
			RE::GFxValue caption;
			RE::GFxValue slot[Skin::kCaptionSlots];
			bool         resolved{ false };
		};

		// What was last pushed, so an unchanged frame costs nothing.
		struct Pushed
		{
			bool        visible{ false };
			float       x{ -1.0f };
			float       y{ -1.0f };
			float       scale{ -1.0f };
			float       alpha{ -1.0f };
			WidgetStyle style{ WidgetStyle::kRing };
			std::size_t fillFrame{ SIZE_MAX };
			std::size_t tier{ SIZE_MAX };
			ImU32       color{ 0 };
			std::uint32_t attributes{ UINT32_MAX };
			std::string caption{ "\x01" };
		};

		class WidgetMenu : public RE::IMenu
		{
		public:
			static RE::IMenu* Create() { return new WidgetMenu(); }

			WidgetMenu()
			{
				using Flag = RE::UI_MENU_FLAGS;

				auto* manager = RE::BSScaleformManager::GetSingleton();
				const bool loaded = manager && manager->LoadMovieEx(this, Skin::kMovieName,
					RE::GFxMovieView::ScaleModeType::kNoScale, [](RE::GFxMovieDef*) {});

				if (loaded && uiMovie) {
					// Top-left alignment with no scaling makes _root coordinates plain
					// screen pixels, which is what the fractional positions expect.
					uiMovie->SetViewAlignment(RE::GFxMovieView::AlignType::kTopLeft);
					gLoaded.store(true, std::memory_order_release);
				} else {
					SKSE::log::warn("No widget movie at Interface/{}.swf", Skin::kMovieName);
				}

				// Never takes input; the HUD keeps its own.
				inputContext = Context::kNone;
				depthPriority = 0;
				menuFlags.set(Flag::kAlwaysOpen, Flag::kRequiresUpdate, Flag::kAllowSaving);
			}

			void AdvanceMovie(float a_interval, std::uint32_t a_currentTime) override
			{
				if (!uiMovie) {
					return;
				}
				if (gLoaded.load(std::memory_order_acquire)) {
					Push(a_interval);
				}
				RE::IMenu::AdvanceMovie(a_interval, a_currentTime);
			}

		private:
			bool Resolve()
			{
				if (_resolved || _failed || !uiMovie) {
					return _resolved;
				}

				for (std::size_t i = 0; i < kGaugeCount; ++i) {
					auto&             clips = _clips[i];
					const std::string base = std::format("_root.{}", Skin::kGaugeClips[i]);

					const auto get = [&](RE::GFxValue& a_out, const std::string& a_path) {
						return uiMovie->GetVariable(&a_out, a_path.c_str()) && a_out.IsDisplayObject();
					};

					clips.resolved =
						get(clips.root, base) &&
						get(clips.ring, base + ".ring") &&
						get(clips.ringFill, base + ".ring.fill") &&
						get(clips.bar, base + ".bar") &&
						get(clips.barFill, base + ".bar.fill") &&
						get(clips.icon, base + ".icon") &&
						get(clips.caption, base + ".caption");

					for (std::uint32_t badge = 0; badge < kBuffAttributeCount && clips.resolved; ++badge) {
						clips.resolved = get(clips.badge[badge],
							std::format("{}.badges.badge{}", base, badge));
					}
					for (std::size_t slot = 0; slot < Skin::kCaptionSlots && clips.resolved; ++slot) {
						clips.resolved = get(clips.slot[slot],
							std::format("{}.caption.c{}", base, slot));
					}

					if (!clips.resolved) {
						SKSE::log::error("Skin is missing clips under {} - falling back to the built-in drawing", base);
						gLoaded.store(false, std::memory_order_release);
						_failed = true;
						return false;
					}
				}

				// Nothing in the movie stops itself, so park every multi-frame clip
				// on its first frame before it plays through on its own.
				char buffer[8]{};
				const char* first = FrameLabel(1, buffer);
				for (auto& clips : _clips) {
					clips.ringFill.GotoAndStop(first);
					clips.barFill.GotoAndStop(first);
					clips.icon.GotoAndStop(first);
					for (auto& slot : clips.slot) {
						slot.GotoAndStop(first);
					}
				}

				_resolved = true;
				SKSE::log::info("Widget movie resolved, {} gauges bound", kGaugeCount);
				return true;
			}

			void Push(float a_deltaTime)
			{
				if (!Resolve()) {
					return;
				}

				auto&      settings = *Settings::GetSingleton();
				const auto data = SurvivalData::GetSingleton();
				const bool editing = Input::GetSingleton()->EditMode();

				if (editing) {
					Layout::ForceFade(1.0f);
				} else {
					Layout::UpdateFade(settings.enabled && !Layout::ShouldHideForUI(settings), a_deltaTime);
				}

				const float fade = Layout::Fade();
				const bool  survivalOn = data->SurvivalModeEnabled();
				_time += a_deltaTime;

				const auto  state = RE::BSGraphics::State::GetSingleton();
				const float width = state ? static_cast<float>(state->screenWidth) : 1920.0f;
				const float height = state ? static_cast<float>(state->screenHeight) : 1080.0f;

				for (std::size_t i = 0; i < kGaugeCount; ++i) {
					PushGauge(static_cast<Gauge>(i), settings, data->Get(static_cast<Gauge>(i)),
						survivalOn, editing, fade, width, height);
				}
			}

			void PushGauge(Gauge a_gauge, const Settings& a_settings, const GaugeState& a_state,
				bool a_survivalOn, bool a_editing, float a_fade, float a_width, float a_height)
			{
				const auto index = static_cast<std::size_t>(a_gauge);
				auto&      clips = _clips[index];
				auto&      last = _pushed[index];
				const auto& widget = a_settings.Widget(a_gauge);

				const bool visible = a_editing ?
				                         widget.enabled :
				                         (a_fade > 0.001f &&
											 Layout::GaugeVisible(a_gauge, a_settings, a_state, a_survivalOn));

				if (visible != last.visible) {
					SetVisible(clips.root, visible);
					last.visible = visible;
				}
				if (!visible) {
					return;
				}

				const float  scale = widget.scale * a_settings.globalScale;
				const ImVec2 size = Layout::BoxSize(widget, scale);
				const float  originX = widget.posX * a_width;
				const float  originY = widget.posY * a_height;

				if (originX != last.x || originY != last.y) {
					RE::GFxValue::DisplayInfo info;
					info.SetPosition(originX, originY);
					clips.root.SetDisplayInfo(info);
					last.x = originX;
					last.y = originY;
				}

				const float alpha = a_editing ?
				                        a_settings.opacity * 100.0f :
				                        Layout::StageAlpha(a_settings, a_state, a_fade, _time) * 100.0f;
				if (std::abs(alpha - last.alpha) > 0.4f) {
					RE::GFxValue::DisplayInfo info;
					info.SetAlpha(alpha);
					clips.root.SetDisplayInfo(info);
					last.alpha = alpha;
				}

				PushStyle(clips, last, widget, size, scale);
				PushFills(clips, last, a_state, widget);
				PushColor(clips, last, widget, a_state);
				PushBadges(clips, last, a_settings, a_state, size, scale);
				PushCaption(clips, last, a_settings, a_state, size, scale);
			}

			void PushStyle(GaugeClips& a_clips, Pushed& a_last, const WidgetSettings& a_widget,
				ImVec2 a_size, float a_scale)
			{
				if (a_widget.style == a_last.style && a_scale == a_last.scale) {
					return;
				}
				a_last.style = a_widget.style;
				a_last.scale = a_scale;

				const float outer = std::min(a_size.x, a_size.y) * 0.5f;

				switch (a_widget.style) {
				case WidgetStyle::kIcon:
					SetVisible(a_clips.ring, false);
					SetVisible(a_clips.bar, false);
					SetTransform(a_clips.icon, a_size.x * 0.5f, a_size.y * 0.5f,
						outer * 0.78f * 100.0f / Skin::kIconArtRadius);
					break;

				case WidgetStyle::kBar: {
					SetVisible(a_clips.ring, false);
					SetVisible(a_clips.bar, true);
					SetTransform(a_clips.bar, 0.0, 0.0, a_size.x * 100.0f / Skin::kBarArtWidth);
					const float iconRadius = a_size.y * 0.42f;
					SetTransform(a_clips.icon, iconRadius, a_size.y * 0.5f,
						iconRadius * 100.0f / Skin::kIconArtRadius);
					break;
				}

				case WidgetStyle::kRing:
				default:
					SetVisible(a_clips.bar, false);
					SetVisible(a_clips.ring, true);
					SetTransform(a_clips.ring, a_size.x * 0.5f, a_size.y * 0.5f,
						outer * 100.0f / Skin::kRingArtRadius);
					SetTransform(a_clips.icon, a_size.x * 0.5f, a_size.y * 0.5f,
						outer * 0.62f * 100.0f / Skin::kIconArtRadius);
					break;
				}
			}

			void PushFills(GaugeClips& a_clips, Pushed& a_last, const GaugeState& a_state,
				const WidgetSettings& a_widget)
			{
				const auto frame = static_cast<std::size_t>(
					std::clamp(a_state.fill, 0.0f, 1.0f) * static_cast<float>(Skin::kFillSteps) + 0.5f);

				if (frame != a_last.fillFrame) {
					a_last.fillFrame = frame;
					char buffer[8]{};
					const char* label = FrameLabel(frame + 1, buffer);
					a_clips.ringFill.GotoAndStop(label);
					a_clips.barFill.GotoAndStop(label);
				}

				const auto tier = Layout::IconTier(a_state);
				if (tier != a_last.tier) {
					a_last.tier = tier;
					char buffer[8]{};
					// Only the injury icon carries tier frames; the rest hold a single one.
					a_clips.icon.GotoAndStop(FrameLabel(tier + 1, buffer));
				}
				(void)a_widget;
			}

			void PushColor(GaugeClips& a_clips, Pushed& a_last, const WidgetSettings& a_widget,
				const GaugeState& a_state)
			{
				const ImU32 color = Layout::StageColor(a_widget, a_state);
				if (color == a_last.color) {
					return;
				}
				a_last.color = color;

				SetTint(a_clips.icon, color);
				SetTint(a_clips.ringFill, color);
				SetTint(a_clips.barFill, color);
			}

			void PushBadges(GaugeClips& a_clips, Pushed& a_last, const Settings& a_settings,
				const GaugeState& a_state, ImVec2 a_size, float a_scale)
			{
				const std::uint32_t mask =
					(a_state.timer && a_state.active && a_settings.showAttributes) ? a_state.attributes : 0;
				if (mask == a_last.attributes) {
					return;
				}
				a_last.attributes = mask;

				std::uint32_t present[kBuffAttributeCount]{};
				std::uint32_t count = 0;
				for (std::uint32_t i = 0; i < kBuffAttributeCount; ++i) {
					if (mask & (1u << i)) {
						present[count++] = i;
					}
				}

				for (std::uint32_t i = 0; i < kBuffAttributeCount; ++i) {
					SetVisible(a_clips.badge[i], false);
				}

				if (count == 0) {
					return;
				}

				const float radius = 4.4f * a_scale;
				const float step = radius * 3.2f;
				const float y = a_size.y - radius * 1.5f;
				float       x = a_size.x * 0.5f - step * (static_cast<float>(count) - 1.0f) * 0.5f;

				for (std::uint32_t i = 0; i < count; ++i) {
					auto& clip = a_clips.badge[present[i]];
					SetTransform(clip, x, y, radius * 100.0f / Skin::kBadgeArtRadius);
					SetVisible(clip, true);
					x += step;
				}
			}

			void PushCaption(GaugeClips& a_clips, Pushed& a_last, const Settings& a_settings,
				const GaugeState& a_state, ImVec2 a_size, float a_scale)
			{
				std::string caption = Layout::CaptionFor(a_settings, a_state);
				if (caption.size() > Skin::kCaptionSlots) {
					caption.resize(Skin::kCaptionSlots);
				}
				if (caption == a_last.caption) {
					return;
				}
				a_last.caption = caption;

				if (caption.empty()) {
					SetVisible(a_clips.caption, false);
					return;
				}

				static constexpr std::string_view kOrder{ Skin::kGlyphOrder };
				for (std::size_t slot = 0; slot < Skin::kCaptionSlots; ++slot) {
					const auto found = slot < caption.size() ? kOrder.find(caption[slot]) :
															   std::string_view::npos;
					char       buffer[8]{};
					a_clips.slot[slot].GotoAndStop(
						FrameLabel((found == std::string_view::npos ? 0 : found) + 1, buffer));
				}

				const float size = Layout::kCaptionSize * a_scale;
				const float drop = a_state.timer ? Layout::kTimerDrop : Layout::kValueDrop;
				const float width = static_cast<float>(caption.size()) * Skin::kGlyphCell * size / 100.0f;

				SetTransform(a_clips.caption,
					a_size.x * 0.5f - width * 0.5f,
					a_size.y + drop * a_scale + size * 0.5f,
					size);
				SetVisible(a_clips.caption, true);
			}

			GaugeClips _clips[kGaugeCount]{};
			Pushed     _pushed[kGaugeCount]{};
			bool       _resolved{ false };
			bool       _failed{ false };
			double     _time{ 0.0 };
		};
	}

	void Install()
	{
		if (Settings::GetSingleton()->renderer == Renderer::kBuiltIn) {
			SKSE::log::info("iRenderer=1, the built-in drawing is in charge and no movie is loaded");
			return;
		}

		if (!RE::BSScaleformManager::FileExists("Interface/StarfrostWidgets.swf")) {
			SKSE::log::warn("Interface/StarfrostWidgets.swf not found, using the built-in drawing");
			return;
		}

		if (const auto ui = RE::UI::GetSingleton()) {
			ui->Register(kMenuName, WidgetMenu::Create);
			gRegistered = true;
			SKSE::log::info("Widget menu registered");
		}
	}

	void Show()
	{
		if (!gRegistered) {
			return;
		}

		const auto ui = RE::UI::GetSingleton();
		if (ui && ui->GetMenu(kMenuName)) {
			return;
		}
		if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
			queue->AddMessage(kMenuName, RE::UI_MESSAGE_TYPE::kShow, nullptr);
		}
	}

	bool Active()
	{
		return gLoaded.load(std::memory_order_acquire) &&
		       Settings::GetSingleton()->renderer != Renderer::kBuiltIn;
	}

	bool SuppressBuiltIn()
	{
		return Settings::GetSingleton()->renderer == Renderer::kSkin || Active();
	}
}
