#include "Input.h"

#include "Settings.h"

namespace StarfrostWidgets
{
	namespace
	{
		constexpr std::uint32_t kEscapeScanCode = 0x01;  // DIK_ESCAPE

		// Skyrim's mouse button ids: 0/1/2 are the three buttons, 8 and 9 are the
		// wheel notches.
		constexpr std::uint32_t kWheelUp = 8;
		constexpr std::uint32_t kWheelDown = 9;

		// Raw device deltas are roughly pixel-scaled already; this just takes the
		// edge off so the cursor is not twitchy at high DPI.
		constexpr float kCursorSensitivity = 1.0f;

		// Fallback for when the poll hook could not be installed: stop the player
		// swinging a sword or spinning the camera while dragging a widget around.
		constexpr RE::ControlMap::UEFlag kParkedControls = static_cast<RE::ControlMap::UEFlag>(
			std::to_underlying(RE::ControlMap::UEFlag::kMovement) |
			std::to_underlying(RE::ControlMap::UEFlag::kLooking) |
			std::to_underlying(RE::ControlMap::UEFlag::kActivate) |
			std::to_underlying(RE::ControlMap::UEFlag::kFighting) |
			std::to_underlying(RE::ControlMap::UEFlag::kSneaking) |
			std::to_underlying(RE::ControlMap::UEFlag::kMainFour) |
			std::to_underlying(RE::ControlMap::UEFlag::kPOVSwitch) |
			std::to_underlying(RE::ControlMap::UEFlag::kWheelZoom) |
			std::to_underlying(RE::ControlMap::UEFlag::kJumping) |
			std::to_underlying(RE::ControlMap::UEFlag::kConsole) |
			std::to_underlying(RE::ControlMap::UEFlag::kVATS));

