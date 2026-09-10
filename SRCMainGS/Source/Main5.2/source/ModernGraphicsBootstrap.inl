#ifdef MU_ENABLE_DILIGENT
#include "Graphics/GraphicsEngineOpenGL/interface/EngineFactoryOpenGL.h"
#endif

namespace
{
const char* SafeGLString(GLenum name)
{
    const GLubyte* value = glGetString(name);
    return value != NULL ? reinterpret_cast<const char*>(value) : "unavailable";
}
}

CModernGraphicsBootstrap::CModernGraphicsBootstrap()
{
    ResetState();
}

CModernGraphicsBootstrap::~CModernGraphicsBootstrap()
{
    Shutdown();
}

void CModernGraphicsBootstrap::ResetState()
{
    m_hWnd = NULL;
    m_hDC = NULL;
    m_hGLRC = NULL;
    m_width = 0;
    m_height = 0;
    m_glMajor = 0;
    m_glMinor = 0;
    m_gl46Capable = false;
    m_active = false;
    m_initializationAttempted = false;
    m_backend = ModernGraphicsBackend::Legacy;
    m_ownership = ModernGraphicsOwnership::None;
}

void CModernGraphicsBootstrap::LogOpenGLDiagnostics() const
{
    const char* profile = "unknown";
    GLint profileMask = 0;

    if (m_glMajor > 3 || (m_glMajor == 3 && m_glMinor >= 2))
    {
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profileMask);
        if ((profileMask & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) != 0)
            profile = "compatibility";
        else if ((profileMask & GL_CONTEXT_CORE_PROFILE_BIT) != 0)
            profile = "core";
    }

    char message[768];
    wsprintfA(message,
        "[ModernGraphics] OpenGL vendor=%s | renderer=%s | version=%s | GLSL=%s | parsed=%d.%d | profile=%s.\n",
        SafeGLString(GL_VENDOR),
        SafeGLString(GL_RENDERER),
        SafeGLString(GL_VERSION),
        SafeGLString(GL_SHADING_LANGUAGE_VERSION),
        m_glMajor,
        m_glMinor,
        profile);
    OutputDebugStringA(message);
}

bool CModernGraphicsBootstrap::InitializeOpenGL46(HWND hWnd, HDC hDC, HGLRC hGLRC, unsigned int width, unsigned int height)
{
    Shutdown();

    m_initializationAttempted = true;
    m_hWnd = hWnd;
    m_hDC = hDC;
    m_hGLRC = hGLRC;
    m_width = width;
    m_height = height;

    if (m_hWnd == NULL || m_hDC == NULL || m_hGLRC == NULL)
    {
        OutputDebugStringA("[ModernGraphics] OpenGL 4.6 attach skipped: invalid Win32/WGL handles.\n");
        return false;
    }

    // The modern renderer is deliberately attached only to the context that
    // CreateOpenglWindow() has already made current on this thread.
    if (wglGetCurrentContext() != m_hGLRC || wglGetCurrentDC() != m_hDC)
    {
        OutputDebugStringA("[ModernGraphics] OpenGL 4.6 attach skipped: legacy WGL context is not current.\n");
        return false;
    }

    glGetIntegerv(GL_MAJOR_VERSION, &m_glMajor);
    glGetIntegerv(GL_MINOR_VERSION, &m_glMinor);

    // Legacy WGL contexts may not expose GL_MAJOR_VERSION/GL_MINOR_VERSION on
    // old drivers. Parsing GL_VERSION keeps the failure diagnostic useful.
    if (m_glMajor == 0)
    {
        const char* version = SafeGLString(GL_VERSION);
        int parsedMajor = 0;
        int parsedMinor = 0;
        if (sscanf(version, "%d.%d", &parsedMajor, &parsedMinor) == 2)
        {
            m_glMajor = parsedMajor;
            m_glMinor = parsedMinor;
        }
    }

    LogOpenGLDiagnostics();

    m_gl46Capable = (m_glMajor > 4) || (m_glMajor == 4 && m_glMinor >= 6);
    if (!m_gl46Capable)
    {
        char message[192];
        wsprintfA(message,
            "[ModernGraphics] OpenGL 4.6 unavailable (reported %d.%d). Legacy renderer remains active.\n",
            m_glMajor,
            m_glMinor);
        OutputDebugStringA(message);
        return false;
    }

#ifndef MU_ENABLE_DILIGENT
    OutputDebugStringA("[ModernGraphics] OpenGL 4.6 is available, but MU_ENABLE_DILIGENT is not enabled. Legacy renderer remains active.\n");
    return false;
#else
    Diligent::EngineGLCreateInfo engineCreateInfo;
    Diligent::IEngineFactoryOpenGL* factory = Diligent::GetEngineFactoryOpenGL();

    if (factory == nullptr)
    {
        OutputDebugStringA("[ModernGraphics] Diligent OpenGL factory is unavailable. Legacy renderer remains active.\n");
        return false;
    }

    // AttachToActiveGLContext is intentional. The MU client continues to own
    // HDC/HGLRC and SwapBuffers, so Diligent must not create a second context
    // or swap chain during the coexistence phase.
    factory->AttachToActiveGLContext(
        engineCreateInfo,
        &m_device,
        &m_immediateContext);

    if (!m_device || !m_immediateContext)
    {
        m_immediateContext.Release();
        m_device.Release();
        OutputDebugStringA("[ModernGraphics] Diligent failed to attach to the active OpenGL context. Legacy renderer remains active.\n");
        return false;
    }

    m_backend = ModernGraphicsBackend::OpenGL46;
    m_ownership = ModernGraphicsOwnership::Attached;
    m_active = true;
    OutputDebugStringA("[ModernGraphics] Diligent attached to the existing OpenGL 4.6 context; legacy SwapBuffers remains authoritative.\n");
    return true;
#endif
}

