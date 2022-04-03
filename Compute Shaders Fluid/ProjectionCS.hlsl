
#include "ComputeHelpers.hlsli"

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
	uint3 idL = id; idL.x--;
	uint3 idR = id; idR.x++;
	uint3 idD = id; idD.y--;
	uint3 idU = id; idU.y++;
	uint3 idB = id; idB.z--;
	uint3 idF = id; idF.z++;

	// Pressure of surrounding pixels
	float pL = PressureIn[idL].r;
	float pR = PressureIn[idR].r;
	float pD = PressureIn[idD].r;
	float pU = PressureIn[idU].r;
	float pB = PressureIn[idB].r;
	float pF = PressureIn[idF].r;

	// Pressure gradient
	float3 pressureGradient = 0.5f *
		float3(pR - pL, pU - pD, pF - pB);

	// Grab the old velocity and apply pressure
	float3 vel = VelocityIn[id].xyz;
	vel -= pressureGradient;

	VelocityOut[id] = float4(vel, 1);
}