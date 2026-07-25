#include "Overlay.h"

#include "Input.h"
#include "Settings.h"
#include "SurvivalData.h"
#include "Widgets.h"

#include <imgui_impl_dx11.h>

namespace StarfrostWidgets::Overlay
{
	namespace
	{
		using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);

		// IUnknown (3) + IDXGIObject (4) + IDXGIDeviceSubObject (1) puts
		// IDXGISwapChain::Present at slot 8.
		constexpr std::size_t kPresentVTableIndex = 8;

		PresentFn               gOriginalPresent{ nullptr };
		ID3D11Device*           gDevice{ nullptr };
		ID3D11DeviceContext*    gContext{ nullptr };
		ID3D11RenderTargetView* gRenderTarget{ nullptr };
		UINT                    gWidth{ 0 };
		UINT                    gHeight{ 0 };

		std::atomic<bool> gInstalled{ false };
		bool              gImGuiReady{ false };
		bool              gGaveUp{ false };

		std::chrono::steady_clock::time_point gLastFrame{};
		float                                 gPollAccumulator{ 0.0f };

		bool InitImGui(IDXGISwapChain* a_swapChain)
		{
			if (FAILED(a_swapChain->GetDevice(IID_PPV_ARGS(&gDevice))) || !gDevice) {
				SKSE::log::error("Could not get the D3D11 device from the swap chain");
				return false;
			}

			gDevice->GetImmediateContext(&gContext);
			if (!gContext) {
				SKSE::log::error("Could not get the immediate context");
				return false;
			}

			IMGUI_CHECKVERSION();
			ImGui::CreateContext();

			auto& io = ImGui::GetIO();
			io.IniFilename = nullptr;  // no imgui.ini next to the exe
			io.LogFilename = nullptr;
			io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
			io.BackendPlatformName = "StarfrostWidgets";
			io.MouseDrawCursor = false;

			ImGui::StyleColorsDark();
			auto& style = ImGui::GetStyle();
			style.WindowRounding = 6.0f;
			style.FrameRounding = 4.0f;

			if (!ImGui_ImplDX11_Init(gDevice, gContext)) {
				SKSE::log::error("ImGui_ImplDX11_Init failed");
				return false;
			}

			SKSE::log::info("ImGui initialised");
			return true;
		}

		bool EnsureRenderTarget(IDXGISwapChain* a_swapChain, UINT a_width, UINT a_height)
		{
			if (gRenderTarget && a_width == gWidth && a_height == gHeight) {
				return true;
			}

			if (gRenderTarget) {
				gRenderTarget->Release();
				gRenderTarget = nullptr;
			}

			ID3D11Texture2D* backBuffer{ nullptr };
			if (FAILED(a_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) || !backBuffer) {
				return false;
			}

			const auto hr = gDevice->CreateRenderTargetView(backBuffer, nullptr, &gRenderTarget);
			backBuffer->Release();
			if (FAILED(hr)) {
				gRenderTarget = nullptr;
				return false;
			}

			gWidth = a_width;
			gHeight = a_height;
			return true;
		}

		// Needs are read on the main thread through the task interface, because
		// HasSpell walks the player's spell lists and the render thread has no
		// business doing that concurrently.
		void PumpGameData(float a_deltaTime)
		{
			gPollAccumulator += a_deltaTime;

			const auto interval = Settings::GetSingleton()->pollInterval;
			if (gPollAccumulator < interval) {
				return;
			}
			gPollAccumulator = 0.0f;

			if (const auto tasks = SKSE::GetTaskInterface()) {
				tasks->AddTask([]() { SurvivalData::GetSingleton()->Refresh(); });
			}
		}

		void RenderFrame(IDXGISwapChain* a_swapChain)
		{
			DXGI_SWAP_CHAIN_DESC desc{};
			if (FAILED(a_swapChain->GetDesc(&desc))) {
				return;
			}

			const auto width = desc.BufferDesc.Width;
			const auto height = desc.BufferDesc.Height;
			if (width == 0 || height == 0) {
				return;
			}

			if (!gImGuiReady) {
				if (gGaveUp) {
					return;
				}
				if (!InitImGui(a_swapChain)) {
					gGaveUp = true;  // one failed attempt is enough; do not thrash every frame
					return;
				}
				gImGuiReady = true;
				gLastFrame = std::chrono::steady_clock::now();
			}

			if (!EnsureRenderTarget(a_swapChain, width, height)) {
				return;
			}

			const auto  now = std::chrono::steady_clock::now();
			const float deltaTime = std::clamp(
				std::chrono::duration<float>(now - gLastFrame).count(), 1.0f / 1000.0f, 1.0f);
			gLastFrame = now;

			PumpGameData(deltaTime);

			auto* input = Input::GetSingleton();
			input->SetDisplaySize(static_cast<float>(width), static_cast<float>(height));

			auto& io = ImGui::GetIO();
			io.DisplaySize = ImVec2{ static_cast<float>(width), static_cast<float>(height) };
			io.DeltaTime = deltaTime;
			io.MouseDrawCursor = input->EditMode();
			input->PumpInto(io);

			ImGui_ImplDX11_NewFrame();
			ImGui::NewFrame();

			Widgets::Draw();

			ImGui::Render();
			gContext->OMSetRenderTargets(1, &gRenderTarget, nullptr);
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		}

		HRESULT WINAPI HookedPresent(IDXGISwapChain* a_swapChain, UINT a_syncInterval, UINT a_flags)
		{
			// Never let a drawing problem take the game down with it.
			try {
				RenderFrame(a_swapChain);
			} catch (const std::exception& e) {
				SKSE::log::error("Overlay render threw: {}", e.what());
				gGaveUp = true;
			} catch (...) {
				SKSE::log::error("Overlay render threw an unknown exception");
				gGaveUp = true;
			}

			return gOriginalPresent(a_swapChain, a_syncInterval, a_flags);
		}
	}

	bool Installed()
	{
		return gInstalled.load(std::memory_order_relaxed);
	}

	bool Install()
	{
		if (Installed()) {
			return true;
		}

		const auto window = RE::BSGraphics::Renderer::GetCurrentRenderWindow();
		if (!window || !window->swapChain) {
			SKSE::log::warn("Swap chain not ready yet");
			return false;
		}

		auto* swapChain = reinterpret_cast<IDXGISwapChain*>(window->swapChain);
		auto* vtable = *reinterpret_cast<void***>(swapChain);

		DWORD oldProtect{};
		if (!VirtualProtect(&vtable[kPresentVTableIndex], sizeof(void*), PAGE_READWRITE, &oldProtect)) {
			SKSE::log::error("Could not unprotect the swap chain vtable");
			return false;
		}

		gOriginalPresent = reinterpret_cast<PresentFn>(vtable[kPresentVTableIndex]);
		vtable[kPresentVTableIndex] = reinterpret_cast<void*>(&HookedPresent);

		VirtualProtect(&vtable[kPresentVTableIndex], sizeof(void*), oldProtect, &oldProtect);

		gInstalled.store(true, std::memory_order_relaxed);
		SKSE::log::info("Present hook installed");
		return true;
	}
}
