Texture2DArray g_Textures;
SamplerState g_Textures_sampler;

struct PSInput 
{ 
    float4 Position : SV_POSITION;
    float3 PosInLightViewSpace : LIGHT_SPACE_POS;
    float3 NormalWS : NORMALWS;
    float4 Color  : COLOR0;
    float4 UV : TEXCOORD0;
	float3 Setting : TEXCOORD1;
};

void main(in  PSInput  PSIn)
{
}