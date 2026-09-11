#pragma once

#include "ModernRendererCore.h"

#ifdef MU_ENABLE_DILIGENT

inline CDiligentOpenGL46Adapter::CDiligentOpenGL46Adapter()
    : m_device(nullptr), m_context(nullptr)
{
}

inline bool CDiligentOpenGL46Adapter::Attach(CModernGraphicsBootstrap& graphics)
{
    Detach();
    if (!graphics.IsActive() ||
        graphics.GetBackend() != ModernGraphicsBackend::OpenGL46 ||
        !graphics.IsOpenGL46Capable())
        return false;

    m_device = graphics.GetDevice();
    m_context = graphics.GetImmediateContext();
    return m_device != nullptr && m_context != nullptr;
}

inline void CDiligentOpenGL46Adapter::Detach()
{
    m_device = nullptr;
    m_context = nullptr;
}

inline bool CDiligentOpenGL46Adapter::IsReady() const
{
    return m_device != nullptr && m_context != nullptr;
}

inline ModernGraphicsBackend CDiligentOpenGL46Adapter::GetBackend() const
{
    return ModernGraphicsBackend::OpenGL46;
}

inline Diligent::IRenderDevice* CDiligentOpenGL46Adapter::GetDevice() const
{
    return m_device;
}

inline Diligent::IDeviceContext* CDiligentOpenGL46Adapter::GetImmediateContext() const
{
    return m_context;
}

inline CModernGPUBuffer::CModernGPUBuffer() : m_size(0) {}

inline bool CModernGPUBuffer::Create(Diligent::IRenderDevice* device,
                                     const char* name,
                                     std::uint64_t size,
                                     Diligent::BIND_FLAGS bindFlags,
                                     const void* initialData)
{
    Reset();
    if (device == nullptr || size == 0 || bindFlags == Diligent::BIND_NONE)
        return false;

    Diligent::BufferDesc desc;
    desc.Name = name;
    desc.Size = static_cast<Diligent::Uint64>(size);
    desc.BindFlags = bindFlags;
    desc.Usage = Diligent::USAGE_DEFAULT;
    desc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;

    Diligent::BufferData data;
    Diligent::BufferData* dataPtr = nullptr;
    if (initialData != nullptr)
    {
        data.pData = initialData;
        data.DataSize = static_cast<Diligent::Uint64>(size);
        dataPtr = &data;
    }

    device->CreateBuffer(desc, dataPtr, &m_buffer);
    if (!m_buffer)
        return false;

    m_size = size;
    return true;
}

