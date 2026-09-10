#pragma once

#include <Windows.h>

// Phase 2 dependency activation.
//
// The Main keeps the legacy build path working even when DiligentCore has not
// been prepared yet. Once setup_diligent_opengl46.ps1 places the pinned
// DiligentCore checkout under ../dependencies/DiligentCore, this translation
// unit automatically enables the Diligent coexistence bridge.
#ifndef MU_ENABLE_DILIGENT
#if defined(_MSC_VER) && defined(_WIN32) && defined(__has_include)
#if __has_include("../dependencies/DiligentCore/Graphics/GraphicsEngineOpenGL/interface/EngineFactoryOpenGL.h")
#define MU_ENABLE_DILIGENT 1
#endif
#endif
#endif

#ifdef MU_ENABLE_DILIGENT
// Diligent headers normally receive these definitions from its CMake targets.
// Main consumes only the public interfaces and loads the backend DLL explicitly,
// so define the minimum public-build contract locally for this translation unit.
#ifndef PLATFORM_WIN32
#define PLATFORM_WIN32 1
#endif
#ifndef ENGINE_DLL
#define ENGINE_DLL 1
#endif

// Main's Debug configuration defines DEBUG rather than _DEBUG. Mirror the
// public Diligent debug configuration so its official DLL loader selects the
// matching GraphicsEngineOpenGL_32d.dll and public debug contracts stay aligned
// with the backend built by CMake. Release remains on the _32r backend.
#if defined(DEBUG) && !defined(NDEBUG)
#ifndef DILIGENT_DEVELOPMENT
#define DILIGENT_DEVELOPMENT 1
#endif
#ifndef DILIGENT_DEBUG
#define DILIGENT_DEBUG 1
#endif
#endif

#include "../dependencies/DiligentCore/Common/interface/RefCntAutoPtr.hpp"
#include "../dependencies/DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "../dependencies/DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h"
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

enum class ModernGraphicsOwnership : unsigned char
{
    None = 0,
    Attached,
    Owned,
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
    bool HasAttemptedInitializationFor(HWND hWnd, HGLRC hGLRC) const;
    bool IsAttachedToWindow(HWND hWnd) const;
    ModernGraphicsBackend GetBackend() const;
    ModernGraphicsOwnership GetOwnership() const;
    unsigned int GetWidth() const;
    unsigned int GetHeight() const;
    int GetOpenGLMajor() const;
    int GetOpenGLMinor() const;

#ifdef MU_ENABLE_DILIGENT
    Diligent::IRenderDevice* GetDevice() const;
    Diligent::IDeviceContext* GetImmediateContext() const;
#endif

private:
    void ResetState();
    void LogOpenGLDiagnostics() const;

    HWND m_hWnd;
    HDC m_hDC;
    HGLRC m_hGLRC;
    unsigned int m_width;
    unsigned int m_height;
    int m_glMajor;
    int m_glMinor;
    bool m_gl46Capable;
    bool m_active;
    bool m_initializationAttempted;
    ModernGraphicsBackend m_backend;
    ModernGraphicsOwnership m_ownership;

#ifdef MU_ENABLE_DILIGENT
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> m_device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> m_immediateContext;
#endif
};

CModernGraphicsBootstrap& GetModernGraphics();
