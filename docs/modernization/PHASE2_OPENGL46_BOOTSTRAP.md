# Fase 2 — Bootstrap Diligent / OpenGL 4.6

Data-base: 10/09/2026  
Branch: `modernization`

> Fonte de verdade complementar: [PHASE2_COMPLETENESS_AUDIT.md](PHASE2_COMPLETENESS_AUDIT.md) e [STATUS.md](STATUS.md). A auditoria prevalece sobre anotações históricas anteriores.

## Objetivo

Criar a primeira fronteira de runtime do renderer moderno sem migrar ainda BMD, terrain, effects ou UI. A Main continua funcional pelo renderer legado enquanto Diligent é anexado ao contexto WGL existente.

OpenGL 4.6 é o primeiro backend ativo. Vulkan e Direct3D 11 permanecem extension points da mesma arquitetura Diligent + shared HLSL.

## Ownership

Durante a Fase 2:

- a Main continua dona de `HWND`, `HDC` e `HGLRC`;
- a Main continua dona de toda apresentação no framebuffer da janela;
- Diligent usa `IEngineFactoryOpenGL::AttachToActiveGLContext`;
- Diligent **não** cria um segundo WGL context;
- Diligent **não** cria swap chain para a janela;
- Diligent **não** chama um caminho moderno de `Present`/`SwapBuffers`.

A auditoria de `ZzzScene.cpp` encontrou dois call sites legados de `SwapBuffers(hDC)`: um no loading scene e outro no fluxo normal da cena. Isso continua sendo um único **owner legado de apresentação**, não uma segunda apresentação moderna.

O parser atual de `ModernGraphics.log` **não conta chamadas `SwapBuffers` por frame**. Portanto o gate não deve alegar que essa contagem foi medida; o que está provado estaticamente é que a Fase 2 não adiciona outro owner/present path.

## Bootstrap implementado

Arquivos principais:

- `SRCMainGS/Source/Main5.2/source/ModernGraphicsBootstrap.h`
- `SRCMainGS/Source/Main5.2/source/ModernGraphicsBootstrap.inl`
- `SRCMainGS/Source/Main5.2/source/CShaderGL.cpp`

`CModernGraphicsBootstrap::InitializeOpenGL46(...)` aceita apenas o contexto que já está current no thread da Main e valida antes do attach:

- handles Win32/WGL válidos;
- `HDC/HGLRC` realmente current;
- versão efetiva OpenGL >= 4.6;
- `GL_CONTEXT_COMPATIBILITY_PROFILE_BIT` ativo.

Core-only é rejeitado porque a coexistência ainda depende de fixed-function/client-array legacy paths.

## Fallback legado seguro

A modernização não pode quebrar hardware/contextos que não atendem ao backend moderno.

O bootstrap auditado:

- lê `GL_VERSION` primeiro;
- só consulta `GL_MAJOR_VERSION` / `GL_MINOR_VERSION` quando o contexto já foi identificado como OpenGL 3.x+;
- só consulta `GL_SHADING_LANGUAGE_VERSION` em OpenGL 2.0+;
- rejeita OpenGL < 4.6;
- rejeita core-only;
- preserva o legado se o backend DLL não carregar;
- preserva o legado se a factory Diligent não existir;
- preserva o legado se `AttachToActiveGLContext` falhar;
- não escreve um marcador de shutdown moderno bem-sucedido quando nunca houve attach ativo.

Isso evita que o próprio diagnóstico moderno injete enums inválidos no estado OpenGL do fallback legado.

## Backend Diligent

A dependência é reproduzível e pinada:

- DiligentCore tag: `v2.5.6`
- commit: `b036337d68be2353c9950a85929acf796b9a6d50`

O setup está em:

`SRCMainGS/Source/Main5.2/setup_diligent_opengl46.ps1`

Ele prepara Win32/OpenGL-only com HLSL habilitado, compila os módulos:

- `GraphicsEngineOpenGL_32r.dll`
- `GraphicsEngineOpenGL_32d.dll`

E os copia para `Client_2`.

Main Release usa `_32r`; Main Debug é alinhada com `DILIGENT_DEBUG` para selecionar `_32d`.

## Lifecycle de coexistência

A integração atual usa deliberadamente uma ponte temporária `WH_CALLWNDPROC` no thread da UI. Ela existe para validar coexistência sem reescrever ainda os grandes owners legados.

A ponte:

- detecta o contexto WGL já current após sua criação;
- tenta bootstrap uma vez por geração de contexto;
- encaminha resize apenas quando existe attach moderno ativo;
- cobre `WM_CLOSE`, `WM_DESTROY`, `WM_NCDESTROY` e `WM_USER_MEMORYHACK` nos caminhos conhecidos de teardown;
- libera Diligent antes da destruição WGL quando houve attach real;
- limpa também estado de tentativa falha quando a janela rastreada entra em teardown;
- bloqueia reattach ao contexto moribundo;
- libera a barreira após observar a liberação real do WGL ou um novo par live;
- tolera reciclagem numérica de `HWND/HGLRC` pelo Windows depois que a geração anterior foi realmente liberada.

