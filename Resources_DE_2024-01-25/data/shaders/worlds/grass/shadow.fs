Texture2DArray g_Textures;
SamplerState g_Textures_sampler;

struct PSInput 
{ 
    float4 Position : SV_POSITION;
    float3 PosInLightViewSpace : LIGHT_SPACE_POS;
    float3 NormalWS : NORMALWS;
    float4 Color  : COLOR0;
    float3 UV : TEXCOORD0;
};

void main(in  PSInput  PSIn)
{
	float4 color = g_Textures.Sample(g_Textures_sampler, PSIn.UV) * PSIn.Color;
	if(color.a < 0.25) discard;
}