#pragma once

#include "ModernRendererCore.h"
#include "ModernSkeletonPose.h"

#ifdef MU_ENABLE_DILIGENT

#include <cstdint>

// Formats are explicit because Phase 2 attaches to Main's existing OpenGL
// context without a Diligent swap chain. The caller must describe the actual
// render target/depth target used by the pass; the renderer must not guess it.
struct ModernBMDPipelineDesc
{
    Diligent::TEXTURE_FORMAT ColorFormat = Diligent::TEX_FORMAT_UNKNOWN;
    Diligent::TEXTURE_FORMAT DepthFormat = Diligent::TEX_FORMAT_UNKNOWN;
    Diligent::Uint8 SampleCount = 1;
    Diligent::CULL_MODE CullMode = Diligent::CULL_MODE_BACK;
    bool DepthEnable = true;
    bool DepthWrite = true;
    bool AlphaBlend = false;
};

class CModernBMDPipeline
{
public:
    CModernBMDPipeline() = default;

    bool Initialize(CModernRendererCore& core, const ModernBMDPipelineDesc& desc)
    {
        Shutdown();
        if (!core.IsReady() || desc.ColorFormat == Diligent::TEX_FORMAT_UNKNOWN || desc.SampleCount == 0)
            return false;
        if (desc.DepthEnable && desc.DepthFormat == Diligent::TEX_FORMAT_UNKNOWN)
            return false;

        IModernRendererBackendAdapter* adapter = core.GetBackendAdapter();
        if (adapter == nullptr || !adapter->IsReady())
            return false;

        Diligent::IRenderDevice* device = adapter->GetDevice();
        if (device == nullptr)
            return false;

        // Stable keys local to this first model pipeline. Future material
        // permutations must use distinct keys instead of mutating this PSO.
        constexpr std::uint64_t VSKey = 0x424D445653000001ull;
        constexpr std::uint64_t PSKey = 0x424D445053000001ull;

        Diligent::IShader* vertexShader = core.GetShaderManager().GetOrCreateHLSL(
            device, VSKey, "Modern BMD textured VS", Diligent::SHADER_TYPE_VERTEX,
            GetVertexShaderSource());
        Diligent::IShader* pixelShader = core.GetShaderManager().GetOrCreateHLSL(
            device, PSKey, "Modern BMD textured PS", Diligent::SHADER_TYPE_PIXEL,
            GetPixelShaderSource());
        if (vertexShader == nullptr || pixelShader == nullptr)
            return false;

        if (!m_frameConstants.Create(device, "Modern BMD FrameConstants", sizeof(ModernFrameConstants)) ||
            !m_instanceConstants.Create(device, "Modern BMD InstanceConstants", sizeof(ModernInstanceConstants)) ||
            !m_materialConstants.Create(device, "Modern BMD MaterialConstants", sizeof(ModernMaterialConstants)))
        {
            Shutdown();
            return false;
        }

        Diligent::GraphicsPipelineStateCreateInfo createInfo;
        createInfo.PSODesc.Name = "Modern BMD textured pipeline";
        createInfo.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
        createInfo.pVS = vertexShader;
        createInfo.pPS = pixelShader;

        auto& graphics = createInfo.GraphicsPipeline;
        graphics.NumRenderTargets = 1;
        graphics.RTVFormats[0] = desc.ColorFormat;
        graphics.DSVFormat = desc.DepthFormat;
        graphics.SmplDesc.Count = desc.SampleCount;
        graphics.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        graphics.RasterizerDesc.CullMode = desc.CullMode;
        graphics.DepthStencilDesc.DepthEnable = desc.DepthEnable;
        graphics.DepthStencilDesc.DepthWriteEnable = desc.DepthWrite;
        graphics.DepthStencilDesc.DepthFunc = Diligent::COMPARISON_FUNC_LESS_EQUAL;

        if (desc.AlphaBlend)
        {
            auto& target = graphics.BlendDesc.RenderTargets[0];
            target.BlendEnable = true;
            target.SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
            target.DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
            target.BlendOp = Diligent::BLEND_OPERATION_ADD;
            target.SrcBlendAlpha = Diligent::BLEND_FACTOR_ONE;
            target.DestBlendAlpha = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
            target.BlendOpAlpha = Diligent::BLEND_OPERATION_ADD;
        }

        // ModernBMDVertex is deliberately ordered so automatic offsets produce
        // 0,12,24,32,34,36 and a 40-byte stride. Static asserts live in
        // ModernRendererTypes.h and protect this contract.
        Diligent::LayoutElement layout[] = {
            {0, 0, 3, Diligent::VT_FLOAT32, false},
            {1, 0, 3, Diligent::VT_FLOAT32, false},
            {2, 0, 2, Diligent::VT_FLOAT32, false},
            {3, 0, 1, Diligent::VT_UINT16, false},
            {4, 0, 1, Diligent::VT_UINT16, false},
            {5, 0, 1, Diligent::VT_UINT32, false},
        };
        graphics.InputLayout.LayoutElements = layout;
        graphics.InputLayout.NumElements = static_cast<Diligent::Uint32>(sizeof(layout) / sizeof(layout[0]));

        auto& resourceLayout = createInfo.PSODesc.ResourceLayout;
        resourceLayout.DefaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        Diligent::ShaderResourceVariableDesc variables[] = {
            {Diligent::SHADER_TYPE_VERTEX, "g_SkeletonTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {Diligent::SHADER_TYPE_PIXEL, "g_DiffuseTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        };
        resourceLayout.Variables = variables;
        resourceLayout.NumVariables = static_cast<Diligent::Uint32>(sizeof(variables) / sizeof(variables[0]));

        Diligent::SamplerDesc linearClamp{
            Diligent::FILTER_TYPE_LINEAR, Diligent::FILTER_TYPE_LINEAR, Diligent::FILTER_TYPE_LINEAR,
            Diligent::TEXTURE_ADDRESS_CLAMP, Diligent::TEXTURE_ADDRESS_CLAMP, Diligent::TEXTURE_ADDRESS_CLAMP};
        Diligent::ImmutableSamplerDesc samplers[] = {
            {Diligent::SHADER_TYPE_PIXEL, "g_DiffuseTexture", linearClamp},
        };
        resourceLayout.ImmutableSamplers = samplers;
        resourceLayout.NumImmutableSamplers = static_cast<Diligent::Uint32>(sizeof(samplers) / sizeof(samplers[0]));

        device->CreateGraphicsPipelineState(createInfo, &m_pipeline);
        if (!m_pipeline)
        {
            Shutdown();
            return false;
        }

        auto* frameVar = m_pipeline->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "FrameConstants");
        auto* instanceVar = m_pipeline->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "InstanceConstants");
        auto* materialVar = m_pipeline->GetStaticVariableByName(Diligent::SHADER_TYPE_PIXEL, "MaterialConstants");
        if (frameVar == nullptr || instanceVar == nullptr || materialVar == nullptr)
        {
            Shutdown();
            return false;
        }
        frameVar->Set(m_frameConstants.Get());
        instanceVar->Set(m_instanceConstants.Get());
        materialVar->Set(m_materialConstants.Get());

