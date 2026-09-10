# NextMU — Shader Pipeline Details

Complemento do `OPENGL46_RESOURCE_MAPPING.md` com achados confirmados diretamente no pacote real `Resources_DE_2024-01-25`.

## 1. Configuração real de shadows

Arquivo: `Resources_DE_2024-01-25/config/game.json`

Configuração do pacote:

```json
{
  "Shadows": false,
  "ShadowCascadesCount": 4,
  "ShadowFilterAcrossCascades": true,
  "ShadowBestCascadeSearch": true
}
```

Conclusões:

- shaders suportam até 8 cascatas (`MAX_CASCADES=8`);
- o pacote real configura 4 cascatas quando o sistema é utilizado;
- shadows estão desativadas por padrão neste build;
- filter-across-cascades e best-cascade-search estão habilitados na configuração.

## 2. Duas noções diferentes de shadow

### 2.1 Projected / planar model shadow

Arquivos `shader_shadowmap.vs` dos models não são simplesmente o pass de CSM.

O caminho de `models/texture/shader_shadowmap.vs`:

1. executa GPU skinning normal;
2. aplica `g_Model`;
3. subtrai `g_BodyOrigin`;
4. projeta X em função de Z;
5. força `Position.z = 5.0`;
6. recoloca `g_BodyOrigin`;
7. aplica `g_ViewProj`.

Trecho lógico:

```text
position = model * skinned_position
position -= BodyOrigin
position.x = position.z * (position.x + 2000) / (position.z - 4000)
position.z = 5
position += BodyOrigin
```

Isso caracteriza uma projeção/achatamento geométrico para sombra no chão.

### 2.2 Cascaded shadow-map output pass

Já as permutations terminadas em `_shadow` no `resources.json` usam:

```text
SHADOW_OUTPUT=1
```

com fragment shaders `shadow.fs` / `shadow_ztest.fs`.

Esses fragment shaders fazem basicamente teste de alpha/coverage e não escrevem a cor convencional do material.

Portanto, na arquitetura nova separar:

```text
ProjectedCharacterShadowPass
CascadedShadowDepthPass
```

Não tratar ambos como uma única feature.

## 3. Wave permutation

Arquivo base: `data/shaders/models/texture/shader_wave.vs`

Depois do skinning:

```text
position += normal * NormalScale
position += normal * sin((WorldTime + VertexID * 931) * 0.007) * 28
```

Consequências:

- `wave` é deformação geométrica no vertex shader;
- usa `VertexID` explícito do vertex layout;
- usa `WorldTime`;
- a amplitude observada é 28 unidades;
- a oscilação ocorre ao longo da normal animada.

No renderer novo, `Wave` deve ser uma permutation/policy de vertex deformation.

## 4. ZTest permutation

`shader_ztest.vs` adiciona ao output:

```text
PositionModelView
```

O fragment shader executa:

```text
if (PositionModelView.y < ZTestRef) discard
```

Conclusão:

`ztest` nesse sistema não significa apenas `glDepthFunc`/depth buffer. É um **clip por referência espacial/altura** usando `g_ZTestRef`.

Representar no material/pipeline como feature própria, por exemplo:

```text
MaterialFeature::HeightClip
```

ou manter o nome de compatibilidade `ZTestRef`.

## 5. Chrome 7 e Chrome 8 fechados

### Chrome 7

```text
wave = WorldTime * 0.00006
UV = (normal.zz + normal.xx) * (0.8, 0.8) + (wave, wave)
UV += BlendTexCoord
```

### Chrome 8

```text
UV = normal.xy
UV += BlendTexCoord
```

Com isso, `chrome1..chrome8` têm evidência direta no Resource.

Conclusão arquitetural mantida:

- usar permutations/policies;
- não criar oito renderers independentes.

## 6. Joint renderer

Shader: `data/shaders/joint/shader.vs`

Vertex layout:

