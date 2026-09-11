#pragma once

#include "ModernGraphicsBootstrap.h"
#include "GlobalBitmap.h"

#ifdef MU_ENABLE_DILIGENT

#include "../dependencies/DiligentCore/Graphics/GraphicsEngineOpenGL/interface/RenderDeviceGL.h"

#include <cstdint>
#include <map>

// Non-owning interop wrapper for textures created by Main's legacy OpenGL
// texture manager. Diligent owns only its wrapper object; Main remains the owner
// of BITMAP_t::TextureNumber and may destroy/reload the native GL texture.
class CModernLegacyTextureBridge
{
public:
    Diligent::ITextureView* GetOrCreate(Diligent::IRenderDevice* device,
                                        GLuint bitmapIndex,
                                        BITMAP_t* bitmap)
    {
        if (device == nullptr || bitmap == nullptr || bitmap->TextureNumber == 0)
            return nullptr;

        Diligent::RefCntAutoPtr<Diligent::IRenderDeviceGL> deviceGL{
            device, Diligent::IID_RenderDeviceGL};
        if (!deviceGL)
            return nullptr;

        const LegacyTextureIdentity identity{
            bitmap->TextureNumber,
            bitmap->output_width,
            bitmap->output_height,
            bitmap->Components};

        auto existing = m_textures.find(bitmapIndex);
        if (existing != m_textures.end())
        {
            if (existing->second.Identity == identity)
                return existing->second.View.RawPtr();

            // The bitmap index was reloaded or its native GL object changed.
            // Release only the Diligent wrapper; ownership of the native handle
            // always remains with CGlobalBitmap.
            m_textures.erase(existing);
        }

        Diligent::TextureDesc desc;
        desc.Name = "Legacy Main GL texture wrapper";
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;

        TextureRecord record;
        record.Identity = identity;
        deviceGL->CreateTextureFromGLHandle(
            static_cast<Diligent::Uint32>(bitmap->TextureNumber),
            static_cast<Diligent::Uint32>(GL_TEXTURE_2D),
            desc,
            Diligent::RESOURCE_STATE_SHADER_RESOURCE,
            &record.Texture);
        if (!record.Texture)
            return nullptr;

        record.View = record.Texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
        if (!record.View)
            return nullptr;

        auto inserted = m_textures.emplace(bitmapIndex, record);
        return inserted.first->second.View.RawPtr();
    }

    Diligent::ITextureView* GetOrCreate(Diligent::IRenderDevice* device,
                                        GLuint bitmapIndex)
    {
        return GetOrCreate(device, bitmapIndex, Bitmaps.FindTexture(bitmapIndex));
    }

    void Remove(GLuint bitmapIndex)
    {
        m_textures.erase(bitmapIndex);
    }

    void Clear()
    {
        m_textures.clear();
    }

private:
    struct LegacyTextureIdentity
    {
        GLuint Handle = 0;
        unsigned int Width = 0;
        unsigned int Height = 0;
        BYTE Components = 0;

        bool operator==(const LegacyTextureIdentity& other) const
        {
            return Handle == other.Handle &&
                   Width == other.Width &&
                   Height == other.Height &&
                   Components == other.Components;
        }
    };

    struct TextureRecord
    {
        LegacyTextureIdentity Identity;
        Diligent::RefCntAutoPtr<Diligent::ITexture> Texture;
        Diligent::RefCntAutoPtr<Diligent::ITextureView> View;
    };

    std::map<GLuint, TextureRecord> m_textures;
};

#endif // MU_ENABLE_DILIGENT