        m_pipeline->CreateShaderResourceBinding(&m_resources, true);
        if (!m_resources)
        {
            Shutdown();
            return false;
        }

        m_core = &core;
        return true;
    }

    void Shutdown()
    {
        m_resources.Release();
        m_pipeline.Release();
        m_materialConstants.Reset();
        m_instanceConstants.Reset();
        m_frameConstants.Reset();
        m_core = nullptr;
    }

    bool IsReady() const
    {
        return m_core != nullptr && m_pipeline && m_resources &&
               m_frameConstants.IsValid() && m_instanceConstants.IsValid() &&
               m_materialConstants.IsValid();
    }

    bool UpdateConstants(const ModernFrameConstants& frame,
                         const ModernInstanceConstants& instance,
                         const ModernMaterialConstants& material)
    {
        if (!IsReady())
            return false;
        IModernRendererBackendAdapter* adapter = m_core->GetBackendAdapter();
        Diligent::IDeviceContext* context = adapter != nullptr ? adapter->GetImmediateContext() : nullptr;
        if (context == nullptr)
            return false;
        return m_frameConstants.Update(context, &frame, sizeof(frame)) &&
               m_instanceConstants.Update(context, &instance, sizeof(instance)) &&
               m_materialConstants.Update(context, &material, sizeof(material));
    }

    bool BindTextures(Diligent::ITextureView* skeletonTexture,
                      Diligent::ITextureView* diffuseTexture)
    {
        if (!IsReady() || skeletonTexture == nullptr || diffuseTexture == nullptr)
            return false;
        auto* skeletonVar = m_resources->GetVariableByName(Diligent::SHADER_TYPE_VERTEX, "g_SkeletonTexture");
        auto* diffuseVar = m_resources->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_DiffuseTexture");
        if (skeletonVar == nullptr || diffuseVar == nullptr)
            return false;
        skeletonVar->Set(skeletonTexture);
        diffuseVar->Set(diffuseTexture);
        return true;
    }

    bool BuildSubmission(const ModernBMDMeshResource& mesh,
                         ModernIndexedDrawSubmission& submission) const
    {
        if (!IsReady() || !mesh.VertexBuffer || !mesh.IndexBuffer || mesh.NumIndices == 0)
            return false;
        ModernIndexedDrawSubmission candidate;
        candidate.Pipeline = m_pipeline;
        candidate.Resources = m_resources;
        candidate.VertexBuffer = mesh.VertexBuffer;
        candidate.IndexBuffer = mesh.IndexBuffer;
        candidate.NumIndices = mesh.NumIndices;
        candidate.IndexType = Diligent::VT_UINT32;
        submission = candidate;
        return true;
    }