```text
Position : float3
Color    : float4
UV       : float2
```

Transformação:

```text
Position = mul(float4(Position,1).xzyw, Camera.ViewProj)
```

Depois apenas repassa Color e UV.

Isso confirma que joint/effect geometry usa caminho simples, separado do GPU skinning de BMD.

## 7. Particle renderer

Shader: `data/shaders/particle/shader.vs`

Vertex layout:

```text
Position : float3
Color    : float4
UV       : float2
```

A principal diferença observada do joint é a transformação usada no shader de partícula do pacote, que aplica `Camera.Proj` ao vértice já preparado pelo lado CPU/pipeline.

Implicação: joint e particle podem compartilhar estruturas de vértice, mas não devem ser fundidos automaticamente no mesmo transform path.

## 8. Bounding-box renderer

Shader: `data/shaders/bbox/shader.vs`

Vertex layout:

```text
Position : float3
```

Transforma com `Camera.ViewProj` e usa cor fixa cinza.

Os modelos possuem `model.bbox.json` com:

- AABB global;
- lista de AABBs por animação.

Exemplo confirmado em `player/dark_knight/armor/model.bbox.json`.

Implicação futura:

```text
Offline/Loaded AABB -> Frustum Culling -> Render Submission
```

Isso evita depender de recalcular bounding box da malha skinned a cada frame.

## 9. Registry de modelos e texturas

`data/resources.json` contém:

```text
shaders[]
models[]
textures[]
attachments[]
```

Os models são resolvidos por IDs lógicos para paths físicos.

As textures também usam IDs lógicos e podem declarar estado de wrap:

```text
"wrap": "clamp"
"wrap": "repeat"
```

Conclusão:

O `TextureManager` moderno deve representar:

```text
TextureResource
  id
  gpuTexture
  format
  sampler/wrap policy
```

E o código de gameplay/material deve trabalhar por Resource ID sempre que possível, não por path direto.

## 10. model.json / model.bbox.json

Exemplos de `model.json` mostram:

```text
model = model.bmd
bone_head = -1
textures_dir = ...
```

O arquivo não descreve toda a semântica de material. Logo a interpretação de mesh/material continua dependendo do BMD, texturas e registries/policies globais.

## 11. Shader registry / permutations confirmadas

O `resources.json` cadastra, entre outras:

```text
mesh_normal
mesh_blend
mesh_chrome ... mesh_chrome8
mesh_metal
mesh_oil
```

com variantes:

```text
base
shadowmap
wave
ztest
```

E uma segunda família de shadow-output:

```text
*_shadow
*_shadowmap_shadow
*_wave_shadow
*_ztest_shadow
```

Arquitetura recomendada:

```text
MaterialMode
  Normal
  Blend
  Chrome1..8
  Metal
  Oil

GeometryMode
  Normal
  ProjectedShadow
  Wave
  HeightClip

PassMode
  Color
  ShadowOutput
```

A chave de pipeline pode ser composta aproximadamente como:

```text
PipelineKey = MaterialMode + GeometryMode + PassMode + BlendState + DepthState + CullState
```

Isso reduz duplicação e reproduz melhor o desenho observado no Resource.

## 12. Pendências depois desta rodada

Continuam pendentes:

- valor de `SKELETON_TEXTURE_WIDTH` injetado na compilação;
- dimensões e formato real da Skeleton Texture;
- allocator/packing por frame de skeletons;
- binding slots reais no OpenGL backend;
- criação/cache real dos PSOs na aplicação;
- batching/instancing de models;
- origem e lifecycle de `g_VertexTexture` do material `oil`;
- estado exato de blend/depth/cull por shader ID;
- cruzamento detalhado com a source atual para definir os pontos de adaptação.

## Estado

Todos os itens deste documento estão baseados em arquivos reais do Resource, exceto as recomendações arquiteturais explicitamente marcadas como implicações/recomendações.