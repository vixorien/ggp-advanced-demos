
#include "ComputeHelpers.hlsli"

cbuffer externalData : register(b0)
{
	int gridSizeX;
	int gridSizeY;
	int gridSizeZ;

	float injectRadius;		// In UV coords
	float3 injectPosition;	// In UV coords
	
	float3 injectColor;
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
	if (injectRadius == 0.0f)
		return;

	// Pixel position in [0-gridSize] range and UV coords [0-1] range
	float3 posInGrid = float3(id);
	float3 posUVW = PixelIndexToUVW(posInGrid, gridSizeX, gridSizeY, gridSizeZ);

	// How much to inject based on distance?
	float dist = length(posUVW - injectPosition);
	float injFalloff = max(0, injectRadius - dist) / injectRadius;

	// Grab the old values
	float4 oldColorAndDensity = DensityIn[id];

	// Calculate new values - color is a replacement, density is an add
	float3 newColor = injFalloff > 0 ? injectColor : oldColorAndDensity.rgb;
	float newDensity = oldColorAndDensity.a + injectDensity * injFalloff;

	// Spit out the updates
	DensityOut[id] = float4(newColor, newDensity);
	TemperatureOut[id] = TemperatureIn[id].r + injectTemperature * injFalloff;
}