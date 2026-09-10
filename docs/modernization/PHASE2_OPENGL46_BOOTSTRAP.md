# Fase 2 — Bootstrap Diligent / OpenGL 4.6

Data: 10/09/2026.
Branch: `modernization`.

## Objetivo

Criar a primeira fronteira de runtime para o renderer moderno sem substituir ainda o desenho BMD legado. A fase deve provar ownership de janela/contexto/present, exigir OpenGL 4.6 e preparar os contratos usados por Vulkan e Direct3D 11 futuramente.

## Entregas desta fase

1. Fixar a revisão de Diligent e a revisão dos shaders de referência.
2. Definir ownership explícito de `HWND`, dispositivo, contexto imediato e apresentação.
3. Exigir OpenGL 4.6 no backend OpenGL; falhar com diagnóstico se a versão não estiver disponível.
4. Tratar resize por uma única fronteira do renderer.
5. Garantir apenas uma apresentação por frame.
6. Adicionar diagnósticos de vendor/renderer/version/profile/backend.
7. Disponibilizar um caminho reproduzível para preparar/compilar o backend Diligent Win32.
8. Manter BMD, terrain, effects e UI no legado até o bootstrap ser validado em Windows.

## Estado implementado no branch

A fronteira de bootstrap existe em:

- `SRCMainGS/Source/Main5.2/source/ModernGraphicsBootstrap.h`
- `SRCMainGS/Source/Main5.2/source/ModernGraphicsBootstrap.inl`
- compilação temporária através de `CShaderGL.cpp`, que já pertence ao `Main.vcxproj`.

O código define os backends `Legacy`, `OpenGL46`, `Vulkan` e `Direct3D11`, porém somente OpenGL 4.6 possui caminho de inicialização nesta fase.

O caminho OpenGL usa `IEngineFactoryOpenGL::AttachToActiveGLContext`. Isto é intencional: a Main continua dona de `HWND`, `HDC`, `HGLRC` e `SwapBuffers`; Diligent é anexado ao contexto existente e não cria swap chain durante a coexistência.

## Carregamento do backend

A Main não faz link estático do engine OpenGL do Diligent.

Com `ENGINE_DLL=1`, o bootstrap usa o loader Win32 oficial exposto por Diligent:

- `LoadGraphicsEngineOpenGL()` carrega o módulo do backend;
- o módulo exporta `GetEngineFactoryOpenGL`;
- Release usa `GraphicsEngineOpenGL_32r.dll`;
- Debug da Main é alinhado com `DILIGENT_DEBUG` e usa `GraphicsEngineOpenGL_32d.dll`.

Se o módulo não puder ser carregado ou a factory retornar nula, a inicialização moderna falha de forma controlada e o renderer legado permanece ativo.

Isto aproxima a fronteira da arquitetura modular observada no pacote de referência sem acoplar o código de gameplay a uma API gráfica específica.

## Preparação reproduzível da dependência

Foi adicionado:

`SRCMainGS/Source/Main5.2/setup_diligent_opengl46.ps1`

O script fixa e verifica:

- tag DiligentCore: `v2.5.6`;
- commit: `b036337d68be2353c9950a85929acf796b9a6d50`.

Ele prepara os submodules, configura um build Win32 OpenGL-only com HLSL habilitado, compila os backends Release/Debug e copia as DLLs para `Client_2`.

Também pode compilar a Main:

```powershell
powershell -ExecutionPolicy Bypass -File .\setup_diligent_opengl46.ps1 -BuildMain
```

O checkout e os outputs gerados ficam em `dependencies/DiligentCore/` e `dependencies/_diligent_build/`, ambos ignorados pelo `.gitignore` da `Main5.2`.

`ModernGraphicsBootstrap.h` ativa `MU_ENABLE_DILIGENT` automaticamente no MSVC/Win32 quando detecta o header OpenGL no layout preparado pelo script. Sem a dependência, o código legado continua sendo o caminho padrão.

## Lifecycle de coexistência

O lifecycle real foi mapeado e conectado por uma ponte temporária `WH_CALLWNDPROC` no mesmo thread da UI quando `MU_ENABLE_DILIGENT` está habilitado. A ponte:

- detecta o contexto WGL real depois que ele está current;
- chama `InitializeOpenGL46(...)` uma única vez por `HWND/HGLRC`;
- encaminha `WM_SIZE` para `OnResize(...)`;
- chama `Shutdown()` antes de `WM_CLOSE/WM_DESTROY/WM_NCDESTROY` chegar ao caminho que executa `KillGLWindow()`;
- não substitui `WndProc`, não cria outro contexto e não apresenta frames.

