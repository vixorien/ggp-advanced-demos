
#include "ParticleIncludes.hlsli"

cbuffer ExternalData : register(b0)
{
	int EmitCount;
	float3 StartPosition;

	int MaxParticles;
	float3 PosRandomRange;
	
	float CurrentTime;
	float3 StartVelocity;

	float3 VelRandomRange;
	float PAD;

	float2 RotStartMinMax;
	float2 RotEndMinMax;
}

cbuffer DeadListCounterBuffer : register(b1)
{
	uint DeadListCounter;
}

// Order should match UpdateCS (RW binding issues)
RWStructuredBuffer<Particle> ParticlePool	: register(u0);
ConsumeStructuredBuffer<uint> DeadList		: register(u1);


[numthreads(32, 1, 1)]
void main( uint3 id : SV_DispatchThreadID )
{
	// Outside range?
	if(id.x >= (uint)EmitCount || DeadListCounter == 0 || id.x >= DeadListCounter) 
		return;

	 // Grab a single index from the dead list
	uint emitIndex = DeadList.Consume();

	// Grab this particle from the particle pool
	Particle emitParticle = ParticlePool.Load(emitIndex);

	// Set static pieces
	emitParticle.Alive = 1.0f;
	emitParticle.EmitTime = CurrentTime;
	emitParticle.StartPosition = StartPosition;
	emitParticle.StartVelocity = StartVelocity;
	
	// Seed for random uses the emit index, so each
	// unique particle has its own seed
	uint rng = emitIndex;

	// Create a random starting position value (to be scaled later)
	// and use it to tint the particles based on location
	float3 randomStartPos = float3(
		rand_float(rng, -1.0f, 1.0f),
		rand_float(rng, -1.0f, 1.0f),
		rand_float(rng, -1.0f, 1.0f)
	);
	emitParticle.ColorTint = randomStartPos * 0.5f + 0.5f;

	// Generate random values
	emitParticle.StartPosition.x += PosRandomRange.x * randomStartPos.x;
	emitParticle.StartPosition.y += PosRandomRange.y * randomStartPos.y;
	emitParticle.StartPosition.z += PosRandomRange.z * randomStartPos.z;
	emitParticle.StartVelocity.x += VelRandomRange.x * rand_float(rng, -1.0f, 1.0f);
	emitParticle.StartVelocity.y += VelRandomRange.y * rand_float(rng, -1.0f, 1.0f);
	emitParticle.StartVelocity.z += VelRandomRange.z * rand_float(rng, -1.0f, 1.0f);
	emitParticle.StartRotation = rand_float(rng, RotStartMinMax.x, RotStartMinMax.y);
	emitParticle.EndRotation = rand_float(rng, RotEndMinMax.x, RotEndMinMax.y);

	// Put it back
	ParticlePool[emitIndex] = emitParticle;
}