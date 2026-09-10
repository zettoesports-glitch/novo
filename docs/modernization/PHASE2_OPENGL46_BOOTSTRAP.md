# Fase 2 — Bootstrap Diligent / OpenGL 4.6

Data: 10/09/2026.
Branch: `modernization`.

## Objetivo

Criar a primeira fronteira de runtime para o renderer moderno sem substituir ainda o desenho BMD legado. A fase prova ownership de janela/contexto/present, exige OpenGL 4.6 **compatibility profile** e prepara os contratos usados por Vulkan e Direct3D 11 futuramente.

## Entregas desta fase

1. Fixar a revisão de Diligent e a revisão dos shaders de referência.
2. Definir ownership explícito de `HWND`, dispositivo, contexto imediato e apresentação.
3. Exigir OpenGL >= 4.6 compatibility profile no backend OpenGL; falhar com diagnóstico e preservar o legado se a capability/profile não estiver disponível.
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

Antes do attach, `InitializeOpenGL46()` exige:

- handles Win32/WGL válidos;
- o `HDC/HGLRC` legado corrente no thread;
- versão efetiva OpenGL >= 4.6;
- `GL_CONTEXT_COMPATIBILITY_PROFILE_BIT` ativo.

Um contexto core-only é rejeitado explicitamente e o renderer legado permanece ativo. Esse hardening foi adicionado no commit `e6c0a678ecbb870262d62ed362902ce999adb06c` e passou o gate Windows/x86 `34542334616`.

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
- chama `Shutdown()` antes dos caminhos `WM_CLOSE`, `WM_DESTROY`, `WM_NCDESTROY` e `WM_USER_MEMORYHACK` capazes de alcançar teardown pós-attach;
- não substitui `WndProc`, não cria outro contexto e não apresenta frames.

A auditoria também verificou os `KillGLWindow()` existentes dentro de `CreateOpenglWindow()`. Eles são caminhos de limpeza de falha durante a criação do contexto, antes de um attach moderno bem-sucedido, portanto não representam um teardown moderno descoberto.

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
- A auditoria atual localizou o `SwapBuffers(hDC)` legado em `ZzzScene.cpp`; nenhum código da Fase 2 adiciona outro present.
- O renderer moderno não destrói nem substitui `g_hDC/g_hRC`.
- Ownership Diligent do contexto é `Attached`, nunca `Owned`, quando a inicialização é bem-sucedida.
- Ao alternar de comandos raw OpenGL para comandos Diligent, `BeginModernPass()` invalida o cache de estado Diligent via `IDeviceContext::InvalidateState()`.
- OpenGL core-only não é aceitável para coexistência com os caminhos fixed-function/client-array atuais; o bootstrap exige compatibility profile antes do attach.
- OpenGL 4.6 é o único backend moderno ativo nesta fase. Vulkan e Direct3D 11 são extension points.

## Contexto atual da Main

`CreateOpenglWindow()` ainda usa `wglCreateContext()` e não solicita explicitamente uma versão 4.6. O bootstrap valida a versão e o profile **efetivamente fornecidos pelo driver**. Se a versão for menor que 4.6 ou o contexto não for compatibility profile, a modernização permanece inativa e o legado continua ativo.

Uma futura troca para criação explícita de contexto 4.6 compatibility deve acontecer antes da criação dos assets OpenGL e somente depois de provar que não quebra os caminhos legados.

## Diagnósticos implementados

Na tentativa de attach são registrados:

- `GL_VENDOR`;
- `GL_RENDERER`;
- `GL_VERSION`;
- `GL_SHADING_LANGUAGE_VERSION`;
- major/minor interpretados;
- `core` / `compatibility` profile quando disponível;
- falha de capability/profile;
- falha ao carregar DLL/factory Diligent;
- mensagens da camada de validação Diligent;
- mensagens OpenGL/KHR_debug produzidas pelo callback que o próprio backend Diligent instala quando validation está habilitada;
- sucesso/falha do attach Diligent;
- resize observado;
- shutdown antes do teardown WGL;
- confirmação de que o `SwapBuffers` legado permanece autoritativo.

Os registros da Main são enviados ao debugger e persistidos em `ModernGraphics.log` ao lado de `Main.exe`. O caminho é resolvido pela localização do executável, então launcher ou working directory alternativo não desloca a evidência para fora de `Client_2`.

