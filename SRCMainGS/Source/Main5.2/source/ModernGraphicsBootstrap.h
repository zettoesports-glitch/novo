#pragma once

#include <Windows.h>

#ifdef MU_ENABLE_DILIGENT
#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#endif

// Phase 2 graphics bootstrap.
//
// The legacy client keeps ownership of HWND/HDC/HGLRC and SwapBuffers.
// OpenGL 4.6 is the first modern backend. Vulkan and D3D11 are reserved
// in the backend enum so the game-side code does not need to change later.
enum class ModernGraphicsBackend : unsigned char
{
    Legacy = 0,
    OpenGL46,
    Vulkan,
    Direct3D11,
};

class CModernGraphicsBootstrap
{
public:
    CModernGraphicsBootstrap();
    ~CModernGraphicsBootstrap();

    // Attaches the modern renderer to the OpenGL context already created by
    // CreateOpenglWindow(). This function never creates or owns a second WGL
    // context and never takes ownership of presentation.
    bool InitializeOpenGL46(HWND hWnd, HDC hDC, HGLRC hGLRC, unsigned int width, unsigned int height);

    // Must run before KillGLWindow() destroys the legacy WGL context.
    void Shutdown();

    // Call immediately before issuing Diligent commands after legacy OpenGL
    // commands. Diligent caches GL state, so the cache must be invalidated at
    // every raw-GL -> Diligent boundary during the coexistence phase.
    void BeginModernPass();

    // The application owns the default framebuffer while attached to an
    // existing GL context. For now resize only records the new dimensions;
    // future modern off-screen targets will be recreated from this hook.
    void OnResize(unsigned int width, unsigned int height);

    bool IsActive() const;
    bool IsOpenGL46Capable() const;
    ModernGraphicsBackend GetBackend() const;
    unsigned int GetWidth() const;
    unsigned int GetHeight() const;

#ifdef MU_ENABLE_DILIGENT
    Diligent::IRenderDevice* GetDevice() const;
    Diligent::IDeviceContext* GetImmediateContext() const;
#endif

private:
    void ResetState();

    HWND m_hWnd;
    HDC m_hDC;
    HGLRC m_hGLRC;
    unsigned int m_width;
    unsigned int m_height;
    int m_glMajor;
    int m_glMinor;
    bool m_gl46Capable;
    bool m_active;
    ModernGraphicsBackend m_backend;

#ifdef MU_ENABLE_DILIGENT
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> m_device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> m_immediateContext;
#endif
};

CModernGraphicsBootstrap& GetModernGraphics();
