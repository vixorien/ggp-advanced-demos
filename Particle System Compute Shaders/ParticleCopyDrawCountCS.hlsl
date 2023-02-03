
#include "ParticleIncludes.hlsli"

cbuffer ExternalData : register(b0)
{
	int VertsPerParticle;
}

RWBuffer<uint>						DrawArgs	: register(u0);
RWStructuredBuffer<ParticleDraw>	DrawList	: register(u1);

[numthreads(1, 1, 1)]
void main( uint3 id : SV_DispatchThreadID )
{
	// Increment the counter to get the previous value, which
	// happens to be how many particles we want to draw
	DrawArgs[0] = DrawList.IncrementCounter() * VertsPerParticle; // VertexCountPerInstance (or index count if using an index buffer)
	DrawArgs[1] = 1; // InstanceCount
	DrawArgs[2] = 0; // Offets
	DrawArgs[3] = 0; // Offets
	DrawArgs[5] = 0; // Offets
}