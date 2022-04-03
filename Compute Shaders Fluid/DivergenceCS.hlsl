
#include "ComputeHelpers.hlsli"

Texture3D			VelocityIn		: register(t0);
RWTexture3D<float>	DivergenceOut	: register(u0);

[numthreads(
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS)]
void main(uint3 id : SV_DispatchThreadID)
{
	// Indices of surrounding pixels
	uint3 idL = id; idL.x--;
	uint3 idR = id; idR.x++;
	uint3 idD = id; idD.y--;
	uint3 idU = id; idU.y++;
	uint3 idB = id; idB.z--;
	uint3 idF = id; idF.z++;

	// Velocities of surrounding pixels
	float velL = VelocityIn[idL].x;
	float velR = VelocityIn[idR].x;
	float velD = VelocityIn[idD].y;
	float velU = VelocityIn[idU].y;
	float velB = VelocityIn[idB].z;
	float velF = VelocityIn[idF].z;

	// Compute velocity's divergence and save
	float divergence = 0.5f * (
		(velR - velL) +
		(velU - velD) +
		(velF - velB));
	DivergenceOut[id] = divergence;
}