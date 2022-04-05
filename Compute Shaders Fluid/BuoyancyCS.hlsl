
#include "ComputeHelpers.hlsli"

cbuffer externalData : register(b0)
{
	float densityWeight;
	float temperatureBuoyancy;
	float ambientTemperature;
}

Texture3D			VelocityIn		: register(t0);
Texture3D			DensityIn		: register(t1);
Texture3D			TemperatureIn	: register(t2);
RWTexture3D<float4> VelocityOut		: register(u0);

[numthreads(
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS)]
void main(uint3 id : SV_DispatchThreadID)
{
	// Grab the temperature
	float temp = TemperatureIn[id].r;
	float density = DensityIn[id].a;

	// Calculate buoyancy force
	// GPU Gems 3 version... not great?
	/*float3 buoyancyForce =
		(1.0f / ambientTemperature - 1.0f / temp) *
		deltaTime * buoyancyConstant * float3(0, 1, 0);*/

	// From: http://web.stanford.edu/class/cs237d/smoke.pdf
	float3 buoyancyForce = float3(0, 1, 0) *
		(-densityWeight * density + temperatureBuoyancy * (temp - ambientTemperature));

	// Add to current velocity for output
	VelocityOut[id] = float4(VelocityIn[id].xyz + buoyancyForce, 1);
}