void CModernGraphicsBootstrap::Shutdown()
{
#ifdef MU_ENABLE_DILIGENT
    if (m_immediateContext)
    {
        m_immediateContext->Flush();
        m_immediateContext.Release();
    }
    m_device.Release();
#endif

    ResetState();
}

void CModernGraphicsBootstrap::BeginModernPass()
{
#ifdef MU_ENABLE_DILIGENT
    if (m_active && m_immediateContext)
    {
        // Legacy rendering issues raw OpenGL commands between modern draws.
        // Diligent's cached GL state must therefore be discarded before each
        // transition back into the modern command path.
        m_immediateContext->InvalidateState();
    }
#endif
}

void CModernGraphicsBootstrap::OnResize(unsigned int width, unsigned int height)
{
    m_width = width;
    m_height = height;
}

bool CModernGraphicsBootstrap::IsActive() const
{
    return m_active;
}

bool CModernGraphicsBootstrap::IsOpenGL46Capable() const
{
    return m_gl46Capable;
}

bool CModernGraphicsBootstrap::HasAttemptedInitializationFor(HWND hWnd, HGLRC hGLRC) const
{
    return m_initializationAttempted && m_hWnd == hWnd && m_hGLRC == hGLRC;
}

bool CModernGraphicsBootstrap::IsAttachedToWindow(HWND hWnd) const
{
    return m_initializationAttempted && m_hWnd == hWnd;
}

ModernGraphicsBackend CModernGraphicsBootstrap::GetBackend() const
{
    return m_backend;
}

ModernGraphicsOwnership CModernGraphicsBootstrap::GetOwnership() const
{
    return m_ownership;
}

unsigned int CModernGraphicsBootstrap::GetWidth() const
{
    return m_width;
}

unsigned int CModernGraphicsBootstrap::GetHeight() const
{
    return m_height;
}

int CModernGraphicsBootstrap::GetOpenGLMajor() const
{
    return m_glMajor;
}

int CModernGraphicsBootstrap::GetOpenGLMinor() const
{
    return m_glMinor;
}

#ifdef MU_ENABLE_DILIGENT
Diligent::IRenderDevice* CModernGraphicsBootstrap::GetDevice() const
{
    return m_device;
}

Diligent::IDeviceContext* CModernGraphicsBootstrap::GetImmediateContext() const
{
    return m_immediateContext;
}
#endif

CModernGraphicsBootstrap& GetModernGraphics()
{
    static CModernGraphicsBootstrap instance;
    return instance;
}