`MU_MODERN_GL_DEBUG=1` define `EngineGLCreateInfo.EnableValidation=true` e instala `ModernDiligentMessageCallback` através de `IEngineFactory::SetMessageCallback()`. A Main **não instala um segundo `glDebugMessageCallback`**: o backend OpenGL do Diligent v2.5.6 continua dono do KHR_debug e encaminha suas mensagens pela callback oficial da factory. Isso evita conflito de ownership entre Main e Diligent.

O `CErrorReport::WriteOpenGLInfo()` existente continua sendo mantido; o log moderno não substitui o logger legado.

## Validação de build comprovada

O workflow `.github/workflows/phase2-win32-build.yml` executa o setup em Windows/Visual Studio e valida os outputs reais.

Provas acumuladas:

- Release/x86 run `34533022717`: pinned Diligent preparado, `_32r.dll`/`_32d.dll` produzidas e Main Release/x86 linkada com 0 erros.
- Debug/x86 run `34533868904`: Main Debug/x86 linkada com 0 erros após a normalização C++17 limitada ao projeto Main.
- Combined baseline run `34538658227`: Release + Debug + outputs + parser de evidência sintética aprovados.
- Lifecycle/runtime-gate audit run `34540959623`: aprovado após `WM_USER_MEMORYHACK` e hardening do gate de runtime.
- **Latest source-affecting gate `34542334616`**, commit `e6c0a678ecbb870262d62ed362902ce999adb06c`: Diligent pinned setup, Release/x86, Debug/x86, verificações de outputs e `run_phase2_runtime_test.ps1 -ValidateOnly` com profile compatibility foram concluídos com **success**.

Esse último gate prova que a exigência de compatibility profile compila nas duas configurações reais da Main e não quebrou o setup Diligent nem o parser do gate de runtime.

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
- opcionalmente ativa `MU_MODERN_GL_DEBUG=1`, que liga validation/KHR_debug pelo próprio Diligent;
- espera o fechamento normal do cliente e exige exit code 0;
- valida attach, diagnóstico OpenGL, `profile=compatibility`, roteamento de validation/debug, resize opcional, shutdown e ausência de fallback moderno no `ModernGraphics.log` ao lado do executável.

Também aceita `-ValidateOnly` para validar um log já capturado sem relançar o cliente e sem exigir os binários de runtime. O workflow Windows executa esse modo contra evidência sintética depois dos builds para testar o script sem fingir que isso equivale a uma execução GPU real.

## Estado de conclusão

### Concluído no repositório/build

- pin do Diligent e shaders/resources;
- ownership e single-present definidos;
- mapeamento real do lifecycle da Main;
- bridge de attach/resize/shutdown;
- validação OpenGL >= 4.6 + compatibility profile;
- loader modular do backend OpenGL;
- setup reproduzível Win32/OpenGL/HLSL;
- fallback legado quando dependência, DLL, versão ou profile não estiver disponível;
- builds Release/x86 e Debug/x86 aprovados no latest source gate `34542334616`;
- diagnósticos persistentes implementados;
- validation e OpenGL/KHR_debug roteados pelo callback oficial do Diligent, sem callback GL concorrente na Main;
- gate GPU local transformado em procedimento/script reproduzível;
- validação sintática/funcional do modo `-ValidateOnly` integrada e aprovada no workflow permanente;
- auditoria dos caminhos conhecidos de `KillGLWindow()` concluída sem teardown pós-attach descoberto fora da cobertura da ponte.

### Ainda não validado em runtime GPU

A Fase 2 só será considerada **runtime-concluída** após execução real da Main comprovando:

- criação/anexação do backend OpenGL;
- versão efetiva >= 4.6 e compatibility profile;
- resize sem crash;
- exatamente uma apresentação por frame;
- shutdown sem erro de lifetime/contexto;
- logs reais de vendor, renderer, version, profile e backend;
- validation/KHR_debug sem conflito de callback;
- nenhuma regressão no caminho legado mantido durante o protótipo.

O build CI não substitui esses testes porque não executa o cliente no ambiente gráfico interativo alvo.

## Próxima ação

1. Executar `run_phase2_runtime_test.ps1 -EnableGLDebug -RequireResize` no Windows/GPU alvo e guardar `Client_2/ModernGraphics.log` como evidência.
2. Se o gate GPU passar, reavaliar/substituir a ponte temporária `WH_CALLWNDPROC` por chamadas diretas nos owners de lifecycle já mapeados.
3. Somente após esse gate runtime entram como trabalho ativo os layouts CPU/GPU concretos, constant buffers, pose conversion, Skeleton Texture e o primeiro BMD moderno de duas instâncias.