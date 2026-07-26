#pragma once

namespace StarfrostWidgets
{
	// Fed from Skyrim's input events; a WndProc hook fights the cursor clip, ENB and ReShade.
	class Input :
		public REX::Singleton<Input>,
		public RE::BSTEventSink<RE::InputEvent*>
	{
	public:
		void Install();

		// Driven by the poll hook, or by the event sink if that failed to install.
		void ProcessEvents(RE::InputEvent* const* a_events);

		RE::BSEventNotifyControl ProcessEvent(
			RE::InputEvent* const*               a_event,
			RE::BSTEventSource<RE::InputEvent*>* a_source) override;

		// Whether the game should be shown an empty event list this frame.
		[[nodiscard]] bool ShouldSwallowInput() const { return EditMode(); }

		// --- render thread ---
		void PumpInto(ImGuiIO& a_io);
		void SetDisplaySize(float a_width, float a_height);

		// Mirrored out of ImGui so our hotkeys stand down while a field has the keyboard.
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
				kCharacter,
				kReset
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
		std::vector<QueuedEvent> _drained;

		bool _leftCtrl{ false };
		bool _rightCtrl{ false };
		bool _leftShift{ false };
		bool _rightShift{ false };
		bool _leftAlt{ false };
		bool _rightAlt{ false };

		std::atomic<bool>  _editMode{ false };
		std::atomic<bool>  _controlsParked{ false };
		std::atomic<bool>  _wantTextInput{ false };
		std::atomic<bool>  _pollHookInstalled{ false };
		std::atomic<float> _displayWidth{ 1920.0f };
		std::atomic<float> _displayHeight{ 1080.0f };

		// Driven by raw mouse deltas. Only meaningful in edit mode.
		float _cursorX{ 960.0f };
		float _cursorY{ 540.0f };
	};
}
