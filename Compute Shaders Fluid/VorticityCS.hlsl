
#include "ComputeHelpers.hlsli"

cbuffer externalData : register(b0)
{
	int gridSizeX;
	int gridSizeY;
	int gridSizeZ;
}

Texture3D			VelocityIn		: register(t0);
Texture3D			ObstaclesIn		: register(t1);
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
	// Note: The nature of our neighbor checking here
	// will return the current cell's velocity value
	// (since we're basically clamping).
	// Is that what we want for vorticity?  Not sure!
	float3 velL = VelocityIn[idL].xyz;
	float3 velR = VelocityIn[idR].xyz;
	float3 velD = VelocityIn[idD].xyz;
	float3 velU = VelocityIn[idU].xyz;
	float3 velB = VelocityIn[idB].xyz;
	float3 velF = VelocityIn[idF].xyz;

	// Use this cell's velocity for any surrounding
	// cells that contain an obstacle
	//float3 velHere = VelocityIn[id].xyz; // Should be obstacles eventually?
	//if (ObstaclesIn[idL].r > 0.0f) velL = velHere;
	//if (ObstaclesIn[idR].r > 0.0f) velR = velHere;
	//if (ObstaclesIn[idD].r > 0.0f) velD = velHere;
	//if (ObstaclesIn[idU].r > 0.0f) velU = velHere;
	//if (ObstaclesIn[idB].r > 0.0f) velB = velHere;
	//if (ObstaclesIn[idF].r > 0.0f) velF = velHere;

	// Compute the vorticity based on surrounding cells
	float3 vort = 0.5f * float3(
		((velU.z - velD.z) - (velF.y - velB.y)),
		((velF.x - velB.x) - (velR.z - velL.z)),
		((velR.y - velL.y) - (velU.x - velD.x)));

	VorticityOut[id] = float4(vort, 1);
}