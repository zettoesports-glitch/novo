#pragma once

#include "ModernGraphicsBootstrap.h"

#ifdef MU_ENABLE_DILIGENT

#include "../dependencies/DiligentCore/Common/interface/ObjectBase.hpp"
#include "../dependencies/DiligentCore/Graphics/GraphicsEngineOpenGL/interface/DeviceContextGL.h"
#include "../dependencies/DiligentCore/Graphics/GraphicsEngineOpenGL/interface/RenderDeviceGL.h"
#include "../dependencies/DiligentCore/Graphics/GraphicsEngineOpenGL/interface/SwapChainGL.h"

#include <cstdint>

// Non-presenting swap-chain facade used only to tell Diligent's OpenGL context
// which native framebuffer is the application's default target. Main remains
// the sole owner of SwapBuffers/presentation; Present() here is intentionally a
// no-op and must never be used as an application presentation path.
class CModernLegacyGLSwapChainProxy final : public Diligent::ObjectBase<Diligent::ISwapChainGL>
{
public:
    using TBase = Diligent::ObjectBase<Diligent::ISwapChainGL>;

    CModernLegacyGLSwapChainProxy(Diligent::IReferenceCounters* refCounters,
                                  std::uint32_t width,
                                  std::uint32_t height,
                                  Diligent::TEXTURE_FORMAT colorFormat,
                                  Diligent::TEXTURE_FORMAT depthFormat)
        : TBase{refCounters}
    {
        m_desc.Width = width;
        m_desc.Height = height;
        m_desc.ColorBufferFormat = colorFormat;
        m_desc.DepthBufferFormat = depthFormat;
        m_desc.PreTransform = Diligent::SURFACE_TRANSFORM_IDENTITY;
    }

    IMPLEMENT_QUERY_INTERFACE2_IN_PLACE(Diligent::IID_SwapChain,
                                        Diligent::IID_SwapChainGL,
                                        TBase)

    void DILIGENT_CALL_TYPE Present(Diligent::Uint32) override
    {
        // Main owns SwapBuffers. Deliberately no-op.
    }

    const Diligent::SwapChainDesc& DILIGENT_CALL_TYPE GetDesc() const override
    {
        return m_desc;
    }

    void DILIGENT_CALL_TYPE Resize(Diligent::Uint32 width,
                                   Diligent::Uint32 height,
                                   Diligent::SURFACE_TRANSFORM) override
    {
        if (width != 0)
            m_desc.Width = width;
        if (height != 0)
            m_desc.Height = height;
    }

    void DILIGENT_CALL_TYPE SetFullscreenMode(const Diligent::DisplayModeAttribs&) override {}
    void DILIGENT_CALL_TYPE SetWindowedMode() override {}
    void DILIGENT_CALL_TYPE SetMaximumFrameLatency(Diligent::Uint32) override {}

    Diligent::ITextureView* DILIGENT_CALL_TYPE GetCurrentBackBufferRTV() override
    {
        // OpenGL swap chains expose their default framebuffer through dummy
        // textures rather than returning a conventional back-buffer RTV here.
        return nullptr;
    }

    Diligent::ITextureView* DILIGENT_CALL_TYPE GetDepthBufferDSV() override
    {
        return nullptr;
    }

    Diligent::GLuint DILIGENT_CALL_TYPE GetDefaultFBO() const override
    {
        return 0;
    }

private:
    Diligent::SwapChainDesc m_desc;
};

// Public-interface-only bridge for rendering Diligent commands into the WGL
// default framebuffer that Main already owns. Diligent's OpenGL backend expects
// dummy color/depth textures plus ISwapChainGL::GetDefaultFBO() to identify this
// target when no Diligent-created swap chain exists.
class CModernOpenGLDefaultFramebuffer
{
public:
    bool Initialize(Diligent::IRenderDevice* device,
                    Diligent::IDeviceContext* context,
                    std::uint32_t width,
                    std::uint32_t height)
    {
        Reset();
        if (device == nullptr || context == nullptr || width == 0 || height == 0)
            return false;

        m_deviceGL = Diligent::RefCntAutoPtr<Diligent::IRenderDeviceGL>{device, Diligent::IID_RenderDeviceGL};
        m_contextGL = Diligent::RefCntAutoPtr<Diligent::IDeviceContextGL>{context, Diligent::IID_DeviceContextGL};
        if (!m_deviceGL || !m_contextGL)
        {
            Reset();
            return false;
        }

        // These are proxy formats used by Diligent to validate the graphics
        // pipeline. The legacy WGL pixel format requests a 16-bit color/depth
        // framebuffer; actual native compatibility is still a runtime gate.
        // RGBA8 is the portable OpenGL render-target proxy used by Diligent;
        // depth mirrors Main's requested 16-bit depth buffer.
        m_colorFormat = Diligent::TEX_FORMAT_RGBA8_UNORM;
        m_depthFormat = Diligent::TEX_FORMAT_D16_UNORM;

        m_proxy.Attach(Diligent::MakeNewRCObj<CModernLegacyGLSwapChainProxy>()(
            width, height, m_colorFormat, m_depthFormat));
        if (!m_proxy)
        {
            Reset();
            return false;
        }

        m_contextGL->SetSwapChain(m_proxy);
        if (!CreateDummyTargets(width, height))
        {
            Reset();
            return false;
        }

        m_width = width;
        m_height = height;
        return true;
    }

