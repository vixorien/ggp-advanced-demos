#include "Lighting.hlsli"

// Alignment matters!!!
cbuffer ExternalData : register(b0)
{
	float2 uvScale;
	float2 uvOffset;
	float3 cameraPosition;
	int lightCount;
	Light lights[MAX_LIGHTS];
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
Texture2D RoughnessMap			: register(t2);
Texture2D MetalMap				: register(t3);
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

	// Normal mapping
	input.normal = NormalMapping(NormalMap, BasicSampler, input.uv, input.normal, input.tangent);

	// Surface color with gamma correction
	float4 surfaceColor = AlbedoTexture.Sample(BasicSampler, input.uv);
	surfaceColor.rgb = pow(surfaceColor.rgb, 2.2);

	// Sample the other maps
	float roughness = RoughnessMap.Sample(BasicSampler, input.uv).r;
	float metal = MetalMap.Sample(BasicSampler, input.uv).r;
	
	// Specular color - Assuming albedo texture is actually holding specular color if metal == 1
	// Note the use of lerp here - metal is generally 0 or 1, but might be in between
	// because of linear texture sampling, so we want lerp the specular color to match
	float3 specColor = lerp(F0_NON_METAL.rrr, surfaceColor.rgb, metal);

	// Keep a running total of light
	float3 totalLight = float3(0,0,0);

	// Loop and handle all lights
	for (int i = 0; i < lightCount; i++)
	{
		// Grab this light and normalize the direction (just in case)
		Light light = lights[i];
		light.Direction = normalize(light.Direction);

		// Run the correct lighting calculation based on the light's type
		switch (lights[i].Type)
		{
		case LIGHT_TYPE_DIRECTIONAL:
			totalLight += DirLightPBR(light, input.normal, input.worldPos, cameraPosition, roughness, metal, surfaceColor.rgb, specColor);
			break;

		case LIGHT_TYPE_POINT:
			totalLight += PointLightPBR(light, input.normal, input.worldPos, cameraPosition, roughness, metal, surfaceColor.rgb, specColor);
			break;

		case LIGHT_TYPE_SPOT:
			totalLight += SpotLightPBR(light, input.normal, input.worldPos, cameraPosition, roughness, metal, surfaceColor.rgb, specColor);
			break;
		}
	}

	// Gamma correct and return
	return float4(pow(totalLight, 1.0f / 2.2f), 1.0f);
}