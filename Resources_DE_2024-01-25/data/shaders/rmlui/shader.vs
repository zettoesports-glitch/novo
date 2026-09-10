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

struct VSInput
{
    float2 Position : ATTRIB0;
    float4 Color : ATTRIB1;
    float2 UV : ATTRIB2;
};

struct PSInput 
{
    float4 Position : SV_POSITION; 
    float4 Color : COLOR0;
	float2 UV : TEXCOORD0;
};

void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
	PSIn.Position = mul(
		float4(VSIn.Position + g_RmlAttribs.Translation, 0.0f, 1.0f),
		g_RmlAttribs.Transform
	);
	PSIn.Color = VSIn.Color;
	PSIn.UV = float2(VSIn.UV.x, 1.0f - VSIn.UV.y);
}