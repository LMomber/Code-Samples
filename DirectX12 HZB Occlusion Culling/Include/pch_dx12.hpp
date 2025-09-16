#pragma once

#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

// The min/max macros conflict with like-named member functions.
// Only use std::min and std::max defined in <algorithm>.
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

// In order to define a function called CreateWindow, the Windows macro needs to
// be undefined.
#if defined(CreateWindow)
#undef CreateWindow
#endif

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
using namespace Microsoft::WRL;

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// D3D12 extension library.
#include <dx12Headers/include/directx/d3dx12.h>
#include "../external/DirectXTex/DirectXTex/DirectXTex.h"

// STL Headers
#include <algorithm>
#include <cassert>
#include <chrono>
#include <map>
#include <memory>
#include <filesystem>
#include <exception>

#include "tools/log.hpp"

#include "descriptor_allocation_dx12.hpp"
#include "descriptor_allocator_page_dx12.hpp"
#include "descriptor_allocator_dx12.hpp"

#include "adapter_dx12.hpp"
#include "swapchain_dx12.hpp"
#include "vertex_types_dx12.hpp"
#include "buffers_dx12.hpp"
#include "command_queue_dx12.hpp"
#include "views_dx12.hpp"
#include "core/device.hpp"
#include "dynamic_descriptor_heap.hpp"
#include "generate_mips_PSO_dx12.hpp"
#include "generate_hzb_mips_pso.hpp"
#include "PSO_dx12.hpp"
#include "render_target_dx12.hpp"
#include "resource_tracker_dx12.hpp" 
#include "root_signature_dx12.hpp"
#include "texture_dx12.hpp"
#include "upload_buffer_dx12.hpp"
#include "helpers_dx12.hpp"
#include "commandlist_dx12.hpp"


// From DXSampleHelper.h
// Source: https://github.com/Microsoft/DirectX-Graphics-Samples
inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::exception();
    }
}

#define APP_ICON 5

// Next default values for new objects
//
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE 102
#define _APS_NEXT_COMMAND_VALUE 40001
#define _APS_NEXT_CONTROL_VALUE 1001
#define _APS_NEXT_SYMED_VALUE 101
#endif
#endif