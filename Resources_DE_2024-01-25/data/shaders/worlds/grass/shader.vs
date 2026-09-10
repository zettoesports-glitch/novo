#ifndef SHADOW_OUTPUT
#   define SHADOW_OUTPUT 0
#endif
#ifndef USE_SHADOW
#   define USE_SHADOW 0
#endif

struct CameraAttribs
{
    float4 f4Position;
    float4 f4ViewportSize;

    float2 f2ViewportOrigin;
    float fNearPlaneZ; 
    float fFarPlaneZ;

    matrix mView;
    matrix mProj;
    matrix mViewProj;
    matrix mViewInv;
    matrix mProjInv;
    matrix mViewProjInv;

    float4 f4ExtraData[5];
};

cbuffer cbCameraAttribs
{
    CameraAttribs g_CameraAttribs;
}

#if SHADOW_OUTPUT == 0 && USE_SHADOW == 1
#define MAX_CASCADES 8

struct CascadeAttribs
{
	float4 f4LightSpaceScale;
	float4 f4LightSpaceScaledBias;
    float4 f4StartEndZ;

    // Cascade margin in light projection space ([-1, +1] x [-1, +1] x [-1(GL) or 0, +1])
    float4 f4MarginProjSpace;
};

struct ShadowMapAttribs
{
    matrix mWorldToLightView;  // Transform from view space to light projection space
    CascadeAttribs Cascades[MAX_CASCADES];

    matrix mWorldToShadowMapUVDepth[MAX_CASCADES];
    float4 f4CascadeCamSpaceZEnd[MAX_CASCADES/4];

    float4 f4ShadowMapDim;    // Width, Height, 1/Width, 1/Height

    // Number of shadow cascades
    int   iNumCascades;
    float fNumCascades;
	bool  bVisualizeCascades;
    bool  bVisualizeShadowing;

    float fReceiverPlaneDepthBiasClamp;
    float fFixedDepthBias;
    float fCascadeTransitionRegion;
    int   iMaxAnisotropy;

    float fVSMBias;
    float fVSMLightBleedingReduction;
    float fEVSMPositiveExponent;
    float fEVSMNegativeExponent;

    bool  bIs32BitEVSM;
    int   iFixedFilterSize;
    float fFilterWorldSize;
    bool  fDummy;
};

struct LightAttribs
{
    float4 f4Direction;
    float4 f4AmbientLight;
    float4 f4Intensity;

    ShadowMapAttribs ShadowAttribs;
};

cbuffer cbLightAttribs
{
    LightAttribs g_LightAttribs;
};
#endif

cbuffer TerrainSettings
{
	float WaterMove;
	float WindScale;
	float WindSpeed;
	float Dummy;
};

struct VSInput
{
    uint2 RPosition : ATTRIB0; // Relative Position
    uint2 Position : ATTRIB1; // Fixed Position (x,y)
};

struct PSInput 
{ 
    float4 Position : SV_POSITION; 
    float3 PosInLightViewSpace : LIGHT_SPACE_POS;
    float3 NormalWS : NORMALWS;
    float4 Color : COLOR0;
	float3 UV : TEXCOORD0;
};

Texture2D<uint> g_AttributesTexture;
Texture2D<uint4> g_MappingTexture;
Texture2D<float> g_HeightTexture;
Texture2D<float4> g_UVTexture;
Texture2D<float4> g_NormalTexture;
Texture2D<float4> g_LightTexture;

void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
	float visible = 1.0f - step(1.0f, float(g_AttributesTexture.Load(int3(VSIn.Position, 0)) & 8u));
	uint4 mapping = g_MappingTexture.Load(int3(VSIn.Position, 0));
	float2 settings = float2(mapping.zw) / 255.0f;
	visible *= step(1.0f, 1.0f - settings.x);
	visible *= step(0.001f, 1.0f - settings.y);
	
	uint2 gposition = VSIn.Position + VSIn.RPosition.xx;
	uint2 rposition = VSIn.Position + VSIn.RPosition;
	
	float3 position = float3(gposition, 0.0f);
	position.z = g_HeightTexture.Load(int3(gposition, 0));
	position *= visible;
	position.xy *= 100.0f;
	
	float height = g_UVTexture.Load(int3(settings.y, 0, 0)).x;
	
	float apply = float(1u - VSIn.RPosition.y);
	position.x -= 50.0f * apply;
	position.y += sin(WindSpeed + float(rposition.x) * 5.0f) * WindScale * apply;
	position.z += height * apply;
	
	float3 normal = g_NormalTexture.Load(int3(rposition, 0)).xyz;
	PSIn.Color = g_LightTexture.Load(int3(rposition, 0));
	PSIn.Color *= visible;
	
	PSIn.UV = float3(float2(rposition.x, VSIn.RPosition.y) * float2(0.25f, 1.0f), settings.y);
	
	PSIn.Position = float4(position, 1.0f).xzyw;
#if SHADOW_OUTPUT == 0 && USE_SHADOW == 1
    float4 LightSpacePos = mul( PSIn.Position, g_LightAttribs.ShadowAttribs.mWorldToLightView);
    PSIn.PosInLightViewSpace = LightSpacePos.xyz / LightSpacePos.w;
#else
    PSIn.PosInLightViewSpace = float3(0.0, 0.0, 0.0);
#endif
	PSIn.NormalWS = normal.xzy;
	PSIn.Position = mul(PSIn.Position, g_CameraAttribs.mViewProj);
}