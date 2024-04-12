#include "ParticleIncludes.hlsli"

cbuffer externalData : register(b0)
{
	matrix view;
	matrix projection;

	float4 startColor;
	float4 endColor;

	float currentTime;
	float3 acceleration;

	int spriteSheetWidth;
	int spriteSheetHeight;
	float spriteSheetFrameWidth;
	float spriteSheetFrameHeight;

	float spriteSheetSpeedScale;
	float startSize;
	float endSize;
	float lifetime;

	int constrainYAxis;
};


StructuredBuffer<Particle>	ParticlePool	: register(t0);
StructuredBuffer<uint>		DrawList		: register(t1);

// Defines the output data of our vertex shader
struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
	float4 colorTint	: COLOR;
};


// The entry point for our vertex shader
VertexToPixel main(uint id : SV_VertexID)
{
	// Set up output
	VertexToPixel output;

	// Get id info
	uint drawID = id / 4;	 // Every group of 4 verts are ONE particle!
	uint cornerID = id % 4;	 // 0,1,2,3 = the corner of the particle "quad"

	// Use drawID to get an entry from the draw list
	// Draw list holds actual particle IDs
	uint particleID = DrawList.Load(drawID);
	Particle p = ParticlePool.Load(particleID);

	// Calculate the age and age "percentage" (0 to 1)
	float age = currentTime - p.EmitTime;
	float agePercent = age / lifetime;

	// Constant accleration function to determine the particle's
	// current location based on age, start velocity and accel
	float3 pos = acceleration * age * age / 2.0f + p.StartVelocity * age + p.StartPosition;

	// Size interpolation
	float size = lerp(startSize, endSize, agePercent);

	// Offsets for the 4 corners of a quad - we'll only
	// use one for each vertex, but which one depends
	// on the cornerID above.
	float2 offsets[4];
	offsets[0] = float2(-1.0f, +1.0f);  // TL
	offsets[1] = float2(+1.0f, +1.0f);  // TR
	offsets[2] = float2(+1.0f, -1.0f);  // BR
	offsets[3] = float2(-1.0f, -1.0f);  // BL
	
	// Handle rotation - get sin/cos and build a rotation matrix
	float s, c, rotation = lerp(p.StartRotation, p.EndRotation, agePercent);
	sincos(rotation, s, c); // One function to calc both sin and cos
	float2x2 rot =
	{
		c, s,
		-s, c
	};

	// Rotate the offset for this corner and apply size
	float2 rotatedOffset = mul(offsets[cornerID], rot) * size;

	// Billboarding!
	// Offset the position based on the camera's right and up vectors
	pos += float3(view._11, view._12, view._13) * rotatedOffset.x; // RIGHT
	pos += (constrainYAxis ? float3(0,1,0) : float3(view._21, view._22, view._23)) * rotatedOffset.y; // UP

	// Calculate output position
	matrix viewProj = mul(projection, view);
	output.position = mul(viewProj, float4(pos, 1.0f));

	// Sprite sheet animation calculations
		// Note: Probably even better to swap shaders here (ParticleVS or AnimatedParticleVS)
		//  but this should work for the demo, as we can think of a non-animated particle
		//  as having a sprite sheet with exactly one frame
	float animPercent = fmod(agePercent * spriteSheetSpeedScale, 1.0f);
	uint ssIndex = (uint)floor(animPercent * (spriteSheetWidth * spriteSheetHeight));

	// Get the U/V indices (basically column & row index across the sprite sheet)
	uint uIndex = ssIndex % spriteSheetWidth;
	uint vIndex = ssIndex / spriteSheetWidth; // Integer division is important here!

	// Convert to a top-left corner in uv space (0-1)
	float u = uIndex / (float)spriteSheetWidth;
	float v = vIndex / (float)spriteSheetHeight;

	float2 uvs[4];
	/* TL */ uvs[0] = float2(u, v);
	/* TR */ uvs[1] = float2(u + spriteSheetFrameWidth, v);
	/* BR */ uvs[2] = float2(u + spriteSheetFrameWidth, v + spriteSheetFrameHeight);
	/* BL */ uvs[3] = float2(u, v + spriteSheetFrameHeight);
	
	// Finalize output
	output.uv = saturate(uvs[cornerID]);
	output.colorTint = lerp(startColor, endColor, agePercent) * float4(p.ColorTint, 1);
	return output;
}
	