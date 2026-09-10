# NextMU Resource Mapping — OpenGL 4.6

Este documento consolida o mapeamento técnico do pacote real `Resources_DE_2024-01-25` e da engenharia reversa do NextMU, com foco na modernização do renderer da source do projeto.

## Regra de evidência

- **CONFIRMADO — Resource real**: observado diretamente em arquivos do pacote `Resources_DE_2024-01-25`.
- **CONFIRMADO — Reversa**: observado na análise estática/reconstrução dos executáveis e módulos.
- **INFERIDO**: hipótese arquitetural consistente, mas ainda não validada em todos os caminhos.

---

## 1. Backends gráficos

**CONFIRMADO — Resource real**

O pacote inclui, em x86 e x64, módulos separados para:

- Direct3D 11
- Direct3D 12
- OpenGL
- Vulkan

Arquivos x86 relevantes:

- `Resources_DE_2024-01-25/x86/NextMU.exe`
- `Resources_DE_2024-01-25/x86/GraphicsEngineD3D11_32r.dll`
- `Resources_DE_2024-01-25/x86/GraphicsEngineD3D12_32r.dll`
- `Resources_DE_2024-01-25/x86/GraphicsEngineOpenGL_32r.dll`
- `Resources_DE_2024-01-25/x86/GraphicsEngineVk_32r.dll`

Arquivos x64 equivalentes também existem.

**CONFIRMADO — Reversa**

A seleção de backend é feita através da Diligent Engine. A ordem observada na reversa é:

```text
Direct3D11 -> Vulkan -> Direct3D12 -> OpenGL
```

Para a modernização atual, o backend prioritário é **OpenGL 4.6**, mas a arquitetura deve manter separação suficiente para permitir Vulkan e Direct3D posteriormente.

---

## 2. Organização do pipeline de shaders

**CONFIRMADO — Resource real**

A árvore `Resources_DE_2024-01-25/data/shaders` está separada por categoria de render:

```text
bbox/
joint/
models/
noesisgui/
particle/
rmlui/
worlds/
```

Isso confirma que o NextMU não usa um shader genérico único para toda a cena.

### Models

`data/shaders/models/` contém variantes:

```text
blend
chrome
chrome2
chrome3
chrome4
chrome5
chrome6
chrome7
chrome8
metal
oil
texture
```

Cada família pode conter variantes adicionais:

```text
shader.vs
shader.fs
shader_shadowmap.vs
shader_wave.vs
shader_ztest.vs
shader_ztest.fs
shadow.fs
shadow_ztest.fs
```

Conclusão arquitetural: tratar isso como **shader permutations / render policies** dentro de um único subsistema de materiais/modelos, e não como renderers totalmente independentes.

---

## 3. Shader permutation table declarativa

**CONFIRMADO — Resource real**

`Resources_DE_2024-01-25/data/resources.json` funciona como catálogo declarativo de shaders.

Exemplos de IDs:

```text
mesh_normal
mesh_normal_shadowmap
mesh_normal_wave
mesh_normal_ztest
mesh_blend
mesh_blend_shadowmap
mesh_blend_wave
mesh_blend_ztest
mesh_chrome
mesh_chrome2
...
mesh_chrome8
mesh_metal
mesh_oil
```

Cada entrada mapeia:

```text
shader id -> vertex shader -> fragment shader -> resource_id
```

Algumas entradas também aplicam macros como:

```text
SHADOW_OUTPUT=1
```

### Implicação para a source modernizada

Criar um `ShaderRegistry`/`PipelineRegistry` equivalente, com IDs estáveis e cache de permutations.

O gameplay não deve selecionar programas OpenGL diretamente; deve selecionar uma política/material que resolve para uma permutation.

---

## 4. Vertex layout de modelos

**CONFIRMADO — Resource real**

Os shaders de modelos usam:

```text
ATTRIB0 -> Position : float3
ATTRIB1 -> Normal   : float3
ATTRIB2 -> UV       : float2
ATTRIB3 -> Bone     : uint2
ATTRIB4 -> Vertex   : uint
```

O campo `Bone` possui dois índices:

- `Bone.x`: bone usado para transformar a posição
- `Bone.y`: bone usado para transformar a normal

Isso deve ser preservado na conversão BMD -> GPU mesh.

---

## 5. GPU skinning por Skeleton Texture

**CONFIRMADO — Resource real**

Os vertex shaders de modelos declaram:

```text
Texture2D<float4> g_SkeletonTexture;
```

O shader calcula o offset por personagem através de:

```text
g_BoneOffset
```

A indexação mostra que cada bone ocupa **2 texels**:

```text
boneOffset += boneId;
boneOffset *= 2;
```

### Texel 0 — rotação

A rotação é lida como quaternion:

```text
RequestBoneRotation(...)
```

A ordem lida é rearranjada com `.yzwx`.

### Texel 1 — posição + escala

```text
RequestBonePositionAndScale(...)
```

Formato lógico:

```text
xyz = position
w   = scale
```

### Transformação

O shader aplica quaternion diretamente:

```text
2 * cross(q.xyz, cross(q.xyz, v) + q.w * v) + v
```

