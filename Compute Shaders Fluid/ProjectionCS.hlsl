
#include "ComputeHelpers.hlsli"

cbuffer externalData : register(b0)
{
	int gridSizeX;
	int gridSizeY;
	int gridSizeZ;
}

Texture3D			PressureIn		: register(t0);
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

	// Pressure of surrounding pixels
	float pL = PressureIn[idL].r;
	float pR = PressureIn[idR].r;
	float pD = PressureIn[idD].r;
	float pU = PressureIn[idU].r;
	float pB = PressureIn[idB].r;
	float pF = PressureIn[idF].r;

	// Note: The nature of our neighbor checking here
	// will return the current cell's pressure value
	// (since we're basically clamping), which is exactly
	// what we want at the boundaries of the volume.  This
	// means we don't need to explicitly check for edges.
	// 
	// HOWEVER: We do need to mask out entire dimensions, 
	// so we still need a check here!

	// Check for boundaries
	float3 velocityMask = float3(1, 1, 1);
	if (idL.x == id.x) { velocityMask.x = 0; }
	if (idR.x == id.x) { velocityMask.x = 0; }
	if (idD.y == id.y) { velocityMask.y = 0; }
	if (idU.y == id.y) { velocityMask.y = 0; }
	if (idB.z == id.z) { velocityMask.z = 0; }
	if (idF.z == id.z) { velocityMask.z = 0; }

	// Pressure gradient
	float3 pressureGradient = 
		0.5f * float3(pR - pL, pU - pD, pF - pB);

	// Grab the old velocity and apply pressure
	float3 velOld = VelocityIn[id].xyz;
	float3 velNew = velOld - pressureGradient;

	VelocityOut[id] = float4(velNew * velocityMask, 1);
}