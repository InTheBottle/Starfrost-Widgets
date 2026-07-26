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

		// Logged once and then whenever it flips, so a user's log says which path
		// actually ran without them having to describe what they saw. Compared by
		// pointer, hence the named constants.
		constexpr const char* kGameTargetName = "game";
		constexpr const char* kSwapTargetName = "swapchain";
		constexpr const char* kNoTargetName = "none";

		const char* gReportedTarget{ nullptr };

		bool InitImGui(IDXGISwapChain* a_swapChain)
		{
			// Prefer the renderer's own device and context. Asking the swap chain
			// is the fallback, and it is the less reliable one: with frame
			// generation the swap chain is a proxy whose GetDevice may hand back a
			// D3D12 device. Both live as long as the process, so neither is retained.
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

		// The game caches an RTV for the surface it presents. Under Community
		// Shaders' frame generation that entry is re-pointed at the UI buffer the
		// compositor reads, so honouring it is what keeps the widgets on screen -
		// and it is simply the back buffer's own RTV when nothing has redirected it.
		ID3D11RenderTargetView* GameFrameBufferView()
		{
			const auto renderer = RE::BSGraphics::Renderer::GetSingleton();
			if (!renderer) {
				return nullptr;
			}

			// kFRAMEBUFFER rather than the render window's own swapChainRenderTarget
			// index: they agree for the main window, and kFRAMEBUFFER is the entry
			// Community Shaders redirects, so it is the one worth following.
			return renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGET::kFRAMEBUFFER].RTV;
		}

		// ImGui needs the size of whatever we are drawing into, which is not always
		// the size the swap chain reports. Cached on the view pointer, and thrown
		// away when the swap chain resizes - the only way the texture behind a
		// still-live view realistically changes shape.
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

						// Nothing to keep the back buffer alive for on this path.
						if (gRenderTarget) {
							gRenderTarget->Release();
							gRenderTarget = nullptr;
							gWidth = 0;
							gHeight = 0;
						}
						return target;
					}
				}

				if (mode == RenderTarget::kGameFrameBuffer) {
					// Explicitly asked for, so do not quietly do something else.
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

			const auto target = SelectTarget(a_swapChain, width, height);
			if (!target.view) {
				return;
			}

			const auto  now = std::chrono::steady_clock::now();
			const float deltaTime = std::clamp(
				std::chrono::duration<float>(now - gLastFrame).count(), 1.0f / 1000.0f, 1.0f);
			gLastFrame = now;

			PumpGameData(deltaTime);

			// Everything downstream works in the target's pixels, not the swap
			// chain's - under frame generation those are different surfaces.
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

			// Lets the input thread know a text box has the keyboard, so our own
			// Esc and toggle-key handling gets out of the way while typing.
			input->SetWantTextInput(io.WantTextInput);

			// The game leaves its own viewport bound; ImGui's backend sets one from
			// DisplaySize and puts the old one back, so only the target needs setting.
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
