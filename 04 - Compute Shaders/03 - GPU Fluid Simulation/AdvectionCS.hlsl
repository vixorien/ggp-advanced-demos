
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
Texture3D			AdvectionIn		: register(t1);
Texture3D			ObstaclesIn		: register(t2);
RWTexture3D<float>  AdvectionOut1	: register(u0);
RWTexture3D<float2> AdvectionOut2	: register(u1);
RWTexture3D<float3> AdvectionOut3	: register(u2);
RWTexture3D<float4> AdvectionOut4	: register(u3);

SamplerState SamplerLinearClamp		: register(s0);

[numthreads(
	FLUID_COMPUTE_THREADS_PER_AXIS, 
	FLUID_COMPUTE_THREADS_PER_AXIS, 
	FLUID_COMPUTE_THREADS_PER_AXIS)]
void main( uint3 id : SV_DispatchThreadID )
{
	// Check for obstacle at this cell
	if (ObstaclesIn[id].r > 0.0f)
		return;

	// Pixel position in [0-gridSize] range
	float3 posInGrid = float3(id);

	// Move backwards based on velocity (still in [0-gridSize] range)
	posInGrid -= deltaTime * VelocityIn[id].xyz;

	// Convert position to UVW coords ([0-1] range)
	float3 posUVW = PixelIndexToUVW(posInGrid, gridSizeX, gridSizeY, gridSizeZ);

	// Which dimension?
	switch (channelCount)
	{
	case 1: AdvectionOut1[id] = damper * AdvectionIn.SampleLevel(SamplerLinearClamp, posUVW, 0).r; break;
	case 2:	AdvectionOut2[id] = damper * AdvectionIn.SampleLevel(SamplerLinearClamp, posUVW, 0).rg; break;
	case 3:	AdvectionOut3[id] = damper * AdvectionIn.SampleLevel(SamplerLinearClamp, posUVW, 0).rgb; break;
	case 4:	AdvectionOut4[id] = damper * AdvectionIn.SampleLevel(SamplerLinearClamp, posUVW, 0); break;
	}
}