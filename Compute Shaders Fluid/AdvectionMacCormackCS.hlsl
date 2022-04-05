
#include "ComputeHelpers.hlsli"

cbuffer externalData : register(b0)
{
	float deltaTime;
	int gridSizeX;
	int gridSizeY;
	int gridSizeZ;
	int channelCount;
	float damper;
}

Texture3D			VelocityIn		: register(t0);
Texture3D			PhiNIn			: register(t1);
Texture3D			PhiN1In			: register(t2);
Texture3D			AdvectionIn		: register(t3);
RWTexture3D<float>  AdvectionOut1	: register(u0);
RWTexture3D<float2> AdvectionOut2	: register(u1);
RWTexture3D<float3> AdvectionOut3	: register(u2);
RWTexture3D<float4> AdvectionOut4	: register(u3);

SamplerState SamplerLinearClamp		: register(s0);

[numthreads(
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS)]
void main(uint3 id : SV_DispatchThreadID)
{
	// Pixel position in [0-gridSize] range
	float3 posInGrid = float3(id);

	// Move backwards based on velocity (still in [0-gridSize] range)
	posInGrid -= deltaTime * VelocityIn[id].xyz;

	// Convert position to UVW coords ([0-1] range)
	float3 posUVW = PixelIndexToUVW(posInGrid, gridSizeX, gridSizeY, gridSizeZ);

	// Convert position to UVW coords ([0-1] range) at the 
	// cell center (no half pixel offset yet)
	float3 cornerInGrid = floor(posInGrid + float3(0.5f, 0.5f, 0.5f));
	float3 cornerUVW = cornerInGrid / float3(gridSizeX - 1, gridSizeY - 1, gridSizeZ - 1);

	// Half pixel offset
	float3 sizes = float3(gridSizeX, gridSizeY, gridSizeZ);
	float3 h = 0.5f / sizes;

	// Grab 8 neighboring values around the "cell corner"
	float4 v0 = AdvectionIn[UVWToPixelIndex(cornerUVW + float3(-h.x, -h.y, -h.z), sizes)];
	float4 v1 = AdvectionIn[UVWToPixelIndex(cornerUVW + float3(-h.x, -h.y, +h.z), sizes)];
	float4 v2 = AdvectionIn[UVWToPixelIndex(cornerUVW + float3(-h.x, +h.y, -h.z), sizes)];
	float4 v3 = AdvectionIn[UVWToPixelIndex(cornerUVW + float3(-h.x, +h.y, +h.z), sizes)];
	float4 v4 = AdvectionIn[UVWToPixelIndex(cornerUVW + float3(+h.x, -h.y, -h.z), sizes)];
	float4 v5 = AdvectionIn[UVWToPixelIndex(cornerUVW + float3(+h.x, -h.y, +h.z), sizes)];
	float4 v6 = AdvectionIn[UVWToPixelIndex(cornerUVW + float3(+h.x, +h.y, -h.z), sizes)];
	float4 v7 = AdvectionIn[UVWToPixelIndex(cornerUVW + float3(+h.x, +h.y, +h.z), sizes)];

	// Calculate min and max
	float4 minPhi = min(min(min(min(min(min(min(v0, v1), v2), v3), v4), v5), v6), v7);
	float4 maxPhi = max(max(max(max(max(max(max(v0, v1), v2), v3), v4), v5), v6), v7);

	// Final advection
	float4 final = PhiN1In.SampleLevel(SamplerLinearClamp, posUVW, 0) + 
		0.5f * (AdvectionIn[id] - PhiNIn[id]);

	// Clamp values
	final = max(min(final, maxPhi), minPhi);

	// Which dimension?
	switch (channelCount)
	{
	case 1: AdvectionOut1[id] = damper * final.r; break;
	case 2:	AdvectionOut2[id] = damper * final.rg; break;
	case 3:	AdvectionOut3[id] = damper * final.rgb; break;
	case 4:	AdvectionOut4[id] = damper * final; break;
	}
}