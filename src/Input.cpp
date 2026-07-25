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

		// Everything that would otherwise let the player swing a sword or spin the
		// camera while dragging a widget around.
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
			std::to_underlying(RE::ControlMap::UEFlag::kVATS));
	}

	void Input::Install()
	{
		if (const auto manager = RE::BSInputDeviceManager::GetSingleton()) {
			manager->AddEventSink(this);
			SKSE::log::info("Input sink installed");
		} else {
			SKSE::log::error("No input device manager; edit mode will be unavailable");
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
		} else {
			// Release any button ImGui still thinks is held, or the next edit
			// session starts mid-drag.
			Push({ .kind = QueuedEvent::Kind::kMouseButton, .button = 0, .down = false });
			Settings::GetSingleton()->Save();
		}

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

		SKSE::log::info("Edit mode {}", a_enable ? "on" : "off");
	}

	void Input::Push(QueuedEvent a_event)
	{
		const std::scoped_lock lock{ _queueLock };
		// A stuck frame must not grow this without bound.
		if (_queue.size() < 256) {
			_queue.push_back(a_event);
		}
	}

	RE::BSEventNotifyControl Input::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*)
	{
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}

		const auto* settings = Settings::GetSingleton();
		const bool  editing = EditMode();

		for (auto event = *a_event; event; event = event->next) {
			switch (event->eventType.get()) {
			case RE::INPUT_EVENT_TYPE::kMouseMove:
				{
					if (!editing) {
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
					const auto  code = button->GetIDCode();

					if (device == RE::INPUT_DEVICE::kKeyboard) {
						if (code == settings->editModeKey && button->IsDown()) {
							SetEditMode(!editing);
						} else if (editing && code == kEscapeScanCode && button->IsDown()) {
							SetEditMode(false);
						}
					} else if (device == RE::INPUT_DEVICE::kMouse && editing) {
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
			}
		}

		if (!EditMode()) {
			// Park the cursor off-screen so nothing can be hovered or clicked
			// while the widgets are meant to be inert.
			a_io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
		}
	}
}