A posição ainda recebe tradução; a normal não recebe.

### Arquitetura recomendada

Implementar no renderer moderno:

```text
SkeletonAllocator
SkeletonTexture
BoneOffset por instância
Upload de bones em lote
GPU skinning no vertex shader
```

Isso substitui progressivamente o skinning completo em CPU do renderer legado.

---

## 6. Constant buffers / dados por draw

**CONFIRMADO — Resource real**

Os modelos usam blocos equivalentes a:

### Camera

```text
cbCameraAttribs
```

Com posição, viewport, near/far, matrizes view/projection e inversas.

### Model/ViewProj

```text
ModelViewProj
    g_Model
    g_ViewProj
```

### ModelSettings

```text
g_LightPosition
g_BodyLight
g_BodyOrigin
g_BoneOffset
g_NormalScale
g_EnableLight
g_AlphaTest
g_PremultiplyAlpha
g_WorldTime
g_ZTestRef
g_BlendTexCoord
```

### Implicação

Na source modernizada, separar dados em estruturas claras:

```text
FrameData / CameraData
LightData
ObjectData
MaterialData
SkeletonData
```

No OpenGL 4.6 isso deve ser refletido em UBOs/SSBOs/texture resources conforme o uso.

---

## 7. Iluminação de modelo

**CONFIRMADO — Resource real**

O modelo normal calcula luminosidade no vertex shader usando:

```text
dot(normal, lightPosition) * 0.8 + 0.4
```

com mínimo de 0.2 e controle por `g_EnableLight`.

`g_BodyLight` modula a cor final.

A normal pode sofrer deslocamento geométrico via:

```text
position += normal * g_NormalScale
```

---

## 8. Fragment shader base

**CONFIRMADO — Resource real**

O fragment shader base de modelos executa, em essência:

```text
texture sample * vertex color
premultiply-alpha opcional
alpha test / discard
shadow lighting opcional
sRGB -> linear
```

Existe lógica explícita para:

- alpha test
- premultiplied alpha
- iluminação por sombra
- conversão sRGB/linear

Isso precisa ser mantido para evitar diferenças de brilho/tonalidade entre renderer legado e renderer moderno.

---

## 9. Sombras cascaded

**CONFIRMADO — Resource real**

Há suporte a até 8 cascatas:

```text
MAX_CASCADES 8
```

Modos presentes:

```text
PCF
VSM
EVSM2
EVSM4
```

A infraestrutura inclui:

```text
CascadeAttribs
ShadowMapAttribs
LightAttribs
```

Também há suporte a:

- cascade transition
- fixed depth bias
- receiver plane depth bias
- filter size
- anisotropy
- visualização/debug de cascatas
- filtragem entre cascatas

Há caminhos específicos para GLSL/OpenGL nas rotinas de sample da shadow map.

### Ordem de implementação recomendada

1. renderer de modelos sem shadow
2. shadow depth pass
3. cascaded shadow map PCF
4. filtros avançados depois

---

## 10. Material / render policy — texture

**CONFIRMADO — Resource real**

`models/texture/shader.vs` usa UV original do mesh:

```text
UV = VSIn.UV + g_BlendTexCoord
```

É o caminho base para modelos texturizados.

---

## 11. Material / render policy — blend

**CONFIRMADO — Resource real**

`models/blend/shader.vs` não calcula a mesma luminosidade do modelo normal.

A cor é passada como:

```text
PSIn.Color = g_BodyLight
```

Isso confirma um caminho mais apropriado para efeitos blend/aditivos/translúcidos.

`blend` também possui permutations próprias para `shadowmap`, `wave` e `ztest`.

---

## 12. Material / render policy — metal

**CONFIRMADO — Resource real**

`models/metal/shader.vs` gera UV através da normal animada:

```text
UV = normal.zy * (0.5, 0.5) + (0.2, 0.5)
```

Então `metal` não é somente troca de textura: há geração procedural de coordenadas no vertex shader.

---

## 13. Material / render policy — oil

**CONFIRMADO — Resource real**

`models/oil/shader.vs` usa uma textura adicional:

```text
g_VertexTexture
```

Ela é amostrada no vertex shader para derivar um valor de alpha.

Fluxo:

```text
pixel = pow(sample.x, 2)
alpha = 1 - pixel
BodyLight *= alpha
```

Isso significa que o pipeline de material precisa permitir recursos adicionais além da textura principal.

---

## 14. Chrome 1..8

**CONFIRMADO — Resource real**

As variantes `chrome`, `chrome2`, `chrome3`, etc. usam fórmulas diferentes de UV/reflexo baseadas principalmente em:

```text
normal animada
g_WorldTime
vetores fixos ou temporais
```

Exemplos observados:

- `chrome`: normal + deslocamento temporal simples
- `chrome2`: mistura dos componentes da normal + tempo
- `chrome3`: dot da normal com vetor fixo
- `chrome4`: vetor temporal usando sin/cos
- `chrome5`: variação mais agressiva da mesma ideia
- `chrome6`: combinação distinta de componentes da normal

### Arquitetura recomendada

Representar como:

