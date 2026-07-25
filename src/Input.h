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

		RE::BSEventNotifyControl ProcessEvent(
			RE::InputEvent* const*               a_event,
			RE::BSTEventSource<RE::InputEvent*>* a_source) override;

		// --- render thread ---
		void PumpInto(ImGuiIO& a_io);
		void SetDisplaySize(float a_width, float a_height);

		[[nodiscard]] bool EditMode() const { return _editMode.load(std::memory_order_relaxed); }

		// Toggling edit mode also parks the player's controls, so this must be
		// paired. LeaveEditMode is the safety valve for menu opens and save loads.
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
				kMouseWheel
			};

			Kind  kind{};
			float x{};
			float y{};
			int   button{};
			bool  down{};
		};

		void Push(QueuedEvent a_event);

		std::mutex               _queueLock;
		std::vector<QueuedEvent> _queue;

		std::atomic<bool>  _editMode{ false };
		std::atomic<bool>  _controlsParked{ false };
		std::atomic<float> _displayWidth{ 1920.0f };
		std::atomic<float> _displayHeight{ 1080.0f };

		// Virtual cursor, driven by raw mouse deltas. Only meaningful in edit mode.
		float _cursorX{ 960.0f };
		float _cursorY{ 540.0f };
	};
}
