
#include "ParticleIncludes.hlsli"

cbuffer externalData : register(b0)
{
	matrix world;
	matrix view;
	matrix projection;
};

StructuredBuffer<Particle>		ParticlePool	: register(t0);
StructuredBuffer<ParticleDraw>	DrawList		: register(t1);

struct VStoPS
{
	float4 position : SV_POSITION;
	float4 color	: COLOR;
	float2 uv		: TEXCOORD;
};

VStoPS main(uint id : SV_VertexID)
{
	// Output struct
	VStoPS output;

	// Get id info
	uint particleID = id / 4;
	uint cornerID = id % 4;

	// Look up the draw info, then this particle
	ParticleDraw draw = DrawList.Load(particleID);
	Particle particle = ParticlePool.Load(draw.Index);

	// Offsets for triangles
	float2 offsets[4];
	offsets[0] = float2(-1.0f, +1.0f);  // TL
	offsets[1] = float2(+1.0f, +1.0f);  // TR
	offsets[2] = float2(+1.0f, -1.0f);  // BR
	offsets[3] = float2(-1.0f, -1.0f);  // BL

	// Calc position of this corner
	float3 pos = particle.Position;
	pos += float3(view._11, view._12, view._13) * offsets[cornerID].x * particle.Size;
	pos += float3(view._21, view._22, view._23) * offsets[cornerID].y * particle.Size;

	// Calculate world view proj matrix
	matrix wvp = mul(projection, mul(view, world));
	output.position = mul(wvp, float4(pos, 1.0f));

	float2 uvs[4];
	uvs[0] = float2(0, 0);  // TL
	uvs[1] = float2(1, 0);  // TR
	uvs[2] = float2(1, 1);  // BR
	uvs[3] = float2(0, 1);  // BL

	// Pass through
	output.color = particle.Color;
	output.uv = saturate(uvs[cornerID]);

	return output;
}