#include "Skin.h"

#include "GeneratedSkin.h"
#include "Input.h"
#include "Layout.h"
#include "Menus.h"
#include "Overlay.h"
#include "Settings.h"
#include "SurvivalData.h"

namespace StarfrostWidgets::Skin
{
	namespace
	{
		constexpr const char* kMenuName = "StarfrostWidgetsMenu";

		static_assert(std::size(Skin::kGaugeClips) == kGaugeCount,
			"The generated clip list and the Gauge enum have drifted apart - rerun tools/swfgen/build.py");

		constexpr double kHandoverSeconds = 5.0;
		constexpr int    kResolveAttempts = 300;

		std::atomic<bool>   gMovieLoaded{ false };
		std::atomic<bool>   gDriving{ false };
		std::atomic<double> gLoadedAt{ 0.0 };
		bool                gRegistered{ false };

		[[nodiscard]] double Now()
		{
			return std::chrono::duration<double>(
				std::chrono::steady_clock::now().time_since_epoch())
			    .count();
		}

		class MovieLog : public RE::GFxLog
		{
		public:
			void LogMessageVarg(LogMessageType a_type, const char* a_fmt, std::va_list a_args) override
			{
				char text[1024]{};
				if (std::vsnprintf(text, sizeof(text), a_fmt, a_args) <= 0) {
					return;
				}

				std::string_view message{ text };
				while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
					message.remove_suffix(1);
				}

				const auto subtype = static_cast<std::uint32_t>(a_type) & 0x0F;
				if (subtype == static_cast<std::uint32_t>(LogMessageType::kMessageType_Error)) {
					SKSE::log::error("Scaleform: {}", message);
				} else {
					SKSE::log::info("Scaleform: {}", message);
				}
			}
		};

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

