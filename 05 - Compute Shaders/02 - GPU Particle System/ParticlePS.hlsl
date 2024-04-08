
#include "ParticleIncludes.hlsli"


struct VStoPS
{
	float4 position : SV_POSITION;
	float4 color	: COLOR;
	float2 uv		: TEXCOORD0;
};


float4 main(VStoPS input) : SV_TARGET
{
	// Convert uv to -1 to 1
	input.uv = input.uv * 2 - 1;

	// Distance from center
	float fade = saturate(distance(float2(0,0), input.uv));
	float3 color = lerp(input.color.rgb, float3(0,0,0), fade * fade);

	return float4(color, 1);
}