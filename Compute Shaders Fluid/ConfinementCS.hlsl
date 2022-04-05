
#include "ComputeHelpers.hlsli"

cbuffer externalData : register(b0)
{
	float deltaTime;
	int gridSizeX;
	int gridSizeY;
	int gridSizeZ;
	float vorticityEpsilon;
}

Texture3D			VorticityIn		: register(t0);
Texture3D			VelocityIn		: register(t1);
RWTexture3D<float4>	VelocityOut		: register(u0);

[numthreads(
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS)]
void main(uint3 id : SV_DispatchThreadID)
{
	// Indices of surrounding pixels
	uint3 idL = GetLeftIndex(id);
	uint3 idR = GetRightIndex(id, gridSizeX);
	uint3 idD = GetDownIndex(id);
	uint3 idU = GetUpIndex(id, gridSizeY);
	uint3 idB = GetBackIndex(id);
	uint3 idF = GetForwardIndex(id, gridSizeZ);

	// Vorticity of surrounding pixels
	float vortL = length(VorticityIn[idL].rgb);
	float vortR = length(VorticityIn[idR].rgb);
	float vortD = length(VorticityIn[idD].rgb);
	float vortU = length(VorticityIn[idU].rgb);
	float vortB = length(VorticityIn[idB].rgb);
	float vortF = length(VorticityIn[idF].rgb);

	// Note: The nature of our neighbor checking here
	// will return the current cell's velocity value
	// (since we're basically clamping).
	// Is that what we want for vorticity?  Not sure!

	// Compute the vorticity based on surrounding cells
	float3 omega = VorticityIn[id].rgb;
	float3 eta = 0.5f * float3(
		(vortR - vortL),
		(vortU - vortD),
		(vortF - vortB));
	float3 N = eta / (length(eta) + 0.0001f);
	float3 confine = cross(N, omega) * vorticityEpsilon;

	// Apply
	VelocityOut[id] = float4(VelocityIn[id].rgb + confine, 1);
}