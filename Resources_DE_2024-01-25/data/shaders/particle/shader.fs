Texture2D    g_Texture;
SamplerState g_Texture_sampler;

cbuffer ParticleSettings
{
	float IsPremultipliedAlpha;
	float IsLinear;
};

struct PSInput 
{ 
    float4 Position : SV_POSITION;
    float4 Color  : COLOR0;
    float2 UV : TEXCOORD0;
};

struct PSOutput
{ 
    float4 Color : SV_TARGET; 
};

float3 SRGBtoLINEAR(float3 srgbIn)
{
	float3 bLess  = step(float3(0.04045, 0.04045, 0.04045), srgbIn.xyz);
	float3 linOut = lerp( srgbIn.xyz/12.92, pow(saturate((srgbIn.xyz + float3(0.055, 0.055, 0.055)) / 1.055), float3(2.4, 2.4, 2.4)), bLess );
	return linOut;
}

float4 SRGBtoLINEAR(float4 srgbIn)
{
    return float4(SRGBtoLINEAR(srgbIn.xyz), srgbIn.w);
}

void main(in  PSInput  PSIn,
          out PSOutput PSOut)
{
	float4 color = g_Texture.Sample(g_Texture_sampler, PSIn.UV) * PSIn.Color;
	color.a = lerp(color.a, max(color.r, max(color.g, color.b)), IsPremultipliedAlpha);
    PSOut.Color = lerp(SRGBtoLINEAR(color), color, IsLinear); 
}