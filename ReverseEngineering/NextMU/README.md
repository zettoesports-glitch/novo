# Reversa Vulkan NextMU

Análise estática clean-room dos executáveis NextMU x86/x64 e do pacote `Resources_DE_2024-01-25.zip`, separada da pasta `reversa opengl4.6`.

## Mapeamento OpenGL 4.6 / Resource

O mapeamento detalhado do pipeline gráfico, shaders, GPU skinning, Skeleton Texture, materiais, terrain, particles, attachments, animações e arquitetura alvo está em:

- [`OPENGL46_RESOURCE_MAPPING.md`](./OPENGL46_RESOURCE_MAPPING.md)

Esse documento usa o pacote real `Resources_DE_2024-01-25` como evidência primária e separa claramente itens confirmados, inferidos e pendentes.

## Resultado principal

O NextMU possui suporte real a múltiplos backends através da Diligent Engine:

- Direct3D 11
- Direct3D 12
- OpenGL
- Vulkan

O EXE seleciona/carrega o backend e o código Vulkan de baixo nível fica no plugin da Diligent Engine.

## DLLs Vulkan confirmadas no pacote real

- x86: `GraphicsEngineVk_32r.dll`
- x64: `GraphicsEngineVk_64r.dll`

Exports:

```text
GetEngineFactoryVk
```

A DLL Vulkan carrega dinamicamente:

```text
vulkan-1.dll
vkGetInstanceProcAddr
vkGetDeviceProcAddr
```

E contém o caminho completo de criação/uso Vulkan: instance, physical/logical device, swapchain, command buffers, descriptors, pipelines, memória, fences/semaphores, presentation e shader compilation para SPIR-V.

## Evidências fortes dentro das DLLs

Chamadas/strings confirmadas incluem:

```text
vkCreateInstance
vkEnumeratePhysicalDevices
vkCreateDevice
vkCreateSwapchainKHR
vkAcquireNextImageKHR
vkQueuePresentKHR
vkCreateCommandPool
vkAllocateCommandBuffers
vkCreateDescriptorPool
vkCreateDescriptorSetLayout
vkCreateGraphicsPipelines
vkCreateComputePipelines
vkCreateSemaphore
vkCreateFence
vkAllocateMemory
vkMapMemory
```

Extensões/layers confirmadas:

```text
VK_KHR_surface
VK_KHR_win32_surface
VK_KHR_swapchain
VK_EXT_debug_report
VK_EXT_debug_utils
VK_LAYER_KHRONOS_validation
```

## Shader toolchain confirmada

A DLL traz componentes de glslang e SPIR-V Tools e contém rotinas da própria Diligent:

```text
Diligent::GLSLangUtils::HLSLtoSPIRV
Diligent::GLSLangUtils::GLSLtoSPIRV
Diligent::SPIRVShaderResources
Diligent::ShaderVkImpl
```

Logo o backend suporta converter HLSL/GLSL para SPIR-V e validar/refletir recursos.

## Source paths preservados

Foram encontrados paths como:

```text
DiligentCore/Graphics/GraphicsEngineVulkan/src/EngineFactoryVk.cpp
DiligentCore/Graphics/GraphicsEngineVulkan/src/DeviceContextVkImpl.cpp
DiligentCore/Graphics/GraphicsEngineVulkan/src/SwapChainVkImpl.cpp
DiligentCore/Graphics/GraphicsEngineVulkan/src/BufferVkImpl.cpp
DiligentCore/Graphics/GraphicsEngineVulkan/src/TextureVkImpl.cpp
DiligentCore/Graphics/GraphicsEngineVulkan/src/ShaderVkImpl.cpp
DiligentCore/Graphics/GraphicsEngineVulkan/src/PipelineStateVkImpl.cpp
DiligentCore/Graphics/GraphicsEngineVulkan/src/VulkanUtilities/VulkanInstance.cpp
DiligentCore/Graphics/GraphicsEngineVulkan/src/VulkanUtilities/VulkanPhysicalDevice.cpp
DiligentCore/Graphics/GraphicsEngineVulkan/src/VulkanUtilities/VulkanLogicalDevice.cpp
DiligentCore/Graphics/GraphicsEngineVulkan/src/VulkanUtilities/VulkanMemoryManager.cpp
```

Também aparece no EXE:

```text
D:\NextMU\client-cmake\client\game\src\mu_graphics.cpp
MUGraphics::InitializeEngine
```

## Ordem de tentativa dos backends

Mapeamento observado:

```text
1 = Direct3D11
2 = Direct3D12
3 = OpenGL
5 = Vulkan
```

Sequência automática encontrada:

```text
Direct3D11 -> Vulkan -> Direct3D12 -> OpenGL
```

## Limite honesto

As DLLs mostram que o backend Vulkan é completo, porém isso não significa que o NextMU use todas as features que a Diligent suporta. Devemos separar sempre: capacidade existente no backend vs. recurso efetivamente usado pelo jogo.
