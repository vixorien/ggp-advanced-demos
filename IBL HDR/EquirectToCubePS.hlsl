

#include "Lighting.hlsli"

cbuffer externalData : register(b0)
{
	int faceIndex;
};


struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};

// Textures and samplers
Texture2D		Pixels			: register(t0);
SamplerState	BasicSampler	: register(s0);



float2 DirectionToLatLongUV(float3 dir)
{
	// Calculate polar coords
	float theta = acos(dir.y);
	float phi = atan2(dir.z, -dir.x);

	// Normalize
	return float2(
		(PI + phi) / TWO_PI,
		theta / PI);
}

// http://www.codinglabs.net/article_physically_based_rendering.aspx
float4 main(VertexToPixel input) : SV_TARGET
{
	// Get a -1 to 1 range on x/y
	float2 o = input.uv * 2 - 1;

	// Tangent basis
	float3 dir = float3(0, 0, 0);

	// Figure out the z ("normal" of this pixel)
	switch (faceIndex)
	{
	default:
	case 0: dir = float3(+1, -o.y, -o.x); break;
	case 1: dir = float3(-1, -o.y, +o.x); break;
	case 2: dir = float3(+o.x, +1, +o.y); break;
	case 3: dir = float3(+o.x, -1, -o.y); break;
	case 4: dir = float3(+o.x, -o.y, +1); break;
	case 5: dir = float3(-o.x, -o.y, -1); break;
	}
	dir = normalize(dir);
	
	// Convert from direction to UV
	float2 uv = DirectionToLatLongUV(dir);
	
	// Sample and save
	return Pixels.Sample(BasicSampler, uv);
}