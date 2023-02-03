
#include "ParticleIncludes.hlsli"
#include "SimplexNoise.hlsli"

cbuffer ExternalData : register(b0)
{
	float DT;
	float Lifetime;
	float TotalTime;
	int MaxParticles;
}


// Order should match EmitCS (RW binding issues)
RWStructuredBuffer<Particle> ParticlePool	: register(u0);
AppendStructuredBuffer<uint> DeadList		: register(u1);
RWStructuredBuffer<ParticleDraw> DrawList	: register(u2);

[numthreads(32, 1, 1)]
void main( uint3 id : SV_DispatchThreadID )
{
	// Valid particle?
	if(id.x >= (uint)MaxParticles) return;

	// Grab this particle
	Particle part = ParticlePool.Load(id.x);

	// Early out for ALREADY DEAD particles (so they don't go back on dead list)
	if (part.Alive == 0.0f)	return;

	// Update the particle
	part.Age += DT;
	part.Alive = (float)(part.Age < Lifetime); 
	part.Position += part.Velocity * DT;

	// Generate the particle's new velocity using 3D curl noise
	// Plugging its position into the noise function will give
	// us the velocity at that position - kind of like a flow field
	float3 curlPos = part.Position * 0.1f; // Covert particle's world position to "flow field" position
	float3 curlVel = curlNoise3D(curlPos, 1.0f);
	part.Velocity = curlVel;
	
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

		// Set up draw data
		ParticleDraw drawData;
		drawData.Index = id.x; // This particle's actual index
		drawData.DistanceSq = 0.0f; // Not being used yet, but put here for future work

		DrawList[drawIndex] = drawData;
	}
}