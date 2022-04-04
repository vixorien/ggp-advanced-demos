
#include "SimplexNoise.hlsli"

#define NUM_SAMPLES 256

cbuffer externalData : register(b0)
{
	matrix invWorld;
	float3 cameraPosition;
}

struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float3 uvw				: TEXCOORD;
	float3 worldPos			: POSITION;
};

Texture3D volumeTexture : register(t0);

SamplerState SamplerLinearClamp : register(s0);


// Performs a Ray-Box (specifically AABB) intersection
// Based on: http://prideout.net/blog/?p=64
bool RayAABBIntersection(float3 pos, float3 dir, float3 boxMin, float3 boxMax, out float t0, out float t1)
{
	// Invert the direction and get a test min and max
	float3 invDir = 1.0f / dir;
	float3 testMin = (boxMin - pos) * invDir;
	float3 testMax = (boxMax - pos) * invDir;

	// Figure out min and max of the results
	float3 tmin = min(testMin, testMax);
	float3 tmax = max(testMin, testMax);

	// Get max of tmin's x, y & z
	float2 t = max(tmin.xx, tmin.yz);
	t0 = max(t.x, t.y);

	// Get min of tmax's x, y & z
	t = min(tmax.xx, tmax.yz);
	t1 = min(t.x, t.y);

	// Did we hit?
	return t0 <= t1;
}


float4 main(VertexToPixel input) : SV_TARGET
{
	// Assumption: actual geometry is a 1x1x1 cube with offsets at 0.5 and -0.5
	const float3 aabbMin = float3(-0.5f, -0.5f, -0.5f);
	const float3 aabbMax = float3(0.5f, 0.5f, 0.5f);

	// Get view dir in world space then local
	float3 pos = cameraPosition;
	float3 dir = normalize(input.worldPos - pos);

	float3 posLocal = mul(invWorld, float4(pos, 1)).xyz;
	float3 dirLocal = mul(invWorld, float4(dir, 1)).xyz;

	// Intersect the bounds
	float nearHit;
	float farHit;
	RayAABBIntersection(posLocal, dirLocal, aabbMin, aabbMax, nearHit, farHit);
	
	// Are we in the cube?  If so, start at our position
	if (nearHit < 0.0f)
		nearHit = 0.0f;

	// Beginning and end ray positions
	float3 rayStart = posLocal + dirLocal * nearHit;
	float3 rayEnd = posLocal + dirLocal * farHit;

	// Set up the current position and the step amount
	float maxDist = farHit - nearHit;
	float3 currentPos = rayStart;
	float step = 1.0f / NUM_SAMPLES;// length(rayStart - rayEnd) / NUM_SAMPLES;
	float3 stepDir = step * dir;

	// Accumulate as we raymarch
	float a = 0.0f;
	float4 c = float4(0, 0, 0, 0);
	float totalDist = 0.0f;

	[loop]
	for (int i = 0; i < NUM_SAMPLES; i++)
	{
		float3 uvw = currentPos + float3(0.5f, 0.5f, 0.5f);

		float4 color = volumeTexture.SampleLevel(SamplerLinearClamp, uvw, 0);
		c += color * step;

		// Continue stepping along ray
		currentPos += stepDir;
		totalDist += step;

		if (totalDist >= maxDist)
			break;
	}

	return c;
}