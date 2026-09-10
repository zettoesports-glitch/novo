# Fase 2 — Bootstrap Diligent / OpenGL 4.6

Data: 10/09/2026.
Branch: `modernization`.

## Objetivo

Criar a primeira fronteira de runtime para o renderer moderno sem substituir ainda o desenho BMD legado. A fase deve provar ownership de janela/contexto/present, exigir OpenGL 4.6 e preparar os contratos usados por Vulkan e Direct3D 11 futuramente.

## Entregas desta fase

1. Fixar a revisão de Diligent e a revisão dos shaders compartilhados.
2. Definir ownership explícito de `HWND`, dispositivo, contexto imediato, swap chain e apresentação.
3. Exigir OpenGL 4.6 no backend OpenGL; falhar com diagnóstico se a versão não estiver disponível.
4. Tratar resize por uma única fronteira do renderer.
5. Garantir apenas uma apresentação por frame.
6. Adicionar diagnósticos de vendor/renderer/version/backend.
7. Manter BMD, terrain, effects e UI no legado até o bootstrap ser validado em Windows.

## Regras de coexistência

- O backend moderno não pode apresentar o mesmo frame duas vezes.
- O renderer legado não pode destruir ou substituir o contexto que estiver sob ownership do renderer moderno.
- Se a integração Diligent anexar a um contexto existente, o ownership precisa ser explicitamente `Attached`; se criar o contexto, `Owned`.
- Um perfil Core-only não permite a execução dos caminhos fixed-function/client-array legados. Enquanto houver fallback legado no mesmo frame, a configuração precisa ser compatível com essa coexistência ou o protótipo deve rodar isolado.
- OpenGL 4.6 é o único backend de produção nesta fase. Vulkan e Direct3D 11 permanecem somente como valores/extension points da API.

## Interface mínima proposta

```cpp
enum class RendererBackend
{
    Legacy,
    OpenGL46,
    Vulkan,
    Direct3D11,
};

enum class GraphicsOwnership
{
    None,
    Attached,
    Owned,
};

struct RendererInitDesc
{
    void* NativeWindow = nullptr;
    unsigned int Width = 0;
    unsigned int Height = 0;
    RendererBackend Backend = RendererBackend::OpenGL46;
    GraphicsOwnership Ownership = GraphicsOwnership::Attached;
};

class IRendererRuntime
{
public:
    virtual ~IRendererRuntime() = default;
    virtual bool Initialize(const RendererInitDesc& desc) = 0;
    virtual void Resize(unsigned int width, unsigned int height) = 0;
    virtual void BeginFrame() = 0;
    virtual void Present() = 0;
    virtual void Shutdown() = 0;
};
```

Os nomes acima são contrato de integração; a implementação deve usar os tipos reais de Diligent e a infraestrutura existente do Main após confirmação do projeto/SDK no repositório.

## Gate de validação

A Fase 2 só será considerada concluída após build/execução Windows comprovando:

- criação/anexação do backend OpenGL;
- versão efetiva >= 4.6;
- resize sem crash;
- exatamente uma apresentação por frame;
- shutdown sem erro de lifetime;
- log de vendor, renderer, version e backend;
- nenhuma regressão no caminho legado mantido durante o protótipo.

## Estado atual

O mapa estático da Fase 1 está concluído. Este documento inicia a Fase 2, mas não declara validação runtime. A implementação deve avançar somente após localizar no projeto os pontos reais de criação da janela/contexto e a forma correta de anexar ou criar o dispositivo Diligent.