		// DirectInput scan code to ImGuiKey. Typed text arrives separately as char
		// events, so this only has to carry navigation, editing and the keys that
		// take part in shortcuts - but the whole keyboard is cheap enough to map.
		[[nodiscard]] ImGuiKey ScanCodeToImGuiKey(std::uint32_t a_code)
		{
			switch (a_code) {
			case 0x01: return ImGuiKey_Escape;
			case 0x02: return ImGuiKey_1;
			case 0x03: return ImGuiKey_2;
			case 0x04: return ImGuiKey_3;
			case 0x05: return ImGuiKey_4;
			case 0x06: return ImGuiKey_5;
			case 0x07: return ImGuiKey_6;
			case 0x08: return ImGuiKey_7;
			case 0x09: return ImGuiKey_8;
			case 0x0A: return ImGuiKey_9;
			case 0x0B: return ImGuiKey_0;
			case 0x0C: return ImGuiKey_Minus;
			case 0x0D: return ImGuiKey_Equal;
			case 0x0E: return ImGuiKey_Backspace;
			case 0x0F: return ImGuiKey_Tab;
			case 0x10: return ImGuiKey_Q;
			case 0x11: return ImGuiKey_W;
			case 0x12: return ImGuiKey_E;
			case 0x13: return ImGuiKey_R;
			case 0x14: return ImGuiKey_T;
			case 0x15: return ImGuiKey_Y;
			case 0x16: return ImGuiKey_U;
			case 0x17: return ImGuiKey_I;
			case 0x18: return ImGuiKey_O;
			case 0x19: return ImGuiKey_P;
			case 0x1A: return ImGuiKey_LeftBracket;
			case 0x1B: return ImGuiKey_RightBracket;
			case 0x1C: return ImGuiKey_Enter;
			case 0x1D: return ImGuiKey_LeftCtrl;
			case 0x1E: return ImGuiKey_A;
			case 0x1F: return ImGuiKey_S;
			case 0x20: return ImGuiKey_D;
			case 0x21: return ImGuiKey_F;
			case 0x22: return ImGuiKey_G;
			case 0x23: return ImGuiKey_H;
			case 0x24: return ImGuiKey_J;
			case 0x25: return ImGuiKey_K;
			case 0x26: return ImGuiKey_L;
			case 0x27: return ImGuiKey_Semicolon;
			case 0x28: return ImGuiKey_Apostrophe;
			case 0x29: return ImGuiKey_GraveAccent;
			case 0x2A: return ImGuiKey_LeftShift;
			case 0x2B: return ImGuiKey_Backslash;
			case 0x2C: return ImGuiKey_Z;
			case 0x2D: return ImGuiKey_X;
			case 0x2E: return ImGuiKey_C;
			case 0x2F: return ImGuiKey_V;
			case 0x30: return ImGuiKey_B;
			case 0x31: return ImGuiKey_N;
			case 0x32: return ImGuiKey_M;
			case 0x33: return ImGuiKey_Comma;
			case 0x34: return ImGuiKey_Period;
			case 0x35: return ImGuiKey_Slash;
			case 0x36: return ImGuiKey_RightShift;
			case 0x37: return ImGuiKey_KeypadMultiply;
			case 0x38: return ImGuiKey_LeftAlt;
			case 0x39: return ImGuiKey_Space;
			case 0x3A: return ImGuiKey_CapsLock;
			case 0x3B: return ImGuiKey_F1;
			case 0x3C: return ImGuiKey_F2;
			case 0x3D: return ImGuiKey_F3;
			case 0x3E: return ImGuiKey_F4;
			case 0x3F: return ImGuiKey_F5;
			case 0x40: return ImGuiKey_F6;
			case 0x41: return ImGuiKey_F7;
			case 0x42: return ImGuiKey_F8;
			case 0x43: return ImGuiKey_F9;
			case 0x44: return ImGuiKey_F10;
			case 0x45: return ImGuiKey_NumLock;
			case 0x46: return ImGuiKey_ScrollLock;
			case 0x47: return ImGuiKey_Keypad7;
			case 0x48: return ImGuiKey_Keypad8;
			case 0x49: return ImGuiKey_Keypad9;
			case 0x4A: return ImGuiKey_KeypadSubtract;
			case 0x4B: return ImGuiKey_Keypad4;
			case 0x4C: return ImGuiKey_Keypad5;
			case 0x4D: return ImGuiKey_Keypad6;
			case 0x4E: return ImGuiKey_KeypadAdd;
			case 0x4F: return ImGuiKey_Keypad1;
			case 0x50: return ImGuiKey_Keypad2;
			case 0x51: return ImGuiKey_Keypad3;
			case 0x52: return ImGuiKey_Keypad0;
			case 0x53: return ImGuiKey_KeypadDecimal;
			case 0x57: return ImGuiKey_F11;
			case 0x58: return ImGuiKey_F12;
			case 0x9C: return ImGuiKey_KeypadEnter;
			case 0x9D: return ImGuiKey_RightCtrl;
			case 0xB5: return ImGuiKey_KeypadDivide;
			case 0xB8: return ImGuiKey_RightAlt;
			case 0xC7: return ImGuiKey_Home;
			case 0xC8: return ImGuiKey_UpArrow;
			case 0xC9: return ImGuiKey_PageUp;
			case 0xCB: return ImGuiKey_LeftArrow;
			case 0xCD: return ImGuiKey_RightArrow;
			case 0xCF: return ImGuiKey_End;
			case 0xD0: return ImGuiKey_DownArrow;
			case 0xD1: return ImGuiKey_PageDown;
			case 0xD2: return ImGuiKey_Insert;
			case 0xD3: return ImGuiKey_Delete;
			default: return ImGuiKey_None;
			}
		}

		// The game normally never asks the keyboard device for character events.
		// Turning them on is what makes typing into an ImGui field possible; the
		// counter behind it is shared, so every enable needs its disable.
		void SetTextInputAllowed(bool a_allow)
		{
			if (const auto controlMap = RE::ControlMap::GetSingleton()) {
				controlMap->AllowTextInput(a_allow);
			}
		}

