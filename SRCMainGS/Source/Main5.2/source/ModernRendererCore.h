#pragma once

#include "ModernGraphicsBootstrap.h"
#include "ModernRendererTypes.h"
#include "ModernBMDGeometry.h"

#include <cstdint>
#include <map>

class IModernRendererBackendAdapter
{
public:
    virtual ~IModernRendererBackendAdapter() {}
    virtual bool Attach(CModernGraphicsBootstrap& graphics) = 0;
    virtual void Detach() = 0;
    virtual bool IsReady() const = 0;
    virtual ModernGraphicsBackend GetBackend() const = 0;
#ifdef MU_ENABLE_DILIGENT
    virtual Diligent::IRenderDevice* GetDevice() const = 0;
    virtual Diligent::IDeviceContext* GetImmediateContext() const = 0;
#endif
};

#ifdef MU_ENABLE_DILIGENT
class CDiligentOpenGL46Adapter final : public IModernRendererBackendAdapter
{
public:
    CDiligentOpenGL46Adapter();
    bool Attach(CModernGraphicsBootstrap& graphics) override;
    void Detach() override;
    bool IsReady() const override;
    ModernGraphicsBackend GetBackend() const override;
    Diligent::IRenderDevice* GetDevice() const override;
    Diligent::IDeviceContext* GetImmediateContext() const override;
private:
    Diligent::IRenderDevice* m_device;
    Diligent::IDeviceContext* m_context;
};

class CModernGPUBuffer
{
public:
    CModernGPUBuffer();
    bool Create(Diligent::IRenderDevice* device, const char* name, std::uint64_t size,
                Diligent::BIND_FLAGS bindFlags, const void* initialData = nullptr);
    bool Update(Diligent::IDeviceContext* context, const void* data,
                std::uint64_t size, std::uint64_t offset = 0);
    void Reset();
    bool IsValid() const;
    std::uint64_t GetSize() const;
    Diligent::IBuffer* Get() const;
private:
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_buffer;
    std::uint64_t m_size;
};

class CModernConstantBuffer
{
public:
    bool Create(Diligent::IRenderDevice* device, const char* name, std::uint64_t size);
    bool Update(Diligent::IDeviceContext* context, const void* data, std::uint64_t size);
    void Reset();
    bool IsValid() const;
    Diligent::IBuffer* Get() const;
private:
    CModernGPUBuffer m_buffer;
};

class CModernShaderManager
{
public:
    Diligent::IShader* Get(std::uint64_t key) const;
    Diligent::IShader* GetOrCreateHLSL(Diligent::IRenderDevice* device, std::uint64_t key,
                                       const char* name, Diligent::SHADER_TYPE type,
                                       const char* source, const char* entryPoint = "main",
                                       bool useCombinedTextureSamplers = true);
    void Clear();
private:
    std::map<std::uint64_t, Diligent::RefCntAutoPtr<Diligent::IShader> > m_shaders;
};

struct ModernTextureCreateInfo
{
    std::uint32_t Width;
    std::uint32_t Height;
    Diligent::TEXTURE_FORMAT Format;
    const void* Pixels;
    std::uint64_t RowStride;
    ModernTextureCreateInfo();
};

class CModernTextureSamplerManager
{
public:
    bool CreateTexture2D(Diligent::IRenderDevice* device, std::uint64_t key,
                         const char* name, const ModernTextureCreateInfo& createInfo);
    Diligent::ITextureView* GetTextureView(std::uint64_t key) const;
    Diligent::ISampler* GetOrCreateSampler(Diligent::IRenderDevice* device,
                                           std::uint64_t key,
                                           const Diligent::SamplerDesc& desc);
    void Clear();
private:
    struct TextureResource
    {
        Diligent::RefCntAutoPtr<Diligent::ITexture> Texture;
        Diligent::RefCntAutoPtr<Diligent::ITextureView> ShaderResourceView;
    };
    std::map<std::uint64_t, TextureResource> m_textures;
    std::map<std::uint64_t, Diligent::RefCntAutoPtr<Diligent::ISampler> > m_samplers;
};

