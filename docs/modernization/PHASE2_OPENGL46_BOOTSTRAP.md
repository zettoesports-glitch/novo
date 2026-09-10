# Fase 2 — Bootstrap Diligent / OpenGL 4.6

Data: 10/09/2026.
Branch: `modernization`.

## Objetivo

Criar a primeira fronteira de runtime para o renderer moderno sem substituir ainda o desenho BMD legado. A fase prova ownership de janela/contexto/present, exige OpenGL 4.6 e prepara os contratos usados por Vulkan e Direct3D 11 futuramente.

## Entregas desta fase

1. Fixar a revisão de Diligent e a revisão dos shaders de referência.
2. Definir ownership explícito de `HWND`, dispositivo, contexto imediato e apresentação.
3. Exigir OpenGL 4.6 no backend OpenGL; falhar com diagnóstico se a versão não estiver disponível.
4. Tratar resize por uma única fronteira do renderer.
5. Garantir apenas uma apresentação por frame.
6. Adicionar diagnósticos de vendor/renderer/version/profile/backend e evidência persistente.
7. Disponibilizar caminhos reproduzíveis para preparar/compilar o backend Diligent Win32 e validar o runtime GPU.
8. Manter BMD, terrain, effects e UI no legado até o bootstrap ser validado em Windows/GPU.

## Estado implementado no branch

A fronteira de bootstrap existe em:

- `SRCMainGS/Source/Main5.2/source/ModernGraphicsBootstrap.h`
- `SRCMainGS/Source/Main5.2/source/ModernGraphicsBootstrap.inl`
- compilação temporária através de `CShaderGL.cpp`, que já pertence ao `Main.vcxproj`.

O código define os backends `Legacy`, `OpenGL46`, `Vulkan` e `Direct3D11`, porém somente OpenGL 4.6 possui caminho de inicialização nesta fase.

O caminho OpenGL usa `IEngineFactoryOpenGL::AttachToActiveGLContext`. A Main continua dona de `HWND`, `HDC`, `HGLRC` e `SwapBuffers`; Diligent é anexado ao contexto existente e não cria swap chain durante a coexistência.

## Carregamento do backend

A Main não faz link estático do engine OpenGL do Diligent.

Com `ENGINE_DLL=1`, o bootstrap usa o loader Win32 oficial exposto por Diligent:

- `LoadGraphicsEngineOpenGL()` carrega o módulo do backend;
- o módulo exporta `GetEngineFactoryOpenGL`;
- Release usa `GraphicsEngineOpenGL_32r.dll`;
- Debug da Main é alinhado com `DILIGENT_DEBUG` e usa `GraphicsEngineOpenGL_32d.dll`.

Se o módulo não puder ser carregado ou a factory retornar nula, a inicialização moderna falha de forma controlada e o renderer legado permanece ativo.

## Preparação reproduzível da dependência

`SRCMainGS/Source/Main5.2/setup_diligent_opengl46.ps1` fixa e verifica:

- tag DiligentCore: `v2.5.6`;
- commit: `b036337d68be2353c9950a85929acf796b9a6d50`.

O script prepara os submodules, configura um build Win32 OpenGL-only com HLSL habilitado, compila os backends Release/Debug e copia as DLLs para `Client_2`. Também pode compilar a Main com `-BuildMain`.

O checkout e os outputs gerados ficam em `dependencies/DiligentCore/` e `dependencies/_diligent_build/`, ambos ignorados pelo `.gitignore` da `Main5.2`.

`ModernGraphicsBootstrap.h` ativa `MU_ENABLE_DILIGENT` automaticamente no MSVC/Win32 quando detecta o header OpenGL no layout preparado pelo script. Sem a dependência, o código legado continua sendo o caminho padrão.

## Lifecycle de coexistência

O lifecycle real foi mapeado e conectado por uma ponte temporária `WH_CALLWNDPROC` no mesmo thread da UI quando `MU_ENABLE_DILIGENT` está habilitado. A ponte:

- detecta o contexto WGL real depois que ele está current;
- chama `InitializeOpenGL46(...)` uma única vez por `HWND/HGLRC`;
- encaminha `WM_SIZE` para `OnResize(...)`;
- chama `Shutdown()` antes de `WM_CLOSE/WM_DESTROY/WM_NCDESTROY` chegar ao caminho que executa `KillGLWindow()`;
- não substitui `WndProc`, não cria outro contexto e não apresenta frames.

A ponte continua deliberadamente temporária para a Fase 2. Depois que o gate Win32/GPU provar a integração, as chamadas podem ser movidas diretamente para os owners reais (`CreateOpenglWindow`, `CWINHANDLE::WndProc` e shutdown) sem alterar o contrato público de `CModernGraphicsBootstrap`.

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

`CreateOpenglWindow()` ainda usa `wglCreateContext()` e não solicita explicitamente uma versão 4.6. O bootstrap valida a versão **efetivamente fornecida pelo driver**. Se ela for menor que 4.6, a modernização permanece inativa e o legado continua ativo.

