#include "Lighting.hlsli"

cbuffer ExternalData : register(b0)
{
	float2 uvScale;
	float2 uvOffset;
	float3 cameraPosition;
}

// Struct representing the data we expect to receive from earlier pipeline stages
struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float3 worldPos			: POSITION;
};

// Texture related
Texture2D AlbedoTexture			: register(t0);
Texture2D NormalMap				: register(t1);

SamplerState BasicSampler		: register(s0);

// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// --------------------------------------------------------
float4 main(VertexToPixel input) : SV_TARGET
{
	// Clean up un-normalized normals
	input.normal = normalize(input.normal);
	input.tangent = normalize(input.tangent);
	
	// Scale and offset uv as necessary
	input.uv = input.uv * uvScale + uvOffset;

	// Define a basic "test" light
	Light light;
	light.Type = LIGHT_TYPE_DIRECTIONAL;
	light.Direction = normalize(float3(1, -1, 1));
	light.Intensity = 1.0f;
	light.Color = float3(1, 1, 1);

	// Normal mapping
	input.normal = NormalMapping(NormalMap, BasicSampler, input.uv, input.normal, input.tangent);

	// Surface color with gamma correction
	float4 surfaceColor = AlbedoTexture.Sample(BasicSampler, input.uv);
	surfaceColor.rgb = pow(surfaceColor.rgb, 2.2);

	// Calculate our single "test" light
	float3 totalLight = DirLight(light, input.normal, input.worldPos, cameraPosition, 0.5f, surfaceColor.rgb, 1.0f);

	// Gamma correct and return
	return float4(pow(totalLight, 1.0f / 2.2f), 1.0f);
}