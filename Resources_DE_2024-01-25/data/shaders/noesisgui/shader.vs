cbuffer ProjectionBuffer
{
    float4x4 projectionMtx[2];
}

cbuffer Buffer1
{
    float2 textureSize;
}

// warning X3571: pow(f, e) will not work for negative f
#pragma warning (disable : 3571)

float SRGBToLinear(float v)
{
    if (v <= 0.04045)
    {
      return v * (1.0 / 12.92);
    }
    else
    {
      return pow( v * (1.0 / 1.055) + 0.0521327, 2.4);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void main(in  VSInput VSIn, out PSInput PSIn) 
{
#if STEREO_RENDERING
    PSIn.position = mul(float4(VSIn.position, 0, 1), projectionMtx[VSIn.eyeIndex]);
    PSIn.renderTargetIndex = VSIn.eyeIndex;
#else
    PSIn.position = mul(float4(VSIn.position, 0, 1), projectionMtx[0]);
#endif

#if HAS_COLOR
  #if LINEAR_COLOR_SPACE
    PSIn.color.r = SRGBToLinear(VSIn.color.r);
    PSIn.color.g = SRGBToLinear(VSIn.color.g);
    PSIn.color.b = SRGBToLinear(VSIn.color.b);
    PSIn.color.a = VSIn.color.a;
  #else
    PSIn.color = VSIn.color;
  #endif
#endif

#if DOWNSAMPLE
    PSIn.uv0 = VSIn.uv0 + float2(VSIn.uv1.x, VSIn.uv1.y);
    PSIn.uv1 = VSIn.uv0 + float2(VSIn.uv1.x, -VSIn.uv1.y);
    PSIn.uv2 = VSIn.uv0 + float2(-VSIn.uv1.x, VSIn.uv1.y);
    PSIn.uv3 = VSIn.uv0 + float2(-VSIn.uv1.x, -VSIn.uv1.y);
#else
    #if HAS_UV0
      PSIn.uv0 = VSIn.uv0;
    #endif
    #if HAS_UV1
      PSIn.uv1 = VSIn.uv1;
    #endif
#endif

#if SDF
    PSIn.st1 = float4(VSIn.uv1 * textureSize.xy, 1.0 / (3.0 * textureSize.xy));
#endif

#if HAS_COVERAGE
    PSIn.coverage = VSIn.coverage;
#endif

#if HAS_RECT
    PSIn.rect = VSIn.rect;
#endif

#if HAS_TILE
    PSIn.tile = VSIn.tile;
#endif

#if HAS_IMAGE_POSITION
    PSIn.imagePos = VSIn.imagePos;
#endif
}