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

		// Compared by pointer, so the same object has to come back every time.
		constexpr const char* kGameTargetName = "game";
		constexpr const char* kSwapTargetName = "swapchain";
		constexpr const char* kNoTargetName = "none";

		const char* gReportedTarget{ nullptr };

		bool InitImGui(IDXGISwapChain* a_swapChain)
		{
			// The swap chain may be a frame generation proxy handing back a D3D12 device.
			if (const auto renderer = RE::BSGraphics::Renderer::GetSingleton()) {
				auto& data = renderer->GetRuntimeData();
				gDevice = reinterpret_cast<ID3D11Device*>(data.forwarder);
				gContext = reinterpret_cast<ID3D11DeviceContext*>(data.context);
			}

			if (!gDevice) {
				if (FAILED(a_swapChain->GetDevice(IID_PPV_ARGS(&gDevice))) || !gDevice) {
					SKSE::log::error("Could not get a D3D11 device");
					return false;
				}
			}

			if (!gContext) {
				gDevice->GetImmediateContext(&gContext);
				if (!gContext) {
					SKSE::log::error("Could not get the immediate context");
					return false;
				}
			}

			IMGUI_CHECKVERSION();
			ImGui::CreateContext();

			auto& io = ImGui::GetIO();
			io.IniFilename = nullptr;
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

		// Frame generation re-points kFRAMEBUFFER at the UI buffer its compositor reads.
		ID3D11RenderTargetView* GameFrameBufferView()
		{
			const auto renderer = RE::BSGraphics::Renderer::GetSingleton();
			if (!renderer) {
				return nullptr;
			}

			return renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGET::kFRAMEBUFFER].RTV;
		}

		// The target is not always the size the swap chain reports.
		bool DescribeView(ID3D11RenderTargetView* a_view, UINT a_swapWidth, UINT a_swapHeight, UINT& a_width, UINT& a_height)
		{
			static ID3D11RenderTargetView* cachedView{ nullptr };
			static UINT                    cachedWidth{ 0 };
			static UINT                    cachedHeight{ 0 };
			static UINT                    cachedSwapWidth{ 0 };
			static UINT                    cachedSwapHeight{ 0 };

			if (a_view == cachedView && cachedWidth != 0 &&
				a_swapWidth == cachedSwapWidth && a_swapHeight == cachedSwapHeight) {
				a_width = cachedWidth;
				a_height = cachedHeight;
				return true;
			}

			ID3D11Resource* resource{ nullptr };
			a_view->GetResource(&resource);
			if (!resource) {
				return false;
			}

			ID3D11Texture2D* texture{ nullptr };
			const auto       hr = resource->QueryInterface(IID_PPV_ARGS(&texture));
			resource->Release();
			if (FAILED(hr) || !texture) {
				return false;
			}

			D3D11_TEXTURE2D_DESC desc{};
			texture->GetDesc(&desc);
			texture->Release();

			if (desc.Width == 0 || desc.Height == 0) {
				return false;
			}

			cachedView = a_view;
			cachedWidth = desc.Width;
			cachedHeight = desc.Height;
			cachedSwapWidth = a_swapWidth;
			cachedSwapHeight = a_swapHeight;

			a_width = desc.Width;
			a_height = desc.Height;
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

		struct Target
		{
			ID3D11RenderTargetView* view{ nullptr };
			UINT                    width{ 0 };
			UINT                    height{ 0 };
		};

		Target SelectTarget(IDXGISwapChain* a_swapChain, UINT a_swapWidth, UINT a_swapHeight)
		{
			const auto mode = Settings::GetSingleton()->renderTarget;

			if (mode != RenderTarget::kSwapChain) {
				if (auto* view = GameFrameBufferView()) {
					Target target{ .view = view };
					if (DescribeView(view, a_swapWidth, a_swapHeight, target.width, target.height)) {
						if (gReportedTarget != kGameTargetName) {
							gReportedTarget = kGameTargetName;
							SKSE::log::info("Drawing into the game framebuffer ({}x{})", target.width, target.height);
						}

						if (gRenderTarget) {
							gRenderTarget->Release();
							gRenderTarget = nullptr;
							gWidth = 0;
							gHeight = 0;
						}
						return target;
					}
				}

				// Explicitly asked for, so do not silently fall back.
				if (mode == RenderTarget::kGameFrameBuffer) {
					if (gReportedTarget != kNoTargetName) {
						gReportedTarget = kNoTargetName;
						SKSE::log::warn("iRenderTarget=2 but the game framebuffer could not be resolved; nothing will draw");
					}
					return {};
				}
			}

			if (!EnsureRenderTarget(a_swapChain, a_swapWidth, a_swapHeight)) {
				return {};
			}

			if (gReportedTarget != kSwapTargetName) {
				gReportedTarget = kSwapTargetName;
				SKSE::log::info("Drawing into the swap chain back buffer ({}x{})", a_swapWidth, a_swapHeight);
			}

			return { .view = gRenderTarget, .width = a_swapWidth, .height = a_swapHeight };
		}

		// HasSpell walks the player's spell lists, so it cannot run on the render thread.
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
					gGaveUp = true;  // do not thrash every frame
					return;
				}
				gImGuiReady = true;
				gLastFrame = std::chrono::steady_clock::now();
			}

			const auto target = SelectTarget(a_swapChain, width, height);
			if (!target.view) {
				return;
			}

			const auto  now = std::chrono::steady_clock::now();
			const float deltaTime = std::clamp(
				std::chrono::duration<float>(now - gLastFrame).count(), 1.0f / 1000.0f, 1.0f);
			gLastFrame = now;

			PumpGameData(deltaTime);

			// Under frame generation the target and the swap chain are different surfaces.
			const auto targetWidth = static_cast<float>(target.width);
			const auto targetHeight = static_cast<float>(target.height);

			auto* input = Input::GetSingleton();
			input->SetDisplaySize(targetWidth, targetHeight);

			auto& io = ImGui::GetIO();
			io.DisplaySize = ImVec2{ targetWidth, targetHeight };
			io.DeltaTime = deltaTime;
			io.MouseDrawCursor = input->EditMode();
			input->PumpInto(io);

			ImGui_ImplDX11_NewFrame();
			ImGui::NewFrame();

			Widgets::Draw();

			ImGui::Render();

			// Stops our hotkeys stealing Esc while a text box has the keyboard.
			input->SetWantTextInput(io.WantTextInput);

			// ImGui's backend sets and restores the viewport itself.
			ID3D11RenderTargetView* view = target.view;
			gContext->OMSetRenderTargets(1, &view, nullptr);
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
