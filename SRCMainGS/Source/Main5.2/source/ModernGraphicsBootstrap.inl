#ifdef MU_ENABLE_DILIGENT
#include "../dependencies/DiligentCore/Graphics/GraphicsEngineOpenGL/interface/EngineFactoryOpenGL.h"
#endif

namespace
{
void ModernGraphicsLog(const char* message)
{
    if (message == NULL)
        return;

    OutputDebugStringA(message);

    HANDLE file = CreateFileA(
        "ModernGraphics.log",
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (file != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        WriteFile(file, message, static_cast<DWORD>(lstrlenA(message)), &written, NULL);
        CloseHandle(file);
    }
}

bool IsModernGLDebugRequested()
{
    char value[16] = { 0 };
    const DWORD length = GetEnvironmentVariableA("MU_MODERN_GL_DEBUG", value, sizeof(value));
    if (length == 0 || length >= sizeof(value))
        return false;

    return value[0] == '1' || value[0] == 'y' || value[0] == 'Y' || value[0] == 't' || value[0] == 'T';
}

void APIENTRY ModernOpenGLDebugCallback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam)
{
    UNREFERENCED_PARAMETER(source);
    UNREFERENCED_PARAMETER(type);
    UNREFERENCED_PARAMETER(severity);
    UNREFERENCED_PARAMETER(length);
    UNREFERENCED_PARAMETER(userParam);

    char output[1200];
    wsprintfA(output,
        "[ModernGraphics][GLDebug] id=%u message=%s\n",
        static_cast<unsigned int>(id),
        message != NULL ? message : "unavailable");
    ModernGraphicsLog(output);
}

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
    m_debugCallbackEnabled = false;
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
    ModernGraphicsLog(message);
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

    ModernGraphicsLog("[ModernGraphics] Phase 2 OpenGL 4.6 attach attempt started.\n");

    if (m_hWnd == NULL || m_hDC == NULL || m_hGLRC == NULL)
    {
        ModernGraphicsLog("[ModernGraphics] OpenGL 4.6 attach skipped: invalid Win32/WGL handles.\n");
        return false;
    }

    // The modern renderer is deliberately attached only to the context that
    // CreateOpenglWindow() has already made current on this thread.
    if (wglGetCurrentContext() != m_hGLRC || wglGetCurrentDC() != m_hDC)
    {
        ModernGraphicsLog("[ModernGraphics] OpenGL 4.6 attach skipped: legacy WGL context is not current.\n");
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
        ModernGraphicsLog(message);
        return false;
    }

    if (IsModernGLDebugRequested())
    {
        if (GLEW_VERSION_4_3 || GLEW_KHR_debug)
        {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(ModernOpenGLDebugCallback, NULL);
            m_debugCallbackEnabled = true;
            ModernGraphicsLog("[ModernGraphics] OpenGL debug callback enabled by MU_MODERN_GL_DEBUG.\n");
        }
        else
        {
            ModernGraphicsLog("[ModernGraphics] MU_MODERN_GL_DEBUG requested, but KHR_debug entry points are unavailable.\n");
        }
    }

#ifndef MU_ENABLE_DILIGENT
    ModernGraphicsLog("[ModernGraphics] OpenGL 4.6 is available, but the pinned DiligentCore headers are not prepared. Legacy renderer remains active.\n");
    return false;
#else
    Diligent::EngineGLCreateInfo engineCreateInfo;

    // ENGINE_DLL=1 makes Diligent's public OpenGL header expose the explicit
    // Windows loader. This intentionally mirrors NextMU's backend-module model:
    // Release loads GraphicsEngineOpenGL_32r.dll and Debug loads _32d.dll.
    const auto loadFactory = Diligent::LoadGraphicsEngineOpenGL();
    if (loadFactory == nullptr)
    {
        ModernGraphicsLog("[ModernGraphics] Unable to load the Diligent OpenGL backend DLL. Legacy renderer remains active.\n");
        return false;
    }

    Diligent::IEngineFactoryOpenGL* factory = loadFactory();
    if (factory == nullptr)
    {
        ModernGraphicsLog("[ModernGraphics] Diligent OpenGL factory export returned null. Legacy renderer remains active.\n");
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
        ModernGraphicsLog("[ModernGraphics] Diligent failed to attach to the active OpenGL context. Legacy renderer remains active.\n");
        return false;
    }

    m_backend = ModernGraphicsBackend::OpenGL46;
    m_ownership = ModernGraphicsOwnership::Attached;
    m_active = true;
    ModernGraphicsLog("[ModernGraphics] Diligent attached to the existing OpenGL 4.6 context; legacy SwapBuffers remains authoritative.\n");
    return true;
#endif
}

void CModernGraphicsBootstrap::Shutdown()
{
    const bool hadRuntimeState = m_initializationAttempted || m_active;

#ifdef MU_ENABLE_DILIGENT
    if (m_immediateContext)
    {
        m_immediateContext->Flush();
        m_immediateContext.Release();
    }
    m_device.Release();
#endif

    if (m_debugCallbackEnabled && wglGetCurrentContext() == m_hGLRC)
    {
        glDebugMessageCallback(NULL, NULL);
        glDisable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDisable(GL_DEBUG_OUTPUT);
    }

    if (hadRuntimeState)
        ModernGraphicsLog("[ModernGraphics] Shutdown completed before legacy WGL teardown.\n");

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

    char message[192];
    wsprintfA(message,
        "[ModernGraphics] Resize observed: %ux%u. Presentation remains legacy-owned.\n",
        width,
        height);
    ModernGraphicsLog(message);
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