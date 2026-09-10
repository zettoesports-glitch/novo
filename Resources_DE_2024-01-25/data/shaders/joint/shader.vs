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

struct VSInput
{
    float3 Position : ATTRIB0;
	float4 Color : ATTRIB1;
	float2 UV : ATTRIB2;
};

struct PSInput 
{ 
    float4 Position : SV_POSITION;
    float4 Color  : COLOR0;
    float2 UV : TEXCOORD0;
};

void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
	PSIn.Position = mul(
		float4(VSIn.Position, 1.0f).xzyw,
		g_CameraAttribs.mViewProj
	);
	PSIn.Color = VSIn.Color;
	PSIn.UV = VSIn.UV;
}