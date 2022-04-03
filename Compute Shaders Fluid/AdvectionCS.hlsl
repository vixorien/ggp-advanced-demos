
#include "ComputeHelpers.hlsli"

cbuffer externalData : register(b0)
{
	float deltaTime;
	float advectionDamper;
	int gridSizeX;
	int gridSizeY;
	int gridSizeZ;
}

Texture3D			TextureIn		: register(t0);
RWTexture3D<float4> TextureOut		: register(u0);

SamplerState SamplerLinearClamp		: register(s0);

[numthreads(
	FLUID_COMPUTE_THREADS_PER_AXIS, 
	FLUID_COMPUTE_THREADS_PER_AXIS, 
	FLUID_COMPUTE_THREADS_PER_AXIS)]
void main( uint3 id : SV_DispatchThreadID )
{
	// Pixel position in [0-gridSize] range
	float3 posInGrid = float3(id);

	// Move backwards based on velocity (still in [0-gridSize] range)
	posInGrid -= deltaTime * TextureIn[id].xyz * advectionDamper;

	// Convert position to UVW coords ([0-1] range)
	float3 posUVW = PixelIndexToUVW(posInGrid, gridSizeX, gridSizeY, gridSizeZ);

	// Interpolate and output
	TextureOut[id] = TextureIn.SampleLevel(SamplerLinearClamp, posUVW, 0) * advectionDamper;
}