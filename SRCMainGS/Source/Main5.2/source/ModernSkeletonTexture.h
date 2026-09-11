#pragma once

#include "ModernSkeletonPose.h"
#include "ModernRendererCore.h"

#ifdef MU_ENABLE_DILIGENT

#include <cstdint>

struct ModernSkeletonTextureAddress
{
    std::uint32_t LinearTexelOffset = 0;
    std::uint32_t TextureWidth = 0;
    std::uint32_t BoneCount = 0;
    std::uint32_t Slot = 0;
};

// Phase 3 pose atlas. One entity snapshot occupies one row. Main's MAX_BONES is
// 200, so 512 RGBA32F texels leave headroom above the required 400 texels and
// keep every pose row self-contained. No entity/object state is stored here:
// the caller owns slot identity/generation and uploads the snapshot for that slot.
class CModernSkeletonTexture
{
public:
    static constexpr std::uint32_t DefaultWidthTexels = 512;
    static constexpr std::uint32_t DefaultSlots = 256;

    bool Create(Diligent::IRenderDevice* device,
                std::uint32_t widthTexels = DefaultWidthTexels,
                std::uint32_t slots = DefaultSlots)
    {
        Reset();
        if (device == nullptr || widthTexels < 400 || slots == 0)
            return false;

        Diligent::TextureDesc desc;
        desc.Name = "Modern BMD skeleton pose atlas";
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = widthTexels;
        desc.Height = slots;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
        desc.SampleCount = 1;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        desc.Usage = Diligent::USAGE_DEFAULT;

        device->CreateTexture(desc, nullptr, &m_texture);
        if (!m_texture)
            return false;

        m_view = m_texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
        if (!m_view)
        {
            Reset();
            return false;
        }

        m_width = widthTexels;
        m_slots = slots;
        return true;
    }

    bool UploadPose(Diligent::IDeviceContext* context,
                    std::uint32_t slot,
                    const ModernSkeletonPose& pose,
                    ModernSkeletonTextureAddress& address)
    {
        if (context == nullptr || !m_texture || !m_view || slot >= m_slots || pose.Bones.empty())
            return false;
        if (pose.Bones.size() > 200)
            return false;

        const std::uint32_t texelCount = static_cast<std::uint32_t>(pose.Bones.size()) * 2u;
        if (texelCount > m_width)
            return false;

        Diligent::Box box;
        box.MinX = 0;
        box.MaxX = texelCount;
        box.MinY = slot;
        box.MaxY = slot + 1u;
        box.MinZ = 0;
        box.MaxZ = 1;

        Diligent::TextureSubResData data;
        data.pData = pose.Bones.data();
        data.Stride = static_cast<Diligent::Uint64>(texelCount) * sizeof(float) * 4u;
        data.DepthStride = data.Stride;

        context->UpdateTexture(m_texture,
                               0,
                               0,
                               box,
                               data,
                               Diligent::RESOURCE_STATE_TRANSITION_MODE_NONE,
                               Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        ModernSkeletonTextureAddress result;
        result.LinearTexelOffset = slot * m_width;
        result.TextureWidth = m_width;
        result.BoneCount = static_cast<std::uint32_t>(pose.Bones.size());
        result.Slot = slot;
        address = result;
        return true;
    }

    void Reset()
    {
        m_view.Release();
        m_texture.Release();
        m_width = 0;
        m_slots = 0;
    }

    bool IsValid() const { return static_cast<bool>(m_texture) && static_cast<bool>(m_view); }
    std::uint32_t GetWidth() const { return m_width; }
    std::uint32_t GetSlotCount() const { return m_slots; }
    Diligent::ITextureView* GetView() const { return m_view; }

private:
    Diligent::RefCntAutoPtr<Diligent::ITexture> m_texture;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_view;
    std::uint32_t m_width = 0;
    std::uint32_t m_slots = 0;
};

#endif // MU_ENABLE_DILIGENT
