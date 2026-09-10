# Source reconstruída de integração

Estes arquivos são uma referência clean-room da camada de integração observada no pacote. **Não são a source original do NextMU nem cópia do DiligentCore**.

Objetivo:

```text
config/preference
   -> escolher backend
   -> carregar plugin
   -> resolver factory
   -> criar device/context/swapchain
   -> entregar interface comum ao renderer MU
```

Arquivos:

- `RendererBackend.h` — enum/contrato de backend.
- `BackendModuleLoader.cpp` — construção do nome da DLL + `LoadLibraryA`/`GetProcAddress`.
- `GraphicsBackendSelector.cpp` — ordem/fallback recuperada.
- `VulkanBootstrap.pseudocode.cpp` — sequência de inicialização Diligent/Vulkan para orientar port.

Para integração real com Diligent Engine, substituir os `void*`/pseudotipos pelos headers oficiais da versão selecionada.
