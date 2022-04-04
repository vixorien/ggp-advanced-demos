
#include "ComputeHelpers.hlsli"

cbuffer externalData : register(b0)
{
	int gridSizeX;
	int gridSizeY;
	int gridSizeZ;
}

Texture3D			VelocityIn		: register(t0);
RWTexture3D<float4>	VorticityOut	: register(u0);

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

	// Velocity of surrounding pixels
	float3 velL = VelocityIn[idL].rgb;
	float3 velR = VelocityIn[idR].rgb;
	float3 velD = VelocityIn[idD].rgb;
	float3 velU = VelocityIn[idU].rgb;
	float3 velB = VelocityIn[idB].rgb;
	float3 velF = VelocityIn[idF].rgb;

	// Note: The nature of our neighbor checking here
	// will return the current cell's velocity value
	// (since we're basically clamping).
	// Is that what we want for vorticity?  Not sure!

	// Compute the vorticity based on surrounding cells
	float3 vort = 0.5f * float3(
		((velU.z - velD.z) - (velF.y - velB.y)),
		((velF.x - velB.x) - (velR.z - velL.z)),
		((velR.y - velL.y) - (velU.x - velD.x)));

	VorticityOut[id] = float4(vort, 1);
}