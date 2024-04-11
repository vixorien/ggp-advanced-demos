#ifndef __PARTICLE_INCLUDES
#define __PARTICLE_INCLUDES

struct Particle
{
	float EmitTime;
	float3 StartPosition;

	float Alive;
	float3 StartVelocity;

	float3 ColorTint;
	float PAD;

	float StartRotation;
	float EndRotation;
	float2 PAD2;
};

// RNG =====================
// Random number generation
// From: https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
// Be sure to initialize rng_state first!
static const float uint2float = 1.0 / 4294967296.0;

uint rand_pcg(inout uint rng_state)
{
	uint state = rng_state;
	rng_state = rng_state * 747796405u + 2891336453u;
	uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

float rand_float(inout uint rng_state)
{
	return rand_pcg(rng_state) * uint2float;
}

float rand_float(inout uint rng_state, float min, float max)
{
	return rand_float(rng_state) * (max - min) + min;
}

// End RNG =================

float3 CalcGridPos(uint index, int gridSize)
{
	// Adjust first
	gridSize += 1;

	float3 gridPos;

	gridPos.x = index % gridSize;

	index /= gridSize;
	gridPos.y = index % gridSize;
	
	index /= gridSize;
	gridPos.z = index;

	return gridPos;
}

#endif