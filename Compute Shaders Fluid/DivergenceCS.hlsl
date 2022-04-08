
#include "ComputeHelpers.hlsli"

cbuffer externalData : register(b0)
{
	int gridSizeX;
	int gridSizeY;
	int gridSizeZ;
}

Texture3D			VelocityIn		: register(t0);
Texture3D			ObstaclesIn		: register(t1);
RWTexture3D<float>	DivergenceOut	: register(u0);

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

	// Velocities of surrounding pixels
	float velL = VelocityIn[idL].x;
	float velR = VelocityIn[idR].x;
	float velD = VelocityIn[idD].y;
	float velU = VelocityIn[idU].y;
	float velB = VelocityIn[idB].z;
	float velF = VelocityIn[idF].z;

	// Check for boundaries (should use obstacle velocity once that's in!)
	if (ObstaclesIn[idL].r > 0.0f || idL.x == id.x) { velL = 0; }
	if (ObstaclesIn[idR].r > 0.0f || idR.x == id.x) { velR = 0; }
	if (ObstaclesIn[idD].r > 0.0f || idD.y == id.y) { velD = 0; }
	if (ObstaclesIn[idU].r > 0.0f || idU.y == id.y) { velU = 0; }
	if (ObstaclesIn[idB].r > 0.0f || idB.z == id.z) { velB = 0; }
	if (ObstaclesIn[idF].r > 0.0f || idF.z == id.z) { velF = 0; }

	// Compute velocity's divergence and save
	float divergence = 0.5f * (
		(velR - velL) +
		(velU - velD) +
		(velF - velB));
	DivergenceOut[id] = divergence;
}