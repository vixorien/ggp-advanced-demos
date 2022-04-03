
#include "ComputeHelpers.hlsli"

Texture3D			DivergenceIn	: register(t0);
Texture3D			PressureIn		: register(t1);
RWTexture3D<float>	PressureOut		: register(u0);

[numthreads(
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS)]
void main(uint3 id : SV_DispatchThreadID)
{
	// Get the divergence here
	float div = DivergenceIn[id].r;

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

	// Compute the pressure based on surrounding cells
	float pressure = (pL + pR + pD + pU + pB + pF - div) / 6.0f;
	PressureOut[id] = pressure;
}