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

cbuffer ModelViewProj
{
	float4x4 g_Model;
	float4x4 g_ViewProj;
}

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

struct VSInput
{
    float3 Position : ATTRIB0;
    float3 Normal : ATTRIB1;
	float2 UV : ATTRIB2;
	uint2 Bone : ATTRIB3;
	uint Vertex : ATTRIB4;
};

struct PSInput
{ 
    float4 Position : SV_POSITION; 
    float3 PosInLightViewSpace : LIGHT_SPACE_POS;
    float3 NormalWS : NORMALWS;
    float4 Color : COLOR0;
	float2 UV : TEXCOORD0;
};

Texture2D<float4> g_SkeletonTexture;

int3 GetBoneIndex(uint baseX, uint baseY, uint index)
{
	uint t = (baseX + index);
	uint boneY = t / SKELETON_TEXTURE_WIDTH;
	uint boneX = t - boneY * SKELETON_TEXTURE_WIDTH;
	return int3(int(boneX), int(baseY + boneY), 0);
}

uint2 GetBoneOffset(uint boneOffset, uint boneId)
{
	boneOffset += boneId;
	boneOffset *= 2u;
	
	uint boneY = boneOffset / SKELETON_TEXTURE_WIDTH;
	uint boneX = boneOffset - boneY * SKELETON_TEXTURE_WIDTH;
	
	return uint2(boneX, boneY);
}

float4 RequestBoneRotation(uint2 bone)
{
	return g_SkeletonTexture.Load(GetBoneIndex(bone.x, bone.y, 0u)).yzwx;
}

float4 RequestBonePositionAndScale(uint2 bone)
{
	return g_SkeletonTexture.Load(GetBoneIndex(bone.x, bone.y, 1u));
}

float3 TransformPosition(float3 v, float4 rotation, float4 posAndScale)
{
	v *= posAndScale.w;
	return (2.0f * cross(rotation.xyz, cross(rotation.xyz, v) + rotation.w * v) + v) + posAndScale.xyz;
}

float3 TransformNormal(float3 v, float4 rotation, float4 posAndScale)
{
	v *= posAndScale.w;
	return (2.0f * cross(rotation.xyz, cross(rotation.xyz, v) + rotation.w * v) + v);
}

void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
	uint boneOffset = uint(g_BoneOffset);
	uint2 posBone = GetBoneOffset(boneOffset, VSIn.Bone.x);
	uint2 normalBone = GetBoneOffset(boneOffset, VSIn.Bone.y);
	
	float3 position = TransformPosition(
		VSIn.Position,
		RequestBoneRotation(posBone),
		RequestBonePositionAndScale(posBone)
	);
	float3 normal = TransformNormal(
		VSIn.Normal,
		RequestBoneRotation(normalBone),
		RequestBonePositionAndScale(normalBone)
	);
	
	position += normal * g_NormalScale;
	
	float luminosity = max((dot(normal, g_LightPosition.xyz) * 0.8f + 0.4f) * g_EnableLight + (1.0f - g_EnableLight), 0.2f);
	PSIn.Color = clamp(g_BodyLight * float4(luminosity, luminosity, luminosity, 1.0f), 0.0f, 1.0f);
	
	float wave = fmod(g_WorldTime, 4000.0f) * 0.00025f - 0.4f;
	PSIn.UV = (normal.zz + normal.xx) * float2(0.8f, 0.8f) + float2(wave * 2.0f, wave * 2.0f);
	PSIn.UV += g_BlendTexCoord;
	
	PSIn.Position = mul(float4(position, 1.0f), g_Model).xzyw;
#if SHADOW_OUTPUT == 0 && USE_SHADOW == 1
    float4 LightSpacePos = mul( PSIn.Position, g_LightAttribs.ShadowAttribs.mWorldToLightView);
    PSIn.PosInLightViewSpace = LightSpacePos.xyz / LightSpacePos.w;
#else
    PSIn.PosInLightViewSpace = float3(0.0, 0.0, 0.0);
#endif
	PSIn.NormalWS = normal.xzy;
	PSIn.Position = mul(PSIn.Position, g_ViewProj);
}