		// Swallowing input at the source is cleaner than disabling controls: there
		// is no game state left switched off if we never get to switch it back.
		struct PollInputDevices
		{
			static void thunk(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent* const* a_events)
			{
				auto* input = Input::GetSingleton();

				if (a_events) {
					input->ProcessEvents(a_events);
				}

				if (input->ShouldSwallowInput()) {
					constexpr RE::InputEvent* const dummy[] = { nullptr };
					func(a_dispatcher, dummy);
					return;
				}

				func(a_dispatcher, a_events);
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	void Input::Install()
	{
		try {
			const auto site = REL::RelocationID(67315, 68617).address() + REL::Relocate(0x7B, 0x7B);

			// Only patch if there really is a near call here. On a runtime this
			// offset does not fit, that check is the difference between falling
			// back quietly and writing five bytes over the middle of something.
			if (*reinterpret_cast<const std::uint8_t*>(site) != 0xE8) {
				SKSE::log::warn("No call instruction at the input poll site; falling back to an event sink");
			} else {
				auto& trampoline = SKSE::GetTrampoline();
				PollInputDevices::func = trampoline.write_call<5>(site, PollInputDevices::thunk);
				_pollHookInstalled.store(true, std::memory_order_relaxed);
				SKSE::log::info("Input poll hook installed");
			}
		} catch (const std::exception& e) {
			SKSE::log::warn("Input poll hook failed ({}); falling back to an event sink", e.what());
		}

		if (!_pollHookInstalled.load(std::memory_order_relaxed)) {
			if (const auto manager = RE::BSInputDeviceManager::GetSingleton()) {
				manager->AddEventSink(this);
				SKSE::log::info("Input sink installed");
			} else {
				SKSE::log::error("No input device manager; edit mode will be unavailable");
			}
		}
	}

	void Input::SetDisplaySize(float a_width, float a_height)
	{
		_displayWidth.store(a_width, std::memory_order_relaxed);
		_displayHeight.store(a_height, std::memory_order_relaxed);
	}

	void Input::SetEditMode(bool a_enable)
	{
		if (_editMode.exchange(a_enable) == a_enable) {
			return;
		}

		if (a_enable) {
			// Start the cursor in the middle rather than wherever the last session
			// left it, so it is always findable.
			_cursorX = _displayWidth.load(std::memory_order_relaxed) * 0.5f;
			_cursorY = _displayHeight.load(std::memory_order_relaxed) * 0.5f;
			Push({ .kind = QueuedEvent::Kind::kMousePos, .x = _cursorX, .y = _cursorY });
			SetTextInputAllowed(true);
		} else {
			SetTextInputAllowed(false);
			_wantTextInput.store(false, std::memory_order_relaxed);
			// Release any button ImGui still thinks is held, or the next edit
			// session starts mid-drag.
			Push({ .kind = QueuedEvent::Kind::kMouseButton, .button = 0, .down = false });
			Settings::GetSingleton()->Save();
		}

		// Only needed when the poll hook is absent - it already stops the game
		// seeing anything at all.
		if (!_pollHookInstalled.load(std::memory_order_relaxed)) {
			if (const auto controlMap = RE::ControlMap::GetSingleton()) {
				const bool parked = _controlsParked.load(std::memory_order_relaxed);
				if (a_enable && !parked) {
					controlMap->ToggleControls(kParkedControls, false, false);
					_controlsParked.store(true, std::memory_order_relaxed);
				} else if (!a_enable && parked) {
					controlMap->ToggleControls(kParkedControls, true, false);
					_controlsParked.store(false, std::memory_order_relaxed);
				}
			}
		}

		SKSE::log::info("Edit mode {}", a_enable ? "on" : "off");
	}

	void Input::Push(QueuedEvent a_event)
	{
		const std::scoped_lock lock{ _queueLock };
		// A stuck frame must not grow this without bound.
		if (_queue.size() < 512) {
			_queue.push_back(a_event);
		}
	}

	void Input::HandleKeyboard(const RE::ButtonEvent& a_button)
	{
		const auto  code = a_button.GetIDCode();
		const auto* settings = Settings::GetSingleton();
		const bool  editing = EditMode();
		const bool  typing = _wantTextInput.load(std::memory_order_relaxed);

		// While a text field has the keyboard, Esc belongs to ImGui for cancelling
		// the edit, and a letter-bound toggle key belongs to the field.
		if (!typing) {
			if (code == settings->editModeKey && a_button.IsDown()) {
				SetEditMode(!editing);
				return;
			}
			if (editing && code == kEscapeScanCode && a_button.IsDown()) {
				SetEditMode(false);
				return;
			}
		}

		if (!editing) {
			return;
		}

		const auto key = ScanCodeToImGuiKey(code);
		if (key == ImGuiKey_None) {
			return;
		}

		if (!a_button.IsDown() && !a_button.IsUp()) {
			return;  // key repeat; ImGui does its own repeating
		}

		const bool down = a_button.IsDown();
		Push({ .kind = QueuedEvent::Kind::kKey, .down = down, .key = key });

		// ImGui wants the merged modifier state alongside the physical key, which
		// is what makes ctrl+click on a slider open its text box.
		static bool leftCtrl = false, rightCtrl = false;
		static bool leftShift = false, rightShift = false;
		static bool leftAlt = false, rightAlt = false;

		switch (key) {
		case ImGuiKey_LeftCtrl: leftCtrl = down; break;
		case ImGuiKey_RightCtrl: rightCtrl = down; break;
		case ImGuiKey_LeftShift: leftShift = down; break;
		case ImGuiKey_RightShift: rightShift = down; break;
		case ImGuiKey_LeftAlt: leftAlt = down; break;
		case ImGuiKey_RightAlt: rightAlt = down; break;
		default: return;
		}

		Push({ .kind = QueuedEvent::Kind::kKey, .down = leftCtrl || rightCtrl, .key = ImGuiMod_Ctrl });
		Push({ .kind = QueuedEvent::Kind::kKey, .down = leftShift || rightShift, .key = ImGuiMod_Shift });
		Push({ .kind = QueuedEvent::Kind::kKey, .down = leftAlt || rightAlt, .key = ImGuiMod_Alt });
	}

	void Input::ProcessEvents(RE::InputEvent* const* a_events)
	{
		if (!a_events) {
			return;
		}

		for (auto event = *a_events; event; event = event->next) {
			switch (event->eventType.get()) {
			case RE::INPUT_EVENT_TYPE::kChar:
				{
					if (!EditMode()) {
						break;
					}
					const auto* character = static_cast<const RE::CharEvent*>(event);
					Push({ .kind = QueuedEvent::Kind::kCharacter, .codepoint = character->keyCode });
				}
				break;

			case RE::INPUT_EVENT_TYPE::kMouseMove:
				{
					if (!EditMode()) {
						break;
					}
					const auto* move = static_cast<const RE::MouseMoveEvent*>(event);
					const float width = _displayWidth.load(std::memory_order_relaxed);
					const float height = _displayHeight.load(std::memory_order_relaxed);

					_cursorX = std::clamp(_cursorX + static_cast<float>(move->mouseInputX) * kCursorSensitivity, 0.0f, width);
					_cursorY = std::clamp(_cursorY + static_cast<float>(move->mouseInputY) * kCursorSensitivity, 0.0f, height);
					Push({ .kind = QueuedEvent::Kind::kMousePos, .x = _cursorX, .y = _cursorY });
				}
				break;

			case RE::INPUT_EVENT_TYPE::kButton:
				{
					const auto* button = static_cast<const RE::ButtonEvent*>(event);
					const auto  device = event->GetDevice();

					if (device == RE::INPUT_DEVICE::kKeyboard) {
						HandleKeyboard(*button);
					} else if (device == RE::INPUT_DEVICE::kMouse && EditMode()) {
						const auto code = button->GetIDCode();
						if (code == kWheelUp || code == kWheelDown) {
							if (button->IsDown()) {
								Push({ .kind = QueuedEvent::Kind::kMouseWheel, .y = code == kWheelUp ? 1.0f : -1.0f });
							}
						} else if (code < 3 && (button->IsDown() || button->IsUp())) {
							Push({ .kind = QueuedEvent::Kind::kMouseButton,
								.button = static_cast<int>(code),
								.down = button->IsDown() });
						}
					}
				}
				break;

			default:
				break;
			}
		}
	}

	RE::BSEventNotifyControl Input::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*)
	{
		ProcessEvents(a_event);
		return RE::BSEventNotifyControl::kContinue;
	}

	void Input::PumpInto(ImGuiIO& a_io)
	{
		std::vector<QueuedEvent> drained;
		{
			const std::scoped_lock lock{ _queueLock };
			drained.swap(_queue);
		}

		for (const auto& event : drained) {
			switch (event.kind) {
			case QueuedEvent::Kind::kMousePos:
				a_io.AddMousePosEvent(event.x, event.y);
				break;
			case QueuedEvent::Kind::kMouseButton:
				a_io.AddMouseButtonEvent(event.button, event.down);
				break;
			case QueuedEvent::Kind::kMouseWheel:
				a_io.AddMouseWheelEvent(0.0f, event.y);
				break;
			case QueuedEvent::Kind::kKey:
				a_io.AddKeyEvent(event.key, event.down);
				break;
			case QueuedEvent::Kind::kCharacter:
				a_io.AddInputCharacter(event.codepoint);
				break;
			}
		}

		if (!EditMode()) {
			// Park the cursor off-screen so nothing can be hovered or clicked
			// while the widgets are meant to be inert.
			a_io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
		}
	}
}