inline bool CModernGPUBuffer::Update(Diligent::IDeviceContext* context,
                                     const void* data,
                                     std::uint64_t size,
                                     std::uint64_t offset)
{
    if (context == nullptr || !m_buffer || data == nullptr || size == 0)
        return false;
    if (offset > m_size || size > (m_size - offset))
        return false;

    context->UpdateBuffer(m_buffer,
                          static_cast<Diligent::Uint64>(offset),
                          static_cast<Diligent::Uint64>(size),
                          data,
                          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    return true;
}

inline void CModernGPUBuffer::Reset()
{
    m_buffer.Release();
    m_size = 0;
}

inline bool CModernGPUBuffer::IsValid() const { return static_cast<bool>(m_buffer); }
inline std::uint64_t CModernGPUBuffer::GetSize() const { return m_size; }
inline Diligent::IBuffer* CModernGPUBuffer::Get() const { return m_buffer.Get(); }

inline bool CModernConstantBuffer::Create(Diligent::IRenderDevice* device,
                                          const char* name,
                                          std::uint64_t size)
{
    const std::uint64_t alignedSize = (size + 15u) & ~std::uint64_t(15u);
    return m_buffer.Create(device, name, alignedSize, Diligent::BIND_UNIFORM_BUFFER, nullptr);
}

inline bool CModernConstantBuffer::Update(Diligent::IDeviceContext* context,
                                          const void* data,
                                          std::uint64_t size)
{
    return m_buffer.Update(context, data, size, 0);
}

inline void CModernConstantBuffer::Reset() { m_buffer.Reset(); }
inline bool CModernConstantBuffer::IsValid() const { return m_buffer.IsValid(); }
inline Diligent::IBuffer* CModernConstantBuffer::Get() const { return m_buffer.Get(); }

inline Diligent::IShader* CModernShaderManager::Get(std::uint64_t key) const
{
    const auto it = m_shaders.find(key);
    return it != m_shaders.end() ? it->second.RawPtr() : nullptr;
}

inline Diligent::IShader* CModernShaderManager::GetOrCreateHLSL(Diligent::IRenderDevice* device,
                                                                 std::uint64_t key,
                                                                 const char* name,
                                                                 Diligent::SHADER_TYPE type,
                                                                 const char* source,
                                                                 const char* entryPoint,
                                                                 bool useCombinedTextureSamplers)
{
    if (Diligent::IShader* existing = Get(key))
        return existing;
    if (device == nullptr || source == nullptr || entryPoint == nullptr)
        return nullptr;

    Diligent::ShaderCreateInfo createInfo;
    createInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    // Main stores/multiplies transform contracts row-major. Diligent's OpenGL
    // HLSL conversion also needs this explicit flag or uniform-block matrix
    // packing can diverge from the CPU layout even when the HLSL is otherwise valid.
    createInfo.CompileFlags = Diligent::SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;
    createInfo.Desc.Name = name;
    createInfo.Desc.ShaderType = type;
    createInfo.Desc.UseCombinedTextureSamplers = useCombinedTextureSamplers;
    createInfo.EntryPoint = entryPoint;
    createInfo.Source = source;

    Diligent::RefCntAutoPtr<Diligent::IShader> shader;
    device->CreateShader(createInfo, &shader);
    if (!shader)
        return nullptr;

    m_shaders[key] = shader;
    return shader.RawPtr();
}

inline void CModernShaderManager::Clear()
{
    m_shaders.clear();
}

inline ModernTextureCreateInfo::ModernTextureCreateInfo()
    : Width(0), Height(0), Format(Diligent::TEX_FORMAT_UNKNOWN), Pixels(nullptr), RowStride(0)
{
}

inline bool CModernTextureSamplerManager::CreateTexture2D(Diligent::IRenderDevice* device,
                                                           std::uint64_t key,
                                                           const char* name,
                                                           const ModernTextureCreateInfo& createInfo)
{
    if (m_textures.find(key) != m_textures.end())
        return true;
    if (device == nullptr || createInfo.Width == 0 || createInfo.Height == 0 ||
        createInfo.Format == Diligent::TEX_FORMAT_UNKNOWN)
        return false;

    Diligent::TextureDesc desc;
    desc.Name = name;
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = createInfo.Width;
    desc.Height = createInfo.Height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = createInfo.Format;
    desc.SampleCount = 1;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    desc.Usage = createInfo.Pixels != nullptr ? Diligent::USAGE_IMMUTABLE : Diligent::USAGE_DEFAULT;

    Diligent::TextureSubResData subresource;
    Diligent::TextureData textureData;
    Diligent::TextureData* textureDataPtr = nullptr;
    if (createInfo.Pixels != nullptr)
    {
        if (createInfo.RowStride == 0)
            return false;
        subresource.pData = createInfo.Pixels;
        subresource.Stride = static_cast<Diligent::Uint64>(createInfo.RowStride);
        textureData.pSubResources = &subresource;
        textureData.NumSubresources = 1;
        textureDataPtr = &textureData;
    }

    TextureResource resource;
    device->CreateTexture(desc, textureDataPtr, &resource.Texture);
    if (!resource.Texture)
        return false;

    resource.ShaderResourceView = resource.Texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    if (!resource.ShaderResourceView)
        return false;

    m_textures[key] = resource;
    return true;
}

inline Diligent::ITextureView* CModernTextureSamplerManager::GetTextureView(std::uint64_t key) const
{
    const auto it = m_textures.find(key);
    return it != m_textures.end() ? it->second.ShaderResourceView.RawPtr() : nullptr;
}

inline Diligent::ISampler* CModernTextureSamplerManager::GetOrCreateSampler(Diligent::IRenderDevice* device,
                                                                             std::uint64_t key,
                                                                             const Diligent::SamplerDesc& desc)
{
    const auto it = m_samplers.find(key);
    if (it != m_samplers.end())
        return it->second.RawPtr();
    if (device == nullptr)
        return nullptr;

    Diligent::RefCntAutoPtr<Diligent::ISampler> sampler;
    device->CreateSampler(desc, &sampler);
    if (!sampler)
        return nullptr;

    m_samplers[key] = sampler;
    return sampler.RawPtr();
}

inline void CModernTextureSamplerManager::Clear()
{
    m_samplers.clear();
    m_textures.clear();
}

inline void CModernPipelineResourceCache::RegisterPipeline(std::uint64_t key,
                                                            Diligent::IPipelineState* pipeline)
{
    if (pipeline != nullptr)
        m_pipelines[key] = pipeline;
}

inline Diligent::IPipelineState* CModernPipelineResourceCache::GetPipeline(std::uint64_t key) const
{
    const auto it = m_pipelines.find(key);
    return it != m_pipelines.end() ? it->second.RawPtr() : nullptr;
}

inline void CModernPipelineResourceCache::RegisterResourceBinding(
    std::uint64_t key,
    Diligent::IShaderResourceBinding* binding)
{
    if (binding != nullptr)
        m_resourceBindings[key] = binding;
}

inline Diligent::IShaderResourceBinding* CModernPipelineResourceCache::GetResourceBinding(std::uint64_t key) const
{
    const auto it = m_resourceBindings.find(key);
    return it != m_resourceBindings.end() ? it->second.RawPtr() : nullptr;
}

inline void CModernPipelineResourceCache::Clear()
{
    m_resourceBindings.clear();
    m_pipelines.clear();
}

inline bool CModernBMDMeshCache::Upload(Diligent::IRenderDevice* device,
                                        const ModernBMDMeshKey& key,
                                        const ModernBMDGeometry& geometry)
{
    if (device == nullptr)
        return false;
    // Do not mix resources belonging to different render devices.
    if (m_device != nullptr && m_device != device)
        return false;
    if (Get(key) != nullptr)
        return true; // Same immutable asset revision: no per-frame re-upload.
    if (geometry.Vertices.empty() || geometry.Indices.empty() ||
        geometry.Indices.size() % 3 != 0 ||
        geometry.Indices.size() > (std::numeric_limits<std::uint32_t>::max)())
        return false;
    for (const auto index : geometry.Indices)
        if (index >= geometry.Vertices.size())
            return false;

    ModernBMDMeshResource resource;
    Diligent::BufferDesc desc;
    desc.Name = "Modern BMD immutable vertices";
    desc.Size = static_cast<Diligent::Uint64>(geometry.Vertices.size()) * sizeof(ModernBMDVertex);
    desc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
    desc.Usage = Diligent::USAGE_IMMUTABLE;
    Diligent::BufferData data;
    data.pData = geometry.Vertices.data();
    data.DataSize = desc.Size;
    device->CreateBuffer(desc, &data, &resource.VertexBuffer);
    if (!resource.VertexBuffer)
        return false;

    desc.Name = "Modern BMD immutable indices";
    desc.Size = static_cast<Diligent::Uint64>(geometry.Indices.size()) * sizeof(std::uint32_t);
    desc.BindFlags = Diligent::BIND_INDEX_BUFFER;
    data.pData = geometry.Indices.data();
    data.DataSize = desc.Size;
    device->CreateBuffer(desc, &data, &resource.IndexBuffer);
    if (!resource.IndexBuffer)
        return false; // Local vertex buffer is released; no partial cache entry.

    resource.NumIndices = static_cast<std::uint32_t>(geometry.Indices.size());
    m_meshes.emplace(key, resource);
    m_device = device;
    return true;
}

inline const ModernBMDMeshResource* CModernBMDMeshCache::Get(const ModernBMDMeshKey& key) const
{
    const auto it = m_meshes.find(key);
    return it == m_meshes.end() ? nullptr : &it->second;
}

inline void CModernBMDMeshCache::RemoveAsset(std::uint64_t assetId)
{
    for (auto it = m_meshes.begin(); it != m_meshes.end(); )
    {
        if (it->first.AssetId == assetId)
            it = m_meshes.erase(it);
        else
            ++it;
    }
}

inline void CModernBMDMeshCache::Clear()
{
    m_meshes.clear();
    m_device = nullptr;
}

inline ModernIndexedDrawSubmission::ModernIndexedDrawSubmission()
    : Pipeline(nullptr), Resources(nullptr), VertexBuffer(nullptr), IndexBuffer(nullptr),
      VertexBufferOffset(0), IndexBufferOffset(0), NumIndices(0), IndexType(Diligent::VT_UINT16),
      FirstIndex(0), BaseVertex(0), RenderTarget(nullptr), DepthStencil(nullptr),
      TargetWidth(0), TargetHeight(0)
{
}

#endif // MU_ENABLE_DILIGENT

inline CModernRendererCore::CModernRendererCore()
    : m_graphics(nullptr), m_backend(ModernGraphicsBackend::Legacy), m_width(0), m_height(0), m_ready(false)
#ifdef MU_ENABLE_DILIGENT
    , m_adapter(nullptr)
#endif
{
}

inline bool CModernRendererCore::Initialize(CModernGraphicsBootstrap& graphics)
{
    if (m_ready && m_graphics == &graphics && m_backend == graphics.GetBackend())
        return true;

    Shutdown();
    m_graphics = &graphics;
    m_backend = graphics.GetBackend();
    m_width = graphics.GetWidth();
    m_height = graphics.GetHeight();

#ifdef MU_ENABLE_DILIGENT
    switch (m_backend)
    {
    case ModernGraphicsBackend::OpenGL46:
        m_adapter = &m_openGL46Adapter;
        break;
    case ModernGraphicsBackend::Vulkan:
    case ModernGraphicsBackend::Direct3D11:
    case ModernGraphicsBackend::Legacy:
    default:
        m_adapter = nullptr;
        break;
    }

    if (m_adapter == nullptr || !m_adapter->Attach(graphics))
    {
        Shutdown();
        return false;
    }

    m_ready = true;
    return true;
#else
    Shutdown();
    return false;
#endif
}

inline void CModernRendererCore::Shutdown()
{
#ifdef MU_ENABLE_DILIGENT
    m_pipelineResourceCache.Clear();
    m_bmdMeshCache.Clear();
    m_textureSamplerManager.Clear();
    m_shaderManager.Clear();
    if (m_adapter != nullptr)
        m_adapter->Detach();
    m_adapter = nullptr;
#endif

    m_graphics = nullptr;
    m_backend = ModernGraphicsBackend::Legacy;
    m_width = 0;
    m_height = 0;
    m_ready = false;
}

inline void CModernRendererCore::OnResize(std::uint32_t width, std::uint32_t height)
{
    if (width == 0 || height == 0)
        return;
    m_width = width;
    m_height = height;
}

inline bool CModernRendererCore::IsReady() const { return m_ready; }
inline ModernGraphicsBackend CModernRendererCore::GetBackend() const { return m_backend; }
inline std::uint32_t CModernRendererCore::GetWidth() const { return m_width; }
inline std::uint32_t CModernRendererCore::GetHeight() const { return m_height; }

#ifdef MU_ENABLE_DILIGENT
inline IModernRendererBackendAdapter* CModernRendererCore::GetBackendAdapter() { return m_adapter; }
inline CModernShaderManager& CModernRendererCore::GetShaderManager() { return m_shaderManager; }
inline CModernBMDMeshCache& CModernRendererCore::GetBMDMeshCache() { return m_bmdMeshCache; }
inline CModernTextureSamplerManager& CModernRendererCore::GetTextureSamplerManager() { return m_textureSamplerManager; }
inline CModernPipelineResourceCache& CModernRendererCore::GetPipelineResourceCache() { return m_pipelineResourceCache; }

inline bool CModernRendererCore::SubmitIndexed(const ModernIndexedDrawSubmission& submission)
{
    if (!m_ready || m_adapter == nullptr || !m_adapter->IsReady() || m_graphics == nullptr)
        return false;
    if (submission.Pipeline == nullptr || submission.VertexBuffer == nullptr ||
        submission.IndexBuffer == nullptr || submission.NumIndices == 0 ||
        submission.RenderTarget == nullptr || submission.TargetWidth == 0 || submission.TargetHeight == 0)
        return false;

    Diligent::IDeviceContext* context = m_adapter->GetImmediateContext();
    if (context == nullptr)
        return false;

    // Raw OpenGL owns the rest of the current frame during coexistence.
    // Invalidate Diligent's GL cache at the raw-GL -> Diligent boundary before
    // establishing every piece of modern pass state explicitly.
    m_graphics->BeginModernPass();

    Diligent::ITextureView* renderTargets[] = { submission.RenderTarget };
    context->SetRenderTargets(1,
                              renderTargets,
                              submission.DepthStencil,
                              Diligent::RESOURCE_STATE_TRANSITION_MODE_NONE);

    Diligent::Viewport viewport;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(submission.TargetWidth);
    viewport.Height = static_cast<float>(submission.TargetHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->SetViewports(1, &viewport, submission.TargetWidth, submission.TargetHeight);

    context->SetPipelineState(submission.Pipeline);

    Diligent::IBuffer* vertexBuffers[] = { submission.VertexBuffer };
    Diligent::Uint64 vertexOffsets[] = { static_cast<Diligent::Uint64>(submission.VertexBufferOffset) };
    context->SetVertexBuffers(0, 1, vertexBuffers, vertexOffsets,
                              Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                              Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
    context->SetIndexBuffer(submission.IndexBuffer,
                            static_cast<Diligent::Uint64>(submission.IndexBufferOffset),
                            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    if (submission.Resources != nullptr)
        context->CommitShaderResources(submission.Resources,
                                       Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    Diligent::DrawIndexedAttribs draw;
    draw.NumIndices = submission.NumIndices;
    draw.IndexType = submission.IndexType;
    draw.FirstIndexLocation = submission.FirstIndex;
    draw.BaseVertex = submission.BaseVertex;
    draw.NumInstances = 1;
    draw.Flags = Diligent::DRAW_FLAG_NONE;
    context->DrawIndexed(draw);
    return true;
}
#endif

inline CModernRendererCore& GetModernRendererCore()
{
    // Process-lifetime service. Explicit Shutdown releases GPU objects before
    // CModernGraphicsBootstrap releases the Diligent device/context.
    static CModernRendererCore* renderer = new CModernRendererCore();
    return *renderer;
}