Esta ponte continua deliberadamente temporária para a Fase 2. Depois que o gate Win32 provar a integração, as chamadas podem ser movidas diretamente para os owners reais (`CreateOpenglWindow`, `CWINHANDLE::WndProc` e shutdown) sem alterar o contrato público de `CModernGraphicsBootstrap`.

A descoberta detalhada está em `PHASE2_RUNTIME_DISCOVERY.md`.

## Dependências fixadas

DiligentCore:

- tag: `v2.5.6`
- commit: `b036337d68be2353c9950a85929acf796b9a6d50`

Snapshot de referência dos shaders/resources:

- repo: `zettoesports-glitch/novo`
- baseline: `ceadb719f47bc781f789fe56d2e0eff2c4c8012b`
- package: `Resources_DE_2024-01-25`

Detalhes e ressalvas em `DILIGENT_PIN.md`.

## Regras de coexistência

- O backend moderno não pode apresentar o mesmo frame duas vezes.
- `SwapBuffers(hDC)` continua exclusivamente no renderer/aplicação legado durante esta fase.
- O renderer moderno não destrói nem substitui `g_hDC/g_hRC`.
- Ownership Diligent do contexto é `Attached`, nunca `Owned`, quando a inicialização é bem-sucedida.
- Ao alternar de comandos raw OpenGL para comandos Diligent, `BeginModernPass()` invalida o cache de estado Diligent via `IDeviceContext::InvalidateState()`.
- Um perfil Core-only não é aceitável para coexistência com os caminhos fixed-function/client-array atuais. O bootstrap registra o profile efetivo para diagnóstico.
- OpenGL 4.6 é o único backend moderno ativo nesta fase. Vulkan e Direct3D 11 são extension points.

## Contexto atual da Main

`CreateOpenglWindow()` ainda usa `wglCreateContext()` e não solicita explicitamente uma versão 4.6. Por isso o bootstrap valida a versão **efetivamente fornecida pelo driver**. Se ela for menor que 4.6, a modernização permanece inativa e o legado continua ativo.

Uma futura troca para criação explícita de contexto 4.6 compatibility deve acontecer antes da criação dos assets OpenGL e só depois de provar que não quebra os caminhos legados. Não é necessário substituir o contexto apenas para concluir o wiring desta etapa.

## Diagnósticos implementados

Na tentativa de attach são registrados via debugger:

- `GL_VENDOR`;
- `GL_RENDERER`;
- `GL_VERSION`;
- `GL_SHADING_LANGUAGE_VERSION`;
- major/minor interpretados;
- `core` / `compatibility` profile quando disponível;
- falha ao carregar a DLL/factory Diligent;
- sucesso/falha do attach Diligent;
- confirmação de que o `SwapBuffers` legado permanece autoritativo.

O `CErrorReport::WriteOpenGLInfo()` existente continua registrando a informação persistente do OpenGL da Main.

## Estado de conclusão

### Concluído no repositório

- pin do Diligent e shaders/resources;
- ownership e single-present definidos;
- mapeamento real do lifecycle da Main;
- bridge de attach/resize/shutdown;
- validação de versão OpenGL 4.6 e diagnósticos;
- loader modular do backend OpenGL;
- setup reproduzível Win32/OpenGL/HLSL;
- fallback legado quando dependência/DLL/capability não estiver disponível.

### Ainda não validado

A Fase 2 só será considerada **runtime-concluída** após build/execução Windows comprovando:

- o script prepara DiligentCore e compila `GraphicsEngineOpenGL_32r/32d.dll`;
- `Main.sln` compila com os headers pinados;
- criação/anexação do backend OpenGL;
- versão efetiva >= 4.6;
- resize sem crash;
- exatamente uma apresentação por frame;
- shutdown sem erro de lifetime/contexto;
- logs de vendor, renderer, version, profile e backend;
- nenhuma regressão no caminho legado mantido durante o protótipo.

## Próxima ação

Executar `setup_diligent_opengl46.ps1 -BuildMain` em Windows/Visual Studio e rodar a Main para fechar o gate runtime. Depois disso entram os layouts CPU/GPU concretos, constant buffers, pose conversion, Skeleton Texture e o primeiro BMD moderno de duas instâncias.