Os `KillGLWindow()` dentro das falhas de `CreateOpenglWindow()` foram auditados como cleanup pré-attach.

A ponte é temporária. Depois do gate GPU real, deve ser reavaliada em favor dos owners diretos já mapeados: attach após inicialização WGL, resize no lifecycle da janela e shutdown antes de `KillGLWindow()`.

## Coexistência de estado OpenGL

`BeginModernPass()` chama `IDeviceContext::InvalidateState()` antes de voltar de comandos raw OpenGL para comandos Diligent.

Nenhum draw moderno de produção é ativado nesta fase. Ao iniciar a próxima fase, o contrato de transição de estado nos dois sentidos deverá ser validado junto do primeiro draw moderno antes de ampliar cobertura.

## Diagnósticos

`Client_2/ModernGraphics.log` registra, conforme aplicável:

- início de tentativa de attach;
- vendor / renderer / OpenGL version;
- GLSL version quando suportada pelo contexto;
- versão interpretada;
- profile compatibility/core;
- razões de fallback;
- carregamento/factory Diligent;
- attach bem-sucedido;
- resize durante lifecycle ativo;
- barreira de teardown;
- shutdown moderno antes do WGL teardown;
- mensagens Diligent/OpenGL quando validation está habilitada.

`MU_MODERN_GL_DEBUG=1` habilita `EngineGLCreateInfo.EnableValidation` e registra mensagens através de `IEngineFactory::SetMessageCallback()`. A Main não instala um callback KHR_debug concorrente.

## Gate de runtime

Script:

`SRCMainGS/Source/Main5.2/run_phase2_runtime_test.ps1`

Execução recomendada:

```powershell
powershell -ExecutionPolicy Bypass -File .\run_phase2_runtime_test.ps1 -EnableGLDebug -RequireResize
```

O caminho real:

- exige `Main.exe` e os backends `_32r/_32d`;
- remove evidência antiga;
- inicia `Main.exe` em `Client_2`;
- exige fechamento normal com exit code 0;
- valida ciclos completos e ordenados;
- exige compatibility profile em cada lifecycle bem-sucedido;
- valida debug routing quando solicitado;
- valida resize somente dentro de lifecycle ativo quando solicitado;
- rejeita fallback moderno no log;
- aceita mais de um lifecycle quando há recriação WGL legítima;
- rejeita lifecycle incompleto, reordenado ou attach iniciado no meio de teardown.

`-ValidateOnly` existe para testar o parser com evidência já capturada/sintética. Ele **não** é certificação GPU.

## CI Windows/x86

`.github/workflows/phase2-win32-build.yml` valida:

- sintaxe dos scripts PowerShell;
- checkout pinado do Diligent;
- build dos backends OpenGL Win32 Release/Debug;
- Main Release/x86;
- Main Debug/x86;
- outputs esperados;
- parser com casos positivos e negativos de lifecycle, incluindo recriação e profile incompatível.

O workflow mais recente aprovado deve ser consultado em [STATUS.md](STATUS.md), evitando duplicar um número de run que fica obsoleto a cada hardening da source.

## Estado da Fase 2

### Fechado no repositório/build

- ownership e fronteira WGL/Diligent;
- OpenGL >= 4.6 compatibility requirement;
- loader modular Diligent;
- fallback legado;
- dependency pin/setup;
- Release/Debug x86 gate;
- diagnostics e validation routing;
- lifecycle parser;
- teardown/recreation hardening;
- documentação de evidência.

### Ainda exige ambiente GPU real

A Fase 2 **não é runtime-certified** até uma execução interativa provar no driver alvo:

- DLL/factory real;
- `AttachToActiveGLContext` real;
- OpenGL >= 4.6 compatibility profile real;
- validation/debug routing real;
- resize;
- eventual recriação de modo/contexto;
- teardown/shutdown limpo;
- ausência de regressão visual no legado.

Hosted CI não substitui esse teste WGL/GPU.

## Próximo passo após o gate GPU

1. Reavaliar/substituir a ponte `WH_CALLWNDPROC` por lifecycle direto se a evidência permitir.
2. Definir layouts CPU/GPU e constant-buffer contracts concretos.
3. Criar buffers/textures/pipelines Diligent.
4. Ativar shared-HLSL model pipeline.
5. Implementar pose conversion / Skeleton Texture.
6. Provar duas instâncias independentes do mesmo BMD antes de expandir Hero/remote/Bot/NPC/monster.
