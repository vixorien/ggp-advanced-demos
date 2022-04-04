
#include "ComputeHelpers.hlsli"

cbuffer externalData : register(b0)
{
	float deltaTime;
	float buoyancyConstant;
	float ambientTemperature;
}

Texture3D			VelocityIn		: register(t0);
Texture3D			TemperatureIn	: register(t1);
RWTexture3D<float4> VelocityOut		: register(u0);

[numthreads(
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS)]
void main(uint3 id : SV_DispatchThreadID)
{
	// Grab the temperature
	float temp = TemperatureIn[id].r;
	if (temp == 0.0f) 
		return;

	// Calculate buoyancy force
	float3 buoyancyForce =
		(1.0f / ambientTemperature - 1.0f / temp) *
		deltaTime * buoyancyConstant * float3(0, 1, 0);


	// Add to current velocity for output
	VelocityOut[id] = float4(VelocityIn[id].xyz + buoyancyForce, 1);
}