    bool EnsureSize(std::uint32_t width, std::uint32_t height)
    {
        if (!IsValid() || width == 0 || height == 0)
            return false;
        if (width == m_width && height == m_height)
            return true;

        m_proxy->Resize(width, height, Diligent::SURFACE_TRANSFORM_IDENTITY);
        if (!CreateDummyTargets(width, height))
            return false;

        m_width = width;
        m_height = height;
        return true;
    }

    bool UpdateCurrentContext()
    {
        return m_contextGL && m_contextGL->UpdateCurrentGLContext();
    }

    void Reset()
    {
        if (m_contextGL)
            m_contextGL->SetSwapChain(nullptr);

        m_depthTexture.Release();
        m_colorTexture.Release();
        m_proxy.Release();
        m_contextGL.Release();
        m_deviceGL.Release();
        m_width = 0;
        m_height = 0;
        m_colorFormat = Diligent::TEX_FORMAT_UNKNOWN;
        m_depthFormat = Diligent::TEX_FORMAT_UNKNOWN;
    }

    bool IsValid() const
    {
        return m_deviceGL && m_contextGL && m_proxy && m_colorTexture && m_depthTexture;
    }

    Diligent::ITextureView* GetRenderTargetView() const
    {
        return m_colorTexture ? m_colorTexture->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET) : nullptr;
    }

    Diligent::ITextureView* GetDepthStencilView() const
    {
        return m_depthTexture ? m_depthTexture->GetDefaultView(Diligent::TEXTURE_VIEW_DEPTH_STENCIL) : nullptr;
    }

    Diligent::TEXTURE_FORMAT GetColorFormat() const { return m_colorFormat; }
    Diligent::TEXTURE_FORMAT GetDepthFormat() const { return m_depthFormat; }
    std::uint32_t GetWidth() const { return m_width; }
    std::uint32_t GetHeight() const { return m_height; }

private:
    bool CreateDummyTargets(std::uint32_t width, std::uint32_t height)
    {
        Diligent::RefCntAutoPtr<Diligent::ITexture> color;
        Diligent::TextureDesc colorDesc;
        colorDesc.Name = "Main legacy default framebuffer color proxy";
        colorDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        colorDesc.Width = width;
        colorDesc.Height = height;
        colorDesc.MipLevels = 1;
        colorDesc.ArraySize = 1;
        colorDesc.Format = m_colorFormat;
        colorDesc.SampleCount = 1;
        colorDesc.BindFlags = Diligent::BIND_RENDER_TARGET;
        m_deviceGL->CreateDummyTexture(colorDesc, Diligent::RESOURCE_STATE_RENDER_TARGET, &color);
        if (!color || color->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET) == nullptr)
            return false;

        Diligent::RefCntAutoPtr<Diligent::ITexture> depth;
        Diligent::TextureDesc depthDesc = colorDesc;
        depthDesc.Name = "Main legacy default framebuffer depth proxy";
        depthDesc.Format = m_depthFormat;
        depthDesc.BindFlags = Diligent::BIND_DEPTH_STENCIL;
        m_deviceGL->CreateDummyTexture(depthDesc, Diligent::RESOURCE_STATE_DEPTH_WRITE, &depth);
        if (!depth || depth->GetDefaultView(Diligent::TEXTURE_VIEW_DEPTH_STENCIL) == nullptr)
            return false;

        m_colorTexture = color;
        m_depthTexture = depth;
        return true;
    }

    Diligent::RefCntAutoPtr<Diligent::IRenderDeviceGL> m_deviceGL;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContextGL> m_contextGL;
    Diligent::RefCntAutoPtr<CModernLegacyGLSwapChainProxy> m_proxy;
    Diligent::RefCntAutoPtr<Diligent::ITexture> m_colorTexture;
    Diligent::RefCntAutoPtr<Diligent::ITexture> m_depthTexture;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
    Diligent::TEXTURE_FORMAT m_colorFormat = Diligent::TEX_FORMAT_UNKNOWN;
    Diligent::TEXTURE_FORMAT m_depthFormat = Diligent::TEX_FORMAT_UNKNOWN;
};

#endif // MU_ENABLE_DILIGENT
