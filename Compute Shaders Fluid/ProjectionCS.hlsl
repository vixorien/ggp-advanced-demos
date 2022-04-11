
#include "ComputeHelpers.hlsli"

cbuffer externalData : register(b0)
{
	int gridSizeX;
	int gridSizeY;
	int gridSizeZ;
}

Texture3D			PressureIn		: register(t0);
Texture3D			VelocityIn		: register(t1);
Texture3D			ObstaclesIn		: register(t2);
RWTexture3D<float4>	VelocityOut		: register(u0);

[numthreads(
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS)]
void main(uint3 id : SV_DispatchThreadID)
{
	// Is this cell an obstacle?
	if (ObstaclesIn[id].r > 0.0f)
	{
		VelocityOut[id] = float4(0, 0, 0, 1);
		return;
		// NOTE: This should really be the obstacle's velocity
		// but we don't have that yet
	}

	// Indices of surrounding pixels
	uint3 idL = GetLeftIndex(id);
	uint3 idR = GetRightIndex(id, gridSizeX);
	uint3 idD = GetDownIndex(id);
	uint3 idU = GetUpIndex(id, gridSizeY);
	uint3 idB = GetBackIndex(id);
	uint3 idF = GetForwardIndex(id, gridSizeZ);

	// Pressure of surrounding pixels
	float pressureHere = PressureIn[id].r;
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


	// Velocities of surrounding pixels (change to obstacle velocities then!)
	float velL = VelocityIn[idL].x;
	float velR = VelocityIn[idR].x;
	float velD = VelocityIn[idD].y;
	float velU = VelocityIn[idU].y;
	float velB = VelocityIn[idB].z;
	float velF = VelocityIn[idF].z;

	// Check for boundaries
	// NOTE: Will need obstacle velocity!
	// NOTE: Swapped velocity mask to be negative instead of zero!  Using
	//       zero would result in full x/y/z corners having their velocity
	//       completely masked out
	float3 velocityMask = float3(1, 1, 1);
	if (ObstaclesIn[idL].r > 0.0f || idL.x == id.x) { pL = pressureHere; velocityMask.x = -1; }
	if (ObstaclesIn[idR].r > 0.0f || idR.x == id.x) { pR = pressureHere; velocityMask.x = -1; }
	if (ObstaclesIn[idD].r > 0.0f || idD.y == id.y) { pD = pressureHere; velocityMask.y = -1; }
	if (ObstaclesIn[idU].r > 0.0f || idU.y == id.y) { pU = pressureHere; velocityMask.y = -1; }
	if (ObstaclesIn[idB].r > 0.0f || idB.z == id.z) { pB = pressureHere; velocityMask.z = -1; }
	if (ObstaclesIn[idF].r > 0.0f || idF.z == id.z) { pF = pressureHere; velocityMask.z = -1; }

	// Pressure gradient
	float3 pressureGradient = 
		0.5f * float3(pR - pL, pU - pD, pF - pB);

	// Grab the old velocity and apply pressure
	float3 velOld = VelocityIn[id].xyz;
	float3 velNew = velOld - pressureGradient;

	VelocityOut[id] = float4(velNew * velocityMask, 1);
}