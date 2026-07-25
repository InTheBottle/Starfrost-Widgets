#pragma once

// CommonLibSSE-NG mirrors the Win32 types itself in REX::W32 and refuses to
// build if <Windows.h> was already pulled in, so it has to come first. The real
// Windows headers are still needed for the D3D11 swap chain hook; every handle
// that crosses between the two worlds goes through an explicit reinterpret_cast.
#include <RE/Skyrim.h>
#include <REX/REX.h>
#include <SKSE/SKSE.h>

#include <Windows.h>

#include <d3d11.h>
#include <dxgi.h>

#include <imgui.h>
