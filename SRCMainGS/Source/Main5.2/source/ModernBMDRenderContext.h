#pragma once

#include "ModernRendererTypes.h"

#include <cstdint>

// Transient instance identity propagated from a high-level render producer down
// to the shared BMD draw boundary. This is deliberately not stored on BMD: one
// BMD asset can be rendered by many live entities in the same frame.
enum class ModernBMDInstanceKind : std::uint8_t
{
    Unknown = 0,
    LocalHero,
    RemotePlayer,
    NPC,
    Monster,
    WorldObject,
    ItemPreview,
    Attachment,
};

struct ModernBMDRenderContext
{
    ModernDrawIdentity Draw{};
    ModernBMDInstanceKind Kind = ModernBMDInstanceKind::Unknown;

    // Immutable asset identity. AssetGeneration must change whenever the source
    // BMD data behind AssetId is reloaded/replaced.
    std::uint64_t AssetId = 0;
    std::uint32_t AssetGeneration = 0;

    // Pose atlas slot is an instance resource, never an asset resource. The
    // producer owns allocation/reuse and must pair recycled slots with a new
    // Draw.InstanceGeneration.
    std::uint32_t SkeletonSlot = 0;

    // Progressive migration gate. Unsupported materials/passes or producers
    // simply leave this false and continue through the legacy renderer.
    bool AllowModernDraw = false;
};

namespace ModernBMDRenderContextDetail
{
// Main's legacy renderer is single-threaded today, but thread_local makes this
// boundary safe if preview/loading work later starts issuing draw preparation on
// another thread. The pointer always refers to a scope-owned value.
inline thread_local const ModernBMDRenderContext* Current = nullptr;
}

inline const ModernBMDRenderContext* GetModernBMDRenderContext()
{
    return ModernBMDRenderContextDetail::Current;
}

inline bool HasModernBMDRenderContext()
{
    return GetModernBMDRenderContext() != nullptr;
}

// RAII propagation is intentional. RenderPartObject/RenderLinkObject and item
// preview paths can nest; restoring the previous context prevents one entity or
// attachment from leaking identity/pose addressing into the next BMD draw.
class CModernBMDRenderContextScope
{
public:
    explicit CModernBMDRenderContextScope(const ModernBMDRenderContext& context)
        : m_context{context},
          m_previous{ModernBMDRenderContextDetail::Current}
    {
        ModernBMDRenderContextDetail::Current = &m_context;
    }

    ~CModernBMDRenderContextScope()
    {
        ModernBMDRenderContextDetail::Current = m_previous;
    }

    CModernBMDRenderContextScope(const CModernBMDRenderContextScope&) = delete;
    CModernBMDRenderContextScope& operator=(const CModernBMDRenderContextScope&) = delete;
    CModernBMDRenderContextScope(CModernBMDRenderContextScope&&) = delete;
    CModernBMDRenderContextScope& operator=(CModernBMDRenderContextScope&&) = delete;

private:
    ModernBMDRenderContext m_context;
    const ModernBMDRenderContext* m_previous = nullptr;
};