class CModernPipelineResourceCache
{
public:
    void RegisterPipeline(std::uint64_t key, Diligent::IPipelineState* pipeline);
    Diligent::IPipelineState* GetPipeline(std::uint64_t key) const;
    void RegisterResourceBinding(std::uint64_t key, Diligent::IShaderResourceBinding* binding);
    Diligent::IShaderResourceBinding* GetResourceBinding(std::uint64_t key) const;
    void Clear();
private:
    std::map<std::uint64_t, Diligent::RefCntAutoPtr<Diligent::IPipelineState> > m_pipelines;
    std::map<std::uint64_t, Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> > m_resourceBindings;
};


struct ModernBMDMeshKey
{
    std::uint64_t AssetId;
    std::uint32_t AssetGeneration;
    std::uint32_t MeshIndex;
    bool operator<(const ModernBMDMeshKey& other) const
    {
        return std::tie(AssetId, AssetGeneration, MeshIndex) <
               std::tie(other.AssetId, other.AssetGeneration, other.MeshIndex);
    }
};

struct ModernBMDMeshResource
{
    Diligent::RefCntAutoPtr<Diligent::IBuffer> VertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> IndexBuffer;
    std::uint32_t NumIndices = 0;
};

// Immutable asset geometry shared across entity instances. Keys must change
// when an asset is reloaded; returned pointers expire on RemoveAsset/Clear.
class CModernBMDMeshCache
{
public:
    bool Upload(Diligent::IRenderDevice* device, const ModernBMDMeshKey& key,
                const ModernBMDGeometry& geometry);
    const ModernBMDMeshResource* Get(const ModernBMDMeshKey& key) const;
    void RemoveAsset(std::uint64_t assetId);
    void Clear();
private:
    Diligent::IRenderDevice* m_device = nullptr; // Borrowed; core clears before device teardown.
    std::map<ModernBMDMeshKey, ModernBMDMeshResource> m_meshes;
};

struct ModernIndexedDrawSubmission
{
    Diligent::IPipelineState* Pipeline;
    Diligent::IShaderResourceBinding* Resources;
    Diligent::IBuffer* VertexBuffer;
    Diligent::IBuffer* IndexBuffer;
    std::uint64_t VertexBufferOffset;
    std::uint64_t IndexBufferOffset;
    std::uint32_t NumIndices;
    Diligent::VALUE_TYPE IndexType;
    std::uint32_t FirstIndex;
    std::uint32_t BaseVertex;
    ModernIndexedDrawSubmission();
};
#endif

class CModernRendererCore
{
public:
    CModernRendererCore();
    bool Initialize(CModernGraphicsBootstrap& graphics);
    void Shutdown();
    void OnResize(std::uint32_t width, std::uint32_t height);
    bool IsReady() const;
    ModernGraphicsBackend GetBackend() const;
    std::uint32_t GetWidth() const;
    std::uint32_t GetHeight() const;
#ifdef MU_ENABLE_DILIGENT
    IModernRendererBackendAdapter* GetBackendAdapter();
    CModernShaderManager& GetShaderManager();
    CModernBMDMeshCache& GetBMDMeshCache();
    CModernTextureSamplerManager& GetTextureSamplerManager();
    CModernPipelineResourceCache& GetPipelineResourceCache();
    bool SubmitIndexed(const ModernIndexedDrawSubmission& submission);
#endif
private:
    CModernGraphicsBootstrap* m_graphics;
    ModernGraphicsBackend m_backend;
    std::uint32_t m_width;
    std::uint32_t m_height;
    bool m_ready;
#ifdef MU_ENABLE_DILIGENT
    CDiligentOpenGL46Adapter m_openGL46Adapter;
    IModernRendererBackendAdapter* m_adapter;
    CModernShaderManager m_shaderManager;
    CModernBMDMeshCache m_bmdMeshCache;
    CModernTextureSamplerManager m_textureSamplerManager;
    CModernPipelineResourceCache m_pipelineResourceCache;
#endif
};

CModernRendererCore& GetModernRendererCore();
