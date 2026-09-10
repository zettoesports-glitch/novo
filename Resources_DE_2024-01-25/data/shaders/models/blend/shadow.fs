Texture2D    g_Texture;
SamplerState g_Texture_sampler;

cbuffer ModelSettings
{
	float4 g_LightPosition;
	float4 g_BodyLight;
	float4 g_BodyOrigin;
	float g_BoneOffset;
	float g_NormalScale;
	float g_EnableLight;
	float g_AlphaTest;
	float g_PremultiplyAlpha;
	float g_WorldTime;
	float g_ZTestRef;
	float g_Dummy1;
	float2 g_BlendTexCoord;
};

struct PSInput 
{ 
    float4 Position : SV_POSITION;
    float3 PosInLightViewSpace : LIGHT_SPACE_POS;
    float3 NormalWS : NORMALWS;
    float4 Color  : COLOR0;
    float2 UV : TEXCOORD0;
};

void main(in  PSInput  PSIn)
{
}