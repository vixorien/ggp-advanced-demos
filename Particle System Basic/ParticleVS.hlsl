cbuffer externalData : register(b0)
{
	matrix view;
	matrix projection;
	float currentTime;
};

// Struct representing a single particle
struct Particle
{
	float SpawnTime;
	float3 StartPosition;
};


// Buffer of particle data
StructuredBuffer<Particle> ParticleData : register(t0);

// Defines the output data of our vertex shader
struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};


// The entry point for our vertex shader
VertexToPixel main(uint id : SV_VertexID)
{
	// Set up output
	VertexToPixel output;

	// Get id info
	uint particleID = id / 4; // Every group of 4 verts are ONE particle!
	uint cornerID = id % 4; // 0,1,2,3 = the corner of the particle "quad"

	// Offsets for the 4 corners of a quad - we'll only
	// use one for each vertex, but which one depends
	// on the cornerID above.
	float2 offsets[4];
	offsets[0] = float2(-1.0f, +1.0f);  // TL
	offsets[1] = float2(+1.0f, +1.0f);  // TR
	offsets[2] = float2(+1.0f, -1.0f);  // BR
	offsets[3] = float2(-1.0f, -1.0f);  // BL

	// UVs for the 4 corners of a quad - again, only
	// using one for each vertex, but which one depends
	// on the cornerID above.
	float2 uvs[4];
	uvs[0] = float2(0, 0); // TL
	uvs[1] = float2(1, 0); // TR
	uvs[2] = float2(1, 1); // BR
	uvs[3] = float2(0, 1); // BL

	// Grab one particle and its starting position
	Particle p = ParticleData.Load(particleID);
	float3 pos = p.StartPosition;

	// Calculate the age
	float age = currentTime - p.SpawnTime;
	pos += float3(0.1f, 0, 0) * age;

	// Here is where you could do LOTS of other particle
	// simulation updates, like rotation, acceleration, forces,
	// fading, color interpolation, size changes, etc.

	// Billboarding!
	// Offset the position based on the camera's right and up vectors
	pos += float3(view._11, view._12, view._13) * offsets[cornerID].x; // RIGHT
	pos += float3(view._21, view._22, view._23) * offsets[cornerID].y; // UP

	// Calculate output position
	matrix viewProj = mul(projection, view);
	output.position = mul(viewProj, float4(pos, 1.0f));

	// Pass UVs through, too
	output.uv = uvs[cornerID];

	return output;
}
	