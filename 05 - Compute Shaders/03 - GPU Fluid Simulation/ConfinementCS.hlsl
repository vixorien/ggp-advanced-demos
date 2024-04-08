
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
Texture3D			ObstaclesIn		: register(t2);
RWTexture3D<float4>	VelocityOut		: register(u0);

[numthreads(
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS)]
void main(uint3 id : SV_DispatchThreadID)
{
	//// Check for obstacle at this cell
	//if (ObstaclesIn[id].r > 0.0f)
	//	return;

	// Indices of surrounding pixels
	uint3 idL = GetLeftIndex(id);
	uint3 idR = GetRightIndex(id, gridSizeX);
	uint3 idD = GetDownIndex(id);
	uint3 idU = GetUpIndex(id, gridSizeY);
	uint3 idB = GetBackIndex(id);
	uint3 idF = GetForwardIndex(id, gridSizeZ);

	// Vorticity of surrounding pixels
	// Note: The nature of our neighbor checking here
	// will return the current cell's velocity value
	// (since we're basically clamping).
	// Is that what we want for vorticity?  Not sure!
	float vortL = length(VorticityIn[idL].xyz);
	float vortR = length(VorticityIn[idR].xyz);
	float vortD = length(VorticityIn[idD].xyz);
	float vortU = length(VorticityIn[idU].xyz);
	float vortB = length(VorticityIn[idB].xyz);
	float vortF = length(VorticityIn[idF].xyz);

	// Use this cell's vorticity for any surrounding
	// cells that contain an obstacle
	float3 vortHere = VorticityIn[id].xyz;
	//float vortLength = length(vortHere);
	//if (ObstaclesIn[idL].r > 0.0f) vortL = vortLength;
	//if (ObstaclesIn[idR].r > 0.0f) vortR = vortLength;
	//if (ObstaclesIn[idD].r > 0.0f) vortD = vortLength;
	//if (ObstaclesIn[idU].r > 0.0f) vortU = vortLength;
	//if (ObstaclesIn[idB].r > 0.0f) vortB = vortLength;
	//if (ObstaclesIn[idF].r > 0.0f) vortF = vortLength;

	// Compute the vorticity based on surrounding cells
	float3 vortGrad = 0.5f * float3(
		(vortR - vortL),
		(vortU - vortD),
		(vortF - vortB));

	// Ensure we can normalize the gradient vector
	float3 confine = float3(0, 0, 0);
	if (dot(vortGrad, vortGrad))
	{
		confine = cross(normalize(vortGrad), vortHere) * vorticityEpsilon;
	}

	// Apply
	VelocityOut[id] = float4(VelocityIn[id].rgb + confine, 1);
}