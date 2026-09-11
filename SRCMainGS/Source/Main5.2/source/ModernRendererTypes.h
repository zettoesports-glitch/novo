#pragma once

#include <cstddef>
#include <cstdint>

// Backend-neutral transport contracts for Phase 3.
// These structures deliberately contain no OpenGL/Diligent handles.

enum class ModernRendererViewId : std::uint8_t
{
    Main = 0,
    ItemPreview,
    Auxiliary,
};

enum class ModernRendererPassId : std::uint8_t
{
    Opaque = 0,
    Transparent,
    ProjectedShadow,
    ShadowMap,
    Overlay,
};

// Phase 1 BMD asset layout. Keep this layout stable until the first migrated
// BMD path has been visually compared against the legacy renderer.
struct ModernBMDVertex
{
    float Position[3];
    float Normal[3];
    float UV[2];
    std::uint16_t PositionBone;
    std::uint16_t NormalBone;
    std::uint32_t OriginalVertexId;
};

static_assert(offsetof(ModernBMDVertex, Position) == 0, "ModernBMDVertex.Position contract changed");
static_assert(offsetof(ModernBMDVertex, Normal) == 12, "ModernBMDVertex.Normal contract changed");
static_assert(offsetof(ModernBMDVertex, UV) == 24, "ModernBMDVertex.UV contract changed");
static_assert(offsetof(ModernBMDVertex, PositionBone) == 32, "ModernBMDVertex.PositionBone contract changed");
static_assert(offsetof(ModernBMDVertex, NormalBone) == 34, "ModernBMDVertex.NormalBone contract changed");
static_assert(offsetof(ModernBMDVertex, OriginalVertexId) == 36, "ModernBMDVertex.OriginalVertexId contract changed");
static_assert(sizeof(ModernBMDVertex) == 40, "ModernBMDVertex must remain 40 bytes");

struct alignas(16) ModernFrameConstants
{
    float View[16];
    float Projection[16];
    float CameraPositionWorldTime[4];
    float FogColor[4];
    float FogParams[4];
};

struct alignas(16) ModernInstanceConstants
{
    float Model[16];
    float BodyLightAlpha[4];
    float BodyOriginScale[4];
    float UVOffsetNormalScale[4];
    // x = first skeleton texel for this pose, y = skeleton texture width,
    // z = bone count, w = reserved. The shader uses two float4 texels per bone.
    std::uint32_t Skeleton[4];
};

struct alignas(16) ModernMaterialConstants
{
    float Color[4];
    float Params0[4];
    float Params1[4];
    std::uint32_t Flags[4];
};

static_assert((sizeof(ModernFrameConstants) % 16) == 0, "Frame constants must be 16-byte aligned");
static_assert((sizeof(ModernInstanceConstants) % 16) == 0, "Instance constants must be 16-byte aligned");
static_assert((sizeof(ModernMaterialConstants) % 16) == 0, "Material constants must be 16-byte aligned");
static_assert(sizeof(ModernInstanceConstants) == 128, "Instance constants must match the shared-HLSL layout");

struct ModernDrawIdentity
{
    std::uint32_t FrameId;
    std::uint32_t SubmissionOrder;
    std::uint32_t InstanceSlot;
    std::uint32_t InstanceGeneration;
    ModernRendererViewId ViewId;
    ModernRendererPassId PassId;
    std::uint16_t Reserved;
};
