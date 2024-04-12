
#include "ParticleIncludes.hlsli"

cbuffer ExternalData : register(b0)
{
	int MaxParticles;
	float CurrentTime;
	float Lifetime;
	float PAD;
}

// Order should match EmitCS (RW binding issues)
RWStructuredBuffer<Particle>	ParticlePool	: register(u0);
AppendStructuredBuffer<uint>	DeadList		: register(u1);
RWStructuredBuffer<uint>		DrawList		: register(u2);

[numthreads(32, 1, 1)]
void main( uint3 id : SV_DispatchThreadID )
{
	// Valid particle?
	if(id.x >= (uint)MaxParticles) 
		return;

	// Grab this particle
	Particle part = ParticlePool.Load(id.x);

	// Early out for ALREADY DEAD particles (so they don't go back on dead list)
	if (part.Alive == 0.0f)	
		return;

	// Calculate the age and update alive status
	float age = CurrentTime - part.EmitTime;
	part.Alive = (float)(age < Lifetime);

	// Put the particle back
	ParticlePool[id.x] = part;

	// Newly dead?
	if (part.Alive == 0.0f)
	{
		// Add to dead list
		DeadList.Append(id.x);
	}
	else
	{
		// Increment the counter on the draw list, then put
		// the new draw data at the returned (pre-increment) index
		uint drawIndex = DrawList.IncrementCounter();
		DrawList[drawIndex] = id.x;
	}
}