			std::size_t ringFillFrames{ 1 };
			std::size_t barFillFrames{ 1 };
			std::size_t iconFrames{ 1 };
			std::size_t slotFrames{ 1 };
		};

		[[nodiscard]] std::size_t FrameForFraction(float a_fraction, std::size_t a_frames)
		{
			if (a_frames <= 1) {
				return 1;
			}
			const auto last = static_cast<float>(a_frames - 1);
			return 1 + static_cast<std::size_t>(std::clamp(a_fraction, 0.0f, 1.0f) * last + 0.5f);
		}

		void GotoFrame(RE::GFxValue& a_clip, std::size_t a_frame)
		{
			char buffer[8]{};
			a_clip.GotoAndStop(FrameLabel(a_frame, buffer));
		}

		// What was last pushed, so an unchanged frame costs nothing.
		struct Pushed
		{
			bool        visible{ true };
			float       x{ -1.0f };
			float       y{ -1.0f };
			float       scale{ -1.0f };
			float       alpha{ -1.0f };
			WidgetStyle style{ WidgetStyle::kRing };
			std::size_t ringFrame{ SIZE_MAX };
			std::size_t barFrame{ SIZE_MAX };
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
					RE::GFxMovieView::ScaleModeType::kNoScale, [](RE::GFxMovieDef* a_def) {
						static auto log = RE::make_gptr<MovieLog>();
						a_def->SetState(RE::GFxState::StateType::kLog, log.get());
					});

				if (loaded && uiMovie) {
					// Top-left alignment with no scaling makes _root coordinates plain
					// screen pixels, which is what the fractional positions expect.
					uiMovie->SetViewAlignment(RE::GFxMovieView::AlignType::kTopLeft);
					gLoadedAt.store(Now(), std::memory_order_relaxed);
					gMovieLoaded.store(true, std::memory_order_release);
					SKSE::log::info("Widget movie loaded, waiting for it to start drawing");
				} else {
					SKSE::log::warn("No widget movie at Interface/{}.swf", Skin::kMovieName);
				}

				// Never takes input; the HUD keeps its own.
				inputContext = Context::kNone;
				depthPriority = 0;
				menuFlags.set(Flag::kAlwaysOpen, Flag::kRequiresUpdate, Flag::kAllowSaving);
			}

			~WidgetMenu() override
			{
				gDriving.store(false, std::memory_order_release);
				gMovieLoaded.store(false, std::memory_order_release);
			}

			void AdvanceMovie(float a_interval, std::uint32_t a_currentTime) override
			{
				if (!uiMovie) {
					return;
				}
				if (gMovieLoaded.load(std::memory_order_acquire)) {
					Push(a_interval);
				}
				RE::IMenu::AdvanceMovie(a_interval, a_currentTime);
			}

			void PostDisplay() override
			{
				if (_resolved && !gDriving.exchange(true, std::memory_order_release)) {
					SKSE::log::info("The skin is drawing the widgets");
				}
				RE::IMenu::PostDisplay();
			}

		private:
			bool Resolve()
			{
				if (_resolved || _failed || !uiMovie) {
					return _resolved;
				}

				const auto get = [&](RE::GFxValue& a_out, const std::string& a_path) {
					return uiMovie->GetVariable(&a_out, a_path.c_str()) && a_out.IsDisplayObject();
				};

				const auto frames = [&](const RE::GFxValue& a_clip, const std::string& a_path) -> std::size_t {
					RE::GFxValue total;
					if (a_clip.IsDisplayObject() &&
						uiMovie->GetVariable(&total, (a_path + "._totalframes").c_str()) &&
						total.IsNumber() && total.GetNumber() >= 1.0) {
						return static_cast<std::size_t>(total.GetNumber());
					}
					return 1;
				};

				std::size_t bound = 0;
				std::size_t drawable = 0;
				std::string missing;

				for (std::size_t i = 0; i < kGaugeCount; ++i) {
					auto&             clips = _clips[i];
					const std::string base = std::format("_root.{}", Skin::kGaugeClips[i]);

					if (!get(clips.root, base)) {
						continue;
					}
					++bound;

					const auto want = [&](RE::GFxValue& a_out, const char* a_suffix) {
						if (get(a_out, base + a_suffix)) {
							return true;
						}
						if (missing.size() < 200) {
							missing += ' ';
							missing += Skin::kGaugeClips[i];
							missing += a_suffix;
						}
						return false;
					};

					const bool ring = want(clips.ring, ".ring");
					const bool ringFill = want(clips.ringFill, ".ring.fill");
					const bool bar = want(clips.bar, ".bar");
					const bool barFill = want(clips.barFill, ".bar.fill");
					const bool icon = want(clips.icon, ".icon");
					want(clips.caption, ".caption");

					for (std::uint32_t badge = 0; badge < kBuffAttributeCount; ++badge) {
						get(clips.badge[badge], std::format("{}.badges.badge{}", base, badge));
					}
					for (std::size_t slot = 0; slot < Skin::kCaptionSlots; ++slot) {
						get(clips.slot[slot], std::format("{}.caption.c{}", base, slot));
					}

					if ((ring && ringFill) || (bar && barFill) || icon) {
						++drawable;
					}

					clips.ringFillFrames = frames(clips.ringFill, base + ".ring.fill");
					clips.barFillFrames = frames(clips.barFill, base + ".bar.fill");
					clips.iconFrames = frames(clips.icon, base + ".icon");
					clips.slotFrames = frames(clips.slot[0], base + ".caption.c0");
				}

				if (drawable < kGaugeCount && ++_attempts < kResolveAttempts) {
					return false;
				}

				if (drawable == 0) {
					SKSE::log::error("Skin bound {} of {} gauge clips, none of which carry ring, bar or "
									 "icon art - falling back to the built-in drawing. Missing:{}",
						bound, kGaugeCount, missing.empty() ? " _root.sfw_*" : missing.c_str());
					gDriving.store(false, std::memory_order_release);
					gMovieLoaded.store(false, std::memory_order_release);
					_failed = true;
					return false;
				}

				if (!missing.empty()) {
					SKSE::log::warn("Skin is missing clips, those parts will not draw:{}", missing);
				}

				// Nothing in the movie stops itself, so park every multi-frame clip
				// on its first frame before it plays through on its own.
				for (auto& clips : _clips) {
					for (auto* clip : { &clips.ringFill, &clips.barFill, &clips.icon }) {
						if (clip->IsDisplayObject()) {
							GotoFrame(*clip, 1);
						}
					}
					for (auto& slot : clips.slot) {
						if (slot.IsDisplayObject()) {
							GotoFrame(slot, 1);
						}
					}
				}

				_resolved = true;
				SKSE::log::info("Widget movie resolved, {} of {} gauges have drawable art",
					drawable, kGaugeCount);
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

				Pump(settings, a_deltaTime);
			}

			void Pump(const Settings& a_settings, float a_deltaTime)
			{
				if (Overlay::Installed()) {
					return;
				}

				_poll += a_deltaTime;
				if (_poll < a_settings.pollInterval) {
					return;
				}
				_poll = 0.0f;

				if (const auto tasks = SKSE::GetTaskInterface()) {
					tasks->AddTask([]() {
						Menus::GetSingleton()->Refresh();
						SurvivalData::GetSingleton()->Refresh();
					});
				}
			}

			void PushGauge(Gauge a_gauge, const Settings& a_settings, const GaugeState& a_state,
				bool a_survivalOn, bool a_editing, float a_fade, float a_width, float a_height)
			{
				const auto index = static_cast<std::size_t>(a_gauge);
				auto&      clips = _clips[index];
				auto&      last = _pushed[index];
				const auto& widget = a_settings.Widget(a_gauge);

				if (!clips.root.IsDisplayObject()) {
					return;
				}

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
				PushFills(clips, last, a_state);
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

				if (a_clips.ring.IsDisplayObject()) {
					const bool ring = a_widget.style == WidgetStyle::kRing;
					SetVisible(a_clips.ring, ring);
					if (ring) {
						SetTransform(a_clips.ring, a_size.x * 0.5f, a_size.y * 0.5f,
							outer * 100.0f / Skin::kRingArtRadius);
					}
				}

				if (a_clips.bar.IsDisplayObject()) {
					const bool bar = a_widget.style == WidgetStyle::kBar;
					SetVisible(a_clips.bar, bar);
					if (bar) {
						SetTransform(a_clips.bar, 0.0, 0.0, a_size.x * 100.0f / Skin::kBarArtWidth);
					}
				}

				if (!a_clips.icon.IsDisplayObject()) {
					return;
				}

				switch (a_widget.style) {
				case WidgetStyle::kIcon:
					SetTransform(a_clips.icon, a_size.x * 0.5f, a_size.y * 0.5f,
						outer * 0.78f * 100.0f / Skin::kIconArtRadius);
					break;

				case WidgetStyle::kBar: {
					const float iconRadius = a_size.y * 0.42f;
					SetTransform(a_clips.icon, iconRadius, a_size.y * 0.5f,
						iconRadius * 100.0f / Skin::kIconArtRadius);
					break;
				}

				case WidgetStyle::kRing:
				default:
					SetTransform(a_clips.icon, a_size.x * 0.5f, a_size.y * 0.5f,
						outer * 0.62f * 100.0f / Skin::kIconArtRadius);
					break;
				}
			}

			void PushFills(GaugeClips& a_clips, Pushed& a_last, const GaugeState& a_state)
			{
				const float fill = std::clamp(a_state.fill, 0.0f, 1.0f);

				if (a_clips.ringFill.IsDisplayObject()) {
					const auto frame = FrameForFraction(fill, a_clips.ringFillFrames);
					if (frame != a_last.ringFrame) {
						a_last.ringFrame = frame;
						GotoFrame(a_clips.ringFill, frame);
					}
				}

				if (a_clips.barFill.IsDisplayObject()) {
					const auto frame = FrameForFraction(fill, a_clips.barFillFrames);
					if (frame != a_last.barFrame) {
						a_last.barFrame = frame;
						GotoFrame(a_clips.barFill, frame);
					}
				}

				// Only the injury icon carries tier frames; the rest hold a single one.
				const auto tier = Layout::IconTier(a_state);
				if (tier != a_last.tier) {
					a_last.tier = tier;
					if (a_clips.icon.IsDisplayObject()) {
						GotoFrame(a_clips.icon, std::min(tier + 1, a_clips.iconFrames));
					}
				}
			}

			void PushColor(GaugeClips& a_clips, Pushed& a_last, const WidgetSettings& a_widget,
				const GaugeState& a_state)
			{
				const ImU32 color = Layout::StageColor(a_widget, a_state);
				if (color == a_last.color) {
					return;
				}
				a_last.color = color;

				for (auto* clip : { &a_clips.icon, &a_clips.ringFill, &a_clips.barFill }) {
					if (clip->IsDisplayObject()) {
						SetTint(*clip, color);
					}
				}
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

				for (auto& badge : a_clips.badge) {
					if (badge.IsDisplayObject()) {
						SetVisible(badge, false);
					}
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
					if (clip.IsDisplayObject()) {
						SetTransform(clip, x, y, radius * 100.0f / Skin::kBadgeArtRadius);
						SetVisible(clip, true);
					}
					x += step;
				}
			}

			void PushCaption(GaugeClips& a_clips, Pushed& a_last, const Settings& a_settings,
				const GaugeState& a_state, ImVec2 a_size, float a_scale)
			{
				if (!a_clips.caption.IsDisplayObject()) {
					return;
				}

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
					if (!a_clips.slot[slot].IsDisplayObject()) {
						continue;
					}
					const auto found = slot < caption.size() ? kOrder.find(caption[slot]) :
															   std::string_view::npos;
					const auto glyph = found == std::string_view::npos ? 0 : found;
					GotoFrame(a_clips.slot[slot], std::min(glyph + 1, a_clips.slotFrames));
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
			int        _attempts{ 0 };
			float      _poll{ 0.0f };
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
		return gDriving.load(std::memory_order_acquire) &&
		       Settings::GetSingleton()->renderer != Renderer::kBuiltIn;
	}

	bool SuppressBuiltIn()
	{
		if (Settings::GetSingleton()->renderer == Renderer::kSkin || Active()) {
			return true;
		}

		return gMovieLoaded.load(std::memory_order_acquire) &&
		       Now() - gLoadedAt.load(std::memory_order_relaxed) < kHandoverSeconds;
	}
}
