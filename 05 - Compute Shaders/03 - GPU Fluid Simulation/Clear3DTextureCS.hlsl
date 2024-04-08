
#include "ComputeHelpers.hlsli"

cbuffer externalData : register(b0)
{
	float4 clearColor;
	int channelCount;
}

RWTexture3D<float>  ClearOut1	: register(u0);
RWTexture3D<float2> ClearOut2	: register(u1);
RWTexture3D<float3> ClearOut3	: register(u2);
RWTexture3D<float4> ClearOut4	: register(u3);

[numthreads(
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS,
	FLUID_COMPUTE_THREADS_PER_AXIS)]
void main(uint3 id : SV_DispatchThreadID)
{
	// Which dimension?
	switch (channelCount)
	{
	case 1: ClearOut1[id] = clearColor.r; break;
	case 2:	ClearOut2[id] = clearColor.rg; break;
	case 3:	ClearOut3[id] = clearColor.rgb; break;
	case 4:	ClearOut4[id] = clearColor; break;
	}
}