Uma futura troca para criação explícita de contexto 4.6 compatibility deve acontecer antes da criação dos assets OpenGL e somente depois de provar que não quebra os caminhos legados.

## Diagnósticos implementados

Na tentativa de attach são registrados:

- `GL_VENDOR`;
- `GL_RENDERER`;
- `GL_VERSION`;
- `GL_SHADING_LANGUAGE_VERSION`;
- major/minor interpretados;
- `core` / `compatibility` profile quando disponível;
- falha ao carregar DLL/factory Diligent;
- sucesso/falha do attach Diligent;
- resize observado;
- shutdown antes do teardown WGL;
- confirmação de que o `SwapBuffers` legado permanece autoritativo.

Os registros são enviados ao debugger e persistidos em `Client_2/ModernGraphics.log` quando o cliente é executado com `Client_2` como working directory. O arquivo é ignorado pelo Git.

Um callback OpenGL adicional é opt-in por `MU_MODERN_GL_DEBUG=1`. Quando GL 4.3/KHR_debug estiver disponível, mensagens do driver também são persistidas no mesmo log.

O `CErrorReport::WriteOpenGLInfo()` existente continua sendo mantido; o log moderno não substitui o logger legado.

## Validação de build comprovada

O workflow `.github/workflows/phase2-win32-build.yml` executa o setup em Windows/Visual Studio e valida os outputs reais.

### Release/x86

Run `34533022717`, commit `b641263293ca1eebf5ed28e96cbfbd2643f1269b`:

- checkout exato do Diligent e submodules: aprovado;
- configuração OpenGL-only Win32/HLSL: aprovada;
- `_32r.dll` e `_32d.dll`: compiladas;
- `Main.sln` Release/x86: compilada com C++17;
- `Client_2/Main.exe`: linkado;
- resultado: **0 erros**.

### Debug/x86

Run `34533868904`, commit `51a21347b0f55a98957f85e53a6a5fde6bf7b2a4`:

- setup Diligent aprovado;
- Main Debug/x86 compilada com a normalização C++17 limitada ao projeto Main;
- `Client_2/Main.exe`: linkado;
- resultado: **0 erros**.

A falha Debug anterior era causada pelo C++14 do projeto legado em conflito com os headers sol2 já dependentes de C++17, e não pela integração Diligent.

### Gate combinado permanente

O workflow atual compila Release e Debug sequencialmente no mesmo job. O gate mais recente é `34535389139`, disparado pelas alterações de logging/runtime evidence. Ele deve terminar com sucesso antes de o item combinado ser marcado como concluído.

## Gate GPU reproduzível

Foi adicionado:

`SRCMainGS/Source/Main5.2/run_phase2_runtime_test.ps1`

Execução recomendada a partir de `SRCMainGS/Source/Main5.2`:

```powershell
powershell -ExecutionPolicy Bypass -File .\run_phase2_runtime_test.ps1 -EnableGLDebug -RequireResize
```

O script:

- verifica `Main.exe`, `_32r.dll` e `_32d.dll`;
- remove evidência antiga;
- inicia `Main.exe` com `Client_2` como working directory;
- opcionalmente ativa `MU_MODERN_GL_DEBUG=1`;
- espera o fechamento normal do cliente;
- valida attach, diagnóstico OpenGL, resize opcional, shutdown e ausência de fallback moderno no `ModernGraphics.log`.

Também aceita `-ValidateOnly` para validar um log já capturado sem relançar o cliente.

## Estado de conclusão

### Concluído no repositório/build

- pin do Diligent e shaders/resources;
- ownership e single-present definidos;
- mapeamento real do lifecycle da Main;
- bridge de attach/resize/shutdown;
- validação de versão OpenGL 4.6;
- loader modular do backend OpenGL;
- setup reproduzível Win32/OpenGL/HLSL;
- fallback legado quando dependência/DLL/capability não estiver disponível;
- builds independentes Release/x86 e Debug/x86 aprovados;
- diagnósticos persistentes e callback OpenGL opt-in implementados;
- gate GPU local transformado em procedimento/script reproduzível.

### Ainda não validado em runtime GPU

A Fase 2 só será considerada **runtime-concluída** após execução real da Main comprovando:

- criação/anexação do backend OpenGL;
- versão efetiva >= 4.6;
- resize sem crash;
- exatamente uma apresentação por frame;
- shutdown sem erro de lifetime/contexto;
- logs reais de vendor, renderer, version, profile e backend;
- nenhuma regressão no caminho legado mantido durante o protótipo.

O build CI não substitui esses testes porque não executa o cliente no ambiente gráfico interativo alvo.

## Próxima ação

1. Confirmar o gate combinado `34535389139` contra a source atual.
2. Executar `run_phase2_runtime_test.ps1 -EnableGLDebug -RequireResize` no Windows/GPU alvo.
3. Somente após esse gate runtime entram como trabalho ativo os layouts CPU/GPU concretos, constant buffers, pose conversion, Skeleton Texture e o primeiro BMD moderno de duas instâncias.