private:
    static const char* GetVertexShaderSource()
    {
        return R"HLSL(
cbuffer FrameConstants
{
    float4x4 g_View;
    float4x4 g_Projection;
    float4 g_CameraPositionWorldTime;
    float4 g_FogColor;
    float4 g_FogParams;
};

cbuffer InstanceConstants
{
    float4x4 g_Model;
    float4 g_BodyLightAlpha;
    float4 g_BodyOriginScale;
    float4 g_UVOffsetNormalScale;
    uint4 g_Skeleton;
};

Texture2D<float4> g_SkeletonTexture;

struct VSInput
{
    float3 Position : ATTRIB0;
    float3 Normal : ATTRIB1;
    float2 UV : ATTRIB2;
    uint PositionBone : ATTRIB3;
    uint NormalBone : ATTRIB4;
    uint OriginalVertexId : ATTRIB5;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 UV : TEX_COORD;
    float3 Normal : NORMAL;
    float4 Color : COLOR0;
};

float3 RotateByQuaternion(float3 value, float4 quaternion)
{
    float3 q = quaternion.xyz;
    return value + 2.0 * cross(q, cross(q, value) + quaternion.w * value);
}

float4 LoadSkeletonTexel(uint linearAddress)
{
    uint width = max(g_Skeleton.y, 1u);
    uint address = g_Skeleton.x + linearAddress;
    return g_SkeletonTexture.Load(int3(address % width, address / width, 0));
}

VSOutput main(VSInput input)
{
    VSOutput output;
    float4 positionRotation = LoadSkeletonTexel(input.PositionBone * 2u);
    float4 positionScale = LoadSkeletonTexel(input.PositionBone * 2u + 1u);
    float4 normalRotation = LoadSkeletonTexel(input.NormalBone * 2u);

    float3 skinnedPosition = RotateByQuaternion(input.Position * positionScale.w, positionRotation) + positionScale.xyz;
    // Matches legacy Transform(): final bone transform first, then BodyScale and BodyOrigin.
    skinnedPosition = skinnedPosition * g_BodyOriginScale.w + g_BodyOriginScale.xyz;
    float3 skinnedNormal = normalize(RotateByQuaternion(input.Normal, normalRotation));

    float4 worldPosition = mul(float4(skinnedPosition, 1.0), g_Model);
    output.Position = mul(mul(worldPosition, g_View), g_Projection);
    output.Normal = normalize(mul(skinnedNormal, (float3x3)g_Model));
    output.UV = input.UV + g_UVOffsetNormalScale.xy;
    output.Color = g_BodyLightAlpha;
    return output;
}
)HLSL";
    }

    static const char* GetPixelShaderSource()
    {
        return R"HLSL(
cbuffer MaterialConstants
{
    float4 g_MaterialColor;
    float4 g_MaterialParams0;
    float4 g_MaterialParams1;
    uint4 g_MaterialFlags;
};

Texture2D g_DiffuseTexture;
SamplerState g_DiffuseTexture_sampler;

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 UV : TEX_COORD;
    float3 Normal : NORMAL;
    float4 Color : COLOR0;
};

float4 main(PSInput input) : SV_TARGET
{
    float4 color = g_DiffuseTexture.Sample(g_DiffuseTexture_sampler, input.UV) *
                   input.Color * g_MaterialColor;
    if ((g_MaterialFlags.x & 1u) != 0u)
        clip(color.a - g_MaterialParams0.x);
    return color;
}
)HLSL";
    }

    CModernRendererCore* m_core = nullptr;
    CModernConstantBuffer m_frameConstants;
    CModernConstantBuffer m_instanceConstants;
    CModernConstantBuffer m_materialConstants;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pipeline;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_resources;
};

#endif // MU_ENABLE_DILIGENT
