#pragma once

// Clean-room reconstruction from static analysis of Resources_DE_2024-01-25.
// Not original NextMU source.

namespace NextMUReference
{
    enum class RendererBackend : int
    {
        Unknown  = 0,
        D3D11    = 1,
        D3D12    = 2,
        OpenGL   = 3,
        Vulkan   = 5
    };

    struct BackendAttempt
    {
        RendererBackend backend = RendererBackend::Unknown;
        bool moduleLoaded = false;
        bool factoryResolved = false;
        bool initialized = false;
    };
}