```text
ChromeMode::Chrome1
...
ChromeMode::Chrome8
```

ou como IDs de shader permutation.

Não criar oito renderers separados.

---

## 15. Terreno / mundo

**CONFIRMADO — Resource real**

O terrain renderer é arquiteturalmente diferente do model renderer.

`worlds/normal/shader.vs` recebe posições discretas e reconstrói dados do mundo a partir de texturas GPU:

```text
g_AttributesTexture
g_MappingTexture
g_HeightTexture
g_UVTexture
g_NormalTexture
g_LightTexture
```

A altura vem de `g_HeightTexture`.

A normal vem de `g_NormalTexture`.

A iluminação do terreno vem de `g_LightTexture`.

A seleção/mistura de materiais usa `g_MappingTexture` e `g_UVTexture`.

Água/vento alteram UVs através de:

```text
WaterMove
WindScale
WindSpeed
```

### Arquitetura recomendada

Manter:

```text
ModelRenderer
TerrainRenderer
ParticleRenderer
JointRenderer
UIRenderer
```

sob uma API gráfica comum.

---

## 16. Partículas

**CONFIRMADO — Resource real**

O shader de partículas usa layout simples:

```text
Position
Color
UV
```

O vertex shader aplica projeção e repassa cor/UV.

Partículas não usam o mesmo caminho de skeleton/model mesh.

---

## 17. Attachments e composição de personagem

**CONFIRMADO — Resource real**

`renders.json` define attachments vinculados a bones, por exemplo:

```text
weapon_right
weapon_left
wings
```

com transform local:

```text
position
angle
scale
```

`characters.json` separa:

- classe/subclasse
- sexo
- texturas de skin low/high quality
- partes do corpo
- attachments

### Implicação

A modernização do renderer não deve quebrar a lógica atual de attachment. O renderer deve expor bone transforms/attachment points para itens, armas e asas.

---

## 18. Seleção de animação

**CONFIRMADO — Resource real**

`animations.json` mostra uma árvore de decisão independente da API gráfica.

Condições observadas:

```text
safezone
action
sex
class
mount
weapons
right_weapon
```

### Implicação

Separar claramente:

```text
Animation State Resolver
Animation Evaluation
Skeleton Upload
Rendering
```

A escolha de animação não deve depender de OpenGL/Vulkan/D3D.

---

## 19. Arquitetura alvo para a source

```text
Gameplay / MU Legacy Structures
        |
        v
Animation + Model Adaptation Layer
        |
        +--> SkeletonAllocator / SkeletonTexture
        +--> GPU Mesh Cache
        +--> Material Resolver
        |
        v
Render API / RHI
        |
        +--> OpenGL 4.6   [primeiro]
        +--> Vulkan       [estrutura pronta]
        +--> Direct3D 11  [estrutura pronta]
```

Renderers especializados:

```text
ModelRenderer
TerrainRenderer
ParticleRenderer
JointRenderer
UIRenderer
ShadowRenderer
```

Serviços compartilhados:

```text
ShaderRegistry
PipelineCache
TextureManager
BufferManager
SkeletonAllocator
FrameData
RenderStateCache
```

---

## 20. Ordem sugerida de implementação

1. Criar Render API/RHI sem remover o legado.
2. Inicializar contexto OpenGL 4.6.
3. Criar ShaderRegistry e pipeline cache.
4. Criar GPU mesh compatível com o layout do NextMU.
5. Implementar SkeletonTexture + BoneOffset.
6. Renderizar Hero com `mesh_normal`.
7. Migrar materials `blend`, `metal`, `oil`, `chrome1..8`.
8. Migrar player remoto/bots/NPC/monsters progressivamente.
9. Implementar TerrainRenderer moderno.
10. Implementar ParticleRenderer e JointRenderer.
11. Implementar shadow pass e cascaded shadows.
12. Remover dependências do renderer legado somente após paridade visual/funcional.
13. Manter API pronta para Vulkan/D3D11.

---

## 21. Pontos ainda pendentes de confirmação

Ainda precisam ser fechados por evidência direta do executável/DLL ou uso real em runtime:

- tamanho exato e política de alocação da Skeleton Texture
- constante real de `SKELETON_TEXTURE_WIDTH`
- formato físico da texture de skeleton no backend OpenGL
- política de atualização/upload por frame
- lifecycle/cache de Pipeline State Objects da Diligent
- resource binding exato de cada permutation
- todos os modos `chrome7` e `chrome8` em detalhe
- geração/uso de `g_VertexTexture` no caminho `oil`
- estrutura completa do TerrainRenderer e batching
- batching/instancing de modelos
- shadow setup efetivamente ativado pelo jogo em runtime
- quais features presentes nas DLLs da Diligent são realmente usadas pelo NextMU

Esses itens devem permanecer marcados como pendentes até confirmação direta.

---

## Estado atual

O mapeamento estrutural já é suficiente para iniciar a implementação do renderer OpenGL 4.6 com risco muito menor. A prioridade agora é continuar extraindo os contratos de recursos e depois cruzá-los com `NextMU.exe` / `GraphicsEngineOpenGL_32r.dll` e com a source atual do projeto.