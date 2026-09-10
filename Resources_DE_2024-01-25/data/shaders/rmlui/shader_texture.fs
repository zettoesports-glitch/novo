Texture2D    g_Texture;
SamplerState g_Texture_sampler;

struct RmlAttribs
{
    matrix Transform;
	float2 Translation;
	float IsLinear;
};

cbuffer cbRmlAttribs
{
    RmlAttribs g_RmlAttribs;
}

struct PSInput 
{ 
    float4 Position : SV_POSITION; 
    float4 Color : COLOR0;
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
    PSOut.Color = lerp(SRGBtoLINEAR(color), color, g_RmlAttribs.IsLinear); 
}
