// Clean-room architectural pseudocode based on the analyzed package.
// Replace pseudo-types with the selected Diligent Engine headers for real integration.

/*
RendererResult StartVulkan(HWND hwnd, const SwapChainConfig& cfg)
{
    // 1) Load plugin according to architecture/build.
    auto module = LoadBackendModule("GraphicsEngineVk", "GetEngineFactoryVk");
    if (!module.factory)
        return RendererResult::BackendUnavailable;

    // 2) Get IEngineFactoryVk from GetEngineFactoryVk.
    auto* factory = GetTypedEngineFactoryVk(module.factory);

    // 3) EngineVkCreateInfo:
    //    - validation enabled in development
    //    - dynamic heap sized from measured peak usage
    //    - immediate context/queue configuration
    //    - requested features only, not every feature supported by the DLL
    EngineVkCreateInfo engineCI{};

    // 4) Create device and immediate context.
    factory->CreateDeviceAndContextsVk(engineCI, &device, &immediateContext);
    if (!device || !immediateContext)
        return RendererResult::DeviceCreationFailed;

    // 5) Create Win32 swapchain.
    Win32NativeWindow window{hwnd};
    factory->CreateSwapChainVk(device,
                               immediateContext,
                               swapChainDesc,
                               window,
                               &swapChain);
    if (!swapChain)
        return RendererResult::SwapChainCreationFailed;

    // 6) Create our renderer-level services.
    //    ResourceManager
    //    ShaderManager
    //    PipelineCache
    //    Descriptor/Binding cache
    //    FrameContext ring
    //    Dynamic upload/ring allocator

    // 7) Initialize renderers in controlled order:
    //    Model -> Shadow -> 2D -> Terrain -> Environment -> Effects.

    return RendererResult::Ok;
}

Frame loop concept:

Acquire back buffer
    -> wait/recycle FrameContext
    -> reset dynamic frame allocators
    -> record commands/batches
    -> submit
    -> present
    -> advance FrameContext

Important rule for our MU:
Do not expose VkDevice/VkCommandBuffer directly to gameplay code.
Keep backend details behind renderer/resource interfaces.
*/
