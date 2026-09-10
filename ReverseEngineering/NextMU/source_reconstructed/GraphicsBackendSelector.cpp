// Clean-room reconstruction from static analysis of Resources_DE_2024-01-25.
// Not original NextMU source.

#include "RendererBackend.h"
#include <array>

namespace NextMUReference
{
    // Sequence observed in both NextMU executables: 1,5,2,3.
    constexpr std::array<RendererBackend, 4> kObservedAutomaticOrder = {
        RendererBackend::D3D11,
        RendererBackend::Vulkan,
        RendererBackend::D3D12,
        RendererBackend::OpenGL
    };

    const char* ModuleBaseName(RendererBackend backend)
    {
        switch (backend)
        {
        case RendererBackend::D3D11:  return "GraphicsEngineD3D11";
        case RendererBackend::D3D12:  return "GraphicsEngineD3D12";
        case RendererBackend::OpenGL: return "GraphicsEngineOpenGL";
        case RendererBackend::Vulkan: return "GraphicsEngineVk";
        default:                      return nullptr;
        }
    }

    const char* FactoryName(RendererBackend backend)
    {
        switch (backend)
        {
        case RendererBackend::D3D11:  return "GetEngineFactoryD3D11";
        case RendererBackend::D3D12:  return "GetEngineFactoryD3D12";
        case RendererBackend::OpenGL: return "GetEngineFactoryOpenGL";
        case RendererBackend::Vulkan: return "GetEngineFactoryVk";
        default:                      return nullptr;
        }
    }
}
