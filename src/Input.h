#pragma once

namespace StarfrostWidgets
{
	// Feeds ImGui from Skyrim's own input events rather than from a WndProc hook.
	// The game grabs the mouse and clips the cursor, so window messages are an
	// unreliable source; SKSE's events are not, and staying off the WndProc keeps
	// us out of ENB's and ReShade's way.
	class Input :
		public REX::Singleton<Input>,
		public RE::BSTEventSink<RE::InputEvent*>
	{
	public:
		void Install();

		// Shared core, driven either by the poll hook or - if that failed to
		// install - by the event sink.
		void ProcessEvents(RE::InputEvent* const* a_events);

		RE::BSEventNotifyControl ProcessEvent(
			RE::InputEvent* const*               a_event,
			RE::BSTEventSource<RE::InputEvent*>* a_source) override;

		// Whether the game should be shown an empty event list this frame.
		[[nodiscard]] bool ShouldSwallowInput() const { return EditMode(); }

		// --- render thread ---
		void PumpInto(ImGuiIO& a_io);
		void SetDisplaySize(float a_width, float a_height);

		// Mirrored out of ImGui so the input thread knows when a text field has
		// the keyboard and our own hotkeys should stand down.
		void SetWantTextInput(bool a_want) { _wantTextInput.store(a_want, std::memory_order_relaxed); }

		[[nodiscard]] bool EditMode() const { return _editMode.load(std::memory_order_relaxed); }

		void SetEditMode(bool a_enable);
		void LeaveEditMode() { SetEditMode(false); }

	private:
		friend class REX::Singleton<Input>;
		Input() = default;

		struct QueuedEvent
		{
			enum class Kind
			{
				kMousePos,
				kMouseButton,
				kMouseWheel,
				kKey,
				kCharacter
			};

			Kind     kind{};
			float    x{};
			float    y{};
			int      button{};
			bool     down{};
			ImGuiKey key{ ImGuiKey_None };
			unsigned codepoint{};
		};

		void Push(QueuedEvent a_event);
		void HandleKeyboard(const RE::ButtonEvent& a_button);

		std::mutex               _queueLock;
		std::vector<QueuedEvent> _queue;

		std::atomic<bool>  _editMode{ false };
		std::atomic<bool>  _controlsParked{ false };
		std::atomic<bool>  _wantTextInput{ false };
		std::atomic<bool>  _pollHookInstalled{ false };
		std::atomic<float> _displayWidth{ 1920.0f };
		std::atomic<float> _displayHeight{ 1080.0f };

		// Virtual cursor, driven by raw mouse deltas. Only meaningful in edit mode.
		float _cursorX{ 960.0f };
		float _cursorY{ 540.0f };
	};
}
