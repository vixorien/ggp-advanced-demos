
#include "ComputeHelpers.hlsli"

cbuffer externalData : register(b0)
{
	int gridSizeX;
	int gridSizeY;
	int gridSizeZ;
	float deltaTime;

	float injectRadius;		// In UV coords
	float3 injectPosition;	// In UV coords
	
	//float4 injectDensityColor;
	float injectDensity;

	float injectTemperature;
}

Texture3D			DensityIn		: register(t0);
Texture3D			TemperatureIn	: register(t1);
RWTexture3D<float4> DensityOut		: register(u0);
RWTexture3D<float>	TemperatureOut	: register(u1);

[numthreads(
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS)]
void main(uint3 id : SV_DispatchThreadID)
{
	// Pixel position in [0-gridSize] range and UV coords [0-1] range
	float3 posInGrid = float3(id);
	float3 posUVW = PixelIndexToUVW(posInGrid, gridSizeX, gridSizeY, gridSizeZ);

	// How much to inject based on distance?
	float dist = length(posUVW - injectPosition);
	float injAmount = max(0, injectRadius - dist) / injectRadius;

	// Output should match input plus any new injection
	DensityOut[id] = DensityIn[id] + injAmount * injectDensity * deltaTime;
	TemperatureOut[id] = TemperatureIn[id].r + injAmount * injectTemperature * deltaTime;
}